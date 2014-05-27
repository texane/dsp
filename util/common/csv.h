#ifndef CSV_H_INCLUDED
#define CSV_H_INCLUDED


#include <sys/types.h>


typedef struct csv_handle
{
  size_t nline;
  size_t ncol;
  double** cols;
} csv_handle_t;


int csv_load_file(csv_handle_t*, const char*);
int csv_close(csv_handle_t*);
int csv_get_col(csv_handle_t*, size_t, double**, size_t*);


#endif /* CSV_H_INCLUDED */
