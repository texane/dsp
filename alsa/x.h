#ifndef GRAPHICS_X_H_INCLUDED
# define GRAPHICS_X_H_INCLUDED


#ifdef _WIN32
# include <memory.h>
# include <SDL.H>
#else /* __linux__ */
# include <SDL/SDL.h>
#endif /* _WIN32 */


typedef struct x_color x_color_t;
struct x_event;
typedef SDL_Surface x_surface_t;


/* event types
 */

#define X_EVENT_TICK 0
#define X_EVENT_QUIT 1
#define X_EVENT_KUP_SPACE 2
#define X_EVENT_KDOWN_SPACE 3


int x_initialize(void);
void x_cleanup(void);
void x_loop(int (*)(const struct x_event*, void*), void*);
int x_alloc_color(const unsigned char*, const x_color_t**);
void x_remap_color(const unsigned char*, const x_color_t*);
void x_free_color(const x_color_t*);
void x_draw_pixel(x_surface_t*, int, int, const x_color_t*);
void x_draw_line(x_surface_t*, int, int, int, int, const x_color_t*);
void x_draw_circle(x_surface_t*, int, int, int, const x_color_t*);
void x_draw_disk(x_surface_t*, int, int, int, const x_color_t*);
int x_event_get_type(const struct x_event*);
int x_get_width(void);
int x_get_height(void);
const x_color_t* x_get_transparency_color(void);
x_surface_t* x_create_surface(int, int);
void x_free_surface(x_surface_t*);
void x_blit_surface(x_surface_t*, x_surface_t*);
void x_fill_surface(x_surface_t*, const x_color_t*);
x_surface_t* x_get_screen(void);


#endif /* ! GRAPHICS_X_H_INCLUDED */
