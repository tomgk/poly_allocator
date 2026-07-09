#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
#include <numeric>
#include "common.h"

#include "data.h"

class ArenaArrayViewTest : public ::testing::Test
{
protected:
    ArenaAllocator<StoreTypeInfoType::no> arena;
};

TEST_F(ArenaArrayViewTest, ContainerInterfaceVerificationTest)
{
    // Arrange: Allocate an array of 5 integers, zero-initialized
    std::size_t count = 5;
    ArenaArrayView<int> view = arena.allocateArray<int>(count, true);

    // Assert 1: Verify data() and size() methods
    ASSERT_NE(view.data(), nullptr);
    EXPECT_EQ(view.size(), count);

    // Act: Fill the view with sequential values using the data pointer or index
    std::iota(view.begin(), view.end(), 10); // Fills with 10, 11, 12, 13, 14

    // Assert 2: Verify values via index operator
    EXPECT_EQ(view[0], 10);
    EXPECT_EQ(view[4], 14);

    // Assert 3: Verify modern range-based for loop works (uses begin() and end())
    int expected_value = 10;
    for (int val : view)
    {
        EXPECT_EQ(val, expected_value);
        expected_value++;
    }

    // Assert 4: Verify getArrayCount with type parameter still works perfectly via data()
    EXPECT_EQ(arena.getArrayCount<int>(view.data()), count);

    // Cleanup
    arena.deallocate(view.data());
}
