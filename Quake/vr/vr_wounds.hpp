// vr_wounds.hpp -- wounds painted on models (vr_wounds, vr_wounds_burns, vr_wounds_wet; ROUND21.md, "Dynamic wounds,
// burns and wetness").
//
// Every monster, player and corpse drawn with an alias model gets, on its first wound, a mask of its own in its skin's
// layout (a layer of one texture array: 256 x 256 at most, the skin's shape, the skin's size when it is smaller): red
// blood, green char, blue wetness, alpha heat (a fresh burn's embers). The server says where a model was hit, by what,
// how hard (QVR_SVC_WOUND: QC/vr_wounds.qc), and how high the liquid it stands in comes up it; the client draws the
// model as it is at that moment into its mask, laid out by its skin's coordinates, and each texel takes what the
// splats paint at the model's surface there -- by distance in the world, so a wound goes across the skin's seams as it
// goes across the model (r_alias.c's R_PaintAliasWounds, gl_shaders.h's wound_paint_fragment_shader). The model's
// shader reads the mask at its skin's texels and shows it with an ordered dither (gl_shaders.h's WoundsAt): chunky,
// sharp-edged marks in Quake's palette colours; blood shines and fills the normal map's bumps; fresh burns glow in
// their cracks. Wetness is not dithered: where the mask is wet at all, an even coat (vr_wounds_wet_coat: darker, a
// glossy film in the model's own light) whose strength is the model's, fading as it dries; a weapon or prop in a hand
// that went under the surface takes it all over (wetCoat).
//
// Wetness dries in about 28 seconds (dripping while wet), from the waterline down; embers cool in a few seconds; blood
// and char stay (on corpses for good). The player's own body and jointed hands take the wounds where the blow came
// from, instead of the wound skins (the armour skins stay), and heal: blood and char fade as health comes back, and all
// of it goes on respawning. A new map starts clean, as does a loaded game (the masks are the client's only).
//
// Your body's fine mask is one a side (its arms and legs share their skin's texels, mirrored), and the blood on your
// body and hands that isn't yours (spatter from hits near you, gibs striking you: vr_gore_spatter*) is a mask of its
// own, healing leaving it. What your hands hold (weapons, props) takes that blood too and keeps it, moving with a
// weapon thrown, taken again, holstered (vr_gore_gear); water washes it all (ROUND21.md, "Blood on you, your weapons
// and props").

#pragma once

#include "Zancle/Vocabulary/Span.hpp"

#include <glm/glm.hpp>

struct entity_s;

namespace qvr::wounds
{

// QVR_SVC_WOUND: a wound (vr_client.cpp's dispatch).
void parseEvent();
// QVR_SVC_WOUNDCLEAR: the entity's wounds forgotten (removed on the server: the next one in its slot starts clean).
void parseClear();
// QVR_SVC_WEAPONGONE: a weapon's record freed on the server (QC vr_weaponinst.qc): its blood forgotten.
void parseWeaponGone();
// The gore's burst (vr_gore.cpp's EventBurst: a monster gibbed, a gib bursting) at `org`, `size` times a gib's: its
// blood onto the weapons and props lying near (vr_gore_gear_nearby), what lies right at a big one soaked
// (vr_gore_gear_drops: a gibbed monster's gun and backpack). As parsed: painted in the frame.
void burst(const float org[3], float size);

// Once a frame, after the view's entities are set up (VR_SetupViewEntities: the body and the hands posed): the wounds
// received painted, drying, cooling and healing, the drips.
void frame();

// Whether the player's body and hands show their wounds painted (vr_wounds) rather than the wound skins.
[[nodiscard]] bool replacesSkins();

// A new map, a disconnect, a game directory change: every mask freed (the texture kept).
void clear();

// How many models have a mask now (vr_scene_count).
[[nodiscard]] int liveCount();

// Whether `e` has blood painted on it (a mask: hit, cut, bled on), for the bloody footprints (vr_bloodprints.cpp).
[[nodiscard]] bool bloody(const entity_s* e);

// The gore's bloody hands and washing (vr_gore_hands, vr_gore_wash*, vr_gore_reopen*; vr_wounds.cpp): with the wound
// skins (vr_wounds 0), the damage skin the player's `part` (0 the off hand, 1 the main hand, 2 the body), drawn at
// `origin` (null: unknown), shows for the damage skin `level`: none after a wash until the wounds re-open (or a new
// hit), at least a bloody one while a gib's blood is on a hand.
[[nodiscard]] int skinLevel(int part, int level, const float* origin);

// The point on a model a blow at `org` going `dir` (unit) struck, as the wounds and dents place it: `tris` its triangles
// as drawn (three corners each, the world), the front face the line through `org` meets nearest it, else the facing one
// nearest the line; `at`, the face's outward `normal`, and its first corner's index returned (-1: none).
[[nodiscard]] int strikeTriangles(za::Span<const glm::vec3> tris, const glm::vec3& org, const glm::vec3& dir, glm::vec3& at,
    glm::vec3& normal);

// vr_dummy_gore_reset: every training dummy's dents, blood streams and painted wounds cleared (Gore > Training Dummy).
void dummyReset_f();

// The test and report commands (vr_wounds_test, _dump, _info; vr_gore_hands_test, vr_wrist_blood_test, vr_gore_wash_test,
// vr_gore_hands_info, vr_gore_spatter_test; vr_wounds_diag.cpp), where their usage is.
void registerDiagCommands();

} // namespace qvr::wounds
