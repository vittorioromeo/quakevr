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
// sharp-edged marks in Quake's palette colours; blood and water shine and fill the normal map's bumps; fresh burns
// glow in their cracks.
//
// Wetness dries in about 25 seconds (dripping while wet), from the waterline down; embers cool in a few seconds; blood
// and char stay (on corpses for good). The player's own body and jointed hands take the wounds where the blow came
// from, instead of the wound skins (the armour skins stay), and heal: blood and char fade as health comes back, and all
// of it goes on respawning. A new map starts clean, as does a loaded game (the masks are the client's only).

#pragma once

struct entity_s;

namespace qvr::wounds
{

// QVR_SVC_WOUND: a wound (vr_client.cpp's dispatch).
void parseEvent();
// QVR_SVC_WOUNDCLEAR: the entity's wounds forgotten (removed on the server: the next one in its slot starts clean).
void parseClear();

// Once a frame, after the view's entities are set up (VR_SetupViewEntities: the body and the hands posed): the wounds
// received painted, drying, cooling and healing, the drips.
void frame();

// Whether the player's body and hands show their wounds painted (vr_wounds) rather than the wound skins.
[[nodiscard]] bool replacesSkins();

// A new map, a disconnect, a game directory change: every mask freed (the texture kept).
void clear();

// vr_wounds_test <entity|self> <kind> [amount] [right] [up]: a wound as the server would send it, on entity number n
// (or the player), from the view (the player: from ahead), kind QVR_WOUND_* (1 shot .. 9 liquid), `right`/`up` units
// off the model's middle (a liquid: `up` is its surface's height over the model's feet).
void test_f();

// vr_wounds_dump: every mask in use written to <gamedir>/wounds/mask_<layer>_<model>.png (r blood, g char, b wetness,
// heat as white), laid out as its skin.
void dump_f();

// vr_wounds_info: the pool (layers used, what they are on) and the last paint's cost.
void info_f();

// The gore's bloody hands and washing (vr_gore_hands, vr_gore_wash*, vr_gore_reopen*; vr_wounds.cpp): with the wound
// skins (vr_wounds 0), the damage skin the player's `part` (0 the off hand, 1 the main hand, 2 the body), drawn at
// `origin` (null: unknown), shows for the damage skin `level`: none after a wash until the wounds re-open (or a new
// hit), at least a bloody one while a gib's blood is on a hand.
[[nodiscard]] int skinLevel(int part, int level, const float* origin);

// vr_gore_hands_test [off|main] [amount]: a gib's blood on a hand, as taking one (the main hand; vr_gore_hands).
void handsTest_f();

// vr_gore_hands_info: the player's wounds kept to re-open, the wash, and the blood on the hands and the body (texels).
void handsInfo_f();

} // namespace qvr::wounds
