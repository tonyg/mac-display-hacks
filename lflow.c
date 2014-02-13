/* gcc -O3 -o fbflow fbflow.c
 */
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

static struct screen {
  uint32_t *base;
  uint32_t *shadow;
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
#define WANT_REPLACEMENTS 0

#define FADE_FACTOR 0.999

#define N_MASSES 2

#define MAX_COLOR 1024

#define VEL_SCALE 4
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

static uint32_t colors[MAX_COLOR];

static int frame_counter = 0;

static void setup_screen(void) {
  struct fb_fix_screeninfo f;
  struct fb_var_screeninfo v;
  int fb = open("/dev/fb0", O_RDWR);
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
  screen.stride = f.line_length / sizeof(uint32_t);
  screen.width = v.xres;
  screen.height = v.yres;
  screen.shadow = malloc(screen.stride * screen.height * sizeof(uint32_t));
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

static int mkcolor(int R, int G, int B) {
  return (R << 16) | (G << 8) | B;
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
	 ((screen.height-1) * screen.stride + screen.width) * sizeof(uint32_t));
}

static void putpixel(int x, int y, int c) {
  screen.shadow[y * screen.stride + x] = c;
}

static int getpixel(int x, int y) {
  return screen.shadow[y * screen.stride + x];
}

static void do_frame() {
  Point loc;
  int x, y, c;

  //GetMouse(&loc);
  loc.h = 100;
  loc.v = 100;

  if (frame_counter == 0) {
    clrscr();
  } else {

#if WANT_FADE
    for (y = 0; y < screen.height; y++) {
      for (x = 0; x < screen.width; x++) {
	int p = getpixel(x, y);
	if (p) {
	  int R = (p >> 16) & 0xff;
	  int G = (p >> 8) & 0xff;
	  int B = p & 0xff;
	  R = (int) R * FADE_FACTOR;
	  G = (int) G * FADE_FACTOR;
	  B = (int) B * FADE_FACTOR;
	  p = (R << 16) | (G << 8) | B;
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

  setup_screen();
  setup_colors();
  setup_particles();

  gettimeofday(&t_start, NULL);

  srandom(time(NULL));

  frame_counter = 0;
  while (1) {
    do_frame();
    memcpy(screen.base, screen.shadow, screen.stride * screen.height * sizeof(uint32_t));
    frame_counter++;

    {
      struct timeval t_now;
      gettimeofday(&t_now, NULL);
      signed long d = (t_now.tv_sec - t_start.tv_sec) * 1000000 + (t_now.tv_usec - t_start.tv_usec);
      long nap = frame_counter * (1000000 / 75) - d;
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
