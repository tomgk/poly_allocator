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
        a.allocate<Data>(i);

    dump(a);
}

TEST(ArenaAllocator_allocate, OneTypeWithoutResizeAndClear_Complex_Once)
{
    TypeAwareArenaAllocator a;

    a.allocate<std::string>("#0");

    dump(a);
}

TEST(ArenaAllocator_allocate, OneType_string_WithoutResizeAndClear_Complex_MultipleFull)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(Data);

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));

        std::cout << i << std::endl;
    }

    dump(a);
}
