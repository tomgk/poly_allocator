#pragma once

#include <vector>
#include <memory>
#include <cstring>
#include <cassert>
#include <new>
#include <typeinfo>
#include <iterator>

/**
 * ArenaAllocator: A growth-based arena allocator that stores objects in a std::vector<std::byte>.
 * 
 * Template parameter:
 * - StoreTypeInfo: If true, stores type information for each allocation (default: false)
 * 
 * Properties:
 * - Objects are stored contiguously with proper alignment
 * - When new allocation doesn't fit, vector doubles in size
 * - All objects are moved to the new vector with proper construction/destruction
 * - Destroyed objects leave gaps but remain in place
 * - Each allocation tracks whether the object is still alive via a flag
 * - Optional type information storage for runtime type checking
 * - Iterable over alive allocations
 */
template <bool StoreTypeInfo = false>
class ArenaAllocator {
public:
    /**
     * Iterator for traversing alive allocations
     */
    class Iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = void*;
        using pointer = void**;
        using reference = void*&;
        using iterator_category = std::forward_iterator_tag;

    private:
        friend class ArenaAllocator;
        
        const ArenaAllocator* arena;
        std::size_t offset;

        Iterator(const ArenaAllocator* arena, std::size_t offset)
            : arena(arena), offset(offset) {
            // Skip to the first alive allocation
            advance_to_next_alive();
        }

        void advance_to_next_alive() {
            while (offset < arena->current_offset) {
                const AllocationHeader& header = arena->get_header(offset);
                if (header.is_alive) {
                    break;
                }
                offset += HEADER_SIZE + header.size;
            }
        }

    public:
        Iterator(const Iterator&) = default;
        Iterator& operator=(const Iterator&) = default;

        bool operator==(const Iterator& other) const {
            return offset == other.offset;
        }

        bool operator!=(const Iterator& other) const {
            return offset != other.offset;
        }

        void* operator*() const {
            if (offset >= arena->current_offset) return nullptr;
            return reinterpret_cast<void*>(arena->get_object_pointer(offset));
        }

        Iterator& operator++() {
            if (offset < arena->current_offset) {
                const AllocationHeader& header = arena->get_header(offset);
                offset += HEADER_SIZE + header.size;
                advance_to_next_alive();
            }
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        /**
         * Get the header for the current allocation
         */
        const AllocationHeader& get_header() const {
            return arena->get_header(offset);
        }

        /**
         * Get the size of the current allocation
         */
        std::size_t get_size() const {
            return arena->get_header(offset).size;
        }
    };

private:
    struct AllocationHeader {
        std::size_t size;      // Size of the actual object (excluding header)
        bool is_alive;         // Flag indicating if object is still alive
        void (*destructor)(void*); // Destructor function pointer
        
        // Conditionally include type info pointer
        std::conditional_t<StoreTypeInfo, const std::type_info*, struct {} > type_info;
        
        AllocationHeader() : size(0), is_alive(false), destructor(nullptr) {
            if constexpr (StoreTypeInfo) {
                type_info = nullptr;
            }
        }
    };

    static constexpr std::size_t INITIAL_CAPACITY = 256;
    static constexpr std::size_t HEADER_SIZE = sizeof(AllocationHeader);

    std::vector<std::byte> buffer;
    std::size_t current_offset = 0;

