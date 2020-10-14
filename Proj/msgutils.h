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
typedef enum { START, FLAG, A, C, BCC, STOP } state_enum;

static state_enum event_matrix[][6] = {
    //  FLAG_RCV    A_RCV   C_RCV   BCC_RCV
    {   FLAG,       START,  START,  START}, // START
    {   FLAG,       A,      START,  START}, // FLAG
    {   FLAG,       START,  C,      START}, // A
    {   FLAG,       START,  START,  BCC},   // C
    {   STOP,       START,  START,  START}, // BCC
    {   STOP,       STOP,   STOP,    STOP}, // STOP
};

#endif // MSGUTILS_H
