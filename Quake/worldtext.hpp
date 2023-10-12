#pragma once

#include <string>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "quakeglm_qvec3.hpp"

#undef min
#undef max

struct WorldText
{
    enum class HAlign : std::uint8_t
    {
        Left = 0,
        Center = 1,
        Right = 2
    };

    std::string _text{};
    qvec3 _pos{};
    qvec3 _angles{};
    HAlign _hAlign{HAlign::Left};
    float _scale{1.f};
};

using WorldTextHandle = std::uint16_t;

inline constexpr std::size_t maxWorldTextInstances =
    std::numeric_limits<WorldTextHandle>::max();
