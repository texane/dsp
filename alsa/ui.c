#include "ui.h"
#include "x.h"


/* a tile is a subwindow in the main window, with a frame
   and a drawing area. the frame is not drawable. a pixel
   cache is maintained to accelerate the tile clearing.
   there are at most tile->cache_width pixels per column.
 */

typedef struct ui_tile
{
  /* tile in screen */
  struct
  {
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
  } screen;

  /* tile drawing area */
  struct
  {
    unsigned int x;
    unsigned int y;
    unsigned int hscale;
    unsigned int vscale;
  } draw;

  /* back and fore ground color */
  const x_color_t* frame_color;
  const x_color_t* bg_color;
  const x_color_t* fg_colors[2];

  /* pixel cache */
  struct
  {
    unsigned int* buf;
    unsigned int width;
    unsigned int count;
    unsigned int is_dirty;
  } cache;

} ui_tile_t;


/* global ui data: screen, power spectrum and freq response tiles */

static x_surface_t* screen = NULL;
static unsigned int screen_width;
static unsigned int screen_height;

static ui_tile_t ps_tile;

__attribute__((unused)) static ui_tile_t fr_tile;

static const x_color_t* black_color = NULL;
static const x_color_t* white_color = NULL;
static const x_color_t* red_color = NULL;
static const x_color_t* blue_color = NULL;


/* drawing routines. y always inverted. */

static inline void draw_pixel
(unsigned int x, unsigned int y, const x_color_t* c)
{
  x_draw_pixel(screen, x, screen_height - y - 1, c);
}

static inline void draw_line
(
 unsigned int x0, unsigned int y0,
 unsigned int x1, unsigned int y1,
 const x_color_t* c
)
{
  const unsigned int new_y0 = screen_height - 1 - y0;
  const unsigned int new_y1 = screen_height - 1 - y1;
  x_draw_line(screen, (int)x0, (int)new_y0, (int)x1, (int)new_y1, c);
}


/* tiling routines */

static inline unsigned int tile_band_to_x
(const ui_tile_t* tile, unsigned int band)
{
  /* convert band to x on tile drawing area */
  return tile->draw.x + band * tile->draw.hscale;
}

static inline unsigned int tile_percent_to_y
(const ui_tile_t* tile, unsigned int percent)
{
  /* convert percent to y on tile drawing area */
  return tile->draw.y + percent * tile->draw.vscale;
}

static void tile_clear(ui_tile_t* tile)
{
  unsigned int i;
  unsigned int j;

  if (tile->cache.is_dirty == 0) return ;

  for (i = 0; i < tile->cache.count; ++i)
  {
    for (j = 0; j < tile->cache.width; ++j)
    {
      const unsigned int x = tile_band_to_x(tile, i);
      const unsigned int y = tile->cache.buf[i * 2 + j];
      draw_pixel(x, y, tile->bg_color);
    }
  }

  tile->cache.is_dirty = 0;
}

static void tile_set_band
(ui_tile_t* tile, unsigned int i, unsigned int percent)
{
  /* set the band i to value val */

  const unsigned int x = tile_band_to_x(tile, i);
  const unsigned int y = tile_percent_to_y(tile, percent);

  draw_pixel(x, y, tile->fg_colors[0]);
  tile->cache.buf[i * 2 + 0] = y;
}

static void tile_draw_frame
(ui_tile_t* tile, unsigned int nbands, unsigned int fband)
{
  /* frame include axis and related stuffs */

#define TILE_FRAME_DIM 20
#define TILE_HZ_PER_GRAD 1000
#define TILE_PERCENT_PER_GRAD 10

  const unsigned int grad_dim = TILE_FRAME_DIM / 4;

  unsigned int fu;
  unsigned int bar;
  unsigned int step;
  unsigned int i;

  /* x axis. 1khz per grad. */

  fu = tile->draw.y - 1;
  bar = tile->screen.x + tile->screen.w - 1;
  draw_line(tile->screen.x, fu, bar, fu, tile->frame_color);

  step = TILE_HZ_PER_GRAD / fband;
  for (i = 0; i < nbands; i += step)
  {
    bar = tile_band_to_x(tile, i);
    draw_line(bar, fu - grad_dim, bar, fu, tile->frame_color);
  }

  /* y axis. 10 percent per grad. */

  fu = tile->draw.x - 1;
  bar = tile->screen.y + tile->screen.h - 1;
  draw_line(fu, tile->screen.y, fu, bar, tile->frame_color);

  for (i = 0; i < 100; i += 10)
  {
    bar = tile_percent_to_y(tile, i);
    draw_line(fu - grad_dim, bar, fu, bar, tile->frame_color);
  }
}


