#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>


/* tone generator */

typedef struct tonegen
{
  /* tone count */
  unsigned int n;

  /* current angle, angular step */
  double w[32];
  double a[32];
  double dw[32];

} tonegen_t;

static void tonegen_init(tonegen_t* gen)
{
  gen->n = 0;
}

static void tonegen_add(tonegen_t* gen, double freq, double fsampl, double a)
{
  /* freq the tone frequency */
  /* fsampl the sampling frequency */
  /* a the amplitude */

  const unsigned int i = gen->n++;

  gen->w[i] = 0.0;
  gen->a[i] = a;
  gen->dw[i] = (2.0 * M_PI * freq) / fsampl;
}

static void tonegen_read(tonegen_t* gen, fftw_complex* buf, unsigned int n)
{
  unsigned int i;
  unsigned int j;

  for (i = 0; i < n; ++i)
  {
    buf[i][0] = 0;
    buf[i][1] = 0;

    for (j = 0; j < gen->n; ++j)
    {
      buf[i][0] += gen->a[j] * sin(gen->w[j]);
      gen->w[j] += gen->dw[j];
    }
  }
}


/* millisecond to sample count */

static inline unsigned int ms_to_nsampl
(unsigned int fsampl, unsigned int ms)
{
  return (fsampl * ms) / 1000;
}


/* bandwidth frequency to sample count */

static inline unsigned int fband_to_nsampl(double fband, double fsampl)
{
  return fsampl / fband;
}

static inline double nsampl_to_fband(unsigned int nsampl, double fsampl)
{
  return fsampl / (double)nsampl;
}

static void dft(fftw_complex* in, unsigned int nx, fftw_complex* out)
{
  fftw_plan plan = fftw_plan_dft_1d(nx, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  fftw_execute(plan);
  fftw_destroy_plan(plan);
}

static void idft(fftw_complex* in, unsigned int nx, fftw_complex* out)
{
  fftw_plan plan = fftw_plan_dft_1d(nx, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
  fftw_execute(plan);
  fftw_destroy_plan(plan);
}

static void ps(fftw_complex* x, unsigned int nx, double* xx)
{
  /* power spectrum */

  /* x the fourier transform */

  const unsigned int nxx = nx / 2;

  unsigned int i;
  double sum;

  sum = 0;
  for (i = 0; i < nxx; ++i)
  {
    xx[i] = fabs(x[i][0] + x[i][1]);
    sum += xx[i];
  }

  if (sum >= 0.0001)
  {
    for (i = 0; i < nxx; ++i) xx[i] /= sum;
  }
}

static void do_complex_dft(void)
{
  static const double fsampl = 48000;

  double fband = 100;
  unsigned int nbands = fsampl / (2 * fband);
  if (nbands & 1)
  {
    ++nbands;
    fband = fsampl / (2 * (double)nbands);
  }

  const unsigned int nx = nbands * 2;
  const unsigned int nw = nbands;

  fftw_complex* const x = fftw_malloc(nx * sizeof(fftw_complex));
  fftw_complex* const y = fftw_malloc(nx * sizeof(fftw_complex));
  fftw_complex* const z = fftw_malloc(nx * sizeof(fftw_complex));
  double* w = fftw_malloc(nw * sizeof(double));

  tonegen_t gen;

  unsigned int i;

  tonegen_init(&gen);
  tonegen_add(&gen, 600, fsampl, 1);
  tonegen_add(&gen, 6000, fsampl, 2);
  tonegen_read(&gen, x, nx);

  dft(x, nx, y);
  idft(y, nx, z);
  ps(y, nx, w);

#if 0
  for (i = 0; i < nx; ++i)
  {
    const double zz = z[i][0] / (double)nx;
    printf("%u %lf %lf %lf %lf\n", i, x[i][0], zz, y[i][0], y[i][1]);
  }
#elif 0
  for (i = 0; i < nx; ++i)
  {
    printf("%u %lf\n", i, x[i][0]);
  }
#else
  for (i = 0; i < nw; ++i)
  {
    const double f = fband * (double)i;
    printf("%lf %lf\n", f, w[i]);
  }
#endif

  fftw_free(x);
  fftw_free(y);
  fftw_free(z);
  fftw_free(w);
}


int main(int ac, char** av)
{
  do_complex_dft();
  return 0;
}
