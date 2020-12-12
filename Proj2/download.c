#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define FTP_PORT 21
#define ACK 0
#define NACK 1
#define DEBUG

struct ConnectionObj {
  char hostname[256];
  char user[256];
  char pass[256];
  char dirname[256];
  char filename[256];

  char ip[INET6_ADDRSTRLEN];
  u_short port;
  struct hostent *h;

  int ctrl_sock;
  int data_sock;
};

/*
 *  URL
 */
void printUrlParse(struct ConnectionObj *conObj) {
  // TODO empty field => <empty> or <none>
  printf("##############################\n# User: %s\n# Pass: %s\n# Hostname: "
         "%s\n# Dirname: %s\n# Filename: %s\n##############################\n",
         conObj->user, conObj->pass, conObj->hostname, conObj->dirname,
         conObj->filename);
}

void printUsage(void) {
  fprintf(stderr, "usage: download "
                  "ftp://[<user>:<password>@]<host>/<url-path>\ne.g.:download "
                  "ftp://anonymous:none@fe.up.pt/pub/debian/README\n");
  exit(1);
}

struct hostent *resolveHostName(char *hostname) {
  struct hostent *h;
  if ((h = gethostbyname(hostname)) == NULL) {
    herror("gethostbyname");
    return NULL;
  }
  return h;
}

int isUrlValid(char *url) {
  regex_t regex;
              /* "^ftp://((a-zA-Z0-9)+:(a-zA-Z0-9)*@)?[^!*'();:@&=+$,/?%#]*(/[^/" */
              /* "]+)+/?$", */
  if (regcomp(&regex,
              "^ftp://((a-zA-Z0-9)+:(a-zA-Z0-9)*@)?.*(/.*)*$",
              REG_EXTENDED) != 0) {
    perror("regcomp()");
    exit(1);
  }

  int match = regexec(&regex, url, 0, NULL, 0);
  regfree(&regex);
  if (match == REG_NOMATCH)
    return 0; // Not Valid
  else
    return 1; // Valid
}

int parseUrl(char *url, struct ConnectionObj *conObj) {
  if (!isUrlValid(url)) {
    fprintf(stderr, "Given url is not valid: %s\n", url);
    printUsage();
  }

  char tmp[256] = "";
  const char *delim = "/";
  char *saveptr;
  strtok_r(url, delim, &saveptr); // skip ftp:
  char *token = strtok_r(NULL, delim, &saveptr);
  strcpy(conObj->hostname, token); // get hostname

  // parse user and pass (if any)
  char *saveptr2, *token2 = strtok_r(token, "@", &saveptr2);
  if (strcmp(conObj->hostname, token2)) { // has user/pass
    char *saveptr3, *token3;
    // user
    token3 = strtok_r(token2, ":", &saveptr3);
    strcpy(conObj->user, token3);
    // pass
    token3 = strtok_r(NULL, ":", &saveptr3);
    if (token3 == NULL)
      strcpy(conObj->pass, "");
    else
      strcpy(conObj->pass, token3);

    // real hostname
    token2 = strtok_r(NULL, "@", &saveptr2);
    strcpy(conObj->hostname, token2);
  } else {
    strcpy(conObj->user, "anonymous");
    strcpy(conObj->pass, "");
  }

  // parse directory path
  strcpy(conObj->dirname, "");
  for (token = strtok_r(NULL, delim, &saveptr); token != NULL;
       token = strtok_r(NULL, delim, &saveptr)) {
    if (strcmp(conObj->dirname, ""))
      strcat(conObj->dirname, "/");
    strcat(conObj->dirname, tmp);

    strcpy(tmp, token);
  }
  // save file name
  strcpy(conObj->filename, tmp);

  return 0;
}

/*
 *  FTP
 */
int getFTPState(int sockfd, char *msg, int max_size) {
  int read_ret, ans = NACK, curr_size = 0;
  char response[256]; // TODO maybe 4 is ok
  if (max_size > 0)
    strcpy(msg, "");

  do {
    if ((read_ret = read(sockfd, response, 255)) < 0) {
      ans = NACK;
      break;
    }
    response[read_ret] = '\0';
    // save answer for later parsing
    if (max_size > curr_size) {
      strncat(msg, response, max_size - curr_size);
      curr_size += read_ret;
    }

#ifdef DEBUG
    /* fprintf(stderr, "\t%s", response); */
    fprintf(stderr, "\nAAAAAAAAAAAA%sAAAAAAAAAAAAA\n", response);
    fflush(stderr);
#endif

    char* abc = strrchr(response, '\n');
    abc = strrchr(abc, '\n');
    printf("BBBBBBBBBBBBBBBBBBBB%sBBBBBBBBBBBBBBBBBB\n", abc);

    if ('1' <= response[0] && response[0] <= '3')
      ans = ACK;
    else
      ans = NACK;
  } while (response[3] != ' '); // '-' when multiline, ' ' when last/single

  return ans;
}

int sendFTPCmd(int sockfd, char *cmd, char *msg, int max_size) {
  write(sockfd, cmd, strlen(cmd));

  return getFTPState(sockfd, msg, max_size);
}

int loginFTP(int sockfd, struct ConnectionObj *conObj) {
  char cmd[262];

  snprintf(cmd, 262, "USER %s\n", conObj->user);
  if (sendFTPCmd(sockfd, cmd, NULL, 0) == NACK)
    return NACK;

  snprintf(cmd, 262, "PASS %s\n", conObj->pass);
  if (sendFTPCmd(sockfd, cmd, NULL, 0) == NACK)
    return NACK;

  return ACK;
}

