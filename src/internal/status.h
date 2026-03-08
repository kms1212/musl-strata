#ifndef _STATUS_H
#define _STATUS_H

#include <strata/status.h>

int __status_to_errno(StStatus status);
StStatus __errno_to_status(int errno);

#define MAKE_TERMINATE_STATUS(ec) ((StStatus)((ec) << 16))

#endif // _STATUS_H
