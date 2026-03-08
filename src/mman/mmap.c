#include <sys/mman.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

static void dummy(void) {}
weak_alias(dummy, __vm_wait);

#define UNIT SYSCALL_MMAP2_UNIT
#define OFF_MASK ((-0x2000ULL << (8 * sizeof(syscall_arg_t) - 1)) | (UNIT - 1))

void *__mmap(void *start, size_t len, int prot, int flags, int fd, off_t off) {
  StStatus status;
  uint64_t vpn;
  uint64_t page_count;

  vpn = (uint64_t)start / PAGE_SIZE;
  page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

  if (flags & MAP_ANON) {
    uint32_t memory_flags = 0;

    if (prot & PROT_READ)
      memory_flags |= PROCESS_MEMMAPFLAGS_READ;
    if (prot & PROT_WRITE)
      memory_flags |= PROCESS_MEMMAPFLAGS_WRITE;
    if (prot & PROT_EXEC)
      memory_flags |= PROCESS_MEMMAPFLAGS_EXECUTE;

    // TODO: handle more flags

    status =
        StIfPrc_MapMemory(__process_handle, page_count, memory_flags, &vpn);
    if (!CHECK_SUCCESS(status)) {
      errno = ENOMEM;
      return MAP_FAILED;
    }

    return (void *)(vpn * PAGE_SIZE);
  }

  errno = ENOSYS;
  return MAP_FAILED;
}

weak_alias(__mmap, mmap);
