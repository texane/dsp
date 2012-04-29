#include <math.h>
#include "tonegen.h"

void tonegen_init(tonegen_t* gen)
{
  gen->n = 0;
}

void tonegen_add
(tonegen_t* gen, double fsampl, double freq, double a, double phi)
{
  /* freq the tone frequency */
  /* fsampl the sampling frequency */
  /* a the amplitude */

  const unsigned int i = gen->n++;

  gen->w[i] = 0.0;
  gen->a[i] = a;
  gen->phi[i] = phi;
  gen->dw[i] = (2.0 * M_PI * freq) / fsampl;
}

void tonegen_read(tonegen_t* gen, double* buf, unsigned int n)
{
  unsigned int i;
  unsigned int j;

  for (i = 0; i < n; ++i)
  {
    buf[i] = 0;

    for (j = 0; j < gen->n; ++j)
    {
      buf[i] += gen->a[j] * cos(gen->w[j] + gen->phi[j]);
      gen->w[j] += gen->dw[j];
    }
  }
}
