#define _GNU_SOURCE
#include <sys/mman.h>

#include <errno.h>
#include <limits.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;
extern StIfPrc_MemAdviseType __madv_type_map[MADV_SOFT_OFFLINE + 1];

int posix_madvise(void *addr, size_t len, int advice) {
  StStatus status;
  uint64_t vpn;
  uint64_t page_count;

  vpn = (uint64_t)addr / PAGE_SIZE;
  page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

  if (advice == MADV_DONTNEED)
    return 0;
  if (advice < 0 ||
      advice >= sizeof(__madv_type_map) / sizeof(__madv_type_map[0])) {
    return -EINVAL;
  }

  status = StIfPrc_AdviseMemory(__process_handle, vpn, page_count,
                                __madv_type_map[advice]);
  if (!CHECK_SUCCESS(status)) {
    return -1; // TODO: get proper errno
  }
  return 0;
}
