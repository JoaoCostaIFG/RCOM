#ifndef DATALINK_H // TOOL - Schism
#define DATALINK_H

#include <stdbool.h>

#include "vector.h"

#define TIMEOUT 3 // seconds between answers
#define MAXATTEMPTS 3
#define DFLTBAUDRATE 38400

#define MAX_SIZE 256 // in Bytes

struct linkLayer {
  char port[20];                 /* Dispositivo /dev/ttySx, x = 0, 1 */
  int baudRate;                  /* Velcidade de transmissao */
  unsigned int sequenceNumber;   /* Numero de sequencia da trama: 0, 1*/
  unsigned int timeout;          /* Valor do temporizador, e.g.: 1 sec */
  unsigned int numTransmissions; /* Numero de retransmissoes em caso de falha */
  // int frameSize;                 [> Tamanho (em bytes) da trama atual <]
  // unsigned char frame[MAX_SIZE]; [> Trama <]
  vector *frame;
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
