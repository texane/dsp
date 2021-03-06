#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>
#include "../common/csv.h"


#ifdef CONFIG_PERROR
#define PERROR()				\
do {						\
printf("[!] %s, %u\n", __FILE__, __LINE__);	\
} while (0)
#else
#define PERROR()
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


/* command line */

typedef struct
{
#define CMDLINE_FLAG_FSAMPL (1 << 0)
#define CMDLINE_FLAG_TSAMPL_LO (1 << 1)
#define CMDLINE_FLAG_TSAMPL_HI (1 << 2)
#define CMDLINE_FLAG_IFILE (1 << 3)
#define CMDLINE_FLAG_OFILE (1 << 4)
#define CMDLINE_FLAG_ICOL (1 << 5)
  uint32_t flags;

  double fsampl;
  double tsampl_lo;
  double tsampl_hi;

  const char* ifile;
  const char* ofile;

  size_t icol;

} cmdline_info_t;

static double str_to_double(const char* s)
{
  if ((strlen(s) > 2) && (s[0] == '0') && (s[1] == 'x'))
    return (double)strtoul(s, NULL, 16);
  return strtod(s, NULL);
}

static int get_cmdline_info(cmdline_info_t* ci, int ac, char** av)
{
  size_t i;

  if (ac & 1) goto on_error;

  ci->flags = 0;

  ci->fsampl = 0;
  ci->tsampl_lo = 0;
  ci->tsampl_hi = 0;
  ci->ifile = NULL;
  ci->ofile = NULL;
  ci->icol = 0;

  for (i = 0; i != ac; i += 2)
  {
    const char* const k = av[i + 0];
    const char* const v = av[i + 1];

    if (strcmp(k, "-fsampl") == 0)
    {
      ci->flags |= CMDLINE_FLAG_FSAMPL;
      ci->fsampl = str_to_double(v);
    }
    else if (strcmp(k, "-tsampl_lo") == 0)
    {
      /* in seconds, double format */
      ci->flags |= CMDLINE_FLAG_TSAMPL_LO;
      ci->tsampl_lo = str_to_double(v);
    }
    else if (strcmp(k, "-tsampl_hi") == 0)
    {
      /* in seconds, double format */
      ci->flags |= CMDLINE_FLAG_TSAMPL_HI;
      ci->tsampl_hi = str_to_double(v);
    }
    else if (strcmp(k, "-ifile") == 0)
    {
      /* input file */
      ci->flags |= CMDLINE_FLAG_IFILE;
      ci->ifile = v;
    }
    else if (strcmp(k, "-ofile") == 0)
    {
      /* output file */
      ci->flags |= CMDLINE_FLAG_OFILE;
      ci->ofile = v;
    }
    else if (strcmp(k, "-icol") == 0)
    {
      /* input column */
      ci->flags |= CMDLINE_FLAG_ICOL;
      ci->icol = (size_t)str_to_double(v);
    }
    else
    {
      goto on_error;
    }
  }

  /* check and default values */

  return 0;

 on_error:
  PERROR();
  return -1;
}

/* main */

int main(int ac, char** av)
{
  int err = -1;
  cmdline_info_t ci;
  csv_handle_t icsv;
  double fband;
  size_t nbin;
  double* xx;
  double* ps;
  double* x;
  size_t nx;
  size_t i;
  size_t n;

  if (get_cmdline_info(&ci, ac - 1, av + 1))
  {
    PERROR();
    goto on_error_0;
  }

  if ((ci.flags & CMDLINE_FLAG_IFILE) == 0)
  {
    PERROR();
    goto on_error_0;
  }

  if (csv_load_file(&icsv, ci.ifile))
  {
    PERROR();
    goto on_error_0;
  }

  if ((ci.flags & CMDLINE_FLAG_ICOL) == 0)
  {
    PERROR();
    goto on_error_1;
  }

  if (csv_get_col(&icsv, ci.icol, &x, &nx))
  {
    PERROR();
    goto on_error_1;
  }

  /* default tsampl_lo tsampl_hi */

  if ((ci.flags & CMDLINE_FLAG_FSAMPL) == 0)
  {
    PERROR();
    goto on_error_1;
  }

  if ((ci.flags & CMDLINE_FLAG_TSAMPL_LO) == 0)
  {
    ci.flags |= CMDLINE_FLAG_TSAMPL_LO;
    ci.tsampl_lo = 0;
  }

  if ((ci.flags & CMDLINE_FLAG_TSAMPL_HI) == 0)
  {
    ci.flags |= CMDLINE_FLAG_TSAMPL_HI;
    ci.tsampl_hi = ci.fsampl * (double)nx;
  }

  /* recompute sample count */

  i = (size_t)floor(ci.tsampl_lo * ci.fsampl);
  if (i >= nx)
  {
    PERROR();
    goto on_error_1;
  }

  n = (size_t)ceil((ci.tsampl_hi - ci.tsampl_lo) * ci.fsampl);
  if ((i + n) > nx)
  {
    PERROR();
    goto on_error_1;
  }

  fband = nsampl_to_fband(n, ci.fsampl);

  nbin = n / 2 + 1;

  ps = malloc(nbin * sizeof(double));
  if (ps == NULL)
  {
    PERROR();
    goto on_error_1;
  }

  xx = malloc(nbin * 2 * sizeof(double));
  if (xx == NULL)
  {
    PERROR();
    goto on_error_2;
  }

  fft(xx, x + i, n);
  fft_to_power_spectrum(ps, xx, nbin);

  for (i = 0; i < nbin; ++i)
  {
    printf("%lf %lf\n", bin_to_freq(i, fband), ps[i]);
  }

  err = 0;

  free(xx);
 on_error_2:
  free(ps);
 on_error_1:
  csv_close(&icsv);
 on_error_0:
  return err;
}
