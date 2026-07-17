#include <gtest/gtest.h>
#include "arena_allocator.hpp"

/**
 * @brief Test fixture for ArenaBufferHandle reallocation safety tests
 * 
 * These tests assume the updated ArenaBufferHandle template with ValueType parameter,
 * and allocateArray functions returning ArenaBufferHandle<M, T>
 */
class ArenaBufferHandleReallocationTest : public ::testing::Test
{
protected:
    PlainArenaAllocator arena;
};

/**
 * @brief Test that typed array handle survives arena reallocation with primitives
 * 
 * This test verifies that:
 * 1. allocateArray<T> returns ArenaBufferHandle<M, T>
 * 2. Data is correctly initialized before reallocation
 * 3. After forcing reallocation, the handle automatically resolves to the new location
 * 4. All data values remain unchanged after reallocation
 */
TEST_F(ArenaBufferHandleReallocationTest, IntArrayHandleValidAfterReallocation)
{
    // Arrange: Allocate an array via the new interface returning ArenaBufferHandle<M, int>
    const std::size_t count = 5;
    auto handle = arena.allocateArray<int>(count);
    
    // Initialize with known values
    for (std::size_t i = 0; i < count; ++i)
    {
        handle[i] = static_cast<int>(i * 100);  // 0, 100, 200, 300, 400
    }
    
    // Verify initial state before reallocation
    EXPECT_EQ(handle.size(), count);
    for (std::size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(handle[i], static_cast<int>(i * 100));
    }
    
    // Act: Force a reallocation to move all objects to a new buffer
    arena.forceReallocate();
    
    // Assert: Handle should still point to valid data with unchanged values
    EXPECT_EQ(handle.size(), count);
    for (std::size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(handle[i], static_cast<int>(i * 100)) 
            << "Element at index " << i << " corrupted after reallocation";
    }
}

/**
 * @brief Test array handle with copy-initialized values survives reallocation
 * 
 * This test verifies that:
 * 1. allocateArray<T>(count, value) returns ArenaBufferHandle<M, T>
 * 2. All elements are correctly initialized as copies before reallocation
 * 3. After reallocation, all copies remain intact and unchanged
 */
TEST_F(ArenaBufferHandleReallocationTest, CopyInitializedArrayHandleValidAfterReallocation)
{
    // Arrange: Allocate array initialized with copies of a single value
    const std::size_t count = 4;
    int value = 42;
    auto handle = arena.allocateArray<int>(count, value);
    
    // Verify all elements are copies of the value before reallocation
    ASSERT_EQ(handle.size(), count);
    for (std::size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(handle[i], 42);
    }
    
    // Modify a single element to verify independence
    handle[1] = 99;
    EXPECT_EQ(handle[1], 99);
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: All values preserved (including the modification)
    ASSERT_EQ(handle.size(), count);
    EXPECT_EQ(handle[0], 42);
    EXPECT_EQ(handle[1], 99);
    EXPECT_EQ(handle[2], 42);
    EXPECT_EQ(handle[3], 42);
}

/**
 * @brief Test that multiple typed array handles remain valid after reallocation
 * 
 * This test verifies that:
 * 1. Multiple allocations of different types can coexist
 * 2. All handles survive a single reallocation event
 * 3. Each handle independently resolves to the correct new location
 * 4. Data in all arrays remains consistent
 */
