#ifndef UV_UNIX_LINUX_IOURING_
#define UV_UNIX_LINUX_IOURING_

#include <sys/uio.h>
#include <signal.h>
#include <string.h>
#include <inttypes.h>
#include "linux-syscalls.h"

/* Helper functions for working with io_uring. Currently there's a one-to-one
 * requirement between getting an SQE with uv__io_uring_get_sqe and submitting
 * it with uv__io_uring_submit; however, that can be changed so that multiple
 * SQEs  can be submitted at once if necessary.
 */

struct uv__io_uring_sq {
  unsigned* head;
  unsigned* tail;
  unsigned* ring_mask;
  unsigned* ring_entries;
  unsigned* flags;
  unsigned* dropped;  /* num not submitted */
  unsigned* array;    /* sqe index array */

  struct uv__io_uring_sqe* sqes;

  size_t map_sz; /* for unmapping */

  /* These are the retrieved, but not ubmitted to the kernel. */
  // unsigned sqe_head;
  // unsigned sqe_tail;
};

struct uv__io_uring_cq {
  unsigned* head;
  unsigned* tail;
  unsigned* ring_mask;
  unsigned* ring_entries;
  unsigned* overflow;

  struct uv__io_uring_cqe* cqes;

  size_t map_sz;
};

struct uv__io_uring {
  struct uv__io_uring_sq sq;
  struct uv__io_uring_cq cq;
  int ring_fd;
};

/* Returns 0 on success, errno on error. */
int uv__io_uring_init(unsigned entries, struct uv__io_uring* ring);
void uv__io_uring_destroy(struct uv__io_uring* ring);

/* Gets the tail SQE to fill. Returns NULL if SQ is full. */
struct uv__io_uring_sqe* uv__io_uring_get_sqe(struct uv__io_uring* ring);
/* Submit the tail SQE to the kernel. Returns 0 on success. */
int uv__io_uring_submit(struct uv__io_uring* ring);
/* Gets an available CQE. If no CQE is available, cqe_ptr will be null. This
 * consumes the CQE (updates the ring head).
 */
void uv__io_uring_get_cqe(struct uv__io_uring* ring, struct uv__io_uring_cqe** cqe_ptr);

#endif // ndef UV_UNIX_LINUX_IOURING_
