#include <gtest/gtest.h>
#include "arena_allocator.hpp"
#include <numeric>
#include <vector>

struct SamplePoint {
    int x;
    int y;
};

TEST(ArenaAllocatorIteratorTest, EmptyArenaHasEqualBeginAndEnd)
{
    TypeAwareArenaAllocator arena;

    // begin() and end() must be identical in a fresh arena
    EXPECT_EQ(arena.begin(), arena.end());
}

TEST(ArenaAllocatorIteratorTest, IterationOverMultipleAliveObjects)
{
    TypeAwareArenaAllocator arena;

    arena.allocate<int>(10);
    arena.allocate<int>(20);
    arena.allocate<int>(30);

    std::vector<int> values;

    // Standard range-based for-loop utilizing operator* and operator++
    for (auto it = arena.begin(); it != arena.end(); ++it)
    {
        // For type-aware or proxy-based iterators, use the standard template getter via arrow or star
        // If your test suite uses the direct .get<T>() on the proxy:
        values.push_back(it->get<int>());
    }

    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 30);
}

TEST(ArenaAllocatorIteratorTest, IteratorAutomaticallySkipsDeadAllocations)
{
    TypeAwareArenaAllocator arena;

    int* a = arena.allocate<int>(100);
    int* b = arena.allocate<int>(200); // This one will be killed
    int* c = arena.allocate<int>(300);

    // Deallocate the middle element, creating a gap
    arena.deallocate(b);

    auto it = arena.begin();
    ASSERT_NE(it, arena.end());
    EXPECT_EQ(it->get<int>(), 100);

    // Incrementing must bypass 'b' and jump straight to 'c'
    ++it;
    ASSERT_NE(it, arena.end());
    EXPECT_EQ(it->get<int>(), 300);

    ++it;
    EXPECT_EQ(it, arena.end());
}

TEST(ArenaAllocatorIteratorTest, ArrowOperatorAccessesMetadataAndData)
{
    TypeAwareArenaAllocator arena;
    arena.allocate<SamplePoint>(5, 10);

    auto it = arena.begin();
    ASSERT_NE(it, arena.end());

    // Verify arrow operator for structural member extraction
    EXPECT_EQ(it->get_size(), sizeof(SamplePoint));
    EXPECT_TRUE(it.get_header().is_alive);

    SamplePoint p = it->get<SamplePoint>();
    EXPECT_EQ(p.x, 5);
    EXPECT_EQ(p.y, 10);
}

TEST(ArenaAllocatorIteratorTest, ConstIteratorTraversalSupport)
{
    TypeAwareArenaAllocator arena;
    arena.allocate<int>(999);

    // Enforce const-correctness context
    const TypeAwareArenaAllocator& const_arena = arena;

    // begin() and end() must resolve to ConstIterator types automatically
    auto it = const_arena.begin();
    auto end_it = const_arena.end();

    ASSERT_NE(it, end_it);
    EXPECT_EQ(it->get<int>(), 999);

    ++it;
    EXPECT_EQ(it, end_it);
}

TEST(ArenaAllocatorIteratorTest, PostIncrementBehavior)
{
    TypeAwareArenaAllocator arena;
    arena.allocate<int>(11);
    arena.allocate<int>(22);

    auto it = arena.begin();

    // Post-increment should return a copy of the old position before moving forward
    auto old_it = it++;

    EXPECT_EQ(old_it->get<int>(), 11);
    EXPECT_EQ(it->get<int>(), 22);
}

TEST(ArenaAllocatorIteratorTest, ArrayAllocationTraversal)
{
    TypeAwareArenaAllocator arena;

    // Allocate a contiguous block array
    arena.allocateArray<int>(3);

    auto it = arena.begin();
    ASSERT_NE(it, arena.end());

    // An array block is managed by a single header.
    // The size of the payload must equal the full array allocation.
    EXPECT_EQ(it->get_size(), 3 * sizeof(int));

    // Jump past the array
    ++it;
    EXPECT_EQ(it, arena.end());
}
