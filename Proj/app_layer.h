#ifndef APPLAYER_H
#define APPLAYER_H

#include <termios.h>

#define C_DATA 1
#define C_START 2
#define C_END 3
#define L2VAL 256
#define T_SIZE 0
#define T_NAME 1

enum applicationStatus { TRANSMITTER, RECEIVER };

struct applicationLayer {
  int fd; /* file descriptor correspondente a porta serie */
  enum applicationStatus status;
};

// int llopen(int porta, enum applicationStatus appStatus);
int llopen(char* porta, enum applicationStatus appStatus);

int llwrite(int fd, char* buffer, int length);

int llread(int fd, char* buffer);

int llclose(int fd);

#endif // APPLAYER_H
