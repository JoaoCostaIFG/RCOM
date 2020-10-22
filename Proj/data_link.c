#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "data_link.h"

static volatile bool needResend = false;

/* ALARM */
void alrmHandler(int signo) {
  if (signo != SIGALRM)
    return;

  needResend = true;
}

int resendHandler(struct linkLayer *linkLayer, int fd) {
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
  linkLayer.baudRate = BAUDRATE;
  linkLayer.sequenceNumber = 0;
  linkLayer.timeout = TIMEOUT;
  linkLayer.numTransmissions = MAXATTEMPTS;

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
void assembleSUFrame(struct linkLayer *linkLayer,
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

int assembleInfoFrame(struct linkLayer *linkLayer, unsigned char *buf,
                      int size) {
  fillByteField(linkLayer->frame, FLAG1_FIELD, FLAG);
  fillByteField(linkLayer->frame, A_FIELD, A_SENDER);
  fillByteField(linkLayer->frame, C_FIELD, (linkLayer->sequenceNumber << 6));
  setBCCField(linkLayer->frame);

  // in the worst case, every byte is stuffed
  unsigned char *stuffed_string =
      (unsigned char *)malloc(sizeof(unsigned char) * size * 2);
  if (stuffed_string == NULL) {
    perror("assembleInfoFrame malloc");
    return -1;
  }
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
  return new_size + i;
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
int inputLoopSET(struct linkLayer *linkLayer, int fd) {
  unsigned char currByte, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  fprintf(stderr, "Getting SET.\n");
  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res < 0) { // if we get interrupted, it might be the alarm
      if (resendHandler(linkLayer, fd) < 0) {
        return -1;
      }
    }

    transition = byteToTransitionSET(currByte, buf, curr_state);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST)
      bufLen = 0;
    else
      buf[bufLen++] = currByte;
  }

  fprintf(stderr, "Got SET.\n");
  return 0;
}

int sendUAMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUFrame(linkLayer, UA_MSG);

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
int inputLoopUA(struct linkLayer *linkLayer, int fd) {
  unsigned char currByte, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  fprintf(stderr, "Getting UA.\n");
  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res < 0) { // if we get interrupted, it might be the alarm
      if (resendHandler(linkLayer, fd) < 0) {
        return -1;
      }
    }

    transition = byteToTransitionUA(currByte, buf, curr_state);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST)
      bufLen = 0;
    else
      buf[bufLen++] = currByte;
  }

  alarm(0); // cancel pending alarm
  fprintf(stderr, "Got UA.\n");
  return 0;
}

int sendSetMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUFrame(linkLayer, SET_MSG);

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
  assembleSUFrame(linkLayer, RR_MSG);

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
  assembleSUFrame(linkLayer, REJ_MSG);

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

