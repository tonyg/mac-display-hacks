/* gcc -arch i386 -O3 -o fbheight-noerosion fbheight-noerosion.c -framework ApplicationServices -framework Carbon
 */
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>

#include <ApplicationServices/ApplicationServices.h>

static struct screen {
  uint32_t *base;
  uint32_t *shadow;
  int width;
  int height;
  int stride;
} screen;

static double *heights;
static int terrain_length;

typedef struct vec3 {
  double x, y, z;
} vec3;

#define V3(X,Y,Z) ((vec3){ (X), (Y), (Z) })

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))

static int frame_counter = 0;

static int isometric = 1;
static double water_level = 0.5;
static double ambient_bright = 0.05;
static vec3 lightvec = V3(-0.5, -0.1, -0.5);
static int vp_left = 0;
static int vp_top = 0;

#define VIEWPORT_WIDTH 256

#define VSCALE (terrain_length / 4)

static void setup_screen(void) {
  CGDirectDisplayID targetDisplay = kCGDirectMainDisplay;

  CGDisplayCapture(targetDisplay); /* crucial for being permitted to write on it */
  screen.base = (uint32_t *) CGDisplayBaseAddress(targetDisplay);
  screen.stride = CGDisplayBytesPerRow(targetDisplay) / sizeof(uint32_t);
  screen.width = CGDisplayPixelsWide(targetDisplay);
  screen.height = CGDisplayPixelsHigh(targetDisplay);
  screen.shadow = malloc(screen.stride * screen.height * sizeof(uint32_t));
}

static void setup_heights(void) {
  /* terrain_length = 1; */
  /* while (terrain_length < screen.width || terrain_length < screen.height) { */
  /*   terrain_length = terrain_length << 1; */
  /* } */
  /* terrain_length++; */
  terrain_length = 1024 + 1;
  heights = malloc(terrain_length * terrain_length * sizeof(double));
}

static double *height_at(int x, int y) {
  return &heights[(y % terrain_length) * terrain_length + (x % terrain_length)];
}

static double skew(double variation) {
  return (random() % 1001 - 500) * variation / 1000;
}

static double lerp(double a, double b, double r) {
  return a * r + b * (1 - r);
}

