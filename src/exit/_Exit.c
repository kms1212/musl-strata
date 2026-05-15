#include <stdlib.h>
#include <stdint.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "libc.h"
#include "sidl/process.h"

extern StHandle __process_handle;

#define MAKE_PROCESS_EXIT_STATUS(exit_code) \
  MAKE_BASE_STATUS((uint8_t)(exit_code) != 0, STATUS_AREA_PROCESS_EXIT, \
                   (StStatus)(uint8_t)(exit_code))

_Noreturn void __st_Exit(StStatus status) {
  for (;;) {
    StIfPrc_Terminate(__process_handle, status);
  }
}

_Noreturn void _Exit(int ec) {
  __st_Exit(MAKE_PROCESS_EXIT_STATUS((uint8_t)ec));
}
