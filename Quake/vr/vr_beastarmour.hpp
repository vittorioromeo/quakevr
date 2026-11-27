#pragma once

// vr_beastarmour.hpp -- the beasts' armour (ROUND26.md, "beastarmour"): the rottweiler, the ogre, the scrag and the fiend
// wear the armed humanoids' armour (vr_agarmor.hpp, QC vr_agarmor.qc: the same pieces' slots, pools, rolls, breaking,
// debris and sounds), made of rigid pieces rather than shells cut from their bodies: each piece a model of its own
// (quakevr/progs/vrbarmor_<kind>_<slot>.mdl, Misc/quakevr/make_beastarmour.py) riding one bone of the monster's derived
// rig (vr_ragdoll.cpp's seed tables), so any monster with a derived rig can wear it (the shambler: one more table in the
// script). A large body's cuirass is several pieces on their own bones, each breaking on its own (`group`).
//
// The list: quakevr/data/beastarmour.txt (made by the script): per kind its model and cuirass group, per piece its slot,
// its bone, its model and its middle in the rest pose (pose 0). Where a piece is now: its bone's vertices as drawn
// (hitmodel::drawnVertexPoints: the animation's lerp, a ragdoll's parts), the rigid motion from their rest places
// (rigpose::fitRigid) carrying the piece's middle and turn. The server's queries answer for agarmor's (shots, a point,
// the bones a cut takes, where a piece is for its debris); the client draws each worn piece as an entity of its own (a
// listen server's view: the server's bodies as drawn; a remote client draws none yet).

#include "vr_engine.hpp"

#include <glm/fwd.hpp>

namespace qvr::beastarmour
{

// Whether `ent`'s model is a beast kind's (it may wear this armour), and `name`'s (a client entity's model).
[[nodiscard]] bool wears(struct edict_s* ent);
[[nodiscard]] bool kindNamed(const char* model);
// Its kind's slots (bits; 0 not a kind) and its cuirass group (the slots the roll's cuirass gives).
[[nodiscard]] int slots(struct edict_s* ent);
[[nodiscard]] int cuirassGroup(struct edict_s* ent);
// Piece `slot`'s model ("" none), a string kept for good.
[[nodiscard]] const char* pieceModel(struct edict_s* ent, int slot);

// agarmor's queries for a beast (agarmor::shotPiece, pieceAt, pieceBones, plateWhere).
[[nodiscard]] int shotPiece(struct edict_s* ent, const float* at, const float* dir, int mask);
[[nodiscard]] int pieceAt(struct edict_s* ent, const float* at);
[[nodiscard]] uint32_t pieceBones(struct edict_s* ent, int slot);
void plateWhere(struct edict_s* ent, int slot, int what, float* out);

// The client (agarmor::relink): begin a frame's entities, then each beast entity `e` (number `num`) wearing `type` on
// `pieces` (wear bits as agarmor's: 0-9 at two thirds, 10-19 at a third) gets its pieces' entities.
void beginRelink();
void relinkEntity(const struct entity_s* e, int num, int type, int pieces, int wear);
// How many pieces' entities were drawn this frame (the tests).
[[nodiscard]] int drawnCount();

void init(); // vr_beastarmour_info

} // namespace qvr::beastarmour
