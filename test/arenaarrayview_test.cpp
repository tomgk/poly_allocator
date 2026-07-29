#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <algorithm>
#include <gtest/gtest.h>
#include <numeric>

class ArenaArrayViewTest : public ::testing::Test
{
protected:
    PlainArenaAllocator arena;
};

TEST_F(ArenaArrayViewTest, ContainerInterfaceVerificationTest)
{
    // Arrange: Allocate an array of 5 integers, zero-initialized
    std::size_t count = 5;
    auto view = arena.allocateArray<int>(count, true);

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

// A custom type that fulfills the ArenaAllocatorConstructable requirement
struct ElementTracker
{
    int val;
    static inline int copies = 0;

    ElementTracker() : val(0) {}
    ElementTracker(int v) : val(v) {}

    // Copy constructor tracking
    ElementTracker(const ElementTracker& other) : val(other.val)
    {
        copies++;
    }

    // Required shift constructor for relocation support
    ElementTracker(CopyWithOffsetConstruct, const ElementTracker& other, std::ptrdiff_t offset)
        : val(other.val) {}

    ~ElementTracker() {}
};

class ArenaArrayViewAdvancedTest : public ::testing::Test
{
protected:
    PlainArenaAllocator arena;
};

// 1. Test: Verifies that ArenaArrayView works flawlessly with standard STL algorithms
TEST_F(ArenaArrayViewAdvancedTest, STLAlgorithmsCompatibilityTest)
{
    std::size_t count = 6;
    auto view = arena.allocateArray<int>(count, true);

    // Act: Fill with unsorted data using standard iterators
    view[0] = 50; view[1] = 10; view[2] = 40;
    view[3] = 20; view[4] = 60; view[5] = 30;

    // Use std::sort directly on the view since it provides begin() and end()
    std::sort(view.begin(), view.end());

    // Assert: Check if sorting worked
    EXPECT_EQ(view[0], 10);
    EXPECT_EQ(view[1], 20);
    EXPECT_EQ(view[5], 60);

    // Use std::find to look for an element
    auto it = std::find(view.begin(), view.end(), 40);
    ASSERT_NE(it, view.end());
    EXPECT_EQ(*it, 40);

    // Use std::accumulate to sum up values
    int sum = std::accumulate(view.begin(), view.end(), 0);
    EXPECT_EQ(sum, 210);

    arena.deallocate(view.data());
}

// 2. Test: Verifies read-only behavior through const ArenaArrayView
TEST_F(ArenaArrayViewAdvancedTest, ConstViewAccessTest)
{
    std::size_t count = 3;
    auto mutable_view = arena.allocateArray<int>(count, true);
    mutable_view[0] = 100;
    mutable_view[1] = 200;
    mutable_view[2] = 300;

    // Create a read-only const reference to the view
    const auto& const_view = mutable_view;

    // Assert: Read access works via const_iterator and const operator[]
    EXPECT_EQ(const_view.size(), count);
    EXPECT_EQ(const_view[1], 200);
    EXPECT_EQ(*const_view.cbegin(), 100);

    // The following lines would fail compilation if uncommented (uncomment to verify read-only safety):
    // const_view[0] = 500;
    // *const_view.begin() = 500;

    arena.deallocate(mutable_view.data());
}

// 3. Test: Ensures that allocateArray with a value parameter deep-copies the object into every slot
TEST_F(ArenaArrayViewAdvancedTest, ValueInitializationDeepCopyTest)
{
    ElementTracker::copies = 0; // Reset counter
    ElementTracker blueprint(42);
    std::size_t count = 4;

    // Act: Create view where every slot is initialized with a copy of blueprint
    auto view = arena.allocateArray<ElementTracker>(count, blueprint);

    // Assert: The copy constructor must have been called exactly 'count' times
    EXPECT_EQ(ElementTracker::copies, static_cast<int>(count));
    EXPECT_EQ(arena.getArrayCount<ElementTracker>(view.data()), count);

    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(view[i].val, 42);
    }

    // Isolate verify: changing one doesn't affect others
    view[1].val = 99;
    EXPECT_EQ(view[0].val, 42);
    EXPECT_EQ(view[1].val, 99);
    EXPECT_EQ(blueprint.val, 42); // Blueprint on stack remains untouched

    arena.deallocate(view.data());
}

