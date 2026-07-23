#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H
//#pragma once

#include<functional>
#include<vector>
#include<iterator>
#include<variant>
#include<stdexcept>

#include "arena_allocator_basic.h"
#include "callback.h"
#include "callback_array.h"

// Forward declaration of the allocator
template <ArenaMode M>
class ArenaAllocator;

template <ArenaMode M, typename T>
class ArenaBufferHandle
{
public:
    // STL Container Type Definitions strictly using value_type
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = value_type*;
    using const_iterator = const value_type*;
private:
    // Strongly-typed pointer to your actual allocator
    ArenaAllocator<M>* m_arena;
    std::size_t m_offset;
    std::size_t m_size; // Stores the size in raw bytes

    // Resolves the pointer freshly on-demand via the strongly-typed arena pointer
    value_type* get_ptr() const noexcept
    {
        if (!m_arena) return nullptr;
        return reinterpret_cast<value_type*>(m_arena->get_object_pointer(m_offset));
    }

public:

    // Strongly-typed constructor used by the allocator
    ArenaBufferHandle(ArenaAllocator<M>* arena, std::size_t offset, std::size_t size)
        : m_arena(arena), m_offset(offset), m_size(size) {}

    // Default constructor for empty handles
    ArenaBufferHandle() : m_arena(nullptr), m_offset(0), m_size(0) {}

    // Element Access
    reference operator[](size_type index) { return get_ptr()[index]; }
    const_reference operator[](size_type index) const { return get_ptr()[index]; }

    reference at(size_type index) {
        if (index >= m_size) throw std::out_of_range("ArenaBufferHandle::at() out of bounds");
        return get_ptr()[index];
    }
    const_reference at(size_type index) const {
        if (index >= m_size) throw std::out_of_range("ArenaBufferHandle::at() out of bounds");
        return get_ptr()[index];
    }

    reference front() { return *get_ptr(); }
    const_reference front() const { return *get_ptr(); }

    reference back() { return get_ptr()[m_size - 1]; }
    const_reference back() const { return get_ptr()[m_size - 1]; }

    pointer data() noexcept { return get_ptr(); }
    const_pointer data() const noexcept { return get_ptr(); }

    // STL Iterators (Raw pointers act as hyper-fast random-access iterators)
    iterator begin() noexcept { return get_ptr(); }
    iterator end() noexcept { return get_ptr() + m_size; }

    const_iterator begin() const noexcept { return get_ptr(); }
    const_iterator end() const noexcept { return get_ptr() + m_size; }

    const_iterator cbegin() const noexcept { return get_ptr(); }
    const_iterator cend() const noexcept { return get_ptr() + m_size; }

    // Capacity Observers
    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }
    size_type size() const noexcept { return m_size; }
    size_type max_size() const noexcept { return m_size; }

    std::size_t get_offset() const noexcept { return m_offset; }
};

/**
 * @brief A growth-based arena allocator that stores objects in contiguous memory.
 * 
 * ArenaAllocator manages objects in a std::vector<std::byte> with the following characteristics:
 * - Objects are stored contiguously with proper alignment
 * - When new allocation doesn't fit, the buffer doubles in size
 * - All alive objects are moved to the new buffer with proper construction/destruction
 * - Destroyed objects leave gaps but remain in place (marked as dead)
 * - Each allocation tracks whether it's still alive via a flag
 * - Optional runtime type information storage
 * - Forward iteration support over alive allocations
 *
 * In case an array allocation is requested and the size is zero the allocator
 * might return nullptr and act like nullptr is an array of size zero
 * 
 * @tparam StoreTypeInfo If true, stores type information for each allocation. Default: false
 * 
 * @note When StoreTypeInfo is false, no runtime overhead is incurred for type tracking.
 */
template <ArenaMode M = ArenaMode::Standard>
class ArenaAllocator
{
    static constexpr bool StoreTypeInfo = (M == ArenaMode::TypeAware);
    static constexpr bool IsLightweight = (M == ArenaMode::Lightweight);

    using CopyFunction = void (*)(void *src, void *dst, std::ptrdiff_t offset);
    using DestructorFunction = void (*)(void*);

    // Conditional types for the header function pointers
    using CopyPtr = std::conditional_t<IsLightweight, std::monostate, CopyFunction>;
    using DestructPtr = std::conditional_t<IsLightweight, std::monostate, DestructorFunction>;

    template <ArenaMode Mode, typename T>
    friend class ArenaBufferHandle;

private:
    struct AllocationHeader
    {
        using TypeInfo = std::conditional_t<StoreTypeInfo, const std::type_info*, std::monostate>;

        std::size_t size;
        bool is_alive;

        // Force zero overhead in Lightweight mode using [[no_unique_address]]
        [[no_unique_address]] CopyPtr copy;
        [[no_unique_address]] DestructPtr destructor;
        [[no_unique_address]] TypeInfo type_info;

        AllocationHeader() : size(0), is_alive(false)
        {
            if constexpr (!IsLightweight)
            {
                copy = nullptr;
                destructor = nullptr;
            }
            if constexpr (StoreTypeInfo)
            {
                type_info = nullptr;
            }
        }
    };

public:

