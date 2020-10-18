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
#define C_DISC 0x0B
#define C_UA 0x07
#define C_RR 0x05
#define C_REJ 0x01
#define C_CTRL0 0x00
#define C_CTRL1 0x40
#define ESC 0x7d
#define STUFF_BYTE 0x20

#define MAX_SIZE 256 // in Bytes
#define TIMEOUT 3    // seconds between answers
#define MAXATTEMPTS 3
#define BAUDRATE B38400

enum SUMessageType { SET_MSG, DISC_MSG, UA_MSG, RR_MSG, REJ_MSG };

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

  int frameSize;                 /* Tamanho (em bytes) da trama atual */
  unsigned char frame[MAX_SIZE]; /* Trama */
};

struct linkLayer initLinkLayer();

#define FLIPSEQUENCENUMBER(linkLayer)                                          \
  linkLayer.sequenceNumber = (linkLayer.sequenceNumber == 0 ? 1 : 0);

void fillByteField(unsigned char *buf, enum SUByteField field,
                   unsigned char byte);

void assembleSUPacket(struct linkLayer *linkLayer,
                      enum SUMessageType messageType);

void assembleInfoPacket(struct linkLayer *linkLayer, unsigned char *buf,
                        int size);

void setBCCField(unsigned char *buf);

unsigned char calcBCCField(unsigned char *buf);

bool checkBCCField(unsigned char *buf);

unsigned char calcBCC2Field(unsigned char *buf, int size);

bool checkBCC2Field(unsigned char *buf, int size);

void printfBuf(unsigned char *buf);

int stuffString(unsigned char str[], unsigned char res[], int size);

unsigned char destuffByte(unsigned char byte);

int stuffByte(unsigned char byte, unsigned char res[]);

int sendAndAlarm(struct linkLayer *linkLayer, int fd);

int sendAndAlarmReset(struct linkLayer* linkLayer, int fd);

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

transitions byteToTransitionSET(unsigned char byte, unsigned char *buf,
                                state curr_state);
transitions byteToTransitionUA(unsigned char byte, unsigned char *buf,
                               state curr_state);
transitions byteToTransitionI(unsigned char byte, unsigned char *buf,
                              state curr_state);
transitions byteToTransitionRR(unsigned char byte, unsigned char *buf,
                              state curr_state);

// clang-format off
static state state_machine[][6] = {
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
