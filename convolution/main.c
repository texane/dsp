/* convolution routines */
/* dsp guide, chapters 5 and 6 */


#include <stdio.h>
#include <math.h>


static inline unsigned int min(unsigned int a, unsigned int b)
{
  return a < b ? a : b;
}


static void conv0
(
 const double* x, unsigned int nx,
 const double* h, unsigned int nh,
 double* y
)
{
  unsigned int i;
  unsigned int j;

  for (i = 0; i < nx; ++i) y[i] = 0;

  for (i = 0; i < nx; ++i)
  {
    const unsigned int nn = min(nh, nx - i);
    for (j = 0; j < nn; ++j)
      y[i + j] += x[i] * h[j];
  }
}


static void conv1
(
 const double* x, unsigned int nx,
 const double* h, unsigned int nh,
 double* y
)
{
  unsigned int i;
  unsigned int j;

  for (i = 0; i < nx; ++i)
  {
    const unsigned int nn = min(i, nh);
    y[i] = 0;
    for (j = 0; j < nn; ++j)
      y[i] += x[i - j] * h[j];
  }
}


int main(int ac, char** av)
{
  unsigned int i;

  /* generate inputs */
#if 0

#define NX 80
#define NH 30
  double x[NX];
  double y0[NX];
  double y1[NX];
  double h[NH];

  /* low + hi freqs */
  for (i = 0; i < NX; ++i) x[i] = 0;
  for (i = 8; i < (NX - 10); ++i)
    x[i] = cos((double)i * (M_PI / 8)) + cos((double)i * (M_PI / 40));

  /* inverting attenuator kernel */
  for (i = 0; i < NH; ++i) h[i] = 0;
  h[15] = -0.5;

#elif 1

#define NH 9
#define NX NH
  double x[NX] = { 1, -0.5, -0.25, 0, };
  double h[NH] = { 0, -1, -1.25, 2, 1.3, 1.3, 0.75, 0, -0.75 };
  double y0[NX];
  double y1[NX];

#endif


  conv0(x, NX, h, NH, y0);
  conv1(x, NX, h, NH, y1);

  for (i = 0; i < NX; ++i)
    printf("%u %lf %lf %lf\n", i, x[i], y0[i], y1[i]);

  return 0;
}
