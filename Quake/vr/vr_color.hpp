// vr_color.hpp -- colour helpers.

#pragma once

#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmod.hpp"

#include <glm/glm.hpp>


namespace qvr
{

// A colour from hue (degrees), saturation and value (0..1).
[[nodiscard]] inline glm::vec3 hsv(float hueDegrees, float saturation, float value)
{
    const float h = za::fmod(za::fmod(hueDegrees, 360.f) + 360.f, 360.f) / 60.f;
    const float c = value * saturation;
    const float x = c * (1.f - za::fabs(za::fmod(h, 2.f) - 1.f));
    glm::vec3 rgb{0.f};
    if(h < 1.f) { rgb = {c, x, 0.f}; }
    else if(h < 2.f) { rgb = {x, c, 0.f}; }
    else if(h < 3.f) { rgb = {0.f, c, x}; }
    else if(h < 4.f) { rgb = {0.f, x, c}; }
    else if(h < 5.f) { rgb = {x, 0.f, c}; }
    else { rgb = {c, 0.f, x}; }
    return rgb + (value - c);
}

} // namespace qvr
