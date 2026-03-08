#define _GNU_SOURCE
#include <sys/mman.h>

#include <errno.h>
#include <limits.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

StIfPrc_MemAdviseType __madv_type_map[MADV_SOFT_OFFLINE + 1] = {
    [POSIX_MADV_NORMAL] = PROCESS_MEMADVISETYPE_NORMAL,
    [POSIX_MADV_RANDOM] = PROCESS_MEMADVISETYPE_RANDOM,
    [POSIX_MADV_SEQUENTIAL] = PROCESS_MEMADVISETYPE_SEQUENTIAL,
    [POSIX_MADV_WILLNEED] = PROCESS_MEMADVISETYPE_WILLNEED,
    [POSIX_MADV_DONTNEED] = PROCESS_MEMADVISETYPE_DONTNEED,
    [MADV_FREE] = PROCESS_MEMADVISETYPE_FREE,
    [MADV_REMOVE] = PROCESS_MEMADVISETYPE_REMOVE,
    [MADV_DONTFORK] = PROCESS_MEMADVISETYPE_DONTFORK,
    [MADV_DOFORK] = PROCESS_MEMADVISETYPE_DOFORK,
    [MADV_MERGEABLE] = PROCESS_MEMADVISETYPE_MERGEABLE,
    [MADV_UNMERGEABLE] = PROCESS_MEMADVISETYPE_UNMERGEABLE,
    [MADV_HUGEPAGE] = PROCESS_MEMADVISETYPE_HUGEPAGE,
    [MADV_NOHUGEPAGE] = PROCESS_MEMADVISETYPE_NOHUGEPAGE,
    [MADV_DONTDUMP] = PROCESS_MEMADVISETYPE_DONTDUMP,
    [MADV_DODUMP] = PROCESS_MEMADVISETYPE_DODUMP,
    [MADV_WIPEONFORK] = PROCESS_MEMADVISETYPE_WIPEONFORK,
    [MADV_KEEPONFORK] = PROCESS_MEMADVISETYPE_KEEPONFORK,
    [MADV_COLD] = PROCESS_MEMADVISETYPE_COLD,
    [MADV_PAGEOUT] = PROCESS_MEMADVISETYPE_PAGEOUT,
    [MADV_HWPOISON] = PROCESS_MEMADVISETYPE_HWPOISON,
    [MADV_SOFT_OFFLINE] = PROCESS_MEMADVISETYPE_SOFT_OFFLINE,
};

int __madvise(void *addr, size_t len, int advice) {
  StStatus status;
  uint64_t vpn;
  uint64_t page_count;

  vpn = (uint64_t)addr / PAGE_SIZE;
  page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

  if (advice < 0 ||
      advice >= sizeof(__madv_type_map) / sizeof(__madv_type_map[0])) {
    return EINVAL;
  }

  status = StIfPrc_AdviseMemory(__process_handle, vpn, page_count,
                                __madv_type_map[advice]);
  if (!CHECK_SUCCESS(status)) {
    return 1; // TODO: get proper errno
  }
  return 0;
}

weak_alias(__madvise, madvise);
