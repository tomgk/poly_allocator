#include "data.h"

#include <gtest/gtest.h>

Data::Data(int value):
    m_value(value), m_self(this)
{

}

Data::Data(const Data &d): m_self(this), m_value(d.m_value)
{
    std::cout << "CCopy " << (void*)this << " from " << (void*)&d << std::endl;
}

Data &Data::operator=(const Data &d)
{
    std::cout << "ACopy " << (void*)this << " from " << (void*)&d << std::endl;
    m_value = d.m_value;
    return *this;
}

Data::~Data()
{
    if(m_state != ALIVE)
        EXPECT_TRUE(false) << (void*)this << " is already dead";
    else
        m_state = DEAD;

    if(m_self != this)
        EXPECT_TRUE(false) << (void*)this << " was moved by raw copy to " << (void*)m_self;
}
