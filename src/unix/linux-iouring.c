#include "linux-iouring.h"
#include "linux-syscalls.h"
#include "atomic-ops.h"
#include "uv.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define munmap_checked(addr, size) if (munmap((addr), (size))) abort()

int uv__io_uring_init(unsigned entries, struct uv__io_uring* ring) {
  struct uv__io_uring_params params;
  int fd;
  char* sq_ptr;
  char* cq_ptr;
  size_t sqe_map_size;
  int event_fd;
  int status;

  memset(&params, 0, sizeof(params));

  fd = uv__io_uring_setup(entries, &params);
  if (fd < 0)
    return UV__ERR(errno);

  status = uv__cloexec(fd, 1);
  if (status) {
    uv__close(fd);
    return status;
  }

  ring->ring_fd = fd;

  /* Map SQ */
  ring->sq.map_sz = params.sq_off.array + params.sq_entries * sizeof(uint32_t);
  sq_ptr = mmap(NULL,
                ring->sq.map_sz,
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_POPULATE,
                fd,
                UV__IORING_OFF_SQ_RING);

  if (sq_ptr == MAP_FAILED) {
    status = errno; /* Return the errno from mmap. */
    uv__close(fd);
    return UV__ERR(status);
  }

  ring->sq.head = (unsigned*)(sq_ptr + params.sq_off.head);
  ring->sq.tail = (unsigned*)(sq_ptr + params.sq_off.tail);
  ring->sq.ring_mask = (unsigned*)(sq_ptr + params.sq_off.ring_mask);
  ring->sq.ring_entries = (unsigned*)(sq_ptr + params.sq_off.ring_entries);
  ring->sq.flags = (unsigned*)(sq_ptr + params.sq_off.flags);
  ring->sq.dropped = (unsigned*)(sq_ptr + params.sq_off.dropped);
  ring->sq.array = (unsigned*)(sq_ptr + params.sq_off.array);

  /* Map SQEs */
  sqe_map_size = params.sq_entries * sizeof(*ring->sq.sqes);
  ring->sq.sqes = mmap(NULL,
                       sqe_map_size,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_POPULATE,
                       fd,
                       UV__IORING_OFF_SQES);

  if (ring->sq.sqes == MAP_FAILED) {
    status = errno;
    munmap_checked(ring->sq.head, ring->sq.map_sz);
    uv__close(fd);
    return UV__ERR(status);
  }

  /* Map CQ */
  ring->cq.map_sz = params.cq_off.cqes + params.cq_entries *
      sizeof(*ring->cq.cqes);
  cq_ptr = mmap(NULL,
                ring->cq.map_sz,
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_POPULATE,
                fd,
                UV__IORING_OFF_CQ_RING);

  if (cq_ptr == MAP_FAILED) {
    status = errno;
    munmap_checked(ring->sq.sqes, sqe_map_size);
    munmap_checked(ring->sq.head, ring->sq.map_sz);
    uv__close(fd);
    return UV__ERR(status);
  }

  ring->cq.head = (unsigned*)(cq_ptr + params.cq_off.head);
  ring->cq.tail = (unsigned*)(cq_ptr + params.cq_off.tail);
  ring->cq.ring_mask = (unsigned*)(cq_ptr + params.cq_off.ring_mask);
  ring->cq.ring_entries = (unsigned*)(cq_ptr + params.cq_off.ring_entries);
  ring->cq.overflow = (unsigned*)(cq_ptr + params.cq_off.overflow);
  ring->cq.cqes = (struct uv__io_uring_cqe*)(cq_ptr + params.cq_off.cqes);

  event_fd = uv__eventfd2(0, UV__EFD_NONBLOCK | UV__EFD_CLOEXEC);
  if (event_fd == -1) {
    status = errno;
    munmap_checked(ring->sq.sqes, sqe_map_size);
    munmap_checked(ring->sq.head, ring->sq.map_sz);
    munmap_checked(ring->cq.head, ring->cq.map_sz);
    return UV__ERR(status);
  }

  status = uv__io_uring_register(fd, UV__IORING_REGISTER_EVENTFD, &event_fd, 1);
  if (status < 0) {
    munmap_checked(ring->sq.sqes, sqe_map_size);
    munmap_checked(ring->sq.head, ring->sq.map_sz);
    munmap_checked(ring->cq.head, ring->cq.map_sz);
    uv__close(event_fd);
    uv__close(fd);
    return status;
  }

  ring->event_fd = event_fd;

  ring->pending = 0;

  return 0;
}


void uv__io_uring_destroy(struct uv__io_uring* ring) {
  munmap_checked(ring->sq.sqes,
                 *ring->sq.ring_entries * sizeof(*ring->sq.sqes));
  munmap_checked(ring->sq.head, ring->sq.map_sz);
  munmap_checked(ring->cq.head, ring->cq.map_sz);
  uv__close(ring->ring_fd);
  uv__close(ring->event_fd);
}

struct uv__io_uring_sqe* uv__io_uring_get_sqe(struct uv__io_uring* ring) {
  struct uv__io_uring_sqe* sqe;
  unsigned idx;

  /* Barrier before reading the SQ head. */
  mem_barrier();
  if (*ring->sq.tail - *ring->sq.head == *ring->sq.ring_entries) {
    /* SQ is full */
    return NULL;
  }

  idx = *ring->sq.tail & *ring->sq.ring_mask;
  sqe = &ring->sq.sqes[idx];

  memset(sqe, 0, sizeof(*sqe));

  return sqe;
}


