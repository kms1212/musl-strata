#include "pthread_impl.h"

#include "sidl/thread.h"

extern StHandle __main_thread_handle; // TODO: use current thread

int __set_thread_area(void *p) {
  StStatus status;

  status = StIfThr_SetTlsBase(__main_thread_handle, THREAD_REGISTERID_FS, p);
  if (!CHECK_SUCCESS(status)) {
    return -1;
  }
  return 0;
}
