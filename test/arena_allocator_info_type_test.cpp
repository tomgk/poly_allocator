#include <gtest/gtest.h>
#include "arena_allocator.hpp"

TEST(ArenaAllocatorTypeMetricsTest, TrackCountsAndBytesCorrectly)
{
    // Use the type-aware version of your allocator
    TypeAwareArenaAllocator arena;

    // Initially everything must be zero
    EXPECT_EQ(arena.get_allocation_count<int>(), 0);
    EXPECT_EQ(arena.get_total_bytes_for_type<int>(), 0);

    // Allocate some individual integers
    arena.allocate<int>(10);
    arena.allocate<int>(20);

    // Allocate a different type to ensure isolation
    arena.allocate<double>(3.14);

    EXPECT_EQ(arena.get_allocation_count<int>(), 2);
    EXPECT_EQ(arena.get_total_bytes_for_type<int>(), 2 * sizeof(int));

    EXPECT_EQ(arena.get_allocation_count<double>(), 1);
    EXPECT_EQ(arena.get_total_bytes_for_type<double>(), sizeof(double));
}

TEST(ArenaAllocatorTypeMetricsTest, ArrayAllocationsAreCountedPerElement)
{
    TypeAwareArenaAllocator arena;

    // Allocate an array of 5 integers
    arena.allocateArray2<int>(5);

    // Elements inside arrays are tracked via their aggregated block size
    EXPECT_EQ(arena.get_allocation_count<int>(), 5);
    EXPECT_EQ(arena.get_total_bytes_for_type<int>(), 5 * sizeof(int));
}

TEST(ArenaAllocatorTypeMetricsTest, DeallocationReducesMetrics)
{
    TypeAwareArenaAllocator arena;

    int* a = arena.allocate<int>(100);
    int* b = arena.allocate<int>(200);

    ASSERT_EQ(arena.get_allocation_count<int>(), 2);

    // Kill one object
    arena.deallocate(a);

    // The counts and bytes must decrease accordingly
    EXPECT_EQ(arena.get_allocation_count<int>(), 1);
    EXPECT_EQ(arena.get_total_bytes_for_type<int>(), sizeof(int));
}
