#ifndef APPLAYER_H
#define APPLAYER_H

#include <termios.h>

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
