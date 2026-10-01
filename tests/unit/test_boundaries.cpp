/**
 * \file            test_boundaries.cpp
 * \brief           Fault injection and bounded resource validation
 */
#include <cstring>
#include <gtest/gtest.h>
#include <xgen/memory/arena.h>
#include <xgen/memory/libc_allocator.h>
#include <xgen/memory/pool.h>
#include <xgen/memory/size_class_allocator.h>
#include <xgen/memory/tracking_allocator.h>

namespace {
struct Backend {
    alignas(max_align_t) unsigned char bytes[512]{};
    size_t allocations = 0;
    size_t frees = 0;
    size_t requested = 0;
    bool fail = false;
    bool misalign = false;
};

void* Allocate(void* context, size_t size) {
    auto* backend = static_cast<Backend*>(context);
    ++backend->allocations;
    backend->requested = size;
    return backend->fail || size > sizeof(backend->bytes)
               ? nullptr
               : backend->bytes + (backend->misalign ? 1 : 0);
}

void Release(void* context, void*) {
    ++static_cast<Backend*>(context)->frees;
}
}  // namespace

TEST(Pool, InvalidInputsAndNullQueries) {
    alignas(16) unsigned char storage[128]{};
    xgm_pool_t pool{};
    EXPECT_EQ(xgm_pool_init(nullptr, storage, 128, 32, 16, 4),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_init(&pool, nullptr, 128, 32, 16, 4),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_init(&pool, storage, 128, 32, 0, 4),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_init(&pool, storage, 128, 32, 3, 4),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_init(&pool, storage, 128, 1, 1, 4),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_init(&pool, storage, 128, 17, 16, 4),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_init(&pool, storage + 1, 127, 32, 16, 3),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_init(&pool, storage, 128, 32, 16, 0),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_init(&pool, storage, 10, 32, 16, 4), XGS_CAPACITY);
    EXPECT_EQ(xgm_pool_free(nullptr, nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_free(&pool, nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_deinit(nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_deinit(&pool), XGS_INVALID_ARGUMENT);
    EXPECT_FALSE(xgm_pool_contains(nullptr, storage));
    EXPECT_FALSE(xgm_pool_contains(&pool, nullptr));
    EXPECT_EQ(xgm_pool_alloc(nullptr), nullptr);
    EXPECT_EQ(xgm_pool_used_count(nullptr), 0U);
    EXPECT_EQ(xgm_pool_free_count(nullptr), 0U);
    EXPECT_EQ(xgm_pool_peak_used(nullptr), 0U);
    xgm_pool_reset_stats(nullptr);
    EXPECT_FALSE(xgm_allocator_is_valid(nullptr));
    auto invalid = xgm_pool_allocator(nullptr);
    EXPECT_FALSE(xgm_allocator_is_valid(&invalid));
    size_t block = 9, bytes = 10;
    EXPECT_EQ(xgm_pool_measure(0, 1, 1, &block, &bytes), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_measure(1, 0, 1, &block, &bytes), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_measure(1, 3, 1, &block, &bytes), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_measure(1, 1, 0, &block, &bytes), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_measure(1, 1, 1, nullptr, &bytes), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_measure(1, 1, 1, &block, nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_measure(1, 1, 1, &block, &block), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_pool_measure(SIZE_MAX, 16, 1, &block, &bytes), XGS_CAPACITY);
    EXPECT_EQ(xgm_pool_measure(16, 16, SIZE_MAX, &block, &bytes), XGS_CAPACITY);
    EXPECT_EQ(block, 9U);
    EXPECT_EQ(bytes, 10U);
    ASSERT_EQ(xgm_pool_init(&pool, storage + 16, 96, 32, 16, 3), XGS_OK);
    EXPECT_FALSE(xgm_pool_contains(&pool, storage));
    EXPECT_FALSE(xgm_pool_contains(&pool, storage + 112));
    EXPECT_EQ(xgm_pool_free(&pool, nullptr), XGS_OK);
    EXPECT_EQ(xgm_pool_deinit(&pool), XGS_OK);
}

TEST(SizeClass, EmptyClassesNullStateAndValidation) {
    xgm_pool_t pools[2]{};
    xgm_size_class_allocator_t state{};
    const xgm_size_class_spec_t empty[] = {{0, 0}, {16, 0}};
    ASSERT_EQ(xgm_size_class_init_ex(&state, pools, 2, empty, 2, nullptr, 0,
                                     XGM_SIZE_CLASS_FALLBACK),
              XGS_OK);
    EXPECT_EQ(xgm_size_class_alloc(&state, 1), nullptr);
    EXPECT_EQ(xgm_size_class_alloc(nullptr, 1), nullptr);
    EXPECT_EQ(xgm_size_class_free(nullptr, nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_deinit(nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_used_memory(nullptr), 0U);
    EXPECT_EQ(xgm_size_class_peak_memory(nullptr), 0U);
    xgm_size_class_reset_stats(nullptr);
    EXPECT_EQ(xgm_size_class_deinit(&state), XGS_OK);
    EXPECT_EQ(xgm_size_class_deinit(&state), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_free(&state, nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init(nullptr, pools, 2, empty, 2, nullptr, 0),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init(&state, nullptr, 2, empty, 2, nullptr, 0),
              XGS_INVALID_ARGUMENT);
    size_t bytes = 3, alignment = 4;
    EXPECT_EQ(xgm_size_class_measure(nullptr, 1, &bytes, &alignment),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_measure(empty, 0, &bytes, &alignment),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_measure(empty, 2, nullptr, &alignment),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_measure(empty, 2, &bytes, nullptr),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_measure(empty, 2, &bytes, &bytes),
              XGS_INVALID_ARGUMENT);
}

TEST(SizeClass, SeparateBufferFailuresAreAtomic) {
    alignas(max_align_t) unsigned char storage[128]{};
    xgm_size_class_allocator_t state{};
    xgm_pool_t pools[2]{};
    xgm_size_class_buffer_t buffers[] = {{storage, 64, 32, 2},
                                         {storage + 32, 64, 32, 2}};
    auto before = state;
    EXPECT_EQ(xgm_size_class_init_buffers(nullptr, pools, 2, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init_buffers(&state, nullptr, 2, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 2, nullptr, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 2, buffers, 0,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 1, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_CAPACITY);
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 2, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    buffers[0].storage = storage + 64;
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 2, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    buffers[0].storage = nullptr;
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 2, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    buffers[0].storage = storage + 1;
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 2, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    buffers[0].storage = storage;
    buffers[0].storage_size = 1;
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 2, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_CAPACITY);
    buffers[0].block_size = SIZE_MAX;
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 2, buffers, 2,
                                          XGM_SIZE_CLASS_STRICT),
              XGS_CAPACITY);
    EXPECT_EQ(std::memcmp(&state, &before, sizeof(state)), 0);
}

TEST(SizeClass, OwnedFailureRollsBackWithoutPublishingState) {
    Backend backend;
    xgm_allocator_t service{&backend, Allocate, Release};
    xgm_size_class_owned_t owned{};
    xgm_pool_t pools[2]{};
    xgm_size_class_spec_t specs[] = {{32, 1}, {64, 1}};
    auto before = owned;
    backend.fail = true;
    EXPECT_EQ(xgm_size_class_init_owned(&owned, pools, 2, specs, 2, &service,
                                        XGM_SIZE_CLASS_FALLBACK),
              XGS_NO_MEMORY);
    EXPECT_EQ(backend.allocations, 1U);
    EXPECT_EQ(backend.frees, 0U);
    backend.fail = false;
    backend.misalign = true;
    EXPECT_EQ(xgm_size_class_init_owned(&owned, pools, 2, specs, 2, &service,
                                        XGM_SIZE_CLASS_FALLBACK),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(backend.frees, 1U);
    EXPECT_EQ(std::memcmp(&owned, &before, sizeof(owned)), 0);
    EXPECT_EQ(xgm_size_class_init_owned(nullptr, pools, 2, specs, 2, &service,
                                        XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init_owned(&owned, nullptr, 2, specs, 2, &service,
                                        XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init_owned(&owned, pools, 1, specs, 2, &service,
                                        XGM_SIZE_CLASS_STRICT),
              XGS_CAPACITY);
    EXPECT_EQ(xgm_size_class_init_owned(&owned, pools, 2, specs, 2, nullptr,
                                        XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_init_owned(&owned, pools, 2, nullptr, 2, &service,
                                        XGM_SIZE_CLASS_STRICT),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_deinit_owned(nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_size_class_deinit_owned(&owned), XGS_INVALID_ARGUMENT);
    specs[0].block_count = 0;
    specs[1].block_count = 0;
    ASSERT_EQ(xgm_size_class_init_owned(&owned, pools, 2, specs, 2, nullptr,
                                        XGM_SIZE_CLASS_STRICT),
              XGS_OK);
    EXPECT_EQ(xgm_size_class_deinit_owned(&owned), XGS_OK);
}

TEST(Tracking, RequestAccountingAndCounterOverflowKeepLiveBytesExact) {
    Backend backend;
    xgm_allocator_t service{&backend, Allocate, Release};
    xgm_tracking_allocator_t tracker{};
    xgm_allocator_stats_t stats{}, phases[1]{};
    ASSERT_EQ(
        xgm_tracking_allocator_init(&tracker, &service, &stats, phases, 1),
        XGS_OK);
    stats.total_allocated = SIZE_MAX;
    stats.alloc_count = SIZE_MAX;
    phases[0].total_allocated = SIZE_MAX;
    void* block = xgm_tracking_alloc(&tracker, 5);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(backend.requested, 5U + xgm_tracking_overhead());
    EXPECT_TRUE(stats.saturated);
    EXPECT_TRUE(phases[0].saturated);
    stats.total_freed = SIZE_MAX;
    stats.free_count = SIZE_MAX;
    xgm_tracking_free(&tracker, block);
    EXPECT_EQ(stats.current_allocated, 0U);
    EXPECT_EQ(stats.live_blocks, 0U);
    EXPECT_EQ(stats.total_freed, SIZE_MAX);
    EXPECT_EQ(stats.free_count, SIZE_MAX);
    xgm_tracking_allocator_reset_stats(&tracker);
    EXPECT_FALSE(stats.saturated);
    stats.current_allocated = SIZE_MAX;
    EXPECT_EQ(xgm_tracking_alloc(&tracker, 1), nullptr);
    stats.current_allocated = 0;
    stats.live_blocks = SIZE_MAX;
    EXPECT_EQ(xgm_tracking_alloc(&tracker, 1), nullptr);
    stats.live_blocks = 0;
    phases[0].current_allocated = SIZE_MAX;
    EXPECT_EQ(xgm_tracking_alloc(&tracker, 1), nullptr);
    phases[0].current_allocated = 0;
    phases[0].live_blocks = SIZE_MAX;
    EXPECT_EQ(xgm_tracking_alloc(&tracker, 1), nullptr);
    phases[0].live_blocks = 0;
    EXPECT_EQ(backend.allocations, 1U);
    EXPECT_EQ(xgm_tracking_allocator_deinit(&tracker), XGS_OK);
}

TEST(Tracking, NullInputsAndUnsupportedStateOverlap) {
    xgm_tracking_allocator_t tracker{};
    xgm_allocator_stats_t stats{}, phases[1]{};
    auto backend = *xgm_allocator_libc();
    EXPECT_EQ(
        xgm_tracking_allocator_init(nullptr, &backend, &stats, nullptr, 0),
        XGS_INVALID_ARGUMENT);
    EXPECT_EQ(
        xgm_tracking_allocator_init(&tracker, &backend, nullptr, nullptr, 0),
        XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_tracking_allocator_init(&tracker, &tracker.underlying, &stats,
                                          nullptr, 0),
              XGS_INVALID_ARGUMENT);
    backend.alloc = nullptr;
    EXPECT_EQ(
        xgm_tracking_allocator_init(&tracker, &backend, &stats, nullptr, 0),
        XGS_INVALID_ARGUMENT);
    backend = *xgm_allocator_libc();
    backend.free = nullptr;
    EXPECT_EQ(
        xgm_tracking_allocator_init(&tracker, &backend, &stats, nullptr, 0),
        XGS_INVALID_ARGUMENT);
    backend = *xgm_allocator_libc();
    EXPECT_EQ(xgm_tracking_allocator_init(
                  &tracker, &backend,
                  reinterpret_cast<xgm_allocator_stats_t*>(&tracker), phases,
                  1),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_tracking_allocator_init(
                  &tracker, &backend, &stats,
                  reinterpret_cast<xgm_allocator_stats_t*>(&tracker), 1),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_tracking_alloc(nullptr, 1), nullptr);
    EXPECT_EQ(xgm_tracking_alloc(&tracker, 1), nullptr);
    EXPECT_EQ(xgm_tracking_allocator_get_interface(nullptr), nullptr);
    EXPECT_EQ(xgm_tracking_allocator_get_interface(&tracker), nullptr);
    EXPECT_EQ(xgm_tracking_allocator_set_phase(nullptr, 0),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_tracking_allocator_set_phase(&tracker, 0),
              XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_tracking_allocator_deinit(nullptr), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_tracking_allocator_deinit(&tracker), XGS_INVALID_ARGUMENT);
    xgm_tracking_allocator_reset_stats(nullptr);
    xgm_tracking_allocator_reset_stats(&tracker);
    xgm_tracking_free(nullptr, nullptr);
    xgm_tracking_free(&tracker, nullptr);
}

TEST(Arena, PaddingExhaustionAndAddressOverflow) {
    alignas(64) unsigned char storage[64]{};
    xgm_arena_t arena{};
    ASSERT_EQ(xgm_arena_init(&arena, storage + 1, 2), XGS_OK);
    EXPECT_EQ(xgm_arena_alloc(&arena, 1, 64), nullptr);
    EXPECT_EQ(xgm_arena_used(&arena), 0U);
    EXPECT_EQ(
        xgm_arena_init(&arena, reinterpret_cast<void*>(UINTPTR_MAX - 1), 4),
        XGS_CAPACITY);
    EXPECT_EQ(xgm_arena_alloc(nullptr, 1, 1), nullptr);
}
