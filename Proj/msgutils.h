#ifndef MSGUTILS_H
#define MSGUTILS_H

#include <stdbool.h>
#include <termios.h>

// I, SET, DISC: commands
// UA, RR, REJ: answers

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

  int frameSize;        /* Tamanho (em bytes) da trama atual */
  char frame[MAX_SIZE]; /* Trama */
};

#define sendCurrPacket(linkLayer, fd)                                          \
  write(fd, linkLayer.frame, linkLayer.frameSize)

struct linkLayer initLinkLayer();

void fillByteField(char *buf, enum SUByteField field, char byte);

void assembleOpenPacket(struct linkLayer *linkLayer,
                        enum applicationStatus appStatus);

void assembleInfoPacket(struct linkLayer *linkLayer, char *buf, int size);

void setBCCField(char *buf);

char calcBCCField(char *buf);

bool checkBCCField(char *buf);

void printfBuf(char *buf);

int stuffString(char str[], char res[], int size);

char destuffByte(char byte);

int stuffByte(char byte, char res[]);

/* enum: transition
 * F_RCV:  0
 * A_RCV: 1
 * CI_RCV:  2
 * CS_RCV: 3
 * OTHER_RCV : 4
 */
typedef enum {
  FLAG_RCV = 0,
  A_RCV = 1,
  CI_RCV = 2,
  CS_RCV = 3,
  BCC_RCV = 4,
  OTHER_RCV = 5
} transitions;

typedef enum {
  START_ST = 0,
  FLAG_ST = 1,
  A_ST = 2,
  CS_ST = 3,
  BCC_ST = 4,
  CI_ST = 5,
  D_ST = 6,
  STOP_ST = 7
} state;

transitions byteToTransitionSET(char byte, char *buf, state curr_state);
transitions byteToTransitionUA(char byte, char *buf, state curr_state);

// clang-format off
static state info_state_machine[][6] {
/*  F_RCV     A_RCV     CI_RCV    CS_RCV    BCC_RCV   OTHER_RCV */
  { FLAG_ST,  START_ST, START_ST, START_ST, START_ST, START_ST},  // START_ST
  { FLAG_ST,  A_ST,     START_ST, START_ST, START_ST, START_ST},  // FLAG_ST
  { FLAG_ST,  START_ST, CI_ST,    CS_ST,    START_ST, START_ST},  // A_ST
  { FLAG_ST,  START_ST, START_ST, START_ST, BCC_ST,   START_ST},  // CS_ST
  { STOP_ST,  START_ST, START_ST, START_ST, START_ST, START_ST},  // BCC_ST
  { FLAG_ST,  START_ST, START_ST, START_ST, D_ST,     START_ST},  // CI_ST
  { STOP_ST,  D_ST,     D_ST,     D_ST,     D_ST,     D_ST    },  // D_ST
  { STOP_ST,  STOP_ST,  STOP_ST,  STOP_ST,  STOP_ST,  STOP_ST }   // STOP_ST
};
// clang-format on

#endif // MSGUTILS_H
