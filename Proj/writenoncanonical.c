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

#include "msgutils.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

volatile int STOP = false;
static int attempts = MAXATTEMPTS;

void sendSetMsg() {
  // assemble msg
  fillByteField(buf, FLAG1_FIELD, FLAG);
  fillByteField(buf, A_FIELD, A_SENDER);
  fillByteField(buf, C_FIELD, C_SET);
  fillByteField(buf, FLAG2_FIELD, FLAG);
  setBCCField(buf);

  // send msg
  int res;
  res = write(fd, buf, 5 * sizeof(char));
  printf("\t%d bytes written\n", res);
  printf("Sent msg: ");
  printfBuf(buf);
}

void alrmHandler(int signo) {
  if (signo != SIGALRM)
    return;

  if (attempts <= 0) {
    fprintf(stderr, "Waiting for answer timedout\n");
    exit(-1);
  }

  --attempts;
  sendSetMsg();
  alarm(TIMEOUT);
}

void inputLoop() {
  int res = 0;
  while (STOP == false) {
    /* returns after VMIN chars have been input */
    res += read(fd, buf + res, MAX_SIZE);
    if (res >= MAX_SIZE)
      STOP = true;
  }

  printf("Received msg: ");
  printfBuf(buf);

  if (checkBCCField(buf)) {
    printf("Message is OK!\n");
  }
  else {
    printf("Message is not OK! BCC field check failed.\n");
    inputLoop(); // TODO
  }
}

int main(int argc, char **argv) {
  struct termios oldtio, newtio;

  if (argc < 2) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

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

  fd = open(argv[1], O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror(argv[1]);
    exit(-1);
  }

  if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
  */
  newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
  newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */

  // clear queue
  tcflush(fd, TCIOFLUSH);
  // set new struct
  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }
  printf("New termios structure set\n");

  sendSetMsg();
  alarm(TIMEOUT); // set alarm for timeout/retry

  // input loop

  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
