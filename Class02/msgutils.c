#include <string.h>

#include "msgutils.h"

void fillByteField(unsigned char* buf, enum byteField field, unsigned char* byte) {
  switch (field) {
    case FLAG1:
      memcpy(buf, byte, 8 * sizeof(unsigned char));
      break;
    case A:
      memcpy(buf + 8, byte, 8 * sizeof(unsigned char));
      break;
    case C:
      memcpy(buf + 16, byte, 8 * sizeof(unsigned char));
      break;
    case BCC:
      memcpy(buf + 24, byte, 8 * sizeof(unsigned char));
      break;
    case FLAG2:
      memcpy(buf + 32, byte, 8 * sizeof(unsigned char));
      break;
    default:
      break;
  }
}
