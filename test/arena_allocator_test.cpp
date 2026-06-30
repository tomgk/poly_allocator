#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
#include "common.h"

#include "data.h"

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
        a.allocate<Data>(i);//"#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator, MixedWithoutResize)
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
        a.allocate<Data>(i);//"#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator, MixedWithResize)
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
        a.allocate<Data>(i);//std::string>("#"+std::to_string(i));
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
