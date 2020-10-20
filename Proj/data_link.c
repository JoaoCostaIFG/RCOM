#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "data_link.h"

struct linkLayer initLinkLayer() {
  struct linkLayer linkLayer;
  linkLayer.baudRate = BAUDRATE;
  linkLayer.sequenceNumber = 0;
  linkLayer.timeout = TIMEOUT;
  linkLayer.numTransmissions = MAXATTEMPTS;

  return linkLayer;
}

void fillByteField(unsigned char *buf, enum SUByteField field,
                   unsigned char byte) {
  buf[field] = byte;
}

unsigned char calcBCCField(unsigned char *buf) {
  return (buf[A_FIELD] ^ buf[C_FIELD]);
}

bool checkBCCField(unsigned char *buf) {
  return calcBCCField(buf) == buf[BCC_FIELD];
}

void setBCCField(unsigned char *buf) {
  unsigned char bcc = buf[A_FIELD] ^ buf[C_FIELD];
  fillByteField(buf, BCC_FIELD, bcc);
}

unsigned char calcBCC2Field(unsigned char *buf, int size) {
  unsigned char ret = 0;
  for (int i = 0; i < size; ++i)
    ret ^= buf[i];
  return ret;
}

bool checkBCC2Field(unsigned char *buf, int size) {
  return calcBCC2Field(buf, size) == buf[size];
}

/* STRING STUFFING */
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

/* PACKET ASSEMBLY */
void assembleSUPacket(struct linkLayer *linkLayer,
                      enum SUMessageType messageType) {
  fillByteField(linkLayer->frame, FLAG1_FIELD, FLAG);
  fillByteField(linkLayer->frame, A_FIELD, A_SENDER);
  fillByteField(linkLayer->frame, FLAG2_FIELD, FLAG);

  // set C_FIELD
  switch (messageType) {
  case SET_MSG:
    fillByteField(linkLayer->frame, C_FIELD, C_SET);
    break;
  case DISC_MSG:
    fillByteField(linkLayer->frame, C_FIELD, C_DISC);
    break;
  case UA_MSG:
    fillByteField(linkLayer->frame, C_FIELD, C_UA);
    break;
  case RR_MSG:
    fillByteField(linkLayer->frame, C_FIELD,
                  C_RR | (linkLayer->sequenceNumber << 7));
    break;
  case REJ_MSG:
    fillByteField(linkLayer->frame, C_FIELD,
                  C_REJ | (linkLayer->sequenceNumber << 7));
    break;
  default: // ?
    fillByteField(linkLayer->frame, C_FIELD, 0);
    break;
  }

  setBCCField(linkLayer->frame);
  linkLayer->frameSize = 5;
}

void assembleInfoPacket(struct linkLayer *linkLayer, unsigned char *buf,
                        int size) {
  fillByteField(linkLayer->frame, FLAG1_FIELD, FLAG);
  fillByteField(linkLayer->frame, A_FIELD, A_SENDER);
  fillByteField(linkLayer->frame, C_FIELD, (linkLayer->sequenceNumber << 6));
  setBCCField(linkLayer->frame);

  unsigned char stuffed_string[MAX_SIZE];
  int new_size = stuffString(buf, stuffed_string, size);
  int i = 4;
  memcpy(linkLayer->frame + i, stuffed_string, new_size);

  unsigned char bcc_res[2], bcc = calcBCC2Field(buf, size);
  int bcc_size = stuffByte(bcc, bcc_res);
  linkLayer->frame[new_size + (i++)] = bcc_res[0];

  if (bcc_size == 2)
    linkLayer->frame[new_size + (i++)] = bcc_res[1];

  linkLayer->frame[new_size + (i++)] = FLAG;

  linkLayer->frameSize = new_size + i;
}

/* WRITE FUNCTIONS */
int sendAndAlarm(struct linkLayer *linkLayer, int fd) {
  int res = write(fd, linkLayer->frame, linkLayer->frameSize);
  // set alarm
  alarm(linkLayer->timeout);
  return res;
}

