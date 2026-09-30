/**
 * \file            test_memory.cpp
 * \brief           Memory migration and boundary behavior
 */
#include <gtest/gtest.h>
#include <xgen/memory/pool.h>
#include <xgen/memory/tracking_allocator.h>
#include <xgen/memory/libc_allocator.h>
#include <xgen/memory/size_class_allocator.h>
#include <cstring>
extern "C" {
int baseline_allocator(void);
int baseline_pool(void);
int baseline_size_class(void);
int baseline_tracking(void);
}
TEST(Migration, Allocator) { EXPECT_EQ(baseline_allocator(), 0); }
TEST(Migration, Pool) { EXPECT_EQ(baseline_pool(), 0); }
TEST(Migration, SizeClass) { EXPECT_EQ(baseline_size_class(), 0); }
TEST(Migration, Tracking) { EXPECT_EQ(baseline_tracking(), 0); }

TEST(Pool, RejectsUnderalignedAllocatorAdapter) {
    alignas(max_align_t) unsigned char storage[65] = {};
    xgm_pool_t pool{};
    ASSERT_EQ(xgm_pool_init(&pool, storage + 1, 64, 16, 1, 4), XGS_OK);
    auto service = xgm_pool_allocator(&pool);
    EXPECT_FALSE(xgm_allocator_is_valid(&service));
}
TEST(Tracking, RejectsAliasedAggregateAndPhasesWithoutMutation) {
    xgm_tracking_allocator_t tracker{};
    xgm_allocator_stats_t stats[2]{};
    stats[0].total_allocated = 17;
    EXPECT_EQ(xgm_tracking_allocator_init(&tracker, xgm_allocator_libc(),
        &stats[0], stats, 2), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(stats[0].total_allocated, 17U);
}
TEST(Tracking, SaturatesHistoricalCounters) {
    xgm_tracking_allocator_t tracker{};
    xgm_allocator_stats_t stats{};
    ASSERT_EQ(xgm_tracking_allocator_init(&tracker, xgm_allocator_libc(), &stats,
        nullptr, 0), XGS_OK);
    stats.total_allocated = SIZE_MAX - 4;
    void* allocation = xgm_tracking_alloc(&tracker, 8);
    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(stats.total_allocated, SIZE_MAX);
    EXPECT_EQ(stats.current_allocated, 8U);
    xgm_tracking_free(&tracker, allocation);
}

TEST(SizeClass, FallbackSkipsExhaustedClassesAndPreservesStrictDefault) {
    alignas(max_align_t) unsigned char storage[192]{};
    xgm_pool_t pools[3]{};
    xgm_size_class_allocator_t state{};
    const xgm_size_class_spec_t specs[] = {{64, 1}, {32, 2}, {64, 1}};
    ASSERT_EQ(xgm_size_class_init_ex(&state, pools, 3, specs, 3,
        storage, sizeof(storage), XGM_SIZE_CLASS_FALLBACK), XGS_OK);
    void* values[4]{};
    for (auto& value : values) {
        value = xgm_size_class_alloc(&state, 1);
        ASSERT_NE(value, nullptr);
    }
    EXPECT_EQ(xgm_size_class_alloc(&state, 1), nullptr);
    EXPECT_EQ(xgm_size_class_used_memory(&state), 192U);
    EXPECT_EQ(xgm_size_class_peak_memory(&state), 192U);
    EXPECT_EQ(xgm_size_class_deinit(&state), XGS_BUSY);
    for (auto value : values) EXPECT_EQ(xgm_size_class_free(&state, value), XGS_OK);
    EXPECT_EQ(xgm_size_class_used_memory(&state), 0U);
    xgm_size_class_reset_stats(&state);
    EXPECT_EQ(xgm_size_class_peak_memory(&state), 0U);
    EXPECT_EQ(xgm_size_class_deinit(&state), XGS_OK);
}
TEST(SizeClass, SeparateBuffersEmptyClassesAndOverlapValidation) {
    alignas(max_align_t) unsigned char small[32]{}, large[64]{};
    xgm_pool_t pools[3]{};
    xgm_size_class_allocator_t state{};
    const xgm_size_class_buffer_t buffers[] = {
        {small, sizeof(small), 32, 1}, {nullptr, 0, 0, 0},
        {large, sizeof(large), 64, 1}};
    ASSERT_EQ(xgm_size_class_init_buffers(&state, pools, 3, buffers, 3,
        XGM_SIZE_CLASS_FALLBACK), XGS_OK);
    EXPECT_EQ(xgm_size_class_alloc(&state, 1), small);
    EXPECT_EQ(xgm_size_class_alloc(&state, 1), large);
    EXPECT_EQ(xgm_size_class_free(&state, small), XGS_OK);
    EXPECT_EQ(xgm_size_class_free(&state, large), XGS_OK);
    EXPECT_EQ(xgm_size_class_deinit(&state), XGS_OK);
    auto before = state;
    const xgm_size_class_buffer_t overlapping[] = {
        {large, sizeof(large), 32, 1}, {large, sizeof(large), 32, 1}};
    EXPECT_EQ(xgm_size_class_init_buffers(&state, pools, 3, overlapping, 2,
        XGM_SIZE_CLASS_STRICT), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(std::memcmp(&state, &before, sizeof(state)), 0);
}
TEST(SizeClass, OwnedStorageIsExplicitAndCannotBeDestroyedWithLiveBlocks) {
    xgm_size_class_allocator_t state{};
    xgm_pool_t pools[2]{};
    const xgm_size_class_spec_t specs[] = {{32, 1}, {64, 1}};
    ASSERT_EQ(xgm_size_class_init_owned(&state, pools, 2, specs, 2,
        xgm_allocator_libc(), XGM_SIZE_CLASS_FALLBACK), XGS_OK);
    void* value = xgm_size_class_alloc(&state, 12);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(xgm_size_class_deinit(&state), XGS_BUSY);
    EXPECT_EQ(xgm_size_class_free(&state, value), XGS_OK);
    EXPECT_EQ(xgm_size_class_deinit(&state), XGS_OK);
}
TEST(Tracking, LiveCountsSurviveResetAndDeinitRequiresNoLiveBlocks) {
    xgm_tracking_allocator_t tracker{};
    xgm_allocator_stats_t stats{}, phases[2]{};
    ASSERT_EQ(xgm_tracking_allocator_init(&tracker, xgm_allocator_libc(), &stats,
        phases, 2), XGS_OK);
    void* block = xgm_tracking_alloc(&tracker, 4);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(stats.live_blocks, 1U);
    EXPECT_EQ(xgm_tracking_allocator_deinit(&tracker), XGS_BUSY);
    xgm_tracking_allocator_reset_stats(&tracker);
    EXPECT_EQ(stats.live_blocks, 1U);
    EXPECT_FALSE(stats.saturated);
    EXPECT_EQ(xgm_tracking_allocator_set_phase(&tracker, 1), XGS_OK);
    xgm_tracking_free(&tracker, block);
    EXPECT_EQ(phases[0].free_count, 1U);
    EXPECT_EQ(phases[1].free_count, 0U);
    EXPECT_EQ(stats.live_blocks, 0U);
    EXPECT_GT(xgm_tracking_overhead(), 0U);
    EXPECT_EQ(xgm_tracking_allocator_deinit(&tracker), XGS_OK);
}
TEST(Pool, CheckedCloseAndStorageMeasure) {
    size_t block_size = 0, storage_size = 0;
    ASSERT_EQ(xgm_pool_measure(3, alignof(max_align_t), 4, &block_size,
        &storage_size), XGS_OK);
    EXPECT_EQ(block_size % alignof(max_align_t), 0U);
    EXPECT_EQ(storage_size, block_size * 4);
    alignas(max_align_t) unsigned char storage[256]{};
    xgm_pool_t pool{};
    ASSERT_EQ(xgm_pool_init(&pool, storage, sizeof(storage), block_size,
        alignof(max_align_t), 4), XGS_OK);
    void* value = xgm_pool_alloc(&pool);
    EXPECT_EQ(xgm_pool_close(&pool), XGS_BUSY);
    EXPECT_EQ(xgm_pool_free(&pool, value), XGS_OK);
    EXPECT_EQ(xgm_pool_close(&pool), XGS_OK);
}
