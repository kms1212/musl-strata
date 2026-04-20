#include <sys/uio.h>
#include "strata_fd.h"

ssize_t readv(int fd, const struct iovec *iov, int count)
{
	return __strata_fd_readv(fd, iov, count);
}
