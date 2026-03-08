#include <sys/mman.h>

#include <limits.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

static void dummy(void) {}
weak_alias(dummy, __vm_wait);

int __munmap(void *start, size_t len) {
  StStatus status;
  uint64_t vpn;
  uint64_t page_count;

  vpn = (uint64_t)start / PAGE_SIZE;
  page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

  __vm_wait();

  status = StIfPrc_UnmapMemory(__process_handle, vpn, page_count);
  if (!CHECK_SUCCESS(status)) {
    return -1;
  }
  return 0;
}

weak_alias(__munmap, munmap);
