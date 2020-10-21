#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_layer.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

static struct applicationLayer appLayer;
static char port[50] = "";
static char file_name[256] = "";
static long baudrate = 38400, packetsize = 256;

void print_usage() {
  fprintf(stderr, "Usage:\t -s <RECEIVER|TRANSMITTER> -p <SerialPort> -f "
                  "<file_name> -b <baudrate>\n\tex: "
                  "-s TRANSMITTER -p /dev/ttyS1 -f file -b 38400\n");
  exit(-1);
}

void parseArgs(int argc, char **argv) {
  if (argc < 3)
    print_usage();

  static struct option long_options[] = {
      {"status", required_argument, 0, 's'},
      {"port", required_argument, 0, 'p'},
      {"file", required_argument, 0, 'f'},
      {"baudrate", required_argument, 0, 'b'},
      {"packetsize", required_argument, 0, 't'},
      {0, 0, 0, 0}};
  int c, option_index = 0;
  while ((c = getopt_long(argc, argv, "s:p:f:b:t:", long_options,
                          &option_index)) != -1) {

    switch (c) {
    case 's':
      if (strcmp(optarg, "TRANSMITTER"))
        appLayer.status = TRANSMITTER;
      else if (strcmp(optarg, "RECEIVER"))
        appLayer.status = RECEIVER;
      else
        print_usage();
      break;
    case 'p':
      strcpy(port, optarg);
      break;
    case 'f':
      strcpy(file_name, optarg);
      break;
    case 'b':
      baudrate = strtol(optarg, NULL, 10);
      if (baudrate <= 0) {
        fprintf(stderr, "Invalid baudrate: %s\n", optarg);
        print_usage();
      }
      break;
    case 't':
      packetsize = strtol(optarg, NULL, 10);
      if (packetsize <= 0) {
        fprintf(stderr, "Invalid packet size: %s\n", optarg);
        print_usage();
      }
      break;
    case '?': // invalid option
      print_usage();
      break;
    default:
      fprintf(stderr, "getopt returned character code %#X\n", c);
      break;
    }
  }

  if (optind < argc) {
    printf("non-option ARGV-elements: ");
    while (optind < argc)
      printf("%s ", argv[optind++]);
    printf("\n");
  }
}

int main(int argc, char **argv) {
  // parse args
  appLayer.status = NONE;
  parseArgs(argc, argv);

  if (!strcmp(port, "")) {
    fprintf(stderr, "port is not optional.\n");
    print_usage();
  }
  if (appLayer.status == NONE) {
    fprintf(stderr, "status is not optional.\n");
    print_usage();
  } else if (appLayer.status == TRANSMITTER) {
    if (!strcmp(file_name, "")) {
      fprintf(stderr,
              "Transmitters need to specify the name of the file to send.\n");
      print_usage();
    }
  }

  appLayer.fd = llopen(argv[2], appLayer.status);
  if (appLayer.fd < 0) {
    fprintf(stderr, "llopen() failed");
    exit(-1);
  }

  llclose(appLayer.fd, appLayer.status);
  return 0;
}
