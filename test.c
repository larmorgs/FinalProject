#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define STRAND_LEN 160 // Number of LEDs on strand

#define TIME 1000
#define DELAY 1000

static FILE *grb_fp;
static FILE *data_fp;

void pattern1() {
  int i;
  unsigned char data[3];
  
  for (i = 0; i < TIME; i++) {
    data[0] = i % 0x7F;
    data[1] = i % 0x3F;
    data[2] = i % 0x1F;
    fprintf(grb_fp, "%d %d %d", data[0], data[1], data[2]);
    fflush(grb_fp);
    usleep(DELAY);
  }
}

void pattern2() {
  int i, j;
  unsigned char data[STRAND_LEN * 3];
  
  for (i = 0; i < TIME; i++) {
    for (j = 0; j < STRAND_LEN * 3; j++) {
      data[j] = i % 0x3F + j % 0x3F;
    }
    for (j = 0; j < STRAND_LEN * 3; j += 3) {
      fprintf(data_fp, "%d %d %d", data[i], data[i+1], data[i+2]);
    }
    fflush(data_fp);
    usleep(DELAY);
  }
}

//signal handler that breaks program loop and cleans up
void signal_handler(int signo){
  if (signo == SIGINT) {
    printf("\n^C pressed, cleaning up and exiting..\n");
    fflush(stdout);
    fflush(grb_fp);
    fclose(grb_fp);
    exit(0);
  }
}

int main() { 
  grb_fp = fopen("/sys/firmware/spi-lpd8806/device/grb", "w");
  if (grb_fp == NULL) {
    return 1;
  }
  
  data_fp = fopen("/sys/firmware/spi-lpd8806/device/grb", "w");
  if (data_fp == NULL) {
    return 1;
  }

  if (signal(SIGINT, signal_handler) == SIG_ERR) {
    printf("Error with SIGINT handler\n");
    return 1;
  }
  
  while (1) {
    pattern1();
    pattern2();
  }
}
