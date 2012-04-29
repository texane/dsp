#ifndef TONEGEN_H_INCLUDED
# define TONEGEN_H_INCLUDED

#include <fftw3.h>

/* tone generator */

typedef struct tonegen
{
  /* tone count */
  unsigned int n;

  /* amplitude, phase */
  double a[32];
  double phi[32];

  /* current angle, angular step */
  double w[32];
  double dw[32];

} tonegen_t;

void tonegen_init(tonegen_t*);
void tonegen_add(tonegen_t*, double, double, double, double);
void tonegen_read(tonegen_t*, double*, unsigned int);
void tonegen_read_complex(tonegen_t*, fftw_complex*, unsigned int);


#endif /* ! TONEGEN_H_INCLUDED */
