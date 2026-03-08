#define _BSD_SOURCE
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

void *sbrk(intptr_t inc) {
  errno = ENOSYS;
  return NULL;
}
