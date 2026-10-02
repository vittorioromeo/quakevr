// vr_retro.hpp -- retro textures (vr_retro; docs/vr-port/ROUND21.md, "Retro textures"): high-resolution textures drawn
// in Quake's chunky low-resolution look by the shaders that read them (vr_retro.h has the GLSL and how it works).
//
// Each kind of thing drawn is a category with its own full set of settings, cvars vr_retro_<key><suffix> (World:
// vr_retro_world, vr_retro_world_block, ...), made here from the tables in vr_retro.cpp; vr_retro is the switch for
// them all. A category is a set in the shaders' block (its index + 1; 0: none).

#pragma once

struct cvar_s;

namespace qvr::retro
{

enum class Category
{
    World,     // the world's own surfaces (worldspawn), its liquids
    Brush,     // brush entities: doors, lifts, buttons, moving walls
    Items,     // pickups: the ammo and health boxes (maps/b_*.bsp), armour, keys, powerups, backpacks
    Props,     // crates, explosive boxes, rocks, bricks, planks, shell casings, lanterns
    Gibs,      // gibs and heads
    SmallGibs, // the small gibs torn out by hits (gibs scaled down)
    Weapons,   // weapons in the world: dropped, thrown, lying as pickups
    Held,      // the weapons in your hands and holsters
    Monsters,  // monsters, their corpses, other players
    Hands,     // your hands (hand*, finger_*), and your body's vertices on its hand and wrist bones
    Arms,      // your body's arms (its vertices whose heaviest bone is an upper arm's, a forearm's or a twist joint's)
    Torso,     // your body's torso and head (pelvis, spine, chest, neck, head, clavicles; the body without bones whole)
    Legs,      // your body's legs (thighs, calves, feet)
    Gear,      // your other things: the wrist gadget, the flashlight, pauldrons, pouches, leg holsters
    Other,     // every other model: projectiles, torches and flames, ...
    Count
};

enum class Param
{
    On,          // the category's switch (with vr_retro)
    Snap,        // texel snapping: the texture read on a grid of blocks
    Block,       // a block's size: Quake texels of the texture (a model's skin's), or world units with Units
    Units,       // 1: Block in world units, whatever the texture's scale on the surface
    Average,     // 1: a block's colour is the average of the texels under it (0: the one at its centre)
    Soft,        // the blocks' edges blended over this many pixels (0: hard, aliased)
    Fade,        // blocks a pixel spans where it is plain mipmapping again (fading from half that)
    Palette,     // pulled to Quake's 256 colours, 0..1
    Dither,      // an ordered dither before that, fixed to the blocks, 0..1
    DitherScale, // the dither's cell, in blocks
    Bump,        // bump and specular maps: 0 smooth (the high-resolution ones), 1 the blocks', between: a blend
    Detail,      // how much of the detail textures' grain (vr_detail) stays on these, 0..1 (the world's and brushes')
    Count
};

constexpr int categoryCount = static_cast<int>(Category::Count);
// Your body (progs/vrbody*) is drawn in one draw with a set for each part (Hands .. Legs, in that order: the instance's
// set is the Hands one's and a vertex adds its heaviest bone's part, InstanceData's RetroPart).
constexpr int bodyPartCount = 4;
constexpr int paramCount = static_cast<int>(Param::Count);

// VR_Init: the categories' and the editor's cvars and the commands (vr_retro_reset, vr_retro_list, vr_retro_pick,
// vr_retro_override, vr_retro_overrides_reload).
void registerCvars();

// Each frame (VR_Frame): a pick's countdown and outline; your overrides written a second after the last edit.
void frame();

// ---- Per-object overrides: by model name (alias models, maps/b_*.bsp boxes and other .bsp models) or by world texture
// (the world's and brush entities' surfaces; an animation's frames together), each setting inherited from the object's
// category, replaced or multiplied. Shipped ones in quakevr/retro_overrides_default.txt (committed), yours in
// quakevr/retro_overrides.txt (written by the editor; yours for a model or texture replaces the shipped one whole). A
// model's override goes on top of its category, a texture's on top of that.
enum class OverrideMode
{
    Inherit,
    Replace,
    Multiply
};

// The editor (Graphics > Retro Textures > Override): what was picked (vr_retro_pick), its override in these cvars
// (vr_retro_edit_<setting> and _mode; editing them writes it to your file), keyed by the model or the texture
// (vr_retro_edit_kind) when it has both.
[[nodiscard]] cvar_s& editValue(Param p);
[[nodiscard]] cvar_s& editMode(Param p);
[[nodiscard]] cvar_s& editKindCvar();
[[nodiscard]] bool hasTarget();
[[nodiscard]] bool targetHasBoth();
[[nodiscard]] const char* targetText();
void useShipped();    // your entry for it removed: the shipped one (if any) applies again
void clearOverride(); // your entry for it inherits everything (hides a shipped one)
void saveNow();

// ---- All Categories (Graphics > Retro Textures > All Categories): one set of values (vr_retro_all_<setting>), the
// settings to apply (vr_retro_all_apply_<setting>; On unchecked by default) and the categories to apply them to
// (vr_retro_all_to_<key>). vr_retro_all_apply copies them; with vr_retro_all_live (not archived) each change of a
// checked value is copied as it is made. vr_retro_all_copy [category] (none: vr_retro_all_from's) takes a category's
// values as the starting point. Per-object overrides are untouched.
[[nodiscard]] cvar_s& allValue(Param p);
[[nodiscard]] cvar_s& allApply(Param p);
[[nodiscard]] cvar_s& allTarget(Category c);
[[nodiscard]] cvar_s& allLiveCvar();
[[nodiscard]] cvar_s& allFromCvar();
void allApplyNow();
void allCopyChosen();
void allCheckSettings(bool on);
void allCheckCategories(bool on);
[[nodiscard]] const char* allSummary();

[[nodiscard]] cvar_s& cvarOf(Category c, Param p);
[[nodiscard]] const char* categoryLabel(Category c);
[[nodiscard]] const char* categoryHelp(Category c);
[[nodiscard]] const char* categoryKey(Category c);

} // namespace qvr::retro
