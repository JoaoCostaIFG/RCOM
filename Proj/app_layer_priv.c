#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_layer_priv.h"

static unsigned char *startPacket;

int assembleControlPacket(struct applicationLayer *appLayer, bool is_end,
                          unsigned char **buf) {
  // C = 2/3 | T1 - L1 - V1 | T2 - L2 - V2 | ...
  // T (type): 0 tamanho file, 1 nome file, etc...
  // L (byte): length
  // V: valor (L length bytes)

  /* assemble packet */
  unsigned char *packet;
  int file_name_len = strlen(appLayer->file_name);
  int length = 1 + 2 + file_name_len + 2 + sizeof(long);
  packet = (unsigned char *)malloc(length * sizeof(unsigned char));
  if (packet == NULL) {
    perror("App layer packet instantiation");
    return -1;
  }

  int curr_ind = 0;
  if (is_end)
    packet[curr_ind++] = C_END;
  else
    packet[curr_ind++] = C_START;

  // TLV file size
  packet[curr_ind++] = T_SIZE;
  packet[curr_ind++] = sizeof(long);
  memcpy(packet + curr_ind, &appLayer->file_size, sizeof(long));
  curr_ind += sizeof(long);

  // TLV file name
  packet[curr_ind++] = T_NAME;
  packet[curr_ind++] = file_name_len;
  memcpy(packet + curr_ind, appLayer->file_name, file_name_len);
  *buf = packet;

  return length;
}

int assembleInfoPacket(char *buffer, int length, unsigned char **res) {
  // C = 1 | N | L2 - L1: 256 * L2 + L1 = k | P1..Pk (k bytes)
  /* assemble packet */
  static unsigned char n =
      MAXSEQNUM; // unsigned integer overflow is defined >:(
  ++n;

  int new_length = length + 4;
  unsigned char *packet =
      (unsigned char *)malloc(new_length * sizeof(unsigned char));
  if (packet == NULL) {
    perror("App layer packet instantiation");
    return -1;
  }

  packet[C_CONTROL] = C_DATA;
  packet[SEQ_NUMBER] = n;
  packet[L2] = (unsigned char)(length / L2VAL);
  packet[L1] = (unsigned char)(length % L2VAL);
  memcpy(packet + 4, buffer, sizeof(char) * length);

  *res = packet;
  return new_length;
}

int parseControlPacket(unsigned char *packet) {
  int curr_ind = C_CONTROL + 1;
  int size_type = packet[curr_ind++];
  if (size_type != T_SIZE) {
    fprintf(stderr, "Invalid size type %d\n", size_type);
    return -1;
  }

  int size_length = packet[curr_ind++];
  curr_ind += size_length;

  int name_type = packet[curr_ind++];
  if (name_type != T_NAME) {
    fprintf(stderr, "Invalid name type %d\n", name_type);
    return -1;
  }

  if (packet[C_CONTROL] == C_START)
    startPacket = packet;

  return 0;
}

int parsePacket(unsigned char *packet, int packet_length) {
  // unsigned integer overflow is defined >:(
  static unsigned char n = 0;

  if (packet[C_CONTROL] == C_DATA) { // Verifications in case of data
    if (packet[SEQ_NUMBER] != n) { // Invalid sequence number
      fprintf(stderr, "Sq num: %d->%d\n", packet[SEQ_NUMBER], n);
      free(packet);
      return -2;
    }
    ++n;

    int expected_length = packet[L2] * L2VAL + packet[L1] + 4;
    if (expected_length != packet_length) {
      fprintf(stderr, "Invalid length: %d->%d\n", expected_length,
              packet_length);
      free(packet);
      return -2;
    }

  } else if (packet[C_CONTROL] == C_START ||
             packet[C_CONTROL] == C_END) { // Start and End

    if (parseControlPacket(packet) < 0) {
      free(packet);
      return -3;
    }

  } else {
    fprintf(stderr, "Seq header %d\n", packet[C_CONTROL]);
    free(packet);
    return -3; // Unexpected C header
  }

  return packet[C_CONTROL];
}

unsigned char *getStartPacket() { return startPacket; }
