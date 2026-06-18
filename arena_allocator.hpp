#pragma once

#include <vector>
#include <memory>
#include <cstring>
#include <cassert>
#include <new>

/**
 * ArenaAllocator: A growth-based arena allocator that stores objects in a std::vector<std::byte>.
 * 
 * Properties:
 * - Objects are stored contiguously with proper alignment
 * - When new allocation doesn't fit, vector doubles in size
 * - All objects are moved to the new vector with proper construction/destruction
 * - Destroyed objects leave gaps but remain in place
 * - Each allocation tracks whether the object is still alive via a flag
 */
class ArenaAllocator {
private:
    struct AllocationHeader {
        std::size_t size;      // Size of the actual object (excluding header)
        bool is_alive;         // Flag indicating if object is still alive
        void (*destructor)(void*); // Destructor function pointer
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

    /**
     * Get the object pointer given the offset (points to data after header)
     */
    std::byte* get_object_pointer(std::size_t offset) {
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
