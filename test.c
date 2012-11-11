#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define STRAND_LEN 160 // Length of data memory

int main() { 
  unsigned char data[STRAND_LEN][3];
  FILE *fp;
  
  fp = fopen("/sys/firmware/lpd8806/device/rgb", "w");
  if (fp == NULL) {
    return 1;
  }
  
  int i;
  
  for (i = 0; i < STRAND_LEN; i++) {
    data[i][0] = i;
    data[i][1] = 255 - i;
    data[i][2] = 160 - i;
  }
  
  for (i = 0; i < STRAND_LEN; i++) {
    fprintf(fp, "%d %d %d\n", data[i][0], data[i][1], data[i][2]);
  }
  
  fclose(fp);
}
