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
    static constexpr std::size_t HEADER_SIZE = sizeof(AllocationHeader);

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

        CopyFunction copy = [](void* src, void *dst, std::ptrdiff_t offset)
        {
#ifdef ARENA_ALLOCATOR_LOG
            std::cout << "Copy " << typeid(T).name() << " " << src << " to " << dst << std::endl;
#endif
            if constexpr(PlainObject<T>)
            {
                std::cout << "no-offset copy" << std::endl;
                new (dst)T(std::move(*reinterpret_cast<T*>(src)));
            }
            else if constexpr(CopyWithOffsetConstructable<T>)
            {
                std::cout << "offset copy" << std::endl;
                new (dst)T(copyWithOffsetConstruct, std::move(*reinterpret_cast<T*>(src)), offset);
            }
            else
                static_assert("Type is neither trivial with standard layout nor supports copy construct with offset");
        };
        DestructorFunction destruct = [](void* ptr)
        {
#ifdef ARENA_ALLOCATOR_LOG
            std::cout << "Destruct " << typeid(T).name() << " " << ptr << std::endl;
#endif
            auto str = reinterpret_cast<T*>(ptr);
            str->~T();
        };

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

    void forceReallocate()
    {
        reallocate(buffer.capacity()+1);
    }

private:
    template <typename T, typename C>
    T* allocate0(C construct, size_t align, size_t objectSize, CopyFunction copy, DestructorFunction destruct)
    {
        // Calculate required space: header + alignment padding + object
        std::size_t header_offset = current_offset;
        std::size_t object_offset = align_offset(header_offset + HEADER_SIZE, align);
        std::size_t required_size = object_offset - header_offset + objectSize;

        // Check if reallocation is needed
        if (current_offset + required_size > buffer.capacity())
        {
            reallocate(required_size);
            // Recalculate offsets after reallocation
            header_offset = current_offset;
            object_offset = align_offset(header_offset + HEADER_SIZE, align);
        }

        // Ensure buffer is large enough
        if (buffer.size() < object_offset + objectSize)
        {
            //throw std::runtime_error("buffer too small");
            //buffer.resize(object_offset + objectSize);
            ///\todo don't call reallocate twice
            reallocate(object_offset + objectSize);
        }

        // Place header
        AllocationHeader& header = *reinterpret_cast<AllocationHeader*>(
            buffer.data() + header_offset);
        header.size = objectSize;
        header.is_alive = true;
        header.copy = copy;
        header.destructor = destruct;
        
        // Store type information if enabled
        if constexpr (StoreTypeInfo)
        {
            header.type_info = &typeid(T);
        }

        // Construct object in place with forwarded arguments
        T* obj = construct(buffer.data() + object_offset);

#ifdef ARENA_ALLOCATOR_LOG
        std::cout << "Construct " << typeid(T).name() << (void*)obj << std::endl;
#endif

        // Update offset
        current_offset = object_offset + objectSize;

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
        if (!ptr)
            throw std::runtime_error("nullptr");

        // Find the object and mark it as dead, but don't move anything
        std::size_t offset = 0;
        while (offset < current_offset)
        {
            AllocationHeader& header = get_header(offset);
            std::size_t object_size = header.size;
            std::byte* object_ptr = get_object_pointer(offset);

            if (object_ptr == reinterpret_cast<std::byte*>(ptr))
            {
                // Call destructor and mark as dead
                if (header.is_alive && header.destructor)
                {
                    header.destructor(object_ptr);
                }
                header.is_alive = false;
                return;
            }

            offset += HEADER_SIZE + object_size;
        }

        assert(false && "Object not found in arena");
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
