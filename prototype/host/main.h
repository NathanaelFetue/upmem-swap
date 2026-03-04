#ifndef __UPMEM_SWAP_HOST_H__
#define __UPMEM_SWAP_HOST_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* DPU includes (will be available when SDK is accessible) */
#ifdef HAVE_DPU_H
#include <dpu.h>
/* Note: older examples include <dpu_utils.h>. Not all SDKs provide it.
	Guarded out to improve portability. */
#else
#warning "UPMEM SDK not available - using stub definitions"
#define DPU_RANK void*
#endif

/* Configuration */
#define NR_DPUS 1
#define BUFFER_SIZE (8 * 1024 * 1024)  /* 8MB */

/* Function declarations */
void* allocate_swap_buffer(size_t size);
void free_swap_buffer(void* buffer);
int main(int argc, char* argv[]);

#endif /* __UPMEM_SWAP_HOST_H__ */
