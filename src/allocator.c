/**
 * \file            allocator.c
 * \brief           Context-aware allocation without an implicit heap backend
 * \author          X-Gen Lab
 */

#include <xgen/memory/allocator.h>

bool xgm_allocator_is_valid(const xgm_allocator_t* allocator) {
    return allocator != NULL && allocator->alloc != NULL &&
           allocator->free != NULL;
}

void* xgm_alloc(const xgm_allocator_t* allocator, size_t size) {
    if (!xgm_allocator_is_valid(allocator) || size == 0U) {
        return NULL;
    }
    return allocator->alloc(allocator->ctx, size);
}

void xgm_free(const xgm_allocator_t* allocator, void* ptr) {
    if (xgm_allocator_is_valid(allocator) && ptr != NULL) {
        allocator->free(allocator->ctx, ptr);
    }
}
