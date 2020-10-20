#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_layer.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

static struct applicationLayer appLayer;

void print_usage() {
  fprintf(stderr, "Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
  exit(-1);
}

int main(int argc, char **argv) {
  // parse args
  if (argc < 3)
    print_usage();

  if (strcmp(argv[1], "TRANSMITTER"))
    appLayer.status = TRANSMITTER;
  else if (strcmp(argv[1], "RECEIVER"))
    appLayer.status = RECEIVER;
  else
    print_usage();

  appLayer.fd = llopen(argv[2], appLayer.status);
  if (appLayer.fd < 0) {
    fprintf(stderr, "llopen() failed");
    exit(-1);
  }

  llclose(appLayer.fd, appLayer.status);
  return 0;
}
