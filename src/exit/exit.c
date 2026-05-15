#include <stdlib.h>
#include <stdint.h>
#include "libc.h"

static void dummy()
{
}

/* atexit.c and __stdio_exit.c override these. the latter is linked
 * as a consequence of linking either __toread.c or __towrite.c. */
weak_alias(dummy, __funcs_on_exit);
weak_alias(dummy, __stdio_exit);
weak_alias(dummy, _fini);

extern weak hidden void (*const __fini_array_start)(void), (*const __fini_array_end)(void);

#define MAKE_PROCESS_EXIT_STATUS(exit_code) \
	MAKE_BASE_STATUS((uint8_t)(exit_code) != 0, STATUS_AREA_PROCESS_EXIT, \
	                 (StStatus)(uint8_t)(exit_code))

static void libc_exit_fini(void)
{
	uintptr_t a = (uintptr_t)&__fini_array_end;
	for (; a>(uintptr_t)&__fini_array_start; a-=sizeof(void(*)()))
		(*(void (**)())(a-sizeof(void(*)())))();
	_fini();
}

weak_alias(libc_exit_fini, __libc_exit_fini);

_Noreturn void exit(int code)
{
	__st_exit(MAKE_PROCESS_EXIT_STATUS((uint8_t)code));
}

_Noreturn void __st_exit(StStatus status)
{
	__funcs_on_exit();
	__libc_exit_fini();
	__stdio_exit();
	__st_Exit(status);
}
