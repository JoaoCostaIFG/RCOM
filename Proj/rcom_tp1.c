#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_layer.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

static struct applicationLayer appLayer;

void print_usage() {
  fprintf(stderr, "Usage:\t <RECEIVER|TRANSMITTER> <SerialPort>\n\tex: "
                  "TRANSMITTER /dev/ttyS1\n");
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

  if (appLayer.status == TRANSMITTER) {
  } else if (appLayer.status == RECEIVER) {
    unsigned char *buf;
    unsigned char res[];
    bool valid_packet = false;
    do {
      int n = llread(appLayer.fd, (char *)buf);
      if (n < 0) {
        perror("Morreu mesmo");
        valid_packet = false;
      } else if (n == 0) {
        valid_packet = true; // Ignore this packet
      } else {               // n > 0
      }

    } while (!isEndPacket(buf) && valid_packet);
  }

  llclose(appLayer.fd, appLayer.status);
  return 0;
}
