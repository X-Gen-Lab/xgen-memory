/**
 * \file            test_memory.cpp
 * \brief           Memory migration and boundary behavior
 */
#include <gtest/gtest.h>
#include <xgen/memory/pool.h>
#include <xgen/memory/tracking_allocator.h>
#include <xgen/memory/libc_allocator.h>
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
