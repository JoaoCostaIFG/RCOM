#ifndef DATALINK_H // TOOL - Schism
#define DATALINK_H

#include <stdbool.h>
#include <time.h>

#include "vector.h"

#define TIMEOUT 3 // seconds between answers
#define MAXATTEMPTS 3
#define DFLTBAUDRATE 38400

#define MAX_SIZE 256 // in Bytes

struct linkStats {
  unsigned int sent;      /* total number of sent frames */
  unsigned int resent;    /* number of resent frames */
  unsigned int received;  /* total number of received frames */
  unsigned int RRs;       /* number of RRs sent */
  unsigned int REJs;      /* number of REJs sent */
  struct timespec start_time; /* time at which the connection started */
  struct timespec end_time;   /* time at which the connection ended */
};

struct linkLayer {
  char port[20];                 /* Dispositivo /dev/ttySx, x = 0, 1 */
  int baudRate;                  /* Velcidade de transmissao */
  unsigned int sequenceNumber;   /* Numero de sequencia da trama: 0, 1*/
  unsigned int timeout;          /* Valor do temporizador, e.g.: 1 sec */
  unsigned int numTransmissions; /* Numero de retransmissoes em caso de falha */
  vector *frame;                 /* last sent frame */
  struct linkStats stats;        /* connection statistics */
};

struct linkLayer initLinkLayer();

int initConnection(struct linkLayer *linkLayer, int fd, bool isReceiver);
int endConnection(struct linkLayer *linkLayer, int fd, bool isReceiver);
int setBaudRate(struct linkLayer *linkLayer, int baudrate);

/* ret: < 0, error
 *      0, request resend of packet
 *      > 0, size of packet
 */
int getFrame(struct linkLayer *linkLayer, int fd, unsigned char **packet);

/* llwrite BACKEND */
int sendFrame(struct linkLayer *linkLayer, int fd, unsigned char *packet,
              int len);

#endif // DATALINK_H
