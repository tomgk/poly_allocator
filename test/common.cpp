#include "common.h"

std::string LONG_STRING = []{
    std::string str;

    for(size_t i=0;i<sizeof(std::string)+1;++i)
        str += ('A'+i);

    return str;
}();


void dump0(const ArenaAllocator<StoreTypeInfoType::yes> &a)
{
    for(auto iter=a.begin();iter!=a.end();++iter)
    {
        const std::type_info &type = *iter.get_header().type_info;
        std::cout << type.name();

        if(typeid(std::string) == type)
        {
            std::cout << "STR " << iter.get<std::string>();
        }
        else if(typeid(int) == type)
        {
            std::cout << "INT " << iter.get<int>();
        }

        std::cout << std::endl;
    }

    std::cout << "capacity: " << a.get_capacity() << std::endl;
}

void dump(const TypeAwareArenaAllocator &a)
{
    if(false)
    {
        TypeAwareArenaAllocator::ConstIterator iter = a.begin();
        const auto &val = *iter;
        //const TypeAwareArenaAllocator::Entry<true> &val = *iter;
    }

    //INFO: keep type non-auto to catch compiler errors after changes
    for(const TypeAwareArenaAllocator::Entry<EntryConstness::yes> &val : a)
    {
        const std::type_info &type = *val.get_header().type_info;
        std::cout << type.name();

        if(typeid(std::string) == type)
        {
            std::cout << "STR " << val.get<std::string>();
        }
        else if(typeid(int) == type)
        {
            std::cout << "INT " << val.get<int>();
        }

        std::cout << std::endl;
    }

    std::cout << "capacity: " << a.get_capacity() << std::endl;
}
