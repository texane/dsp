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


#define PERROR() printf("%s %u\n", __FILE__, __LINE__)


typedef struct linked_list
{
  struct linked_list* next;
} linked_list_t;

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

  if (p == ml->base) return -1;

  /* update offset */
  mf->off += (p - ml->base) + skipnl;

  return 0;
}

static int next_col(mapped_line_t* ml, uint8_t* buf)
{
  if (ml->off == ml->len) return -1;

  for (; ml->off < ml->len; ++ml->off, ++buf)
  {
    if (ml->base[ml->off] == ' ') break;
    *buf = ml->base[ml->off];
  }

  *buf = 0;

  /* skip comma */
  if (ml->off != ml->len) ++ml->off;

  return 0;
}

__attribute__((unused))
static size_t get_col_count(mapped_file_t* mf)
{
  mapped_file_t tmp_mf = *mf;
  mapped_line_t ml;
  unsigned char buf[256];
  size_t col_count;

  if (next_line(&tmp_mf, &ml) == -1) return 0;
  for (col_count = 0; next_col(&ml, buf) != -1; ++col_count) ;
  return col_count;
}

static inline bool is_digit(unsigned char c)
{
  return (c >= '0' && c <= '9') || (c == '.') || (c == '-');
}

__attribute__((unused))
static void skip_first_line(mapped_file_t* mf)
{
  mapped_file_t tmp_mf = *mf;
  mapped_line_t ml;

  if (next_line(&tmp_mf, &ml) == -1) return ;

  unsigned char buf[256];
  if (next_col(&ml, buf) == -1) return ;

  for (size_t i = 0; buf[i]; ++i)
  {
    // skip the line if non digit token found
    if (is_digit(buf[i]) == false)
    {
      *mf = tmp_mf;
      break;
    }
  }
}

static int next_value(mapped_file_t& mf, double* v)
{
  char* endptr;

  if (mf.off >= mf.len) return -1;

  *v = strtod((char*)mf.base + mf.off, &endptr);

  /* endptr points to the next caracter */
  mf.off = endptr - (char*)mf.base + 1;

  return 0;
}

int table_read_csv_file
(table& table, const char* path, const vector<table::type_id>& tids)
{
  // table_create not assumed

  int error = -1;

  if (table_create(table) == -1) return -1;

  table.tids = tids;

  mapped_file_t mf;
  if (map_file(&mf, path) == -1) return -1;

  // get the column count
  table.col_count = tids.size();

  vector<table::data_type> row;
  row.resize(table.col_count);

  table::data_type value;
  while (1)
  {
    // no more value
    if (next_value(mf, value, tids[0]) == -1) break ;

    size_t col_pos = 0;
    goto add_first_value;
    for (; col_pos < table.col_count; ++col_pos)
    {
      if (next_value(mf, value, tids[col_pos]) == -1) break ;
    add_first_value:
      row[col_pos] = value;
    }

    if (col_pos != table.col_count)
    {
      // bug: if the end of file terminates with '\n'
      goto on_error;
    }

    table.rows.push_back(row);
    ++table.row_count;
  }

  // success
  error = 0;

 on_error:
  unmap_file(&mf);
  return error;
}


/* exported */

int csv_load_file(csv_handle_t* csv, const char* path)
{
  mapped_file_t mf;
  mapped_line_t ml;
  int err = -1;

  if (map_file(&mf, path))
  {
    PERROR();
    goto on_error_0;
  }

  csv->nline = 0;
  csv->ncol = 0;
  csv->cols = NULL;

  while (1)
  {
    /* empty file */
    if (next_line(&mf, &ml) == -1) break ;

    /* comment */
    if (ml->base[0] == '#') continue ;
  }

  /* guess delimiter */

  err = 0;

 on_error_2:
  free(csv->cols);
 on_error_1:
  unmap_file(&mf);
 on_error_0:
  return err;
}

int csv_close(csv_handle_t* csv)
{
  free(csv->cols);
  return 0;
}

int csv_get_col(csv_handle_t* csv, size_t i, double** x, size_t* n)
{
  /* i the column index. assumed correct. */
  /* x the value array */
  /* n the array size */

  *x = csv->cols[i];
  *n = csv->nlines;

  return 0;
}


/* unit */

int main(int ac, char** av)
{
  const char* filename = av[1];
  const size_t col = atoi(av[2]);
  double* x;
  size_t n;
  size_t i;

  if (csv_read_col(filename, col, &x, &n))
  {
    return -1;
  }

  for (i = 0; i != n; ++i)
  {
  }

  return 0;
}
