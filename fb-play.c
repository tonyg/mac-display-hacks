/* gcc -O3 -o fb-play fb-play.c -framework ApplicationServices -framework Carbon
 */
/* Running this as root makes a noticeable difference in the
 * smoothness of the updating!
 */
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <IOKit/graphics/IOGraphicsLib.h>
#include <ApplicationServices/ApplicationServices.h>

typedef uint32_t pix_t;

#define N_FRAMES 500

int main(int argc, char *argv[]) {
  int i, x, y, width, height, stride, c;
  CGDirectDisplayID targetDisplay = kCGDirectMainDisplay;
  pix_t *screen;
  struct timeval t_start, t_stop;

  CGDisplayCapture(targetDisplay); /* crucial for being permitted to write on it */
  screen = (pix_t *) CGDisplayBaseAddress(targetDisplay);
  printf("Pointer is %p, sizeof(pix_t) is %d\n", screen, sizeof(pix_t));

  stride = CGDisplayBytesPerRow(targetDisplay) / 4;
  width = CGDisplayPixelsWide(targetDisplay);
  height = CGDisplayPixelsHigh(targetDisplay);

  gettimeofday(&t_start, NULL);
  c = 0;
  i = 0;
  while (!Button()) {
    Point loc;
    GetMouse(&loc);
    /* memset(screen, c, ((height-1) * stride + width) * sizeof(pix_t)); */
    for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
	//c = x ^ y % (i + 1);
	c = (x % (i + 1)) ^ y;
	//c = x / ((x ^ y) + 1);
	//c = (x + 1) % (y + 1);
	if ((abs(y - loc.v) < 10) && (abs(x - loc.h) < 10)) {
	  c = 255 << 16;
	}
    	screen[y * stride + x] = c;
      }
    }
    i++;
  }
  gettimeofday(&t_stop, NULL);

  {
    long delta = (t_stop.tv_sec - t_start.tv_sec) * 1000000 + (t_stop.tv_usec - t_start.tv_usec);
    printf("%d frames took %lu microseconds, so %g frames/sec\n",
	   i,
	   delta,
	   i / (delta / 1000000.0));
  }
}
