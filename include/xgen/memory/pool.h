/**
 * \file            pool.h
 * \brief           Caller-owned fixed block pool with bounded capacity
 * \author          X-Gen Lab
 */

#ifndef XGM_POOL_H
#define XGM_POOL_H

#include <xgen/memory/allocator.h>
#include <xgen/status/status.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *storage;
    void *free_list;
    size_t block_size;
    size_t block_count;
    size_t free_count;
    size_t peak_used;
} xgm_pool_t;

/* Caller owns pool and storage; neither may move while initialized.
 * alignment must be a nonzero power of two, storage must be aligned, and
 * block_size must be a multiple of alignment and at least sizeof(void *).
 * Exactly count blocks are used. No allocation or implicit size rounding.
 * Failure leaves *pool unchanged. Do not reinitialize a pool with live blocks.
 * Calls require external serialization. */
/**
 * \brief           Initialize exactly count fixed blocks in caller storage
 * \param[in,out]   pool: Caller-owned pool descriptor
 * \param[in,out]   storage: Caller-owned block storage with the requested
 *                  alignment
 * \param[in]       storage_size: Available storage bytes
 * \param[in]       block_size: Bytes per block, at least sizeof(void *)
 * \param[in]       alignment: Nonzero power-of-two storage and block
 *                  alignment
 * \param[in]       count: Number of blocks
 * \return          XGS_OK on success; a status code on validation or capacity
 *                  failure
 */
xgs_status_t xgm_pool_init(xgm_pool_t *pool, void *storage, size_t storage_size,
                           size_t block_size, size_t alignment, size_t count);
/**
 * \brief           Invalidate the pool descriptor without releasing storage
 * \param[in,out]   pool: Caller-owned pool descriptor
 */
void xgm_pool_deinit(xgm_pool_t *pool);
/**
 * \brief           Remove one block from the bounded free list
 * \param[in,out]   pool: Caller-owned pool descriptor
 * \return          Matching object, or NULL when absent or unavailable
 */
void *xgm_pool_alloc(xgm_pool_t *pool);
/* NULL is accepted. Foreign, interior, or already-free blocks are rejected.
 * Double-free detection scans the bounded free list. */
/**
 * \brief           Return a block and reject invalid pointers or double frees
 * \param[in,out]   pool: Caller-owned pool descriptor
 * \param[in,out]   ptr: Block pointer, or NULL
 * \return          XGS_OK on success; a status code on validation or capacity
 *                  failure
 */
xgs_status_t xgm_pool_free(xgm_pool_t *pool, void *ptr);
/**
 * \brief           Check whether a pointer is a block boundary in this pool
 * \param[in]       pool: Caller-owned pool descriptor
 * \param[in]       ptr: Block pointer, or NULL
 * \return          true when the condition holds, otherwise false
 */
bool xgm_pool_contains(const xgm_pool_t *pool, const void *ptr);
/**
 * \brief           Query the number of available blocks
 * \param[in]       pool: Caller-owned pool descriptor
 * \return          Calculated or queried value
 */
size_t xgm_pool_free_count(const xgm_pool_t *pool);
/**
 * \brief           Query the number of live blocks
 * \param[in]       pool: Caller-owned pool descriptor
 * \return          Calculated or queried value
 */
size_t xgm_pool_used_count(const xgm_pool_t *pool);
/**
 * \brief           Query the peak simultaneous block use
 * \param[in]       pool: Caller-owned pool descriptor
 * \return          Calculated or queried value
 */
size_t xgm_pool_peak_used(const xgm_pool_t *pool);
/**
 * \brief           Reset peak use to the current live block count
 * \param[in,out]   pool: Caller-owned pool descriptor
 */
void xgm_pool_reset_stats(xgm_pool_t *pool);

/**
 * \brief           Create an explicit allocator descriptor borrowing a pool
 * \param[in,out]   pool: Pool whose lifetime covers the descriptor and blocks
 * \return          Context-aware service; oversized or zero requests fail
 * \note            Align the pool to max_align_t before using this service.
 */
xgm_allocator_t xgm_pool_allocator(xgm_pool_t *pool);

#ifdef __cplusplus
}
#endif
#endif
