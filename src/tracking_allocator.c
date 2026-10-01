/**
 * \file            tracking_allocator.c
 * \brief           Allocation headers and phase accounting for explicit
 *                  backends
 * \author          X-Gen Lab
 */

#include <xgen/memory/tracking_allocator.h>

#include <stdint.h>

#define XGM_TRACKING_MAGIC UINT32_C(0xA110CA7E)

/**
 * \brief           Aligned private prefix placed before returned storage
 */
typedef union {
    xgm_max_align_t alignment;

    struct {
        xgm_tracking_allocator_t* owner;
        size_t size;
        size_t phase;
        uint32_t magic;
    } value;
} xgm_tracking_header_t;

static bool overlaps(const void* left, size_t left_size, const void* right,
                     size_t right_size) {
    if (left_size == 0U || right_size == 0U) {
        return false;
    }
    uintptr_t a = (uintptr_t)left;
    uintptr_t b = (uintptr_t)right;
    return a <= b ? b - a < left_size : a - b < right_size;
}

static size_t saturated_add(xgm_allocator_stats_t* stats, size_t value,
                            size_t increment) {
    if (increment > SIZE_MAX - value) {
        stats->saturated = true;
        return SIZE_MAX;
    }
    return value + increment;
}

/**
 * \brief           Update a counter set after a successful allocation
 * \param[in,out]   stats: Counter set to update
 * \param[in]       size: Number of requested bytes
 */
static void record_alloc(xgm_allocator_stats_t* stats, size_t size) {
    stats->total_allocated = saturated_add(stats, stats->total_allocated, size);
    stats->current_allocated += size;
    stats->alloc_count = saturated_add(stats, stats->alloc_count, 1U);
    ++stats->live_blocks;
    if (stats->current_allocated > stats->peak_allocated) {
        stats->peak_allocated = stats->current_allocated;
    }
}

/**
 * \brief           Update a counter set after releasing an allocation
 * \param[in,out]   stats: Counter set to update
 * \param[in]       size: Number of requested bytes originally allocated
 */
static void record_free(xgm_allocator_stats_t* stats, size_t size) {
    stats->total_freed = saturated_add(stats, stats->total_freed, size);
    stats->current_allocated -= size;
    stats->free_count = saturated_add(stats, stats->free_count, 1U);
    --stats->live_blocks;
}

/**
 * \brief           Preserve live bytes while clearing historical counters
 * \param[in,out]   stats: Counter set to reset
 */
static void reset_stats(xgm_allocator_stats_t* stats) {
    stats->total_allocated = stats->current_allocated;
    stats->total_freed = 0U;
    stats->peak_allocated = stats->current_allocated;
    stats->alloc_count = 0U;
    stats->free_count = 0U;
    stats->saturated = false;
}

/**
 * \brief           Adapt the allocator callback to a tracking state
 * \param[in,out]   ctx: Tracking state
 * \param[in]       size: Requested byte count
 * \return          Tracked allocation, or NULL on failure
 */
static void* tracking_alloc(void* ctx, size_t size) {
    return xgm_tracking_alloc(ctx, size);
}

/**
 * \brief           Adapt the release callback to a tracking state
 * \param[in,out]   ctx: Tracking state
 * \param[in]       ptr: Live tracked allocation, or NULL
 */
static void tracking_free(void* ctx, void* ptr) {
    xgm_tracking_free(ctx, ptr);
}

