/* reference: http://equalarea.com/paul/alsa-audio.html */

/* alsa plugings references
   http://www.volkerschatz.com/noise/alsa.html
   http://www.alsa-project.org/alsa-doc/alsa-lib/pcm_plugins.html
   http://www.mail-archive.com/alsa-user@lists.sourceforge.net/msg21396.html
 */

/* reference: http://www.linuxjournal.com/article/6735
   an audio card has an assoicated large circular buffer.
   tranferring all data in one would result in latency,
   so ALSA transfers dat in unit of periods. the period
   size is configurable.
   overrun occurs when data is not read fast enough by
   the application. underrun occurs when the application
   does not transmit data fast enough for the playback
   device. ALSA calls the 2 states XRUN.
 */

/* tone generation using ecasound
   ecasound -i tone,sine,400,2000 -o /tmp/tone.wav
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <math.h> /* log2 */
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>
#include "ui.h"


/* static configuration */

#define CONFIG_FSAMPL 44100
#define CONFIG_FBAND 25
#define CONFIG_NCHAN 2
#define CONFIG_TIMING_CONTROL 0
#define CONFIG_ENABLE_PLAYBACK 1


/* buffer allocation */

static int alloc_buf(void** buf, unsigned int size)
{
  if (posix_memalign(buf, 0x1000, size)) return -1;
  mlock(*buf, size);
  return 0;
}

static void free_buf(void* buf, unsigned int size)
{
  /* note: may have failed */
  munlock(buf, size);
  free(buf);
}

static int alloc_buf3(void* bufs[3], unsigned int size)
{
  unsigned int i;

  for (i = 0; i < 3; ++i)
  {
    if (alloc_buf(&bufs[i], size))
    {
      for (i -= 1; (int)i > 0; --i)
      {
	free_buf(bufs[i], size);
	bufs[i] = NULL;
      }

      return -1;
    }
  }

  return 0;
}

static void free_buf3(void* bufs[3], unsigned int size)
{
  unsigned int i;
  for (i = 0; i < 3; ++i) if (bufs[i]) free_buf(bufs[i], size);
}


/* buffer index permutation */

static inline unsigned int perm3(unsigned int x)
{
  return x == 2 ? 0 : x + 1;
}


/* pcm device routines */

#if 0 /* salsa */
#define snd_strerror(__x) ""
#endif /* salsa */

static int setup_common_dev(snd_pcm_t* pcm, unsigned int nsampl)
{
  /* 44100 hz sampling rate, 2 channels, int16_t samples */

  snd_pcm_hw_params_t* parms;
  int err;

  snd_pcm_hw_params_alloca(&parms);

  if ((err = snd_pcm_hw_params_any(pcm, parms)))
  {
    printf("[!] snd_pcm_hw_params_any: %s\n", snd_strerror(err));
    goto on_error;
  }

  err = snd_pcm_hw_params_set_access(pcm, parms, SND_PCM_ACCESS_RW_INTERLEAVED);
  if (err)
  {
    printf("[!] snd_pcm_hw_params_set_access: %s\n", snd_strerror(err));
    goto on_error;
  }

  if ((err = snd_pcm_hw_params_set_format(pcm, parms, SND_PCM_FORMAT_S16_LE)))
  {
    printf("[!] snd_pcm_hw_params_set_format: %s\n", snd_strerror(err));
    goto on_error;
  }

  if ((err = snd_pcm_hw_params_set_rate(pcm, parms, CONFIG_FSAMPL, 0)))
  {
    printf("[!] snd_pcm_hw_params_set_rate: %s\n", snd_strerror(err));
    goto on_error;
  }

  if ((err = snd_pcm_hw_params_set_channels(pcm, parms, CONFIG_NCHAN)))
  {
    printf("[!] snd_pcm_hw_params_set_channels: %s\n", snd_strerror(err));
    goto on_error;
  }

  if ((err = snd_pcm_hw_params_set_period_size(pcm, parms, nsampl, 0)))
  {
    printf("[!] snd_pcm_hw_params_set_period_size: %s\n", snd_strerror(err));
    goto on_error;
  }

  if ((err = snd_pcm_hw_params(pcm, parms)))
  {
    printf("[!] snd_pcm_hw_params: %s\n", snd_strerror(err));
    goto on_error;
  }

#if 1 /* info */
  {
    snd_pcm_uframes_t frames;
    unsigned int us;
    int dir;
    snd_pcm_hw_params_get_period_size(parms, &frames, &dir);
    snd_pcm_hw_params_get_period_time(parms, &us, &dir);
    printf("period: %d %u\n", (int)frames, us);

    us = 0;
    dir = 1;
    snd_pcm_hw_params_get_rate_min(parms, &us, &dir);
    printf("min_rate: %u\n", us);
  }
#endif /* info */

 on_error:
  return err;
} 

