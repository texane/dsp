#ifdef _WIN32
# include <memory.h>
# include <SDL.H>
#else /* __linux__ */
# include <SDL/SDL.h>
#endif /* _WIN32 */
#include "x.h"



struct x_color
{
  Uint32 value;
  Uint8 rgb[3];
};



struct x_event
{
  int type;
};



static SDL_Surface* g_screen = NULL;
static const x_color_t* pink_color = NULL;
static const unsigned char pink_rgb[3] = {0xf2, 0, 0xf2};



__attribute__((unused))
static Uint32 on_timer(Uint32 interval, void* param)
{
  static SDL_UserEvent event =
    {
#define SDL_TICKEVENT SDL_USEREVENT + 0
      SDL_TICKEVENT,
      0,
      NULL,
      NULL
    };

  SDL_PushEvent((SDL_Event*)&event);

  return interval;
}



int x_initialize(unsigned int w, unsigned int h)
{
  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_DOUBLEBUF) < 0)
    return -1;

  g_screen = SDL_SetVideoMode(w, h, 16, SDL_SWSURFACE);
  if (g_screen == NULL)
    {
      SDL_Quit();
      return -1;
    }

  x_alloc_color(pink_rgb, &pink_color);
  SDL_SetColorKey(g_screen, SDL_SRCCOLORKEY, pink_color->value);

  return 0;
}



void x_cleanup(void)
{
  if (g_screen != NULL)
    {
      SDL_FreeSurface(g_screen);
      g_screen = NULL;
    }

  SDL_Quit();
}


void x_loop(int (*on_event)(const struct x_event*, void*), void* ctx)
{
  SDL_Event event;
  struct x_event x_event;
  int is_done;
  static const struct x_color* black_color;
  static const unsigned char black_rgb[3] = {0, 0, 0};

  if (on_event == NULL)
    return ;

  if (x_alloc_color(black_rgb, &black_color) == -1)
    return ;

  is_done = 0;

  while (!is_done)
    {
      SDL_WaitEvent(&event);
      switch (event.type)
	{
	case SDL_TICKEVENT:
	  {
	    if (SDL_MUSTLOCK(g_screen))
	      SDL_LockSurface(g_screen);

	    SDL_FillRect(g_screen, NULL, black_color->value);

	    x_event.type = X_EVENT_TICK;
	    if (on_event(&x_event, ctx) == -1)
	      is_done = 1;

	    SDL_Flip(g_screen);

	    if (SDL_MUSTLOCK(g_screen))
	      SDL_UnlockSurface(g_screen);

	    break;
	  }

	case SDL_KEYDOWN:
	  {
	    switch (event.key.keysym.sym)
	      {
	      case SDLK_SPACE:
		x_event.type = X_EVENT_KDOWN_SPACE;
		on_event(&x_event, ctx);
		break;

	      default:
		break;
	      }

	    break;
	  }

	case SDL_KEYUP:
	  {
	    switch (event.key.keysym.sym)
	      {
	      case SDLK_SPACE:
		x_event.type = X_EVENT_KUP_SPACE;
		on_event(&x_event, ctx);
		break;

	      default:
		break;
	      }

	    break;
	  }

	case SDL_QUIT:
	  x_event.type = X_EVENT_QUIT;
	  if (on_event(&x_event, ctx) == -1)
	    is_done = 1;
	  break;

	default:
	  break;
	}
    }

  x_free_color(black_color);
}



int x_alloc_color(const unsigned char* rgb, const struct x_color** res)
{
  static struct x_color colors[32];
  static unsigned int i = 0;

  unsigned int j;
  struct x_color* color;

  for (j = 0; j < i; ++j)
    if (!memcmp(colors[j].rgb, rgb, sizeof(colors[0].rgb)))
      {
	*res = &colors[j];
	return 0;
      }

  if (i >= (sizeof(colors) / sizeof(colors[0])))
    return -1;

  color = &colors[i++];

  color->rgb[0] = rgb[0];
  color->rgb[1] = rgb[1];
  color->rgb[2] = rgb[2];

  color->value = SDL_MapRGB(g_screen->format, color->rgb[0], color->rgb[1], color->rgb[2]);

  *res = color;

  return 0;
}


