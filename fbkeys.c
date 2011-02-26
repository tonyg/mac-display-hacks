/* gcc -arch i386 -O3 -o fbkeys fbkeys.c -framework ApplicationServices -framework Carbon
 */
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>

#include <ApplicationServices/ApplicationServices.h>

int main(int argc, char *argv[]) {
  uint32_t keys[4];

  while (1) {
    GetKeys(&keys[0]);
    printf("%08x %08x %08x %08x\n", keys[0], keys[1], keys[2], keys[3]);
    usleep(100000);
  }
}
