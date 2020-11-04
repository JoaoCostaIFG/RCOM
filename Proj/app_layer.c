#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_layer_priv.h"
#include "data_link.h"

#define PROGRESSSIZE 70

static struct linkLayer linkLayer;
static struct termios oldtio;

void initAppLayer(struct applicationLayer *appLayer, int baudrate,
                  long chunksize) {
  linkLayer = initLinkLayer(); // TODO check error

  appLayer->chunksize = chunksize;
  if (setBaudRate(&linkLayer, baudrate) < 0)
    fprintf(stderr, "Invalid baudrate. Using default: %d\n", DFLTBAUDRATE);
}

int llopen(int porta, enum applicationStatus appStatus) {
  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */
  char port_str[20];
  sprintf(port_str, "%d", porta);
  strcpy(linkLayer.port, PORTLOCATION);
  strcat(linkLayer.port, port_str);

  int fd = open(linkLayer.port, O_RDWR | O_NOCTTY);
  if (fd < 0) {
    perror(linkLayer.port);
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
  newtio.c_cc[VTIME] = MYVTIME; /* inter-unsigned character timer unused */
  newtio.c_cc[VMIN] = MYVMIN;   /* blocking read until X chars received */
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
    if (initConnection(&linkLayer, fd, true) < 0)
      return -5;
  } else if (appStatus == TRANSMITTER) {
    if (initConnection(&linkLayer, fd, false) < 0)
      return -5;
  } else {
    return -4;
  }

  return fd;
}

int llwrite(int fd, char *buffer, int length) {
  if (sendFrame(&linkLayer, fd, (unsigned char *)buffer, length) < 0)
    return -1;
  return 0;
}

int llread(int fd, char **buffer) {
  int packet_length = getFrame(&linkLayer, fd, (unsigned char **)buffer);

  if (packet_length == 0)
    return 0; // Morreu só neste
  else if (packet_length < 0)
    return -1; // Morreu mesmo

  return packet_length;
}

int llclose(int fd, enum applicationStatus appStatus) {
  if (appStatus == RECEIVER) {
    if (endConnection(&linkLayer, fd, true))
      return -1;
  } else if (appStatus == TRANSMITTER) {
    if (endConnection(&linkLayer, fd, false))
      return -1;
  } else {
    return -2;
  }

  /* Reset serial port */
  sleep(1); // for safety (in case the transference is still on-going)
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    return -3;
  }

  return close(fd);
}

long getStartPacketFileSize() {
  long file_size;
  memcpy(&file_size, getStartPacket() + 3, sizeof(long));
  return file_size;
}

int getStartPacketFileName(char **file_name) {
  *file_name = (char *)(getStartPacket() + 2 + sizeof(long) + 3);
  return (int) *(getStartPacket() + 2 + sizeof(long) + 2);
}

void drawProgress(float currPerc, int divs, bool isRedraw) {
  static int prev_full = -1;
  int full = (int)roundf(currPerc * divs);
  if (full < 0)
    full = 0;
  else if (full > divs)
    full = divs;

  if (full == prev_full)
    return;
  prev_full = full;

  /* draw */
  if (isRedraw) {
    for (int i = 0; i < divs + 10; ++i)
      printf("\b");
  }

  printf("[");
  for (int i = 0; i < full; ++i)
    printf("-");
  printf("\33\[33m\33\[1m");
  printf(full % 2 == 0 ? "C" : "c");
  printf("\33\[0m");
  for (int i = full + 1; i < divs; ++i)
    printf(i % 2 == 0 ? "o" : " ");
  printf("] %.2f%%", currPerc * 100);

  fflush(stdout);
}

