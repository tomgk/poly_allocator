#define ARENA_ALLOCATOR_LOG
#include "arena_allocator.hpp"

#include <gtest/gtest.h>
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
// A custom class to demonstrate non-primitive array behavior
struct Tracker
{
    int id;
    static inline int counter = 0;

    // 1. Standard constructor for default allocation
    Tracker() : id(++counter)
    {
        std::cout << "  [Tracker] Default constructed object #" << id << "\n";
    }

    // 2. The required special constructor to satisfy CopyWithOffsetConstructable
    Tracker(CopyWithOffsetConstruct, const Tracker& other, std::ptrdiff_t offset)
        : id(other.id) // Copy data fields from the source object
    {
        // If Tracker contained internal pointers to other arena locations,
        // they would be recalculated here using the 'offset' parameter.
        std::cout << "  [Tracker] Relocated object #" << id << " with offset: " << offset << "\n";
    }

    ~Tracker()
    {
        std::cout << "  [Tracker] Destructed object #" << id << "\n";
    }
};

TEST(ArenaAllocator_allocate, array)
{
    ArenaAllocator<StoreTypeInfoType::yes> arena;

    std::cout << "--- 1. Allocating uninitialized primitive array ---\n";
    // Allocates space for 5 integers. Performance is blazing fast because
    // zero_initialize defaults to false, skipping any loops or memsets.
    int* fast_array = arena.allocateArray<int>(5);

    // Assign values manually
    for (size_t i = 0; i < 5; ++i)
    {
        fast_array[i] = static_cast<int>(i * 10);
    }

    std::cout << "Fast array values: ";
    for (size_t i = 0; i < 5; ++i) std::cout << fast_array[i] << " ";
    std::cout << "\n\n";


    std::cout << "--- 2. Allocating zero-initialized primitive array ---\n";
    // Passing 'true' activates std::memset underneath to instantly clear the block
    int* zero_array = arena.allocateArray<int>(5, true);

    std::cout << "Zero array values (guaranteed 0): ";
    for (size_t i = 0; i < 5; ++i) std::cout << zero_array[i] << " ";
    std::cout << "\n\n";


    std::cout << "--- 3. Using getArrayCount ---\n";
    // Check the size of our arrays without needing template type parameters
    std::cout << "Elements in fast_array: " << arena.getArrayCount<int>(fast_array) << "\n";
    std::cout << "Elements in zero_array: " << arena.getArrayCount<int>(zero_array) << "\n\n";

    static_assert(ArenaAllocatorConstructable<Tracker>);

    std::cout << "--- 4. Allocating object array (Custom Types) ---\n";
    // Custom objects are always default-constructed via placement new loops,
    // ignoring the zero_initialize flag.
    size_t obj_count = 3;
    Tracker* obj_array = arena.allocateArray<Tracker>(obj_count);
    std::cout << "Elements in obj_array: " << arena.getArrayCount<Tracker>(obj_array) << "\n\n";


    std::cout << "--- 5. Deallocating via standard deallocate() ---\n";
    // You do not need a separate deallocateArray function.
    // The regular deallocate automatically triggers the tracked destructor loops.
    std::cout << "Deallocating custom object array:\n";
    arena.deallocate(obj_array);

    std::cout << "\nDeallocating primitive array (destructor loop is skipped via if constexpr):\n";
    arena.deallocate(zero_array);


    std::cout << "\n--- 6. Clearing the Arena ---\n";
    // Any remaining active objects (like fast_array) will be cleaned up safely here
    std::cout << "Clearing entire arena...\n";
    arena.clear();
}

TEST(ArenaAllocator_allocate, array_copyN)
{
    ArenaAllocator<StoreTypeInfoType::yes> arena;

    // 1. Primitive array filled with a specific default value (e.g., 42)
    int* answer_array = arena.allocateArray<int>(10, 42);
    // Every single one of the 10 elements is now guaranteed to be 42.

    // 2. Custom object array pre-configured
    Tracker blueprint; // Let's say this gets ID #1
    blueprint.id = 999; // Override the ID manually for the blueprint

    Tracker* filled_trackers = arena.allocateArray<Tracker>(3, blueprint);
    // All 3 elements inside filled_trackers are now copies and have the ID 999.
}