    /**
     * Align a pointer/offset to the required alignment boundary
     */
    static std::size_t align_offset(std::size_t offset, std::size_t alignment) {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    /**
     * Get the AllocationHeader for an object at the given offset
     */
    AllocationHeader& get_header(std::size_t offset) {
        return *reinterpret_cast<AllocationHeader*>(buffer.data() + offset);
    }

    const AllocationHeader& get_header(std::size_t offset) const {
        return *reinterpret_cast<const AllocationHeader*>(buffer.data() + offset);
    }

    /**
     * Get the object pointer given the offset (points to data after header)
     */
    std::byte* get_object_pointer(std::size_t offset) {
        return buffer.data() + offset + HEADER_SIZE;
    }

    const std::byte* get_object_pointer(std::size_t offset) const {
        return buffer.data() + offset + HEADER_SIZE;
    }

    /**
     * Reallocate to a larger buffer and move all objects
     */
    void reallocate(std::size_t required_size) {
        std::size_t new_capacity = buffer.capacity();
        
        // Double capacity until it fits
        while (new_capacity < current_offset + required_size) {
            new_capacity *= 2;
        }

        // Create new buffer
        std::vector<std::byte> new_buffer;
        new_buffer.reserve(new_capacity);
        new_buffer.resize(0);

        std::size_t new_offset = 0;

        // Move all alive objects to the new buffer
        std::size_t offset = 0;
        while (offset < current_offset) {
            AllocationHeader& old_header = get_header(offset);
            std::size_t object_size = old_header.size;

            if (old_header.is_alive) {
                // Align the new offset
                std::size_t aligned_new_offset = align_offset(new_offset, alignof(AllocationHeader));
                
                // Ensure new_buffer has enough space
                if (new_buffer.capacity() < aligned_new_offset + HEADER_SIZE + object_size) {
                    new_buffer.reserve(std::max(new_buffer.capacity() * 2, 
                                              aligned_new_offset + HEADER_SIZE + object_size + 256));
                }
                new_buffer.resize(aligned_new_offset + HEADER_SIZE + object_size);

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
        while (offset < current_offset) {
            AllocationHeader& header = get_header(offset);
            std::size_t object_size = header.size;

            if (header.is_alive && header.destructor) {
                header.destructor(get_object_pointer(offset));
            }

            offset += HEADER_SIZE + object_size;
        }

        // Replace old buffer with new one
        buffer = std::move(new_buffer);
        current_offset = new_offset;
    }

public:
    ArenaAllocator() {
        buffer.reserve(INITIAL_CAPACITY);
    }

    /**
     * Allocate memory for an object of type T and construct it with the given arguments
     * Returns a pointer to the allocated object
     */
    template <typename T, typename... Args>
    T* allocate(Args&&... args) {
        // Calculate required space: header + alignment padding + object
        std::size_t header_offset = current_offset;
        std::size_t object_offset = align_offset(header_offset + HEADER_SIZE, alignof(T));
        std::size_t required_size = object_offset - header_offset + sizeof(T);

        // Check if reallocation is needed
        if (current_offset + required_size > buffer.capacity()) {
            reallocate(required_size);
            // Recalculate offsets after reallocation
            header_offset = current_offset;
            object_offset = align_offset(header_offset + HEADER_SIZE, alignof(T));
        }

        // Ensure buffer is large enough
        if (buffer.size() < object_offset + sizeof(T)) {
            buffer.resize(object_offset + sizeof(T));
        }

        // Place header
        AllocationHeader& header = *reinterpret_cast<AllocationHeader*>(
            buffer.data() + header_offset);
        header.size = sizeof(T);
        header.is_alive = true;
        header.destructor = [](void* ptr) {
            reinterpret_cast<T*>(ptr)->~T();
        };
        
        // Store type information if enabled
        if constexpr (StoreTypeInfo) {
            header.type_info = &typeid(T);
        }

        // Construct object in place with forwarded arguments
        T* obj = new (buffer.data() + object_offset) T(std::forward<Args>(args)...);

        // Update offset
        current_offset = object_offset + sizeof(T);

        return obj;
    }

    /**
     * Deallocate an object previously allocated via allocate()
     */
    template <typename T>
    void deallocate(T* ptr) {
        if (!ptr) return;

        // Find the object and mark it as dead, but don't move anything
        std::size_t offset = 0;
        while (offset < current_offset) {
            AllocationHeader& header = get_header(offset);
            std::size_t object_size = header.size;
            std::byte* object_ptr = get_object_pointer(offset);

            if (object_ptr == reinterpret_cast<std::byte*>(ptr)) {
                // Call destructor and mark as dead
                if (header.is_alive && header.destructor) {
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
     * Get type information for an allocated object (only available if StoreTypeInfo is true)
     */
    template <typename T>
    const std::type_info* get_type_info(T* ptr) const 
        requires StoreTypeInfo
    {
        if (!ptr) return nullptr;

        std::size_t offset = 0;
        while (offset < current_offset) {
            AllocationHeader& header = get_header(offset);
            std::size_t object_size = header.size;
            std::byte* object_ptr = get_object_pointer(offset);

            if (object_ptr == reinterpret_cast<std::byte*>(ptr)) {
                return header.type_info;
            }

            offset += HEADER_SIZE + object_size;
        }

        return nullptr;
    }

    /**
     * Get iterator to the first alive allocation
     */
    Iterator begin() const {
        return Iterator(this, 0);
    }

    /**
     * Get iterator past the last allocation
     */
    Iterator end() const {
        return Iterator(this, current_offset);
    }

    /**
     * Get current buffer usage
     */
    std::size_t get_used_bytes() const {
        return current_offset;
    }

    /**
     * Get current buffer capacity
     */
    std::size_t get_capacity() const {
        return buffer.capacity();
    }

    /**
     * Clear the arena (destroy all objects)
     */
    void clear() {
        std::size_t offset = 0;
        while (offset < current_offset) {
            AllocationHeader& header = get_header(offset);
            std::size_t object_size = header.size;

            if (header.is_alive && header.destructor) {
                header.destructor(get_object_pointer(offset));
            }

            offset += HEADER_SIZE + object_size;
        }
        buffer.clear();
        current_offset = 0;
    }

    ~ArenaAllocator() {
        clear();
    }
};
