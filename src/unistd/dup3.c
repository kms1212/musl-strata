#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include "strata_fd.h"

int __dup3(int old, int new, int flags)
{
	return __strata_fd_dup3(old, new, flags);
}

weak_alias(__dup3, dup3);
