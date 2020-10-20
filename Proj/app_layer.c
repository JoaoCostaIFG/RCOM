#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "app_layer.h"
#include "data_link.h"

static struct linkLayer linkLayer;
static struct termios oldtio;

// TODO char* porta ou int porta
int llopen(char *porta, enum applicationStatus appStatus) {
  linkLayer = initLinkLayer();
  strcpy(linkLayer.port, porta);

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  int fd = open(linkLayer.port, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror(porta);
    return -1;
  }

  if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    return -2;
  }

  struct termios newtio;
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
  tcflush(fd, TCIOFLUSH);

  // set new struct
  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    return -3;
  }

  // handshake (receiver starts first)
  // SET ->
  // UA <-
  if (appStatus == RECEIVER) {
    inputLoopSET(&linkLayer, fd);
    if (sendUAMsg(&linkLayer, fd) < 0)
      return -5;
    /* struct rcv_file rcv_file = getFile(); */
  }
  else if (appStatus == TRANSMITTER) {
    if (sendSetMsg(&linkLayer, fd) < 0)
      return -5;
    inputLoopUA(&linkLayer, fd);
    /* sendFile(FILETOSEND); */
  }
  else {
    return -4;
  }

  return fd;
}

int llwrite(int fd, char *buffer, int length) {
  return 0;
}

int llread(int fd, char *buffer) {
  return 0;
}

int llclose(int fd) {
  // TODO discs go here

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    return -1;
  }

  return close(fd);
}
