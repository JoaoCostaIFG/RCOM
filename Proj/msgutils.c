#include <stdio.h>
#include <string.h>

#include "msgutils.h"

void fillByteField(unsigned char *buf, enum SUByteField field,
                   unsigned char byte) {
  buf[field] = byte;
}

void setBCCField(unsigned char *buf) {
  unsigned char bcc = buf[A_FIELD] ^ buf[C_FIELD];
  fillByteField(buf, BCC_FIELD, bcc);
}

bool checkBCCField(unsigned char *buf) {
  return (buf[A_FIELD] ^ buf[C_FIELD]) == buf[BCC_FIELD];
}

void printfBuf(unsigned char *buf) {
  for (int i = 0; i < MSG_SIZE; ++i)
    printf("%X ", buf[i]);
  printf("\n");
}
