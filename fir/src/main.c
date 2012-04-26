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
  double dw[32];

} tonegen_t;

static void tonegen_init(tonegen_t* gen)
{
  gen->n = 0;
}

static void tonegen_add(tonegen_t* gen, double freq, double fsampl)
{
  /* freq the tone frequency */
  /* fsampl the sampling frequency */

  const unsigned int i = gen->n++;

  gen->w[i] = 0.0;
  gen->dw[i] = (2.0 * M_PI * freq) / fsampl;
}

static void tonegen_read(tonegen_t* gen, double* buf, unsigned int n)
{
  unsigned int i;
  unsigned int j;

  for (i = 0; i < n; ++i)
  {
    buf[i] = 0;

    for (j = 0; j < gen->n; ++j)
    {
      buf[i] += sin(gen->w[j]);
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


/* compute impulse response from frequency response. */

static void make_lowpass_fresp
(double* x, unsigned int nx, double fcut, double fsampl)
{
  /* make a lowpass frequency response */

  /* fcut the cutoff frequency */
  /* fsampl the sampling frequency */

  const unsigned int ii = fcut * 2 * nx / fsampl;
  
  unsigned int i;
  for (i = 0; i < ii; ++i) x[i] = 1;
  for (; i < nx; ++i) x[i] = 0;

#if 0
  {
    const unsigned int fu = 20;
    const double delta = M_PI / (4 * (double)fu);
    for (i = 0; i < fu * 2; ++i)
      x[ii - fu + i] = cos((double)i * delta);
  }
#endif
}


static void idft(const double* x, unsigned int nx, double* xx)
{
  fftw_complex* in = fftw_malloc(nx * sizeof(fftw_complex));
  fftw_complex* out = fftw_malloc(nx * sizeof(fftw_complex));
  fftw_plan plan = fftw_plan_dft_1d(nx, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
  unsigned int i;

  for (i = 0; i < nx; ++i)
  {
    in[i][0] = x[i] * sqrt(0.5);
    in[i][1] = 0;
  }

  fftw_execute(plan);

  for (i = 0; i < nx; ++i)
  {
    const double re = out[i][0];
    const double im = out[i][1];
#if 0
    xx[i] = sqrt(re * re + im * im) / (double)nx;
#else
    xx[i] = re / (double)nx;
#endif
  }

  fftw_free(in);
  fftw_free(out);
  fftw_destroy_plan(plan);
}


static void make_blackman_window(double* x, unsigned int nx)
{
  unsigned int i;
  for (i = 0; i < nx; ++i)
  {
    x[i] = 0.42 - 0.5 * cos((2.0 * M_PI * (double)i) / (double)nx)
      + 0.08 * cos((4 * M_PI * (double)i) / (double)nx);
  }
}


static void mul(double* x, double* y, unsigned int nx)
{
  /* assume nx == ny */
  unsigned int i;
  for (i = 0; i < nx; ++i) x[i] *= y[i];
}


static void do_impulse_response(void)
{
  static const double fsampl = 48000;
  static const double fband = 100;
  static const double fcut = 6000;

  const unsigned int nbands = 1 + fsampl / (2 * fband);

  double* const fresp = malloc(nbands * sizeof(double));
  double* const iresp = malloc(nbands * 2 * sizeof(double));
  double* const w = malloc(nbands * sizeof(double)); 

  unsigned int i;

  make_lowpass_fresp(fresp, nbands, fcut, fsampl);
  make_blackman_window(w, nbands);
  idft(fresp, nbands, iresp);
  mul(iresp, w, nbands);

  for (i = 0; i < nbands; ++i) printf("%lf\n", iresp[i]);

  free(fresp);
  free(iresp);
}


int main(int ac, char** av)
{
  do_impulse_response();
  return 0;
}


#if 0

/* main */

int main(int ac, char** av)
{
  static const double fsampl = 48000.0;

  static const double ftones[] = { 100.0, 12000.0 };
  /* static const double ftones[] = { 400.0, 666.0, 4000.0, 22222.0 }; */
  /* static const double ftones[] = { 400.0, 666.0, 4000.0 }; */
  /* static const double ftones[] = { 400.0 }; */

  /* may be updated for nsampl to fit pow2 */
  double fband = 20.0;

  unsigned int log2_nsampl;

  tonegen_t gen;
  unsigned int nsampl;
  unsigned int nbin;
  unsigned int i;
  unsigned int nxx;
  double* x = NULL;
  double* xx = NULL;
  double* ps = NULL;
  double* gains = NULL;

  /* compute nsampl according to fband. adjust to be pow2 */
  nsampl = fband_to_nsampl(fband, fsampl);
  log2_nsampl = (unsigned int)log2(nsampl);
  if (nsampl != (1 << log2_nsampl))
  {
    nsampl = 1 << (log2_nsampl + 1);
    fband = nsampl_to_fband(nsampl, fsampl);
  }

  nbin = nsampl / 2 + 1;

  x = malloc(nsampl * sizeof(double));
  ps = malloc(nbin * sizeof(double));

  /* init tone generator and output samples */
  tonegen_init(&gen);
  for (i = 0; i < sizeof(ftones) / sizeof(double); ++i)
    tonegen_add(&gen, ftones[i], fsampl);
  tonegen_read(&gen, x, nsampl);

#if 0 /* unused */
  /* set some frequency gains to 3 db */
  gains = malloc(nbin * sizeof(double));
  for (i = 0; i < nbins; ++i) gains[i] = 0;
  gains[freq_to_bin(666.0, fband)] = 3;
  gains[freq_to_bin(22222.0, fband)] = 3;
#endif

  return 0;
}

#endif
