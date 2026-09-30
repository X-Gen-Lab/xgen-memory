/**
 * \file            allocator.h
 * \brief           Context-aware allocation without an implicit heap backend
 * \author          X-Gen Lab
 */

#ifndef XGM_ALLOCATOR_H
#define XGM_ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *ctx;
    void *(*alloc)(void *ctx, size_t size);
    void (*free)(void *ctx, void *ptr);
} xgm_allocator_t;

/**
 * \brief           Check that an explicit allocator provides both callbacks
 * \param[in]       allocator: Allocator descriptor, or NULL
 * \return          true when allocation and release callbacks are present
 */
bool xgm_allocator_is_valid(const xgm_allocator_t *allocator);

/* No implicit default allocator. Both callbacks must be present.
 * alloc(0) returns NULL; free(NULL) does nothing. Successful allocations must
 * satisfy max_align_t. The descriptor and ctx outlive their allocations.
 * Free a block with the same allocator that allocated it. */
/**
 * \brief           Allocate through the explicitly provided allocator
 * \param[in]       allocator: Allocator descriptor with both callbacks
 *                  present
 * \param[in]       size: Requested number of bytes; zero returns NULL
 * \return          Aligned block, or NULL if size is zero or allocation fails
 */
void *xgm_alloc(const xgm_allocator_t *allocator, size_t size);
/**
 * \brief           Release through the same allocator that supplied the block
 * \param[in]       allocator: Allocator descriptor with both callbacks
 *                  present
 * \param[in,out]   ptr: Block pointer, or NULL
 */
void xgm_free(const xgm_allocator_t *allocator, void *ptr);

#ifdef __cplusplus
}
#endif
#endif