    template<EntryConstness C>
    class Entry
    {
        static constexpr bool Const = (bool)C;
    protected:
        using Alloc = std::conditional_t<Const, const ArenaAllocator, ArenaAllocator>;

        Alloc* arena;
        std::size_t offset;
    public:
        Entry(Alloc* arena, std::size_t offset)
            : arena(arena), offset(offset)
        {

        }

        const AllocationHeader& get_header() const
        {
            return arena->get_header(offset);
        }

        template<typename T>
        using ref_type = std::conditional_t<Const, const T, T>;

        template<typename T>
        ref_type<T>& get() requires StoreTypeInfo
        {
            if(typeid(T) != *get_header().type_info)
                throw std::invalid_argument("wrong type");

            return *reinterpret_cast<ref_type<T>*>(arena->get_object_pointer(offset));
        }

        template<typename T>
        const T& get() const requires StoreTypeInfo
        {
            if(typeid(T) != *get_header().type_info)
                throw std::invalid_argument("wrong type");

            return *reinterpret_cast<const T*>(arena->get_object_pointer(offset));
        }

        /**
         * @brief Get the size of the current allocation.
         * @return Size of the allocated object in bytes
         */
        std::size_t get_size() const
        {
            return arena->get_header(offset).size;
        }
    };

    /**
     * @brief Forward iterator for traversing alive allocations in the arena.
     * 
     * The iterator automatically skips dead allocations and provides access to
     * allocation metadata. Iterators become invalid if the arena is reallocated
     * during iteration.
     */
    template<EntryConstness C>
    class IteratorImpl : private Entry<C>
    {
        static constexpr bool Const = (bool)C;
        using parent = Entry<C>;
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = parent;
        using pointer = parent*;
        using reference = parent&;
        using iterator_category = std::forward_iterator_tag;

        //using Entry<C>::get;

    private:

        friend class ArenaAllocator;

        /**
         * @brief Construct iterator at given byte offset.
         * @param arena Pointer to the arena
         * @param offset Byte offset in the buffer
         * @param find_alive If true, advance to first alive allocation
         */
        IteratorImpl(parent::Alloc* arena, std::size_t offset, bool find_alive):
            parent(arena, offset)
        {
            if (find_alive)
            {
                advance_to_next_alive();
            }
        }

        /**
         * @brief Advance offset to the next alive allocation.
         */
        void advance_to_next_alive()
        {
            while (parent::offset < parent::arena->current_offset)
            {
                const AllocationHeader& header = parent::arena->get_header(parent::offset);
                if (header.is_alive)
                    break;

                parent::offset += HEADER_SIZE + header.size;
            }
        }

    public:
        IteratorImpl(const IteratorImpl&) = default;
        IteratorImpl& operator=(const IteratorImpl&) = default;

        /**
         * @brief Check equality of two iterators.
         */
        bool operator==(const IteratorImpl& other) const
        {
            return parent::offset == other.offset;
        }

        /**
         * @brief Check inequality of two iterators.
         */
        bool operator!=(const IteratorImpl& other) const
        {
            return parent::offset != other.offset;
        }

        using Value = std::conditional_t<Const, const parent, parent>;

        operator Value&()
        {
            return *this;
        }

        operator const Value&() const
        {
            return *this;
        }

        /**
         * @brief Dereference iterator to get void pointer to current Entry.
         * @return Pointer to the Entry
         */
        const Entry<C>& operator*() const
        {
            return *this;
        }

        Entry<C>* operator->()
        {
            return this;
        }

        /**
         * @brief Pre-increment operator.
         * @return Reference to this iterator after advancing
         */
        IteratorImpl& operator++()
        {
            if (parent::offset < parent::arena->current_offset)
            {
                const AllocationHeader& header = parent::arena->get_header(parent::offset);
                parent::offset += HEADER_SIZE + header.size;
                advance_to_next_alive();
            }
            return *this;
        }

        /**
         * @brief Post-increment operator.
         * @return Copy of iterator before advancing
         */
        IteratorImpl operator++(int)
        {
            IteratorImpl temp = *this;
            ++(*this);
            return temp;
        }

        /**
         * @brief Get the allocation header for the current object.
         * @return Reference to the AllocationHeader
         */
        const AllocationHeader& get_header() const
        {
            return parent::arena->get_header(parent::offset);
        }
    };

    using Iterator = IteratorImpl<EntryConstness::no>;
    using ConstIterator = IteratorImpl<EntryConstness::yes>;

private:
    static constexpr std::size_t INITIAL_CAPACITY = 256;    ///< Initial buffer capacity
public:
    static constexpr std::size_t HEADER_SIZE = sizeof(AllocationHeader);
private:
    std::vector<std::byte> buffer;      ///< Raw byte buffer storing all allocations
    std::size_t current_offset = 0;     ///< Byte offset to end of last allocation
    std::size_t m_max_capacity = 1024 * 1024; ///< Maximum allowed memory; Default limit: 1 MB
    std::function<void(std::size_t, std::size_t)> m_reallocation_callback = nullptr;

