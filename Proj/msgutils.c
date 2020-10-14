#include <stdio.h>
#include <string.h>

#include "msgutils.h"

struct linkLayer initLinkLayer() {
  struct linkLayer linkLayer;
  linkLayer.baudRate = BAUDRATE;
  linkLayer.sequenceNumber = 0;
  linkLayer.timeout = TIMEOUT;
  linkLayer.numTransmissions = MAXATTEMPTS;

  return linkLayer;
}

void fillByteField(char *buf, enum SUByteField field, char byte) {
  buf[field] = byte;
}

void assembleOpenMsg(struct linkLayer *linkLayer, enum applicationStatus appStatus) {
  fillByteField(linkLayer->frame, FLAG1_FIELD, FLAG);
  if (appStatus == TRANSMITTER) {
    fillByteField(linkLayer->frame, A_FIELD, A_SENDER);
    fillByteField(linkLayer->frame, C_FIELD, C_SET);
  }
  else {
    fillByteField(linkLayer->frame, A_FIELD, A_RECEIVER);
    fillByteField(linkLayer->frame, C_FIELD, C_UA);
  }
  fillByteField(linkLayer->frame, FLAG2_FIELD, FLAG);
  setBCCField(linkLayer->frame);
}

void setBCCField(char *buf) {
  unsigned char bcc = buf[A_FIELD] ^ buf[C_FIELD];
  fillByteField(buf, BCC_FIELD, bcc);
}

char calcBCCField(char *buf) {
  return (buf[A_FIELD] ^ buf[C_FIELD]);
}

bool checkBCCField(char *buf) {
  return calcBCCField(buf) == buf[BCC_FIELD];
}

void printfBuf(char *buf) {
  for (int i = 0; i < MAX_SIZE; ++i)
    printf("%X ", buf[i]);
  printf("\n");
}

transitions_enum byteToTransition(char byte, char* buf, enum applicationStatus appStatus) {
  transitions_enum transition;
  switch (byte) {
    case FLAG:
      transition = FLAG_RCV;
      buf[FLAG1_FIELD] = byte;
      break;
    case A_SENDER:
    /* case C_SET: */
      transition = A_RCV;
      buf[A_FIELD] = byte;

      if (appStatus == RECEIVER) {
        transition = C_RCV;
        buf[C_FIELD] = byte;
      }
      else
        transition = OTHER_RCV;
      break;
    case C_UA:
      if (appStatus == TRANSMITTER) {
        transition = C_RCV;
        buf[C_FIELD] = byte;
      }
      else
        transition = OTHER_RCV;
      break;
    default:
      if (byte == (buf[A_FIELD] ^ buf[C_FIELD]))
        transition = BCC_RCV;
      else
        transition = OTHER_RCV;
      break;
  }

  return transition;
}
