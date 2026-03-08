#include <sys/mman.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

int mlockall(int flags) {
  StStatus status;
  StIfPrc_MemLockFlags ml_flags = 0;

  if (flags & MCL_CURRENT)
    ml_flags |= PROCESS_MEMLOCKFLAGS_CURRENT;
  if (flags & MCL_FUTURE)
    ml_flags |= PROCESS_MEMLOCKFLAGS_FUTURE;
  if (flags & MCL_ONFAULT)
    ml_flags |= PROCESS_MEMLOCKFLAGS_ONFAULT;

  status = StIfPrc_LockAllMemory(__process_handle, ml_flags);
  if (!CHECK_SUCCESS(status)) {
    return -1;
  }
  return 0;
}
