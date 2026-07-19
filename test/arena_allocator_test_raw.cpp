#include <gtest/gtest.h>
#include "arena_allocator.hpp"
#include <algorithm>

TEST(ArenaAllocatorRawTest, HandleActsLikeAnSTLContainer)
{
    PlainArenaAllocator arena;

    // Allocate 4 bytes of raw memory with an alignment of 1
    PlainArenaBufferHandle handle = arena.allocateRaw(4, 1);

    ASSERT_EQ(handle.size(), 4);
    EXPECT_FALSE(handle.empty());

    // 1. Element assignment using standard container operator[]
    handle[0] = std::byte{10};
    handle[1] = std::byte{20};
    handle[2] = std::byte{30};
    handle[3] = std::byte{40};

    EXPECT_EQ(handle.front(), std::byte{10});
    EXPECT_EQ(handle.back(), std::byte{40});

    // 2. Integration with modern C++ range-based for loops
    int sum = 0;
    for (std::byte b : handle)
    {
        sum += std::to_integer<int>(b);
    }
    EXPECT_EQ(sum, 100);

    // 3. Integration with standard STL algorithms (e.g., std::all_of)
    bool all_greater_zero = std::all_of(handle.begin(), handle.end(), [](std::byte b){
        return std::to_integer<int>(b) > 0;
    });
    EXPECT_TRUE(all_greater_zero);
}

TEST(ArenaAllocatorRawTest, HandleSurvivesReallocationAndCompaction)
{
    PlainArenaAllocator arena;

    // Allocate a raw buffer and write a specific data signature into it
    PlainArenaBufferHandle raw_handle = arena.allocateRaw(4, 1);
    raw_handle[0] = std::byte{'X'};
    raw_handle[1] = std::byte{'Y'};
    raw_handle[2] = std::byte{'Z'};
    raw_handle[3] = std::byte{'!'};

    // Allocate an intermediate object that we will delete to enforce a "dead-space" gap
    int* temporary_object = arena.allocate<int>(999);

    // Allocate another object behind it to ensure the arena has active data layout trailing
    arena.allocate<double>(3.1415);

    // Mark the middle object as dead to set up a fragmentation compaction scenario
    arena.deallocate(temporary_object);

    ASSERT_GT(arena.get_dead_bytes(), 0);

    // TRIGGER COMPACTION & REALLOCATION:
    // This forces the arena to completely migrate all alive chunks into a fresh buffer
    // and eliminates the dead space. The physical addresses of ALL objects change here!
    arena.forceReallocate();

    // CRITICAL RUNTIME VERIFICATION:
    // The handle utilizes internal offset tracking instead of a raw dangling pointer.
    // It must still resolve the correct data at the new memory location without crashing.
    EXPECT_EQ(raw_handle[0], std::byte{'X'});
    EXPECT_EQ(raw_handle[1], std::byte{'Y'});
    EXPECT_EQ(raw_handle[2], std::byte{'Z'});
    EXPECT_EQ(raw_handle[3], std::byte{'!'});

    // Ensure compaction cleaned out the dead bytes successfully
    EXPECT_EQ(arena.get_dead_bytes(), 0);
}

TEST(ArenaAllocatorRawTest, ZeroAllocationReturnsEmptyHandle)
{
    PlainArenaAllocator arena;

    // Allocating 0 bytes should return a safe, empty default handle
    PlainArenaBufferHandle handle = arena.allocateRaw(0, 1);

    EXPECT_TRUE(handle.empty());
    EXPECT_EQ(handle.size(), 0);
    EXPECT_EQ(handle.data(), nullptr);
}
