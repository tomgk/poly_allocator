#ifndef ARENA_ALLOCATOR_BASIC_H
#define ARENA_ALLOCATOR_BASIC_H

#include<cstring>
#include<cassert>
#include<typeinfo>
#include<string>

#ifdef ARENA_ALLOCATOR_LOG
#include<iostream>
#endif

std::string getTypeName(const std::type_info &type);

enum class ArenaMode
{
    Lightweight,  // No TypeInfo, no Copy/Destructor function pointers (Trivial types only)
    Standard,     // No TypeInfo, keeps Copy/Destructor function pointers
    TypeAware     // Stores TypeInfo and keeps Copy/Destructor function pointers
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
concept PlainObject = (std::is_trivial_v<T> && std::is_standard_layout_v<T>) || !std::is_class_v<T>;

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

    ArenaArrayView(): ArenaArrayView(nullptr, 0){}
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

#if __cplusplus >= 202002L
#include <span>
template <typename T>
using ArenaArrayResult = std::span<T>;
#else
// Fallback for older C++ standards
template <typename T>
using ArenaArrayResult = ArenaArrayView<T>;
#endif

#endif // ARENA_ALLOCATOR_BASIC_H
