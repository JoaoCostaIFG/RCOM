/*Non-Canonical Input Processing*/

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "msgutils.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

struct rcv_file {
  unsigned char *file_content;
  size_t file_size;
};

volatile int STOP = false;
static struct applicationLayer appLayer;
static struct linkLayer linkLayer;

void sendUAMsg() {
  assembleSUPacket(&linkLayer, UA_MSG);

  // send msg
  fprintf(stderr, "Sending UA.\n");
  /* if (sendAndAlarm(&linkLayer, appLayer.fd) < 0) { */
  int res = write(appLayer.fd, linkLayer.frame, linkLayer.frameSize);
  if (res < 0) {
    perror("Failed sending UA");
    exit(1);
  }
  fprintf(stderr, "Sent UA.\n");
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

void inputLoopSET() {
  unsigned char currByte, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  state curr_state = START_ST;
  transitions transition;

  fprintf(stderr, "Getting SET.\n");
  while (curr_state != STOP_ST) {
    res = read(appLayer.fd, &currByte, sizeof(unsigned char));
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

int main(int argc, char **argv) {
  struct termios oldtio, newtio;
  linkLayer = initLinkLayer();
  appLayer.status = RECEIVER;

  if (argc < 2) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }
  strcpy(linkLayer.port, argv[1]);

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  appLayer.fd = open(linkLayer.port, O_RDWR | O_NOCTTY);
  if (appLayer.fd < 0) {
    perror(linkLayer.port);
    exit(-1);
  }

  if (tcgetattr(appLayer.fd, &oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = linkLayer.baudRate | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
  */
  newtio.c_cc[VTIME] = 0; /* inter-unsigned character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 5 unsigned chars received */
  // clear queue
  tcflush(appLayer.fd, TCIOFLUSH);

  // set new struct
  if (tcsetattr(appLayer.fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  // read string
  inputLoopSET();
  sendUAMsg();
  struct rcv_file rcv_file = getFile();

  FILE *fp = fopen("carlos.gif", "w+");
  if (fp == NULL) {
    perror("carlos.gif");
  } else {
    fwrite(rcv_file.file_content, sizeof(unsigned char), rcv_file.file_size, fp);
    fclose(fp);
  }
  free(rcv_file.file_content);

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(appLayer.fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(appLayer.fd);
  return 0;
}