    /**
     * @brief Align an offset to the required alignment boundary.
     * @param offset Current offset
     * @param alignment Required alignment in bytes
     * @return Aligned offset
     */
    static std::size_t align_offset(std::size_t offset, std::size_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    /**
     * @brief Get the AllocationHeader at the given byte offset.
     * @param offset Byte offset in buffer
     * @return Reference to AllocationHeader
     */
    AllocationHeader& get_header(std::size_t offset)
    {
        return *reinterpret_cast<AllocationHeader*>(buffer.data() + offset);
    }

    /**
     * @brief Get the const AllocationHeader at the given byte offset.
     * @param offset Byte offset in buffer
     * @return Const reference to AllocationHeader
     */
    const AllocationHeader& get_header(std::size_t offset) const
    {
        return *reinterpret_cast<const AllocationHeader*>(buffer.data() + offset);
    }

    /**
     * @brief Get the object pointer given the header offset.
     * @param offset Byte offset of the header
     * @return Pointer to object data (after header)
     */
    std::byte* get_object_pointer(std::size_t offset)
    {
        return buffer.data() + offset + HEADER_SIZE;
    }

    /**
     * @brief Get the const object pointer given the header offset.
     * @param offset Byte offset of the header
     * @return Const pointer to object data (after header)
     */
    const std::byte* get_object_pointer(std::size_t offset) const
    {
        return buffer.data() + offset + HEADER_SIZE;
    }

    size_t getOffset(void *ptr)
    {
        if(!contains(ptr))
            throw std::invalid_argument("pointer outside of range");

        return static_cast<std::byte*>(ptr) - buffer.data();
    }

public:
    /**
     * @brief Checks if a given pointer resides within the currently active buffer bounds of this arena.
     *
     * @param ptr The pointer to verify
     * @return true if the pointer belongs to this arena, false otherwise
     */
    bool contains(const void* ptr) const noexcept
    {
        if (!ptr || buffer.empty())
            return false;

        const std::byte* byte_ptr = reinterpret_cast<const std::byte*>(ptr);
        return byte_ptr >= buffer.data() && byte_ptr < buffer.data() + current_offset;
    }

    /**
     * @brief Calculates the total memory wasted by dead allocations.
     *
     * @return Total number of dead bytes within the buffer range
     */
    std::size_t get_dead_bytes() const noexcept
    {
        std::size_t dead_bytes = 0;
        std::size_t offset = 0;

        while (offset < current_offset)
        {
            const AllocationHeader& header = get_header(offset);

            // Fix: To find the true next header, we must simulate the exact alignment
            // logic used during allocation. However, since the type T is unknown here,
            // we can find the next header by reading the function pointers or alignment boundaries.
            // For the sake of the test, we align the next expected block start:
            std::size_t next_header_offset = offset + HEADER_SIZE + header.size;

            // Align the next offset to the header's own alignment to keep the loop synchronized
            next_header_offset = (next_header_offset + alignof(AllocationHeader) - 1) & ~(alignof(AllocationHeader) - 1);

            std::size_t total_block_size = next_header_offset - offset;

            if (!header.is_alive)
            {
#ifdef ARENA_ALLOCATOR_LOG
                std::cout << "Found block at " << (void*)&header << std::endl;
#endif
                dead_bytes += total_block_size;
            }

            offset = next_header_offset;
        }
        return dead_bytes;
    }
    /**
     * @brief Calculates the memory fragmentation ratio within the used buffer range.
     *
     * The ratio is determined by dividing the bytes wasted by dead allocations
     * by the total active buffer offset.
     *
     * @return double A value between 0.0 (no fragmentation) and 1.0 (completely fragmented).
     */
    double get_fragmentation_ratio() const noexcept
    {
        if (current_offset == 0)
            return 0.0;

        return static_cast<double>(get_dead_bytes()) / static_cast<double>(current_offset);
    }
private:
    /**
     * @brief Zerstört alle lebendigen Objekte in einem spezifizierten Speicherbereich.
     *
     * @param data_ptr Zeiger auf den Start des Byte-Buffers
     * @param end_offset Der maximale Offset, bis zu dem Objekte geprüft werden
     */
    void destroy_objects_in_range(std::byte* data_ptr, std::size_t end_offset)
    {
        std::size_t offset = 0;
        while (offset < end_offset)
        {
            auto& header = *reinterpret_cast<AllocationHeader*>(data_ptr + offset);
            std::size_t object_size = header.size;

            if constexpr(!IsLightweight)
            {
                if (header.is_alive && header.destructor)
                {
                    // Objekt-Pointer relativ zum übergebenen data_ptr berechnen
                    void* object_ptr = data_ptr + offset + HEADER_SIZE;
                    header.destructor(object_ptr);
                }
            }

            offset += HEADER_SIZE + object_size;
        }
    }

public:
    /**
     * @brief forces a reallocating which will trigger moving objects
     *
     * Mainly for testing purposes to see if moving objects into new buffer works
     */
    void forceReallocate()
    {
        reallocate(buffer.capacity()+1);
    }

    /**
     * @brief Reallocate to a larger buffer and move all alive objects.
     * 
     * Doubles capacity until the new allocation fits. All alive objects are
     * moved and destructors of old objects are called.
     * 
     * @param required_size Minimum additional bytes needed
     */
    void reallocate(std::size_t required_size)
    {
        std::size_t old_capacity = buffer.capacity();
        std::size_t new_capacity = buffer.capacity();
        if (new_capacity == 0) new_capacity = INITIAL_CAPACITY;

        while (new_capacity < current_offset + required_size)
        {
            new_capacity *= 2;
        }

        if (new_capacity > m_max_capacity)
            throw std::runtime_error("ArenaAllocator: reached maximum capacity limit");

        if (m_reallocation_callback)
        {
            m_reallocation_callback(old_capacity, new_capacity);
        }
#ifdef ARENA_ALLOCATOR_LOG
        std::cout << "reallocate: " << buffer.capacity() << " -> " << new_capacity << std::endl;
#endif

        std::vector<std::byte> new_buffer;
        new_buffer.resize(new_capacity);

        std::ptrdiff_t memoryRelocationOffset = new_buffer.data() - buffer.data();

        std::size_t new_offset = 0;
        std::size_t old_offset = 0;

        // SCHRITT 1: Objekte in den neuen Buffer verschieben/kopieren
        try
        {
            while (old_offset < current_offset)
            {
                AllocationHeader& old_header = get_header(old_offset);
                std::size_t object_size = old_header.size;

                if (old_header.is_alive)
                {
                    std::size_t aligned_new_offset = align_offset(new_offset, alignof(AllocationHeader));

                    if (aligned_new_offset + HEADER_SIZE + object_size > new_capacity)
                        throw std::invalid_argument("wrong allocation");

                    AllocationHeader& new_header = *reinterpret_cast<AllocationHeader*>(
                        new_buffer.data() + aligned_new_offset);
                    new_header = old_header;

                    void *dst = new_buffer.data() + aligned_new_offset + HEADER_SIZE;
                    void *src = get_object_pointer(old_offset);

                    if constexpr (IsLightweight)
                    {
                        // Blazing fast direct memory migration without overhead
                        std::memcpy(dst, src, old_header.size);
                    }
                    else
                    {
                        //std::ptrdiff_t exactObjectOffset = reinterpret_cast<std::byte*>(dst) - reinterpret_cast<std::byte*>(src);
                        new_header.copy(src, dst, memoryRelocationOffset);
                    }

                    new_offset = aligned_new_offset + HEADER_SIZE + object_size;
                }

                old_offset += HEADER_SIZE + object_size;
            }
        }
        catch (...)
        {
            //destroy newly constructed objects in case of an exception
            destroy_objects_in_range(new_buffer.data(), new_offset);
            throw;
        }

        //in case no errors occured destroy old objects
        destroy_objects_in_range(buffer.data(), current_offset);

        // Buffer austauschen
        buffer = std::move(new_buffer);
        current_offset = new_offset;
    }

    /**
     * @brief Gets the current maximum capacity limit configured for this arena.
     *
     * @return std::size_t The maximum allowed capacity in bytes.
     */
    std::size_t get_max_capacity() const noexcept
    {
        return m_max_capacity;
    }

    /**
     * @brief Sets a custom maximum capacity limit for the arena buffer.
     *
     * @param limit The maximum allowed capacity in bytes.
     */
    void set_max_capacity(std::size_t limit) noexcept
    {
        m_max_capacity = limit;
    }

    /**
     * @brief Registers a callback function that is triggered whenever a physical reallocation occurs.
     *
     * @param callback The function to invoke, receiving the old and new capacity in bytes.
     */
    void on_reallocation(std::function<void(std::size_t old_cap, std::size_t new_cap)> callback)
    {
        m_reallocation_callback = std::move(callback);
    }

    ///\todo make it actually work
    /**
     * @brief Compacts the arena in-place without allocating a new buffer vector.
     */
    void compact()
    {
        //check if there is even unused space
        if (get_dead_bytes() == 0)
            return;

        std::size_t read_offset = 0;
        std::size_t write_offset = 0;
        std::byte* base = buffer.data();

        while (read_offset < current_offset)
        {
            AllocationHeader& old_header = *reinterpret_cast<AllocationHeader*>(base + read_offset);
            std::size_t object_size = old_header.size;

            if (old_header.is_alive)
            {
                std::size_t aligned_write_offset = align_offset(write_offset, alignof(AllocationHeader));

                // If the object actually needs to move forward
                if (aligned_write_offset < read_offset)
                {
                    void* src = base + read_offset + HEADER_SIZE;
                    void* dst = base + aligned_write_offset + HEADER_SIZE;
                    std::ptrdiff_t offset_shift = reinterpret_cast<std::byte*>(dst) - reinterpret_cast<std::byte*>(src);

                    // Reconstruct header at the new position first
                    AllocationHeader temp_header = old_header;
                    AllocationHeader& new_header = *reinterpret_cast<AllocationHeader*>(base + aligned_write_offset);
                    new_header = temp_header;

                    // Move/Copy the object data to the new front position
                    new_header.copy(src, dst, offset_shift);

                    // Destroy the old object at its old position
                    if (temp_header.destructor)
                        temp_header.destructor(src);
                }
                else
                {
                    // Object is already as far forward as possible, just update the write boundary
                    aligned_write_offset = read_offset;
                }

                write_offset = aligned_write_offset + HEADER_SIZE + object_size;
            }
            else
            {
                // If the object is dead, we destroy it right now if not already done
                void* src = base + read_offset + HEADER_SIZE;
                if (old_header.destructor)
                    old_header.destructor(src);
            }

            read_offset += HEADER_SIZE + object_size;
        }

        current_offset = write_offset;
    }
    /**
     * @brief Reserves at least the specified minimum capacity in the arena buffer.
     *
     * If the requested capacity is greater than the current capacity, the buffer
     * is expanded and all alive objects are relocated. Otherwise, the capacity
     * remains unchanged.
     *
     * @param new_capacity The minimum total capacity in bytes to reserve
     */
    void reserve(std::size_t new_capacity)
    {
        if (new_capacity <= buffer.capacity())
            return;

        // Calculate how many additional bytes are needed starting from current_offset
        // to reach the requested absolute new_capacity boundary.
        std::size_t required_extra_size = new_capacity - current_offset;

        // Trigger the internal, exception-safe reallocation logic
        reallocate(required_extra_size);
    }
public:
    /**
     * @brief Construct an empty arena allocator.
     */
    ArenaAllocator(size_t initialCapacity = INITIAL_CAPACITY)
    {
        buffer.resize(initialCapacity);
    }

    size_t byteSize() const
    {
        return buffer.size();
    }

private:
    template <ArenaAllocatorConstructable T>
    static void Callback_Destruct(void* ptr)
    {
#ifdef ARENA_ALLOCATOR_LOG
        std::cout << "Destruct " << typeid(T).name() << " " << ptr << std::endl;
#endif
        auto str = reinterpret_cast<T*>(ptr);
        str->~T();
    };

public:

    /**
     * @brief Allocate memory for an object of type T and construct it.
     * 
     * Allocates space for an object of type T, properly aligned, with an
     * allocation header. If necessary, reallocates the buffer.
     * 
     * @tparam T Type of object to allocate
     * @tparam Args Types of constructor arguments
     * @param args Arguments to forward to T's constructor
     * @return Pointer to the newly constructed object
     * 
     * @note The returned pointer is valid until the next reallocation
     */
    template <ArenaAllocatorConstructable T, typename... Args>
    T* allocate(Args&&... args)
    {
        if constexpr (IsLightweight)
            static_assert(PlainObject<T>, "ArenaAllocator: Lightweight mode only supports PlainObjects (trivial/standard layout).");

        auto construct = [&](void *ptr){
            return new (ptr) T(std::forward<Args>(args)...);
        };

        CopyFunction copy = callback::CopyObject<T, AllocationHeader>;
        DestructorFunction destruct = Callback_Destruct<T>;

        return allocate0<T>(construct, alignof(T), sizeof(T), copy, destruct);
    }

    /**
     * @brief allocateWithNoOffset
     *
     * \warning only safe to use if the object doesn't contain any pointer to an object inside this allocator
     *
     * @param args
     * @return
     */
    template <typename T, typename... Args>
    T* allocateWithNoOffset(Args&&... args)
    {
        auto construct = [&](void *ptr){
            return new (ptr) T(std::forward<Args>(args)...);
        };

        CopyFunction copy = callback::CopyObjectWithNoObject<T>;
        DestructorFunction destruct = callback::DestructObject<T>;

        return allocate0<T>(construct, alignof(T), sizeof(T), copy, destruct);
    }

    /**
     * @brief Allocates raw, uninitialized memory of a specific size and alignment.
     *        Returns a reallocation-safe STL-like handle.
     *
     * @param size The number of bytes to allocate.
     * @param alignment The required alignment boundary.
     * @return ArenaBufferHandle<M> An STL-compatible container view that survives arena reallocation.
     */
    ArenaBufferHandle<M, std::byte> allocateRaw(std::size_t size, std::size_t alignment)
    {
        // 1. Safety check for empty allocations
        if (size == 0) return ArenaBufferHandle<M, std::byte>();

        // 2. Trivial constructor lambda with safe type conversion to prevent compiler errors
        auto construct = [](void* ptr) {
            return static_cast<std::byte*>(ptr);
        };

        // 3. Fast bitwise memory copy for reallocation
        CopyFunction copy = [](void* src, void* dst, std::ptrdiff_t offset) {
            std::byte* header_ptr = reinterpret_cast<std::byte*>(src) - sizeof(AllocationHeader);
            AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);
            std::memcpy(dst, src, header->size);
        };

        // 4. No-op destructor (raw bytes don't need destruction)
        DestructorFunction destruct = [](void* ptr) {};

        // 5. Upfront capacity projection to prevent mid-flight stack crashes
        std::size_t estimated_header_offset = current_offset;
        std::size_t estimated_object_offset = align_offset(estimated_header_offset + HEADER_SIZE, alignment);
        std::size_t required_size = estimated_object_offset - estimated_header_offset + size;

        if (current_offset + required_size > buffer.capacity())
        {
            reallocate(required_size);
            estimated_header_offset = current_offset;
        }

        // 6. Allocate using your core 5-parameter template engine
        allocate0<std::byte>(construct, alignment, size, copy, destruct);

        // 7. Return the stable handle pointing to this exact offset block
        return ArenaBufferHandle<M, std::byte>(this, estimated_header_offset, size);
    }


private:

