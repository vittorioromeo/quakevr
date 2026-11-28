#pragma once

// vr_attach.hpp -- weapon attachments (docs/vr-port/ATTACHMENTS.md): props such as the bayonet fixed on a weapon, part of
// the weapon's own state (QC vr_attach.qc: its record's .wi_attach, so it goes wherever the weapon goes: hands, holsters,
// drops, throws, embeds, saves, level changes). Here: the kinds (their models and points), each weapon's mounts (Weapon
// Offsets keys attN_*, edited in Weapon Offsets > Attachments), the clients told which weapon has which attachment
// (QVR_SVC_WEAPONATTACH by the weapon's id; another player's guns: their VrGunLook's), and where an attachment is drawn on
// a weapon entity (view::placeAttachment, vr_view.cpp: held, holstered, lying, another player's).

#include "vr_engine.hpp"

#include <glm/glm.hpp>

namespace qvr::attach
{

enum Kind : int
{
    None = 0,
    Bayonet = 1,
    Dagger = 2, // the player's dagger (QC VR_SWORDKIND_DAGGER) put on a gun as a bayonet (vr_attach_daggers)
    Count // (kinds 1 .. Count - 1)
};

struct KindInfo
{
    const char* name;  // its word (menus, the console)
    const char* model; // its model (precached by QC: vr_bayonet.qc)
    glm::vec3 tip;     // its striking point in its own model space (none: '0 0 0')
    glm::vec3 base;    // where its blade starts (its own model space)
    // Its mount frame in its own model space (flatbayonet): `forward` along its blade towards the tip, `across` along its
    // guard (the frame's left); up is forward x across. A mount's Pitch / Yaw / Roll turn this frame (Mount::basis), so all
    // zero lays the blade along the barrel with the guard level, and each turns about the blade's own axes.
    glm::vec3 forward;
    glm::vec3 across;
};

// The kind's data (null: none or unknown).
[[nodiscard]] const KindInfo* info(int kind);
// Whether the kind strikes as a blade (it has a tip: the bayonet, the dagger): the gun stabs and sticks by its point.
[[nodiscard]] bool strikes(int kind);

// A weapon's mount for an attachment (its Weapon Offsets keys attN_*: the gun model's own space, frame 0).
struct Mount
{
    bool fits{false};
    glm::vec3 pos{0.f};
    glm::vec3 angles{0.f}; // pitch up, yaw left, roll (the hands' order: view::anglesBasis) of the kind's mount frame
    // Its model's axes in the gun's (view::anglesBasis(angles) x the kind's frame: KindInfo::forward / across): yaw about
    // the gun's up, then pitch about the guard, then roll about the blade.
    glm::mat3 basis{1.f};
    float scale{1.f};
    // The attachment model's own Scale, as the renderer draws it (a weapon model's weapons.txt Scale: the dagger's 0.34;
    // 1 the bayonet, which has no slot): its points are drawn that much nearer its origin (onGun counts it).
    float own{1.f};
};
[[nodiscard]] Mount mount(int slot, int kind);
// A point of the attachment (its own model space) in the gun model's own space, by the mount.
[[nodiscard]] glm::vec3 onGun(const Mount& m, const glm::vec3& p);

// The bayonet's mount drawn on a weapon a bayonet fits (bayomounts: progs/bayonet_mount.mdl, its origin the bayonet's):
// at the gun's bayonet mount moved and turned by its Weapon Offsets mnt_* keys (in the bayonet's units and frame), sized
// mnt_size times the bayonet. `model`: the gun's (a weapon slot's, or a map pickup's: pickupRails in vr_attach.cpp).
// False if no bayonet fits it (or vr_attach_mount_show 0).
constexpr const char* railModel = "progs/bayonet_mount.mdl";
[[nodiscard]] bool railMount(const qmodel_t* model, Mount& out);
// The weapon slot whose bayonet mount the gun drawn with `model` takes (its own, or a pickup's gun's); -1 none.
[[nodiscard]] int railSlot(const qmodel_t* model);
// A mount of `kind` made before its frame (its angles turned its model's own axes) as the frame's angles and place: the
// same blade, its guard rolled level, its tip kept (vr_wofs_version 80). `size`: the mount's scale times the
// attachment's own (Mount::scale * Mount::own).
void convertOldMount(int kind, glm::vec3& pos, glm::vec3& angles, float size);
// The one place an attachment's points are measured from a gun (the server's: QC attachinfo; the drawn one is placed by
// the same view::ModelSpace, view::placeAttachment): point `what` (1 its mount, 2 its tip, 3 its blade's start) of attachment `kind` on the gun
// drawn with `model`, from the gun's drawn muzzle (view::weaponLocalGeometry), in the gun's axes as drawn (frame 0, unturned:
// x forward, y left, z up; the whole model transform in it, its Offset too). False if it doesn't fit.
[[nodiscard]] bool fromMuzzle(qmodel_t* model, int kind, int what, glm::vec3& out);

// Client: the attachment the weapon with id `uid` has (QVR_SVC_WEAPONATTACH), None if not known.
[[nodiscard]] int kindOfUid(int uid);
void parseEvent();          // QVR_SVC_WEAPONATTACH
void forgetUid(int uid);    // QVR_SVC_WEAPONGONE
void clientClear();         // a new server
void registerCommands();    // vr_attach_mdl_dump

// Server: the attachment of the weapon record `rec` (QC vr_weaponinst.qc), and of the record `ent`'s field `refOfs`
// refers to (a hand's .weaponinst, a holster's, a prop's).
[[nodiscard]] int kindOfRecord(edict_t* rec);
[[nodiscard]] int kindOfRef(edict_t* ent, int refOfs);
void serverFrame();               // the records whose attachment changed, to every client (reliable)
void serverWriteAll(sizebuf_t* msg); // with the spawn state: every record's that has one

} // namespace qvr::attach
