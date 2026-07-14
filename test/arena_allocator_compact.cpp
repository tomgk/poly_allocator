#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>

class ArenaCompactTest : public ::testing::Test
{
protected:
    using AllocatorType = PlainArenaAllocator;
    AllocatorType arena;
};

// Verifies that compact() eliminates dead space and shrinks the used byte offset
TEST_F(ArenaCompactTest, DISABLED_CompactionEliminatesDeadSpaceTest)
{
    // Act 1: Allocate three separate items to create a sequence in memory
    int* item_1 = arena.allocate<int>(10);
    int* item_2 = arena.allocate<int>(20);
    int* item_3 = arena.allocate<int>(30);

    std::size_t offset_with_all_alive = arena.get_used_bytes();
    EXPECT_EQ(arena.get_dead_bytes(), 0);

    // Act 2: Deallocate the middle item to create a memory hole (dead space)
    arena.deallocate(item_2);

    std::size_t expected_dead = sizeof(int) + AllocatorType::HEADER_SIZE;
    EXPECT_EQ(arena.get_dead_bytes(), expected_dead);
    EXPECT_EQ(arena.get_used_bytes(), offset_with_all_alive); // Offset hasn't moved yet

    // Act 3: Perform compaction
    arena.compact();

    // Assert: Dead bytes must be gone and the total used bytes must be smaller
    EXPECT_EQ(arena.get_dead_bytes(), 0);
    EXPECT_LT(arena.get_used_bytes(), offset_with_all_alive);

    // Verify remaining data integrity after relocation
    EXPECT_EQ(*item_1, 10);
    EXPECT_EQ(*item_3, 30);

    // Cleanup the rest
    arena.deallocate(item_1);
    arena.deallocate(item_3);
}
