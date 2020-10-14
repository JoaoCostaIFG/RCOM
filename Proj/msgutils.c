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

void assembleOpenMsg(struct linkLayer *linkLayer,
                     enum applicationStatus appStatus) {
  fillByteField(linkLayer->frame, FLAG1_FIELD, FLAG);
  if (appStatus == TRANSMITTER) {
    fillByteField(linkLayer->frame, A_FIELD, A_SENDER);
    fillByteField(linkLayer->frame, C_FIELD, C_SET);
  } else {
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

char calcBCCField(char *buf) { return (buf[A_FIELD] ^ buf[C_FIELD]); }

bool checkBCCField(char *buf) { return calcBCCField(buf) == buf[BCC_FIELD]; }

void printfBuf(char *buf) {
  for (int i = 0; i < MAX_SIZE; ++i)
    printf("%X ", buf[i]);
  printf("\n");
}

transitions_enum byteToTransitionSET(char byte, char *buf,
                                     state_enum curr_state) {
  transitions_enum transition;
  if (curr_state == C_ST && byte == calcBCCField(buf)) {
    transition = BCC_RCV;
    buf[BCC_FIELD] = calcBCCField(buf);
  } else {
    switch (byte) {
    case FLAG:
      transition = FLAG_RCV;
      if (curr_state == START_ST)
        buf[FLAG1_FIELD] = FLAG;
      else if (curr_state == BCC_ST)
        buf[FLAG2_FIELD] = FLAG;
      break;
    case A_SENDER:
      transition = A_RCV;
      buf[A_FIELD] = byte;
      break;
    case C_SET:
      transition = C_RCV;
      buf[C_FIELD] = byte;
      break;
    default:
      transition = OTHER_RCV;
      break;
    }
  }

  return transition;
}

transitions_enum byteToTransitionUA(char byte, char *buf,
                                     state_enum curr_state) {
  transitions_enum transition;
  if (curr_state == C_ST && byte == calcBCCField(buf)) {
    transition = BCC_RCV;
    buf[BCC_FIELD] = calcBCCField(buf);
  } else {
    switch (byte) {
    case FLAG:
      transition = FLAG_RCV;
      if (curr_state == START_ST)
        buf[FLAG1_FIELD] = FLAG;
      else if (curr_state == BCC_ST)
        buf[FLAG2_FIELD] = FLAG;
      break;
    case A_SENDER:
      transition = A_RCV;
      buf[A_FIELD] = byte;
      break;
    case C_UA:
      transition = C_RCV;
      buf[C_FIELD] = byte;
      break;
    default:
      transition = OTHER_RCV;
      break;
    }
  }

  return transition;
}
