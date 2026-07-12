#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H
//#pragma once

#include<vector>
#include<cstring>
#include<cassert>
#include<typeinfo>
#include<iterator>
#include<variant>
#include<iostream>

#ifdef ARENA_ALLOCATOR_LOG
#include<iostream>
#endif

std::string getTypeName(const std::type_info &type);

enum class StoreTypeInfoType : bool
{
    no, yes
};

enum class EntryConstness : bool
{
    no, yes
};

/**
 * \internal
 */
class CopyWithOffsetConstruct
{
};

inline constexpr CopyWithOffsetConstruct copyWithOffsetConstruct;

template<typename T>
T* getNewMemoryLocation(T* org, std::ptrdiff_t offset)
{
    return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(org)+offset);
}

///\todo better name and actually check for members of pointer type which may point to a memory inside the arena allocator
template<typename T>
concept PlainObject = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

template<typename T>
concept CopyWithOffsetConstructable =
    std::is_constructible_v<T, CopyWithOffsetConstruct, const T&, std::ptrdiff_t>;

template<typename T>
concept ArenaAllocatorConstructable = PlainObject<T> || CopyWithOffsetConstructable<T>;

template <typename T>
class ArenaArrayView
{
private:
    T* m_data;
    std::size_t m_size;

public:
    // Container type definitions for compatibility with STL algorithms
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;

    ArenaArrayView(T* data, std::size_t size) : m_data(data), m_size(size) {}

    // Observers and data access
    T* data() noexcept { return m_data; }
    const T* data() const noexcept { return m_data; }
    std::size_t size() const noexcept { return m_size; }
    bool empty() const noexcept { return m_size == 0; }

    // Element access with bracket operator
    T& operator[](std::size_t index) { return m_data[index]; }
    const T& operator[](std::size_t index) const { return m_data[index]; }

    // Iterator support
    iterator begin() noexcept { return m_data; }
    iterator end() noexcept { return m_data + m_size; }
    const_iterator begin() const noexcept { return m_data; }
    const_iterator end() const noexcept { return m_data + m_size; }
    const_iterator cbegin() const noexcept { return m_data; }
    const_iterator cend() const noexcept { return m_data + m_size; }
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
template <StoreTypeInfoType S = StoreTypeInfoType::no>
class ArenaAllocator
{
    static constexpr bool StoreTypeInfo = (bool)S;

    using CopyFunction = void (*)(void *src, void *dst, std::ptrdiff_t offset);
    using DestructorFunction = void (*)(void*);

private:
    /**
     * @brief Metadata stored before each allocation.
     */
    struct AllocationHeader
    {
        using TypeInfo = std::conditional_t<StoreTypeInfo, const std::type_info*, std::monostate>;

        std::size_t size;                           ///< Size of the actual object (excluding header)
        bool is_alive;                              ///< Flag indicating if object is still alive
        CopyFunction copy;                          ///< Function pointer to object's copy constructor
        DestructorFunction destructor;              ///< Function pointer to object's destructor
        /**
         * @brief Optional: RTTI information
         *
         * Only available if type info is enabled
         */
        [[no_unique_address]]
        TypeInfo type_info;

        AllocationHeader() : size(0), is_alive(false), copy(nullptr), destructor(nullptr)
        {
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
    };