int goPasvFTP(int sockfd, struct ConnectionObj *conObj) {
  char msg[100];
  if (sendFTPCmd(sockfd, "PASV\n", msg, 100) == NACK)
    return NACK;

  *(strchr(msg, ')')) = ','; /* (h1,h2,h3,h4,p1,p2). */
  char num[6][4]; /* h1,h2,h3,h4,p1,p2, */
  for (char *i = strchr(msg, '(') + 1, j = 0; i != strchr(msg, '.'); ++i, ++j) {
    char *next_comma = strchr(i, ',');
    int ind;
    for (ind = 0; i != next_comma; ++i, ++ind)
      num[(int)j][ind] = *i;
    num[(int)j][ind] = '\0';
  }

  conObj->port = strtol(num[4], NULL, 10) * 256 + strtol(num[5], NULL, 10);
  printf("# Our IP: %s.%s.%s.%s\n# Server data port: %d\n", num[0], num[1],
         num[2], num[3], conObj->port);

  return ACK;
}

int openFTPSock(const char *const ip, const int port) {
  struct sockaddr_in server_addr;
  int sockfd;

  // server address handling
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  // 32 bit Internet address network byte ordered
  server_addr.sin_addr.s_addr = inet_addr(ip);
  // server TCP port must be network byte ordered
  server_addr.sin_port = htons(port);

  // open an TCP socket
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket()");
    return -1;
  }
  // connect to the server
  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect()");
    return -1;
  }

  return sockfd;
}

int startCtrlFTPCon(struct ConnectionObj *conObj) {
  /* meia do CMD (pasv) */
  if ((conObj->h = resolveHostName(conObj->hostname)) == NULL) {
    fprintf(stderr, "Couldn't resolve hostname: %s\n", conObj->hostname);
    return 1;
  }
  strcpy(conObj->ip, inet_ntoa(*((struct in_addr *)conObj->h->h_addr)));
  printf("\n# Server IP: %s\n", conObj->ip);

  int openRet = openFTPSock(conObj->ip, FTP_PORT);
  if (openRet < 0)
    return 1;
  conObj->ctrl_sock = openRet;

  /* Check server initial state */
  if (getFTPState(conObj->ctrl_sock, NULL, 0) != ACK) {
    fprintf(stderr, "startCtrlFTPCon(): Initial connection failed :(\n");
    close(conObj->ctrl_sock);
    return 1;
  }

  /* Login */
  if (loginFTP(conObj->ctrl_sock, conObj) == NACK) {
    fprintf(stderr,
            "loginFTP(): Login failed (maybe it's anonymous only) :(\n");
    close(conObj->ctrl_sock);
    return 1;
  }

  puts("\nSuccessfully opened control connection with the server.\n");
  return 0;
}

int startDataFTPCon(struct ConnectionObj *conObj) {
  /* meia da data (file) */
  int openRet = openFTPSock(conObj->ip, conObj->port);
  if (openRet < 0)
    return 1;
  conObj->data_sock = openRet;

  puts("\nSuccessfully opened data connection with the server.");

  return 0;
}

int getFTPFileData(struct ConnectionObj *conObj) {
  FILE *fp;
  if ((fp = fopen(conObj->filename, "w")) == NULL)
    return NACK;
  unsigned char d[256];

  int read_ret = read(conObj->data_sock, d, 256);
  while (read_ret > 0) {
    fwrite(d, sizeof(unsigned char), read_ret, fp);
    read_ret = read(conObj->data_sock, d, 256);
  }

  fclose(fp);
  return getFTPState(conObj->ctrl_sock, NULL, 0);
}

int getFTPFile(struct ConnectionObj *conObj) {
  char cmd[262];

  if (strlen(conObj->dirname) > 0) { // dir exists
    snprintf(cmd, 262, "CWD %s\n", conObj->dirname);
    if (sendFTPCmd(conObj->ctrl_sock, cmd, NULL, 0) == NACK)
      return NACK;
  }

  snprintf(cmd, 262, "RETR %s\n", conObj->filename);
  if (sendFTPCmd(conObj->ctrl_sock, cmd, NULL, 0) == NACK)
    return NACK;

  if (getFTPFileData(conObj) == NACK)
    return NACK;

  return ACK;
}

int main(int argc, char *argv[]) {
  if (argc != 2)
    printUsage();

  /* parse input */
  struct ConnectionObj conObj;
  parseUrl(argv[1], &conObj);
  printUrlParse(&conObj);

  /* open ctrl connection */
  if (startCtrlFTPCon(&conObj)) {
    fprintf(stderr, "startCtrlFTPCon(): failed to open control connection with "
                    "the server.\n");
    exit(1);
  }

  /* Go into passive mode */
  goPasvFTP(conObj.ctrl_sock, &conObj);

  /* open data connection */
  if (startDataFTPCon(&conObj)) {
    fprintf(stderr, "startDataFTPCon(): failed to open data connection with "
                    "the server.\n");
    exit(1);
  }

  if (getFTPFile(&conObj)) {
    fprintf(stderr, "getFTPFile(): failed to get the file %s/%s\n",
            conObj.dirname, conObj.filename);
    exit(1);
  }

  /* Cleanup */
  close(conObj.ctrl_sock);
  close(conObj.data_sock);

  return 0;
}
