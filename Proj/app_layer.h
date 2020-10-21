#ifndef APPLAYER_H
#define APPLAYER_H

#include <termios.h>
#include <stdbool.h>

#define C_DATA 1
#define C_START 2
#define C_END 3
#define L2VAL 256
#define T_SIZE 0
#define T_NAME 1

/* Packet Headers */
#define C_CONTROL 0
#define SEQ_NUMBER 1
#define L2 2
#define L1 3

#define PORTLOCATION "/dev/ttyS"

enum applicationStatus { TRANSMITTER, RECEIVER, NONE };

struct applicationLayer {
  int fd; /* file descriptor correspondente a porta serie */
  enum applicationStatus status;
  char *file_name; // TODO is malloced in main
  long file_size;
};

// int llopen(int porta, enum applicationStatus appStatus);
int llopen(int porta, enum applicationStatus appStatus);

unsigned char *assembleControlPacket(int fd, bool is_end, char *file_name,
                                     long file_size);
unsigned char *assembleInfoPacket(char *buffer, int length);
int llwrite(int fd, char *buffer, int length);

int llread(int fd, char *buffer);

int llclose(int fd, enum applicationStatus appStatus);

#endif // APPLAYER_H
