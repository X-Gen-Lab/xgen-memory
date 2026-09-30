/**
 * \file            tracking_allocator.h
 * \brief           Context-aware allocation tracking with caller-defined
 *                  phases
 * \author          X-Gen Lab
 */

#ifndef XGM_TRACKING_ALLOCATOR_H
#define XGM_TRACKING_ALLOCATOR_H

#include <xgen/memory/allocator.h>
#include <xgen/status/status.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           Requested-byte allocation counters
 */
typedef struct {
    size_t total_allocated;   /**< Cumulative requested bytes */
    size_t total_freed;       /**< Cumulative released bytes */
    size_t current_allocated; /**< Bytes still owned by callers */
    size_t peak_allocated;    /**< Highest current allocation count in bytes */
    size_t alloc_count;       /**< Successful allocations */
    size_t free_count;        /**< Successful releases */
} xgm_allocator_stats_t;

/**
 * \brief           Tracking service borrowing caller-owned statistics storage
 * \note            The state, statistics and backend context must outlive all
 *                  allocations. Do not copy or reinitialize an active state.
 *                  Operations require external synchronization when shared.
 */
typedef struct {
    xgm_allocator_t service; /**< Interface returned to allocation users */
    xgm_allocator_t
        underlying; /**< Explicit backend copied at initialization */
    xgm_allocator_stats_t *stats; /**< Required aggregate statistics */
    xgm_allocator_stats_t
        *phases;          /**< Optional caller-defined phase counters */
    size_t phase_count;   /**< Number of available phase counters */
    size_t current_phase; /**< Phase assigned to the next allocation */
} xgm_tracking_allocator_t;

/**
 * \brief           Initialize an allocation tracker without allocating
 *                  storage
 * \param[out]      tracker: Tracker state, disjoint from all input objects
 * \param[in]       underlying: Explicit maximum-alignment allocation backend
 * \param[out]      stats: Aggregate counters cleared during initialization
 * \param[out]      phases: Optional phase counter array, cleared on success
 * \param[in]       phase_count: Number of phase entries; zero disables phases
 * \return          XGS_OK on success, XGS_INVALID_ARGUMENT for invalid inputs
 * \note            The initial phase is zero. All statistics exclude the
 *                  private tracking header stored before each returned
 *                  allocation.
 */
xgs_status_t xgm_tracking_allocator_init(xgm_tracking_allocator_t *tracker,
                                         const xgm_allocator_t *underlying,
                                         xgm_allocator_stats_t *stats,
                                         xgm_allocator_stats_t *phases,
                                         size_t phase_count);

/**
 * \brief           Allocate a maximally aligned block and record its phase
 * \param[in,out]   tracker: Initialized tracker state
 * \param[in]       size: Requested bytes; zero and overflowing sizes fail
 * \return          Allocated block, or NULL when the request cannot be
 *                  satisfied
 */
void *xgm_tracking_alloc(xgm_tracking_allocator_t *tracker, size_t size);

/**
 * \brief           Release a tracked block and charge its original phase
 * \param[in,out]   tracker: Tracker that allocated the block
 * \param[in]       ptr: Live tracked block, or NULL
 * \note            Only pointers from this tracking API may be supplied. A
 *                  live block belonging to another tracker is ignored.
 */
void xgm_tracking_free(xgm_tracking_allocator_t *tracker, void *ptr);

/**
 * \brief           Select the phase assigned to subsequent allocations
 * \param[in,out]   tracker: Initialized tracker state
 * \param[in]       phase: Index in the caller-provided phase array
 * \return          XGS_OK on success, XGS_INVALID_ARGUMENT for an invalid
 *                  index
 */
xgs_status_t xgm_tracking_allocator_set_phase(xgm_tracking_allocator_t *tracker,
                                              size_t phase);

/**
 * \brief           Reset counters while preserving outstanding allocation
 *                  bytes
 * \param[in,out]   tracker: Initialized tracker state, or NULL
 * \note            Totals and peaks restart at current outstanding bytes.
 */
void xgm_tracking_allocator_reset_stats(xgm_tracking_allocator_t *tracker);

/**
 * \brief           Obtain the context-aware allocation interface
 * \param[in,out]   tracker: Initialized tracker state, or NULL
 * \return          Borrowed interface descriptor, or NULL for invalid state
 */
const xgm_allocator_t *
xgm_tracking_allocator_get_interface(xgm_tracking_allocator_t *tracker);

#ifdef __cplusplus
}
#endif
#endif /* XGM_TRACKING_ALLOCATOR_H */
