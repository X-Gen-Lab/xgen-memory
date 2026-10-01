/**
 * \file            test_arena.cpp
 * \brief           Caller-owned arena lifetime and overflow tests
 */
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <xgen/memory/arena.h>

TEST(Arena, AlignedBumpsAndWholeLifetimeReset) {
    alignas(64) unsigned char storage[128]{};
    xgm_arena_t arena{};
    ASSERT_EQ(xgm_arena_init(&arena, storage + 1, 127), XGS_OK);
    EXPECT_EQ(xgm_arena_alloc(&arena, 3, 1), storage + 1);
    void* aligned = xgm_arena_alloc(&arena, 8, 32);
    ASSERT_NE(aligned, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(aligned) % 32, 0U);
    EXPECT_EQ(xgm_arena_used(&arena), 39U);
    EXPECT_EQ(xgm_arena_remaining(&arena), 88U);
    EXPECT_EQ(xgm_arena_peak_used(&arena), 39U);
    xgm_arena_reset(&arena);
    EXPECT_EQ(xgm_arena_used(&arena), 0U);
    EXPECT_EQ(xgm_arena_peak_used(&arena), 39U);
    EXPECT_EQ(xgm_arena_alloc(&arena, 127, 1), storage + 1);
    EXPECT_EQ(xgm_arena_alloc(&arena, 1, 1), nullptr);
    EXPECT_EQ(xgm_arena_peak_used(&arena), 127U);
    xgm_arena_deinit(&arena);
    EXPECT_EQ(xgm_arena_alloc(&arena, 1, 1), nullptr);
}

TEST(Arena, InvalidRequestsPreserveCursorAndFailedInitPreservesState) {
    unsigned char storage[32]{};
    xgm_arena_t arena{};
    ASSERT_EQ(xgm_arena_init(&arena, storage, sizeof(storage)), XGS_OK);
    auto before = arena;
    EXPECT_EQ(xgm_arena_alloc(&arena, 0, 1), nullptr);
    EXPECT_EQ(xgm_arena_alloc(&arena, 1, 0), nullptr);
    EXPECT_EQ(xgm_arena_alloc(&arena, 1, 3), nullptr);
    EXPECT_EQ(xgm_arena_alloc(&arena, SIZE_MAX, 8), nullptr);
    EXPECT_EQ(std::memcmp(&arena, &before, sizeof(arena)), 0);
    EXPECT_EQ(xgm_arena_init(&arena, nullptr, 1), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(std::memcmp(&arena, &before, sizeof(arena)), 0);
    EXPECT_EQ(xgm_arena_init(nullptr, storage, 32), XGS_INVALID_ARGUMENT);
    EXPECT_EQ(xgm_arena_init(&arena, nullptr, 0), XGS_OK);
    EXPECT_EQ(xgm_arena_alloc(&arena, 1, 1), nullptr);
    EXPECT_EQ(xgm_arena_used(nullptr), 0U);
    EXPECT_EQ(xgm_arena_remaining(nullptr), 0U);
    EXPECT_EQ(xgm_arena_peak_used(nullptr), 0U);
    xgm_arena_reset(nullptr);
    xgm_arena_deinit(nullptr);
}
