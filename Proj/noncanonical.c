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

void sendUaMsg() {
  assembleOpenMsg(&linkLayer, appLayer.status);

  fprintf(stderr, "Sending UA.\n");
  int res = write(appLayer.fd, linkLayer.frame, 5 * sizeof(char));
  fprintf(stderr, "Sent UA.\n");
  printfBuf(&linkLayer);
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
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
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

  if (tcsetattr(appLayer.fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  // read string
  fprintf(stderr, "Getting SET.\n");
  int res = 0;
  while (STOP == false) {
    res += read(appLayer.fd, linkLayer.frame + res, 255); /* returns after VMIN chars read */
    if (res == MAX_SIZE)
      STOP = true;
  }
  fprintf(stderr, "Got SET.\n");

  if (!checkBCCField(&linkLayer))
    fprintf(stderr, "SET BCC field error.");
  else {
    sendUaMsg();
  }

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(appLayer.fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(appLayer.fd);
  return 0;
}
