#ifndef DATA_H
#define DATA_H

#include<cstdint>

class Data
{
private:
    //use different bytes to distinguess values
    static constexpr uint32_t  ALIVE = 0x12345678;
    static constexpr uint32_t  DEAD  = 0x9ABCDEF0;
    uint32_t m_state = ALIVE;
    int m_value{};
public:
    Data(int value);
    ~Data();
};

#endif // DATA_H
