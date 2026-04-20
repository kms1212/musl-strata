#include <unistd.h>
#include "strata_fd.h"

ssize_t read(int fd, void *buf, size_t count)
{
	return __strata_fd_read(fd, buf, count);
}
