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

#if __cplusplus >= 202002L
#include <span>
template <typename T>
using ArenaArrayResult = std::span<T>;
#else
#error not supported
#endif

#endif // ARENA_ALLOCATOR_BASIC_H
