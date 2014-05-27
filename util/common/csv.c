#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "csv.h"


#ifdef CSV_CONFIG_DEBUG
#define CSV_PERROR() \
do { printf("%s %u\n", __FILE__, __LINE__); fflush(stdout); } while (0)
#else
#define CSV_PERROR()
#endif /* CSV_CONFIG_DEBUG */


typedef struct mapped_file
{
  uint8_t* base;
  size_t off;
  size_t len;
} mapped_file_t;

typedef mapped_file_t mapped_line_t;

static int map_file(mapped_file_t* mf, const char* path)
{
  int error = -1;
  struct stat st;
  int fd;

  fd = open(path, O_RDONLY);
  if (fd == -1) return -1;

  if (fstat(fd, &st) == -1) goto on_error;

  mf->base = (uint8_t*)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (mf->base == MAP_FAILED) goto on_error;

  mf->off = 0;
  mf->len = st.st_size;

  /* success */
  error = 0;

 on_error:
  close(fd);

  return error;
}

static void unmap_file(mapped_file_t* mf)
{
  munmap((void*)mf->base, mf->len);
  mf->base = (uint8_t*)MAP_FAILED;
  mf->len = 0;
}

static int next_line(mapped_file_t* mf, mapped_line_t* ml)
{
  const unsigned char* end = mf->base + mf->len;
  const unsigned char* p;
  size_t skipnl = 0;

  ml->off = 0;
  ml->len = 0;
  ml->base = mf->base + mf->off;

  for (p = ml->base; p != end; ++p, ++ml->len)
  {
    if (*p == '\n')
    {
      skipnl = 1;
      break;
    }
  }

  if ((p + skipnl) == ml->base) return -1;

  /* update offset */
  mf->off += (p - ml->base) + skipnl;

  return 0;
}

static int next_data_line(mapped_file_t* mf, mapped_line_t* ml)
{
  /* next line containing data */

  while (next_line(mf, ml) == 0)
  {
    /* skip empty line */
    if (ml->len == 0) continue ;

    /* skip comment line */
    if (ml->base[0] == '#') continue ;

    return 0;
  }

  return -1;
}

static int next_col(mapped_line_t* ml)
{
  if (ml->off == ml->len) return -1;

  for (; ml->off < ml->len; ++ml->off)
  {
    if (ml->base[ml->off] == ' ') break;
  }

  /* skip comma */
  if (ml->off != ml->len) ++ml->off;

  return 0;
}

static size_t get_col_count(mapped_line_t* ml)
{
  size_t col_count;
  for (col_count = 0; next_col(ml) != -1; ++col_count) ;
  return col_count;
}

static int next_value(mapped_file_t* mf, double* x)
{
  char* endptr;

  if (mf->off >= mf->len) return -1;

  *x = strtod((char*)mf->base + mf->off, &endptr);

  /* endptr points to the next caracter */
  mf->off = endptr - (char*)mf->base + 1;

  return 0;
}


/* exported */

int csv_load_file(csv_handle_t* csv, const char* path)
{
  mapped_file_t mf;
  mapped_file_t saved_mf;
  mapped_line_t ml;
  int err = -1;
  size_t cpos;
  size_t lpos;

  if (map_file(&mf, path))
  {
    CSV_PERROR();
    goto on_error_0;
  }

  saved_mf = mf;

  csv->nline = 0;
  csv->ncol = 0;
  csv->cols = NULL;

  /* get column count */

  if (next_data_line(&mf, &ml))
  {
    CSV_PERROR();
    goto on_error_1;
  }

  csv->ncol = get_col_count(&ml);
  if (csv->ncol == 0) goto on_error_1;

  /* get line count */

  for (csv->nline = 1; next_data_line(&mf, &ml) == 0; ++csv->nline) ;

  csv->cols = malloc(csv->nline * csv->ncol * sizeof(double));
  if (csv->cols == NULL)
  {
    CSV_PERROR();
    goto on_error_1;
  }

  /* get values */

  mf = saved_mf;
  for (lpos = 0; next_data_line(&mf, &ml) == 0; ++lpos)
  {
    for (cpos = 0; cpos != csv->ncol; ++cpos)
    {
      double* const x = csv->cols + cpos * csv->nline + lpos;
      if (next_value(&ml, x))
      {
	CSV_PERROR();
	goto on_error_2;
      }
    }
  }

  /* success */

  err = 0;

 on_error_2:
  if (err) free(csv->cols);
 on_error_1:
  unmap_file(&mf);
 on_error_0:
  return err;
}

int csv_close(csv_handle_t* csv)
{
  /* may be null if no lines */
  if (csv->cols != NULL) free(csv->cols);
  return 0;
}

int csv_get_col(csv_handle_t* csv, size_t i, double** x, size_t* n)
{
  /* i the column index. assumed correct. */
  /* x the value array */
  /* n the array size */

  *x = csv->cols + i * csv->nline;
  *n = csv->nline;

  return 0;
}


#if CSV_CONFIG_UNIT /* unit */

int main(int ac, char** av)
{
  const char* const filename = "./fu.csv";
  csv_handle_t csv;
  double* x[2];
  size_t n;
  size_t i;

  csv_load_file(&csv, filename);

  printf("nline == %zu, ncol == %zu\n", csv.nline, csv.ncol);

  csv_get_col(&csv, 1, &x[0], &n);
  csv_get_col(&csv, 2, &x[1], &n);

  for (i = 0; i != n; ++i) printf("%lf %lf\n", x[0][i], x[1][i]);

  csv_close(&csv);

  return 0;
}

#endif /* CSV_CONFIG_UNIT */
