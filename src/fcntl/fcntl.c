#define _GNU_SOURCE
#include <fcntl.h>
#include <stdarg.h>
#include "strata_fd.h"

int fcntl(int fd, int cmd, ...)
{
	unsigned long arg;
	va_list ap;
	va_start(ap, cmd);
	arg = va_arg(ap, unsigned long);
	va_end(ap);
	return __strata_fd_fcntl(fd, cmd, arg);
}
