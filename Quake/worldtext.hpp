#pragma once

#include <string>

#include "quakeglm_qvec3.hpp"

// TODO VR: (P0) move
struct WorldText
{
    std::string _text;
    qvec3 _pos;
    qvec3 _angles;
};

using WorldTextHandle = int;
