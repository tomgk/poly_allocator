#ifndef CALLBACK_H
#define CALLBACK_H

#include "arena_allocator_basic.h"

namespace callback
{

template <ArenaAllocatorConstructable T, typename AllocationHeader>
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

}

#endif // CALLBACK_H