    /**
     * @brief Forward iterator for traversing alive allocations in the arena.
     * 
     * The iterator automatically skips dead allocations and provides access to
     * allocation metadata. Iterators become invalid if the arena is reallocated
     * during iteration.
     */
    template<EntryConstness C>
    //TODO: make inheritance private
    class IteratorImpl : public Entry<C>
    {
        static constexpr bool Const = (bool)C;
        using parent = Entry<C>;
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = parent;
        using pointer = parent*;
        using reference = parent&;
        using iterator_category = std::forward_iterator_tag;

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
         * @brief Dereference iterator to get void pointer to current allocation.
         * @return Pointer to the allocated object, or nullptr if at end
         */
        std::conditional_t<Const, const value_type&, value_type&> operator*() const
        {
            return *this;
            /*
            if (parent::offset >= parent::arena->current_offset) return nullptr;
            return reinterpret_cast<void*>(parent::arena->get_object_pointer(parent::offset));
            */
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

        /**
         * @brief Get the size of the current allocation.
         * @return Size of the allocated object in bytes
         */
        std::size_t get_size() const
        {
            return parent::arena->get_header(parent::offset).size;
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
            std::size_t total_block_size = HEADER_SIZE + header.size;

            if (!header.is_alive)
                dead_bytes += total_block_size;

            offset += total_block_size;
        }
        return dead_bytes;
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

            if (header.is_alive && header.destructor)
            {
                // Objekt-Pointer relativ zum übergebenen data_ptr berechnen
                void* object_ptr = data_ptr + offset + HEADER_SIZE;
                header.destructor(object_ptr);
            }

            offset += HEADER_SIZE + object_size;
        }
    }

public:
    ///forces a reallocating which will trigger moving objects
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
        std::size_t new_capacity = buffer.capacity();
        if (new_capacity == 0) new_capacity = INITIAL_CAPACITY;

        while (new_capacity < current_offset + required_size)
        {
            new_capacity *= 2;
        }

        if (new_capacity > 1024 * 1024)
            throw std::runtime_error("reached 1 MB");

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

                    new_header.copy(src, dst, memoryRelocationOffset);

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
        {
            return;
        }

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
    ArenaAllocator()
    {
        buffer.resize(INITIAL_CAPACITY);
    }

    size_t byteSize() const
    {
        return buffer.size();
    }

private:
    template <ArenaAllocatorConstructable T>
    static void Callback_CopyObject(void* src, void* dst, std::ptrdiff_t offset)
    {
        T* src_array = reinterpret_cast<T*>(src);
        T* dst_array = reinterpret_cast<T*>(dst);

        // Wichtig: Ohne exakten Alignment-Fix ist diese Zeile gefährlich,
        // aber wir korrigieren zumindest die Kopierlogik:
        std::byte* header_ptr = reinterpret_cast<std::byte*>(src) - sizeof(AllocationHeader);
        AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);
        std::size_t element_count = header->size / sizeof(T);

        if constexpr (PlainObject<T>)
        {
            std::memcpy(dst, src, header->size);
        }
        else
        {
            std::size_t copied = 0;
            try
            {
                for (; copied < element_count; ++copied)
                {
                    if constexpr (CopyWithOffsetConstructable<T>)
                    {
                        new (&dst_array[copied]) T(copyWithOffsetConstruct, std::move(src_array[copied]), offset);
                    }
                    else
                    {
                        // FIX: Falls der Typ kein Custom-Offset unterstützt,
                        // nutzen wir ein normales Placement-New mit std::move
                        new (&dst_array[copied]) T(std::move(src_array[copied]));
                    }
                }
            }
            catch (...)
            {
                // Rollback bei Konstruktor-Ausnahme
                for (std::size_t i = copied; i > 0; --i)
                {
                    dst_array[i - 1].~T();
                }
                throw;
            }
        }
    }

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
        auto construct = [&](void *ptr){
            return new (ptr) T(std::forward<Args>(args)...);
        };

        CopyFunction copy = Callback_CopyObject<T>;
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

        CopyFunction copy = [](void* src, void *dst, std::ptrdiff_t offset)
        {
#ifdef ARENA_ALLOCATOR_LOG
            std::cout << "Copy " << typeid(T).name() << " " << src << " to " << dst << std::endl;
#endif
            new (dst)T(std::move(*reinterpret_cast<T*>(src)));
        };
        DestructorFunction destruct = [](void* ptr)
        {
#ifdef ARENA_ALLOCATOR_LOG
            std::cout << "Destruct " << typeid(T).name() << " " << ptr << std::endl;
#endif
            ///\todo check with std::is_constructible if there is a constructor with an offset
            auto str = reinterpret_cast<T*>(ptr);
            str->~T();
        };

