#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_layer.h"
#include "data_link.h"

static struct linkLayer linkLayer;
static struct termios oldtio;
static long fileSize;
static char *fileName;

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
    inputLoopSET(&linkLayer, fd);
    if (sendUAMsg(&linkLayer, fd) < 0)
      return -5;
  } else if (appStatus == TRANSMITTER) {
    if (sendSetMsg(&linkLayer, fd) < 0)
      return -5;
    inputLoopUA(&linkLayer, fd);
  } else {
    return -4;
  }

  return fd;
}

int assembleControlPacket(struct applicationLayer *appLayer, bool is_end,
                          unsigned char *packet) {
  // C = 2/3 | T1 - L1 - V1 | T2 - L2 - V2 | ...
  // T (type): 0 tamanho file, 1 nome file, etc...
  // L (byte): length
  // V: valor (L length bytes)

  /* assemble packet */
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

  return length;
}

int assembleInfoPacket(char *buffer, int length, unsigned char *packet) {
  // C = 1 | N | L2 - L1: 256 * L2 + L1 = k | P1..Pk (k bytes)
  /* assemble packet */
  static unsigned char n = 255; // unsigned integer overflow is defined >:(
  ++n;

  int new_length = length + 4;
  packet = (unsigned char *)malloc(new_length * sizeof(unsigned char));
  if (packet == NULL) {
    perror("App layer packet instantiation");
    return -1;
  }

  packet[C_CONTROL] = C_DATA;
  packet[SEQ_NUMBER] = n;
  packet[L2] = (unsigned char)(length % 256);
  packet[L1] = (unsigned char)(length - packet[2] * 256);
  memcpy(packet + 4, buffer, sizeof(char) * length);

  return new_length;
}

int llwrite(int fd, char *buffer, int length) {
  if (sendFrame(&linkLayer, fd, (unsigned char *)buffer, length) < 0) {
    free(buffer);
    return -1;
  }
  free(buffer);
  return 0;
}

int llread(int fd, char *buffer) {
  static unsigned char n = 255; // unsigned integer overflow is defined >:(
  ++n;

  unsigned char *packet = NULL;
  int packet_length = getFrame(&linkLayer, fd, packet);

  if (packet_length < 0)
    return -1; // Morreu mesmo
  else if (packet_length == 0)
    return -2; // Morreu só neste

  if (packet[C_CONTROL] == C_DATA) {
    if (packet[SEQ_NUMBER] != n) { // Invalid sequence number
      free(packet);
      return -2;
    }

    int expected_length = packet[L2] * 256 + L1;
    if (expected_length != packet_length) {
      free(packet);
      return -2;
    }

    return 0;
  } else if (packet[C_CONTROL] == C_START || packet[C_CONTROL] == C_END) {

    int curr_ind = C_CONTROL + 1;
    int type_size = packet[curr_ind++];
    if (type_size != T_SIZE) {
      free(packet);
      return -4;
    }
    int length_size = packet[curr_ind++];
    long packet_length;
    memcpy(&packet_length, packet + curr_ind, length_size);
    curr_ind += length_size;

    int type_name = packet[curr_ind++];
    if (type_name != T_NAME) {
      free(packet);
      return -5;
    }
    int length_name = packet[curr_ind++];
    char *packet_file_name = (char *)malloc(length_name * sizeof(char));

    if (packet[C_CONTROL] == C_START) {
      fileName = packet_file_name;
      fileSize = packet_length;

      return 1;
    } else { // C_END
      if (strcmp(packet_file_name, fileName) != 0) {
        free(packet);
        free(packet_file_name);
        free(fileName);
        return -6;
      }
      free(packet_file_name);

      if (packet_length != fileSize) {
        free(packet);
        free(fileName);
        return -6;
      }

      return 2;
    }

  } else {
    free(packet);
    return -3; // Unexpected C header
  }

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
