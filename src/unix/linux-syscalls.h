/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef UV_LINUX_SYSCALL_H_
#define UV_LINUX_SYSCALL_H_

#undef  _GNU_SOURCE
#define _GNU_SOURCE

#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#if defined(__alpha__)
# define UV__O_CLOEXEC        0x200000
#elif defined(__hppa__)
# define UV__O_CLOEXEC        0x200000
#elif defined(__sparc__)
# define UV__O_CLOEXEC        0x400000
#else
# define UV__O_CLOEXEC        0x80000
#endif

#if defined(__alpha__)
# define UV__O_NONBLOCK       0x4
#elif defined(__hppa__)
# define UV__O_NONBLOCK       O_NONBLOCK
#elif defined(__mips__)
# define UV__O_NONBLOCK       0x80
#elif defined(__sparc__)
# define UV__O_NONBLOCK       0x4000
#else
# define UV__O_NONBLOCK       0x800
#endif

#define UV__EFD_CLOEXEC       UV__O_CLOEXEC
#define UV__EFD_NONBLOCK      UV__O_NONBLOCK

#define UV__IN_CLOEXEC        UV__O_CLOEXEC
#define UV__IN_NONBLOCK       UV__O_NONBLOCK

#define UV__SOCK_CLOEXEC      UV__O_CLOEXEC
#if defined(SOCK_NONBLOCK)
# define UV__SOCK_NONBLOCK    SOCK_NONBLOCK
#else
# define UV__SOCK_NONBLOCK    UV__O_NONBLOCK
#endif

/* inotify flags */
#define UV__IN_ACCESS         0x001
#define UV__IN_MODIFY         0x002
#define UV__IN_ATTRIB         0x004
#define UV__IN_CLOSE_WRITE    0x008
#define UV__IN_CLOSE_NOWRITE  0x010
#define UV__IN_OPEN           0x020
#define UV__IN_MOVED_FROM     0x040
#define UV__IN_MOVED_TO       0x080
#define UV__IN_CREATE         0x100
#define UV__IN_DELETE         0x200
#define UV__IN_DELETE_SELF    0x400
#define UV__IN_MOVE_SELF      0x800

struct uv__statx_timestamp {
  int64_t tv_sec;
  uint32_t tv_nsec;
  int32_t unused0;
};

struct uv__statx {
  uint32_t stx_mask;
  uint32_t stx_blksize;
  uint64_t stx_attributes;
  uint32_t stx_nlink;
  uint32_t stx_uid;
  uint32_t stx_gid;
  uint16_t stx_mode;
  uint16_t unused0;
  uint64_t stx_ino;
  uint64_t stx_size;
  uint64_t stx_blocks;
  uint64_t stx_attributes_mask;
  struct uv__statx_timestamp stx_atime;
  struct uv__statx_timestamp stx_btime;
  struct uv__statx_timestamp stx_ctime;
  struct uv__statx_timestamp stx_mtime;
  uint32_t stx_rdev_major;
  uint32_t stx_rdev_minor;
  uint32_t stx_dev_major;
  uint32_t stx_dev_minor;
  uint64_t unused1[14];
};

struct uv__inotify_event {
  int32_t wd;
  uint32_t mask;
  uint32_t cookie;
  uint32_t len;
  /* char name[0]; */
};

struct uv__mmsghdr {
  struct msghdr msg_hdr;
  unsigned int msg_len;
};

/* io_uring (adapted from io_uring.h) */

/* IO submission data structure (Submission Queue Entry) */
struct uv__io_uring_sqe {
  uint8_t opcode;  /* type of operation for this sqe */
  uint8_t flags;   /* IOSQE_ flags */
  uint16_t ioprio; /* ioprio for the request */
  int32_t fd;      /* file descriptor to do IO on */
  uint64_t off;    /* offset into file */
  uint64_t addr;   /* pointer to buffer or iovecs */
  uint32_t len;    /* buffer size or number of iovecs */
  union {
    uint32_t rw_flags;
    uint32_t fsync_flags;
    uint16_t poll_events;
    uint32_t sync_range_flags;
    uint32_t msg_flags;
  } flags_events_u;
  uint64_t user_data;   /* data to be passed back at completion time */
  union {
    uint16_t buf_index; /* index into fixed buffers, if used */
    uint64_t __pad2[3];
  } buf_u;
};