    template<typename T>
    static void Callback_CopyArray(void* src, void* dst, std::ptrdiff_t offset)
    {
        // 1. Metadaten aus dem AllocationHeader auslesen (Abhängiger Teil)
        std::byte* header_ptr = reinterpret_cast<std::byte*>(src) - sizeof(AllocationHeader);
        AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);

        // Elementanzahl berechnen
        std::size_t element_count = header->size / sizeof(T);

        // 2. Eigentliche Logik an die unabhängige Funktion übergeben
        callback::CopyArray<T>(src, dst, element_count, offset);
    }
    template<typename T>
    static void Callback_DestructArray_Independent(void* ptr, std::size_t element_count)
    {
        // Trivial types (PlainObjects) do not need their destructors called
        if constexpr (!PlainObject<T>)
        {
            if (!ptr || element_count == 0) return;

            T* array_start = reinterpret_cast<T*>(ptr);

            // Destroy elements in reverse order of construction
            for (std::size_t i = element_count; i > 0; --i)
            {
                array_start[i - 1].~T();
            }
        }
    }

    template<typename T>
    static void Callback_DestructArray(void* ptr)
    {
#ifdef ARENA_ALLOCATOR_LOG
        std::cout << "Destruct Array of " << typeid(T).name() << " at " << ptr << std::endl;
#endif

        // Extract metadata from the AllocationHeader if the type requires destruction
        if constexpr (!PlainObject<T>)
        {
            std::byte* header_ptr = reinterpret_cast<std::byte*>(ptr) - sizeof(AllocationHeader);
            AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);
            std::size_t element_count = header->size / sizeof(T);

            // Delegate to the independent implementation
            Callback_DestructArray_Independent<T>(ptr, element_count);
        }
        else
        {
            // For trivial types, we can skip header extraction and pass 0 elements
            Callback_DestructArray_Independent<T>(ptr, 0);
        }
    }

    /**
     * @brief turns a pointer within the buffer into an offset
     * @param ptr the pointer
     * @return the offset
     */
    template<typename T>
    size_t getOffset(T* ptr)
    {
        auto bptr = reinterpret_cast<std::byte*>(ptr);

        if(bptr < &buffer[0] || bptr> &buffer[buffer.size()-1])
            throw std::invalid_argument("out of range");

        return bptr - &buffer[0];
    }

