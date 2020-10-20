#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_layer.h"
#include "data_link.h"

static struct linkLayer linkLayer;
static struct termios oldtio;

// TODO char* porta ou int porta
int llopen(char *porta, enum applicationStatus appStatus) {
  linkLayer = initLinkLayer();
  strcpy(linkLayer.port, porta);

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  int fd = open(linkLayer.port, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror(porta);
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
    inputLoopSET(&linkLayer, fd);
    if (sendUAMsg(&linkLayer, fd) < 0)
      return -5;
    /* struct rcv_file rcv_file = getFile(); */
  } else if (appStatus == TRANSMITTER) {
    if (sendSetMsg(&linkLayer, fd) < 0)
      return -5;
    inputLoopUA(&linkLayer, fd);
    /* sendFile(FILETOSEND); */
  } else {
    return -4;
  }

  return fd;
}

int llmetawrite(int fd, bool is_end, char *file_name, long file_size) {
  // C = 2/3 | T1 - L1 - V1 | T2 - L2 - V2 | ...
  // T (type): 0 tamanho file, 1 nome file, etc...
  // L (byte): length
  // V: valor (L length bytes)

  /* assemble packet */
  int file_name_len = strlen(file_name);
  int new_length = 1 + 2 + file_name_len + 2 + sizeof(long);
  unsigned char *packet =
      (unsigned char *)malloc(new_length * sizeof(unsigned char));
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
  memcpy(packet + curr_ind, &file_size, sizeof(long));
  curr_ind += sizeof(long) + 1;

  // TLV file name
  packet[curr_ind++] = T_NAME;
  packet[curr_ind++] = file_name_len;
  memcpy(packet + curr_ind, file_name, file_name_len);

  if (sendPacket(&linkLayer, fd, packet, new_length) < 0) {
    free(packet);
    return -2;
  }
  free(packet);
  return 0;
}

int llwrite(int fd, char *buffer, int length) {
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

  packet[0] = C_DATA;
  packet[1] = n;
  packet[2] = (unsigned char)(length % 256);
  packet[3] = (unsigned char)(length - packet[2] * 256);
  memcpy(packet + 4, buffer, sizeof(char) * length);

  if (sendPacket(&linkLayer, fd, packet, new_length) < 0) {
    free(packet);
    return -2;
  }
  free(packet);
  return 0;
}

int llread(int fd, char *buffer) {
  // TODO memcmp
  return 0;
}

int llclose(int fd, enum applicationStatus appStatus) {
  if (appStatus == RECEIVER) {
    inputLoopDISC(&linkLayer, fd);
    if (sendDISCMsg(&linkLayer, fd) < 0)
      return -1;
    inputLoopUA(&linkLayer, fd);

  } else if (appStatus == TRANSMITTER) {
    if (sendDISCMsg(&linkLayer, fd) < 0)
      return -2;
    inputLoopDISC(&linkLayer, fd);
    if (sendUAMsg(&linkLayer, fd) < 0)
      return -3;

  } else {
    return -4;
  }

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    return -5;
  }

  return close(fd);
}