int sendFile(struct applicationLayer *appLayer) {
  printf("Sending file..\n");

  FILE *fp = fopen(appLayer->file_name, "rb");
  if (fp == NULL) {
    perror(appLayer->file_name);
    return -1;
  }

  struct stat sb;
  if (stat(appLayer->file_name, &sb) == -1) {
    perror("stat");
    return -1;
  }
  appLayer->file_size = sb.st_size;

  unsigned char *file_content =
      (unsigned char *)malloc(sizeof(unsigned char) * appLayer->file_size);
  if (appLayer->file_size > 0) { // empty file
    if (fread(file_content, sizeof(unsigned char), appLayer->file_size, fp) ==
        0) {
      perror("sendfile() read");
      return -2;
    }
  }

  // Send start control packet
  unsigned char *start_packet = NULL;
  int start_length = assembleControlPacket(appLayer, false, &start_packet);
  if (llwrite(appLayer->fd, (char *)start_packet, start_length) < 0) {
    free(start_packet);
    return -3;
  }

  puts("");
  drawProgress(0, PROGRESSSIZE, false);
  // Send info packets
  long ind = 0, trSize = 0;
  while (ind < appLayer->file_size) {
    // send info fragment
    unsigned char *packet = NULL;
    int length;
    if (appLayer->file_size - ind >= appLayer->chunksize) {
      length = assembleInfoPacket((char *)file_content + ind,
                                  appLayer->chunksize, &packet);
      trSize += appLayer->chunksize;
    } else {
      length = assembleInfoPacket((char *)file_content + ind,
                                  appLayer->file_size - ind, &packet);
      trSize += appLayer->file_size - ind;
    }

    if (llwrite(appLayer->fd, (char *)packet, length) < 0) {
      free(packet);
      return -4;
    }

    drawProgress((float)trSize / appLayer->file_size, PROGRESSSIZE, true);
    free(packet);
    ind += appLayer->chunksize;
  }

  // Send end control packet
  unsigned char *end_packet = NULL;
  int end_length = assembleControlPacket(appLayer, true, &end_packet);
  if (llwrite(appLayer->fd, (char *)end_packet, end_length) < 0) {
    free(end_packet);
    return -5;
  }

  drawProgress(100, PROGRESSSIZE, true);
  puts("");

  return appLayer->file_size;
}

int receiveFile(struct applicationLayer *appLayer, unsigned char **res) {
  printf("Receiving file..\n");

  unsigned char *buf = NULL;
  bool stop = false;
  do {
    int n = llread(appLayer->fd, (char **)&buf);
    if (n < 0) {
      fprintf(stderr, "Morreu mesmo.\n");
      return -1;
    } else if (n == 0) { // Invalid packet, try again
      stop = false;
    } else {
      int status = parsePacket(buf, n);
      if (status < 0)
        fprintf(stderr, "Invalid packet formatting.\n");
      else if (status == C_START)
        stop = true;
      else
        stop = false;
    }

  } while (!stop);

  long fileSize = getStartPacketFileSize();
  *res = (unsigned char *)malloc(sizeof(unsigned char) * fileSize);

  puts("");
  drawProgress(0, PROGRESSSIZE, false);
  stop = false;
  int curr_file_n = 0;
  while (!stop) {
    int n = llread(appLayer->fd, (char **)&buf);

    if (n < 0) {
      perror("Morreu mesmo");
      free(res);
      return -1;
    } else if (n == 0) {
      stop = false;
    } else {
      int status = parsePacket(buf, n);
      if (status < 0)
        fprintf(stderr, "Invalid packet formatting.\n");
      else if (status == C_END)
        stop = true;
      else if (status == C_DATA) {
        memcpy(*res + curr_file_n, buf + 4, n - 4);
        curr_file_n += n - 4;
        stop = false;
      } else
        stop = false;
      drawProgress((float)curr_file_n / fileSize, PROGRESSSIZE, true);
    }
  }

  drawProgress(1, PROGRESSSIZE, true);
  puts("");
  free(buf);
  return 0;
}

void write_file(struct applicationLayer *appLayer,
                unsigned char *file_content) {
  char out_file[512];
  if (strcmp(appLayer->file_name, "")) { // not empty
    stpcpy(out_file, appLayer->file_name);
  } else {
    strcpy(out_file, "out/");
    char *file_name;
    int file_name_size = getStartPacketFileName(&file_name);
    strncat(out_file, file_name, file_name_size);
  }

  FILE *fp = fopen(out_file, "w+b");
  if (fp == NULL) {
    perror("Failed creating output file");
  } else {
    if (fwrite(file_content, sizeof(unsigned char), getStartPacketFileSize(),
               fp) == 0) {
      perror("Failed writting");
    } else {
      printf("\nSuccessfully wrote file contents to: %s\nFile size: %ld\n",
             out_file, getStartPacketFileSize());
    }
    fclose(fp);
  }
}
