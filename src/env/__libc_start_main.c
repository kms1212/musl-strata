#include "atomic.h"
#include "libc.h"
#include <elf.h>
#include <signal.h>
#include <unistd.h>

#include <strata/handle.h>

#include "sidl/process.h"
#include "strata_fd.h"

static void dummy(void) {}
weak_alias(dummy, _init);

extern weak hidden void (*const __init_array_start)(void),
    (*const __init_array_end)(void);

static void dummy1(void *p) {}
weak_alias(dummy1, __init_ssp);

#define AUX_CNT 38

StHandle __process_handle;
StHandle __main_thread_handle;
StHandle __stdin_handle;
StHandle __stdout_handle;
StHandle __stderr_handle;

extern StStatus __StHandle_Init(void *krt_entries);

void __init_libc_handles(void) {
  StStatus status;

  status = __StHandle_Init((void *)__sysinfo);
  if (!CHECK_SUCCESS(status)) {
    __process_handle = STHANDLE_INVALID;
    __main_thread_handle = STHANDLE_INVALID;
    __stdin_handle = STHANDLE_INVALID;
    __stdout_handle = STHANDLE_INVALID;
    __stderr_handle = STHANDLE_INVALID;
    return;
  }

  status = StHandle_Open((const uint8_t *)"/System/Processes/Current", 0,
                         &__process_handle);
  if (!CHECK_SUCCESS(status)) {
    __process_handle = STHANDLE_INVALID;
  }

  status =
      StHandle_Open((const uint8_t *)"/System/Processes/Current/Threads/Main",
                    0, &__main_thread_handle);
  if (!CHECK_SUCCESS(status)) {
    __main_thread_handle = STHANDLE_INVALID;
  }

  status = StHandle_Open((const uint8_t *)"/System/Processes/Current/Stdin", 0,
                         &__stdin_handle);
  if (!CHECK_SUCCESS(status)) {
    __stdin_handle = STHANDLE_INVALID;
  }

  status = StHandle_Open((const uint8_t *)"/System/Processes/Current/Stdout", 0,
                         &__stdout_handle);
  if (!CHECK_SUCCESS(status)) {
    __stdout_handle = STHANDLE_INVALID;
  }

  status = StHandle_Open((const uint8_t *)"/System/Processes/Current/Stderr", 0,
                         &__stderr_handle);
  if (!CHECK_SUCCESS(status)) {
    __stderr_handle = STHANDLE_INVALID;
  }
}

#ifdef __GNUC__
__attribute__((__noinline__))
#endif
void __init_libc(char **envp, char *pn) {
  size_t i, *auxv, aux[AUX_CNT] = {0};
  __environ = envp;
  for (i = 0; envp[i]; i++)
    ;
  libc.auxv = auxv = (void *)(envp + i + 1);
  for (i = 0; auxv[i]; i += 2)
    if (auxv[i] < AUX_CNT)
      aux[auxv[i]] = auxv[i + 1];
  __hwcap = aux[AT_HWCAP];
  if (aux[AT_SYSINFO])
    __sysinfo = aux[AT_SYSINFO];
  libc.page_size = aux[AT_PAGESZ];

  if (!pn)
    pn = (void *)aux[AT_EXECFN];
  if (!pn)
    pn = "";
  __progname = __progname_full = pn;
  for (i = 0; pn[i]; i++)
    if (pn[i] == '/')
      __progname = pn + i + 1;

  __init_libc_handles();
  __strata_fd_init();
  __init_tls(aux);
  __init_ssp((void *)aux[AT_RANDOM]);

  if (aux[AT_UID] == aux[AT_EUID] && aux[AT_GID] == aux[AT_EGID] &&
      !aux[AT_SECURE])
    return;

  libc.secure = 1;
}

static void libc_start_init(void) {
  _init();
  uintptr_t a = (uintptr_t)&__init_array_start;
  for (; a < (uintptr_t)&__init_array_end; a += sizeof(void (*)()))
    (*(void (**)(void))a)();
}

weak_alias(libc_start_init, __libc_start_init);

typedef int lsm2_fn(int (*)(int, char **, char **), int, char **);
static lsm2_fn libc_start_main_stage2;

int __libc_start_main(int (*main)(int, char **, char **), int argc, char **argv,
                      void (*init_dummy)(), void (*fini_dummy)(),
                      void (*ldso_dummy)()) {
  char **envp = argv + argc + 1;

  /* External linkage, and explicit noinline attribute if available,
   * are used to prevent the stack frame used during init from
   * persisting for the entire process lifetime. */
  __init_libc(envp, argv[0]);

  /* Barrier against hoisting application code or anything using ssp
   * or thread pointer prior to its initialization above. */
  lsm2_fn *stage2 = libc_start_main_stage2;
  __asm__("" : "+r"(stage2) : : "memory");
  return stage2(main, argc, argv);
}

static int libc_start_main_stage2(int (*main)(int, char **, char **), int argc,
                                  char **argv) {
  char **envp = argv + argc + 1;
  __libc_start_init();

  /* Pass control to the application */
  exit(main(argc, argv, envp));
  return 0;
}
