#ifndef MSGUTILS_H
#define MSGUTILS_H

#include <stdbool.h>

#define FLAG 0x7e
#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07

#define MSG_SIZE 5 // 5 Bytes
#define TIMEOUT 3 // 3 seconds between answers
#define MAXATTEMPTS 3

enum byteField {
  FLAG1_FIELD = 0,
  A_FIELD = 1,
  C_FIELD = 2,
  BCC_FIELD = 3,
  FLAG2_FIELD = 4
};

void fillByteField(unsigned char *buf, enum byteField field,
                   unsigned char byte);

void setBCCField(unsigned char *buf);

bool checkBCCField(unsigned char *buf);

void printfBuf(unsigned char *buf);

#endif // MSGUTILS_H
