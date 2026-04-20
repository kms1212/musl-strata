#include <unistd.h>
#include "aio_impl.h"
#include "strata_fd.h"

static int dummy(int fd)
{
	return fd;
}

weak_alias(dummy, __aio_close);

int close(int fd)
{
	fd = __aio_close(fd);
	return __strata_fd_close(fd);
}
