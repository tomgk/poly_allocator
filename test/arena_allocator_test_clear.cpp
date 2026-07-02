#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
#include "common.h"

#include "data.h"

TEST(ArenaAllocator_clear, Data)
{
    TypeAwareArenaAllocator a;

    a.allocateWithNoOffset<Data>(12);
    a.clear();
}

TEST(ArenaAllocator_clear, string_short)
{
    TypeAwareArenaAllocator a;

    a.allocateWithNoOffset<std::string>("12");
    a.clear();
}

TEST(ArenaAllocator_clear, string_long)
{
    TypeAwareArenaAllocator a;

    a.allocateWithNoOffset<std::string>(LONG_STRING);
    a.clear();
}
