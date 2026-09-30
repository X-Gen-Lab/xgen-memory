/**
 * \file            test_alignment.cpp
 * \brief           Maximum alignment matches the public C and C++ contract
 */
#include <cstddef>
#include <gtest/gtest.h>
#include <xgen/memory/allocator.h>
TEST(Allocator, PublicMaximumAlignmentMatchesHostAbi) {
    EXPECT_EQ(alignof(xgm_max_align_t), alignof(max_align_t));
}
