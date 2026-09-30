/**
 * \file            test_core.c
 * \brief           Offline regression tests for independent public components
 * \author          X-Gen Lab
 */

#include <xgen/memory/allocator.h>
#include <xgen/memory/pool.h>
#include <xgen/memory/size_class_allocator.h>
#include <xgen/status/status.h>
#include <xgen/memory/tracking_allocator.h>
#ifdef XGM_TEST_LIBC
#include <xgen/memory/libc_allocator.h>
#endif
#include <stdio.h>
#include <string.h>
#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                       \
        }                                                                   \
    } while (0)

typedef struct {
    _Alignas(max_align_t) uint8_t storage[64];
    size_t alloc_calls;
    size_t free_calls;
    bool busy;
} allocator_context_t;

static void *context_alloc(void *ctx, size_t size)
{
    allocator_context_t *context = (allocator_context_t *) ctx;
    ++context->alloc_calls;
    if (context->busy || size > sizeof(context->storage)) {
        return NULL;
    }
    context->busy = true;
    return context->storage;
}

static void context_free(void *ctx, void *ptr)
{
    allocator_context_t *context = (allocator_context_t *) ctx;
    if (ptr == context->storage && context->busy) {
        context->busy = false;
        ++context->free_calls;
    }
}

int baseline_allocator(void)
{
    allocator_context_t first = {0}, second = {0};
    xgm_allocator_t a = {&first, context_alloc, context_free};
    xgm_allocator_t b = {&second, context_alloc, context_free};
    xgm_allocator_t incomplete = {&first, context_alloc, NULL};
    xgm_allocator_t missing_alloc = {&first, NULL, context_free};
    CHECK(!xgm_allocator_is_valid(NULL));
    CHECK(!xgm_allocator_is_valid(&incomplete));
    CHECK(!xgm_allocator_is_valid(&missing_alloc));
    CHECK(xgm_allocator_is_valid(&a));
    CHECK(xgm_allocator_is_valid(&b));
    CHECK(xgm_alloc(NULL, 8U) == NULL);
    CHECK(xgm_alloc(&a, 0U) == NULL);
    CHECK(xgm_alloc(&incomplete, 8U) == NULL);
    CHECK(first.alloc_calls == 0U);
    void *one = xgm_alloc(&a, 32U);
    void *two = xgm_alloc(&b, 32U);
    CHECK(one == first.storage && two == second.storage && one != two);
    CHECK(xgm_alloc(&a, 1U) == NULL);
    CHECK(first.alloc_calls == 2U && second.alloc_calls == 1U);
    xgm_free(&a, NULL);
    CHECK(first.free_calls == 0U);
    xgm_free(&a, one);
    CHECK(!first.busy && second.busy);
    xgm_free(&b, two);
    CHECK(first.free_calls == 1U && second.free_calls == 1U);
#ifdef XGM_TEST_LIBC
    void *allocated = xgm_alloc(xgm_allocator_libc(), sizeof(max_align_t));
    CHECK(allocated != NULL &&
          (uintptr_t) allocated % _Alignof(max_align_t) == 0U);
    xgm_free(xgm_allocator_libc(), allocated);
#endif
    return 0;
}

