// vr_rigseq.hpp -- the Rig Studio's animations in the game (docs/vr-port/RIGSTUDIO.md, "In the game"): the override
// tracks and the new sequences of vr_rigdata.hpp drawn on the rigged monsters, and the sequences started by QC.
//
// - The server (QC): a monster's `.string vr_rigseq` set to a sequence's name starts it ("-" stops it); the engine takes
//   the request at the server frame's end (VR_ServerFrameEnd), writes `.float vr_rigseq_len` (its length in seconds; -1:
//   no such sequence for its model) and, each frame it plays, `.float vr_rigseq_frame` (its frame now; -1 none), and
//   calls QC's `void(string seq, string ev, float frame) VR_RigSeq_Event` (self the monster) for each of its events as
//   its frame passes it, "end" when it ends and "stopped" when it is cut short (stopped, or its held frame changed by
//   the monster's own animation: a pain). Every client is told (QVR_SVC_RIGSEQ, reliable).
// - The clients draw it (kickInstance, vr_animsmooth.cpp): the sequence's keyed bones laid over the frame drawn (the
//   unkeyed ones the frame's), blended in from it and back out; a model's override tracks over its own animations by
//   their frames; an armed humanoid's arms (a sequence with `arms 1`) by their hands' places, through the arms' IK
//   (agarms::Targets' fk).
// - The studio's subject: its pose this frame (setStudio), whatever it plays.
// Nothing runs without the files, the studio, or a sequence started.

#pragma once

#include "vr_agarms.hpp"
#include "vr_engine.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

namespace qvr::rigseq
{

// The client's layer (kickInstance): cheap first (no files, no studio, nothing playing: false at once).
[[nodiscard]] bool wants(const entity_t* e);
// `e`'s bones with the layer (rest -> now, model space) over the frame drawn (`posed`: the frames' fits at pose1, pose2,
// blend); `arms`: its arm bones drive an armed humanoid's arms. False: nothing for `e` now.
bool bones(const entity_t* e, const ragdoll::Rig& rig, const aliashdr_t* hdr, int pose1, int pose2, float blend,
    const rigpose::Rigid* posed, rigpose::Rigid* F, bool* arms);
// kickInstance: `kick` (the bones' turns over `posed`, boneTurns') with the layer laid under it: drawn = kick (F
// posed^-1) posed. False: nothing for `e` (`kick` untouched).
bool layer(const entity_t* e, const ragdoll::Rig& rig, const aliashdr_t* hdr, int pose1, int pose2, float blend,
    const rigpose::Rigid* posed, rigpose::Rigid* kick);
// relinkSkinned, before the solve (the layer's arms on): an armed humanoid's hands put where its forearm bones are drawn
// (`posed`, every layer on, and `kick` its hit reactions or null), its elbows' poles so (t.fk).
void adjustTargets(const ragdoll::Rig& rig, const agarms::Spec& spec, const rigpose::Rigid* posed, const rigpose::Rigid* kick,
    agarms::Targets& t);

// The studio: entity `ent` drawn with these bones (model space, rest -> now) this frame; `arms`: its arms by them too.
// ent 0: none.
void setStudio(int ent, const rigpose::Rigid* F, int numBones, bool arms);
[[nodiscard]] int studioEnt();

// The client's sequences (QVR_SVC_RIGSEQ).
void parseMessage();
void clientClear(); // a map change, a disconnect

// The server's frame end: QC's requests taken, the plays advanced, their events sent to QC.
void serverFrame();
void serverClear(); // a new map

// Debug: the play on entity `ent` now (client): its sequence's name ("": none), its time (s) and weight.
bool playingOn(int ent, const char*& name, float& time, float& weight);

void registerCommands(); // vr_rigseq_play, vr_rigseq_stop, vr_rigseq_status

} // namespace qvr::rigseq