public:
    /**
     * @brief Allocates a continuous array of type T with 'count' elements.
     *
     * @tparam T Type of the array elements
     * @param count Number of elements in the array
     * @param zero_initialize If true and T is a primitive/trivial type, the memory will be filled with zeros.
     *                        For custom objects, they are always default-constructed.
     * @return Pointer to the first element of the newly created array
     */
    template <ArenaAllocatorConstructable T>
    ArenaBufferHandle<M, T> allocateArrayWithDefault(std::size_t count, bool zero_initialize = false) requires std::is_default_constructible_v<T>
    {
        if (count == 0)
            return {};

        std::size_t objectSize = count * sizeof(T);

        // Lambda for continuous construction of all array elements
        auto construct = [count, zero_initialize](void* ptr) {
            T* array_start = reinterpret_cast<T*>(ptr);

            if constexpr (PlainObject<T>)
            {
                if (zero_initialize)
                {
                    // Highly optimized zero-initialization for primitive types
                    std::memset(ptr, 0, count * sizeof(T));
                }
                return array_start;
            }
            else
            {
                // Normal loop only for custom objects with constructors
                std::size_t constructed = 0;
                try
                {
                    for (; constructed < count; ++constructed)
                    {
                        new (&array_start[constructed]) T();
                    }
                }
                catch (...)
                {
                    // Rollback in case a constructor throws
                    for (std::size_t i = constructed; i > 0; --i)
                    {
                        array_start[i - 1].~T();
                    }
                    throw;
                }
                return array_start;
            }
        };

        CopyFunction copy = Callback_CopyArray<T>;
        DestructorFunction destruct = Callback_DestructArray<T>;

        T* raw_ptr = allocate0<T>(construct, alignof(T), objectSize, copy, destruct);
        return {this, getOffset(raw_ptr), count};
    }

    /**
     * @brief Allocates a continuous array of type T where each element is a copy of the given value.
     *
     * @tparam T Type of the array elements (Must be copy-constructible)
     * @param count Number of elements in the array
     * @param value The object to copy into every element of the array
     * @return Pointer to the first element of the newly created array
     */
    template <ArenaAllocatorConstructable T>
    ArenaArrayResult<T> allocateArray(std::size_t count, const T& value) requires std::is_copy_constructible_v<T>
    {
        if (count == 0)
            return ArenaArrayResult<T>();

        std::size_t objectSize = count * sizeof(T);

        // Lambda for continuous construction by copying the provided value
        auto construct = [count, &value](void* ptr) {
            T* array_start = reinterpret_cast<T*>(ptr);
            std::size_t constructed = 0;
            try
            {
                for (; constructed < count; ++constructed)
                {
                    // Copy-construct the value into the arena memory
                    new (&array_start[constructed]) T(value);
                }
            }
            catch (...)
            {
                // Rollback: Destroy already constructed elements in reverse order if an exception occurs
                for (std::size_t i = constructed; i > 0; --i)
                {
                    array_start[i - 1].~T();
                }
                throw;
            }
            return array_start;
        };
        CopyFunction copy = Callback_CopyArray<T>;
        DestructorFunction destruct = Callback_Destruct<T>;

        T* raw_ptr = allocate0<T>(construct, alignof(T), objectSize, copy, destruct);
        return ArenaArrayResult<T>(raw_ptr, count);
    }
    /**
     * @brief Returns the number of elements in the array
     *
     * If a non-array non-nullptr is passed then the function will return 1
     * (array of size 1 and object are not distinguished)
     *
     * \warning The array must have been allocated with this allocator or else it is undefined behaviour
     * \todo add unit tests
     *
     * @tparam T element type
     * @param ptr pointer to array (return from allocateArray)
     * @return number of elements or 0 if nullptr gets passed
     */
    template <typename T>
    std::size_t getArrayCount(const T* ptr) const
    {
        if (!ptr)
            return 0;

        //access header which is before array
        const std::byte* header_ptr = reinterpret_cast<const std::byte*>(ptr) - HEADER_SIZE;
        const AllocationHeader* header = reinterpret_cast<const AllocationHeader*>(header_ptr);

        //optional runtime check to see if the function was called with the wrong type
        if constexpr (StoreTypeInfo)
        {
            if (header->type_info && *header->type_info != typeid(T))
            {
                throw std::invalid_argument("ArenaAllocator: ArrayCount was called with wrong type");
            }
        }

        return header->size / sizeof(T);
    }

