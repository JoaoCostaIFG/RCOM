#include <string.h>

#include "msgutils.h"

void fillByteField(unsigned char *buf, enum byteField field,
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
