#define _GNU_SOURCE
#include <sys/mman.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

static void dummy(void) {}
weak_alias(dummy, __vm_wait);

void *__mremap(void *old_addr, size_t old_len, size_t new_len, int flags, ...) {
  StStatus status;

  va_list ap;
  void *new_addr = 0;
  uint64_t vpn;
  uint64_t old_page_count;
  uint64_t new_page_count;

  if (new_len >= PTRDIFF_MAX) {
    errno = ENOMEM;
    return MAP_FAILED;
  }

  if (flags & MREMAP_FIXED) {
    __vm_wait();
    va_start(ap, flags);
    new_addr = va_arg(ap, void *);
    va_end(ap);
  }

  vpn = (uint64_t)old_addr / PAGE_SIZE;
  old_page_count = (old_len + PAGE_SIZE - 1) / PAGE_SIZE;
  new_page_count = (new_len + PAGE_SIZE - 1) / PAGE_SIZE;

  if (flags & MAP_ANON) {
    uint32_t memory_flags = PROCESS_MEMREMAPFLAGS_PRESERVE_PROTECTIONS;

    // TODO: handle more flags

    status = StIfPrc_RemapMemory(__process_handle, old_page_count,
                                 new_page_count, memory_flags, &vpn);
    if (!CHECK_SUCCESS(status)) {
      errno = ENOMEM;
      return MAP_FAILED;
    }

    return (void *)(vpn * PAGE_SIZE);
  }

  errno = ENOSYS;
  return MAP_FAILED;
}

weak_alias(__mremap, mremap);
