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
// `pieces` (wear bits as agarmor's: 0-9 at two thirds, 10-19 at a third) gets its pieces' entities (a piece armour.txt
// shapes drawn as "<piece model>#tweak") and one for its leather junctions ("<model>#bridge", its bones the pieces').
void beginRelink();
void relinkEntity(struct entity_s* e, int num, int type, int pieces, int wear);
// How many pieces were drawn this frame (`parts`: every entity, a slot's parts on other bones too), and junctions'
// entities (the tests).
[[nodiscard]] int drawnCount(bool parts = false);
[[nodiscard]] int leatherDrawn();
// A junctions' entity's bones (VR_AliasBonePoses through agarmor::bonePoses; 0 not one), and the models made here
// ("#tweak", "#bridge"; through agarmor::makeModel).
[[nodiscard]] int bonePoses(const struct entity_s* e, const float** matrices);
[[nodiscard]] bool makeModel(struct qmodel_s* mod);

// ---- armour.txt's tweaks (vr_armourdata.hpp: a beast by its model's path, its pieces by slot as armourdata::beastPart;
// each piece's sleeve its leather junction to the piece it joins) and the Armour Studio (vr_armourstudio.cpp) ----
// The kinds (beastarmour.txt's order), each one's model, its slots worn (bits), the slot a piece's junction joins (-1
// none), whether it is made (its leather on, edges to join), its bone, the model its piece is drawn with.
[[nodiscard]] int kindCount();
[[nodiscard]] const char* kindModel(int kind);
[[nodiscard]] int kindIndex(const char* model);
[[nodiscard]] int kindSlots(int kind);
[[nodiscard]] int joins(int kind, int slot);
[[nodiscard]] bool junctionMade(int kind, int slot);
[[nodiscard]] const char* boneOf(int kind, int slot);
[[nodiscard]] const char* drawnModel(int kind, int slot);
// A piece's box in its own space (its shape tweaks on it).
bool pieceExtent(int kind, int slot, float* lo, float* hi);
// The studio's subject (0 none): its body drawn `bodyAlpha` see-through, the plates and leather whole; the piece(s) picked
// (or their junctions: `sleeve`) outlined (agarmor::debugDraw).
void studioView(int ent, float bodyAlpha);
void outline(struct edict_s* ent, int pieces, bool sleeve);
// The studio's part (armourdata Part; -1 none): outline draws its pieces alone (a slot's tail apart from its belt), and
// partFrame takes its own piece; `alone` (wear the part alone): the subject draws that part's pieces alone, not the rest of
// their slot (the scrag's back half without his front half and tail; a junction's part: its slots whole).
void studioPart(int part, bool alone = false);
// The slots (bits) whose pieces or parts are armour.txt's `part` (a sleeve: their junctions) on `side` (a limb part's): the
// scrag's tail its belt's slot 8, the ogre's shins the thighs' 6/7 (ROUND26.md, "beastfit").
[[nodiscard]] int partPieces(int kind, int part, int side);
// The first piece of `pieces` on `ent` as placed now: its middle (world) and the map from its tweaks' space to the world's
// (agarmor::studioPartFrame).
bool partFrame(struct edict_s* ent, int pieces, glm::vec3& centre, glm::mat3& parent);
// The tests: piece `slot`'s junction on `ent` wearing `pieces` as posed now: its end rings' furthest from where their
// pieces' places carry them, its longest edge along it, its middle ring's middle.
struct JunctionCheck
{
    bool found{false};
    int verts{0};
    float endError{0.f};
    float longestEdge{0.f};
    float middle[3]{};
};
[[nodiscard]] JunctionCheck checkJunction(struct edict_s* ent, int slot, int pieces);

void init(); // vr_beastarmour_info

} // namespace qvr::beastarmour
