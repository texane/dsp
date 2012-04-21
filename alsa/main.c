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

  if ((err = snd_pcm_nonblock(pcm, 1)))
  {
    printf("[!] snd_pcm_nonblock: %s\n", snd_strerror(err));
    goto on_error;
  }

 on_error:
  snd_pcm_hw_params_free(parms);

  return err;
} 

static int open_capture_dev(snd_pcm_t** pcm, const char* name)
{
  int err;

  err = snd_pcm_open(pcm, name, SND_PCM_STREAM_CAPTURE, 0);
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

  err = snd_pcm_open(pcm, name, SND_PCM_STREAM_PLAYBACK, 0);
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
  snd_pcm_close(pcm);
}

static int start_dev(snd_pcm_t* pcm)
{
  int err;

  if ((err = snd_pcm_prepare(pcm)))
  {
    printf("[!] snd_pcm_prepare(): %s\n", snd_strerror(err));
    return -1;
  }

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


/* setup the task scheduling parameters */

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


static void trans(int16_t* buf, unsigned int nsampl)
{
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

  void* bufs[3] = { NULL, NULL, NULL };

  struct timeval tm_ref;
  struct timeval tm_now;
  struct timeval tm_sub;

  unsigned int deadline_ms;

  unsigned int iter = 0;

  int err;

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
    ++iter;

    if ((err = snd_pcm_readi(idev, bufs[rbuf], nsampl)) != nsampl)
    {
      printf("[!] snd_pcm_readi(): %d\n", err);
      break ;
    }

    if ((err = snd_pcm_writei(odev, bufs[wbuf], nsampl)) != nsampl)
    {
      printf("[!] snd_pcm_writei(): %d\n", err);
      break ;
    }

    trans(bufs[tbuf], nsampl);

    wbuf = perm3(wbuf);
    tbuf = perm3(tbuf);
    rbuf = perm3(rbuf);

    gettimeofday(&tm_now, NULL);

    timersub(&tm_now, &tm_ref, &tm_sub);
    if (timeval_to_ms(&tm_sub) > deadline_ms)
    {
      printf("unmet deadline (%u) at %u\n", timeval_to_ms(&tm_sub), iter);
      break ;
    }

    tm_ref = tm_now;
    ++iter;
  }

 on_error:
  if (idev) close_dev(idev);
  if (odev) close_dev(odev);
  free_buf3(bufs, buf_size);

  return 0;
}
