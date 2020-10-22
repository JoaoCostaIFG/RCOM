#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_layer.h"
#include "data_link.h"

static struct linkLayer linkLayer;
static struct termios oldtio;
static unsigned char *startPacket;

void initAppLayer(int port, int baudrate) {
  linkLayer = initLinkLayer();

  char port_str[20];
  sprintf(port_str, "%d", port);
  strcpy(linkLayer.port, PORTLOCATION);
  strcat(linkLayer.port, port_str);

  if (setBaudRate(&linkLayer, baudrate) < 0)
    fprintf(stderr, "Invalid baudrate. Using default: %d\n", DFLTBAUDRATE);
}

int llopen(int porta, enum applicationStatus appStatus) {
  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  int fd = open(linkLayer.port, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror(linkLayer.port);
    return -1;
  }

  if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    return -2;
  }

  struct termios newtio;
  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = linkLayer.baudRate | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
  */
  newtio.c_cc[VTIME] = 0; /* inter-unsigned character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 5 unsigned chars received */
  // clear queue
  tcflush(fd, TCIOFLUSH);

  // set new struct
  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    return -3;
  }

  // handshake (receiver starts first)
  // SET ->
  // UA <-
  if (appStatus == RECEIVER) {
    if (initConnection(&linkLayer, fd, true) < 0)
      return -5;
  } else if (appStatus == TRANSMITTER) {
    if (initConnection(&linkLayer, fd, false) < 0)
      return -5;
  } else {
    return -4;
  }

  return fd;
}

int assembleControlPacket(struct applicationLayer *appLayer, bool is_end,
                          unsigned char **buf) {
  // C = 2/3 | T1 - L1 - V1 | T2 - L2 - V2 | ...
  // T (type): 0 tamanho file, 1 nome file, etc...
  // L (byte): length
  // V: valor (L length bytes)

  /* assemble packet */
  unsigned char *packet;
  int file_name_len = strlen(appLayer->file_name);
  int length = 1 + 2 + file_name_len + 2 + sizeof(long);
  packet = (unsigned char *)malloc(length * sizeof(unsigned char));
  if (packet == NULL) {
    perror("App layer packet instantiation");
    return -1;
  }

  int curr_ind = 0;
  if (is_end)
    packet[curr_ind++] = C_END;
  else
    packet[curr_ind++] = C_START;

  // TLV file size
  packet[curr_ind++] = T_SIZE;
  packet[curr_ind++] = sizeof(long);
  memcpy(packet + curr_ind, &appLayer->file_size, sizeof(long));
  curr_ind += sizeof(long);

  // TLV file name
  packet[curr_ind++] = T_NAME;
  packet[curr_ind++] = file_name_len;
  memcpy(packet + curr_ind, appLayer->file_name, file_name_len);
  *buf = packet;

  return length;
}

int assembleInfoPacket(char *buffer, int length, unsigned char **res) {
  // C = 1 | N | L2 - L1: 256 * L2 + L1 = k | P1..Pk (k bytes)
  /* assemble packet */
  static unsigned char n = MAXSEQNUM; // unsigned integer overflow is defined >:(
  ++n;

  int new_length = length + 4;
  unsigned char *packet =
      (unsigned char *)malloc(new_length * sizeof(unsigned char));
  if (packet == NULL) {
    perror("App layer packet instantiation");
    return -1;
  }

  packet[C_CONTROL] = C_DATA;
  packet[SEQ_NUMBER] = n;
  packet[L2] = (unsigned char)(length / L2VAL);
  packet[L1] = (unsigned char)(length % L2VAL);
  memcpy(packet + 4, buffer, sizeof(char) * length);

  *res = packet;
  return new_length;
}

int llwrite(int fd, char *buffer, int length) {
  if (sendFrame(&linkLayer, fd, (unsigned char *)buffer, length) < 0)
    return -1;
  return 0;
}

int llread(int fd, char **buffer) {
  int packet_length = getFrame(&linkLayer, fd, (unsigned char **)buffer);

  if (packet_length == 0)
    return 0; // Morreu só neste
  else if (packet_length < 0)
    return -1; // Morreu mesmo

  return packet_length;
}

int llclose(int fd, enum applicationStatus appStatus) {
  if (appStatus == RECEIVER) {
    if (endConnection(&linkLayer, fd, true))
      return -1;
  } else if (appStatus == TRANSMITTER) {
    if (endConnection(&linkLayer, fd, false))
      return -1;
  } else {
    return -2;
  }

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    return -3;
  }

  return close(fd);
}

/* llread BACKEND */
int parseControlPacket(unsigned char *packet) {

  int curr_ind = C_CONTROL + 1;
  int size_type = packet[curr_ind++];
  if (size_type != T_SIZE) {
    fprintf(stderr, "Invalid size type %d\n", size_type);
    return -1;
  }

  int size_length = packet[curr_ind++];
  curr_ind += size_length;

  int name_type = packet[curr_ind++];
  if (name_type != T_NAME) {
    fprintf(stderr, "Invalid name type %d\n", name_type);
    return -1;
  }

  if (packet[C_CONTROL] == C_START)
    startPacket = packet;

  return 0;
}

int parsePacket(unsigned char *packet, int packet_length) {
  static unsigned char n = MAXSEQNUM; // unsigned integer overflow is defined >:(

  if (packet[C_CONTROL] == C_DATA) { // Verifications in case of data
    ++n;
    if (packet[SEQ_NUMBER] != n) { // Invalid sequence number
      fprintf(stderr, "Sq num: %d_%d\n", packet[SEQ_NUMBER], n);
      free(packet);
      return -2;
    }

    int expected_length = packet[L2] * L2VAL + packet[L1] + 4;
    if (expected_length != packet_length) {
      fprintf(stderr, "Invalid length: %d_%d\n", expected_length,
              packet_length);
      free(packet);
      return -2;
    }

  } else if (packet[C_CONTROL] == C_START ||
             packet[C_CONTROL] == C_END) { // Start and End

    if (parseControlPacket(packet) < 0) {
      free(packet);
      return -3;
    }

  } else {
    fprintf(stderr, "Seq header %d\n", packet[C_CONTROL]);
    free(packet);
    return -3; // Unexpected C header
  }

  return packet[C_CONTROL];
}

unsigned char *getStartPacket() { return startPacket; }

long getStartPacketFileSize() {
  long file_size;
  memcpy(&file_size, startPacket + 3, sizeof(long));
  return file_size;
}

char* getStartPacketFileName() {
  return (char*)(startPacket + 2 + sizeof(long) + 3);
}
