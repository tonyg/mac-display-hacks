/* gcc -O3 -o fbflow fbflow.c
 */
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/fb.h>
#include <linux/input.h>

static struct screen {
  uint16_t *base;
  uint16_t *shadow;
  int width;
  int height;
  int stride;
} screen;

typedef struct Point {
  int h;
  int v;
} Point;

#define N_PARTICLES 50000
#define ERASE_TRACKS 0
#define WANT_FADE 1
#define WANT_REPLACEMENTS 1
#define FRAME_RATE_TARGET 30

#define FADE_FACTOR 0.95

#define N_MASSES 2

#define MAX_COLOR 1024

#define VEL_SCALE 6
#define MASS_CLIP 0 /* 400 */

#define M1 1000
#define M2 10000

static struct particle {
  float x, y, vx, vy;
} particles[N_PARTICLES];

static struct mass {
  float x, y;
  float mass;
} masses[N_MASSES];

#define DEAD_PARTICLE_MARKER 1000000

static uint16_t colors[MAX_COLOR];

static int frame_counter = 0;

static struct fb_var_screeninfo v;
static int fb;

static void setup_screen(void) {
  struct fb_fix_screeninfo f;
  fb = open("/dev/fb0", O_RDWR);
  if (fb < 0) {
    perror("open /dev/fb0");
    exit(1);
  }
  if (ioctl(fb, FBIOGET_FSCREENINFO, &f) < 0) {
    perror("ioctl FBIOGET_FSCREENINFO");
    close(fb);
    exit(1);
  }
  if (ioctl(fb, FBIOGET_VSCREENINFO, &v) < 0) {
    perror("ioctl FBIOGET_VSCREENINFO");
    close(fb);
    exit(1);
  }
  screen.base = mmap(NULL, f.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
  if (screen.base == NULL) {
    perror("mmap");
    close(fb);
    exit(1);
  }
  screen.stride = f.line_length / sizeof(uint16_t);
  screen.width = v.xres;
  screen.height = v.yres;
  screen.shadow = malloc(screen.stride * screen.height * sizeof(uint16_t));
  printf("bpp %d\n", v.bits_per_pixel);
  v.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
  if (ioctl(fb, FBIOPUT_VSCREENINFO, &v) < 0) {
    perror("ioctl FBIOPUT_VSCREENINFO");
    close(fb);
    exit(1);
  }
}

static void init_particle(struct particle *p) {
  p->x = random() % screen.width;
  p->y = random() % screen.height;
  p->vx = (float)(random() % 200) / 100 - 1;
  p->vy = (float)(random() % 200) / 100 - 1;
  p->vx *= VEL_SCALE;
  p->vy *= VEL_SCALE;
}

static void setup_particles(void) {
  int i;
  for (i = 0; i < N_PARTICLES; i++) {
    init_particle(&particles[i]);
  }
}

static uint16_t mkcolor(int R, int G, int B) {
  return (((R & 0xFF) >> 3) << 11) | (((G & 0xFF) >> 2) << 5) | (((B & 0xFF) >> 3) << 0);
}

static void setup_colors(void) {
  int i, c;
  int R, G, B;
  c = 0;
  for (i = 0; i < 256; i++) {
    colors[c++] = mkcolor(0xff, 0xff, 0xff - i);
  }
  for (i = 0; i < MAX_COLOR - 256; i++) {
    colors[c++] = mkcolor(0xff - (i >> 3), 0xff - (i >> 2), 0);
  }
}

static void clrscr(void) {
  memset(screen.shadow,
	 0,
	 ((screen.height-1) * screen.stride + screen.width) * sizeof(uint16_t));
}

static void putpixel(int x, int y, int c) {
  screen.shadow[y * screen.stride + x] = c;
}

static int getpixel(int x, int y) {
  return screen.shadow[y * screen.stride + x];
}

static void do_frame(Point loc) {
  int x, y, c;

  if (frame_counter == 0) {
    clrscr();
  } else {

#if WANT_FADE
    for (y = 0; y < screen.height; y++) {
      for (x = 0; x < screen.width; x++) {
	int p = getpixel(x, y);
	if (p) {
	  int R = (p >> 11) & 0x1f;
	  int G = (p >> 5) & 0x3f;
	  int B = p & 0x1f;
	  R = (int) R * FADE_FACTOR;
	  G = (int) G * FADE_FACTOR;
	  B = (int) B * FADE_FACTOR;
	  p = (R << 11) | (G << 5) | B;
	  putpixel(x, y, p);
	}
      }
    }
#endif

    masses[0].x = loc.h;
    masses[0].y = loc.v;
    masses[0].mass = M1;

    masses[1].x = screen.width - loc.h;
    masses[1].y = screen.height - loc.v;
    masses[1].mass = M2;

    for (c = 0; c < N_PARTICLES; c++) {
      struct particle *p = &particles[c];
      int bodynum;
      int color_index;
      float oldx = p->x;
      float oldy = p->y;

#if !WANT_REPLACEMENTS
      if (oldx == DEAD_PARTICLE_MARKER)
	continue;
#endif

      for (bodynum = 0; bodynum < N_MASSES; bodynum++) {
	float dx = masses[bodynum].x - p->x;
	float dy = masses[bodynum].y - p->y;
	float m = dx * dx + dy * dy;
	float k = masses[bodynum].mass / (m * sqrt(m));
	float dvx = k * dx;
	float dvy = k * dy;
	if (m < MASS_CLIP) {
#if WANT_REPLACEMENTS
	  init_particle(p);
#else
	  p->x = DEAD_PARTICLE_MARKER;
#endif
	  goto done_particle;
	}
	p->vx += dvx;
	p->vy += dvy;
      }

      color_index = sqrt(p->vx * p->vx + p->vy * p->vy) * 200;
      if (color_index > MAX_COLOR) color_index = MAX_COLOR;
      color_index = MAX_COLOR - color_index;

      p->x = p->x + p->vx;
      p->y = p->y + p->vy;
      if ((p->x < 0) || (p->y < 0) || (p->x >= screen.width) || (p->y >= screen.height)) {
	if ((p->x < -screen.width) || (p->y < -screen.height) || (p->x >= screen.width * 2) || (p->y > screen.height * 2)) {
#if WANT_REPLACEMENTS
	  init_particle(p);
#else
	  p->x = DEAD_PARTICLE_MARKER;
#endif
	}
      } else {
	putpixel(p->x, p->y, colors[color_index]);
      }
    done_particle:
#if ERASE_TRACKS
      if (oldx >= 0 && oldy >= 0 && oldx < screen.width && oldy < screen.height) {
	putpixel(oldx, oldy, 0);
      }
#endif
      continue;
    }
  }
}

int main(int argc, char *argv[]) {
  struct timeval t_start, t_stop;
  int inputfd = open("/dev/input/event1", O_RDONLY);
  if (inputfd < 0) perror("open /dev/input/event1");
  {
    int flags = fcntl(inputfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    if (fcntl(inputfd, F_SETFL, flags) < 0) perror("fcntl");
  }
  struct input_absinfo iinfo;
  if (ioctl(inputfd, EVIOCGABS(ABS_MT_POSITION_X), &iinfo) < 0) perror("ioctl ABS_MT_POSITION_X");
  uint32_t max_x = iinfo.maximum;
  printf("X %d %d\n", iinfo.minimum, iinfo.maximum);
  if (ioctl(inputfd, EVIOCGABS(ABS_MT_POSITION_Y), &iinfo) < 0) perror("ioctl ABS_MT_POSITION_Y");
  uint32_t max_y = iinfo.maximum;
  printf("Y %d %d\n", iinfo.minimum, iinfo.maximum);
  uint32_t curr_x, curr_y;

  setup_screen();
  setup_colors();
  setup_particles();

  gettimeofday(&t_start, NULL);

  srandom(time(NULL));

  frame_counter = 0;
  while (1) {
    do_frame((Point) {
	.h = curr_x * v.xres / max_x,
			.v = curr_y * v.yres / max_y
      });
    memcpy(screen.base, screen.shadow, screen.stride * screen.height * sizeof(uint16_t));
    frame_counter++;

    if (ioctl(fb, FBIOPAN_DISPLAY, &v) < 0) {
      perror("ioctl FBIOPAN_DISPLAY");
      close(fb);
      exit(1);
    }

    {
      struct input_event buf;
      unsigned int count = 0;
      ssize_t result;
      while ((result = read(inputfd, &buf, sizeof(buf))) == sizeof(buf)) {
	count++;
	switch (buf.type) {
	case EV_ABS:
	  switch (buf.code) {
	  case ABS_MT_POSITION_X:
	    curr_x = buf.value;
	    break;
	  case ABS_MT_POSITION_Y:
	    curr_y = buf.value;
	    break;
	  default:
	    break;
	  }
	  // fall through
	default:
	  printf("%u %u %d. ", buf.type, buf.code, buf.value);
	}
      }
      if (result != -1) printf("result read %ld\n", (long) result);
      if (errno != EWOULDBLOCK) perror("read inputfd");
      if (count > 0) printf("\n");
    }

    {
      struct timeval t_now;
      gettimeofday(&t_now, NULL);
      signed long d = (t_now.tv_sec - t_start.tv_sec) * 1000000 + (t_now.tv_usec - t_start.tv_usec);
      if ((frame_counter % FRAME_RATE_TARGET) == 0) {
	printf("actual %ld target %d\n", FRAME_RATE_TARGET * 1000000 / (d / (frame_counter / FRAME_RATE_TARGET)), FRAME_RATE_TARGET);
      }
      long nap = frame_counter * (1000000 / FRAME_RATE_TARGET) - d;
      if (nap > 0) {
    	usleep(nap);
      }
    }
  }

  gettimeofday(&t_stop, NULL);

  {
    long delta = (t_stop.tv_sec - t_start.tv_sec) * 1000000 + (t_stop.tv_usec - t_start.tv_usec);
    printf("%d frames took %lu microseconds, so %g frames/sec\n",
	   frame_counter,
	   delta,
	   frame_counter / (delta / 1000000.0));
  }
}
