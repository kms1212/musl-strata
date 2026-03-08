#include <sys/mman.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "sidl/process.h"

extern StHandle __process_handle;

int munlockall(void) {
  StStatus status;

  status = StIfPrc_UnlockAllMemory(__process_handle);
  if (!CHECK_SUCCESS(status)) {
    return -1;
  }
  return 0;
}
