#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include <strata/handle.h>
#include <strata/status.h>

#include "libc.h"
#include "lock.h"
#include "strata_fd.h"
#include "sidl/byte_stream.h"
#include "sidl/directory.h"
#include "sidl/fileinfo.h"

#define STRATA_FD_KIND_BYTESTREAM 0x01
#define STRATA_FD_KIND_DIRECTORY  0x02
#define STRATA_FD_KIND_FILEINFO   0x04
#define STRATA_FD_KIND_TTY        0x08

#define STRATA_FD_MAX 1024
#define STRATA_FILE_DESCRIPTION_MAX 1024

struct strata_file_description {
	StHandle handle;
	int open_flags;
	unsigned kind;
	int kind_valid;
	int refcount;
	int slot_refcount;
	int owned;
	int is_tty;
};

struct strata_fd_slot {
	struct strata_file_description *description;
	int fd_flags;
};

extern StHandle __stdin_handle;
extern StHandle __stdout_handle;
extern StHandle __stderr_handle;

static const struct StUuid __strata_uuid_bytestream = UUID_BYTESTREAM_INTERFACE_INIT;
static const struct StUuid __strata_uuid_directory = UUID_DIRECTORY_INTERFACE_INIT;
static const struct StUuid __strata_uuid_fileinfo = UUID_FILEINFO_INTERFACE_INIT;

static volatile int __strata_fd_lock[1];
static struct strata_file_description __strata_descriptions[STRATA_FILE_DESCRIPTION_MAX];
static struct strata_fd_slot __strata_fd_slots[STRATA_FD_MAX];
static int __strata_fd_initialized;

static int __strata_fd_has_interface(StHandle handle, const struct StUuid *uuid);

static int __strata_fd_status_to_errno(StStatus status)
{
	switch (status) {
	case STATUS_SUCCESS:
	case STATUS_ALREADY_PERFORMED:
	case STATUS_CREATED:
		return 0;
	case STATUS_INVALID_VALUE:
		return EINVAL;
	case STATUS_NOT_SUPPORTED:
		return ENOTSUP;
	case STATUS_ENTRY_NOT_FOUND:
		return ENOENT;
	case STATUS_NOT_IMPLEMENTED:
		return ENOSYS;
	case STATUS_BUFFER_UNDERFLOW:
	case STATUS_END_OF_FILE:
		return 0;
	case STATUS_BUFFER_TOO_SMALL:
		return ENOBUFS;
	case STATUS_INSUFFICIENT_SPACE:
		return ENOSPC;
	case STATUS_NOT_PERMITTED:
		return EACCES;
	case STATUS_INTERRUPTED:
		return EINTR;
	case STATUS_IO_ERROR:
		return EIO;
	case STATUS_INVALID_HANDLE:
		return EBADF;
	case STATUS_TEMPORARY_ERROR:
		return EAGAIN;
	case STATUS_RESOURCE_BUSY:
		return EBUSY;
	case STATUS_NOT_A_DIRECTORY:
		return ENOTDIR;
	case STATUS_IS_A_DIRECTORY:
		return EISDIR;
	case STATUS_TOO_MANY_OPEN_FILES:
		return EMFILE;
	case STATUS_NODE_NAME_TOO_LONG:
		return ENAMETOOLONG;
	case STATUS_DUPLICATE_ENTRY:
		return EEXIST;
	default:
		return EIO;
	}
}

static int __strata_fd_fail_status(StStatus status)
{
	errno = __strata_fd_status_to_errno(status);
	return -1;
}

static int __strata_fd_validate_number(int fd)
{
	if (fd < 0 || fd >= STRATA_FD_MAX) {
		errno = EBADF;
		return -1;
	}

	return 0;
}

static uint32_t __strata_fd_kernel_open_flags(int flags)
{
	return (uint32_t)(flags & ~(O_CLOEXEC | O_APPEND | O_NONBLOCK));
}