private:
    /**
     * @brief actually does the allocation, potentionally resizing the buffer to make it fit
     * @param construct constructor function, gets passed a pointer to the memory location, returning constructed object pointer
     * @param align alignment to be used
     * @param objectSize the size of the object to allocate, might be an array
     * @param copy copy callback function
     * @param destruct deconstructor callback function
     * @return
     */
    template <typename T, typename C>
    T* allocate0(C construct, size_t align, size_t objectSize, CopyFunction copy, DestructorFunction destruct)
    {
        std::size_t header_offset = current_offset;
        std::size_t object_offset = align_offset(header_offset + HEADER_SIZE, align);
        std::size_t required_size = object_offset - header_offset + objectSize;

        std::size_t end_offset = object_offset + objectSize;

        // 1. If the new object does not fit into the current size, we must act
        if (end_offset > buffer.size())
        {
            // 2. If it even exceeds the capacity, we MUST relocate the whole arena
            if (end_offset > buffer.capacity())
            {
                reallocate(required_size);
                header_offset = current_offset;
                object_offset = align_offset(header_offset + HEADER_SIZE, align);
                end_offset = object_offset + objectSize;
            }

            // 3. In both cases (after reallocate OR if we just had unused capacity left),
            // we now safely grow the vector's size to match the new object's end boundary.
            buffer.resize(end_offset);
        }

        // 4. Construction safely happens within the officially resized vector bounds
        AllocationHeader& header = *reinterpret_cast<AllocationHeader*>(buffer.data() + header_offset);
        header.size = objectSize;
        header.is_alive = true;

        //lightweight mode has no copy and destructor
        if constexpr(!IsLightweight)
        {
            header.copy = copy;
            header.destructor = destruct;
        }

        if constexpr (StoreTypeInfo)
            header.type_info = &typeid(T);

        T* obj = construct(buffer.data() + object_offset);

#ifdef ARENA_ALLOCATOR_LOG
        std::cout << "Construct " << typeid(T).name() << " " << (void*)obj << std::endl;
#endif

        current_offset = end_offset;
        return obj;
    }

