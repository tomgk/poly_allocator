#ifndef CALLBACK_H
#define CALLBACK_H

#include "arena_allocator_basic.h"

namespace callback
{

template <ArenaAllocatorConstructable T, typename AllocationHeader>
static void CopyObject(void* src, void* dst, std::ptrdiff_t offset)
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

template<typename T>
void CopyObjectWithNoObject(void* src, void *dst, std::ptrdiff_t offset)
{
#ifdef ARENA_ALLOCATOR_LOG
    std::cout << "Copy " << typeid(T).name() << " " << src << " to " << dst << std::endl;
#endif
    new (dst)T(std::move(*reinterpret_cast<T*>(src)));
}

template<typename T>
void DestructObject(void* ptr)
{
#ifdef ARENA_ALLOCATOR_LOG
    std::cout << "Destruct " << typeid(T).name() << " " << ptr << std::endl;
#endif
    ///\todo check with std::is_constructible if there is a constructor with an offset
    auto str = reinterpret_cast<T*>(ptr);
    str->~T();
}

}

#endif // CALLBACK_H
