#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
#include "common.h"

#include "data.h"

TEST(ArenaAllocator_clear, Data)
{
    TypeAwareArenaAllocator a;

    a.allocate<Data>(12);
    a.clear();
}

TEST(ArenaAllocator_clear, string_short)
{
    TypeAwareArenaAllocator a;

    a.allocate<std::string>("12");
    a.clear();
}

TEST(ArenaAllocator_clear, string_long)
{
    TypeAwareArenaAllocator a;

    a.allocate<std::string>(LONG_STRING);
    a.clear();
}