/* exported */

int ui_init(unsigned int nband, unsigned int fband)
{
  static const unsigned char white_rgb[] = { 0xff, 0xff, 0xff };
  static const unsigned char black_rgb[] = { 0x00, 0x00, 0x00 };
  static const unsigned char red_rgb[] = { 0xff, 0x00, 0x00 };
  static const unsigned char blue_rgb[] = { 0x00, 0x00, 0xff };

  unsigned int i;

  x_initialize();

  screen = x_get_screen();
  screen_width = (unsigned int)x_get_width();
  screen_height = (unsigned int)x_get_height();

  x_alloc_color(black_rgb, &black_color);
  x_alloc_color(white_rgb, &white_color);
  x_alloc_color(blue_rgb, &blue_color);
  x_alloc_color(red_rgb, &red_color);

  x_fill_surface(screen, black_color);

  /* power spectrum tile */

  ps_tile.screen.x = 0;
  ps_tile.screen.y = 0;
  ps_tile.screen.w = screen_width;
  ps_tile.screen.h = screen_height;

  ps_tile.draw.x = TILE_FRAME_DIM;
  ps_tile.draw.y = TILE_FRAME_DIM;
  ps_tile.draw.hscale = (screen_width - TILE_FRAME_DIM) / nband;
  ps_tile.draw.vscale = (screen_height / 2 - TILE_FRAME_DIM) / 100;

  ps_tile.frame_color = white_color;
  ps_tile.bg_color = black_color;
  ps_tile.fg_colors[0] = red_color;
  ps_tile.fg_colors[1] = blue_color;

  ps_tile.cache.width = 2;
  ps_tile.cache.count = nband;
  ps_tile.cache.is_dirty = 0;
  ps_tile.cache.buf = malloc(2 * nband * sizeof(unsigned int));
  for (i = 0; i < nband; ++i)
  {
    ps_tile.cache.buf[i * 2 + 0] = 0;
    ps_tile.cache.buf[i * 2 + 1] = 0;
  }

  tile_draw_frame(&ps_tile, nband, fband);

#if 0 /* todo */
  /* frequency response tile */
#endif

  /* draw split line */
  i = screen_height / 2;
  draw_line(0, i + 0, screen_width - 1, i + 0, white_color);
  draw_line(0, i + 1, screen_width - 1, i + 1, white_color);

  return 0;
}

void ui_fini(void)
{
  x_free_color(red_color);
  x_free_color(blue_color);
  x_free_color(white_color);
  x_free_color(black_color);

  x_cleanup();
}

static inline unsigned int convert_percent(double x)
{
  return (unsigned int)(x * 100);
}

void ui_update(const double* ips_bands, unsigned int nband)
{
  /* ips_bands the input power spectrum bands, in percent */

  unsigned int i;

  const int must_lock = SDL_MUSTLOCK(screen);

  if (must_lock) SDL_LockSurface(screen);

  /* update power spectrum tile */
  tile_clear(&ps_tile);
  for (i = 0; i < nband; ++i)
    tile_set_band(&ps_tile, i, convert_percent(ips_bands[i]));
  ps_tile.cache.is_dirty = 1;

#if 0 /* TODO */
  /* update frequency response tile */
  tile_clear(&fr_tile);
#endif

  SDL_Flip(screen);

  if (must_lock) SDL_UnlockSurface(screen);
}