static int __strata_fd_has_interface(StHandle handle, const struct StUuid *uuid)
{
	uint32_t funcid_base, result_abi;

	if (handle == STHANDLE_INVALID)
		return 0;

	return CHECK_SUCCESS(StHandle_Query(handle, uuid, 0, &funcid_base, &result_abi));
}

static unsigned __strata_fd_query_kind(StHandle handle, int is_tty)
{
	unsigned kind = 0;

	if (handle != STHANDLE_INVALID) {
		if (__strata_fd_has_interface(handle, &__strata_uuid_bytestream))
			kind |= STRATA_FD_KIND_BYTESTREAM;
		if (__strata_fd_has_interface(handle, &__strata_uuid_directory))
			kind |= STRATA_FD_KIND_DIRECTORY;
		if (__strata_fd_has_interface(handle, &__strata_uuid_fileinfo))
			kind |= STRATA_FD_KIND_FILEINFO;
	}
	if (is_tty)
		kind |= STRATA_FD_KIND_TTY;

	return kind;
}

static StStatus __strata_fd_destroy_description_locked(
	struct strata_file_description *description,
	int ignore_close_failure)
{
	StStatus status = STATUS_SUCCESS;

	if (!description || description->refcount || description->slot_refcount)
		return STATUS_SUCCESS;

	if (description->owned && description->handle != STHANDLE_INVALID)
		status = StHandle_Close(description->handle);

	memset(description, 0, sizeof(*description));
	if (!ignore_close_failure && !CHECK_SUCCESS(status))
		return status;

	return STATUS_SUCCESS;
}

static void __strata_fd_release_description(struct strata_file_description *description)
{
	LOCK(__strata_fd_lock);
	if (description && description->refcount > 0) {
		description->refcount--;
		__strata_fd_destroy_description_locked(description, 1);
	}
	UNLOCK(__strata_fd_lock);
}

static StStatus __strata_fd_drop_slot_locked(int fd, int ignore_close_failure)
{
	struct strata_fd_slot *slot = &__strata_fd_slots[fd];
	struct strata_file_description *description = slot->description;

	if (!description)
		return STATUS_INVALID_HANDLE;

	slot->description = 0;
	slot->fd_flags = 0;
	description->slot_refcount--;
	description->refcount--;

	return __strata_fd_destroy_description_locked(description, ignore_close_failure);
}

static struct strata_file_description *__strata_fd_alloc_description_locked(
	StHandle handle,
	int open_flags,
	int owned,
	int is_tty)
{
	size_t i;

	for (i = 0; i < STRATA_FILE_DESCRIPTION_MAX; ++i) {
		struct strata_file_description *description = &__strata_descriptions[i];

		if (description->refcount)
			continue;

		memset(description, 0, sizeof(*description));
		description->handle = handle;
		description->open_flags = open_flags & ~O_CLOEXEC;
		description->refcount = 1;
		description->slot_refcount = 1;
		description->owned = owned;
		description->is_tty = is_tty;
		return description;
	}

	return 0;
}

static int __strata_fd_find_free_slot_locked(int min_fd)
{
	int fd;

	if (min_fd < 0)
		return -1;
	if (min_fd >= STRATA_FD_MAX)
		return -1;

	for (fd = min_fd; fd < STRATA_FD_MAX; ++fd)
		if (!__strata_fd_slots[fd].description)
			return fd;

	return -1;
}

static unsigned __strata_fd_get_kind_locked(struct strata_file_description *description)
{
	if (!description->kind_valid) {
		description->kind =
			__strata_fd_query_kind(description->handle, description->is_tty);
		description->kind_valid = 1;
	}

	return description->kind;
}

