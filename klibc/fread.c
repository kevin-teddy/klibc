/*
 * fread.c
 */

#include <errno.h>
#include <unistd.h>
#include <stdio.h>

size_t __fread(void *buf, size_t count, FILE *f)
{
  size_t bytes = 0;
  ssize_t rv;
  char *p = buf;

  while ( count ) {
    rv = read(fileno(f), p, count);
    if ( rv == -1 ) {
      if ( errno == EINTR )
	continue;
      else
	return bytes ? bytes : -1;
    } else if ( rv == 0 ) {
      break;
    }

    p += rv;
    bytes += rv;
    count -= rv;
  }

  return bytes;
}

    
      