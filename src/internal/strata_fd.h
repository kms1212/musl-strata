#ifndef _STRATA_FD_H
#define _STRATA_FD_H

#include <sys/types.h>

struct iovec;
struct stat;

hidden void __strata_fd_init(void);

hidden int __strata_fd_open(const char *path, int flags, mode_t mode);
hidden int __strata_fd_close(int fd);
hidden ssize_t __strata_fd_read(int fd, void *buf, size_t count);
hidden ssize_t __strata_fd_write(int fd, const void *buf, size_t count);
hidden ssize_t __strata_fd_readv(int fd, const struct iovec *iov, int count);
hidden ssize_t __strata_fd_writev(int fd, const struct iovec *iov, int count);
hidden off_t __strata_fd_lseek(int fd, off_t offset, int whence);
hidden int __strata_fd_fcntl(int fd, int cmd, unsigned long arg);
hidden int __strata_fd_dup(int fd);
hidden int __strata_fd_dup2(int oldfd, int newfd);
hidden int __strata_fd_dup3(int oldfd, int newfd, int flags);
hidden int __strata_fd_isatty(int fd);
hidden int __strata_fd_fstat(int fd, struct stat *st);

#endif
