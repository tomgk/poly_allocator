#include <gtest/gtest.h>
#include "arena_allocator.hpp"

TEST(ArenaAllocatorCallbackTest, CallbackFiresOnForcedReallocation)
{
    PlainArenaAllocator arena;

    bool callback_called = false;
    std::size_t captured_old_cap = 0;
    std::size_t captured_new_cap = 0;

    // Register the reallocation hook
    arena.on_reallocation([&](std::size_t old_cap, std::size_t new_cap) {
        callback_called = true;
        captured_old_cap = old_cap;
        captured_new_cap = new_cap;
    });

    std::size_t expected_old_cap = arena.get_capacity();

    // Explicitly force a reallocation
    arena.forceReallocate();

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(captured_old_cap, expected_old_cap);
    EXPECT_GT(captured_new_cap, expected_old_cap);
}

TEST(ArenaAllocatorCallbackTest, CallbackFiresWhenCapacityIsExceeded)
{
    PlainArenaAllocator arena;

    std::size_t trigger_count = 0;
    arena.on_reallocation([&](std::size_t old_cap, std::size_t new_cap) {
        trigger_count++;
    });

    // Fill the arena with a large array allocation to break past the INITIAL_CAPACITY
    std::size_t safe_large_count = arena.get_capacity() + 10;
    arena.allocateArraywithdefault<std::byte>(safe_large_count);

    // The callback must have been triggered exactly once during the allocation process
    EXPECT_EQ(trigger_count, 1);
}

TEST(ArenaAllocatorCallbackTest, UnregisteredCallbackDoesNotCrash)
{
    PlainArenaAllocator arena;

    // Ensure that allocating or forcing a realloc without any callback registered is safe
    EXPECT_NO_THROW({
        arena.forceReallocate();
    });
}