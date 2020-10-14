/*Non-Canonical Input Processing*/

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "msgutils.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

volatile int STOP = false;
static struct applicationLayer appLayer;
static struct linkLayer linkLayer;

void sendSetMsg() {
  assembleOpenPacket(&linkLayer, appLayer.status);

  // send msg
  fprintf(stderr, "Sending SET.\n");
  int res = write(appLayer.fd, linkLayer.frame, linkLayer.frameSize);
  fprintf(stderr, "Sent SET.\n");
}

void alrmHandler(int signo) {
  if (signo != SIGALRM)
    return;

  if (linkLayer.numTransmissions <= 0) {
    fprintf(stderr, "Waiting for answer timedout\n");
    exit(-1);
  }

  --linkLayer.numTransmissions;
  int res = write(appLayer.fd, linkLayer.frame, linkLayer.frameSize);
  alarm(TIMEOUT);
}

void inputLoop() {
  char currByte, buf[MAX_SIZE];
  int res = 0;
  state_enum curr_state = START_ST;
  transitions_enum transition;

  fprintf(stderr, "Getting UA.\n");
  while (curr_state != STOP_ST) {
    res = read(appLayer.fd, &currByte, sizeof(char));
    if (res <= 0)
      perror("UA read.");

    transition = byteToTransitionUA(currByte, buf, curr_state);
    curr_state = event_matrix[curr_state][transition];
  }
  alarm(0); // cancel pending alarm
  fprintf(stderr, "Got UA.\n");
}

int main(int argc, char **argv) {
  struct termios oldtio, newtio;
  linkLayer = initLinkLayer();
  appLayer.status = TRANSMITTER;

  if (argc < 2) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }
  strcpy(linkLayer.port, argv[1]);

  /* Set alarm signal handler */
  struct sigaction sa;
  sa.sa_handler = alrmHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGALRM, &sa, NULL) == -1) {
    perror("setting alarm handler failed");
    exit(-1);
  }

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
    leitura do(s) pr�ximo(s) caracter(es)
  */
  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */
  // clear queue
  tcflush(appLayer.fd, TCIOFLUSH);

  // set new struct
  if (tcsetattr(appLayer.fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  sendSetMsg();
  alarm(linkLayer.timeout); // set alarm for timeout/retry
  inputLoop();

  assembleInfoPacket(&linkLayer, "Ola Mundo!", 10);
  int res = sendCurrPacket(linkLayer, appLayer.fd);

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(appLayer.fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(appLayer.fd);
  return 0;
}

