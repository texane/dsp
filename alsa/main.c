/* reference: http://equalarea.com/paul/alsa-audio.html */


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


/* static configuration */

#define CONFIG_FSAMPL 44100
#define CONFIG_FBAND 50
#define CONFIG_NCHAN 2


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

static int setup_common_dev(snd_pcm_t* pcm)
{
  /* 44100 hz sampling rate, 2 channels, int16_t samples */

  snd_pcm_hw_params_t* parms;
  int err;

  err = snd_pcm_hw_params_malloc(&parms);
  if (err)
  {
    printf("[!] snd_pcm_hw_params_malloc(): %s\n", snd_strerror(err));
    return -1;
  }

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

  if ((err = snd_pcm_hw_params(pcm, parms)))
  {
    printf("[!] snd_pcm_hw_params: %s\n", snd_strerror(err));
    goto on_error;
  }

 on_error:
  /* FIXME: is this a leak not to free here */
  if (err) snd_pcm_hw_params_free(parms);

  return err;
} 

static int open_capture_dev(snd_pcm_t** pcm, const char* name)
{
  int err;

  err = snd_pcm_open(pcm, name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
  if (err < 0)
  {
    printf("[!] snd_pcm_open(capture): %s\n", snd_strerror(err));
    return -1;
  }

  if (setup_common_dev(*pcm))
  {
    snd_pcm_close(*pcm);
    *pcm = NULL;
    return -1;
  }
  
  return 0;
}

static int open_playback_dev(snd_pcm_t** pcm, const char* name)
{
  int err;

  err = snd_pcm_open(pcm, name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
  if (err < 0)
  {
    printf("[!] snd_pcm_open(playback): %s\n", snd_strerror(err));
    return -1;
  }

  if (setup_common_dev(*pcm))
  {
    snd_pcm_close(*pcm);
    *pcm = NULL;
    return -1;
  }

  return 0;
}

static inline void close_dev(snd_pcm_t* pcm)
{
  /* FIXME: does it free hw_params */
  snd_pcm_hw_free(pcm);
  snd_pcm_close(pcm);
}

static int start_dev(snd_pcm_t* pcm)
{
#if 0 /* FIXME: automatically called */
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


/* sampling */

static inline unsigned int fband_to_nsampl
(unsigned int fband, unsigned int fsampl)
{
  return 1 + fsampl / fband;
}

static unsigned int get_nsampl(void)
{
  unsigned int log2_nsampl;
  unsigned int nsampl;

  /* compute nsampl according to fband. adjust to be pow2 */
  nsampl = fband_to_nsampl(CONFIG_FBAND, CONFIG_FSAMPL);
  log2_nsampl = (unsigned int)log2(nsampl);
  if (nsampl != (1 << log2_nsampl)) nsampl = 1 << (log2_nsampl + 1);

  return nsampl;
}

static inline unsigned int nsampl_to_ms
(unsigned int nsampl, unsigned int fsampl)
{
  return nsampl * 1000 / fsampl;
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
  double* ibuf;
  fftw_complex* obuf;
  
} filter_data_t;


static int filter_init(filter_data_t* data, unsigned int nsampl)
{
  data->plan = NULL;
  data->ibuf = NULL;
  data->obuf = NULL;

  data->ibuf = fftw_malloc(nsampl * sizeof(double));
  if (data->ibuf == NULL) goto on_error_0;

  data->obuf = fftw_malloc((nsampl / 2 + 1) * sizeof(fftw_complex));
  if (data->obuf == NULL) goto on_error_1;

  data->plan = fftw_plan_dft_r2c_1d
    (nsampl, data->ibuf, data->obuf, FFTW_ESTIMATE);
  if (data->plan == NULL) goto on_error_2;

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
}

static void do_power_spectrum
(filter_data_t* data, const int16_t* buf, unsigned int nsampl)
{
  double sum;
  unsigned int i;

  /* convert int16 dual channel into double single channel */
  for (i = 0; i < nsampl; ++i)
    data->ibuf[i] = ((double)buf[i * 2 + 0] + (double)buf[i * 2 + 1]) / 2;

  /* real to complex fast fourier transform */
  fftw_execute(data->plan);

  /* power spectrum (ie. polar magnitude) */
  sum = 0;
  for (i = 0; i < nsampl / 2 + 1; ++i)
  {
    const double re = data->obuf[i * 2][0];
    const double im = data->obuf[i * 2][1];
    const double p = sqrt(re * re + im * im);
    data->ibuf[i] = p;
    sum += p;
  }

  /* normalize (percent of) */
  if (sum > 0.00001) for (i = 0; i < nsampl / 2 + 1; ++i) data->ibuf[i] /= sum;
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
#elif 1 /* power spectrum */
  do_power_spectrum(data, buf, nsampl);
#else /* nop */
#endif
}


/* main */

int main(int ac, char** av)
{
  const char* const dev_name = ac > 1 ? av[1] : "";

  snd_pcm_t* idev = NULL;
  snd_pcm_t* odev = NULL;

  const unsigned int nsampl = get_nsampl();

  /* in bytes */
  const unsigned int buf_size = nsampl * CONFIG_NCHAN * sizeof(int16_t);

  /* buffer indices (read, transform, write) */
  unsigned int wbuf = 0;
  unsigned int tbuf = 1;
  unsigned int rbuf = 2;

  unsigned int actual_nsampl = 0;

  void* bufs[3] = { NULL, NULL, NULL };

  struct timeval tm_ref;
  struct timeval tm_now;
  struct timeval tm_sub;

  unsigned int elpased_ms;
  unsigned int deadline_ms;

  unsigned int iter = 0;

  int err;

  filter_data_t filter_data;

  if (filter_init(&filter_data, nsampl)) goto  on_error;

  if (setup_sched()) goto on_error;

  if (open_capture_dev(&idev, dev_name)) goto on_error;
  if (open_playback_dev(&odev, dev_name)) goto on_error;

  if (alloc_buf3(bufs, buf_size)) goto on_error;

  printf("buf_size: %u\n", buf_size);

  deadline_ms = nsampl_to_ms(nsampl, CONFIG_FSAMPL);
  printf("deadline: %u\n", deadline_ms);

  if (start_dev(idev)) goto on_error;
  if (start_dev(odev)) goto on_error;

  /* bootstrap timer */
  gettimeofday(&tm_ref, NULL);

  while (1)
  {
    filter_apply(&filter_data, bufs[tbuf], actual_nsampl);

    snd_pcm_wait(idev, deadline_ms);
  read_again:
    if ((err = snd_pcm_readi(idev, bufs[rbuf], nsampl)) < 0)
    {
      if (err == -EAGAIN) { printf("rEAGAIN\n"); goto read_again; }

      printf("[!] snd_pcm_readi(): %d\n", err);
      printf("[!] snd_pcm_readi(): %s\n", snd_strerror(err));
      break ;
    }
    else if (err != nsampl)
    {
      printf("read error == %d\n", err);
      /* goto on_read_error; */
    }

    /* may be smaller than required */
    actual_nsampl = (unsigned int)err;

    snd_pcm_wait(odev, deadline_ms);
  write_again:
    if ((err = snd_pcm_writei(odev, bufs[wbuf], nsampl)) != nsampl)
    {
      if (err == -EAGAIN) { printf("wEAGAIN\n"); goto write_again; }

      if (err == -EPIPE)
      {
	/* an underrun occured, correct and rerun */
	printf("wEPIPE\n");
	snd_pcm_recover(odev, err, 1);
	goto write_again;
      }

    on_write_error:
      printf("[!] snd_pcm_writei(): %d\n", err);
      printf("[!] snd_pcm_writei(): %s\n", snd_strerror(err));
      break ;
    }
    else if (err != nsampl)
    {
      printf("write error == %d\n", err);
      goto on_write_error;
    }

    wbuf = perm3(wbuf);
    tbuf = perm3(tbuf);
    rbuf = perm3(rbuf);

    gettimeofday(&tm_now, NULL);
    timersub(&tm_now, &tm_ref, &tm_sub);
    elpased_ms = timeval_to_ms(&tm_sub);
    if (elpased_ms > deadline_ms)
    {
      printf("unmet deadline (%u) at %u\n", elpased_ms, iter);
      break ;
    }

    printf("%u\n", elpased_ms);

    usleep((deadline_ms - elpased_ms) * 1000);

    gettimeofday(&tm_ref, NULL);

    ++iter;
  }

 on_error:
  if (idev) close_dev(idev);
  if (odev) close_dev(odev);
  free_buf3(bufs, buf_size);
  filter_fini(&filter_data);

  return 0;
}
