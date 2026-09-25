// vr_modellight.hpp -- models (monsters, items, weapons, hands) shaded from the map's lights.
//
// Ironwail shades every alias model from one fixed world direction, whatever lights it; up close
// in VR that is the flattest thing on screen. The map's light entities (parsed when it loads) give
// each model a direction instead: the strongest few that reach it and see it (Quake's linear
// falloff, a line of sight through the world's BSP), weighted and smoothed over time. The alias
// shader mixes it with the fixed direction by how coherent the light is (vr_model_lighting).

#pragma once

#include "vr_engine.hpp"

#include <vector>

namespace qvr::modellight
{

// A light entity of the map (lit at the start).
struct MapLight
{
    glm::vec3 pos;
    float value; // Quake's "light": brightness at the light, falling off linearly
    float scale; // "wait": how fast it falls off (1 by default); it reaches value / scale units
};

// The current map's lights, parsed when it loads.
[[nodiscard]] const std::vector<MapLight>& mapLights();

// World-space direction towards the light reaching `e` (xyz) and how much it applies (w, 0..1);
// {0, 0, 0, 0} keeps the fixed direction. Updated at most once per frame per entity.
[[nodiscard]] glm::vec4 direction(const entity_t* e);

} // namespace qvr::modellight
