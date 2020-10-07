#include <string.h>

#include "msgutils.h"

void fillByteField(unsigned char *buf, enum byteField field,
                   unsigned char byte) {
  buf[field] = byte;
}