xgs_status_t xgm_tracking_allocator_init(xgm_tracking_allocator_t* tracker,
                                         const xgm_allocator_t* underlying,
                                         xgm_allocator_stats_t* stats,
                                         xgm_allocator_stats_t* phases,
                                         size_t phase_count) {
    if (tracker == NULL || underlying == NULL || underlying->alloc == NULL ||
        underlying->free == NULL || stats == NULL ||
        (phase_count != 0U && phases == NULL) ||
        phase_count > SIZE_MAX / sizeof(*phases)) {
        return XGS_INVALID_ARGUMENT;
    }
    size_t phase_bytes = phase_count * sizeof(*phases);
    if (overlaps(stats, sizeof(*stats), phases, phase_bytes) ||
        overlaps(tracker, sizeof(*tracker), stats, sizeof(*stats)) ||
        overlaps(tracker, sizeof(*tracker), phases, phase_bytes) ||
        overlaps(underlying, sizeof(*underlying), stats, sizeof(*stats)) ||
        overlaps(underlying, sizeof(*underlying), phases, phase_bytes) ||
        overlaps(tracker, sizeof(*tracker), underlying, sizeof(*underlying))) {
        return XGS_INVALID_ARGUMENT;
    }
    *stats = (xgm_allocator_stats_t){0};
    for (size_t i = 0U; i < phase_count; ++i) {
        phases[i] = (xgm_allocator_stats_t){0};
    }
    *tracker =
        (xgm_tracking_allocator_t){{tracker, tracking_alloc, tracking_free},
                                   *underlying,
                                   stats,
                                   phases,
                                   phase_count,
                                   0U};
    return XGS_OK;
}

void* xgm_tracking_alloc(xgm_tracking_allocator_t* tracker, size_t size) {
    if (tracker == NULL || tracker->stats == NULL || size == 0U ||
        size > SIZE_MAX - sizeof(xgm_tracking_header_t) ||
        size > SIZE_MAX - tracker->stats->current_allocated ||
        tracker->stats->live_blocks == SIZE_MAX) {
        return NULL;
    }
    if (tracker->phase_count != 0U &&
        (size > SIZE_MAX -
                    tracker->phases[tracker->current_phase].current_allocated ||
         tracker->phases[tracker->current_phase].live_blocks == SIZE_MAX)) {
        return NULL;
    }
    xgm_tracking_header_t* header =
        xgm_alloc(&tracker->underlying, sizeof(*header) + size);
    if (header == NULL) {
        return NULL;
    }
    header->value.owner = tracker;
    header->value.size = size;
    header->value.phase = tracker->current_phase;
    header->value.magic = XGM_TRACKING_MAGIC;
    record_alloc(tracker->stats, size);
    if (tracker->phase_count != 0U) {
        record_alloc(&tracker->phases[tracker->current_phase], size);
    }
    return header + 1;
}

void xgm_tracking_free(xgm_tracking_allocator_t* tracker, void* ptr) {
    if (tracker == NULL || tracker->stats == NULL || ptr == NULL) {
        return;
    }
    xgm_tracking_header_t* header = (xgm_tracking_header_t*)ptr - 1;
    if (header->value.magic != XGM_TRACKING_MAGIC ||
        header->value.owner != tracker) {
        return;
    }
    record_free(tracker->stats, header->value.size);
    if (header->value.phase < tracker->phase_count) {
        record_free(&tracker->phases[header->value.phase], header->value.size);
    }
    header->value.magic = 0U;
    xgm_free(&tracker->underlying, header);
}

xgs_status_t xgm_tracking_allocator_set_phase(xgm_tracking_allocator_t* tracker,
                                              size_t phase) {
    if (tracker == NULL || tracker->stats == NULL ||
        phase >= tracker->phase_count) {
        return XGS_INVALID_ARGUMENT;
    }
    tracker->current_phase = phase;
    return XGS_OK;
}

void xgm_tracking_allocator_reset_stats(xgm_tracking_allocator_t* tracker) {
    if (tracker == NULL || tracker->stats == NULL) {
        return;
    }
    reset_stats(tracker->stats);
    for (size_t i = 0U; i < tracker->phase_count; ++i) {
        reset_stats(&tracker->phases[i]);
    }
}

const xgm_allocator_t*
xgm_tracking_allocator_get_interface(xgm_tracking_allocator_t* tracker) {
    return tracker != NULL && tracker->stats != NULL ? &tracker->service : NULL;
}

size_t xgm_tracking_overhead(void) {
    return sizeof(xgm_tracking_header_t);
}

xgs_status_t xgm_tracking_allocator_deinit(xgm_tracking_allocator_t* tracker) {
    if (tracker == NULL || tracker->stats == NULL) {
        return XGS_INVALID_ARGUMENT;
    }
    if (tracker->stats->live_blocks != 0U) {
        return XGS_BUSY;
    }
    *tracker = (xgm_tracking_allocator_t){0};
    return XGS_OK;
}
