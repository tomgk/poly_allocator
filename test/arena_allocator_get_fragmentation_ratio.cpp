#include <gtest/gtest.h>
#include "arena_allocator.hpp" // Adapt pathway if necessary

// Helper struct for complex types if needed
struct MockObject {
    int data[4];
};

TEST(ArenaAllocatorFragmentationTest, EmptyArenaHasZeroFragmentation)
{
    PlainArenaAllocator arena;

    // An empty arena must return exactly 0.0
    EXPECT_DOUBLE_EQ(arena.get_fragmentation_ratio(), 0.0);
}

TEST(ArenaAllocatorFragmentationTest, DISABLED_ActiveObjectsHaveZeroFragmentation)
{
    PlainArenaAllocator arena;

    // Allocate multiple items
    int* a = arena.allocate<int>(42);
    MockObject* b = arena.allocate<MockObject>();

    // As long as all objects are alive, fragmentation must remain 0.0
    EXPECT_DOUBLE_EQ(arena.get_fragmentation_ratio(), 0.0);
}

TEST(ArenaAllocatorFragmentationTest, DeallocationTriggersFragmentation)
{
    PlainArenaAllocator arena;

    // Allocate three distinct blocks
    int* a = arena.allocate<int>(10);
    int* b = arena.allocate<int>(20);
    int* c = arena.allocate<int>(30);

    std::size_t initial_used = arena.get_used_bytes();

    // Deallocate the middle element to create a dead-space gap
    arena.deallocate(b);

    double ratio = arena.get_fragmentation_ratio();

    // Fragmentation must now be strictly greater than 0.0 and less than 1.0
    EXPECT_GT(ratio, 0.0);
    EXPECT_LT(ratio, 1.0);

    // Mathematically verify the exact expected ratio bounds
    std::size_t dead = arena.get_dead_bytes();
    EXPECT_DOUBLE_EQ(ratio, static_cast<double>(dead) / static_cast<double>(initial_used));
}

TEST(ArenaAllocatorFragmentationTest, CompactionResetsFragmentationToZero)
{
    PlainArenaAllocator arena;

    int* a = arena.allocate<int>(1);
    int* b = arena.allocate<int>(2);
    int* c = arena.allocate<int>(3);

    // Create fragmentation
    arena.deallocate(b);
    ASSERT_GT(arena.get_fragmentation_ratio(), 0.0);

    // Trigger compaction via manual reallocation force
    arena.forceReallocate();

    // After compaction, all dead space is eliminated. Fragmentation must be 0.0
    EXPECT_DOUBLE_EQ(arena.get_fragmentation_ratio(), 0.0);
    EXPECT_EQ(arena.get_dead_bytes(), 0); // Ensure dead bytes are cleared too
}

TEST(ArenaAllocatorFragmentationTest, ClearResetsFragmentation)
{
    PlainArenaAllocator arena;

    arena.allocate<int>(100);
    int* b = arena.allocate<int>(200);
    arena.deallocate(b);

    ASSERT_GT(arena.get_fragmentation_ratio(), 0.0);

    // Completely wipe the arena
    arena.clear();

    // Clear resets the tracking offset, so fragmentation must return to 0.0
    EXPECT_DOUBLE_EQ(arena.get_fragmentation_ratio(), 0.0);
}