int baseline_pool(void)
{
    _Alignas(max_align_t) uint8_t storage[256];
    xgm_pool_t pool = {0};
    const size_t block = 2U * sizeof(max_align_t);
    CHECK(xgm_pool_init(&pool, storage, sizeof(storage), block,
                        _Alignof(max_align_t), 4U) == XGS_OK);
    void *allocated[4];
    for (size_t i = 0U; i < 4U; ++i) {
        allocated[i] = xgm_pool_alloc(&pool);
        CHECK(allocated[i] != NULL);
        CHECK((uintptr_t) allocated[i] % _Alignof(max_align_t) == 0U);
        memset(allocated[i], (int) (i + 1U), block);
    }
    CHECK(xgm_pool_alloc(&pool) == NULL && xgm_pool_peak_used(&pool) == 4U);
    CHECK(xgm_pool_free(&pool, storage + 1) == XGS_INVALID_ARGUMENT);
    int foreign = 0;
    CHECK(xgm_pool_free(&pool, &foreign) == XGS_INVALID_ARGUMENT);
    CHECK(xgm_pool_free(&pool, allocated[1]) == XGS_OK);
    CHECK(xgm_pool_free(&pool, allocated[1]) == XGS_ALREADY_EXISTS);
    CHECK(xgm_pool_alloc(&pool) == allocated[1]);
    for (size_t i = 0U; i < 4U; ++i) {
        CHECK(xgm_pool_free(&pool, allocated[i]) == XGS_OK);
    }
    CHECK(xgm_pool_free_count(&pool) == 4U && xgm_pool_used_count(&pool) == 0U);
    xgm_pool_reset_stats(&pool);
    CHECK(xgm_pool_peak_used(&pool) == 0U);
    xgm_allocator_t service = xgm_pool_allocator(&pool);
    CHECK(xgm_allocator_is_valid(&service));
    CHECK(xgm_alloc(&service, 0U) == NULL);
    CHECK(xgm_alloc(&service, block + 1U) == NULL);
    void *service_block = xgm_alloc(&service, block);
    CHECK(service_block != NULL && xgm_pool_used_count(&pool) == 1U);
    xgm_free(&service, service_block);
    CHECK(xgm_pool_used_count(&pool) == 0U);
    xgm_pool_t snapshot = pool;
    CHECK(xgm_pool_init(&pool, storage, sizeof(storage), block,
                        _Alignof(max_align_t), SIZE_MAX) == XGS_CAPACITY);
    CHECK(memcmp(&pool, &snapshot, sizeof(pool)) == 0);
    xgm_pool_deinit(&pool);
    CHECK(xgm_pool_alloc(&pool) == NULL);
    return 0;
}

int baseline_size_class(void)
{
    const size_t alignment = _Alignof(max_align_t);
    xgm_size_class_spec_t specs[] = {
        {alignment * 4U, 1U}, {1U, 1U}, {alignment, 1U}};
    size_t required = 0U, measured_alignment = 0U;
    CHECK(xgm_size_class_measure(specs, 3U, &required, &measured_alignment) ==
          XGS_OK);
    CHECK(required == alignment * 6U && measured_alignment == alignment);
    _Alignas(max_align_t) uint8_t storage[256];
    _Alignas(max_align_t) uint8_t other_storage[256];
    CHECK(required <= sizeof(storage));
    xgm_pool_t pools[3] = {0}, other_pools[3] = {0};
    xgm_size_class_allocator_t allocator = {0}, other = {0};
    CHECK(xgm_size_class_init(&allocator, pools, 3U, specs, 3U, storage,
                              required) == XGS_OK);
    CHECK(xgm_size_class_init(&other, other_pools, 3U, specs, 3U, other_storage,
                              required) == XGS_OK);
    void *small = xgm_alloc(&allocator.service, 1U);
    void *second_small = xgm_alloc(&allocator.service, alignment);
    CHECK(small != NULL && second_small != NULL && small != second_small);
    CHECK((uintptr_t) small % alignment == 0U &&
          (uintptr_t) second_small % alignment == 0U);
    CHECK(xgm_alloc(&allocator.service, 1U) ==
          NULL); /* No larger-class spill. */
    void *large = xgm_alloc(&allocator.service, alignment + 1U);
    CHECK(large != NULL && xgm_pool_contains(&pools[0], large));
    CHECK(xgm_alloc(&allocator.service, alignment * 4U) == NULL);
    CHECK(xgm_alloc(&allocator.service, alignment * 4U + 1U) == NULL);
    CHECK(xgm_alloc(&allocator.service, 0U) == NULL);
    CHECK(xgm_size_class_deinit(&allocator) == XGS_BUSY);
    CHECK(xgm_size_class_free(&allocator, (uint8_t *) small + 1U) ==
          XGS_INVALID_ARGUMENT);
    CHECK(xgm_size_class_free(&other, small) == XGS_INVALID_ARGUMENT);
    void *separate = xgm_alloc(&other.service, 1U);
    CHECK(separate != NULL && separate != small);
    CHECK(xgm_size_class_free(&allocator, separate) == XGS_INVALID_ARGUMENT);
    CHECK(xgm_size_class_free(&allocator, small) == XGS_OK);
    CHECK(xgm_size_class_free(&allocator, small) == XGS_ALREADY_EXISTS);
    CHECK(xgm_alloc(&allocator.service, 1U) == small);
    xgm_free(&allocator.service, small);
    xgm_free(&allocator.service, second_small);
    xgm_free(&allocator.service, large);
    xgm_free(&other.service, separate);
    for (size_t i = 0U; i < 1000U; ++i) {
        void *a = xgm_alloc(&allocator.service, alignment);
        void *b = xgm_alloc(&allocator.service, alignment);
        void *c = xgm_alloc(&allocator.service, alignment * 4U);
        CHECK(a != NULL && b != NULL && c != NULL && a != b);
        memset(a, 0xa5, alignment);
        memset(b, 0x5a, alignment);
        memset(c, 0x3c, alignment * 4U);
        xgm_free(&allocator.service, b);
        xgm_free(&allocator.service, c);
        xgm_free(&allocator.service, a);
    }
    CHECK(xgm_size_class_deinit(&allocator) == XGS_OK);
    CHECK(xgm_size_class_deinit(&other) == XGS_OK);
    CHECK(xgm_alloc(&allocator.service, 1U) == NULL);

    memset(storage, 0x5a, sizeof(storage));
    xgm_size_class_allocator_t before_state = allocator;
    xgm_pool_t before_pools[3];
    memcpy(before_pools, pools, sizeof(pools));
    CHECK(xgm_size_class_init(&allocator, pools, 2U, specs, 3U, storage,
                              required) == XGS_CAPACITY);
    CHECK(xgm_size_class_init(&allocator, pools, 3U, specs, 3U, storage,
                              required - 1U) == XGS_CAPACITY);
    CHECK(xgm_size_class_init(&allocator, pools, 3U, specs, 3U, storage + 1,
                              required) == XGS_INVALID_ARGUMENT);
    CHECK(memcmp(&allocator, &before_state, sizeof(allocator)) == 0);
    CHECK(memcmp(pools, before_pools, sizeof(pools)) == 0);
    for (size_t i = 0U; i < sizeof(storage); ++i) {
        CHECK(storage[i] == 0x5aU);
    }
    xgm_size_class_spec_t invalid = {SIZE_MAX, 1U};
    size_t size_out = 71U, alignment_out = 73U;
    CHECK(xgm_size_class_measure(&invalid, 1U, &size_out, &alignment_out) ==
          XGS_CAPACITY);
    invalid = (xgm_size_class_spec_t) {alignment, SIZE_MAX};
    CHECK(xgm_size_class_measure(&invalid, 1U, &size_out, &alignment_out) ==
          XGS_CAPACITY);
    xgm_size_class_spec_t sum_overflow[] = {{alignment, SIZE_MAX / alignment},
                                            {alignment, 1U}};
    CHECK(xgm_size_class_measure(sum_overflow, 2U, &size_out, &alignment_out) ==
          XGS_CAPACITY);
    invalid = (xgm_size_class_spec_t) {0U, 1U};
    CHECK(xgm_size_class_measure(&invalid, 1U, &size_out, &alignment_out) ==
          XGS_INVALID_ARGUMENT);
    CHECK(size_out == 71U && alignment_out == 73U);
    /* Disabled classes are intentionally accepted by the unified engine. */
    invalid = (xgm_size_class_spec_t) {1U, 0U};
    CHECK(xgm_size_class_measure(&invalid, 1U, &size_out, &alignment_out) ==
          XGS_OK);
    CHECK(size_out == 0U && alignment_out == alignment);
    return 0;
}

