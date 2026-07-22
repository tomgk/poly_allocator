#include <gtest/gtest.h>
#include "arena_allocator.hpp"

TEST(ArenaAllocatorMaxCapacityTest, ThrowsWhenCustomLimitIsExceeded)
{
    PlainArenaAllocator arena;

    // Set a very strict custom limit of 512 bytes
    arena.set_max_capacity(512);

    // This allocation fits within the default 1MB, but must breach our 512 bytes limit
    EXPECT_THROW({
        arena.allocateArrayWithDefault<std::byte>(600);
    }, std::runtime_error);
}

TEST(ArenaAllocatorMaxCapacityTest, AllowsExpandingBeyondDefaultOneMegabyte)
{
    PlainArenaAllocator arena;

    // Expand the limit to 4 MB
    arena.set_max_capacity(4 * 1024 * 1024);

    // This allocation would crash with the old hardcoded 1 MB limit, but must succeed now
    EXPECT_NO_THROW({
        arena.allocateArrayWithDefault<std::byte>(2 * 1024 * 1024);
    });
}

TEST(ArenaAllocatorMaxCapacityTest, ExactLimitMatchDoesNotThrow)
{
    PlainArenaAllocator arena;

    arena.set_max_capacity(512);

    // Allocating exactly up to a boundary that matches or stays below 512 after growth is fine
    EXPECT_NO_THROW({
        arena.allocateArrayWithDefault<std::byte>(100);
    });
}

TEST(ArenaAllocatorMaxCapacityTest, GetterReturnsCorrectConfiguredLimits)
{
    PlainArenaAllocator arena;

    // 1. Verify that the default maximum capacity is exactly 1 MB
    EXPECT_EQ(arena.get_max_capacity(), 1024 * 1024);

    // 2. Change the limit to a custom value (e.g., 2 MB)
    std::size_t custom_limit = 2 * 1024 * 1024;
    arena.set_max_capacity(custom_limit);

    // 3. Verify that the getter returns the updated custom limit
    EXPECT_EQ(arena.get_max_capacity(), custom_limit);
}