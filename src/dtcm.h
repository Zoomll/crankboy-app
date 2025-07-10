#pragma once

#include <stdbool.h>
#include <stdio.h>

extern void* dtcm_mempool;
extern bool is_dtcm_init;

void dtcm_set_mempool(void* addr);
void dtcm_init(void);
void dtcm_deinit(void);
bool dtcm_verify(const char* context);

// TODO: dtcm_free, which has no effect if allocated on dtcm, but if address is outside of dtcm
// region then does normal free

void* dtcm_alloc(size_t size);
void* dtcm_alloc_aligned(size_t size, size_t offset);
void dtcm_free(void* ptr);

struct dtcm_store_t;

// copies dtcm region to a buffer outside of dtcm.
// use this before an operation which might destroy dtcm.
struct dtcm_store_t* dtcm_store(void);

// restores from above, and invalidates the store
void dtcm_restore(struct dtcm_store_t*);

#define DTCM_VERIFY__(f, l) dtcm_verify(f ":" #l)
#define DTCM_VERIFY_(f, l) DTCM_VERIFY__(f, l)
#define DTCM_VERIFY() DTCM_VERIFY_(__FILE__, __LINE__)
#if DTCM_DEBUG
#define DTCM_VERIFY_DEBUG() DTCM_VERIFY()
#else
#define DTCM_VERIFY_DEBUG() 1
#endif

// true if dtcm_init called and DTCM_ALLOC enabled
static inline bool dtcm_enabled(void)
{
#ifndef DTCM_ALLOC
    return false;
#endif
    return is_dtcm_init;
}
