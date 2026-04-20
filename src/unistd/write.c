#include <unistd.h>
#include "strata_fd.h"

ssize_t write(int fd, const void *buf, size_t count)
{
	return __strata_fd_write(fd, buf, count);
}