static int open_capture_dev
(snd_pcm_t** pcm, const char* name, unsigned int nsampl)
{
  int err;

  /* err = snd_pcm_open(pcm, name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK); */
  /* err = snd_pcm_open(pcm, name, SND_PCM_STREAM_CAPTURE, 0); */
  err = snd_pcm_open(pcm, "pcm.infile", SND_PCM_STREAM_CAPTURE, 0);
  if (err < 0)
  {
    printf("[!] snd_pcm_open(capture): %s\n", snd_strerror(err));
    return -1;
  }

  if (setup_common_dev(*pcm, nsampl))
  {
    snd_pcm_close(*pcm);
    *pcm = NULL;
    return -1;
  }
  
  return 0;
}

static int open_playback_dev
(snd_pcm_t** pcm, const char* name, unsigned int nsampl)
{
  int err;

  /* err = snd_pcm_open(pcm, name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK); */
  err = snd_pcm_open(pcm, name, SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0)
  {
    printf("[!] snd_pcm_open(playback): %s\n", snd_strerror(err));
    return -1;
  }

  snd_pcm_drain(*pcm);

  if (setup_common_dev(*pcm, nsampl))
  {
    snd_pcm_close(*pcm);
    *pcm = NULL;
    return -1;
  }

  return 0;
}

static inline void close_dev(snd_pcm_t* pcm)
{
  snd_pcm_hw_free(pcm);
  snd_pcm_close(pcm);
}

static int start_dev(snd_pcm_t* pcm)
{
#if 1 /* FIXME: automatically called */
  int err;

  if ((err = snd_pcm_prepare(pcm)))
  {
    printf("[!] snd_pcm_prepare(): %s\n", snd_strerror(err));
    return -1;
  }
#endif

  snd_pcm_start(pcm);

  return 0;
}

static unsigned int write_dev
(snd_pcm_t* pcm, const void* buf, unsigned int nsampl, unsigned int deadline_ms)
{
  /* return (uint)-1 on unrecoverable error */

  int err;

  snd_pcm_wait(pcm, deadline_ms);
 write_again:
  if ((err = snd_pcm_writei(pcm, buf, nsampl)) != nsampl)
  {
    if (err == -EAGAIN) { goto write_again; }

    if (err == -EPIPE)
    {
      /* an underrun occured, correct and rerun */
      /* printf("wEPIPE\n"); */
      snd_pcm_recover(pcm, err, 1);
      /* goto write_again; */
      return nsampl;
    }

    printf("[!] snd_pcm_writei(): %d, %s\n", err, snd_strerror(err));
    return -1;
  }

  return nsampl;
}


static unsigned int read_dev
(snd_pcm_t* pcm, const void* buf, unsigned int nsampl, unsigned int deadline_ms)
{
  int err;

  snd_pcm_wait(pcm, deadline_ms);
 read_again:
  if ((err = snd_pcm_readi(pcm, (void*)buf, nsampl)) < 0)
  {
    if (err == -EAGAIN) { goto read_again; }

    if (err == -EPIPE)
    {
      /* an underrun occured, correct and rerun */
      /* printf("rEPIPE\n"); */
      snd_pcm_recover(pcm, err, 1);
      goto read_again;
    }

    printf("[!] snd_pcm_readi(): %d %s\n", err, snd_strerror(err));
    return -1;
  }

  /* may be smaller than required */
  return (unsigned int)err;
}


