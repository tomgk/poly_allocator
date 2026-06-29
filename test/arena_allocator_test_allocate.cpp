#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
#include "common.h"

#include "data.h"

TEST(ArenaAllocator_allocate, OneTypeWithoutResizeAndClear_Complex_Once)
{
    TypeAwareArenaAllocator a;

    a.allocate<std::string>("#0");

    dump(a);
}

TEST(ArenaAllocator_allocate, OneType_Data_WithoutResizeAndClear_Complex_MultipleFull)
{
    TypeAwareArenaAllocator a;

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7/*70*/;++i)
    {
        a.allocate<Data>(i);

        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator_allocate, DISABLED_OneType_string_WithoutResizeAndClear_Complex_MultipleFull)
{
    TypeAwareArenaAllocator a;

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));

        std::cout << i << std::endl;
    }

    dump(a);
}