static int __strata_fd_get_snapshot(
	int fd,
	struct strata_file_description **description,
	int *open_flags,
	int *fd_flags,
	unsigned *kind)
{
	struct strata_fd_slot *slot;
	struct strata_file_description *result;

	if (__strata_fd_validate_number(fd) < 0)
		return -1;

	LOCK(__strata_fd_lock);
	slot = &__strata_fd_slots[fd];
	if (!slot->description) {
		UNLOCK(__strata_fd_lock);
		errno = EBADF;
		return -1;
	}

	result = slot->description;
	result->refcount++;
	if (kind)
		*kind = __strata_fd_get_kind_locked(result);
	if (open_flags)
		*open_flags = result->open_flags;
	if (fd_flags)
		*fd_flags = slot->fd_flags;
	*description = result;
	UNLOCK(__strata_fd_lock);

	return 0;
}

static int __strata_fd_set_open_flags(int fd, int open_flags)
{
	if (__strata_fd_validate_number(fd) < 0)
		return -1;

	LOCK(__strata_fd_lock);
	if (!__strata_fd_slots[fd].description) {
		UNLOCK(__strata_fd_lock);
		errno = EBADF;
		return -1;
	}

	__strata_fd_slots[fd].description->open_flags =
		(__strata_fd_slots[fd].description->open_flags &
		 ~(O_APPEND | O_NONBLOCK)) |
		(open_flags & (O_APPEND | O_NONBLOCK)) |
		(__strata_fd_slots[fd].description->open_flags & O_ACCMODE);
	UNLOCK(__strata_fd_lock);

	return 0;
}

static int __strata_fd_set_fd_flags(int fd, int fd_flags)
{
	if (__strata_fd_validate_number(fd) < 0)
		return -1;

	LOCK(__strata_fd_lock);
	if (!__strata_fd_slots[fd].description) {
		UNLOCK(__strata_fd_lock);
		errno = EBADF;
		return -1;
	}

	__strata_fd_slots[fd].fd_flags = fd_flags & FD_CLOEXEC;
	UNLOCK(__strata_fd_lock);

	return 0;
}

static int __strata_fd_duplicate_locked(
	struct strata_file_description *description,
	int min_fd,
	int fd_flags)
{
	int new_fd = __strata_fd_find_free_slot_locked(min_fd);

	if (new_fd < 0) {
		errno = (min_fd >= STRATA_FD_MAX) ? EINVAL : EMFILE;
		return -1;
	}

	description->refcount++;
	description->slot_refcount++;
	__strata_fd_slots[new_fd].description = description;
	__strata_fd_slots[new_fd].fd_flags = fd_flags & FD_CLOEXEC;
	return new_fd;
}

static int __strata_fd_fill_stat_from_description(
	struct strata_file_description *description,
	unsigned kind,
	struct stat *st)
{
	StIfFi_StatInfo statinfo;
	StStatus status;

	memset(st, 0, sizeof(*st));
	st->st_nlink = 1;
	st->st_blksize = libc.page_size ? libc.page_size : 4096;

	if (kind & STRATA_FD_KIND_TTY)
		st->st_mode = S_IFCHR | 0600;
	else if (kind & STRATA_FD_KIND_DIRECTORY)
		st->st_mode = S_IFDIR | 0755;
	else
		st->st_mode = S_IFREG | 0644;

	if (!(kind & STRATA_FD_KIND_FILEINFO))
		return 0;

	memset(&statinfo, 0, sizeof(statinfo));
	status = StIfFi_GetStat(description->handle, &statinfo, sizeof(statinfo));
	if (!CHECK_SUCCESS(status))
		return __strata_fd_fail_status(status);

	st->st_size = statinfo.size;
	st->st_blocks = (statinfo.alloc_size + 511) / 512;
	return 0;
}

