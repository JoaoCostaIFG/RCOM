#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "data_link_priv.h"

static volatile bool needResend = false;

/* ALARM */
void alrmHandler(int signo) {
  if (signo != SIGALRM)
    return;

  needResend = true;
}

int resendHandler(struct linkLayer *linkLayer, int fd) {
  printf("RESEND\n");
  fflush(stdout);
  if (errno != EINTR || !needResend) // the failure wasn't caused by the alarm
    return -1;
  needResend = false;

  --linkLayer->numTransmissions;
  if (linkLayer->numTransmissions <= 0) {
    fprintf(stderr, "resendHandler(): Failed waiting for answer timedout\n");
    return -2;
  }

  if (sendAndAlarm(linkLayer, fd) < 0) {
    perror("Failed re-sending last message");
    return -3;
  }

  return 0;
}

/* BASICS */
struct linkLayer initLinkLayer() {
  struct linkLayer linkLayer;
  setBaudRate(&linkLayer, DFLTBAUDRATE);
  linkLayer.sequenceNumber = 0;
  linkLayer.timeout = TIMEOUT;
  linkLayer.numTransmissions = MAXATTEMPTS;
  linkLayer.frame = new_vector();
  vec_reserve(linkLayer.frame, MAX_SIZE);

  /* Set alarm signal handler */
  struct sigaction sa;
  sa.sa_handler = alrmHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGALRM, &sa, NULL) == -1) {
    perror("setting alarm handler failed");
  }

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

int stuffString(unsigned char *str, unsigned char *res, int size) {
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
void assembleSUFrame(struct linkLayer *linkLayer,
                     enum SUMessageType messageType) {
  fillByteField(linkLayer->frame->data, FLAG1_FIELD, FLAG);
  fillByteField(linkLayer->frame->data, A_FIELD, A_SENDER);
  fillByteField(linkLayer->frame->data, FLAG2_FIELD, FLAG);

  // set C_FIELD
  switch (messageType) {
  case SET_MSG:
    fillByteField(linkLayer->frame->data, C_FIELD, C_SET);
    break;
  case DISCSEND_MSG:
    fillByteField(linkLayer->frame->data, C_FIELD, C_DISC);
    break;
  case DISCRECV_MSG:
    fillByteField(linkLayer->frame->data, C_FIELD, C_DISC);
    fillByteField(linkLayer->frame->data, A_FIELD, A_RCV);
    break;
  case UASEND_MSG:
    fillByteField(linkLayer->frame->data, C_FIELD, C_UA);
    break;
  case UARECV_MSG:
    fillByteField(linkLayer->frame->data, C_FIELD, C_UA);
    fillByteField(linkLayer->frame->data, A_FIELD, A_RCV);
    break;
  case RR_MSG:
    fillByteField(linkLayer->frame->data, C_FIELD,
                  C_RR | (linkLayer->sequenceNumber << 7));
    break;
  case REJ_MSG:
    fillByteField(linkLayer->frame->data, C_FIELD,
                  C_REJ | (linkLayer->sequenceNumber << 7));
    break;
  default: // ?
    fillByteField(linkLayer->frame->data, C_FIELD, 0);
    break;
  }

  setBCCField(linkLayer->frame->data);
  linkLayer->frame->end = 5;
}

int assembleInfoFrame(struct linkLayer *linkLayer, unsigned char *buf,
                      int size) {
  fillByteField(linkLayer->frame->data, FLAG1_FIELD, FLAG);
  fillByteField(linkLayer->frame->data, A_FIELD, A_SENDER);
  fillByteField(linkLayer->frame->data, C_FIELD,
                (linkLayer->sequenceNumber << 6));
  setBCCField(linkLayer->frame->data);

  // in the worst case, every byte is stuffed
  unsigned char *stuffed_string =
      (unsigned char *)malloc(sizeof(unsigned char) * size * 2);
  if (stuffed_string == NULL) {
    perror("assembleInfoFrame malloc");
    return -1;
  }
  int new_size = stuffString(buf, stuffed_string, size);
  int i = HEADER_LEN;

  vec_reserve(linkLayer->frame,
              HEADER_LEN + new_size + 3); // BCC2 (x2) + end flag
  memcpy(linkLayer->frame->data + i, stuffed_string, new_size);
  free(stuffed_string);

  unsigned char bcc2_res[2], bcc2 = calcBCC2Field(buf, size);
  int bcc2_size = stuffByte(bcc2, bcc2_res);
  linkLayer->frame->data[new_size + (i++)] = bcc2_res[0];

  if (bcc2_size == 2)
    linkLayer->frame->data[new_size + (i++)] = bcc2_res[1];

  linkLayer->frame->data[new_size + i] = FLAG;

  linkLayer->frame->end = new_size + i + 1;
  return new_size + i;
}

/* WRITE FUNCTIONS */
int sendAndAlarm(struct linkLayer *linkLayer, int fd) {
  int res = write(fd, linkLayer->frame->data, linkLayer->frame->end);
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
                               state curr_state, bool isRecv) {
  transitions transition;
  if (curr_state == CS_ST && byte == calcBCCField(buf)) {
    transition = BCC_RCV;
  } else {
    switch (byte) {
    case FLAG:
      transition = FLAG_RCV;
      break;
    case A_RECEIVER:
      if (isRecv)
        transition = A_RCV;
      else
        transition = OTHER_RCV;
      break;
    case A_SENDER:
      if (!isRecv)
        transition = A_RCV;
      else
        transition = OTHER_RCV;
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
                                 state curr_state, bool isRecv) {
  transitions transition;
  if (curr_state == CS_ST && byte == calcBCCField(buf)) {
    transition = BCC_RCV;
  } else {
    switch (byte) {
    case FLAG:
      transition = FLAG_RCV;
      break;
    case A_RECEIVER:
      if (!isRecv) // if we are receiver, we want the sender byte
        transition = A_RCV;
      else
        transition = OTHER_RCV;
      break;
    case A_SENDER:
      if (isRecv)
        transition = A_RCV;
      else
        transition = OTHER_RCV;
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
int inputLoopSET(struct linkLayer *linkLayer, int fd) {
  unsigned char currByte = 0x00, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res == 0) {
      continue;
    } else if (res < 0) { // if we get interrupted, it might be the alarm
      if (resendHandler(linkLayer, fd) < 0) {
        return -1;
      }
    }

    transition = byteToTransitionSET(currByte, buf, curr_state);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST) {
      bufLen = 0;
    } else {
      buf[bufLen++] = currByte;
      if (curr_state == FLAG_ST)
        bufLen = 1;
    }
  }

  return 0;
}

int sendUAMsg(struct linkLayer *linkLayer, int fd, bool isRecv) {
  // when we are the receiver, we send the answer as transmitter
  assembleSUFrame(linkLayer, isRecv ? UASEND_MSG : UARECV_MSG);

  // send msg
  int res = write(fd, linkLayer->frame->data, linkLayer->frame->end);
  if (res < 0) {
    perror("Failed sending UA");
    return -1;
  }

  return 0;
}

/* transmiter */
int inputLoopUA(struct linkLayer *linkLayer, int fd, bool isRecv) {
  unsigned char currByte = 0x00, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res == 0) {
      continue;
    } else if (res < 0) { // if we get interrupted, it might be the alarm
      if (resendHandler(linkLayer, fd) < 0) {
        return -1;
      }
    }

    transition = byteToTransitionUA(currByte, buf, curr_state, isRecv);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST) {
      bufLen = 0;
    } else {
      buf[bufLen++] = currByte;
      if (curr_state == FLAG_ST)
        bufLen = 1;
    }
  }

  alarm(0); // cancel pending alarm
  return 0;
}

int sendSetMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUFrame(linkLayer, SET_MSG);

  // send msg and set alarm for timeout/retry
  if (sendAndAlarmReset(linkLayer, fd) < 0) {
    perror("Failed sending SET");
    return -1;
  }

  return 0;
}

/* llread BACKEND */
int sendRRMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUFrame(linkLayer, RR_MSG);

  // send msg
  int res = write(fd, linkLayer->frame->data, linkLayer->frame->end);
  if (res < 0) {
    perror("Failed sending RR");
    return -1;
  }

  return 0;
}

int sendREJMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUFrame(linkLayer, REJ_MSG);

  // send msg
  int res = write(fd, linkLayer->frame->data, linkLayer->frame->end);
  if (res < 0) {
    perror("Failed sending REJ");
    return -1;
  }

  return 0;
}

int getFrame(struct linkLayer *linkLayer, int fd, unsigned char **buffer) {
  unsigned char *packet = NULL;

  vector *buf = new_vector();
  if (buf == NULL) {
    perror("New Vec malloc"); // New log here TODO
    return -1;
  }

  bool isNextEscape = false;
  state curr_state = START_ST;
  unsigned char currByte = 0x00;
  transitions transition;

  while (curr_state != STOP_ST) {
    int res = read(fd, &currByte, sizeof(unsigned char));
    if (res < 0) {
      perror("getFrame read"); // TODO Log here
      free_vector(buf);
      return -2;
    }

    transition = byteToTransitionI(currByte, buf->data, curr_state);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST) {
      buf->end = 0;
    } else if (curr_state == FLAG_ST) {
      if (buf->end > 0)
        buf->end = 0;
      vec_push(buf, currByte);
    } else {
      if (isNextEscape) {
        --buf->end;
        currByte = destuffByte(currByte);
        isNextEscape = false;
      } else if (currByte == ESC) {
        isNextEscape = true;
      }

      if (vec_push(buf, currByte)) {
        // Log here vec realloc failed
        return -3;
      }
    }
  }

  bool isOk = true;
  if (!checkBCC2Field(buf->data + 4, buf->end - 6)) {
    // BCC2 is not ok -> REJ
    fprintf(stderr, "BCC2 is not OK!");
    isOk = false;
  }

  bool isRepeated = false;
  // Check sequence number for missed/duplicate packets
  if ((linkLayer->sequenceNumber == 0 && buf->data[C_FIELD] == C_CTRL1) ||
      (linkLayer->sequenceNumber == 1 && buf->data[C_FIELD] == C_CTRL0)) {
    isRepeated = true;
  }

  if (isOk && !isRepeated) {
    // all gud RR
    FLIPSEQUENCENUMBER(linkLayer);
    sendRRMsg(linkLayer, fd); // flips sequence number

    int info_size = (buf->end - 1) - 5; // Buflen is ahead by one
    packet = (unsigned char *)malloc(sizeof(unsigned char) * info_size);
    if (packet == NULL) {
      free_vector(buf); // TODO Log here
      return -4;
    }

    memcpy(packet, buf->data + DATA_FIELD, info_size);

    free_vector(buf);
    *buffer = packet;
    return info_size;
  } else if (isRepeated) {
    // repeated RR
    sendRRMsg(linkLayer, fd); // flips sequence number
    packet = NULL;
    free_vector(buf);
    return 0;
  } else {
    // REJ
    fprintf(stderr, "REJ\n");
    packet = NULL;
    sendREJMsg(linkLayer, fd);
    free_vector(buf);
    return 0;
  }
}

