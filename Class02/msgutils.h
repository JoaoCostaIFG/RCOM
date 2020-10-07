#ifndef MSGUTILS_H
#define MSGUTILS_H

enum byteField {FLAG1, A, C, BCC, FLAG2};

void fillByteField(unsigned char* buf, enum byteField field, unsigned char* byte);

#endif // MSGUTILS_H
