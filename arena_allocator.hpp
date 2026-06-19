//#pragma once

#include<vector>
#include<memory>
#include<cstring>
#include<cassert>
#include<new>
#include<typeinfo>
#include<iterator>
#include<variant>

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
template <bool StoreTypeInfo = false>
class ArenaAllocator
{
private:
    /**
     * @brief Metadata stored before each allocation.
     */
    struct AllocationHeader
    {
        std::size_t size;                           ///< Size of the actual object (excluding header)
        bool is_alive;                              ///< Flag indicating if object is still alive
        void (*destructor)(void*);                  ///< Function pointer to object's destructor
        [[no_unique_address]]
        std::conditional_t<StoreTypeInfo, const std::type_info*, std::monostate > type_info;  ///< Optional: RTTI information

        AllocationHeader() : size(0), is_alive(false), destructor(nullptr)
        {
            if constexpr (StoreTypeInfo)
            {
                type_info = nullptr;
            }
        }
    };
public:
    /**
     * @brief Forward iterator for traversing alive allocations in the arena.
     * 
     * The iterator automatically skips dead allocations and provides access to
     * allocation metadata. Iterators become invalid if the arena is reallocated
     * during iteration.
     */
    class Iterator
    {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = void*;
        using pointer = void**;
        using reference = void*&;
        using iterator_category = std::forward_iterator_tag;

    private:

        friend class ArenaAllocator;
        
        ArenaAllocator* arena;
        std::size_t offset;

        /**
         * @brief Construct iterator at given byte offset.
         * @param arena Pointer to the arena
         * @param offset Byte offset in the buffer
         * @param find_alive If true, advance to first alive allocation
         */
        Iterator(ArenaAllocator* arena, std::size_t offset, bool find_alive)
            : arena(arena), offset(offset)
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
            while (offset < arena->current_offset)
            {
                const AllocationHeader& header = arena->get_header(offset);
                if (header.is_alive)
                    break;

                offset += HEADER_SIZE + header.size;
            }
        }

    public:
        Iterator(const Iterator&) = default;
        Iterator& operator=(const Iterator&) = default;

        /**
         * @brief Check equality of two iterators.
         */
        bool operator==(const Iterator& other) const
        {
            return offset == other.offset;
        }

        /**
         * @brief Check inequality of two iterators.
         */
        bool operator!=(const Iterator& other) const
        {
            return offset != other.offset;
        }

        /**
         * @brief Dereference iterator to get void pointer to current allocation.
         * @return Pointer to the allocated object, or nullptr if at end
         */
        void* operator*() const
        {
            if (offset >= arena->current_offset) return nullptr;
            return reinterpret_cast<void*>(arena->get_object_pointer(offset));
        }

        /**
         * @brief Pre-increment operator.
         * @return Reference to this iterator after advancing
         */
        Iterator& operator++()
        {
            if (offset < arena->current_offset)
            {
                const AllocationHeader& header = arena->get_header(offset);
                offset += HEADER_SIZE + header.size;
                advance_to_next_alive();
            }
            return *this;
        }

        /**
         * @brief Post-increment operator.
         * @return Copy of iterator before advancing
         */
        Iterator operator++(int)
        {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        /**
         * @brief Get the allocation header for the current object.
         * @return Reference to the AllocationHeader
         */
        const AllocationHeader& get_header() const
        {
            return arena->get_header(offset);
        }

        template<typename T>
        T& get() requires StoreTypeInfo
        {
            if(typeid(T) != *get_header().type_info)
                throw std::invalid_argument("wrong type");

            return *reinterpret_cast<T*>(arena->get_object_pointer(offset));
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
        
        // Double capacity until it fits
        while (new_capacity < current_offset + required_size)
        {
            new_capacity *= 2;
        }

        // Create new buffer
        std::vector<std::byte> new_buffer;
        //new_buffer.reserve(new_capacity);
        //new_buffer.resize(0);
        new_buffer.resize(new_capacity);

        std::size_t new_offset = 0;

        // Move all alive objects to the new buffer
        std::size_t offset = 0;
        while (offset < current_offset)
        {
            AllocationHeader& old_header = get_header(offset);
            std::size_t object_size = old_header.size;

            if (old_header.is_alive)
            {
                // Align the new offset
                std::size_t aligned_new_offset = align_offset(new_offset, alignof(AllocationHeader));
                
                // Ensure new_buffer has enough space
                if (new_buffer.capacity() < aligned_new_offset + HEADER_SIZE + object_size)
                    throw std::invalid_argument("wrong allocation");
                //{
                //    new_buffer.reserve(std::max(new_buffer.capacity() * 2,
                //                              aligned_new_offset + HEADER_SIZE + object_size + 256));
                //}
                //new_buffer.resize(aligned_new_offset + HEADER_SIZE + object_size);

                // Copy header
                AllocationHeader& new_header = *reinterpret_cast<AllocationHeader*>(
                    new_buffer.data() + aligned_new_offset);
                new_header = old_header;

                // Copy object data
                std::memcpy(new_buffer.data() + aligned_new_offset + HEADER_SIZE,
                           get_object_pointer(offset),
                           object_size);

                new_offset = aligned_new_offset + HEADER_SIZE + object_size;
            }

            // Move to next allocation
            offset += HEADER_SIZE + object_size;
        }

        // Destroy all objects in the old buffer
        offset = 0;
        while (offset < current_offset)
        {
            AllocationHeader& header = get_header(offset);
            std::size_t object_size = header.size;

            if (header.is_alive && header.destructor)
            {
                header.destructor(get_object_pointer(offset));
            }

            offset += HEADER_SIZE + object_size;
        }

        // Replace old buffer with new one
        buffer = std::move(new_buffer);
        current_offset = new_offset;
    }

public:
    /**
     * @brief Construct an empty arena allocator.
     */
    ArenaAllocator()
    {
        buffer.reserve(INITIAL_CAPACITY);
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
    template <typename T, typename... Args>
    T* allocate(Args&&... args)
    {
        // Calculate required space: header + alignment padding + object
        std::size_t header_offset = current_offset;
        std::size_t object_offset = align_offset(header_offset + HEADER_SIZE, alignof(T));
        std::size_t required_size = object_offset - header_offset + sizeof(T);

        // Check if reallocation is needed
        if (current_offset + required_size > buffer.capacity())
        {
            reallocate(required_size);
            // Recalculate offsets after reallocation
            header_offset = current_offset;
            object_offset = align_offset(header_offset + HEADER_SIZE, alignof(T));
        }

        // Ensure buffer is large enough
        if (buffer.size() < object_offset + sizeof(T))
        {
            buffer.resize(object_offset + sizeof(T));
        }

        // Place header
        AllocationHeader& header = *reinterpret_cast<AllocationHeader*>(
            buffer.data() + header_offset);
        header.size = sizeof(T);
        header.is_alive = true;
        header.destructor = [](void* ptr)
        {
            reinterpret_cast<T*>(ptr)->~T();
        };
        
        // Store type information if enabled
        if constexpr (StoreTypeInfo)
        {
            header.type_info = &typeid(T);
        }

        // Construct object in place with forwarded arguments
        T* obj = new (buffer.data() + object_offset) T(std::forward<Args>(args)...);

        // Update offset
        current_offset = object_offset + sizeof(T);

        return obj;
    }

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
        std::size_t offset = 0;
        while (offset < current_offset)
        {
            AllocationHeader& header = get_header(offset);
            std::size_t object_size = header.size;

            if (header.is_alive && header.destructor)
            {
                header.destructor(get_object_pointer(offset));
            }

            offset += HEADER_SIZE + object_size;
        }
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
