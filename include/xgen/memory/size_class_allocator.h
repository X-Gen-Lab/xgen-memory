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

/** \brief Exhaustion policy for the smallest fitting active class. */
typedef enum {
    XGM_SIZE_CLASS_STRICT = 0, /**< Preserve capacity in larger classes. */
    XGM_SIZE_CLASS_FALLBACK = 1 /**< Borrow the smallest larger available class. */
} xgm_size_class_policy_t;

/** \brief Separate caller-owned storage for one class; zero count disables it. */
typedef struct {
    void* storage;       /**< Aligned writable storage; NULL for an empty class. */
    size_t storage_size; /**< Available storage bytes. */
    size_t block_size;   /**< Requested size before maximum-alignment rounding. */
    size_t block_count;  /**< Block count; zero disables the class. */
} xgm_size_class_buffer_t;

/**
 * \brief           Allocator service and caller-owned pool descriptors
 */
typedef struct {
    xgm_allocator_t service;
    xgm_pool_t *pools;
    size_t pool_count;
    xgm_size_class_policy_t policy; /**< Exhaustion behavior. */
    xgm_allocator_t backend; /**< Copied backend for owned storage only. */
    void* owned_storage; /**< One owned allocation, or NULL for borrowed storage. */
    size_t used_memory; /**< Current reserved block bytes, including rounding. */
    size_t peak_memory; /**< Maximum simultaneous reserved block bytes. */
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
 * \brief Initialize contiguous caller-owned storage with an explicit policy.
 * \param[out] state: Stable caller-owned state, disjoint from all other objects.
 * \param[out] pools: Stable descriptor array, disjoint from specs and storage.
 * \param[in] pool_capacity: Available descriptor count.
 * \param[in] specs: Unordered class specifications; duplicates and zero counts
 * are accepted. A disabled class ignores block_size.
 * \param[in] count: Nonzero number of specifications.
 * \param[in,out] storage: Maximum-aligned storage; NULL if all counts are zero.
 * \param[in] storage_size: Available bytes.
 * \param[in] policy: Strict or fallback allocation behavior.
 * \return XGS_OK, XGS_INVALID_ARGUMENT, or XGS_CAPACITY.
 * \note Failure changes no caller objects. Do not reinitialize live state.
 * No heap, locks, callbacks or blocking are used. Serialize all operations.
 */
xgs_status_t xgm_size_class_init_ex(xgm_size_class_allocator_t* state,
    xgm_pool_t* pools, size_t pool_capacity, const xgm_size_class_spec_t* specs,
    size_t count, void* storage, size_t storage_size, xgm_size_class_policy_t policy);

/**
 * \brief Initialize independent borrowed buffers using the shared class engine.
 * \param[out] state: Stable state disjoint from descriptors and buffers.
 * \param[out] pools: Stable descriptor array disjoint from input objects.
 * \param[in] pool_capacity: Available descriptor count.
 * \param[in] buffers: Per-class storage; used spans must not overlap.
 * \param[in] count: Nonzero number of buffer descriptors.
 * \param[in] policy: Strict or fallback allocation behavior.
 * \return XGS_OK, XGS_INVALID_ARGUMENT, or XGS_CAPACITY.
 * \note Inputs are borrowed; descriptors may be discarded after success.
 * Validation is O(count squared), initialization O(total blocks); allocation
 * is O(count), release is O(count + owning pool blocks). Failure is atomic.
 */
xgs_status_t xgm_size_class_init_buffers(xgm_size_class_allocator_t* state,
    xgm_pool_t* pools, size_t pool_capacity, const xgm_size_class_buffer_t* buffers,
    size_t count, xgm_size_class_policy_t policy);

/**
 * \brief Acquire one backing allocation and initialize the same class engine.
 * \param[out] state: Stable state disjoint from all input objects.
 * \param[out] pools: Caller-owned stable descriptor array.
 * \param[in] pool_capacity: Available descriptor count.
 * \param[in] specs: Class specifications; zero counts are disabled.
 * \param[in] count: Nonzero specification count.
 * \param[in] backend: Explicit allocator copied on success; NULL is accepted
 * only when all classes are disabled. Context must outlive state.
 * \param[in] policy: Strict or fallback allocation behavior.
 * \return XGS_OK, XGS_INVALID_ARGUMENT, XGS_CAPACITY, or XGS_NO_MEMORY.
 * \note Initialization calls backend at most once, rolls back on failure, and
 * leaves outputs unchanged. Deinit releases owned storage after all blocks
 * are returned. Timing and ISR eligibility also depend on backend callbacks.
 */
xgs_status_t xgm_size_class_init_owned(xgm_size_class_allocator_t* state,
    xgm_pool_t* pools, size_t pool_capacity, const xgm_size_class_spec_t* specs,
    size_t count, const xgm_allocator_t* backend, xgm_size_class_policy_t policy);

/** \brief Query current reserved block bytes, excluding descriptors.
 * \param[in] state: Initialized state, or NULL.
 * \return Reserved bytes, or zero for NULL.
 */
size_t xgm_size_class_used_memory(const xgm_size_class_allocator_t* state);
/** \brief Query simultaneous peak reserved block bytes.
 * \param[in] state: Initialized state, or NULL.
 * \return Peak bytes, or zero for NULL; not the sum of per-pool peaks.
 */
size_t xgm_size_class_peak_memory(const xgm_size_class_allocator_t* state);
/** \brief Reset aggregate and per-pool peaks to their current use.
 * \param[in,out] state: Initialized state, or NULL.
 */
void xgm_size_class_reset_stats(xgm_size_class_allocator_t* state);

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
