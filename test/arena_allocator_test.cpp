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

    auto ptr = a.allocate<std::string>("12");
    a.deallocate(ptr);
}

TEST(ArenaAllocator_deallocate, string_long)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<std::string>(LONG_STRING);
    a.deallocate(ptr);
}

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

TEST(ArenaAllocator_allocate, OneTypeWithoutResizeAndClear_Complex_Once)
{
    TypeAwareArenaAllocator a;

    a.allocate<std::string>("#0");

    dump(a);
}

TEST(ArenaAllocator_allocate, DISABLED_OneTypeWithoutResizeAndClear_Complex_MultipleFull)
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

TEST(ArenaAllocator, DISABLED_MixedWithoutResizeAndClear)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    std::cout << "capacity: " << a.get_capacity() << std::endl;
    a.get_used_bytes();

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7/*256*/;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator, DISABLED_MixedWithoutResize)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    std::cout << "capacity: " << a.get_capacity() << std::endl;
    a.get_used_bytes();
    a.clear();

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7/*256*/;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator, DISABLED_MixedWithResize)
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    std::cout << "capacity: " << a.get_capacity() << std::endl;
    a.get_used_bytes();
    //TODO: add test for that
    //TODO: removing this line makes it crash for some reason
    a.clear();

    for(int i=0;i<8;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);

    std::cout << "Starting deallocation..." << std::endl;
}

void test()
{
    TypeAwareArenaAllocator a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    a.get_capacity();
    a.get_used_bytes();
    a.clear();

    for(auto iter = a.begin();iter != a.end(); ++iter)
    {
        iter.get_header();
        iter.get_size();
        iter.get<int>();
    }
}

#include "exception_handling.h"

int main(int argc, char **argv)
{
    installExceptionHandlers();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
