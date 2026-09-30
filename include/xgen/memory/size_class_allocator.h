/**
 * \file            size_class_allocator.h
 * \brief           Caller-owned, bounded allocator with fixed size classes
 * \author          X-Gen Lab
 */

#ifndef XGM_SIZE_CLASS_ALLOCATOR_H
#define XGM_SIZE_CLASS_ALLOCATOR_H

#include <xgen/memory/allocator.h>
#include <xgen/memory/pool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Capacity of one size class before alignment rounding
 */
typedef struct {
    size_t block_size;
    size_t block_count;
} xgm_size_class_spec_t;

/**
 * \brief           Allocator service and caller-owned pool descriptors
 */
typedef struct {
    xgm_allocator_t service;
    xgm_pool_t *pools;
    size_t pool_count;
} xgm_size_class_allocator_t;

/**
 * \brief           Measure the block storage required by fixed size classes
 * \param[in]       specs: Nonzero block sizes and counts; order is
 *                  unrestricted
 * \param[in]       count: Number of specifications; repeated sizes are
 *                  allowed
 * \param[out]      storage_size: Required block storage bytes
 * \param[out]      storage_alignment: Required alignment for caller storage
 * \return          XGS_OK, XGS_INVALID_ARGUMENT, or XGS_CAPACITY on overflow
 * \note            Block sizes round up to max_align_t. Output pointers must
 *                  be distinct. State and pool descriptors are excluded.
 *                  Failure leaves both outputs unchanged.
 */
xgs_status_t xgm_size_class_measure(const xgm_size_class_spec_t *specs,
                                    size_t count, size_t *storage_size,
                                    size_t *storage_alignment);

/**
 * \brief           Initialize fixed pools and the context-aware allocator
 *                  service
 * \param[out]      state: Caller-owned allocator state
 * \param[out]      pools: Caller-owned pool descriptors, one per
 *                  specification
 * \param[in]       pool_capacity: Available descriptor count
 * \param[in]       specs: Requested block sizes and counts
 * \param[in]       count: Number of specifications
 * \param[in,out]   storage: Block storage with the measured alignment
 * \param[in]       storage_size: Available block storage bytes
 * \return          XGS_OK, XGS_INVALID_ARGUMENT, or XGS_CAPACITY
 * \note            State, pools, specs, and storage must be separate valid
 *                  objects. Specs may be discarded after success. State,
 *                  pools, and storage must not move while initialized.
 *                  Failure leaves caller objects unchanged. Calls require
 *                  serialization.
 */
xgs_status_t xgm_size_class_init(xgm_size_class_allocator_t *state,
                                 xgm_pool_t *pools, size_t pool_capacity,
                                 const xgm_size_class_spec_t *specs,
                                 size_t count, void *storage,
                                 size_t storage_size);

/**
 * \brief           Allocate from the smallest rounded class that fits the
 *                  request
 * \param[in,out]   state: Initialized allocator
 * \param[in]       size: Requested bytes; zero is rejected
 * \return          Aligned block, or NULL for zero size or exhausted capacity
 * \note            Allocation never spills into a larger class when all pools
 *                  of the smallest fitting size are full. No per-block
 *                  header.
 */
void *xgm_size_class_alloc(xgm_size_class_allocator_t *state, size_t size);
/**
 * \brief           Return a block to its owning pool
 * \param[in,out]   state: Initialized allocator
 * \param[in]       ptr: Previously allocated block, or NULL
 * \return          XGS_OK, XGS_INVALID_ARGUMENT, or XGS_ALREADY_EXISTS
 * \note            Foreign and interior pointers are invalid; double frees
 *                  are rejected. The generic service callback discards this
 *                  status.
 */
xgs_status_t xgm_size_class_free(xgm_size_class_allocator_t *state, void *ptr);
/**
 * \brief           Invalidate allocator and descriptors once all blocks are
 *                  free
 * \param[in,out]   state: Initialized allocator
 * \return          XGS_OK, XGS_BUSY for live blocks, or XGS_INVALID_ARGUMENT
 * \note            Does not erase block storage. Busy state remains
 *                  unchanged.
 */
xgs_status_t xgm_size_class_deinit(xgm_size_class_allocator_t *state);

#ifdef __cplusplus
}
#endif
#endif
