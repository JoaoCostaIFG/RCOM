#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "app_layer.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

static struct applicationLayer appLayer;
static int port = -1;
static long chunksize = 256;
static int baudrate = 38400;

void print_connection_info() {
  printf("==========================\n"
         "= Connection information =\n"
         "==========================\n"
         "= Baud rate: %d\n"
         "= Port: %d\n",
         baudrate, port);

  if (appLayer.status == RECEIVER) {
    printf("= Status: RECEIVER\n");

    char out_file[512];
    if (strcmp(appLayer.file_name, "")) { // not empty
      stpcpy(out_file, appLayer.file_name);
    } else {
      stpcpy(out_file, "to be determined");
    }

    printf("= File: %s\n", out_file);
  } else {
    printf("= Status: TRANSMITTER\n"
           "= File: %s\n"
           "= Chunksize: %ld\n",
           appLayer.file_name, chunksize);
  }

  printf("==========================\n");
  fflush(stdout);
}

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
      {"chunksize", required_argument, 0, 't'},
      {0, 0, 0, 0}};
  int c, option_index = 0;
  while ((c = getopt_long(argc, argv, "s:p:f:b:t:", long_options,
                          &option_index)) != -1) {

    switch (c) {
    case 's':
      if (!strcmp(optarg, "TRANSMITTER"))
        appLayer.status = TRANSMITTER;
      else if (!strcmp(optarg, "RECEIVER"))
        appLayer.status = RECEIVER;
      else
        print_usage();
      break;
    case 'p':
      port = (int)strtol(optarg, NULL, 10);
      break;
    case 'f':
      strcpy(appLayer.file_name, optarg);
      break;
    case 'b':
      baudrate = strtol(optarg, NULL, 10);
      if (baudrate <= 0) {
        fprintf(stderr, "Invalid baudrate: %s\n", optarg);
        print_usage();
      }
      break;
    case 't':
      chunksize = strtol(optarg, NULL, 10);
      if (chunksize <= 0) {
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
  appLayer.status = NONE;
  strcpy(appLayer.file_name, "");
  // parse args
  parseArgs(argc, argv);
  initAppLayer(&appLayer, port, baudrate, chunksize);


  if (port < 0) {
    fprintf(stderr, "port is not optional.\n");
    print_usage();
  }
  if (appLayer.status == NONE) {
    fprintf(stderr, "status is not optional.\n");
    print_usage();
  } else if (appLayer.status == TRANSMITTER) {
    if (!strcmp(appLayer.file_name, "")) {
      fprintf(stderr,
              "Transmitters need to specify the name of the file to send.\n");
      print_usage();
    }
  }
  print_connection_info();

  appLayer.fd = llopen(port, appLayer.status);
  if (appLayer.fd < 0) {
    fprintf(stderr, "llopen() failed\n");
    exit(-1);
  }

  if (appLayer.status == TRANSMITTER) {
    if (sendFile(&appLayer) < 0)
      return -1;
  } else { // RECEIVER
    unsigned char *file_content = NULL;
    if (receiveFile(&appLayer, &file_content) < 0) {
      fprintf(stderr, "receiveFile() failed\n");
      exit(-2);
    }

    write_file(&appLayer, file_content);
    free(file_content);
  }

  if (llclose(appLayer.fd, appLayer.status)) {
    fprintf(stderr, "llclose() failed\n");
    exit(-1);
  }

  return 0;
}
