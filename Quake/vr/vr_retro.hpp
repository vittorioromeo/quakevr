// vr_retro.hpp -- retro textures (vr_retro; docs/vr-port/ROUND21.md, "Retro textures"): high-resolution textures drawn
// in Quake's chunky low-resolution look by the shaders that read them (vr_retro.h has the GLSL and how it works).
//
// Each kind of thing drawn is a category with its own full set of settings, cvars vr_retro_<key><suffix> (World:
// vr_retro_world, vr_retro_world_block, ...), made here from the tables below; vr_retro is the switch for them all.
// A category is a set in the shaders' block (its index + 1; 0: none).

#pragma once

struct cvar_s;

namespace qvr::retro
{

enum class Category
{
    World, // the world's own surfaces (worldspawn), its liquids
    Count
};

enum class Param
{
    On,          // the category's switch (with vr_retro)
    Snap,        // texel snapping: the texture read on a grid of blocks
    Block,       // a block's size: Quake texels of the texture (or world units with Units)
    Units,       // 1: Block in world units, whatever the texture's scale on the face
    Average,     // 1: a block's colour is the average of the texels under it (0: the one at its centre)
    Soft,        // the blocks' edges blended over this many pixels (0: hard, aliased)
    Fade,        // blocks a pixel spans where it is plain mipmapping again (fading from half that)
    Palette,     // pulled to Quake's 256 colours, 0..1
    Dither,      // an ordered dither before that, fixed to the blocks, 0..1
    DitherScale, // the dither's cell, in blocks
    Bump,        // bump and specular maps: 0 smooth (the high-resolution ones), 1 the blocks', between: a blend
    Detail,      // how much of the detail textures' grain (vr_detail) stays on these, 0..1
    Count
};

constexpr int categoryCount = static_cast<int>(Category::Count);
constexpr int paramCount = static_cast<int>(Param::Count);

// VR_Init: the categories' cvars and the vr_retro_reset command.
void registerCvars();

[[nodiscard]] cvar_s& cvarOf(Category c, Param p);
[[nodiscard]] const char* categoryLabel(Category c);

} // namespace qvr::retro