TEST_F(ArenaBufferHandleReallocationTest, MultipleTypedHandlesValidAfterReallocation)
{
    // Arrange: Allocate multiple arrays of different types
    auto int_handle = arena.allocateArray<int>(3);
    auto double_handle = arena.allocateArray<double>(4);
    auto char_handle = arena.allocateArray<char>(2);
    
    // Initialize each array with distinct patterns
    int_handle[0] = 10;  int_handle[1] = 20;  int_handle[2] = 30;
    double_handle[0] = 1.1; double_handle[1] = 2.2; 
    double_handle[2] = 3.3; double_handle[3] = 4.4;
    char_handle[0] = 'A'; char_handle[1] = 'B';
    
    // Record pointers before reallocation (they will change)
    const int* ptr_int_before = int_handle.data();
    const double* ptr_double_before = double_handle.data();
    const char* ptr_char_before = char_handle.data();
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: Pointers should have changed (different buffer)
    EXPECT_NE(int_handle.data(), ptr_int_before);
    EXPECT_NE(double_handle.data(), ptr_double_before);
    EXPECT_NE(char_handle.data(), ptr_char_before);
    
    // Assert: All data remains intact and unchanged
    ASSERT_EQ(int_handle.size(), 3);
    EXPECT_EQ(int_handle[0], 10);
    EXPECT_EQ(int_handle[1], 20);
    EXPECT_EQ(int_handle[2], 30);
    
    ASSERT_EQ(double_handle.size(), 4);
    EXPECT_EQ(double_handle[0], 1.1);
    EXPECT_EQ(double_handle[1], 2.2);
    EXPECT_EQ(double_handle[2], 3.3);
    EXPECT_EQ(double_handle[3], 4.4);
    
    ASSERT_EQ(char_handle.size(), 2);
    EXPECT_EQ(char_handle[0], 'A');
    EXPECT_EQ(char_handle[1], 'B');
}

/**
 * @brief Test that array handle iterators function correctly after reallocation
 * 
 * This test verifies that:
 * 1. begin() and end() iterators work before reallocation
 * 2. After reallocation, iterators still traverse the array correctly
 * 3. Iterator arithmetic (++ operator) functions correctly on the new buffer
 */
TEST_F(ArenaBufferHandleReallocationTest, IteratorValidAfterReallocation)
{
    // Arrange: Create an array and populate it
    const std::size_t count = 5;
    auto handle = arena.allocateArray<int>(count);
    
    // Fill with values
    for (std::size_t i = 0; i < count; ++i)
    {
        handle[i] = static_cast<int>(10 + i);  // 10, 11, 12, 13, 14
    }
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: Iterate through handle and verify all values
    std::vector<int> values;
    for (auto it = handle.begin(); it != handle.end(); ++it)
    {
        values.push_back(*it);
    }
    
    ASSERT_EQ(values.size(), count);
    for (std::size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(values[i], static_cast<int>(10 + i));
    }
}

/**
 * @brief Test that element access methods work correctly after reallocation
 * 
 * This test verifies that:
 * 1. operator[] works before and after reallocation
 * 2. at() method works and performs bounds checking after reallocation
 * 3. front() and back() return correct elements after reallocation
 * 4. data() returns a valid pointer after reallocation
 */
TEST_F(ArenaBufferHandleReallocationTest, ElementAccessMethodsValidAfterReallocation)
{
    // Arrange: Create array with sentinel values
    const std::size_t count = 4;
    auto handle = arena.allocateArray<int>(count);
    
    handle[0] = 111;
    handle[1] = 222;
    handle[2] = 333;
    handle[3] = 444;
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: All element access methods work correctly
    EXPECT_EQ(handle[0], 111);
    EXPECT_EQ(handle[1], 222);
    EXPECT_EQ(handle[2], 333);
    EXPECT_EQ(handle[3], 444);
    
    // Test at() method
    EXPECT_EQ(handle.at(0), 111);
    EXPECT_EQ(handle.at(3), 444);
    EXPECT_THROW(handle.at(10), std::out_of_range);
    
    // Test front() and back()
    EXPECT_EQ(handle.front(), 111);
    EXPECT_EQ(handle.back(), 444);
    
    // Test data() returns valid pointer
    int* ptr = handle.data();
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 111);
}

/**
 * @brief Test that capacity observer methods work correctly after reallocation
 * 
 * This test verifies that:
 * 1. size() returns correct count after reallocation
 * 2. empty() works correctly after reallocation
 * 3. max_size() returns the allocated size
 */
TEST_F(ArenaBufferHandleReallocationTest, CapacityObserversValidAfterReallocation)
{
    // Arrange: Create array
    const std::size_t count = 7;
    auto handle = arena.allocateArray<double>(count);
    
    // Verify before reallocation
    EXPECT_EQ(handle.size(), count);
    EXPECT_FALSE(handle.empty());
    EXPECT_EQ(handle.max_size(), count);
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: All capacity methods still report correct values
    EXPECT_EQ(handle.size(), count);
    EXPECT_FALSE(handle.empty());
    EXPECT_EQ(handle.max_size(), count);
}

