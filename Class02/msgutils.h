#ifndef MSGUTILS_H
#define MSGUTILS_H

#define MSG_SIZE 5 // 5 Bytes
#define FLAG 0x7e
#define A_SENDER 0x03
#define A_RECEIVER 0x01
#define C1 0x03
#define C2 0x07

enum byteField { FLAG1 = 0, A = 1, C = 2, BCC = 3, FLAG2 = 4 };

void fillByteField(unsigned char *buf, enum byteField field,
                   unsigned char byte);

void setBCCField(unsigned char *buf);

#endif // MSGUTILS_H
