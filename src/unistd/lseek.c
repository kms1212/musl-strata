#include <unistd.h>
#include "strata_fd.h"

off_t __lseek(int fd, off_t offset, int whence)
{
	return __strata_fd_lseek(fd, offset, whence);
}

weak_alias(__lseek, lseek);