/**
 * \brief           Aligned test blocks supplied without a libc allocator
 */
typedef union {
    max_align_t alignment;
    uint8_t bytes[2048];
} memory_test_block_t;

/**
 * \brief           Deterministic bounded backend with allocation fault
 *                  injection
 */
typedef struct {
    memory_test_block_t blocks[3];
    bool used[3];
    size_t alloc_calls;
    size_t free_calls;
    size_t invalid_frees;
    size_t fail_on;
} memory_test_backend_t;

/**
 * \brief           Allocate one aligned test block or inject an expected
 *                  failure
 * \param[in,out]   ctx: Bounded backend state
 * \param[in]       size: Requested byte count
 * \return          Available block, or NULL for exhaustion or injected
 *                  failure
 */
static void *memory_test_alloc(void *ctx, size_t size)
{
    memory_test_backend_t *backend = ctx;
    ++backend->alloc_calls;
    if (backend->alloc_calls == backend->fail_on ||
        size > sizeof(backend->blocks[0].bytes)) {
        return NULL;
    }
    for (size_t i = 0U; i < 3U; ++i) {
        if (!backend->used[i]) {
            backend->used[i] = true;
            return backend->blocks[i].bytes;
        }
    }
    return NULL;
}

/**
 * \brief           Return a test block and record invalid release attempts
 * \param[in,out]   ctx: Bounded backend state
 * \param[in]       ptr: Exact block pointer returned by the backend
 */