int uv__io_uring_submit(struct uv__io_uring* ring) {
  unsigned idx;
  int rc;

  idx = *ring->sq.tail & *ring->sq.ring_mask;
  ring->sq.array[idx] = idx;
  /* Ensure SQE stores are visible before we update the tail. */
  mem_barrier();

  (*ring->sq.tail)++;
  /* Ensure kernel sees tail update. */
  mem_barrier();
  rc = uv__io_uring_enter(ring->ring_fd, 1, 0, 0, NULL);

  if (rc == -1) {
    /* The kernel doesn't increment the head on failure. */
    (*ring->sq.tail)--;
    mem_barrier();
  }

  return rc;
}


void uv__io_uring_get_cqe(struct uv__io_uring* ring,
                          struct uv__io_uring_cqe** cqe_ptr) {
  unsigned head;

  head = *ring->cq.head;
  /* Barrier before reading the CQ tail. */
  mem_barrier();
  if (head != *ring->cq.tail)
    *cqe_ptr = &ring->cq.cqes[head & *ring->cq.ring_mask];
  else
    *cqe_ptr = NULL;
}


/* TODO I'd rather this be static inline, but atomic-ops.h (mem_barrier)
 * includes internal.h includes linux-iouring.h because it needs uv__io_uring.
 */
void uv__io_uring_advance_cq(struct uv__io_uring* ring) {
  (*ring->cq.head)++;
  mem_barrier();
  /* Write barrier so kernel sees head increment. */
  /* While we're here and had to place a barrier, check for CQ overflow. */
  assert(*ring->cq.overflow == 0 && "io_uring CQ overflowed");
}


/* Return values:
 * 0 on success.
 * UV_ENOSYS if io_uring is not available.
 * UV_ENOTSUP if the request is not async.
 * UV_ENOTSUP if off == -1.
 * UV_ENOTSUP if the file is 0, 1 or 2. (Workaround for kernel bug, 5c8b0b54d.)
 * UV_ENOMEM if the SQ is full or the CQ might become full.
 * -1 if no jobs were successfully submitted. (Should not happen.)
 * Any of the errors that may be set by io_uring_enter(2).
 * Any of the errors that may be set by lseek(2), if off==-1.
 */
int uv__fs_work(uint8_t opcode,
                uv_loop_t* loop,
                uv_fs_t* req,
                uv_os_fd_t file,
                const uv_buf_t bufs[],
                unsigned int nbufs,
                int64_t off,
                uv_fs_cb cb) {
  struct uv__backend_data_io_uring* backend_data;
  struct uv__io_uring_sqe* sqe;
  int submitted;
  uint32_t incr_val;

  uv_poll_t* handle;

  if (cb == NULL || loop == NULL)
    return UV_ENOTSUP;

  if (!(loop->flags & UV_LOOP_USE_IOURING))
    return UV_ENOSYS;
  
  /* io_uring does not support current-position ops, and we can't acheive atomic
   * behavior with lseek(2).
   */
  if (off == -1)
    return UV_ENOTSUP;

  /* io_uring_submit fails with EINVAL for fd 0, 1, 2. Pending Q to axboe. */
  if (file == STDIN_FILENO || file == STDOUT_FILENO || file == STDERR_FILENO)
    return UV_ENOTSUP;

  backend_data = loop->backend.data;

  /* The CQ is 2x the size of the SQ, but the kernel quickly frees up the slot
   * in the SQ after submission, so we could potentially overflow it if we
   * submit a ton of SQEs in one loop iteration.
   */
  incr_val = (uint32_t) backend_data->ring.pending + 1;
  if (incr_val > *backend_data->ring.sq.ring_entries)
    return UV_ENOMEM;

  sqe = uv__io_uring_get_sqe(&backend_data->ring);
  /* See TODO where #define IOURING_SQ_SIZE is. */
  if (sqe == NULL)
    return UV_ENOMEM;

  sqe->opcode = opcode;
  sqe->fd = file;
  sqe->off = off;
  sqe->addr = (uint64_t)bufs;
  sqe->len = nbufs;
  sqe->user_data = (uint64_t)req;

  if (backend_data->ring.pending++ == 0) {
    handle = &backend_data->ring.poll_handle;
    uv__io_start(loop, &handle->io_watcher, POLLIN);
    uv__handle_start(handle);
  }

  submitted = uv__io_uring_submit(&backend_data->ring);

  if (submitted == 1) {
    req->fs_req_engine &= UV__ENGINE_IOURING;
    return 0;
  }

  /* Should not happen; mainly to check we're advancing the rings correctly. */
  if (submitted == 0)
    return -1;

  return UV__ERR(errno);
}


int uv__platform_fs_read(uv_loop_t* loop,
                         uv_fs_t* req,
                         uv_os_fd_t file,
                         const uv_buf_t bufs[],
                         unsigned int nbufs,
                         int64_t off,
                         uv_fs_cb cb) {
  return uv__fs_work(UV__IORING_OP_READV,
    loop, req, file, bufs, nbufs, off, cb);
}


int uv__platform_fs_write(uv_loop_t* loop,
                          uv_fs_t* req,
                          uv_os_fd_t file,
                          const uv_buf_t bufs[],
                          unsigned int nbufs,
                          int64_t off,
                          uv_fs_cb cb) {
  return uv__fs_work(UV__IORING_OP_WRITEV,
    loop, req, file, bufs, nbufs, off, cb);
}


int uv__platform_fs_fsync(uv_loop_t* loop,
                          uv_fs_t* req,
                          uv_os_fd_t file,
                          uv_fs_cb cb) {
  return uv__fs_work(UV__IORING_OP_FSYNC, loop, req, file, NULL, 0, 0, cb);
}


int uv__platform_work_cancel(uv_req_t* req) {
  if (req->type == UV_FS) {
    if (((uv_fs_t*)req)->fs_req_engine == UV__ENGINE_IOURING) {
      ((uv_fs_t*)req)->result = UV_ECANCELED;
      return 0;
    }
  }

  return UV_ENOSYS;
}
