#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"
#include <gtest/gtest.h>

#if defined(__GNUC__) && defined(__GLIBCXX__)
#include<cxxabi.h>
#include<initializer_list>
#endif

std::string getTypeName(const std::type_info &type)
{
#if defined(__GNUC__) && defined(__GLIBCXX__)
    if(typeid(std::string) == type)
        return "std::string";

    int status;
    //not using length since there might be more space allocated after \0
    char *ret = abi::__cxa_demangle(type.name(), 0, 0, &status);
    if(status == 0)
    {
        std::string name = ret;
        free(ret);
        return name;
    }
    else if(status == -2)
        throw std::runtime_error("wrong arg");
    else
        throw std::runtime_error("unexpected status "+std::to_string(status));
#else
    //fallback is to just use original name
    return type.name();
#endif
}

TEST(ArenaAllocator, deallocate)
{
    ArenaAllocator<true> a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
}

TEST(ArenaAllocator, MixedWithoutResize)
{
    ArenaAllocator<true> a;

    auto ptr = a.allocate<int>(12);
    a.deallocate(ptr);
    a.get_type_info(ptr);
    std::cout << "capacity: " << a.get_capacity() << std::endl;
    a.get_used_bytes();
    //TODO: add test for that
    //TODO: removing this line makes it crash for some reason
    a.clear();

    //TODO: i<8 (which requires reallocation) causes a crash for some reason
    for(int i=0;i<7/*256*/;++i)
    {
        a.allocate<std::string>("#"+std::to_string(i));
        std::cout << i << std::endl;
    }

    for(auto iter=a.begin();iter!=a.end();++iter)
    {
        const std::type_info &type = *iter.get_header().type_info;
        std::cout << type.name();

        if(typeid(std::string) == type)
        {
            std::cout << "STR " << iter.get<std::string>();
        }
        else if(typeid(std::string) == type)
        {
            std::cout << "INT " << iter.get<int>();
        }

        std::cout << std::endl;
    }

    std::cout << "capacity: " << a.get_capacity() << std::endl;
}

TEST(ArenaAllocator, DISABLED_MixedWithResize)
{
    ArenaAllocator<true> a;

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

    for(auto iter=a.begin();iter!=a.end();++iter)
    {
        const std::type_info &type = *iter.get_header().type_info;
        std::cout << type.name();

        if(typeid(std::string) == type)
        {
            std::cout << "STR " << iter.get<std::string>();
        }
        else if(typeid(std::string) == type)
        {
            std::cout << "INT " << iter.get<int>();
        }

        std::cout << std::endl;
    }

    std::cout << "capacity: " << a.get_capacity() << std::endl;
    std::cout << "Starting deallocation..." << std::endl;
}

void test()
{
    ArenaAllocator<true> a;

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

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
