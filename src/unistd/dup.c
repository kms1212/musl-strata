#include <unistd.h>
#include "strata_fd.h"

int dup(int fd)
{
	return __strata_fd_dup(fd);
}
