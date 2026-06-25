#include "data.h"

#include <gtest/gtest.h>

Data::Data(int value):
    m_value(value)
{

}

Data::~Data()
{
    if(m_state != ALIVE)
        EXPECT_TRUE(false) << (void*)this << " is already dead";
    else
        m_state = DEAD;
}
