#ifndef CALLBACK_ARRAY_H
#define CALLBACK_ARRAY_H

#include "arena_allocator_basic.h"

namespace callback
{

template<typename T>
static void CopyArray(void* src, void* dst, std::size_t element_count, std::ptrdiff_t offset)
{
    if (!src || !dst || element_count == 0) return;

    T* src_array = reinterpret_cast<T*>(src);
    T* dst_array = reinterpret_cast<T*>(dst);
    std::size_t total_bytes = element_count * sizeof(T);

    // Pfad 1: Für triviale Typen/Plain Objects (memcpy)
    if constexpr (PlainObject<T>)
    {
        std::memcpy(dst, src, total_bytes);
    }
    // Pfad 2: Für komplexe Objekte (Placement-New mit Element-Iterierung)
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
                    // Fallback für Typen ohne Custom-Offset-Unterstützung
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

#endif // CALLBACK_ARRAY_H
