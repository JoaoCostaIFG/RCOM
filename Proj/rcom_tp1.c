#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_layer.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

static struct applicationLayer appLayer;
static int port = -1;
static long baudrate = 38400, chunksize = 256; // TODO

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
  int c, option_index = 0, name_len;
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
      name_len = strlen(optarg);
      appLayer.file_name = (char *)malloc(sizeof(char) * name_len);
      if (appLayer.file_name != NULL)
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

int sendFile() {
  FILE *fp = fopen(appLayer.file_name, "r");
  if (fp == NULL) {
    perror(appLayer.file_name);
    return -1;
  }

  appLayer.file_size = ftell(fp);
  unsigned char *file_content =
      (unsigned char *)malloc(sizeof(unsigned char) * appLayer.file_size);
  if (fread(file_content, sizeof(unsigned char), appLayer.file_size, fp) < 0) {
    perror("File content read");
    return -2;
  }

  long ind = 0;
  while (ind < appLayer.file_size) {
    // send info fragment
    /* assembleInfoPacket(&linkLayer, file_content, size); */
    /* sendAndAlarmReset(&linkLayer, appLayer.fd); */
    /* size = read(fp, file_content, MAX_SIZE / 2); */

    /* // Get RR/REJ answer */
    /* int nextSeqNum = NEXTSEQUENCENUMBER(linkLayer); */
    /* bool okAnswer = false; */
    /* while (!okAnswer) { */
      /* fprintf(stderr, "Getting RR/REJ %d.\n", linkLayer.sequenceNumber); */

      /* unsigned char currByte, buf[MAX_SIZE]; */
      /* int res, bufLen = 0; */
      /* state curr_state = START_ST; */
      /* transitions transition; */

      /* while (curr_state != STOP_ST) { */
        /* res = read(appLayer.fd, &currByte, sizeof(unsigned char)); */
        /* if (res <= 0) */
          /* perror("RR/REJ read"); */

        /* transition = byteToTransitionRR(currByte, buf, curr_state); */
        /* curr_state = state_machine[curr_state][transition]; */

        /* if (curr_state == START_ST) */
          /* bufLen = 0; */
        /* else */
          /* buf[bufLen++] = currByte; */
      /* } */

      /* if (buf[C_FIELD] == (C_RR | (nextSeqNum << 7))) { */
        /* okAnswer = true; */
        /* FLIPSEQUENCENUMBER(linkLayer); */
        /* fprintf(stderr, "Got RR %d.\n", linkLayer.sequenceNumber); */
      /* } else if (buf[C_FIELD] == (C_REJ | (nextSeqNum << 7))) { */
        /* // reset attempts (we got an answer) */
        /* linkLayer.numTransmissions = MAXATTEMPTS; */
        /* fprintf(stderr, "Got REJ %d.\n", linkLayer.sequenceNumber); */
      /* } */
    /* } */

    /* alarm(0); // reset pending alarm */
  }

  return 0;
}

int main(int argc, char **argv) {
  // parse args
  appLayer.status = NONE;
  parseArgs(argc, argv);

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

  appLayer.fd = llopen(port, appLayer.status);
  if (appLayer.fd < 0) {
    fprintf(stderr, "llopen() failed\n");
    exit(-1);
  }

  if (appLayer.status == TRANSMITTER) {
    sendFile();
  } else { // RECEIVER
  }

  if (llclose(appLayer.fd, appLayer.status)) {
    fprintf(stderr, "llclose() failed\n");
    exit(-1);
  }

  free(appLayer.file_name);
  return 0;
}