void __strata_fd_init(void)
{
	LOCK(__strata_fd_lock);
	if (__strata_fd_initialized) {
		UNLOCK(__strata_fd_lock);
		return;
	}

	memset(__strata_descriptions, 0, sizeof(__strata_descriptions));
	memset(__strata_fd_slots, 0, sizeof(__strata_fd_slots));

	if (__stdin_handle != STHANDLE_INVALID) {
		struct strata_file_description *description =
			__strata_fd_alloc_description_locked(__stdin_handle, O_RDONLY, 0, 1);
		if (description)
			__strata_fd_slots[0].description = description;
	}
	if (__stdout_handle != STHANDLE_INVALID) {
		struct strata_file_description *description =
			__strata_fd_alloc_description_locked(__stdout_handle, O_WRONLY, 0, 1);
		if (description)
			__strata_fd_slots[1].description = description;
	}
	if (__stderr_handle != STHANDLE_INVALID) {
		struct strata_file_description *description =
			__strata_fd_alloc_description_locked(__stderr_handle, O_WRONLY, 0, 1);
		if (description)
			__strata_fd_slots[2].description = description;
	}

	__strata_fd_initialized = 1;
	UNLOCK(__strata_fd_lock);
}

int __strata_fd_open(const char *path, int flags, mode_t mode)
{
	StHandle handle;
	StStatus status;
	struct strata_file_description *description;
	int fd;
	int fd_flags;

	(void)mode;

	if (!path) {
		errno = EFAULT;
		return -1;
	}

	status = StHandle_Open((const uint8_t *)path,
		__strata_fd_kernel_open_flags(flags), &handle);
	if (!CHECK_SUCCESS(status))
		return __strata_fd_fail_status(status);

	fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;

	LOCK(__strata_fd_lock);
	fd = __strata_fd_find_free_slot_locked(0);
	if (fd < 0) {
		UNLOCK(__strata_fd_lock);
		StHandle_Close(handle);
		errno = EMFILE;
		return -1;
	}

	description = __strata_fd_alloc_description_locked(handle, flags, 1, 0);
	if (!description) {
		UNLOCK(__strata_fd_lock);
		StHandle_Close(handle);
		errno = EMFILE;
		return -1;
	}

	__strata_fd_slots[fd].description = description;
	__strata_fd_slots[fd].fd_flags = fd_flags;
	UNLOCK(__strata_fd_lock);

	return fd;
}

int __strata_fd_close(int fd)
{
	StStatus status;

	if (__strata_fd_validate_number(fd) < 0)
		return -1;

	LOCK(__strata_fd_lock);
	if (!__strata_fd_slots[fd].description) {
		UNLOCK(__strata_fd_lock);
		errno = EBADF;
		return -1;
	}

	status = __strata_fd_drop_slot_locked(fd, 0);
	UNLOCK(__strata_fd_lock);

	if (!CHECK_SUCCESS(status))
		return __strata_fd_fail_status(status);
	return 0;
}

ssize_t __strata_fd_read(int fd, void *buf, size_t count)
{
	struct strata_file_description *description;
	StStatus status;
	uint64_t transferred = 0;
	StIfBs_IoFlags io_flags = 0;
	int open_flags;
	unsigned kind;

	if (__strata_fd_get_snapshot(fd, &description, &open_flags, 0, &kind) < 0)
		return -1;
	if (!(kind & STRATA_FD_KIND_BYTESTREAM)) {
		__strata_fd_release_description(description);
		errno = (kind & STRATA_FD_KIND_DIRECTORY) ? EISDIR : EBADF;
		return -1;
	}
	if ((open_flags & O_ACCMODE) == O_WRONLY) {
		__strata_fd_release_description(description);
		errno = EBADF;
		return -1;
	}
	if (open_flags & O_NONBLOCK)
		io_flags |= BYTESTREAM_IOFLAGS_NONBLOCKING;

	status = StIfBs_Read(description->handle, buf, count, io_flags, &transferred);
	__strata_fd_release_description(description);

	if (status == STATUS_END_OF_FILE)
		return 0;
	if (!CHECK_SUCCESS(status))
		return __strata_fd_fail_status(status);

	return transferred;
}