/**
 * @brief Test handle to empty array allocation
 * 
 * This test verifies that:
 * 1. Empty allocations create empty handles
 * 2. Empty handles behave safely after attempted reallocation
 */
TEST_F(ArenaBufferHandleReallocationTest, EmptyHandleRemainsValidAfterReallocation)
{
    // Arrange: Create empty handle
    auto empty_handle = arena.allocateArray<int>(0);
    
    // Verify it's empty
    EXPECT_TRUE(empty_handle.empty());
    EXPECT_EQ(empty_handle.size(), 0);
    EXPECT_EQ(empty_handle.data(), nullptr);
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: Empty handle should still be empty and safe
    EXPECT_TRUE(empty_handle.empty());
    EXPECT_EQ(empty_handle.size(), 0);
    EXPECT_EQ(empty_handle.data(), nullptr);
}

/**
 * @brief Test array handle validity through multiple sequential reallocations
 * 
 * This test verifies that:
 * 1. Handles survive multiple consecutive reallocations
 * 2. Data remains consistent through all reallocation events
 */
TEST_F(ArenaBufferHandleReallocationTest, HandleValidAfterMultipleReallocations)
{
    // Arrange: Create handles
    auto handle1 = arena.allocateArray<int>(3);
    auto handle2 = arena.allocateArray<int>(2);
    
    handle1[0] = 100; handle1[1] = 200; handle1[2] = 300;
    handle2[0] = 10;  handle2[1] = 20;
    
    // Act & Assert: Multiple reallocation cycles
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        arena.forceReallocate();
        
        // Verify data integrity after each reallocation
        ASSERT_EQ(handle1.size(), 3) << "Cycle " << cycle;
        EXPECT_EQ(handle1[0], 100) << "Cycle " << cycle;
        EXPECT_EQ(handle1[1], 200) << "Cycle " << cycle;
        EXPECT_EQ(handle1[2], 300) << "Cycle " << cycle;
        
        ASSERT_EQ(handle2.size(), 2) << "Cycle " << cycle;
        EXPECT_EQ(handle2[0], 10) << "Cycle " << cycle;
        EXPECT_EQ(handle2[1], 20) << "Cycle " << cycle;
    }
}

/**
 * @brief Test handle with zero-initialized array survives reallocation
 * 
 * This test verifies that:
 * 1. allocateArray with zero_initialize=true returns valid handle
 * 2. All elements are zero-initialized before reallocation
 * 3. Zero values survive reallocation
 */
TEST_F(ArenaBufferHandleReallocationTest, ZeroInitializedArrayHandleValidAfterReallocation)
{
    // Arrange: Allocate and zero-initialize
    const std::size_t count = 5;
    auto handle = arena.allocateArray<int>(count, true);
    
    // Verify zero-initialization
    ASSERT_EQ(handle.size(), count);
    for (std::size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(handle[i], 0);
    }
    
    // Modify some values
    handle[1] = 11;
    handle[3] = 33;
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: Values preserved after reallocation
    ASSERT_EQ(handle.size(), count);
    EXPECT_EQ(handle[0], 0);
    EXPECT_EQ(handle[1], 11);
    EXPECT_EQ(handle[2], 0);
    EXPECT_EQ(handle[3], 33);
    EXPECT_EQ(handle[4], 0);
}

/**
 * @brief Stress test: many handles of various types through multiple reallocations
 * 
 * This test creates a complex scenario with multiple allocations
 * of different types and sizes, then forces reallocation multiple times
 * to ensure all handles remain valid and consistent.
 */
