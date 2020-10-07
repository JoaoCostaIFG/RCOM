#include <string.h>

#include "msgutils.h"

void fillByteField(unsigned char *buf, enum byteField field,
                   unsigned char byte) {
  buf[field] = byte;
}

void setBCCField(unsigned char *buf) {
  unsigned char bcc = buf[A] ^ buf[C];
  fillByteField(buf, BCC, bcc);
}
