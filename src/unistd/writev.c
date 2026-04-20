#include <sys/uio.h>
#include "strata_fd.h"

ssize_t writev(int fd, const struct iovec *iov, int count)
{
	return __strata_fd_writev(fd, iov, count);
}
