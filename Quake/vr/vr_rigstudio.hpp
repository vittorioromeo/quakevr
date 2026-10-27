// vr_rigstudio.hpp -- the Rig Studio (docs/vr-port/RIGSTUDIO.md; `vr_rigstudio`, Debug > Tools): a flat-screen editor,
// in the game, for the rigged humanoids' skeletons, skin weights, poses and animations, with Dear ImGui (vr_imgui.hpp).
// It loads a room (vr_rigstudio_map), stands a frozen armed humanoid (or any monster with a rig) in it and edits its
// model's data (vr_rigdata.hpp): the rest pose's bones and limits, the vertices' weights, named poses, override tracks on
// its own animations and new keyed sequences, saved to quakevr/rigs and quakevr/anims and drawn by the game at once
// (vr_rigseq.hpp). Its camera is Blender's (the middle mouse orbits, Shift pans, the wheel zooms, F frames, the numpad
// snaps). Closed, none of it runs: ImGui is made only when it first opens.

#pragma once

#include "vr_engine.hpp"
#include "vr_posestudio.hpp"

namespace qvr::rigstudio
{

void init();     // its commands (and the data's: vr_rigdata_*, vr_rigseq_*)
void shutdown(); // VR_Shutdown: closed, ImGui let go

[[nodiscard]] bool active();

// VR_BeginFrame (after the input events): the studio's frame (ImGui's, its windows, the camera, the edits).
void clientFrame();
// VR_ServerFrameEnd: its scene (the room's load, the subject made, held where it stands).
void serverFrame();
// VR_OnSpawnServerBeforeLoad: a map loading (the subject gone with its edicts: staged again in the new map).
void beforeLoad();
// QC's hold query (weapondrawnpose 19) for entity `ent` (its kind and weapon class as QC tells them): the studio's
// hold on its subject (x the pose, y its phase, z Play's count, as posestudio::qcHold) into `out`. False: not the
// subject (the Pose Studio's answer stands).
[[nodiscard]] bool qcHold(int ent, int kind, int cls, glm::vec3& out);

// R_SetupAliasFrame's end (VR_AnimSmoothPinned): the subject drawn at the timeline's frames. False: not the subject.
bool pinned(const entity_t* e, const void* aliashdr, short* pose1, short* pose2, float* blend);

// relinkSkinned (vr_hitreact.cpp): the subject's arms as solved (its IK targets, grips and weapons, shown).
[[nodiscard]] bool wantsNotes();
void note(const posestudio::Note& n);
// relinkArms (vr_hitreact.cpp), for the subject drawn on the authored rig (Range of Motion, vr_rig2 1: its model swapped
// to "<model>#rig2"): where its weapon `prop` in hand `side` (0 right, 1 left) goes (model space, `out`): on that rig's
// hand (the right on its `weapon` bone, the left on `weapon_l` or `hand_l`), the hand's rest grip frame
// (agarms::ragdollGrip) carried by the bone, the weapon at its grip offset `offset` (the Grip Editor's) from it.
// False: not the subject, or no rig or grip for that hand.
bool authoredWeapon(int ent, int side, int prop, const rigpose::Rigid& offset, rigpose::Rigid& out);

} // namespace qvr::rigstudio
