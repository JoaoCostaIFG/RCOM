#ifndef APPLAYER_PRIV_H
#define APPLAYER_PRIV_H

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

#define MYVTIME 1
#define MYVMIN 0

#include "app_layer.h"

int assembleControlPacket(struct applicationLayer *appLayer, bool is_end,
                          unsigned char **buffer);
int assembleInfoPacket(char *buffer, int length, unsigned char **packet);

int parseControlPacket(unsigned char *packet);
int parsePacket(unsigned char *packet, int packet_length);
bool isEndPacket(unsigned char *packet, int n);
unsigned char *getStartPacket();

#endif // APPLAYER_PRIV_H
