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


/* monotone generator */

typedef struct tonegen
{
  /* current angle, angular step */
  double w;
  double dw;
} tonegen_t;

static void tonegen_init(tonegen_t* gen, double freq, double fsampl)
{
  /* freq the tone frequency */
  /* fsampl the sampling frequency */

  gen->w = 0.0;
  gen->dw = (2.0 * M_PI * freq) / fsampl;
}

static void tonegen_read(tonegen_t* gen, double* buf, unsigned int n)
{
  unsigned int i;
  for (i = 0; i < n; ++i, gen->w += gen->dw) buf[i] = sin(gen->w);
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
  static const double ftone = 4000.0;

  /* may be updated for nsampl to fit pow2 */
  double fband = 50.0;

  unsigned int log2_nsampl;

  tonegen_t gen;
  unsigned int nsampl;
  unsigned int nbin;
  unsigned int i;
  double* x;
  double* xx;
  double* ps;

  tonegen_init(&gen, ftone, fsampl);

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
  xx = malloc(nbin * 2 * sizeof(double));
  ps = malloc(nbin * sizeof(double));

  tonegen_read(&gen, x, nsampl);

  fft(xx, x, nsampl);

  fft_to_power_spectrum(ps, xx, nbin);

  for (i = 0; i < nbin; ++i)
    printf("%lf %lf\n", bin_to_freq(i, fband), ps[i]);

  free(x);
  free(xx);
  free(ps);

  return 0;
}