/* llwrite BACKEND */
int sendFrame(struct linkLayer *linkLayer, int fd, unsigned char *packet,
              int len) {
  // send info fragment
  assembleInfoFrame(linkLayer, packet, len);
  sendAndAlarmReset(linkLayer, fd);

  // Get RR/REJ answer
  int nextSeqNum = NEXTSEQUENCENUMBER(linkLayer);
  bool okAnswer = false;
  while (!okAnswer) {
    unsigned char currByte = 0x00, buf[MAX_SIZE];
    int res, bufLen = 0;
    state curr_state = START_ST;
    transitions transition;

    while (curr_state != STOP_ST) {
      res = read(fd, &currByte, sizeof(unsigned char));
      if (res == 0) {
        continue;
      } else if (res < 0) { // if we get interrupted, it might be the alarm
        if (resendHandler(linkLayer, fd) < 0) {
          return -1;
        }
      }

      transition = byteToTransitionRR(currByte, buf, curr_state);
      curr_state = state_machine[curr_state][transition];

      if (curr_state == START_ST) {
        bufLen = 0;
      } else {
        buf[bufLen++] = currByte;
        if (curr_state == FLAG_ST)
          bufLen = 1;
      }
    }

    // check the received answer
    if (buf[C_FIELD] == (C_RR | (nextSeqNum << 7))) {
      // RR
      // flip sequenceNumber
      okAnswer = true;
      FLIPSEQUENCENUMBER(linkLayer);
    } else if (buf[C_FIELD] == (C_RR | (linkLayer->sequenceNumber << 7))) {
      // RR repeated
      // do not flip sequenceNumber
      sendAndAlarmReset(linkLayer, fd);
    } else if (buf[C_FIELD] == (C_REJ | (linkLayer->sequenceNumber << 7))) {
      // REJ
      // sequence number isn't flipped
      // reset attempts (we got an answer) and resend
      sendAndAlarmReset(linkLayer, fd);
    }
  }

  alarm(0);
  return 0;
}

/*llclose BACKEND */
int sendDISCMsg(struct linkLayer *linkLayer, int fd, bool isRecv) {
  assembleSUFrame(linkLayer, isRecv ? DISCRECV_MSG : DISCSEND_MSG);

  if (sendAndAlarmReset(linkLayer, fd) < 0) {
    perror("Failed sending DISC");
    return -1;
  }

  return 0;
}

int inputLoopDISC(struct linkLayer *linkLayer, int fd, bool isRecv) {
  unsigned char currByte = 0x00, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res == 0) {
      continue;
    } else if (res < 0) { // if we get interrupted, it might be the alarm
      if (resendHandler(linkLayer, fd) < 0) {
        return -1;
      }
    }

    transition = byteToTransitionDISC(currByte, buf, curr_state, isRecv);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST) {
      bufLen = 0;
    } else {
      buf[bufLen++] = currByte;
      if (curr_state == FLAG_ST)
        bufLen = 1;
    }
  }

  alarm(0); // cancel pending alarm
  return 0;
}
