// vr_mapstyle.hpp -- a map's style per area, from the textures round it (DYNAMIC_SPAWNS.md, "Prerequisites
// available"): base/tech, medieval, runic/metal, and water, slime or lava areas. For the dynamic spawns' placement rules
// (not used for spawning yet: no gameplay reads it). The world's surfaces are binned once a map into cells of
// cellSize units, each weighted by its area; an area's style is the heaviest in its cell and the cells round it, a
// liquid's when liquids are a quarter of it or more.
//   QC: float(vector p) mapstyle (VR_STYLE_* numbers: Style's)
//   Console: vr_mapstyle [x y z] (the style where you stand, or at a point, and its weights); vr_mapstyle_dump (the
//   map's totals and every cell that has surfaces)
#pragma once

#include <glm/glm.hpp>

namespace qvr::mapstyle
{
enum class Style : int
{
    Unknown = 0,
    Base = 1,     // tech, comp, plat panels (E1)
    Medieval = 2, // wbrick, wizmet, city, wood (castles)
    Runic = 3,    // rune, wizard, skill, metal (E2-E4's runic metal)
    Water = 4,
    Slime = 5,
    Lava = 6,
    Count = 7
};

inline constexpr float cellSize = 512.f;

// A texture's style (Unknown: sky, clip, triggers, anything the rules don't name).
[[nodiscard]] Style ofTexture(const char* texture);
[[nodiscard]] const char* name(Style s);
// The style of the area round `p` on the server's map (Unknown: no map, nothing there).
[[nodiscard]] Style at(const glm::vec3& p);

void registerCommands();
void builtin(); // QC's mapstyle(vector p)
} // namespace qvr::mapstyle
