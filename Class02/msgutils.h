#ifndef MSGUTILS_H
#define MSGUTILS_H

#define FLAG 0x7e
#define A1 0x03
#define A2 0x01
#define C1 0x03
#define C2 0x07

enum byteField {FLAG1, A, C, BCC, FLAG2};

void fillByteField(unsigned char* buf, enum byteField field, unsigned char byte);

#endif // MSGUTILS_H
