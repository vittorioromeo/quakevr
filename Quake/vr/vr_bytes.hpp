#pragma once

// vr_bytes.hpp -- little-endian reads from a byte buffer (model, BSP and pack files). le32/lef32 are the unchecked
// reads the model makers use on files they have already sized; View is the bounds-checked one (0 past the end).

#include "vr_engine.hpp"

#include "Zancle/Base/SizeT.hpp"

#include <string.h>

namespace qvr::bytes
{

// The int / float at byte offset `at` of `b` (the caller has checked the offset).
[[nodiscard]] inline int le32(const byte* b, const size_t at)
{
    int v;
    memcpy(&v, b + at, 4);
    return LittleLong(v);
}
[[nodiscard]] inline float lef32(const byte* b, const size_t at)
{
    float v;
    memcpy(&v, b + at, 4);
    return LittleFloat(v);
}

// A buffer read with bounds checks: a read that does not fit gives 0.
struct View
{
    const unsigned char* data{nullptr};
    za::SizeT            size{0};

    [[nodiscard]] int i32(const za::SizeT at) const
    {
        int v = 0;
        if(at + 4 <= size)
        {
            memcpy(&v, data + at, 4);
        }
        return LittleLong(v);
    }
    [[nodiscard]] short i16(const za::SizeT at) const
    {
        short v = 0;
        if(at + 2 <= size)
        {
            memcpy(&v, data + at, 2);
        }
        return LittleShort(v);
    }
    [[nodiscard]] unsigned short u16(const za::SizeT at) const { return static_cast<unsigned short>(i16(at)); }
    [[nodiscard]] float          f32(const za::SizeT at) const
    {
        float v = 0.f;
        if(at + 4 <= size)
        {
            memcpy(&v, data + at, 4);
        }
        return LittleFloat(v);
    }
};

} // namespace qvr::bytes
