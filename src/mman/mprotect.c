#include <sys/mman.h>

#include <errno.h>
#include "libc.h"
#include <limits.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

int __mprotect(void *addr, size_t len, int prot) {
  StStatus status;
  StIfPrc_MemRemapFlags remap_flags = 0;
  uint64_t vpn;
  uint64_t page_count;

  vpn = (uint64_t)addr / PAGE_SIZE;
  page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

  if (prot & PROT_READ)
    remap_flags |= PROCESS_MEMREMAPFLAGS_READ;
  if (prot & PROT_WRITE)
    remap_flags |= PROCESS_MEMREMAPFLAGS_WRITE;
  if (prot & PROT_EXEC)
    remap_flags |= PROCESS_MEMREMAPFLAGS_EXECUTE;
  if (prot & PROT_GROWSDOWN)
    remap_flags |= PROCESS_MEMREMAPFLAGS_GROWSDOWN;
  if (prot & PROT_GROWSUP)
    remap_flags |= PROCESS_MEMREMAPFLAGS_GROWSUP;

  status = StIfPrc_RemapMemory(__process_handle, page_count, page_count,
                               remap_flags, &vpn);
  if (!CHECK_SUCCESS(status)) {
    errno = status == STATUS_NOT_SUPPORTED ? ENOSYS : ENOMEM;
    return -1;
  }
  return 0;
}

weak_alias(__mprotect, mprotect);
