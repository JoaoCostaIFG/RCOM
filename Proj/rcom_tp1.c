/*Non-Canonical Input Processing*/

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "app_layer.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

struct rcv_file {
  unsigned char *file_content;
  size_t file_size;
};

static struct applicationLayer appLayer;

void alrmHandler(int signo) {
  if (signo != SIGALRM)
    return;

  --linkLayer.numTransmissions;
  if (linkLayer.numTransmissions <= 0) {
    fprintf(stderr, "Waiting for answer timedout\n");
    exit(1);
  }

  if (sendAndAlarm(&linkLayer, appLayer.fd) < 0) {
    perror("Failed re-sending last message");
    exit(1);
  }
}

void sendRRMsg() {
  FLIPSEQUENCENUMBER(linkLayer);
  assembleSUPacket(&linkLayer, RR_MSG);

  // send msg
  fprintf(stderr, "Sending RR %d.\n", linkLayer.sequenceNumber);
  int res = write(appLayer.fd, linkLayer.frame, linkLayer.frameSize);
  if (res < 0) {
    perror("Failed sending RR");
    exit(1);
  }
  fprintf(stderr, "Sent RR %d.\n", linkLayer.sequenceNumber);
}

void sendREJMsg() {
  assembleSUPacket(&linkLayer, REJ_MSG);

  // send msg
  fprintf(stderr, "Sending REJ %d.\n", linkLayer.sequenceNumber);
  int res = write(appLayer.fd, linkLayer.frame, linkLayer.frameSize);
  if (res < 0) {
    perror("Failed sending REJ");
    exit(1);
  }
  fprintf(stderr, "Sent REJ %d.\n", linkLayer.sequenceNumber);
}

struct rcv_file getFile() {
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

  while (1) {
    bufLen = 0;
    isNextEscape = false;
    curr_state = START_ST;

    while (curr_state != STOP_ST) {
      res = read(appLayer.fd, &currByte, sizeof(unsigned char));
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
    if (linkLayer.sequenceNumber == 0) {
      if (buf[C_FIELD] == C_CTRL1)
        isOk = false;
    } else if (buf[C_FIELD] == C_CTRL0)
      isOk = false;

    if (isOk) {
      sendRRMsg();
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
      sendREJMsg();
    }
  }

  return rcv_file;
}

#define FILETOSEND "pinguim.gif2"
void sendFile(char *file_name) {
  int fp = open(file_name, O_RDONLY);
  if (fp < 0) {
    perror(FILETOSEND);
    exit(1);
  }

  unsigned char file_content[MAX_SIZE / 2];
  int size = read(fp, file_content, MAX_SIZE / 2);
  while (size > 0) {
    // send info fragment
    assembleInfoPacket(&linkLayer, file_content, size);
    sendAndAlarmReset(&linkLayer, appLayer.fd);
    size = read(fp, file_content, MAX_SIZE / 2);

    // Get RR/REJ answer
    int nextSeqNum = NEXTSEQUENCENUMBER(linkLayer);
    bool okAnswer = false;
    while (!okAnswer) {
      fprintf(stderr, "Getting RR/REJ %d.\n", linkLayer.sequenceNumber);

      unsigned char currByte, buf[MAX_SIZE];
      int res, bufLen = 0;
      state curr_state = START_ST;
      transitions transition;

      while (curr_state != STOP_ST) {
        res = read(appLayer.fd, &currByte, sizeof(unsigned char));
        if (res <= 0)
          perror("RR/REJ read");

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
        fprintf(stderr, "Got RR %d.\n", linkLayer.sequenceNumber);
      } else if (buf[C_FIELD] == (C_REJ | (nextSeqNum << 7))) {
        // reset attempts (we got an answer)
        linkLayer.numTransmissions = MAXATTEMPTS;
        fprintf(stderr, "Got REJ %d.\n", linkLayer.sequenceNumber);
      }
    }

    alarm(0); // reset pending alarm
  }
}

void print_usage() {
  fprintf(stderr, "Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
  exit(-1);
}

int main(int argc, char **argv) {
  // parse args
  if (argc < 3)
    print_usage();

  enum applicationStatus appStatus;
  if (strcmp(argv[1], "TRANSMITTER"))
    appLayer.status = TRANSMITTER;
  else if (strcmp(argv[1], "RECEIVER"))
    appLayer.status = RECEIVER;
  else
    print_usage();

  /* Set alarm signal handler */
  struct sigaction sa;
  sa.sa_handler = alrmHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGALRM, &sa, NULL) == -1) {
    perror("setting alarm handler failed");
    exit(-1);
  }

  appLayer.fd = llopen(argv[2], appStatus);
  if (appLayer.fd < 0) {
    fprintf(stderr, "llopen() failed");
    exit(-1);
  }

  llclose(appLayer.fd);
  return 0;
}
