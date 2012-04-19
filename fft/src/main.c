#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>


#if 0

typedef uint16_t dsp_sampl_type;


static void alloc_sampl_buf()
{
}


static void free_sampl_buf()
{
}


/* dsp input device */

typedef struct dsp_dev
{
  /* dummy signal generator */

  double t; /* current time */
  double dt; /* time step */
  double w; /* angular frequency */

} dsp_dev_t;

static void dsp_dev_open(dsp_dev_t* dev, dsp_buf_t* buf)
{
  static double fsampl = 48000;
  static unsigned int tsampl = ;
}

static void dsp_dev_close(dsp_dev_t* dev)
{
}

static void dsp_dev_read(dsp_dev_t* dev, void* buf, unsigned int n)
{
  /* read n samples into buf */

  unsigned int i;
  for (i = 0; i < n; ++i) buf[i] = ;
}


/* dsp output device */

typedef struct dsp_odev
{
  

} dsp_odev_t;

#endif



/* compute sizes for a given bandwidth and time */

static inline unsigned int compute_sampl_buf_size
(unsigned int fsampl, unsigned int fband, unsigned int tsampl)
{
  /* fsampl the sampling frequency, in hz */
  /* fband the bandwidth frequency, in hz */
  /* tsampl the sampling duration, in millisecond */
  /* we have: bufsize = 1 + (fsampl * tsampl) / (fband * 1000) */

  return 1 + (fsampl * tsampl) / (fband * 1000);
}


/* frequency to fft bin routines */

static inline unsigned int freq_to_bin(unsigned int freq, double fband)
{
  /* frequency to fft bin */
  return (unsigned int)(freq / fband);
}

static inline double bin_to_freq(unsigned int bin, double fband)
{
  /* fft bin to frequency */
  return fband * (double)bin;
}

static inline void freqs_to_bins
(
 unsigned int* bins,
 const unsigned int* freqs, unsigned int nfreq,
 double fband
)
{
  unsigned int i;
  for (i = 0; i < nfreq; ++i) bins[i] = freq_to_bin(freqs[i], fband);
}

static inline void bins_to_freqs
(
 unsigned int* freqs,
 const unsigned int* bins, unsigned int nbins,
 double fband
)
{
  unsigned int i;
  for (i = 0; i < nbins; ++i) freqs[i] = bin_to_freq(bins[i], fband);
}


/* compute normalized power spectrum */

static void fft_to_power_spectrum
(double* ps, const double* xx, unsigned int nxx)
{
  /* xx the real fft coeffs */
  /* nxx the xx count */

  double sum = 0;
  unsigned int i;

  for (i = 0; i < nxx; ++i)
  {
    const double re = xx[i * 2 + 0];
    const double im = xx[i * 2 + 1];
    const double p = sqrt(re * re + im * im);

    ps[i] = p;
    sum += p;
  }

  /* normalize (percent of) */
  if (sum > 0.00001) for (i = 0; i < nxx; ++i) ps[i] /= sum;
}


/* compute the real dft using fft algorithm */

static void fft
(double* xx, const double* x, unsigned int n)
{
  /* n the size of the transform */

  fftw_plan plan;
  fftw_complex* out;
  unsigned int i;

  out = fftw_malloc((n / 2 + 1) * sizeof(fftw_complex));
  plan = fftw_plan_dft_r2c_1d(n, (double*)x, out, FFTW_ESTIMATE);

  fftw_execute(plan);

  for (i = 0; i < n / 2 + 1; ++i)
  {
    xx[i * 2 + 0] = out[i][0];
    xx[i * 2 + 1] = out[i][1];
  }

  fftw_destroy_plan(plan);
  fftw_free(out);
}


/* inplace finite impulse response filter */

#if 0 /* unused */
static inline unsigned int min_uint(unsigned int a, unsigned int b)
{
  return a < b ? a : b;
}
#endif /* unused */

static void convolve
(
 double** xx, unsigned int* nxx,
 const double* x, unsigned int nx,
 const double* h, unsigned int nh
)
{
  int i;
  int j;

  *nxx = nx + nh;
  *xx = malloc((*nxx) * sizeof(double));

  for (i = 0; i < (int)*nxx; ++i)
  {
    (*xx)[i] = 0;

    for (j = 0; j < (int)nh; ++j)
    {
      if (i - j < 0) continue ;
      if (i - j >= nx) continue ;

      (*xx)[i] += h[j] * x[i - j];
    }
  }
}

static void fir
(
 double** xx, unsigned int* nxx,
 const double* x, unsigned int nx
)
{
  static const double h[] =
  {
#if 0
    1964.09456418888,
    -10208.338288738,
    16795.2945884697,
    -0.0,
    -23281.999649468,
    0.0572589041085133,
    37237.1250088098,
    -0.0,
    -50515.8159352444,
    -0.0,
    56020.202875443,
    -0.0,
    -50515.8159352444,
    -0.0,
    37237.1250088098,
    0.0572589041085133,
    -23281.999649468,
    -0.0,
    16795.2945884697,
    -10208.338288738,
    1964.09456418888
#else
# include "/tmp/fu.h"
#endif
  };

  static const unsigned int nh = sizeof(h) / sizeof(h[0]);
  convolve(xx, nxx, x, nx, h, nh);
}


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


/* main */

int main(int ac, char** av)
{
  static const double fsampl = 48000.0;

  static const double ftones[] = { 3000.0, 12000.0 };
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

#if 0 /* compute print the power spectrum */

  xx = malloc(nbin * 2 * sizeof(double));
  fft(xx, x, nsampl);

  fft_to_power_spectrum(ps, xx, nbin);

  for (i = 0; i < nbin; ++i)
    printf("%lf %lf\n", bin_to_freq(i, fband), ps[i]);

#else /* print the signal time domain */

  fir(&xx, &nxx, x, nsampl);

  for (i = 0; i < (nsampl < 100 ? nsampl : 100); ++i)
  {
    printf("%lf %lf %lf\n", (double)i / fsampl, xx[i], x[i]);
  }

#endif

  if (x) free(x);
  if (xx) free(xx);
  if (ps) free(ps);
  if (gains) free(gains);

  return 0;
}
