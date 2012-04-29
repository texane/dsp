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

  /* amplitude, phase */
  double a[32];
  double phi[32];

  /* current angle, angular step */
  double w[32];
  double dw[32];

} tonegen_t;

static void tonegen_init(tonegen_t* gen)
{
  gen->n = 0;
}

static void tonegen_add
(tonegen_t* gen, double freq, double fsampl, double a, double phi)
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
      buf[i][0] += gen->a[j] * cos(gen->w[j] + gen->phi[j]);
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

__attribute__((unused))
static void ps_percent(fftw_complex* x, unsigned int nx, double* xx)
{
  /* power spectrum (percent of total amplitudes) */

  /* x the fourier transform */

  const unsigned int nxx = nx / 2;

  unsigned int i;
  double sum;

  sum = 0;
  for (i = 0; i < nxx; ++i)
  {
    /* dsp guide, steven w. smith, 573
       rectangular notation is:
       x[i] = re(X[i]) * cos(w) - im(X[i]) * sin(w);
     */

    /* do not care about the scaling factor since ampls are
       computed to be a percentage of the total spectrum
     */

    xx[i] = x[i][0] - x[i][1];
    sum += xx[i];
  }

  if (sum >= 0.0001)
  {
    for (i = 0; i < nxx; ++i) xx[i] /= sum;
  }
}

__attribute__((unused))
static void ps_abs(fftw_complex* x, unsigned int nx, double* xx)
{
  /* power spectrum (absolute amplitudes) */

  /* x the fourier transform */

  const unsigned int nxx = nx / 2;

  unsigned int i;

  for (i = 0; i < nxx; ++i)
  {
    /* no scaling factor applied by FFTW (refer to: what FFTW really computes)
       according dsp guide, p.570, a factor of nx is applied on the DFT coeffs.
       thus, we must divide by nx. however, we work only on the positive
       frequency side of the spectrum since negative side is symetric. thus,
       the actual scaling factor is (2 / nx).
     */

    const double re = x[i][0];
    const double im = x[i][1];

    /* (a / n)^2 + (b / n)^2 = (a^2 + b^2) / n */
    xx[i] = (2 * sqrt(re * re + im * im)) / (double)nx;
  }
}

__attribute__((unused))
static void compute_phi(fftw_complex* x, unsigned int nx, double* phi)
{
  /* x the fourier coefficient, rectangular form */
  /* phi the phase */

  const unsigned int nxx = nx / 2;

  unsigned int i;

  for (i = 0; i < nxx; ++i)
  {
    /* ((im / n) / (re / n)) = (im / re) thus no scaling needed */
    if (fabs(x[i][0]) >= 0.00001) phi[i] = atan(x[i][1] / x[i][0]);
    else phi[i] = 0;
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
  double* phi = fftw_malloc(nw * sizeof(double));

  tonegen_t gen;

  unsigned int i;

  tonegen_init(&gen);
  tonegen_add(&gen, 2000, fsampl, 100, 0);
  tonegen_add(&gen, 4000, fsampl, 100, 0.666);
  tonegen_add(&gen, 6000, fsampl, 100, 0);
  tonegen_read(&gen, x, nx);

  dft(x, nx, y);
  idft(y, nx, z);
  compute_phi(y, nx, phi);
  ps_abs(y, nx, w);

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
    printf("%lf %lf %lf %lf %lf\n",
	   f, w[i],
	   y[i][0] / (double)nbands,
	   -1 * y[i][1] / (double)nbands,
	   phi[i]);
  }
#endif

  fftw_free(x);
  fftw_free(y);
  fftw_free(z);
  fftw_free(w);
  fftw_free(phi);
}


int main(int ac, char** av)
{
  do_complex_dft();
  return 0;
}
