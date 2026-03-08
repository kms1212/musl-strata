#include <sys/mman.h>

#include <limits.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

int msync(void *start, size_t len, int flags) {
  StStatus status;
  uint64_t vpn;
  uint64_t page_count;

  vpn = (uint64_t)start / PAGE_SIZE;
  page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

  if (flags & MS_ASYNC) {
    status = StIfPrc_ScheduleSyncFileMemory(__process_handle, vpn, page_count);
  } else if (flags & MS_SYNC) {
    status = StIfPrc_SyncFileMemory(__process_handle, vpn, page_count);
  } else if (flags & MS_INVALIDATE) {
    status = StIfPrc_InvalidateFileMemory(__process_handle, vpn, page_count);
  }
  if (!CHECK_SUCCESS(status)) {
    return -1;
  }
  return 0;
}