static void memory_test_free(void *ctx, void *ptr)
{
    memory_test_backend_t *backend = ctx;
    for (size_t i = 0U; i < 3U; ++i) {
        if (ptr == backend->blocks[i].bytes && backend->used[i]) {
            backend->used[i] = false;
            ++backend->free_calls;
            return;
        }
    }
    ++backend->invalid_frees;
}

/**
 * \brief           Verify independent tracking phases, ownership and failure
 *                  paths
 * \return          Zero on success, one when an assertion fails
 */
int baseline_tracking(void)
{
    memory_test_backend_t backend = {0};
    const xgm_allocator_t allocator = {&backend, memory_test_alloc,
                                       memory_test_free};
    xgm_tracking_allocator_t tracker = {0}, other = {0};
    xgm_allocator_stats_t total = {0}, other_total = {0};
    xgm_allocator_stats_t phases[11] = {0};
    CHECK(xgm_tracking_allocator_init(&tracker, NULL, &total, phases, 11U) ==
          XGS_INVALID_ARGUMENT);
    CHECK(xgm_tracking_allocator_init(&tracker, &allocator, &total, NULL, 1U) ==
          XGS_INVALID_ARGUMENT);
    CHECK(xgm_tracking_allocator_init(&tracker, &allocator, &total, phases,
                                      SIZE_MAX / sizeof(*phases) + 1U) ==
          XGS_INVALID_ARGUMENT);
    CHECK(xgm_tracking_allocator_init(&tracker, &allocator, &total, phases,
                                      11U) == XGS_OK);
    CHECK(xgm_tracking_allocator_init(&other, &allocator, &other_total, NULL,
                                      0U) == XGS_OK);
    CHECK(xgm_tracking_alloc(&tracker, 0U) == NULL);
    CHECK(xgm_tracking_alloc(&tracker, SIZE_MAX) == NULL);
    CHECK(backend.alloc_calls == 0U);
    CHECK(xgm_tracking_allocator_set_phase(&tracker, 10U) == XGS_OK);
    const xgm_allocator_t *service =
        xgm_tracking_allocator_get_interface(&tracker);
    void *first = xgm_alloc(service, 73U);
    CHECK(first != NULL && (uintptr_t) first % _Alignof(max_align_t) == 0U);
    CHECK(xgm_tracking_allocator_set_phase(&tracker, 3U) == XGS_OK);
    void *second = xgm_tracking_alloc(&tracker, 9U);
    CHECK(second != NULL && first != second);
    memset(first, 0xa5, 73U);
    memset(second, 0x5a, 9U);
    CHECK(total.current_allocated == 82U && total.peak_allocated == 82U);
    CHECK(phases[10].current_allocated == 73U &&
          phases[3].current_allocated == 9U);
    CHECK(xgm_tracking_allocator_set_phase(&tracker, 11U) ==
          XGS_INVALID_ARGUMENT);
    CHECK(tracker.current_phase == 3U);
    CHECK(xgm_tracking_allocator_set_phase(&other, 0U) == XGS_INVALID_ARGUMENT);

    backend.fail_on = backend.alloc_calls + 1U;
    CHECK(xgm_tracking_alloc(&tracker, 5U) == NULL);
    CHECK(total.alloc_count == 2U && total.current_allocated == 82U);
    backend.fail_on = 0U;
    void *separate = xgm_tracking_alloc(&other, 7U);
    CHECK(separate != NULL && other_total.current_allocated == 7U);
    xgm_tracking_free(&other, first);
    CHECK(backend.free_calls == 0U && other_total.free_count == 0U);
    CHECK(((uint8_t *) first)[72] == 0xa5U);
    xgm_tracking_allocator_reset_stats(&tracker);
    CHECK(total.total_allocated == 82U && total.peak_allocated == 82U);
    CHECK(total.alloc_count == 0U && total.free_count == 0U);
    CHECK(phases[10].total_allocated == 73U && phases[3].total_allocated == 9U);
    xgm_tracking_free(&tracker, first);
    xgm_free(service, second);
    xgm_tracking_free(&other, separate);
    CHECK(total.current_allocated == 0U && total.total_freed == 82U);
    CHECK(phases[10].total_freed == 73U && phases[3].total_freed == 9U);
    CHECK(other_total.current_allocated == 0U && other_total.total_freed == 7U);
    CHECK(backend.free_calls == 3U && backend.invalid_frees == 0U);
    CHECK(!backend.used[0] && !backend.used[1] && !backend.used[2]);
    xgm_tracking_free(&tracker, NULL);
    CHECK(backend.free_calls == 3U);
    return 0;
}
