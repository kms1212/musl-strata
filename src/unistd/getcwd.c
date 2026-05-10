#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "syscall.h"

char *getcwd(char *buf, size_t size)
{
	long ret;
	if (!buf) {
		char tmp[PATH_MAX];
		ret = syscall(SYS_getcwd, tmp, sizeof tmp);
		if (ret < 0)
			return 0;
		if (ret == 0 || tmp[0] != '/') {
			errno = ENOENT;
			return 0;
		}
		return strdup(tmp);
	}
	if (!size) {
		errno = EINVAL;
		return 0;
	}

	ret = syscall(SYS_getcwd, buf, size);
	if (ret < 0)
		return 0;
	if (ret == 0 || buf[0] != '/') {
		errno = ENOENT;
		return 0;
	}
	return buf;
}
