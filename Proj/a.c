#include <stdio.h>
#include <sys/stat.h>

int main() {
  struct stat sb;
  stat("stadodo.txt", &sb);
  printf("%ld", sb.st_size);
}