        return allocate0<T>(construct, alignof(T), sizeof(T), copy, destruct);
    }

    /**
     * @brief Allocates raw, uninitialized memory of a specific size and alignment.
     *
     * This function bypasses construction and destruction tracking. It is ideal
     * for custom low-level storage or buffer allocations within the arena.
     *
     * @param size The number of bytes to allocate.
     * @param alignment The required alignment for the allocated memory boundary.
     * @return void* Pointer to the newly allocated uninitialized memory block.
     */
    void* allocateRaw(std::size_t size, std::size_t alignment)
    {
        if (size == 0) return nullptr;

        // 1. A trivial inline constructor lambda that does absolutely nothing
        auto construct = [](void* ptr) {
            return ptr;
        };

        // 2. A fast bitwise memory copy for reallocation
        CopyFunction copy = [](void* src, void* dst, std::ptrdiff_t offset)
        {
            // To safely copy during reallocate, we extract the size from the header
            std::byte* header_ptr = reinterpret_cast<std::byte*>(src) - sizeof(AllocationHeader);
            AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);
            std::memcpy(dst, src, header->size);
        };

        // 3. A no-op destructor (raw bytes don't need destruction)
        DestructorFunction destruct = [](void* ptr)
        {
            // No operation needed for raw uninitialized bytes
        };

