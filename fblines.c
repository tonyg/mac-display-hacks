/* gcc -arch i386 -O3 -o fblines fblines.c -framework ApplicationServices -framework Carbon
 */
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <ApplicationServices/ApplicationServices.h>

static struct screen {
  uint32_t *base;
  int width;
  int height;
  int stride;
} screen;

typedef struct keydef {
    int index;
    int mask;
} keydef_t;

#define TARGET_FPS 500

#define KEYDEF(i, m)	((keydef_t) { .index = (i), .mask = (m) })

#define DIR_R 0
#define DIR_U 1
#define DIR_L 2
#define DIR_D 3

typedef struct player {
  int x;
  int y;
  int direction;
  int is_dead;
  int color;
  keydef_t keydefs[4]; /* right, up, left, down */
} player_t;

static player_t *players;
static int n_players;
static int alive_count;

static int frame_counter = 0;

static void setup_screen(void) {
  CGDirectDisplayID targetDisplay = kCGDirectMainDisplay;

  CGDisplayCapture(targetDisplay); /* crucial for being permitted to write on it */
  screen.base = (uint32_t *) CGDisplayBaseAddress(targetDisplay);
  screen.stride = CGDisplayBytesPerRow(targetDisplay) / sizeof(uint32_t);
  screen.width = CGDisplayPixelsWide(targetDisplay);
  screen.height = CGDisplayPixelsHigh(targetDisplay);
}

static void init_player(player_t *p, int color) {
  p->x = random() % screen.width;
  p->y = random() % screen.height;
  p->direction = 0;
  p->is_dead = 0;
  p->color = color;
  /* keydefs to be initialized manually after this */
}

static int mkcolor(int R, int G, int B) {
  return (R << 16) | (G << 8) | B;
}

static void setup_players(void) {
  n_players = 2;

  players = malloc(n_players * sizeof(player_t));

  init_player(&players[0], mkcolor(255, 0, 0));
  players[0].keydefs[0] = KEYDEF(3, 0x10000000) /* right */;
  players[0].keydefs[1] = KEYDEF(3, 0x40000000) /* up */;
  players[0].keydefs[2] = KEYDEF(3, 0x08000000) /* left */;
  players[0].keydefs[3] = KEYDEF(3, 0x20000000) /* down */;

  init_player(&players[1], mkcolor(0, 255, 0));
  players[1].keydefs[0] = KEYDEF(0, 0x00000002) /* s */;
  players[1].keydefs[1] = KEYDEF(0, 0x00002000) /* w */;
  players[1].keydefs[2] = KEYDEF(0, 0x00000001) /* a */;
  players[1].keydefs[3] = KEYDEF(0, 0x00000040) /* z */;
}

static void clrscr(void) {
  memset(screen.base,
	 0,
	 ((screen.height-1) * screen.stride + screen.width) * sizeof(uint32_t));
}

static void putpixel(int x, int y, int c) {
  screen.base[y * screen.stride + x] = c;
}

static int getpixel(int x, int y) {
  return screen.base[y * screen.stride + x];
}

static void do_frame() {
  uint32_t keys[4];
  int i, d;

  GetKeys(&keys[0]);

  for (i = 0; i < n_players; i++) {
    player_t *p = &players[i];
    if (p->is_dead) {
      continue;
    }

    for (d = 0; d < 4; d++) {
      if (keys[p->keydefs[d].index] & p->keydefs[d].mask) {
	p->direction = d;
	break;
      }
    }

    switch (p->direction) {
      case DIR_R: p->x++; break;
      case DIR_U: p->y--; break;
      case DIR_L: p->x--; break;
      case DIR_D: p->y++; break;
      default:
	fprintf(stderr, "Bad direction %d\n", p->direction);
	abort();
    }

    p->x = (p->x + screen.width) % screen.width;
    p->y = (p->y + screen.height) % screen.height;
  }

  alive_count = 0;

  for (i = 0; i < n_players; i++) {
    player_t *p = &players[i];
    if (p->is_dead) {
      continue;
    }

    if (getpixel(p->x, p->y) != 0) {
      /* crash! */
      printf("Crash for player %d at %d, %d\n", i, p->x, p->y);
      p->is_dead = 1;
      continue;
    }

    alive_count++;

    putpixel(p->x, p->y, p->color);
  }
}

int main(int argc, char *argv[]) {
  struct timeval t_start, t_stop;

  srandom(time(NULL));

  setup_screen();
  setup_players();

  gettimeofday(&t_start, NULL);

  clrscr();

  frame_counter = 0;
  alive_count = n_players;

  while ((alive_count > 1) && !Button()) {
    do_frame();
    frame_counter++;

    {
      struct timeval t_now;
      gettimeofday(&t_now, NULL);
      useconds_t d = (t_now.tv_sec - t_start.tv_sec) * 1000000 + (t_now.tv_usec - t_start.tv_usec);
      long nap = frame_counter * (1000000.0 / TARGET_FPS) - d;
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

  {
    int i;
    for (i = 0; i < n_players; i++) {
      player_t *p = &players[i];
      if (p->is_dead) {
	printf("player %d DIED\n", i);
      } else {
	printf("PLAYER %d SURVIVED!!!\n", i);
      }
    }
  }
}
