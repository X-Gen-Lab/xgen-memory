/**
 * \file            allocator.c
 * \brief           Explicit libc heap allocator backend
 * \author          X-Gen Lab
 */

#include <xgen/memory/libc_allocator.h>

#include <stdlib.h>

static void *libc_alloc(void *ctx, size_t size)
{
    (void) ctx;
    return size == 0U ? NULL : malloc(size);
}

static void libc_free(void *ctx, void *ptr)
{
    (void) ctx;
    free(ptr);
}

const xgm_allocator_t *xgm_allocator_libc(void)
{
    static const xgm_allocator_t allocator = {NULL, libc_alloc, libc_free};
    return &allocator;
}
