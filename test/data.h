#ifndef DATA_H
#define DATA_H

#include<cstdint>
#include "arena_allocator.hpp"

class Data
{
private:
    //use different bytes to distinguess values
    static constexpr uint32_t  ALIVE = 0x12345678;
    static constexpr uint32_t  DEAD  = 0x9ABCDEF0;
    //store own address to detect raw copy
    Data *m_self{};
    Data *m_next{};
    uint32_t m_state = ALIVE;
    int m_value{};
    //just to
    //char waste[50];
public:
    Data(int value);
    Data(const Data &d) = delete;
    Data(CopyWithOffsetConstruct, const Data &d, std::ptrdiff_t offset);
    Data &operator=(const Data &d);
    ~Data();
};

#endif // DATA_H
