#include <string.h>

#include "msgutils.h"

void fillByteField(unsigned char* buf, enum byteField field, unsigned char byte) {
  switch (field) {
    case FLAG1:
      buf[0] = byte;
      break;
    case A:
      buf[1] = byte;
      break;
    case C:
      buf[2] = byte;
      break;
    case BCC:
      buf[3] = byte;
      break;
    case FLAG2:
      buf[4] = byte;
      break;
    default:
      break;
  }
}
