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
#include <cstddef>
#endif

/* MSVC's C11 frontend omits max_align_t from its C standard headers. */
#if defined(__cplusplus)
/**
 * \brief           Standard maximum scalar alignment for the selected C++ ABI.
 */
typedef std::max_align_t xgm_max_align_t;
#elif defined(_MSC_VER)
/**
 * \brief           Maximum scalar alignment for the supported MSVC C ABI.
 */
typedef union {
    long double floating; /**< Maximum floating-point alignment. */
    long long integer;    /**< Maximum integer alignment. */
    void* pointer;        /**< Object pointer alignment. */
} xgm_max_align_t;
#else
/**
 * \brief           Standard maximum scalar alignment for the selected C/C++
 *                  ABI.
 */
typedef max_align_t xgm_max_align_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Explicit allocation service; callbacks are synchronous.
 * \note            Caller supplies synchronization and verifies backend ISR
 *                  suitability. No default backend or global state exists.
 *                  Backend context and descriptor outlive allocations.
 *                  Successful blocks satisfy max_align_t and must be returned
 *                  exactly once to the same service. Backend determines time
 *                  bounds.
 */
typedef struct {
    void* ctx; /**< Borrowed context passed unchanged to callbacks. */
    void* (*alloc)(void* ctx,
                   size_t size); /**< Nonzero byte allocation callback. */
    void (*free)(void* ctx, void* ptr); /**< Live-block release callback. */
} xgm_allocator_t;

/**
 * \brief           Check that an explicit allocator provides both callbacks
 * \param[in]       allocator: Allocator descriptor, or NULL
 * \return          true when allocation and release callbacks are present
 */
bool xgm_allocator_is_valid(const xgm_allocator_t* allocator);

/**
 * \brief           Allocate through the explicitly provided allocator
 * \param[in]       allocator: Allocator descriptor with both callbacks present
 * \param[in]       size: Requested number of bytes; zero returns NULL
 * \return          Aligned block, or NULL if size is zero or allocation fails
 */
void* xgm_alloc(const xgm_allocator_t* allocator, size_t size);
/**
 * \brief           Release through the same allocator that supplied the block
 * \param[in]       allocator: Allocator descriptor with both callbacks present
 * \param[in,out]   ptr: Block pointer, or NULL
 */
void xgm_free(const xgm_allocator_t* allocator, void* ptr);

#ifdef __cplusplus
}
#endif
#endif
