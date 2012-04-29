#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <fftw3.h>
#include "tonegen.h"


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

  /* ensure first and last values are 0 */
  x[0] = 0;
  x[nx - 1] = 0;

#if 0
  {
    const unsigned int fu = 20;
    const double delta = M_PI / (4 * (double)fu);
    for (i = 0; i < fu * 2; ++i)
      x[ii - fu + i] = cos((double)i * delta);
  }
#endif
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


static void make_filter_kernel
(const double* fresp, unsigned int nx, double* kernel)
{
  /* dsp_smith, p.298 */

  const unsigned int nxx = nx * 2;

  fftw_complex* in;
  fftw_complex* out;
  fftw_plan plan;
  unsigned int i;
  unsigned int nk;

  /* compute impulse response using idft(fresp) */

  in = fftw_malloc(nxx * sizeof(fftw_complex));
  out = fftw_malloc(nxx * sizeof(fftw_complex));
  for (i = 0; i < nx; ++i)
  {
    /* convert polar (no phase) to rectangular form */
    /* frequency, in radians per seconds */
    const double w = ((double)i * 2 * M_PI) / (double)nxx;
    in[i][0] = fresp[i] * cos(w);
    in[i][1] = fresp[i] * sin(w);
  }

  /* alias the remaining coefficients */

  for (i = 0; i < nx; ++i)
  {
    in[nx + i][0] = in[nx - i - 1][0];
    in[nx + i][1] = in[nx - i - 1][1];
  }

  plan = fftw_plan_dft_1d(nxx, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
  fftw_execute(plan);

  /* make the filter kernel: shift and pad with 0 */

  nk = nxx < 16 ? nxx : 16;

  const unsigned int nkk = nk / 2;
  for (i = 0; i < nkk; ++i)
  {
    const double re = out[i][0];
    const double im = out[i][1];
    const double mag = sqrt(re * re + im * im) / (double)nxx;
    kernel[nkk - i - 1] = mag;
    kernel[nkk + i + 0] = mag;
  }

  for (i = nk; i < nxx; ++i) kernel[i] = 0;

  /* apply blackman window on [0, nk[ */

  double* const w = malloc(nk * sizeof(double)); 
  make_blackman_window(w, nk);
  for (i = 0; i < nk; ++i) kernel[i] *= w[i];
  free(w);

  fftw_free(in);
  fftw_free(out);
  fftw_destroy_plan(plan);
}


static void fft(const double* x, unsigned int nx, double* xx)
{
  fftw_complex* const in = fftw_malloc(nx * sizeof(fftw_complex));
  fftw_complex* const out = fftw_malloc(nx * sizeof(fftw_complex));
  unsigned int i;

  for (i = 0; i < nx; ++i)
  {
    in[i][0] = x[i];
    in[i][1] = 0;
  }

  fftw_plan plan = fftw_plan_dft_1d(nx, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  fftw_execute(plan);
  fftw_destroy_plan(plan);

  for (i = 0; i < nx / 2; ++i)
  {
    const double re = out[i][0];
    const double im = out[i][1];
    xx[i] = (2 * sqrt(re * re + im * im)) / (double)nx;
  }

  fftw_free(in);
  fftw_free(out);
}


static void convolve
(
 double* xx, unsigned int nxx,
 const double* x, unsigned int nx,
 const double* h, unsigned int nh
)
{
  int i;
  int j;

  for (i = 0; i < (int)nxx; ++i)
  {
    xx[i] = 0;

    for (j = 0; j < (int)nh; ++j)
    {
      if (i - j < 0) continue ;
      if (i - j >= nx) continue ;

      xx[i] += h[j] * x[i - j];
    }
  }
}


static void do_impulse_response(void)
{
  static const double fsampl = 48000;
  static const double fband = 100;
  static const double fcut = 4000;

  const unsigned int nbands = 1 + fsampl / (2 * fband);

  double* const fresp = malloc(nbands * sizeof(double));
  double* const kernel = malloc(nbands * 2 *  sizeof(double));
  double* const kernel_fresp = malloc(nbands * sizeof(double));

  unsigned int i;

  make_lowpass_fresp(fresp, nbands, fcut, fsampl);
  make_filter_kernel(fresp, nbands, kernel);

#if 0
  fft(kernel, nbands * 2, kernel_fresp);
  for (i = 0; i < nbands; ++i)
  {
    printf("%lf %lf %lf %lf\n",
	   (double)i * fband,
	   fresp[i], kernel[i],
	   kernel_fresp[i]);
  }
#else
  tonegen_t gen;
  const unsigned int nx = nbands * 2 * 5;
  double* ibuf = malloc(nx * sizeof(double));
  double* obuf = malloc(nx * sizeof(double));
  tonegen_init(&gen);
  tonegen_add(&gen, fsampl, 2000, 10, 0);
  tonegen_add(&gen, fsampl, 6000, 10, 0);
  tonegen_read(&gen, ibuf, nx);
  convolve(obuf, nx, ibuf, nx, kernel, nbands * 2);
  for (i = 0; i < nx; ++i) printf("%u %lf %lf\n", i, ibuf[i], obuf[i]);
  free(ibuf);
  free(obuf);
#endif

  free(fresp);
  free(kernel);
  free(kernel_fresp);
}


int main(int ac, char** av)
{
  do_impulse_response();
  return 0;
}
