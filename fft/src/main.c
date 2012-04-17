#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


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


static void fft(const double* x, unsigned int n, double* xx)
{
}


static void ifft(const double* xx, unsigned int n, double* x)
{
}


static void filter(const double* , unsigned int n)
{
}


static inline unsigned int freq_to_bin
(unsigned int freq, unsigned int fsampl, unsigned int fft_size)
{
  /* frequency to fft bin */
  return (2 * fft_size * freq) / fsampl;
}

static inline unsigned int bin_to_freq
(unsigned int bin, unsigned int fsampl, unsigned int fft_size)
{
  /* fft bin to frequency */
  return (fsampl * bin) / (2 * fft_size);
}

static inline void freqs_to_bin
(const unsigned int* freqs, unsigned int nfreq, unsigned int* bins)
{
  
}

static void fft_to_power_spectrum
(const double* fft, unsigned int fft_size)
{
}



int main(int ac, char** av)
{
#if 0
  /* the constraints are linked to the FFT bandwidth */

  dsp_idev_t idev;
  dsp_odev_t odev;

  /* sizes are sample count */
  unsigned int buf_size;
  unsigned int read_size;
  unsigned int fft_size;

  while (1)
  {
    if (off + read_size > buf_size)
    {
      memcpy(buf, , );
      off = 0;
    }

    dsp_dev_read(&dev, buf + off, read_size);
  }
#endif

  printf("%u\n", compute_sampl_buf_size(48000, 1, 10));

  return 0;
}
