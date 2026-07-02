#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
#include "common.h"

#include "data.h"

TEST(ArenaAllocator_allocate, ForceReallocate)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(Data)+1;

    for(size_t i=0;i<count;++i)
        a.allocateWithNoOffset<Data>(i);

    dump(a);
}

TEST(ArenaAllocator_allocate, ForceReallocate_Bytes)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(std::byte)+1;

    for(size_t i=0;i<count;++i)
        a.allocate<std::byte>((std::byte)i);

    dump(a);
}

TEST(ArenaAllocator_allocate, OneTypeWithoutResizeAndClear_Complex_Once)
{
    TypeAwareArenaAllocator a;

    a.allocateWithNoOffset<std::string>("#0");

    dump(a);
}

TEST(ArenaAllocator_allocate, string_noreallocate)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(Data);

    for(int i=0;i<count;++i)
    {
        a.allocateWithNoOffset<std::string>("#"+std::to_string(i));

        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator_allocate, string_reallocate)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(Data)+1;

    for(int i=0;i<count;++i)
    {
        a.allocateWithNoOffset<std::string>("#"+std::to_string(i));

        std::cout << i << std::endl;
    }

    dump(a);
}
