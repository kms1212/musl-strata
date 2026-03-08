#define _GNU_SOURCE
#include <sys/mman.h>

#include <limits.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

int mincore(void *addr, size_t len, unsigned char *vec) {
  StStatus status;

  uint64_t vpn;
  uint64_t page_count;

  vpn = (uint64_t)addr / PAGE_SIZE;
  page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

  status = StIfPrc_CheckMemMapStatus(__process_handle, vpn, page_count,
                                     (uint8_t *)vec);
  if (!CHECK_SUCCESS(status)) {
    return -1;
  }
  return 0;
}
