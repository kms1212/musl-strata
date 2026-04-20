#define _BSD_SOURCE
#include <sys/stat.h>
#include "strata_fd.h"

int __fstat(int fd, struct stat *st)
{
	return __strata_fd_fstat(fd, st);
}

weak_alias(__fstat, fstat);
