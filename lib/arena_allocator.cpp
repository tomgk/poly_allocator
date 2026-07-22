#include "arena_allocator.hpp"

#if defined(__GNUC__) && defined(__GLIBCXX__)
#include<cxxabi.h>
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

namespace
{
void test1(TypeAwareArenaAllocator &a)
{
    auto arr = a.allocateArray1<int>(10);
    arr.data();
    arr.size();
}
void test2(TypeAwareArenaAllocator &a)
{
    auto arr = a.allocateArray2<int>(10);
    arr.data();
    arr.size();
}
}