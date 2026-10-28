#pragma once

// vr_agarmor.hpp -- armour for the armed humanoids, P0 (the spike; ROUND22.md, "Armour for armed humanoids, P0"): shells
// made from the monster's own mesh on its authored rig (vr_rig2.hpp), drawn with the bones it is drawn with.
//
// The shells: Mod_LoadModel of "<model>#armor" (VR_SyntheticModel) makes a skeletal model of the .mdl's rest pose's
// triangles of each piece (the cuirass: the spine's and chest's skin weights; the helmet: the head's, above the brow),
// clipped where their weight crosses 0.5, subdivided, smoothed (Taubin; the edge along itself), never inside the body
// where it was, pushed out along their normals by a gap and a plate's thickness, two-sided with a rim; each vertex
// weighted like the body under it. Textures: quakevr/progs/vragarmor.mdl's skins (0 green, 1 yellow, 2 red; made by
// Misc/quakevr/make_agarmor.py from the player's armour painter). Every piece's bones are its own slots of the model's
// bone list, so a piece is hidden by collapsing its slots (the body's `hidden` bones' way): one draw per armoured body.
//
// Drawn: every body drawn on its authored rig ("<model>#rig2", live, a ragdoll or the Rig Studio's subject) of a model
// with armour (P0: the grunt) gets an extra entity this frame (relink) whose bones are its source's as drawn (bonePoses:
// IK, its stretch, hit reactions, ragdolls, a hidden head alike). Who wears what: vr_agarmor (debug; 0 none, 1 green,
// 2 yellow, 3 red, 4 one of each by entity number), vr_agarmor_pieces (1 cuirass, 2 helmet), vr_agarmor_set per entity.
// Nothing is drawn while vr_agarmor is 0 and no entity was set.

#include "vr_engine.hpp"

namespace qvr::agarmor
{

// Each piece its own bit of a mask (vr_agarmor_pieces, QC's .vr_ar_mask): 1 the cuirass, 2 the helmet, 4/8 the
// pauldrons (upper arms, left/right), 16/32 the bracers (forearms), 64/128 the thighs, 256/512 the shins. QC's
// vr_agarmor.qc has the same numbers.
enum Piece : int
{
    Cuirass = 0,
    Helmet = 1,
    PauldronL = 2,
    PauldronR = 3,
    BracerL = 4,
    BracerR = 5,
    ThighL = 6,
    ThighR = 7,
    ShinL = 8,
    ShinR = 9,
    NumPieces = 10,
};
inline constexpr int allPieces = (1 << NumPieces) - 1;

// ---- The server's (P1: vr_agarmor.qc keeps each humanoid's type and pieces in .vr_ar_type and .vr_ar_mask) ----
// VR_ServerFrameEnd: each entity's armour (its fields) sent to the clients when it changed (QVR_SVC_ARMOR, reliable).
void serverFrame();
// VR_WriteClientSpawnState: every armoured entity's armour (after a clear), for a client coming in (a load, a new map).
void serverWriteAll(struct sizebuf_s* msg);
// The piece a hit at `at` on `ent` strikes (the bone of its vertex nearest, as drawn: box3d::boneAt); -1 none (no plate
// there: a hand, a foot, the pelvis), -2 no rig.
[[nodiscard]] int pieceAt(struct edict_s* ent, const float* at);
// Where piece `piece` of `ent` is now as a model of its own ("<model>#plate<piece>", derivedFile): `what` 0 its origin,
// 1 its angles (an alias model's), as drawn (hitmodel's poses; else its rest pose where the entity stands).
void plateWhere(struct edict_s* ent, int piece, int what, float* out);
// Mod_LoadModel (VR_DerivedModelFile): "<model>#plate<piece>"'s .mdl file (VR_HeapMalloc'd), or null.
[[nodiscard]] unsigned char* derivedFile(const char* name, unsigned int* path_id);

// ---- The client's ----
// QVR_SVC_ARMOR: [short entity, -1 all cleared][byte type][short pieces][short at 2/3 or under][short at 1/3 or under].
void parseEvent();
// The client's word on entity `num` (the tests: ragdollbone's QVR_ARMOR_CLIENT): `what` 0 its type, 1 its pieces, 2 its
// wear (bits 0-9 at two thirds, 10-19 at a third); -1 none heard.
[[nodiscard]] int clientState(int num, int what);

// Mod_LoadModel's "<model>#armor" (vr_ragdoll.cpp VR_SyntheticModel). False: not one (or no rig, no armour for it).
[[nodiscard]] bool makeModel(qmodel_t* mod);
// VR_Rig2Swap, after the authored rig's swaps: an armour entity added for each armoured body drawn on it.
void relink();
// VR_AliasBonePoses: an armour entity's bones (its source's as drawn, a hidden piece's collapsed). 0: not one.
[[nodiscard]] int bonePoses(const struct entity_s* e, const float** matrices);
// The body an armour entity is drawn on (nullptr: not one).
[[nodiscard]] const struct entity_s* sourceOf(const struct entity_s* e);
// Entity `num`'s armour: type 0 none, 1 green, 2 yellow, 3 red; `pieces` a mask of Piece bits; -1: vr_agarmor's again.
void setWorn(int num, int type, int pieces);

void init(); // the commands (vr_agarmor_info, _check, _rebuild, _set)

} // namespace qvr::agarmor
