#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "msgutils.h"

int sendAndAlarm(struct linkLayer *linkLayer, int fd) {
  int res = write(fd, linkLayer->frame, linkLayer->frameSize);
  // reset and set alarm
  linkLayer->numTransmissions = MAXATTEMPTS;
  alarm(linkLayer->timeout);
  return res;
}

struct linkLayer initLinkLayer() {
  struct linkLayer linkLayer;
  linkLayer.baudRate = BAUDRATE;
  linkLayer.sequenceNumber = 0;
  linkLayer.timeout = TIMEOUT;
  linkLayer.numTransmissions = MAXATTEMPTS;

  return linkLayer;
}

void fillByteField(unsigned char *buf, enum SUByteField field, unsigned char byte) {
  buf[field] = byte;
}

void assembleOpenPacket(struct linkLayer *linkLayer,
                        enum applicationStatus appStatus) {
  fillByteField(linkLayer->frame, FLAG1_FIELD, FLAG);
  fillByteField(linkLayer->frame, A_FIELD, A_SENDER);
  if (appStatus == TRANSMITTER) {
    fillByteField(linkLayer->frame, C_FIELD, C_SET);
  } else {
    fillByteField(linkLayer->frame, C_FIELD, C_UA);
  }
  fillByteField(linkLayer->frame, FLAG2_FIELD, FLAG);
  setBCCField(linkLayer->frame);

  linkLayer->frameSize = 5;
}

static unsigned char calcBCC2Field(unsigned char *buf, int size) {
  unsigned char ret = 0;
  for (int i = 0; i < size; ++i)
    ret ^= buf[i];
  return ret;
}

void assembleInfoPacket(struct linkLayer *linkLayer, unsigned char *buf, int size) {
  fillByteField(linkLayer->frame, FLAG1_FIELD, FLAG);
  fillByteField(linkLayer->frame, A_FIELD, A_SENDER);
  fillByteField(linkLayer->frame, C_FIELD, linkLayer->sequenceNumber);
  setBCCField(linkLayer->frame);

  unsigned char stuffed_string[MAX_SIZE];
  int new_size = stuffString(buf, stuffed_string, size);
  int i = 4;
  memcpy(linkLayer->frame + i, stuffed_string, new_size);

  unsigned char bcc_res[2], bcc = calcBCC2Field(buf, new_size);
  int bcc_size = stuffByte(bcc, bcc_res);
  linkLayer->frame[new_size + (i++)] = bcc_res[0];

  if (bcc_size == 2)
    linkLayer->frame[new_size + (i++)] = bcc_res[1];

  linkLayer->frame[new_size + (i++)] = FLAG;

  linkLayer->frameSize = new_size + i;
}

void setBCCField(unsigned char *buf) {
  unsigned char bcc = buf[A_FIELD] ^ buf[C_FIELD];
  fillByteField(buf, BCC_FIELD, bcc);
}

unsigned char calcBCCField(unsigned char *buf) { return (buf[A_FIELD] ^ buf[C_FIELD]); }

bool checkBCCField(unsigned char *buf) { return calcBCCField(buf) == buf[BCC_FIELD]; }

void printfBuf(unsigned char *buf) {
  for (int i = 0; i < MAX_SIZE; ++i)
    printf("%X ", buf[i]);
  printf("\n");
}

transitions byteToTransitionSET(unsigned char byte, unsigned char *buf, state curr_state) {
  transitions transition;
  if (curr_state == CS_ST && byte == calcBCCField(buf)) {
    transition = BCC_RCV;
  } else {
    switch (byte) {
    case FLAG:
      transition = FLAG_RCV;
      break;
    case A_SENDER: // == C_SET
      if (curr_state == FLAG_ST) {
        transition = A_RCV;
      } else if (curr_state == A_ST) {
        transition = CS_RCV;
      }
      break;
    default:
      transition = OTHER_RCV;
      break;
    }
  }

  return transition;
}

transitions byteToTransitionUA(unsigned char byte, unsigned char *buf, state curr_state) {
  transitions transition;
  if (curr_state == CS_ST && byte == calcBCCField(buf)) {
    transition = BCC_RCV;
  } else {
    switch (byte) {
    case FLAG:
      transition = FLAG_RCV;
      break;
    case A_SENDER:
      transition = A_RCV;
      break;
    case C_UA:
      transition = CS_RCV;
      break;
    default:
      transition = OTHER_RCV;
      break;
    }
  }

  return transition;
}

transitions byteToTransitionI(unsigned char byte, unsigned char *buf, state curr_state) {
  transitions transition;
  if (curr_state == CI_ST && byte == calcBCCField(buf)) {
    transition = BCC_RCV;
  } else {
    switch (byte) {
    case FLAG:
      transition = FLAG_RCV;
      break;
    case A_SENDER:
      transition = A_RCV;
      break;
    case C_CTRL0:
    case C_CTRL1:
      transition = CI_RCV;
      break;
    default:
      transition = OTHER_RCV;
      break;
    }
  }

  return transition;
}

int stuffByte(unsigned char byte, unsigned char res[]) {
  int bytes_returned = 1;
  if (byte == ESC) {
    res[0] = ESC;
    res[1] = ESC ^ STUFF_BYTE;
    bytes_returned = 2;
  } else if (byte == FLAG) {
    res[0] = ESC;
    res[1] = FLAG ^ STUFF_BYTE;
    bytes_returned = 2;
  } else
    res[0] = byte;
  return bytes_returned;
}

unsigned char destuffByte(unsigned char byte) { return byte ^ STUFF_BYTE; }

int stuffString(unsigned char str[], unsigned char res[], int size) {
  int j = 0;
  for (int i = 0; i < size; ++i) {
    unsigned char stuffedBytes[2];
    int n = stuffByte(str[i], stuffedBytes);
    res[j++] = stuffedBytes[0];
    if (n > 1)
      res[j++] = stuffedBytes[1];
  }
  return j;
}