/* IO completion data structure (Completion Queue Entry) */
struct uv__io_uring_cqe {
  uint64_t user_data; /* sqe->data submission passed back */
  int32_t res;        /* result code for this event */
  uint32_t flags;
};

/* io_uring_enter(2) opcodes */
#define UV__IORING_OP_NOP              0
#define UV__IORING_OP_READV            1
#define UV__IORING_OP_WRITEV           2
#define UV__IORING_OP_FSYNC            3
#define UV__IORING_OP_READ_FIXED       4
#define UV__IORING_OP_WRITE_FIXED      5
#define UV__IORING_OP_POLL_ADD         6
#define UV__IORING_OP_POLL_REMOVE      7
#define UV__IORING_OP_SYNC_FILE_RANGE  8
#define UV__IORING_OP_SENDMSG          9
#define UV__IORING_OP_RECVMSG          10

/* io_uring_enter(2) flags */
#define UV__IORING_ENTER_GETEVENTS  (1U << 0)
#define UV__IORING_ENTER_SQ_WAKEUP  (1U << 1)

/* sqe->fsync_flags */
#define UV__IORING_FSYNC_DATASYNC  (1U << 0)

/* io_uring_register(2) opcodes and arguments */
#define UV__IORING_REGISTER_BUFFERS    0
#define UV__IORING_UNREGISTER_BUFFERS  1
#define UV__IORING_REGISTER_FILES      2
#define UV__IORING_UNREGISTER_FILES    3
#define UV__IORING_REGISTER_EVENTFD    4
#define UV__IORING_UNREGISTER_EVENTFD  5

/* Magic offsets to mmap the data in the io_uring fd. */
#define UV__IORING_OFF_SQ_RING  0ULL
#define UV__IORING_OFF_CQ_RING  0x8000000ULL
#define UV__IORING_OFF_SQES     0x10000000ULL

/* Filled with the offset for mmap(2) */
struct uv__io_sqring_offsets {
  uint32_t head;
  uint32_t tail;
  uint32_t ring_mask;
  uint32_t ring_entries;
  uint32_t flags;
  uint32_t dropped;      /* num not submitted */
  uint32_t array;        /* sqe index array */
  uint32_t resv1;
  uint64_t resv2;
};

struct uv__io_cqring_offsets {
  uint32_t head;
  uint32_t tail;
  uint32_t ring_mask;
  uint32_t ring_entries;
  uint32_t overflow;
  uint32_t cqes;
  uint64_t resv[2];
};

/* Passed in to io_uring_setup(2). Copied back with updated info on success. */
struct uv__io_uring_params {
  uint32_t sq_entries; /* sq size */
  uint32_t cq_entries; /* cq size */
  uint32_t flags;
  uint32_t sq_thread_cpu;
  uint32_t sq_thread_idle;
  uint32_t resv[5];
	struct uv__io_sqring_offsets sq_off;
	struct uv__io_cqring_offsets cq_off;
};

int uv__accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags);
int uv__eventfd(unsigned int count);
int uv__eventfd2(unsigned int count, int flags);
int uv__inotify_init(void);
int uv__inotify_init1(int flags);
int uv__inotify_add_watch(int fd, const char* path, uint32_t mask);
int uv__inotify_rm_watch(int fd, int32_t wd);
int uv__pipe2(int pipefd[2], int flags);
int uv__recvmmsg(int fd,
                 struct uv__mmsghdr* mmsg,
                 unsigned int vlen,
                 unsigned int flags,
                 struct timespec* timeout);
int uv__sendmmsg(int fd,
                 struct uv__mmsghdr* mmsg,
                 unsigned int vlen,
                 unsigned int flags);
ssize_t uv__preadv(int fd, const struct iovec *iov, int iovcnt, int64_t offset);
ssize_t uv__pwritev(int fd, const struct iovec *iov, int iovcnt, int64_t offset);
int uv__dup3(int oldfd, int newfd, int flags);
int uv__statx(int dirfd,
              const char* path,
              int flags,
              unsigned int mask,
              struct uv__statx* statxbuf);
int uv__io_uring_register(int fd,
                          unsigned int opcode,
                          void* arg,
                          unsigned int nr_args);
int uv__io_uring_setup(unsigned int entries, struct uv__io_uring_params* p);
int uv__io_uring_enter(int fd,
                       unsigned int to_submit,
                       unsigned int min_complete,
                       unsigned int flags, sigset_t* sig);

#endif /* UV_LINUX_SYSCALL_H_ */