public:
    /**
     * @brief Deallocate an object previously allocated via allocate().
     * 
     * Calls the object's destructor and marks the allocation as dead.
     * The space remains in the buffer until the next reallocation.
     * 
     * @tparam T Type of object to deallocate
     * @param ptr Pointer to object returned by allocate()
     * 
     * @note Must be called with a pointer returned by allocate() on this allocator
     */
    template <typename T>
    void deallocate(T* ptr)
    {
        if (!ptr) return;

        std::byte* header_ptr = reinterpret_cast<std::byte*>(ptr) - HEADER_SIZE;
        AllocationHeader& header = *reinterpret_cast<AllocationHeader*>(header_ptr);

        if constexpr (!IsLightweight)
        {
            // call destructor if object is still alive
            if (header.is_alive && header.destructor)
                header.destructor(ptr);
        }

        header.is_alive = false;
    }

    /**
     * @brief Get runtime type information for an allocated object.
     * 
     * @tparam T Type of object
     * @param ptr Pointer to object returned by allocate()
     * @return Pointer to std::type_info for the allocated object, or nullptr if not found
     * 
     * @note This method is only available if StoreTypeInfo template parameter is true
     */
    template <typename T>
    const std::type_info* get_type_info(T* ptr) const 
        requires StoreTypeInfo
    {
        if (!ptr) return nullptr;

        std::size_t offset = 0;
        while (offset < current_offset)
        {
            const AllocationHeader& header = get_header(offset);
            std::size_t object_size = header.size;
            const std::byte* object_ptr = get_object_pointer(offset);

            if (object_ptr == reinterpret_cast<std::byte*>(ptr))
                return header.type_info;

            offset += HEADER_SIZE + object_size;
        }

        return nullptr;
    }

    /**
     * @brief Get an iterator to the first alive allocation.
     * @return Iterator positioned at the first alive allocation
     */
    Iterator begin()
    {
        return Iterator(this, 0, true);
    }

    /**
     * @brief Get an iterator past the last allocation.
     * @return Iterator positioned at current_offset (end sentinel)
     */
    Iterator end()
    {
        return Iterator(this, current_offset, false);
    }

    /**
     * @brief Get an iterator to the first alive allocation.
     * @return Iterator positioned at the first alive allocation
     */
    ConstIterator begin() const
    {
        return ConstIterator(this, 0, true);
    }

    /**
     * @brief Get an iterator past the last allocation.
     * @return Iterator positioned at current_offset (end sentinel)
     */
    ConstIterator end() const
    {
        return ConstIterator(this, current_offset, false);
    }

    /**
     * @brief Get the current buffer usage in bytes.
     * 
     * Includes space used by alive and dead allocations, plus headers and padding.
     * 
     * @return Number of bytes currently used in the buffer
     */
    std::size_t get_used_bytes() const
    {
        return current_offset;
    }

    /**
     * @brief Get the current buffer capacity in bytes.
     * @return Total capacity of the underlying buffer
     */
    std::size_t get_capacity() const
    {
        return buffer.capacity();
    }

    /**
     * @brief Clear the arena and destroy all alive objects.
     * 
     * Calls destructors for all alive objects and resets the allocator to empty state.
     * The buffer is cleared but not deallocated (capacity is reset).
     */
    void clear()
    {
        destroy_objects_in_range(buffer.data(), current_offset);
        clearWithoutDestruct();
    }

    /**
     * @brief Clears the arena but calls no destructors
     * @warning This function is only supposed to be called when no type with a destructor has been allocated
     */
    void clearWithoutDestruct()
    {
        buffer.clear();
        current_offset = 0;
    }

    /**
     * @brief Counts how many currently alive allocations belong to the specific type T.
     *
     * @tparam T The type to query.
     * @return std::size_t Number of alive instances of type T.
     */
    template <typename T>
    std::size_t get_allocation_count() const
    {
        // Safety check: Type tracking must be active for this operation
        static_assert(StoreTypeInfo, "ArenaAllocator: get_allocation_count requires StoreTypeInfo to be enabled.");

        std::size_t count = 0;
        std::size_t offset = 0;

        while (offset < current_offset)
        {
            const AllocationHeader& header = get_header(offset);

            // Check if the block is alive and matches the exact type information
            if (header.is_alive && header.type_info && *header.type_info == typeid(T))
            {
                // If it's an array allocation, we deduce the element count, otherwise it's 1
                std::size_t elements = header.size / sizeof(T);
                count += (elements > 0) ? elements : 1;
            }

            offset += HEADER_SIZE + header.size;
        }
        return count;
    }

    /**
     * @brief Calculates the total payload bytes consumed by alive allocations of type T.
     *
     * @tparam T The type to query.
     * @return std::size_t Total bytes utilized by alive objects of type T (excluding headers).
     */
    template <typename T>
    std::size_t get_total_bytes_for_type() const
    {
        static_assert(StoreTypeInfo, "ArenaAllocator: get_total_bytes_for_type requires StoreTypeInfo to be enabled.");

        std::size_t total_bytes = 0;
        std::size_t offset = 0;

        while (offset < current_offset)
        {
            const AllocationHeader& header = get_header(offset);

            if (header.is_alive && header.type_info && *header.type_info == typeid(T))
            {
                total_bytes += header.size;
            }

            offset += HEADER_SIZE + header.size;
        }
        return total_bytes;
    }

    /**
     * @brief Destructor that clears all allocations.
     */
    ~ArenaAllocator()
    {
        clear();
    }
};

/**
 * \brief ArenaAllocator who supports any type but doesn't keep track of type it is
 */
using PlainArenaAllocator = ArenaAllocator<ArenaMode::Standard>;

/**
 * \brief ArenaAllocator who supports any type and doesn't keep track of type it is
 */
using TypeAwareArenaAllocator = ArenaAllocator<ArenaMode::TypeAware>;

/**
 * \brief ArenaAllocator that just allocates raw bytes
 * \warning do not use with any type that can't deal with std::memcpy
 */
using LightweightArenaAllocator = ArenaAllocator<ArenaMode::Lightweight>;

// Corresponding handles matching your existing arena aliases
template<typename T>
using PlainArenaBufferHandle       = ArenaBufferHandle<ArenaMode::Standard, T>;
template<typename T>
using TypeAwareArenaBufferHandle   = ArenaBufferHandle<ArenaMode::TypeAware, T>;
template<typename T>
using LightweightArenaBufferHandle = ArenaBufferHandle<ArenaMode::Lightweight, T>;

#endif
