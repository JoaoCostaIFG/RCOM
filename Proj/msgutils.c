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
  setBCCField(linkLayer);
}

void setBCCField(struct linkLayer *linkLayer) {
  char *buf = linkLayer->frame;
  unsigned char bcc = buf[A_FIELD] ^ buf[C_FIELD];
  fillByteField(buf, BCC_FIELD, bcc);
}

bool checkBCCField(struct linkLayer *linkLayer) {
  char *buf = linkLayer->frame;
  return (buf[A_FIELD] ^ buf[C_FIELD]) == buf[BCC_FIELD];
}

void printfBuf(struct linkLayer *linkLayer) {
  char *buf = linkLayer->frame;
  for (int i = 0; i < MAX_SIZE; ++i)
    printf("%X ", buf[i]);
  printf("\n");
}

transitions_enum byteToTransition(char byte) {
  transitions_enum trans;
  switch (byte) {
  case FLAG:
    trans = FLAG_RCV;
    break;
  case A_RECEIVER:
    /*case A_SENDER:*/
    /*TODO halp trans = A_RCV;*/
    break;
  case C_SET:
  case C_RCV:
  case C_DISC:
    trans = C_RCV;
    break;
  }
  return trans;
}