        // 4. Fall back to your optimized allocate0 method
        return allocate0<std::byte>(construct, alignment, size, copy, destruct);
    }

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
    ArenaArrayView<T> allocateArray(std::size_t count, bool zero_initialize = false) requires std::is_default_constructible_v<T>
    {
        if (count == 0) return ArenaArrayView<T>(nullptr, 0);

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

        // Copy function for the entire array during reallocation
        CopyFunction copy = [](void* src, void* dst, std::ptrdiff_t offset)
        {
            T* src_array = reinterpret_cast<T*>(src);
            T* dst_array = reinterpret_cast<T*>(dst);

            // Wichtig: Ohne exakten Alignment-Fix ist diese Zeile gefährlich,
            // aber wir korrigieren zumindest die Kopierlogik:
            std::byte* header_ptr = reinterpret_cast<std::byte*>(src) - sizeof(AllocationHeader);
            AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);
            std::size_t element_count = header->size / sizeof(T);

            if constexpr (PlainObject<T>)
            {
                std::memcpy(dst, src, header->size);
            }
            else
            {
                std::size_t copied = 0;
                try
                {
                    for (; copied < element_count; ++copied)
                    {
                        if constexpr (CopyWithOffsetConstructable<T>)
                        {
                            new (&dst_array[copied]) T(copyWithOffsetConstruct, std::move(src_array[copied]), offset);
                        }
                        else
                        {
                            // FIX: Falls der Typ kein Custom-Offset unterstützt,
                            // nutzen wir ein normales Placement-New mit std::move
                            new (&dst_array[copied]) T(std::move(src_array[copied]));
                        }
                    }
                }
                catch (...)
                {
                    // Rollback bei Konstruktor-Ausnahme
                    for (std::size_t i = copied; i > 0; --i)
                    {
                        dst_array[i - 1].~T();
                    }
                    throw;
                }
            }
        };

        // Destructor function for the entire array
        DestructorFunction destruct = [](void* ptr)
        {
#ifdef ARENA_ALLOCATOR_LOG
            std::cout << "Destruct Array of " << typeid(T).name() << " at " << ptr << std::endl;
#endif
            if constexpr (!PlainObject<T>)
            {
                T* array_start = reinterpret_cast<T*>(ptr);
                std::byte* header_ptr = reinterpret_cast<std::byte*>(ptr) - sizeof(AllocationHeader);
                AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);
                std::size_t element_count = header->size / sizeof(T);

                // Destroy custom elements in reverse order
                for (std::size_t i = element_count; i > 0; --i)
                {
                    array_start[i - 1].~T();
                }
            }
            // Trivial types don't need their destructors called
        };

        T* raw_ptr = allocate0<T>(construct, alignof(T), objectSize, copy, destruct);
        return ArenaArrayView<T>(raw_ptr, count);
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
    ArenaArrayView<T> allocateArray(std::size_t count, const T& value) requires std::is_copy_constructible_v<T>
    {
        if (count == 0) return ArenaArrayView<T>(nullptr, 0);

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

        // Copy function for the entire array during reallocation (reused from default version)
        CopyFunction copy = [](void* src, void* dst, std::ptrdiff_t offset)
        {
#ifdef ARENA_ALLOCATOR_LOG
            std::cout << "Copy Array of " << typeid(T).name() << " from " << src << " to " << dst << std::endl;
#endif
            T* src_array = reinterpret_cast<T*>(src);
            T* dst_array = reinterpret_cast<T*>(dst);

            std::byte* header_ptr = reinterpret_cast<std::byte*>(src) - sizeof(AllocationHeader);
            AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);
            std::size_t element_count = header->size / sizeof(T);

            if constexpr (PlainObject<T>)
            {
                std::memcpy(dst, src, header->size);
            }
            else
            {
                std::size_t copied = 0;
                try
                {
                    for (; copied < element_count; ++copied)
                    {
                        if constexpr (CopyWithOffsetConstructable<T>)
                        {
                            new (&dst_array[copied]) T(copyWithOffsetConstruct, std::move(src_array[copied]), offset);
                        }
                        else
                            new (&dst_array[copied]) T(std::move(src_array[copied]), offset);
                    }
                }
                catch (...)
                {
                    for (std::size_t i = copied; i > 0; --i)
                    {
                        dst_array[i - 1].~T();
                    }
                    throw;
                }
            }
        };

        // Destructor function for the entire array (reused from default version)
        DestructorFunction destruct = [](void* ptr)
        {
#ifdef ARENA_ALLOCATOR_LOG
            std::cout << "Destruct Array of " << typeid(T).name() << " at " << ptr << std::endl;
#endif
            if constexpr (!PlainObject<T>)
            {
                T* array_start = reinterpret_cast<T*>(ptr);
                std::byte* header_ptr = reinterpret_cast<std::byte*>(ptr) - sizeof(AllocationHeader);
                AllocationHeader* header = reinterpret_cast<AllocationHeader*>(header_ptr);
                std::size_t element_count = header->size / sizeof(T);

                for (std::size_t i = element_count; i > 0; --i)
                {
                    array_start[i - 1].~T();
                }
            }
        };

        T* raw_ptr = allocate0<T>(construct, alignof(T), objectSize, copy, destruct);
        return ArenaArrayView<T>(raw_ptr, count);
    }
    /**
     * @brief Returns the number of elements in the array
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
        header.copy = copy;
        header.destructor = destruct;

        if constexpr (StoreTypeInfo)
        {
            header.type_info = &typeid(T);
        }

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

        // Direktzugriff auf den Header in O(1) statt linearer Suchschleife
        std::byte* header_ptr = reinterpret_cast<std::byte*>(ptr) - HEADER_SIZE;
        AllocationHeader& header = *reinterpret_cast<AllocationHeader*>(header_ptr);

        // Aufruf des Destruktors, falls das Objekt noch lebt
        if (header.is_alive && header.destructor)
        {
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
        buffer.clear();
        current_offset = 0;
    }

    /**
     * @brief Destructor that clears all allocations.
     */
    ~ArenaAllocator()
    {
        clear();
    }
};

using PlainArenaAllocator = ArenaAllocator<StoreTypeInfoType::no>;
using TypeAwareArenaAllocator = ArenaAllocator<StoreTypeInfoType::yes>;

#endif
