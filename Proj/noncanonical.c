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

volatile int STOP = false;
static struct applicationLayer appLayer;
static struct linkLayer linkLayer;

void sendUAMsg() {
  assembleOpenPacket(&linkLayer, appLayer.status);

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

void getFile() {
  unsigned char currByte, buf[MAX_SIZE];
  int res = 0, bufLen = 0;
  bool isNextEscape = false;
  state curr_state = START_ST;
  transitions transition;

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
  getFile();

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(appLayer.fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(appLayer.fd);
  return 0;
}