ssize_t __strata_fd_write(int fd, const void *buf, size_t count)
{
	struct strata_file_description *description;
	StStatus status;
	uint64_t transferred = 0;
	StIfBs_IoFlags io_flags = 0;
	int open_flags;
	int64_t ignored;
	unsigned kind;

	if (__strata_fd_get_snapshot(fd, &description, &open_flags, 0, &kind) < 0)
		return -1;
	if (!(kind & STRATA_FD_KIND_BYTESTREAM)) {
		__strata_fd_release_description(description);
		errno = (kind & STRATA_FD_KIND_DIRECTORY) ? EISDIR : EBADF;
		return -1;
	}
	if ((open_flags & O_ACCMODE) == O_RDONLY) {
		__strata_fd_release_description(description);
		errno = EBADF;
		return -1;
	}
	if (open_flags & O_NONBLOCK)
		io_flags |= BYTESTREAM_IOFLAGS_NONBLOCKING;
	if (open_flags & O_APPEND) {
		status = StIfBs_Seek(description->handle, 0, SEEK_END, &ignored);
		if (!CHECK_SUCCESS(status)) {
			__strata_fd_release_description(description);
			return __strata_fd_fail_status(status);
		}
	}

	status = StIfBs_Write(description->handle, (const uint8_t *)buf, count,
		io_flags, &transferred);
	__strata_fd_release_description(description);

	if (!CHECK_SUCCESS(status))
		return __strata_fd_fail_status(status);

	return transferred;
}

ssize_t __strata_fd_readv(int fd, const struct iovec *iov, int count)
{
	ssize_t total = 0, chunk;
	int i;

	if (count < 0) {
		errno = EINVAL;
		return -1;
	}

	for (i = 0; i < count; i++) {
		if (!iov[i].iov_len)
			continue;
		chunk = __strata_fd_read(fd, iov[i].iov_base, iov[i].iov_len);
		if (chunk < 0)
			return total ? total : -1;
		total += chunk;
		if ((size_t)chunk != iov[i].iov_len)
			break;
	}

	return total;
}

ssize_t __strata_fd_writev(int fd, const struct iovec *iov, int count)
{
	ssize_t total = 0, chunk;
	int i;

	if (count < 0) {
		errno = EINVAL;
		return -1;
	}

	for (i = 0; i < count; i++) {
		if (!iov[i].iov_len)
			continue;
		chunk = __strata_fd_write(fd, iov[i].iov_base, iov[i].iov_len);
		if (chunk < 0)
			return total ? total : -1;
		total += chunk;
		if ((size_t)chunk != iov[i].iov_len)
			break;
	}

	return total;
}

off_t __strata_fd_lseek(int fd, off_t offset, int whence)
{
	struct strata_file_description *description;
	StStatus status;
	int64_t result = 0;
	unsigned kind;

	if (__strata_fd_get_snapshot(fd, &description, 0, 0, &kind) < 0)
		return -1;
	if (!(kind & STRATA_FD_KIND_BYTESTREAM)) {
		__strata_fd_release_description(description);
		errno = (kind & STRATA_FD_KIND_DIRECTORY) ? EISDIR : EBADF;
		return -1;
	}

	status = StIfBs_Seek(description->handle, offset, whence, &result);
	__strata_fd_release_description(description);

	if (!CHECK_SUCCESS(status))
		return __strata_fd_fail_status(status);

	return result;
}