static void subdivide(int left, int top, int len, double variation, int R, int B) {
  int right = left + len - 1;
  int bottom = top + len - 1;
  int midx = left + len / 2;
  int midy = top + len / 2;
  double *tl = height_at(left, top);
  double *tm = height_at(midx, top);
  double *tr = height_at(right, top);
  double *ml = height_at(left, midy);
  double *mm = height_at(midx, midy);
  double *mr = height_at(right, midy);
  double *bl = height_at(left, bottom);
  double *bm = height_at(midx, bottom);
  double *br = height_at(right, bottom);
  double newvar = variation / 1.9;
  int newlen = len / 2 + 1;

  if (len > 2) {
    /*  */ *tm = lerp(*tl, *tr, 0.5) + skew(variation);
    if (B) *bm = lerp(*bl, *br, 0.5) + skew(variation);
    /*  */ *ml = lerp(*tl, *bl, 0.5) + skew(variation);
    if (R) *mr = lerp(*tr, *br, 0.5) + skew(variation);

    *mm = lerp(lerp(*tl, *tr, 0.5), lerp(*bl, *br, 0.5), 0.5) + skew(variation);

    subdivide(midx, midy, newlen, newvar, R, B);
    subdivide(left, midy, newlen, newvar, 0, B);
    subdivide(midx, top, newlen, newvar, R, 0);
    subdivide(left, top, newlen, newvar, 0, 0);
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

static int mkcolor(int R, int G, int B) {
  return (R << 16) | (G << 8) | B;
}

static vec3 mulf(vec3 v, double s) {
  return V3(v.x*s, v.y*s, v.z*s);
}

static vec3 cross(vec3 a, vec3 b) {
  return V3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

static double dot(vec3 a, vec3 b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

static double mag(vec3 v) {
  return sqrt(dot(v, v));
}

static vec3 negate(vec3 v) {
  return V3(-v.x, -v.y, -v.z);
}

static vec3 norm(vec3 v) {
  double m = mag(v);
  if (m != 0) {
    return mulf(v, 1/m);
  } else {
    return V3(0, 0, 1);
  }
}

static vec3 normal_at(int x, int y) {
  if (x >= terrain_length - 1) x = terrain_length - 2;
  if (y >= terrain_length - 1) y = terrain_length - 2;
  double *h = height_at(x, y);
  double *hx = height_at(x + 1, y);
  double *hy = height_at(x, y + 1);
  return norm(cross(V3(1, 0, VSCALE * (*h - *hx)),
		    V3(0, 1, VSCALE * (*h - *hy))));
}

static double brightness_at(int x, int y) {
  double c = dot(norm(negate(lightvec)), normal_at(x, y));
  if (c < 0) c = 0;
  return ambient_bright + c * (1.0 - ambient_bright);
}

static vec3 base_color(int x, int y) {
  double *h = height_at(x, y);
  if (*h < water_level) {
    return V3(0, 0, (*h + (1 - water_level)) / 2);
  } else {
    int V = (*h - water_level) / (1 - water_level);
    return mulf(V3(V, 1.0, V), brightness_at(x, y));
  }
}

#define BAR_HEIGHT 10
static void iso_putpixel(int x, int y, double hh, int c) {
  double A = M_PI/6;
  double C = cos(A);
  double S = sin(A);
  int h = VSCALE * hh;
  int x1 = x*C + y*C - (VIEWPORT_WIDTH * C - screen.width / 2);
  int y1 = y*S - x*S + (screen.height / 2) - h;

  if (x1 >= 0 && y1 >= 0 && x1 < screen.width && (y1 + BAR_HEIGHT) < screen.height) {
    int yy;
    for (yy = y1; yy < y1 + BAR_HEIGHT; yy++) {
      putpixel(x1, yy, c);
    }
  }
}

static void erode(void) {
  
}

static void fresh_map(void) {
  *height_at(0, 0) = 0.5;
  *height_at(terrain_length - 1, 0) = 0.5;
  *height_at(0, terrain_length - 1) = 0.5;
  *height_at(terrain_length - 1, terrain_length - 1) = 0.5;

  subdivide(0, 0, terrain_length, 0.5, 1, 1);
}

static void do_frame(void) {
  Point loc;
  uint32_t keys[4];
  int x, y;

  GetMouse(&loc);
  GetKeys(&keys[0]);
  if (keys[1] & 0x2000 /* n */) {
    fresh_map();
  }

  if (keys[1] & 0x20000 /* space */) {
    isometric = !isometric;
  }

    
  if (keys[1] & 0x1000000 /* shift */) {
    water_level = (double) loc.v / screen.height;
  } else if (keys[1] & 0x8000000 /* ctrl */) {
    lightvec = V3((double) loc.h / screen.width - 0.5,
		  (double) loc.v / screen.height - 0.5,
		  -0.5);
  } else {
    vp_left = (int) ((double) loc.h * (terrain_length - VIEWPORT_WIDTH) / screen.width);
    vp_top = (int) ((double) loc.v * (terrain_length - VIEWPORT_WIDTH) / screen.height);
  }

  clrscr();
  if (isometric) {
    for (y = 0; y < VIEWPORT_WIDTH; y++) {
      for (x = 0; x < VIEWPORT_WIDTH; x++) {
	vec3 c = mulf(base_color(x + vp_left, y + vp_top), 255);
	double hh = *height_at(x + vp_left, y + vp_top) - water_level;
	if (hh < 0) hh = 0;
	iso_putpixel(x, y, hh, mkcolor(c.x, c.y, c.z));
	/* double *h = height_at(x, y); */
	/* int c = ((int)(*h * (screen.width / 2)) & 1) * 255; */
	/* putpixel(x, y, mkcolor(c,c,c)); */
      }
    }
  } else {
    for (y = 0; y < VIEWPORT_WIDTH; y++) {
      for (x = 0; x < VIEWPORT_WIDTH; x++) {
	vec3 c = mulf(base_color(x + vp_left, y + vp_top), 255);
	putpixel(x, y, mkcolor(c.x, c.y, c.z));
      }
    }
  }

  erode();
}

int main(int argc, char *argv[]) {
  struct timeval t_start, t_stop;

  srandom(time(NULL));

  setup_screen();
  setup_heights();

  fresh_map();

  /* { */
  /*   int y, x; */
  /*   for (y = 0; y < screen.height; y++) { */
  /*     for (x = 0; x < screen.width; x++) { */
  /* 	*height_at(x, y) = sin(x / 100.0) * sin(y / 100.0) * 0.5 + 0.5; */
  /*     } */
  /*   } */
  /* } */

  gettimeofday(&t_start, NULL);

  frame_counter = 0;
  while (!Button()) {
    do_frame();
    memcpy(screen.base, screen.shadow, screen.stride * screen.height * sizeof(uint32_t));
    frame_counter++;

    {
      struct timeval t_now;
      gettimeofday(&t_now, NULL);
      useconds_t d = (t_now.tv_sec - t_start.tv_sec) * 1000000 + (t_now.tv_usec - t_start.tv_usec);
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