void x_remap_color(const unsigned char* rgb, const struct x_color* const_color)
{
  struct x_color* const color = (struct x_color*)const_color;

  color->rgb[0] = rgb[0];
  color->rgb[1] = rgb[1];
  color->rgb[2] = rgb[2];

  ((struct x_color*)color)->value = SDL_MapRGB(g_screen->format, color->rgb[0], color->rgb[1], color->rgb[2]);
}


void x_free_color(const struct x_color* color)
{
}


void x_draw_pixel
(x_surface_t* s, int x, int y, const struct x_color* c)
{
  switch (s->format->BytesPerPixel)
  {
  case 1:
    {
      Uint8 *bufp;
      bufp = (Uint8 *)s->pixels + y * s->pitch + x;
      *bufp = c->value;
      break;
    }

  case 2:
    {
      Uint16 *bufp;
      bufp = (Uint16 *)s->pixels + y * s->pitch /  2 + x;
      *bufp = c->value;
      break;
    }

  case 3:
    {
      Uint8 *bufp;
      bufp = (Uint8 *)s->pixels + y * s->pitch + x;
      *(bufp + s->format->Rshift / 8) = c->rgb[0];
      *(bufp + s->format->Gshift / 8) = c->rgb[1];
      *(bufp + s->format->Bshift / 8) = c->rgb[2];
      break;
    }

  case 4:
    {
      Uint32 *bufp;
      bufp = (Uint32 *)s->pixels + y * s->pitch / 4 + x;
      *bufp = c->value;
      break;
    }

  default: break;
  }
}


void x_draw_line
(x_surface_t* s, int x0, int y0, int x1, int y1, const x_color_t* c)
{
  int dx;
  int dy;
  int x;
  int y;
  int e;
  int step;

#define swap_ints(a, b)	\
  do {			\
    const int __tmp = a;\
    a = b;		\
    b = __tmp;		\
  } while (0)

  if (x0 == x1)
  {
    if (y0 > y1)
      swap_ints(y0, y1);

    for (y = y0; y < y1; ++y)
      x_draw_pixel(s, x0, y, c);

    return ;
  }
  else if (y0 == y1)
  {
    if (x0 > x1)
      swap_ints(x0, x1);

    for (x = x0; x < x1; ++x)
      x_draw_pixel(s, x, y0, c);

    return ;
  }

  dx = x1 - x0;
  if (dx < 0)
    dx *= -1;

  dy = y1 - y0;
  if (dy < 0)
    dy *= -1;

  if (dx > dy)
  {
    if (x0 > x1)
    {
      swap_ints(x0, x1);
      swap_ints(y0, y1);
    }

    if (y0 > y1)
      step = -1;
    else
      step = 1;

    e = 0;
    x = x0;
    y = y0;

    while (x <= x1)
    {
      x_draw_pixel(s, x, y, c);

      e += dy;

      if ((2 * e) >= dx)
      {
	y += step;
	e -= dx;
      }

      ++x;
    }
  }
  else
  {
    if (y0 > y1)
    {
      swap_ints(x0, x1);
      swap_ints(y0, y1);
    }

    if (x0 > x1)
      step = -1;
    else
      step = 1;

    e = 0;
    x = x0;
    y = y0;

    while (y <= y1)
    {
      x_draw_pixel(s, x, y, c);

      e += dx;

      if ((2 * e) >= dy)
      {
	x += step;
	e -= dy;
      }

      ++y;
    }
  }
}