TEST_F(ArenaBufferHandleReallocationTest, StressTestManyHandles)
{
    // Arrange: Create many handles of different sizes and types
    std::vector<decltype(arena.allocateArray<int>(0))> int_handles;
    std::vector<decltype(arena.allocateArray<double>(0))> double_handles;
    std::vector<decltype(arena.allocateArray<char>(0))> char_handles;
    
    // Allocate 5 int arrays
    for (int i = 1; i <= 5; ++i)
    {
        auto h = arena.allocateArray<int>(i);
        for (int j = 0; j < i; ++j)
        {
            h[j] = (i * 100) + j;
        }
        int_handles.push_back(h);
    }
    
    // Allocate 5 double arrays
    for (int i = 1; i <= 5; ++i)
    {
        auto h = arena.allocateArray<double>(i);
        for (int j = 0; j < i; ++j)
        {
            h[j] = (i * 100) + j + 0.5;
        }
        double_handles.push_back(h);
    }
    
    // Allocate 5 char arrays
    for (int i = 1; i <= 5; ++i)
    {
        auto h = arena.allocateArray<char>(i);
        for (int j = 0; j < i; ++j)
        {
            h[j] = static_cast<char>('A' + j);
        }
        char_handles.push_back(h);
    }
    
    // Act: Force multiple reallocations
    for (int r = 0; r < 2; ++r)
    {
        arena.forceReallocate();
        
        // Assert: Verify all int handles
        for (int i = 1; i <= 5; ++i)
        {
            auto& h = int_handles[i - 1];
            ASSERT_EQ(h.size(), static_cast<std::size_t>(i)) << "Int handle " << i << " reallocation " << r;
            for (int j = 0; j < i; ++j)
            {
                EXPECT_EQ(h[j], (i * 100) + j) 
                    << "Int handle " << i << " element " << j << " reallocation " << r;
            }
        }
        
        // Assert: Verify all double handles
        for (int i = 1; i <= 5; ++i)
        {
            auto& h = double_handles[i - 1];
            ASSERT_EQ(h.size(), static_cast<std::size_t>(i)) << "Double handle " << i << " reallocation " << r;
            for (int j = 0; j < i; ++j)
            {
                EXPECT_EQ(h[j], (i * 100) + j + 0.5)
                    << "Double handle " << i << " element " << j << " reallocation " << r;
            }
        }
        
        // Assert: Verify all char handles
        for (int i = 1; i <= 5; ++i)
        {
            auto& h = char_handles[i - 1];
            ASSERT_EQ(h.size(), static_cast<std::size_t>(i)) << "Char handle " << i << " reallocation " << r;
            for (int j = 0; j < i; ++j)
            {
                EXPECT_EQ(h[j], static_cast<char>('A' + j))
                    << "Char handle " << i << " element " << j << " reallocation " << r;
            }
        }
    }
}

/**
 * @brief Test const access to handles works correctly after reallocation
 * 
 * This test verifies that const access methods work correctly
 * after reallocation.
 */
TEST_F(ArenaBufferHandleReallocationTest, ConstAccessValidAfterReallocation)
{
    // Arrange: Create handle
    const std::size_t count = 5;
    auto handle = arena.allocateArray<int>(count);
    
    for (std::size_t i = 0; i < count; ++i)
    {
        handle[i] = static_cast<int>(i * 3);
    }
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: Const access should work
    const auto& const_handle = handle;
    EXPECT_EQ(const_handle.size(), count);
    EXPECT_FALSE(const_handle.empty());
    
    for (std::size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(const_handle[i], static_cast<int>(i * 3));
    }
    
    const int* const_ptr = const_handle.data();
    EXPECT_NE(const_ptr, nullptr);
    EXPECT_EQ(*const_ptr, 0);
}

/**
 * @brief Test cbegin() and cend() const iterators after reallocation
 * 
 * This test verifies that const iterator methods work correctly
 * after reallocation.
 */
TEST_F(ArenaBufferHandleReallocationTest, ConstIteratorsValidAfterReallocation)
{
    // Arrange: Create and populate array
    const std::size_t count = 4;
    auto handle = arena.allocateArray<int>(count);
    
    handle[0] = 10;
    handle[1] = 20;
    handle[2] = 30;
    handle[3] = 40;
    
    // Act: Force reallocation
    arena.forceReallocate();
    
    // Assert: Const iterators should work
    const auto& const_handle = handle;
    std::vector<int> values;
    
    for (auto it = const_handle.cbegin(); it != const_handle.cend(); ++it)
    {
        values.push_back(*it);
    }
    
    ASSERT_EQ(values.size(), count);
    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 30);
    EXPECT_EQ(values[3], 40);
}
