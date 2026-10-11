// vr_brushbake.hpp -- the runtime brushes' full bake (docs/vr-port/BRUSH_TOOL.md 3.7 A): the map relit by ericw-tools'
// light (vr_relight's run, vr_brush_bake_map) with its brushes in it, so that their light is light.exe's (bounce, dirt,
// soft shadows, as the relit world's) and their shadows fall on the world's own lightmaps.
//
// Before light: each brush appended to the map's .bsp as an inline submodel after the map's own (its faces, planes,
// texinfo, vertices and edges; hull 0 a chain of nodes over an empty leaf, hulls 1 and 2 chains of clipnodes: a valid
// model nobody uses), each with an entity for light only ("func_qvr_brushbake", its model, "_shadow" "1"). After light,
// the map's own entities are put back with a comment line a brush ("// qvr_brushbake *<model> <hash> <faces>": COM_Parse
// skips comments) naming the submodel that has its light. When that relit copy is the client's world, a brush whose
// shape and textures are the same (its hash) takes its faces' lightmaps from those submodel faces (downsampled if they
// don't fit its rectangle) instead of the in-game bake; a brush changed since is baked in the game as before (the
// world's shadows of its old shape stay until the next bake). The map's numbering is kept (the submodels are appended),
// so its saves still load. BSP29, BSP2 and 2PSB maps; per-face BSPX lumps of the input are dropped (light makes its own).

#pragma once

#include "vr_brushlight.hpp"
#include "vr_brushes.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::brushbake
{

// The listen server's brushes appended to the .bsp `data` (see above): `out` the .bsp for light, `entities` the entities
// light needs for them (appended to the map's), `notes` the lines the result's entity lump keeps. False (and `error`)
// if there are none or the map's format can't take them (BSP29's limits).
// Tests (vr_brush_bake_write): the entities' class (func_wall: the submodels solid in a map), the entities and notes
// written into the entity lump, each face given `fakeLight` (0..255; -1: none) as its light (the read-back without light).
struct Options
{
    const char* classname{"func_qvr_brushbake"};
    bool entitiesIn{false};
    int fakeLight{-1};
};
bool augment(const unsigned char* data, za::SizeT size, za::Vector<unsigned char>& out, za::String& entities, za::String& notes,
    za::String& error, const Options& options = Options{});

// Brush `b`'s light as light.exe baked it in the client's world (see above): its faces' maps and texels in its
// rectangle; false if the world has none for it (or it has another number of faces than `faces`).
bool fromWorld(const brushes::Brush& b, int faces, za::Vector<brushlight::Map>& maps, brushlight::Texels& texels);
// How many brushes the client's world has light.exe's light for.
[[nodiscard]] int bakedCount();

} // namespace qvr::brushbake
