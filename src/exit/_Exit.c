#include <stdlib.h>

#include <strata/handle.h>

#include "sidl/process.h"

extern StHandle __process_handle;

_Noreturn void _Exit(int ec) {
  for (;;) {
    StIfPrc_Terminate(__process_handle, ec);
  }
}