/* sampling */

static void get_sampling_config(unsigned int* fband, unsigned int* nsampl)
{
  unsigned int log2_nsampl;

  *fband = CONFIG_FBAND;

  /* fband to nsampl */
  *nsampl = CONFIG_FSAMPL / *fband;

  /* fft works better with log2 sizes */
  log2_nsampl = (unsigned int)log2(*nsampl);
  if (*nsampl != (1 << log2_nsampl))
    *nsampl = 1 << (log2_nsampl + 1);

  /* always recompute band frequency (log2 or rounding) */
  *fband = CONFIG_FSAMPL / *nsampl;
}

static inline unsigned int nsampl_to_ms
(unsigned int nsampl, unsigned int fsampl)
{
  return (nsampl * 1000) / fsampl;
}


__attribute__((unused)) static unsigned int get_deadline_ms(snd_pcm_t* pcm)
{
  /* return the deadline, in milliseconds */

  /* note: same result achieved with nsampl_to_ms */

  snd_pcm_hw_params_t* parms;
  unsigned int us;
  int dir;

  snd_pcm_hw_params_alloca(&parms);
  snd_pcm_hw_params_any(pcm, parms);
  snd_pcm_hw_params_get_period_time(parms, &us, &dir);

  return us / 1000;
}


/* time conversion */

static inline unsigned int timeval_to_ms(const struct timeval* tm)
{
  return (unsigned int)(tm->tv_sec * 1000 + tm->tv_usec / 1000);
}


/* setup the process scheduling parameters */

static int setup_sched(void)
{
  static const int policy = SCHED_FIFO;
  const pid_t pid = getpid();
  struct sched_param parms;

  sched_getparam(pid, &parms);
  parms.sched_priority = sched_get_priority_max(policy);
  if (sched_setscheduler(pid, policy, &parms) < 0) return -1;
  return 0;
}


/* signal filtering */

typedef struct filter_data
{
  /* fftw data */
  fftw_plan plan;
  fftw_complex* ibuf;
  fftw_complex* obuf;

  /* fir data */
  double* fir_buf;

} filter_data_t;

static const double fir_coeffs[] =
{
  -0.131625603115805,
  0.103820148451119,
  0.096541374482813,
  0.102280926834003,
  0.113689768997426,
  0.125850624234278,
  0.135740574367069,
  0.141133806318268,
  0.141133806318268,
  0.135740574367069,
  0.125850624234278,
  0.113689768997426,
  0.102280926834003,
  0.096541374482813,
  0.103820148451119,
  -0.131625603115805
};

static int filter_init
(filter_data_t* data, unsigned int nsampl, unsigned int fband)
{
  data->plan = NULL;
  data->ibuf = NULL;
  data->obuf = NULL;
  data->fir_buf = NULL;

  if (ui_init(nsampl / 2, fband)) goto on_error_0;

  data->ibuf = fftw_malloc(nsampl * sizeof(fftw_complex));
  if (data->ibuf == NULL) goto on_error_0;

  data->obuf = fftw_malloc(nsampl * sizeof(fftw_complex));
  if (data->obuf == NULL) goto on_error_1;

  data->plan = fftw_plan_dft_1d
    (nsampl, data->ibuf, data->obuf, FFTW_FORWARD, FFTW_ESTIMATE);
  if (data->plan == NULL) goto on_error_2;

  const unsigned int nh = sizeof(fir_coeffs) / sizeof(fir_coeffs[0]);
  data->fir_buf = malloc((nsampl + nh) * sizeof(double));

  return 0;

 on_error_2:
  fftw_free(data->obuf);
 on_error_1:
  fftw_free(data->ibuf);
 on_error_0:
  return -1;
}

