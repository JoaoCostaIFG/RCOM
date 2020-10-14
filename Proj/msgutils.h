#ifndef MSGUTILS_H
#define MSGUTILS_H

#include <stdbool.h>

#define FLAG 0x7e
#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B

#define MSG_SIZE 5 // 5 Bytes
#define TIMEOUT 3  // 3 seconds between answers
#define MAXATTEMPTS 3

enum SUByteField {
  FLAG1_FIELD = 0,
  A_FIELD = 1,
  C_FIELD = 2,
  BCC_FIELD = 3,
  FLAG2_FIELD = 4
};

void fillByteField(unsigned char *buf, enum SUByteField field,
                   unsigned char byte);

void setBCCField(unsigned char *buf);

bool checkBCCField(unsigned char *buf);

void printfBuf(unsigned char *buf);

/* enum: transition
 * FLAG_RCV:  0
 * A_RCV: 1
 * C_RCV:  2
 * BCC_RCV: 3
 */
typedef enum { FLAG_RCV, A_RCV, C_RCV, BCC_RCV } transitions_enum;

/* enum: state_enum
 * START:   0
 * FLAG:    1
 * A:       2
 * C:       3
 * BCC:     4
 * STOP:    4
 */
typedef enum { START_ST, FLAG_ST, A_ST, C_ST, BCC_ST, STOP_ST } state_enum;

static state_enum event_matrix[][6] = {
    //  FLAG_ST_RCV    A_RCV        C_RCV       BCC_RCV
    {   FLAG_ST,       START_ST,    START_ST,   START_ST},  // START_ST
    {   FLAG_ST,       A_ST,        START_ST,   START_ST},  // FLAG_ST
    {   FLAG_ST,       START_ST,    C_ST,       START_ST},  // A
    {   FLAG_ST,       START_ST,    START_ST,   BCC_ST},    // C
    {   STOP_ST,       START_ST,    START_ST,   START_ST},  // BCC
    {   STOP_ST,       STOP_ST,     STOP_ST,    STOP_ST},   // STOP_ST
};

#endif // MSGUTILS_H
