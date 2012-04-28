#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

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
(tonegen_t* gen, double fsampl, double freq, double a, double phi)
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

static void tonegen_read(tonegen_t* gen, double* buf, unsigned int n)
{
  unsigned int i;
  unsigned int j;

  for (i = 0; i < n; ++i)
  {
    buf[i] = 0;

    for (j = 0; j < gen->n; ++j)
    {
      buf[i] += gen->a[j] * cos(gen->w[j] + gen->phi[j]);
      gen->w[j] += gen->dw[j];
    }
  }
}

static inline int split3
(const char* s, double* a, double* b, double* c)
{
  sscanf(s, "%lf,%lf,%lf", a, b, c);
  return 0;
}

static void convert_double_to_s16x2
(int16_t* rawbuf, const double* genbuf, unsigned int n)
{
  unsigned int i;
  for (i = 0; i < n; ++i)
  {
    const int16_t x = (int16_t)genbuf[i];
    rawbuf[i * 2 + 0] = x;
    rawbuf[i * 2 + 1] = x;
  }
}

int main(int ac, char** av)
{
  static const double fsampl = 44100;

  tonegen_t gen;
  unsigned int i;
  unsigned int ms;
  unsigned int nsampl;

#define CONFIG_BUF_SIZE 1024
  double genbuf[CONFIG_BUF_SIZE];
  int16_t rawbuf[2 * CONFIG_BUF_SIZE];

  if (ac <= 2) return -1;
  ms = atoi(av[1]);
  nsampl = ((unsigned int)fsampl * ms) / 1000;

  tonegen_init(&gen);

  for (i = 2; i < ac; ++i)
  {
    double w = 0;
    double a = 0;
    double phi = 0;

    if (split3(av[i], &w, &a, &phi) == -1) continue;

    tonegen_add(&gen, fsampl, w, a, phi);
  }

  for (i = 0; i < nsampl; i += CONFIG_BUF_SIZE)
  {
    unsigned int n = CONFIG_BUF_SIZE;
    if ((i + CONFIG_BUF_SIZE) > nsampl) n = nsampl - i;
    tonegen_read(&gen, genbuf, n);
    convert_double_to_s16x2(rawbuf, genbuf, n);
    write(1, rawbuf, n * 2 * sizeof(int16_t));
  }

  return 0;
}
