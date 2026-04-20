#include <unistd.h>
#include "strata_fd.h"

int dup2(int old, int new)
{
	return __strata_fd_dup2(old, new);
}
