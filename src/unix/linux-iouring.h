#ifndef UV_UNIX_LINUX_IOURING_
#define UV_UNIX_LINUX_IOURING_

#include "linux-syscalls.h"
#include "uv.h"

#include <inttypes.h>
#include <signal.h>
#include <string.h>
#include <sys/uio.h>

/* Helper functions for working with io_uring. Currently there's a one-to-one
 * requirement between getting an SQE with uv__io_uring_get_sqe and submitting
 * it with uv__io_uring_submit; however, that can be changed so that multiple
 * SQEs can be submitted at once if necessary.
 */

struct uv__io_uring_sq {
  uint32_t* head;     /* kernel controls */
  uint32_t* tail;     /* libuv controls */
  uint32_t* ring_mask;
  uint32_t* ring_entries;
  uint32_t* flags;
  uint32_t* dropped;  /* num not submitted */
  uint32_t* array;    /* sqe index array, pointing to entries in sqes */

  struct uv__io_uring_sqe* sqes;

  size_t map_sz; /* for unmapping */
};

struct uv__io_uring_cq {
  uint32_t* head;          /* libuv controls */
  uint32_t* tail;          /* kernel controls */
  uint32_t* ring_mask;
  uint32_t* ring_entries;
  uint32_t* overflow;      /* num of CQEs lost because queue was full */

  struct uv__io_uring_cqe* cqes;

  size_t map_sz;
};

struct uv__io_uring {
  struct uv__io_uring_sq sq;
  struct uv__io_uring_cq cq;
  uv_poll_t poll_handle;
  int32_t pending;
  int ring_fd;
  int event_fd;
};

/* Returns 0 on success, -errno on error. */
int uv__io_uring_init(unsigned entries, struct uv__io_uring* ring);
void uv__io_uring_destroy(struct uv__io_uring* ring);

/* Gets the tail SQE to fill. Returns NULL if SQ is full. */
struct uv__io_uring_sqe* uv__io_uring_get_sqe(struct uv__io_uring* ring);
/* Submit the tail SQE to the kernel. Returns 0 on success. */
int uv__io_uring_submit(struct uv__io_uring* ring);
/* Gets an available CQE. If no CQE is available, cqe_ptr will be null. After
 * you've copied all necessary information out of the CQE, call
 * uv__io_uring_advance_cq to allow it to be reused by the kernel.
 */
void uv__io_uring_get_cqe(struct uv__io_uring* ring,
                          struct uv__io_uring_cqe** cqe_ptr);
/* Advances the CQ head. This must be called after uv__io_uring_get_cqe. */
void uv__io_uring_advance_cq(struct uv__io_uring* ring);

#endif // ndef UV_UNIX_LINUX_IOURING_
