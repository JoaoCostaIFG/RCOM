#include <stdio.h>

#include "data_link_priv.h"

int initConnection(struct linkLayer *linkLayer, int fd, bool isReceiver) {
  printf("\nAttempting to initialize a connection...\n");
  fflush(stdout);

  int failed = 0;
  if (isReceiver) {
    if (inputLoopSET(linkLayer, fd) < 0 || sendUAMsg(linkLayer, fd, true) < 0)
      failed = -1;
  } else {
    if (sendSetMsg(linkLayer, fd) < 0 || inputLoopUA(linkLayer, fd, false) < 0)
      failed = -1;
  }

  if (!failed) {
    printf("Successfully initialized connection...\n");
    fflush(stdout);
  }

  return failed;
}

int endConnection(struct linkLayer *linkLayer, int fd, bool isReceiver) {
  printf("\nEnding connection...\n");
  fflush(stdout);

  int failed = 0;
  if (isReceiver) {
    if (inputLoopDISC(linkLayer, fd, true) < 0 || sendDISCMsg(linkLayer, fd, true) < 0 ||
        inputLoopUA(linkLayer, fd, true) < 0)
      failed = 1;
  } else {
    if (sendDISCMsg(linkLayer, fd, false) < 0 || inputLoopDISC(linkLayer, fd, false) < 0 ||
        sendUAMsg(linkLayer, fd, false) < 0)
      failed = 1;
  }

  if (!failed) {
    printf("Successfully ended connection...\n");
    fflush(stdout);
  }

  return failed;
}

int setBaudRate(struct linkLayer *linkLayer, int baudrate) {
  switch (baudrate) {
  case 0:
    linkLayer->baudRate = B0;
    return B0;
  case 50:
    linkLayer->baudRate = B50;
    return B50;
  case 75:
    linkLayer->baudRate = B75;
    return B75;
  case 110:
    linkLayer->baudRate = B110;
    return B110;
  case 134:
    linkLayer->baudRate = B134;
    return B134;
  case 150:
    linkLayer->baudRate = B150;
    return B150;
  case 200:
    linkLayer->baudRate = B200;
    return B200;
  case 300:
    linkLayer->baudRate = B300;
    return B300;
  case 600:
    linkLayer->baudRate = B600;
    return B600;
  case 1200:
    linkLayer->baudRate = B1200;
    return B1200;
  case 1800:
    linkLayer->baudRate = B1800;
    return B1800;
  case 2400:
    linkLayer->baudRate = B2400;
    return B2400;
  case 4800:
    linkLayer->baudRate = B4800;
    return B4800;
  case 9600:
    linkLayer->baudRate = B9600;
    return B9600;
  case 19200:
    linkLayer->baudRate = B19200;
    return B19200;
  case 38400:
    linkLayer->baudRate = B38400;
    return B38400;
  case 57600:
    linkLayer->baudRate = B57600;
    return B57600;
  case 115200:
    linkLayer->baudRate = B115200;
    return B115200;
  case 230400:
    linkLayer->baudRate = B230400;
    return B230400;
  case 460800:
    linkLayer->baudRate = B460800;
    return B460800;
  default:
    linkLayer->baudRate = DFLTBAUDRATE;
    return -1;
  }
}