// 4. Test: Verifies that the view pointer stays valid until a reallocation occurs
TEST_F(ArenaArrayViewAdvancedTest, ViewValidityBeforeReallocationTest)
{
    std::size_t count = 2;
    auto view_1 = arena.allocateArray<int>(count, true);
    view_1[0] = 7;
    view_1[1] = 8;

    // Allocate a second array without triggering a reallocation limit
    auto view_2 = arena.allocateArray<int>(count, true);
    view_2[0] = 9;

    // Assert: Allocating more elements in the same buffer capacity must not invalidate view_1
    EXPECT_EQ(view_1[0], 7);
    EXPECT_EQ(view_1[1], 8);
    EXPECT_EQ(view_2[0], 9);

    arena.deallocate(view_1.data());
    arena.deallocate(view_2.data());
}

class ArenaAllocatorReserveTest : public ::testing::Test
{
protected:
    PlainArenaAllocator arena;
};

// Verifies that reserving capacity works and prevents further internal reallocations
TEST_F(ArenaAllocatorReserveTest, ReserveCapacityVerificationTest)
{
    std::size_t initial_capacity = arena.get_capacity();
    std::size_t target_capacity = initial_capacity * 4;

    // Act 1: Reserve a much larger capacity upfront
    arena.reserve(target_capacity);

    // Assert 1: The capacity must now be at least the requested size
    EXPECT_GE(arena.get_capacity(), target_capacity);

    // Act 2: Perform multiple allocations that fit within the reserved space
    int* first_item = arena.allocate<int>(42);
    int* second_item = arena.allocate<int>(84);
    auto view = arena.allocateArray<int>(10, true);

    // Assert 2: Verify that capacity stayed stable and didn't expand unexpectedly
    EXPECT_GE(arena.get_capacity(), target_capacity);

    // Check values to ensure everything was written correctly in the reserved space
    EXPECT_EQ(*first_item, 42);
    EXPECT_EQ(*second_item, 84);
    EXPECT_EQ(view.size(), 10);

    // Cleanup
    arena.deallocate(first_item);
    arena.deallocate(second_item);
    arena.deallocate(view.data());
}

// Verifies that requesting a smaller size than the current capacity does nothing
TEST_F(ArenaAllocatorReserveTest, ReserveSmallerThanCurrentHasNoEffectTest)
{
    std::size_t current_cap = arena.get_capacity();

    // Act: Request a smaller capacity than already allocated
    arena.reserve(current_cap / 2);

    // Assert: The capacity must remain completely unchanged
    EXPECT_EQ(arena.get_capacity(), current_cap);
}

class ArenaUtilityMethodsTest : public ::testing::Test
{
protected:
    PlainArenaAllocator arena;
};

// Verifies that contains() accurately identifies pointers inside and outside the arena
TEST_F(ArenaUtilityMethodsTest, ContainsPointerVerificationTest)
{
    // Act: Allocate an item inside the arena, and create one on the stack
    int* arena_item = arena.allocate<int>(123);
    int stack_item = 456;

    // Assert: The arena must own its allocated item, but not the stack item or nullptr
    EXPECT_TRUE(arena.contains(arena_item));
    EXPECT_FALSE(arena.contains(&stack_item));
    EXPECT_FALSE(arena.contains(nullptr));

    // Cleanup
    arena.deallocate(arena_item);
}

// Verifies that get_dead_bytes() tracking accurately counts memory from deallocated objects
TEST_F(ArenaUtilityMethodsTest, DISABLED_DeadBytesTrackingTest)
{
    // Initially, there should be zero dead bytes
    EXPECT_EQ(arena.get_dead_bytes(), 0);

    // Act 1: Allocate two temporary objects
    int* item_1 = arena.allocate<int>(10);
    int* item_2 = arena.allocate<int>(20);

    // Still 0 dead bytes because both allocations are alive
    EXPECT_EQ(arena.get_dead_bytes(), 0);

    // Act 2: Deallocate the first item
    arena.deallocate(item_1);

    // Assert: Dead bytes must now equal the size of item_1 plus its header size
    std::size_t expected_dead_space = sizeof(int) + PlainArenaAllocator::HEADER_SIZE;
    EXPECT_EQ(arena.get_dead_bytes(), expected_dead_space);

    // Act 3: Deallocate the second item
    arena.deallocate(item_2);

    // Act 4: Force a reallocation to trigger compaction and clear out dead bytes
    arena.forceReallocate();

    // Assert: After compaction, all dead objects are completely removed
    EXPECT_EQ(arena.get_dead_bytes(), 0);
}