int sendAndAlarmReset(struct linkLayer *linkLayer, int fd) {
  // reset attempts
  linkLayer->numTransmissions = MAXATTEMPTS;
  return sendAndAlarm(linkLayer, fd);
}

/* STATE MACHINE */
transitions byteToTransitionSET(unsigned char byte, unsigned char *buf,
                                state curr_state) {
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

transitions byteToTransitionUA(unsigned char byte, unsigned char *buf,
                               state curr_state) {
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

transitions byteToTransitionI(unsigned char byte, unsigned char *buf,
                              state curr_state) {
  transitions transition;
  if (curr_state == CI_ST && byte == calcBCCField(buf)) {
    // TODO mandar REJ?
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

transitions byteToTransitionRR(unsigned char byte, unsigned char *buf,
                               state curr_state) {
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
    case C_RR:
    case C_RR | (1 << 7):
    case C_REJ:
    case C_REJ | (1 << 7):
      transition = CS_RCV;
      break;
    default:
      transition = OTHER_RCV;
      break;
    }
  }

  return transition;
}

transitions byteToTransitionDISC(unsigned char byte, unsigned char *buf,
                                 state curr_state) {
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
    case C_DISC:
      transition = CS_RCV;
      break;
    default:
      transition = OTHER_RCV;
      break;
    }
  }

  return transition;
}

/* llopen BACKEND */
/* receiver */
void inputLoopSET(struct linkLayer *linkLayer, int fd) {
  unsigned char currByte, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  fprintf(stderr, "Getting SET.\n");
  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res <= 0)
      perror("SET read");

    transition = byteToTransitionSET(currByte, buf, curr_state);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST)
      bufLen = 0;
    else
      buf[bufLen++] = currByte;
  }
  fprintf(stderr, "Got SET.\n");
}

int sendUAMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUPacket(linkLayer, UA_MSG);

  // send msg
  fprintf(stderr, "Sending UA.\n");
  int res = write(fd, linkLayer->frame, linkLayer->frameSize);
  if (res < 0) {
    perror("Failed sending UA");
    return -1;
  }
  fprintf(stderr, "Sent UA.\n");

  return 0;
}

/* transmiter */
void inputLoopUA(struct linkLayer *linkLayer, int fd) {
  unsigned char currByte, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  fprintf(stderr, "Getting UA.\n");
  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res <= 0)
      perror("Reading UA");

    transition = byteToTransitionUA(currByte, buf, curr_state);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST)
      bufLen = 0;
    else
      buf[bufLen++] = currByte;
  }
  alarm(0); // cancel pending alarm
  fprintf(stderr, "Got UA.\n");
}

int sendSetMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUPacket(linkLayer, SET_MSG);

  // send msg and set alarm for timeout/retry
  fprintf(stderr, "Sending SET.\n");
  if (sendAndAlarmReset(linkLayer, fd) < 0) {
    perror("Failed sending SET");
    return -1;
  }
  fprintf(stderr, "Sent SET.\n");

  return 0;
}

/* llread BACKEND */
int sendRRMsg(struct linkLayer *linkLayer, int fd) {
  FLIPSEQUENCENUMBER(linkLayer);
  assembleSUPacket(linkLayer, RR_MSG);

  // send msg
  fprintf(stderr, "Sending RR %d.\n", linkLayer->sequenceNumber);
  int res = write(fd, linkLayer->frame, linkLayer->frameSize);
  if (res < 0) {
    perror("Failed sending RR");
    return -1;
  }
  fprintf(stderr, "Sent RR %d.\n", linkLayer->sequenceNumber);

  return 0;
}

int sendREJMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUPacket(linkLayer, REJ_MSG);

  // send msg
  fprintf(stderr, "Sending REJ %d.\n", linkLayer->sequenceNumber);
  int res = write(fd, linkLayer->frame, linkLayer->frameSize);
  if (res < 0) {
    perror("Failed sending REJ");
    return -1;
  }
  fprintf(stderr, "Sent REJ %d.\n", linkLayer->sequenceNumber);

  return 0;
}

struct rcv_file getFile(struct linkLayer *linkLayer, int fd) {
  size_t fileMaxSize = MAX_SIZE;
  struct rcv_file rcv_file;
  rcv_file.file_size = 0;
  rcv_file.file_content = malloc(fileMaxSize * sizeof(unsigned char));

  if (rcv_file.file_content == NULL) {
    perror("Malloc doesn't like us, goodbye!");
    exit(-1);
  }

  unsigned char currByte, buf[MAX_SIZE];
  int res, bufLen;
  bool isNextEscape;
  transitions transition;
  state curr_state;

  while (1) { // TODO TERMINADO POR UMA TRAMA DE APP LAYER (END)
    bufLen = 0;
    isNextEscape = false;
    curr_state = START_ST;

    while (curr_state != STOP_ST) {
      res = read(fd, &currByte, sizeof(unsigned char));
      if (res <= 0) {
        break;
      }

      transition = byteToTransitionI(currByte, buf, curr_state);
      curr_state = state_machine[curr_state][transition];

      if (curr_state == START_ST) {
        bufLen = 0;
      } else {
        if (isNextEscape) {
          --bufLen;
          currByte = destuffByte(currByte);
          isNextEscape = false;
        } else if (currByte == ESC) {
          isNextEscape = true;
        }

        buf[bufLen++] = currByte;
      }
    }

    bool isOk = true;
    if (!checkBCC2Field(buf + 4, bufLen - 6)) { // BCC2 is not ok
      fprintf(stderr, "BCC2 is not OK!");
      isOk = false;
    }

    // Check sequence number for missed/duplicate packets
    if (linkLayer->sequenceNumber == 0) {
      if (buf[C_FIELD] == C_CTRL1)
        isOk = false;
    } else if (buf[C_FIELD] == C_CTRL0)
      isOk = false;

    if (isOk) {
      sendRRMsg(linkLayer, fd);
      if (rcv_file.file_size + bufLen - 6 >= fileMaxSize) {
        // increase alloced size
        fileMaxSize *= 2;
        rcv_file.file_content =
            realloc(rcv_file.file_content, fileMaxSize * sizeof(unsigned char));
        if (rcv_file.file_content == NULL) {
          perror("Realloc also doesn't like us, goodbye!");
          exit(-1);
        }
      }

      memcpy(rcv_file.file_content + rcv_file.file_size, buf + 4,
             (bufLen - 6) * sizeof(unsigned char));
      rcv_file.file_size += bufLen - 6;
    } else {
      sendREJMsg(linkLayer, fd);
    }
  }

  return rcv_file;
}

/* llwrite BACKEND */

/*llclose BACKEND */
int sendUAMsg(struct linkLayer *linkLayer, int fd); // Defined in llopen BACKEND

int sendDISCMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUPacket(linkLayer, DISC_MSG);

  fprintf(stderr, "Sending DISC.\n");
  if (sendAndAlarmReset(linkLayer, fd) < 0) {
    perror("Failed sending DISC");
    return -1;
  }
  fprintf(stderr, "Sent DISC.\n");

  return 0;
}

void inputLoopUA(struct linkLayer *linkLayer, int fd);

void inputLoopDISC(struct linkLayer *linkLayer, int fd) {
  unsigned char currByte, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  fprintf(stderr, "Getting DISC.\n");
  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res <= 0)
      perror("Reading DISC");

    transition = byteToTransitionDISC(currByte, buf, curr_state);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST)
      bufLen = 0;
    else
      buf[bufLen++] = currByte;
  }
  alarm(0); // cancel pending alarm

  fprintf(stderr, "Got DISC.\n");
}
