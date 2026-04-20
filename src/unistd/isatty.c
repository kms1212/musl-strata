#include <unistd.h>
#include "strata_fd.h"

int isatty(int fd)
{
	return __strata_fd_isatty(fd);
}
