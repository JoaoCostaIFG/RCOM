#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_layer.h"
#include "data_link.h"

static struct linkLayer linkLayer;
static struct termios oldtio;
static struct controlPacket *startPacket, *endPacket;

int llopen(int porta, enum applicationStatus appStatus) {
  linkLayer = initLinkLayer();
  char port_str[20];
  sprintf(port_str, "%d", porta);
  strcpy(linkLayer.port, PORTLOCATION);
  strcat(linkLayer.port, port_str);

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
    if (inputLoopSET(&linkLayer, fd) < 0 || sendUAMsg(&linkLayer, fd) < 0)
      return -5;
  } else if (appStatus == TRANSMITTER) {
    if (sendSetMsg(&linkLayer, fd) < 0 || inputLoopUA(&linkLayer, fd) < 0)
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
  if (!is_end)
    packet[curr_ind++] = C_END;
  else
    packet[curr_ind++] = C_START;

  // TLV file size
  packet[curr_ind++] = T_SIZE;
  packet[curr_ind++] = sizeof(long);
  memcpy(packet + curr_ind, &appLayer->file_size, sizeof(long));
  curr_ind += sizeof(long) + 1;

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
  static unsigned char n = 255; // unsigned integer overflow is defined >:(
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
  packet[L2] = (unsigned char)(length % 256);
  packet[L1] = (unsigned char)(length - packet[2] * 256);
  fprintf(stderr, "%d", length);
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
    if (inputLoopDISC(&linkLayer, fd))
      return -1;
    if (sendDISCMsg(&linkLayer, fd) < 0)
      return -1;
    if (inputLoopUA(&linkLayer, fd))
      return -1;

  } else if (appStatus == TRANSMITTER) {
    if (sendDISCMsg(&linkLayer, fd) < 0)
      return -2;
    if (inputLoopDISC(&linkLayer, fd))
      return -2;
    if (sendUAMsg(&linkLayer, fd) < 0)
      return -3;

  } else {
    return -3;
  }

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    return -4;
  }

  return close(fd);
}

/* llread BACKEND */
int parseControlPacket(unsigned char *packet,
                       struct controlPacket *control_packet) {

  int curr_ind = C_CONTROL + 1;
  int size_type = packet[curr_ind++];
  if (size_type != T_SIZE)
    return -1;

  int size_length = packet[curr_ind++];
  long fileSize;
  memcpy(&fileSize, packet + curr_ind, size_length);
  curr_ind += size_length;

  int name_type = packet[curr_ind++];
  if (name_type != T_NAME)
    return -1;

  int name_length = packet[curr_ind++];
  char *packet_file_name = (char *)malloc(name_length * sizeof(char));

  control_packet->nameType = name_type;
  control_packet->nameLength = name_length;
  control_packet->fileName = packet_file_name;

  control_packet->sizeType = size_type;
  control_packet->sizeLength = size_length;
  control_packet->fileSize = fileSize;

  control_packet->packet = packet;
  return 0;
}

int parsePacket(unsigned char *packet, int packet_length) {
  static unsigned char n = 255; // unsigned integer overflow is defined >:(
  ++n;

  if (packet[C_CONTROL] == C_DATA) { // Verifications in case of data
    if (packet[SEQ_NUMBER] != n) {   // Invalid sequence number
      fprintf(stderr, "Sq num: %d_%d\n", packet[SEQ_NUMBER], n);
      free(packet);
      return -2;
    }

    int expected_length = packet[L2] * 256 + packet[L1];
    if (expected_length != packet_length) {
      fprintf(stderr, "Invalid length: %d_%d\n", expected_length,
              packet_length);
      free(packet);
      return -2;
    }

    return 0;
  } else if (packet[C_CONTROL] == C_START || packet[C_CONTROL] == C_END) {
    struct controlPacket *control_packet =
        (struct controlPacket *)malloc(sizeof(struct controlPacket));

    if (parseControlPacket(packet, control_packet) < 0) {
      free(packet);
      free(control_packet->fileName);
      free(control_packet);
      perror("Invalid control packet format");
      return -3;
    }

    if (packet[C_CONTROL] == C_START) {
      control_packet->controlField = C_START;
      startPacket = control_packet;

      return 1;
    } else {
      control_packet->controlField = C_END;
      endPacket = control_packet;

      if (strcmp(startPacket->fileName, endPacket->fileName) != 0) {
        free(packet);
        free(startPacket->fileName);
        free(startPacket);
        free(endPacket->fileName);
        free(endPacket);
        perror("start and end packet differ in fileName");
        return -6;
      }

      if (startPacket->fileSize != endPacket->fileSize) {
        free(packet);
        free(startPacket->fileName);
        free(startPacket);
        free(endPacket->fileName);
        free(endPacket);
        perror("start and end packet differ in fileSize");
        return -6;
      }

      return 2;
    }

  } else {
    fprintf(stderr, "Seq header %d\n", packet[C_CONTROL]);
    free(packet);
    return -3; // Unexpected C header
  }
  return 0;
}

bool isEndPacket(unsigned char *packet) { return endPacket->packet == packet; }
bool isStartPacket(unsigned char *packet) {
  return startPacket->packet == packet;
}

struct controlPacket *getStartPacket() {
  return startPacket;
}

struct controlPacket *getEndPacket() {
  return endPacket;
}