static void filter_fini(filter_data_t* data)
{
  if (data->plan) fftw_destroy_plan(data->plan);
  if (data->obuf) fftw_free(data->obuf);
  if (data->ibuf) fftw_free(data->ibuf);

  if (data->fir_buf) free(data->fir_buf);

  ui_fini();
}

static void do_power_spectrum
(filter_data_t* data, const int16_t* buf, unsigned int nsampl)
{
  /* power spectrum a stored in casted data->ibuf */
  double* const x = (double*)data->ibuf;
  const unsigned int nx = nsampl / 2;

  double sum;
  unsigned int i;

  /* convert int16 dual channel into double single channel */
  for (i = 0; i < nsampl; ++i)
  {
    data->ibuf[i][0] = ((double)buf[i * 2 + 0] + (double)buf[i * 2 + 1]) / 2;
    data->ibuf[i][1] = 0;
  }

  /* real to complex fast fourier transform */
  fftw_execute(data->plan);

  /* power spectrum */
  sum = 0;
  for (i = 0; i < nx; ++i)
  {
    const double re = data->obuf[i][0];
    const double im = data->obuf[i][1];

    x[i] = sqrt(re * re + im * im);
    sum += x[i];
    /* if (x[i] > max) max = x[i]; */
  }

#if 1
  /* x[i] is percent of total spectrum */
  for (i = 0; i < nx; ++i) x[i] /= sum;
#else
  /* normalize x[i] */
  if (max != 0) for (i = 0; i < nx; ++i) x[i] /= max;
#endif
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

static void do_fir(filter_data_t* data, int16_t* buf, unsigned int nsampl)
{
  static const unsigned int nh = sizeof(fir_coeffs) / sizeof(fir_coeffs[0]);

  double* const ibuf = (double*)data->ibuf;

  unsigned int i;

  /* convert int16 dual channel into double single channel */
  for (i = 0; i < nsampl; ++i)
    ibuf[i] = ((double)buf[i * 2 + 0] + (double)buf[i * 2 + 1]) / 2;

  convolve(data->fir_buf, nsampl + nh, ibuf, nsampl, fir_coeffs, nh);

  /* convert back to int16_t */
  for (i = 0; i < nsampl; ++i)
  {
    /* FIXME: double to int16 conversion, not a cast */
    const int16_t val = (int16_t)data->fir_buf[i];
    buf[i * 2 + 0] = val;
    buf[i * 2 + 1] = val;
  }
}

static void filter_apply
(filter_data_t* data, int16_t* buf, unsigned int nsampl)
{
#if 0 /* white noise */
  unsigned int i;
  for (i = 0; i < (nsampl * 2); ++i) buf[i] = (int16_t)rand();
#elif 0 /* amplifier effect */
  unsigned int i;
  for (i = 0; i < (nsampl * 2); ++i) buf[i] *= 4;
  /* for (i = 0; i < (nsampl * 2); ++i) buf[i] *= 1; */
#elif 1 /* fir */
  if (nsampl)
  {
    ui_update_begin();

    do_power_spectrum(data, buf, nsampl);
    ui_update_ips((double*)data->ibuf, nsampl / 2);

    do_fir(data, buf, nsampl);
    do_power_spectrum(data, buf, nsampl);
    ui_update_ops((double*)data->ibuf, nsampl / 2);

    ui_update_end();
  }
#else /* nop */
#endif
}


/* main */

int main(int ac, char** av)
{
  const char* const dev_name = ac > 1 ? av[1] : "";

  snd_pcm_t* idev = NULL;

#if CONFIG_ENABLE_PLAYBACK
  snd_pcm_t* odev = NULL;
#endif

  unsigned int fband;
  unsigned int nsampl;

  get_sampling_config(&fband, &nsampl);

  /* in bytes */
  const unsigned int buf_size = nsampl * CONFIG_NCHAN * sizeof(int16_t);

  /* buffer indices (read, transform, write) */
  unsigned int wbuf = 0;
  unsigned int tbuf = 1;
  unsigned int rbuf = 2;

  unsigned int actual_nsampl = 0;

  void* bufs[3] = { NULL, NULL, NULL };

#if CONFIG_TIMING_CONTROL

  struct timeval tm_ref;
  struct timeval tm_now;
  struct timeval tm_sub;

  unsigned int elpased_ms;

#endif /* CONFIG_TIMING_CONTROL */

  unsigned int deadline_ms;

  unsigned int iter = 0;

  unsigned int i;

  int err;

  filter_data_t filter_data;

  printf("nsampl == %u\n", nsampl);

  if (filter_init(&filter_data, nsampl, fband)) goto  on_error;

  if (setup_sched()) goto on_error;

  if (open_capture_dev(&idev, dev_name, nsampl)) goto on_error;

#if CONFIG_ENABLE_PLAYBACK
  if (open_playback_dev(&odev, dev_name, nsampl)) goto on_error;
#endif

  if (alloc_buf3(bufs, buf_size)) goto on_error;

  printf("buf_size: %u\n", buf_size);

  deadline_ms = nsampl_to_ms(nsampl, CONFIG_FSAMPL);
  printf("deadline: %u\n", deadline_ms);

  if (start_dev(idev)) goto on_error;

#if CONFIG_ENABLE_PLAYBACK
  if (start_dev(odev)) goto on_error;
#endif

#if 0
  snd_pcm_state_t state;
  snd_pcm_nonblock(idev, 0);
  snd_pcm_drop(idev);
  snd_pcm_wait(idev, -1);
 redo_state:
  usleep(1000000);
  state = snd_pcm_state(idev);
  printf("state %d\n", state);
  if (state != SND_PCM_STATE_PREPARED) goto redo_state;
  printf("ok\n");
  snd_pcm_nonblock(idev, 1);

  snd_pcm_nonblock(odev, 0);
  snd_pcm_drain(odev);
  snd_pcm_wait(odev, -1);
  snd_pcm_nonblock(odev, 1);
#endif

#if CONFIG_TIMING_CONTROL
  /* bootstrap timer */
  gettimeofday(&tm_ref, NULL);
#endif /* CONFIG_TIMING_CONTROL */

  while (1)
  {
    filter_apply(&filter_data, bufs[tbuf], actual_nsampl);

    /* write playback device */

#if CONFIG_ENABLE_PLAYBACK
    err = (int)write_dev(odev, bufs[wbuf], nsampl, deadline_ms);
    if (err == -1) break ;
#endif

    /* read capture device */

    actual_nsampl = read_dev(idev, bufs[rbuf], nsampl, deadline_ms);
    if (actual_nsampl == (unsigned int)-1) break ;

    if (actual_nsampl != nsampl)
    {
      /* this is needed since fft plan initialized with nsampl */
      for (i = actual_nsampl; i < nsampl; ++i)
      {
	((int16_t*)bufs[rbuf])[i * 2 + 0] = 0;
	((int16_t*)bufs[rbuf])[i * 2 + 1] = 0;
      }

      actual_nsampl = nsampl;
    }

    /* permute */

    wbuf = perm3(wbuf);
    tbuf = perm3(tbuf);
    rbuf = perm3(rbuf);

#if CONFIG_TIMING_CONTROL

    gettimeofday(&tm_now, NULL);
    timersub(&tm_now, &tm_ref, &tm_sub);
    elpased_ms = timeval_to_ms(&tm_sub);
    if (elpased_ms > deadline_ms)
    {
      /* printf("unmet deadline (%u) at %u\n", elpased_ms, iter); */
      /* break ; */
    }
    else if (deadline_ms > elpased_ms)
    {
      usleep((deadline_ms - elpased_ms) * 1000);
    }

    gettimeofday(&tm_ref, NULL);

#endif /* CONFIG_TIMING_CONTROL */

    ++iter;
  }

 on_error:
  if (idev) close_dev(idev);
#if CONFIG_ENABLE_PLAYBACK
  if (odev) close_dev(odev);
#endif
  free_buf3(bufs, buf_size);
  filter_fini(&filter_data);

  return 0;
}
