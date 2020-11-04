#ifndef APPLAYER_H
#define APPLAYER_H

#include <stdbool.h>
#include <termios.h>

#define PORTLOCATION "/dev/ttyS"

enum applicationStatus { TRANSMITTER, RECEIVER, NONE };

struct applicationLayer {
  int fd; /* file descriptor correspondente a porta serie */
  enum applicationStatus status;
  char file_name[256];
  long file_size;
  long chunksize;
};

void initAppLayer(struct applicationLayer *appLayer, int baudrate, long chunksize);

int llopen(int porta, enum applicationStatus appStatus);

int llwrite(int fd, char *buffer, int length);

int llread(int fd, char **buffer);

int llclose(int fd, enum applicationStatus appStatus);

long getStartPacketFileSize();
int getStartPacketFileName(char **file_name);

int sendFile(struct applicationLayer * appLayer);
int receiveFile(struct applicationLayer * appLayer, unsigned char **res);
void write_file(struct applicationLayer * appLayer, unsigned char *file_content);

#endif // APPLAYER_H
