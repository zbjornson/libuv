#include "linux-syscalls.h"
#include "linux-iouring.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#if defined(__x86_64) || defined(__i386__)
#define mem_barrier() __asm__ __volatile__("":::"memory")
#else
/* Full barrier for other archs. */
#define mem_barrier() __sync_synchronize()
#endif

int uv__io_uring_init(unsigned entries, struct uv__io_uring* ring) {
  struct uv__io_uring_params params;
  int fd;
  char* sq_ptr;
  char* cq_ptr;
  size_t sqe_map_size;

  memset(&params, 0, sizeof(params));

  fd = uv__io_uring_setup(entries, &params);
  if (fd < 0) {
    return -fd;
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
    close(fd);
    return errno;
  }

  ring->sq.head = (unsigned*)(sq_ptr + params.sq_off.head);
  ring->sq.tail = (unsigned*)(sq_ptr + params.sq_off.tail);
  ring->sq.ring_mask = (unsigned*)(sq_ptr + params.sq_off.ring_mask);
  ring->sq.ring_entries = (unsigned*)(sq_ptr + params.sq_off.ring_entries);
  ring->sq.flags = (unsigned*)(sq_ptr + params.sq_off.flags);
  ring->sq.dropped = (unsigned*)(sq_ptr + params.sq_off.dropped);
  ring->sq.array = (unsigned*)(sq_ptr + params.sq_off.array);

  /* Map SQEs */
  sqe_map_size = params.sq_entries * sizeof(struct uv__io_uring_sqe);
  ring->sq.sqes = mmap(NULL,
                       sqe_map_size,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_POPULATE,
                       fd,
                       UV__IORING_OFF_SQES);

  if (ring->sq.sqes == MAP_FAILED) {
    close(fd);
    munmap(ring->sq.head, ring->sq.map_sz);
    return errno;
  }

  /* Map CQ */
  ring->cq.map_sz = params.cq_off.cqes + params.cq_entries *
      sizeof(struct uv__io_uring_cqe);
  cq_ptr = mmap(NULL,
                ring->cq.map_sz,
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_POPULATE,
                fd,
                UV__IORING_OFF_CQ_RING);

  if (cq_ptr == MAP_FAILED) { 
    close(fd);
    munmap(ring->sq.head, ring->sq.map_sz);
    munmap(ring->sq.sqes, sqe_map_size);
    return errno;
  }

  ring->cq.head = (unsigned*)(cq_ptr + params.cq_off.head);
  ring->cq.tail = (unsigned*)(cq_ptr + params.cq_off.tail);
  ring->cq.ring_mask = (unsigned*)(cq_ptr + params.cq_off.ring_mask);
  ring->cq.ring_entries = (unsigned*)(cq_ptr + params.cq_off.ring_entries);
  ring->cq.overflow = (unsigned*)(cq_ptr + params.cq_off.overflow);

  return 0;
}

void uv__io_uring_destroy(struct uv__io_uring* ring) {
  munmap(ring->sq.head, ring->sq.map_sz);
  munmap(ring->sq.sqes, *ring->sq.ring_entries * sizeof(struct uv__io_uring_sqe));
  munmap(ring->cq.head, ring->cq.map_sz);
  close(ring->ring_fd);
}

struct uv__io_uring_sqe* uv__io_uring_get_sqe(struct uv__io_uring* ring) {
  struct uv__io_uring_sqe* sqe;
  unsigned idx;

  // if (ring->sq.sqe_tail - ring->sq.sqe_head == *ring->sq.ring_entries) {
  if (*ring->sq.tail - *ring->sq.head == *ring->sq.ring_entries) {
    /* SQ is full */
    return NULL;
  }

  // idx = ring->sq.sqe_tail & *ring->sq.ring_mask;
  idx = *ring->sq.tail & *ring->sq.ring_mask;
  sqe = &ring->sq.sqes[idx];
  // ring->sq.sqe_tail += 1;

  return sqe;
}

int uv__io_uring_submit(struct uv__io_uring* ring) {
  unsigned idx;

  idx = *ring->sq.tail & *ring->sq.ring_mask;
  ring->sq.array[idx] = idx;
  mem_barrier();

  ring->sq.tail++;
  mem_barrier();

  return uv__io_uring_enter(ring->ring_fd, 1, 0, 0, NULL);
}

void uv__io_uring_get_cqe(struct uv__io_uring* ring, struct uv__io_uring_cqe** cqe_ptr) {
  unsigned head;

  head = *ring->cq.head;
  mem_barrier();
  if (head != *ring->cq.tail) {
    *cqe_ptr = &ring->cq.cqes[head & *ring->cq.ring_mask];
    ring->cq.head++;
    mem_barrier();
  }
}