int getFrame(struct linkLayer *linkLayer, int fd, unsigned char **buffer) {
  unsigned char *packet = NULL;
  int max_buf_size = MAX_SIZE;
  unsigned char *buf =
      (unsigned char *)malloc(sizeof(unsigned char) * max_buf_size);
  if (buf == NULL) {
    perror("getFrame malloc");
    return -1;
  }

  int bufLen = 0;
  bool isNextEscape = false;
  state curr_state = START_ST;
  unsigned char currByte;
  transitions transition;

  while (curr_state != STOP_ST) {
    int res = read(fd, &currByte, sizeof(unsigned char));
    if (res <= 0) {
      perror("getFrame read");
      free(buf);
      return -2;
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

    if (bufLen >= max_buf_size) {
      max_buf_size *= 2;
      unsigned char *new_buf =
          (unsigned char *)realloc(buf, sizeof(unsigned char) * max_buf_size);
      if (new_buf == NULL) {
        perror("getFrame realloc");
        free(buf);
        return -3;
      }
      buf = new_buf;
    }
  }

  bool isOk = true;
  if (!checkBCC2Field(buf + 4, bufLen - 6)) {
    // BCC2 is not ok
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

    int info_size = (bufLen - 1) - 5; // Buflen is ahead by one
    packet = (unsigned char *)malloc(sizeof(unsigned char) * info_size);
    if (packet == NULL) {
      perror("getFrame info malloc");
      free(buf);
      return -4;
    }

    memcpy(packet, buf + DATA_FIELD, info_size);

    free(buf);
    *buffer = packet;
    return info_size;
  } else {
    packet = NULL;
    sendREJMsg(linkLayer, fd);
    free(buf);
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
    fprintf(stderr, "Getting RR/REJ %d.\n", linkLayer->sequenceNumber);

    unsigned char currByte, buf[MAX_SIZE];
    int res, bufLen = 0;
    state curr_state = START_ST;
    transitions transition;

    while (curr_state != STOP_ST) {
      res = read(fd, &currByte, sizeof(unsigned char));
      if (res < 0) { // if we get interrupted, it might be the alarm
        if (resendHandler(linkLayer, fd) < 0) {
          return -1;
        }
      }

      transition = byteToTransitionRR(currByte, buf, curr_state);
      curr_state = state_machine[curr_state][transition];

      if (curr_state == START_ST)
        bufLen = 0;
      else
        buf[bufLen++] = currByte;
    }

    if (buf[C_FIELD] == (C_RR | (nextSeqNum << 7))) {
      okAnswer = true;
      FLIPSEQUENCENUMBER(linkLayer);
      fprintf(stderr, "Got RR %d.\n", linkLayer->sequenceNumber);
    } else if (buf[C_FIELD] == (C_REJ | (nextSeqNum << 7))) {
      // reset attempts (we got an answer) and resend
      sendAndAlarmReset(linkLayer, fd);
      fprintf(stderr, "Got REJ %d.\n", linkLayer->sequenceNumber);
    }
  }

  alarm(0);
  return 0;
}

/*llclose BACKEND */
int sendUAMsg(struct linkLayer *linkLayer, int fd); // Defined in llopen BACKEND

int sendDISCMsg(struct linkLayer *linkLayer, int fd) {
  assembleSUFrame(linkLayer, DISC_MSG);

  fprintf(stderr, "Sending DISC.\n");
  if (sendAndAlarmReset(linkLayer, fd) < 0) {
    perror("Failed sending DISC");
    return -1;
  }
  fprintf(stderr, "Sent DISC.\n");

  return 0;
}

int inputLoopDISC(struct linkLayer *linkLayer, int fd) {
  unsigned char currByte, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  fprintf(stderr, "Getting DISC.\n");
  while (curr_state != STOP_ST) {
    res = read(fd, &currByte, sizeof(unsigned char));
    if (res < 0) { // if we get interrupted, it might be the alarm
      if (resendHandler(linkLayer, fd) < 0) {
        return -1;
      }
    }

    transition = byteToTransitionDISC(currByte, buf, curr_state);
    curr_state = state_machine[curr_state][transition];

    if (curr_state == START_ST)
      bufLen = 0;
    else
      buf[bufLen++] = currByte;
  }
  alarm(0); // cancel pending alarm

  fprintf(stderr, "Got DISC.\n");
  return 0;
}

int initConnection(struct linkLayer *linkLayer, int fd, bool isReceiver) {
  printf("\nAttempting to initialize a connection...\n");
  fflush(stdout);

  int failed = 0;
  if (isReceiver) {
    if (inputLoopSET(linkLayer, fd) < 0 || sendUAMsg(linkLayer, fd) < 0)
      failed = -1;
  }
  else {
    if (sendSetMsg(linkLayer, fd) < 0 || inputLoopUA(linkLayer, fd) < 0)
      failed = -1;
  }

  if (!failed) {
    printf("Successfully initialized connection...\n");
    fflush(stdout);
  }

  return failed;
}
