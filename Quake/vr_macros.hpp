#pragma once

#define VRUTIL_POWER_OF_TWO(xExponent) (1 << xExponent)

#include <string>
#include "quakeglm.hpp"

// TODO VR: (P0) move
struct WorldText
{
    std::string _text;
    qvec3 _pos;
    qvec3 _angles;
};

using WorldTextHandle = int;