static void draw_circle_points
(x_surface_t* s, int cx, int cy, int x, int y, const x_color_t* c)
{
  if (x == 0)
  {
    x_draw_pixel(s, cx, cy + y, c);
    x_draw_pixel(s, cx, cy - y, c);
    x_draw_pixel(s, cx + y, cy, c);
    x_draw_pixel(s, cx - y, cy, c);
  }
  else if (x == y)
  {
    x_draw_pixel(s, cx + x, cy + y, c);
    x_draw_pixel(s, cx - x, cy + y, c);
    x_draw_pixel(s, cx + x, cy - y, c);
    x_draw_pixel(s, cx - x, cy - y, c);
  }
  else if (x < y)
  {
    x_draw_pixel(s, cx + x, cy + y, c);
    x_draw_pixel(s, cx - x, cy + y, c);
    x_draw_pixel(s, cx + x, cy - y, c);
    x_draw_pixel(s, cx - x, cy - y, c);
    x_draw_pixel(s, cx + y, cy + x, c);
    x_draw_pixel(s, cx - y, cy + x, c);
    x_draw_pixel(s, cx + y, cy - x, c);
    x_draw_pixel(s, cx - y, cy - x, c);
  }
}


void x_draw_circle
(x_surface_t* s, int cx, int cy, int r, const struct x_color* c)
{
  /* xc, yc the center x,y */

  int x = 0;
  int y = r;
  int p = (5 - r * 4) / 4;

  draw_circle_points(s, cx, cy, x, y, c);

  while (x < y)
  {
    ++x;

    if (p < 0)
    {
      p += 2 * x + 1;
    }
    else
    {
      --y;
      p += 2 * (x - y) + 1;
    }

    draw_circle_points(s, cx, cy, x, y, c);
  }
}

static void x_draw_disk_lines
(x_surface_t* s, int cx, int cy, int x, int y, const x_color_t* c)
{
  if (x == 0)
  {
    x_draw_line(s, cx, cy - y, cx, cy + y, c);
    x_draw_line(s, cx - y, cy, cx + y, cy, c);
  }
  else if (x == y)
  {
    x_draw_line(s, cx - x, cy + y, cx + x, cy + y, c);
    x_draw_line(s, cx - x, cy - y, cx + x, cy - y, c);
  }
  else if (x < y)
  {
    x_draw_line(s, cx - x, cy - y, cx - x, cy + y, c);
    x_draw_line(s, cx + x, cy - y, cx + x, cy + y, c);
    x_draw_line(s, cx - y, cy + x, cx + y, cy + x, c);
    x_draw_line(s, cx - y, cy - x, cx + y, cy - x, c);
  }
}


void x_draw_disk
(x_surface_t* s, int cx, int cy, int r, const x_color_t* c)
{
  /* xc, yc the center x,y */

  int x = 0;
  int y = r;
  int p = (5 - r * 4) / 4;

  x_draw_disk_lines(s, cx, cy, x, y, c);

  while (x < y)
  {
    ++x;

    if (p < 0)
    {
      p += 2 * x + 1;
    }
    else
    {
      --y;
      p += 2 * (x - y) + 1;
    }

    x_draw_disk_lines(s, cx, cy, x, y, c);
  }
}


int x_event_get_type(const struct x_event* event)
{
  return event->type;
}


x_surface_t* x_create_surface(int w, int h)
{
  x_surface_t* const s = SDL_CreateRGBSurface
    (SDL_SWSURFACE, w, h, 16, 0, 0, 0, 0);

  if (s == NULL)
    return NULL;

  SDL_SetColorKey(s, SDL_SRCCOLORKEY, pink_color->value);

  return s;
}


void x_free_surface(x_surface_t* surface)
{
  SDL_FreeSurface(surface);
}


void x_blit_surface(x_surface_t* ds, x_surface_t* ss)
{
  /* ss the source surface */
  SDL_BlitSurface(ss, NULL, ds, NULL);
}

const x_color_t* x_get_transparency_color(void)
{
  return pink_color;
}


void x_fill_surface(x_surface_t* s, const x_color_t* c)
{
  SDL_FillRect(s, NULL, c->value);
}


x_surface_t* x_get_screen(void)
{
  return g_screen;
}
