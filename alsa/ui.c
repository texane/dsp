#include "ui.h"
#include "x.h"


#define CONFIG_USE_IMPULSE 1
#define CONFIG_MIN_FREQ 0
#define CONFIG_MAX_FREQ 4000


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

/* power spectrum frequencies focused on */
static unsigned int ps_min_band = 0;
static unsigned int ps_max_band = 0;
static unsigned int ps_nband = 0;


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

__attribute__((unused)) static void draw_impulse
(unsigned int x, unsigned int y0, unsigned int y1, const x_color_t* c)
{
  for (; y0 <= y1; ++y0)
    x_draw_pixel(screen, x, screen_height - y0 - 1, c);  
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
  /* clear using the pixel cache */

  unsigned int i;
  unsigned int j;

  if (tile->cache.is_dirty == 0) return ;

  for (i = 0; i < tile->cache.count; ++i)
  {
    for (j = 0; j < tile->cache.width; ++j)
    {
      const unsigned int x = tile_band_to_x(tile, i);
      const unsigned int y = tile->cache.buf[i * 2 + j];
#if CONFIG_USE_IMPULSE
      draw_impulse(x, tile->draw.y, y, tile->bg_color);
#else
      draw_pixel(x, y, tile->bg_color);
#endif
    }
  }

  tile->cache.is_dirty = 0;
}

static void tile_set_band
(
 ui_tile_t* tile,
 unsigned int i, unsigned int percent,
 unsigned int k,
 const x_color_t* c
)
{
  /* set the band i to value val */
  /* k the cache index */

  const unsigned int x = tile_band_to_x(tile, i);
  const unsigned int y = tile_percent_to_y(tile, percent);

  tile->cache.buf[i * 2 + k] = y;

#if CONFIG_USE_IMPULSE
  draw_impulse(x, tile->draw.y, y, c);
#else
  draw_pixel(x, y, c);
#endif
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


#if defined(CONFIG_MIN_FREQ)
/* convert frequency to band index */
static inline unsigned int freq_to_band(unsigned int freq, unsigned int fband)
{
  return freq / fband;
}
#endif /* CONFIG_MIN_FREQ */


/* exported */

int ui_init(unsigned int nband, unsigned int fband)
{
  static const unsigned char white_rgb[] = { 0xff, 0xff, 0xff };
  static const unsigned char black_rgb[] = { 0x00, 0x00, 0x00 };
  static const unsigned char red_rgb[] = { 0xff, 0x00, 0x00 };
  static const unsigned char blue_rgb[] = { 0x00, 0x00, 0xff };

  unsigned int hscale;
  unsigned int vscale;

  unsigned int i;

#if defined(CONFIG_MIN_FREQ)
  ps_min_band = freq_to_band(CONFIG_MIN_FREQ, fband);
  ps_max_band = 1 + freq_to_band(CONFIG_MAX_FREQ, fband);
#else
  ps_min_band = 0;
  ps_max_band = nband;
#endif
  ps_nband = ps_max_band - ps_min_band;

  /* force nband to ps_nband */
  nband = ps_nband;

  /* compute scalings before initliazing main window */
  screen_width = 500;
  screen_height = 500;

#if 0 /* unused, scale always set to 1 */
  hscale = (screen_width - TILE_FRAME_DIM) / nband;
  if (hscale == 0)
#endif
  {
    hscale = 4;
    screen_width = (hscale * nband) + TILE_FRAME_DIM;
  }

#if 0 /* unused, scale always set to 1 */
  vscale = (screen_height / 2 - TILE_FRAME_DIM) / 100;
  if (vscale == 0)
#endif
  {
    vscale = 1;
    screen_height = 2 * (vscale * 100 + TILE_FRAME_DIM);
  }

  if (x_initialize(screen_width, screen_height)) return -1;

  screen = x_get_screen();

  x_alloc_color(black_rgb, &black_color);
  x_alloc_color(white_rgb, &white_color);
  x_alloc_color(blue_rgb, &blue_color);
  x_alloc_color(red_rgb, &red_color);

  x_fill_surface(screen, black_color);

  /* power spectrum tile */

  ps_tile.screen.x = 0;
  ps_tile.screen.y = 0;
  ps_tile.screen.w = screen_width;
  ps_tile.screen.h = screen_height / 2;

  ps_tile.draw.x = TILE_FRAME_DIM;
  ps_tile.draw.y = TILE_FRAME_DIM;
  ps_tile.draw.hscale = hscale;
  ps_tile.draw.vscale = vscale;

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
  i = screen_height / 2 + 2;
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

void update_common_ps
(const double* ps_bands, unsigned int nband, unsigned int k)
{
  /* k the pixel cache index */

  const x_color_t* const c = ps_tile.fg_colors[k];

  unsigned int i;

  const int must_lock = SDL_MUSTLOCK(screen);

  if (must_lock) SDL_LockSurface(screen);

  /* update power spectrum tile */
  for (i = 0; i < ps_nband; ++i)
  {
    const unsigned int j = ps_min_band + i;
    unsigned int hacked_percent = convert_percent(ps_bands[j]);
    if (hacked_percent == 0) continue ;
    if (hacked_percent > 100) hacked_percent = 100;
    tile_set_band(&ps_tile, i, hacked_percent, k, c);
  }
  ps_tile.cache.is_dirty = 1;

  if (must_lock) SDL_UnlockSurface(screen);
}

void ui_update_ips(const double* ips_bands, unsigned int nband)
{
  /* ips_bands the input power spectrum bands, in percent */
  update_common_ps(ips_bands, nband, 0);
}

void ui_update_ops(const double* ops_bands, unsigned int nband)
{
  /* ops_bands the output power spectrum bands, in percent */
  update_common_ps(ops_bands, nband, 1);
}

void ui_update_begin(void)
{
  const int must_lock = SDL_MUSTLOCK(screen);

  if (must_lock) SDL_LockSurface(screen);
  tile_clear(&ps_tile);
#if 0 /* TODO */
  /* update frequency response tile */
  tile_clear(&fr_tile);
#endif
  if (must_lock) SDL_UnlockSurface(screen);
}

void ui_update_end(void)
{
  const int must_lock = SDL_MUSTLOCK(screen);

  if (must_lock) SDL_LockSurface(screen);
  SDL_Flip(screen);
  if (must_lock) SDL_UnlockSurface(screen);
}
