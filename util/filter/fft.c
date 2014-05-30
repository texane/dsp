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


/* filter using fft */

static void filter
(double* xx, const double* x, unsigned int n, const double* coeffs)
{
  /* n the size of the transform */

  fftw_plan plan;
  fftw_complex* out;
  unsigned int i;

  out = fftw_malloc((n / 2 + 1) * sizeof(fftw_complex));

  /* forward transform */

  plan = fftw_plan_dft_r2c_1d(n, (double*)x, out, FFTW_ESTIMATE);
  fftw_execute(plan);
  fftw_destroy_plan(plan);

  /* filter */

  for (i = 0; i != n / 2 + 1; ++i)
  {
    out[i][0] *= coeffs[i];
    out[i][1] *= coeffs[i];
  }

  /* normalized inverse transform */

  plan = fftw_plan_dft_c2r_1d(n, out, (double*)xx, FFTW_ESTIMATE);
  fftw_execute(plan);
  for (i = 0; i != n; ++i) xx[i] /= (double)n;
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

typedef struct node
{
  double triple[3];
  struct node* next;
} node_t;

typedef struct
{
#define CMDLINE_FLAG_FSAMPL (1 << 0)
#define CMDLINE_FLAG_TSAMPL (1 << 1)
#define CMDLINE_FLAG_IFILE (1 << 2)
#define CMDLINE_FLAG_OFILE (1 << 3)
#define CMDLINE_FLAG_ICOL (1 << 4)
  uint32_t flags;

  double fsampl;
  double tsampl[2];

  node_t* filters;

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

static int str_to_tuple(const char* s, double* t, size_t n)
{
  size_t i;

  for (i = 0; i != n; ++i)
  {
    t[i] = str_to_double(s);
    for (; *s && (*s != ':'); ++s) ;
    if (*s == 0) return -1;
    ++s;
  }

  return 0;
}

static int get_cmdline_info(cmdline_info_t* ci, int ac, char** av)
{
  node_t* node;
  size_t i;

  if (ac & 1) goto on_error;

  ci->flags = 0;
  ci->fsampl = 0;
  ci->filters = NULL;
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
    else if (strcmp(k, "-tsampl") == 0)
    {
      /* tlo:thi */
      ci->flags |= CMDLINE_FLAG_TSAMPL;
      str_to_tuple(v, ci->tsampl, 2);
    }
    else if (strcmp(k, "-filter") == 0)
    {
      /* flo:fhi:coeff */
      node = malloc(sizeof(node_t));
      if (node == NULL) goto on_error;
      node->next = ci->filters;
      ci->filters = node;
      str_to_tuple(v, node->triple, 3);
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
  double* coeffs;
  double* x;
  size_t nx;
  size_t i;
  size_t j;
  size_t k;
  size_t n;
  node_t* pos;

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

  if ((ci.flags & CMDLINE_FLAG_TSAMPL) == 0)
  {
    ci.flags |= CMDLINE_FLAG_TSAMPL;
    ci.tsampl[0] = 0.0;
    ci.tsampl[1] = ci.fsampl * (double)nx;
  }

  /* recompute sample count */

  i = (size_t)floor(ci.tsampl[0] * ci.fsampl);
  if (i >= nx)
  {
    PERROR();
    goto on_error_1;
  }

  n = (size_t)ceil((ci.tsampl[1] - ci.tsampl[0]) * ci.fsampl);
  if ((i + n) > nx)
  {
    PERROR();
    goto on_error_1;
  }

  fband = nsampl_to_fband(n, ci.fsampl);

  nbin = n / 2 + 1;

  /* build filtering coeff array */

  coeffs = malloc(nbin * sizeof(double));
  if (coeffs == NULL) goto on_error_1;

  for (j = 0; j != nbin; ++j) coeffs[j] = 1.0;

  for (pos = ci.filters; pos; pos = pos->next)
  {
    if (pos->triple[0] < 0) goto on_error_2;
    if (pos->triple[1] > ((double)nbin * fband)) goto on_error_2;
    if (pos->triple[0] > pos->triple[1]) goto on_error_2;

    j = floor(pos->triple[0] / fband);
    k = ceil(pos->triple[1] / fband);
    if (k > nbin) k = nbin;
    for (; j != k; ++j) coeffs[j] = pos->triple[2];
  }

  /* output */

  xx = malloc(n * sizeof(double));
  if (xx == NULL)
  {
    PERROR();
    goto on_error_2;
  }

  filter(xx, x + i, n, coeffs);

  for (j = 0; j != n; ++j)
  {
    printf("%zu %lf %lf\n", j, x[i + j], xx[j]);
  }

  err = 0;

  free(xx);
 on_error_2:
  pos = ci.filters;
  while (pos != NULL)
  {
    node_t* const tmp = pos;
    pos = pos->next;
    free(tmp);
  }
  free(coeffs);
 on_error_1:
  csv_close(&icsv);
 on_error_0:
  return err;
}
