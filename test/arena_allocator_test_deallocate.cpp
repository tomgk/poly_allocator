#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
#include "common.h"

#include "data.h"

TEST(ArenaAllocator_deallocate, Data)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<Data>(12);
    a.deallocate(ptr);
}

TEST(ArenaAllocator_deallocate, string_short)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocateWithNoOffset<std::string>("12");
    a.deallocate(ptr);
}

TEST(ArenaAllocator_deallocate, string_long)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocateWithNoOffset<std::string>(LONG_STRING);
    a.deallocate(ptr);
}
