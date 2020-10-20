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

static struct applicationLayer appLayer;

/*
 * void alrmHandler(int signo) {
 *   // TODO STOP volatile
     // or ver o interrupt do read com resend

 *   if (signo != SIGALRM)
 *     return;
 *
 *   --linkLayer.numTransmissions;
 *   if (linkLayer.numTransmissions <= 0) {
 *     fprintf(stderr, "Waiting for answer timedout\n");
 *     exit(1);
 *   }
 *
 *   if (sendAndAlarm(&linkLayer, appLayer.fd) < 0) {
 *     perror("Failed re-sending last message");
 *     exit(1);
 *   }
 * }
 */

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
  /*
   * struct sigaction sa;
   * sa.sa_handler = alrmHandler;
   * sigemptyset(&sa.sa_mask);
   * sa.sa_flags = 0;
   * if (sigaction(SIGALRM, &sa, NULL) == -1) {
   *   perror("setting alarm handler failed");
   *   exit(-1);
   * }
   */

  appLayer.fd = llopen(argv[2], appStatus);
  if (appLayer.fd < 0) {
    fprintf(stderr, "llopen() failed");
    exit(-1);
  }

  llclose(appLayer.fd);
  return 0;
}
