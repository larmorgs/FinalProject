#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define TIME 160

int main() { 
  unsigned char data[] = {0, 50, 100};
  FILE *fp;
  
  fp = fopen("/sys/firmware/spi-lpd8806/device/grb", "w");
  if (fp == NULL) {
    return 1;
  }
  
  int i;

  unsigned char dg = 1;
  unsigned char dr = 1;
  unsigned char db = 0;
  
  for (i = 0; i < TIME; i++) {    
    if (!dg) {
      data[0]++;
      if (data[0] == 0x7F) {
        dg = 0;
      }
    } else {
      data[0]--;
      if (data[0] == 0) {
        dg = 1;
      }
    }

    if (!dr) {
      data[1]++;
      if (data[1] == 0x7F) {
        dr = 0;
      } 
    } else {
      data[1]--;
      if (data[1] == 0) {
        dr = 1;
      }
    }

    if (!db) {
      data[2]++;
      if (data[2] == 0x7F) {
        db = 0;
      }
    } else {
      data[2]--;
      if (data[2] == 0) {
        db = 1;
      }
    }
    

    fprintf(fp, "%d %d %d", data[0], data[1], data[2]);
    fflush(fp);
  
  }

  fclose(fp);
}
