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
    arena.allocateArraywithdefault<int>(3);

    auto it = arena.begin();
    ASSERT_NE(it, arena.end());

    // An array block is managed by a single header.
    // The size of the payload must equal the full array allocation.
    EXPECT_EQ(it->get_size(), 3 * sizeof(int));

    // Jump past the array
    ++it;
    EXPECT_EQ(it, arena.end());
}

TEST(ArenaAllocatorRangeBasedForTest, ConstRangeLoopReadingValues)
{
    TypeAwareArenaAllocator arena;
    arena.allocate<int>(5);
    arena.allocate<int>(10);
    arena.allocate<int>(15);

    // Enforce a const context to trigger ConstIterator implicitly
    const TypeAwareArenaAllocator& const_arena = arena;

    std::vector<int> extracted_values;

    // Modern C++ range-based for loop over const reference
    // 'entry' becomes a copy of the temporary Proxy-Entry returned by operator*
    for (const auto entry : const_arena)
    {
        extracted_values.push_back(entry.template get<int>());
    }

    ASSERT_EQ(extracted_values.size(), 3);
    EXPECT_EQ(extracted_values[0], 5);
    EXPECT_EQ(extracted_values[1], 10);
    EXPECT_EQ(extracted_values[2], 15);
}

TEST(ArenaAllocatorRangeBasedForTest, NonConstRangeLoopModifyingObjects)
{
    TypeAwareArenaAllocator arena;

    // Allocate raw data structures
    int* p1 = arena.allocate<int>(100);
    int* p2 = arena.allocate<int>(200);

    // Iterate through a non-const arena using a range-based for loop.
    // We fetch the internal object reference and modify its underlying value.
    for (auto entry : arena)
    {
        // Get a mutable reference to the object inside the arena memory
        int& val = entry.template get<int>();
        val += 50; // Modify the object in place
    }

    // Verify that the memory inside the arena was permanently altered
    EXPECT_EQ(*p1, 150);
    EXPECT_EQ(*p2, 250);
}

TEST(ArenaAllocatorRangeBasedForTest, RangeLoopSkipsIntermittentDeadSpaces)
{
    TypeAwareArenaAllocator arena;

    // Setup a fragmented memory pattern
    int* a = arena.allocate<int>(1);
    int* b = arena.allocate<int>(2); // Will be deleted
    int* c = arena.allocate<int>(3); // Will be deleted
    int* d = arena.allocate<int>(4);

    arena.deallocate(b);
    arena.deallocate(c);

    std::vector<int> active_elements;

    // The range loop must cleanly skip 'b' and 'c' via advance_to_next_alive()
    for (auto entry : arena)
    {
        active_elements.push_back(entry.template get<int>());
    }

    ASSERT_EQ(active_elements.size(), 2);
    EXPECT_EQ(active_elements[0], 1);
    EXPECT_EQ(active_elements[1], 4);
}

TEST(ArenaAllocatorRangeBasedForTest, LightweightModeRangeLoop)
{
    // Ensure that our brand new ArenaMode::Lightweight works perfectly with range loops too
    LightweightArenaAllocator arena;

    struct Point { int x; int y; };
    arena.allocate<Point>(10, 20);
    arena.allocate<Point>(30, 40);

    std::size_t counter = 0;
    for (auto entry : arena)
    {
        // Lightweight mode has no StoreTypeInfo (so no typeid check available),
        // but it tracks blocks and sizes seamlessly.
        EXPECT_EQ(entry.get_size(), sizeof(Point));
        counter++;
    }

    EXPECT_EQ(counter, 2);
}
