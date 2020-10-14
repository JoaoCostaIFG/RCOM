#ifndef MSGUTILS_H
#define MSGUTILS_H

#include <stdbool.h>
#include <termios.h>

#define FLAG 0x7e
#define A_SENDER 0x03   // requests de transmissor e respetivas respostas
#define A_RECEIVER 0x01 // requests de emissor e respetivas respostas
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define ESC 0x7d
#define STUFF_BYTE 0x20

#define MAX_SIZE 256 // in Bytes
#define TIMEOUT 1    // seconds between answers
#define MAXATTEMPTS 3
#define BAUDRATE B38400

enum SUByteField {
  FLAG1_FIELD = 0,
  A_FIELD = 1,
  C_FIELD = 2,
  BCC_FIELD = 3,
  FLAG2_FIELD = 4
};

enum applicationStatus { TRANSMITTER, RECEIVER };

struct applicationLayer {
  int fd; /* fileDescriptor correspondente a porta serie */
  enum applicationStatus status;
};

struct linkLayer {
  char port[20];                 /* Dispositivo /dev/ttySx, x = 0, 1 */
  int baudRate;                  /* Velcidade de transmissao */
  unsigned int sequenceNumber;   /* Numero de sequencia da trama: 0, 1*/
  unsigned int timeout;          /* Valor do temporizador, e.g.: 1 sec */
  unsigned int numTransmissions; /* Numero de retransmissoes em caso de falha */

  char frame[MAX_SIZE]; /* Trama */
};

struct linkLayer initLinkLayer();

void fillByteField(char *buf, enum SUByteField field, char byte);

void assembleOpenMsg(struct linkLayer *linkLayer,
                     enum applicationStatus appStatus);

void setBCCField(char *buf);

char calcBCCField(char *buf);

bool checkBCCField(char *buf);

void printfBuf(char *buf);

int stuffString(char str[], char res[]);

/* enum: transition
 * FLAG_RCV:  0
 * A_RCV: 1
 * C_RCV:  2
 * BCC_RCV: 3
 */
typedef enum { FLAG_RCV, A_RCV, C_RCV, BCC_RCV, OTHER_RCV } transitions_enum;

/* enum: state_enum
 * START:   0
 * FLAG:    1
 * A:       2
 * C:       3
 * BCC:     4
 * STOP:    4
 */
typedef enum { START_ST, FLAG_ST, A_ST, C_ST, BCC_ST, STOP_ST } state_enum;

transitions_enum byteToTransitionSET(char byte, char *buf,
                                     state_enum curr_state);
transitions_enum byteToTransitionUA(char byte, char *buf,
                                    state_enum curr_state);

static state_enum event_matrix[][7] = {
    //  FLAG_ST_RCV   A_RCV        C_RCV       BCC_RCV   OTHER_RCV
    {FLAG_ST, START_ST, START_ST, START_ST, START_ST}, // START_ST
    {FLAG_ST, A_ST, START_ST, START_ST, START_ST},     // FLAG_ST
    {FLAG_ST, START_ST, C_ST, START_ST, START_ST},     // A
    {FLAG_ST, START_ST, START_ST, BCC_ST, START_ST},   // C
    {STOP_ST, START_ST, START_ST, START_ST, START_ST}, // BCC
    {STOP_ST, STOP_ST, STOP_ST, STOP_ST, STOP_ST},     // STOP_ST
};

#endif // MSGUTILS_H
