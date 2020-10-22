#ifndef APPLAYER_H
#define APPLAYER_H

#include <stdbool.h>
#include <termios.h>

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
#define MAXSEQNUM 255

#define PORTLOCATION "/dev/ttyS"

enum applicationStatus { TRANSMITTER, RECEIVER, NONE };

struct applicationLayer {
  int fd; /* file descriptor correspondente a porta serie */
  enum applicationStatus status;
  char file_name[256];
  long file_size;
};

void initAppLayer(struct applicationLayer *appLayer, int port, int baudrate);

int llopen(int porta, enum applicationStatus appStatus);

int assembleControlPacket(struct applicationLayer *appLayer, bool is_end,
                          unsigned char **buffer);
int assembleInfoPacket(char *buffer, int length, unsigned char **packet);
int llwrite(int fd, char *buffer, int length);

int llread(int fd, char **buffer);

int llclose(int fd, enum applicationStatus appStatus);

int parsePacket(unsigned char *packet, int packet_length);
bool isEndPacket(unsigned char *packet, int n);
unsigned char *getStartPacket();
long getStartPacketFileSize();
char* getStartPacketFileName();

#endif // APPLAYER_H