int __strata_fd_fcntl(int fd, int cmd, unsigned long arg)
{
	struct strata_file_description *description;
	int flags;

	switch (cmd) {
	case F_GETFD:
		if (__strata_fd_get_snapshot(fd, &description, 0, &flags, 0) < 0)
			return -1;
		__strata_fd_release_description(description);
		return flags;
	case F_SETFD:
		return __strata_fd_set_fd_flags(fd, arg & FD_CLOEXEC);
	case F_GETFL:
		if (__strata_fd_get_snapshot(fd, &description, &flags, 0, 0) < 0)
			return -1;
		__strata_fd_release_description(description);
		return flags;
	case F_SETFL:
		if (__strata_fd_get_snapshot(fd, &description, &flags, 0, 0) < 0)
			return -1;
		__strata_fd_release_description(description);
		flags = (flags & ~(O_APPEND | O_NONBLOCK)) |
			(arg & (O_APPEND | O_NONBLOCK));
		return __strata_fd_set_open_flags(fd, flags);
	case F_DUPFD:
	case F_DUPFD_CLOEXEC:
		if (arg > INT_MAX) {
			errno = EINVAL;
			return -1;
		}
		if (__strata_fd_validate_number(fd) < 0)
			return -1;
		LOCK(__strata_fd_lock);
		if (!__strata_fd_slots[fd].description) {
			UNLOCK(__strata_fd_lock);
			errno = EBADF;
			return -1;
		}
		fd = __strata_fd_duplicate_locked(__strata_fd_slots[fd].description,
			(int)arg, cmd == F_DUPFD_CLOEXEC ? FD_CLOEXEC : 0);
		UNLOCK(__strata_fd_lock);
		return fd;
	default:
		errno = EINVAL;
		return -1;
	}
}

int __strata_fd_dup(int fd)
{
	return __strata_fd_fcntl(fd, F_DUPFD, 0);
}

int __strata_fd_dup2(int oldfd, int newfd)
{
	return __strata_fd_dup3(oldfd, newfd, 0);
}

int __strata_fd_dup3(int oldfd, int newfd, int flags)
{
	struct strata_file_description *description;
	StStatus status = STATUS_SUCCESS;

	if (flags & ~O_CLOEXEC) {
		errno = EINVAL;
		return -1;
	}
	if (__strata_fd_validate_number(oldfd) < 0 || __strata_fd_validate_number(newfd) < 0)
		return -1;
	if (oldfd == newfd) {
		if (flags) {
			errno = EINVAL;
			return -1;
		}
		LOCK(__strata_fd_lock);
		description = __strata_fd_slots[oldfd].description;
		UNLOCK(__strata_fd_lock);
		if (!description) {
			errno = EBADF;
			return -1;
		}
		return newfd;
	}

	LOCK(__strata_fd_lock);
	description = __strata_fd_slots[oldfd].description;
	if (!description) {
		UNLOCK(__strata_fd_lock);
		errno = EBADF;
		return -1;
	}

	if (__strata_fd_slots[newfd].description)
		status = __strata_fd_drop_slot_locked(newfd, 1);
	if (CHECK_SUCCESS(status)) {
		description->refcount++;
		description->slot_refcount++;
		__strata_fd_slots[newfd].description = description;
		__strata_fd_slots[newfd].fd_flags = (flags & O_CLOEXEC) ? FD_CLOEXEC : 0;
	}
	UNLOCK(__strata_fd_lock);

	if (!CHECK_SUCCESS(status))
		return __strata_fd_fail_status(status);
	return newfd;
}

int __strata_fd_isatty(int fd)
{
	struct strata_file_description *description;
	unsigned kind;

	if (__strata_fd_get_snapshot(fd, &description, 0, 0, &kind) < 0)
		return 0;

	__strata_fd_release_description(description);
	if (kind & STRATA_FD_KIND_TTY)
		return 1;

	errno = ENOTTY;
	return 0;
}

int __strata_fd_fstat(int fd, struct stat *st)
{
	struct strata_file_description *description;
	unsigned kind;
	int result;

	if (!st) {
		errno = EINVAL;
		return -1;
	}
	if (__strata_fd_get_snapshot(fd, &description, 0, 0, &kind) < 0)
		return -1;

	result = __strata_fd_fill_stat_from_description(description, kind, st);
	__strata_fd_release_description(description);
	return result;
}
