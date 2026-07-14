#include <gtest/gtest.h>
#include "arena_allocator.hpp"
#include <string>

struct TrivialPoint {
    int x;
    int y;
};

TEST(ArenaAllocatorLightweightTest, StandardTrivialAllocationsSucceed)
{
    LightweightArenaAllocator arena;

    TrivialPoint* p1 = arena.allocate<TrivialPoint>(10, 20);
    EXPECT_EQ(p1->x, 10);
    EXPECT_EQ(p1->y, 20);

    // Ensure fragmentation and metric tracking still operates seamlessly
    EXPECT_EQ(arena.get_dead_bytes(), 0);
}

TEST(ArenaAllocatorLightweightTest, CompactionWorksWithoutFunctionPointers)
{
    LightweightArenaAllocator arena;

    TrivialPoint* p1 = arena.allocate<TrivialPoint>(1, 2);
    TrivialPoint* p2 = arena.allocate<TrivialPoint>(3, 4);

    arena.deallocate(p1);
    ASSERT_GT(arena.get_dead_bytes(), 0);

    // Trigger compaction: Must use memcpy pathway and succeed seamlessly
    EXPECT_NO_THROW({
        arena.forceReallocate();
    });

    EXPECT_EQ(arena.get_dead_bytes(), 0);
}
