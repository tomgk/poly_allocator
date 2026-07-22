#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
#include <numeric>
#include "common.h"

#include "data.h"

TEST(ArenaAllocator_allocate, ForceReallocate)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(Data)+1;

    for(size_t i=0;i<count;++i)
        a.allocate<Data>(i);

    dump(a);
}

TEST(ArenaAllocator_allocate, ForceReallocate_Bytes)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(std::byte)+1;

    for(size_t i=0;i<count;++i)
        a.allocate<std::byte>((std::byte)i);

    dump(a);
}

TEST(ArenaAllocator_allocate, OneTypeWithoutResizeAndClear_Complex_Once)
{
    TypeAwareArenaAllocator a;

    a.allocateWithNoOffset<std::string>("#0");

    dump(a);
}

TEST(ArenaAllocator_allocate, string_noreallocate)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(Data);

    for(int i=0;i<count;++i)
    {
        a.allocateWithNoOffset<std::string>("#"+std::to_string(i));

        std::cout << i << std::endl;
    }

    dump(a);
}

TEST(ArenaAllocator_allocate, string_reallocate)
{
    TypeAwareArenaAllocator a;

    auto count = a.byteSize()/sizeof(Data)+1;

    for(int i=0;i<count;++i)
    {
        a.allocateWithNoOffset<std::string>("#"+std::to_string(i));

        std::cout << i << std::endl;
    }

    dump(a);
}

// A custom class that satisfies ArenaAllocatorConstructable and supports copying
struct Tracker
{
    int id;
    std::string tag;
    static inline int counter = 0;

    // Default constructor
    Tracker() : id(++counter), tag("Default") {}

    // Explicit copy constructor
    Tracker(const Tracker& other) : id(other.id), tag(other.tag + "_Copy") {}

    // Required special constructor to satisfy CopyWithOffsetConstructable for arena shifts
    Tracker(CopyWithOffsetConstruct, const Tracker& other, std::ptrdiff_t offset)
        : id(other.id), tag(other.tag) {}

    ~Tracker() {}
};

// Test fixture for cleaner arena test setups
class ArenaAllocatorTest : public ::testing::Test
{
protected:
    PlainArenaAllocator arena;
};

// Test to verify that every element in the allocated array is an independent copy
TEST_F(ArenaAllocatorTest, AllocateArrayCopyIsolationTest)
{
    // Arrange: Create a blueprint object on the stack
    Tracker blueprint;
    blueprint.id = 777;
    blueprint.tag = "Original";

    size_t count = 3;

    // Act: Allocate array where each element is initialized as a copy of the blueprint
    Tracker* array = arena.allocateArray<Tracker>(count, blueprint).data();

    // Assert 1: Verify all elements were correctly initialized from the blueprint
    ASSERT_NE(array, nullptr);
    // Explicitly using the template-parameter version of getArrayCount here
    EXPECT_EQ(arena.getArrayCount<Tracker>(array), count);

    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(array[i].id, 777);
        EXPECT_EQ(array[i].tag, "Original_Copy");
    }

    // Act 2: Modify a single element in the middle to test isolation
    array[1].id = 999;
    array[1].tag = "Modified";

    // Assert 2: Prove that modifying one element does NOT affect others or the blueprint
    // Check blueprint on stack remains untouched
    EXPECT_EQ(blueprint.id, 777);
    EXPECT_EQ(blueprint.tag, "Original");

    // Check surrounding elements in the arena remain untouched
    EXPECT_EQ(array[0].id, 777);
    EXPECT_EQ(array[0].tag, "Original_Copy");

    EXPECT_EQ(array[2].id, 777);
    EXPECT_EQ(array[2].tag, "Original_Copy");

    // Check target element was successfully changed
    EXPECT_EQ(array[1].id, 999);
    EXPECT_EQ(array[1].tag, "Modified");

    // Cleanup: Ensure no memory leaks when deallocating
    arena.deallocate(array);
}

// Test to verify the behavior with primitive types
///\todo at times this test fails randomly and then randomly starts working again
TEST_F(ArenaAllocatorTest, DISABLED_AllocatePrimitiveArrayTest)
{
    size_t count = 5;

    // Act: Allocate a zero-initialized primitive array
    int* zero_array = arena.allocateArray<int>(count, true).data();

    // Assert: Verify all elements are strictly 0
    ASSERT_NE(zero_array, nullptr);
    // Explicitly using the template-parameter version of getArrayCount here
    EXPECT_EQ(arena.getArrayCount<int>(zero_array), count);

    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(zero_array[i], 0);
    }

    arena.deallocate(zero_array);
}
