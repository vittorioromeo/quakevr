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

// A placement as Weapon Offsets keep one (mountoffs): a place, a turn (pitch up, yaw left, roll: view::mountBasis) and a
// scale, in its parent's space.
struct MountValues
{
    glm::vec3 pos{0.f};
    glm::vec3 angles{0.f};
    float scale{1.f};
};
// The gun's bayonet mount (mountoffs, his 2026-11-28: "the bayonet/dagger should be positioned where the mount is, and
// then any bayonet offset should be applied on top of that"): its Weapon Offsets mnt_* keys, in the gun model's own space
// (frame 0), its scale the bayonet's size on the gun (mnt_size). Every attachment hangs from it: gun -> this mount -> every
// gun's offset of the kind (globalOffset: vr_attach_<kind>_*, in the mount's units and axes) -> the gun's own (attN_*,
// in the same units and axes) -> the kind's frame. `slot` -1: none (zeros).
[[nodiscard]] MountValues mountValues(int slot);
[[nodiscard]] MountValues globalOffset(int kind);
// A gun's values from before mountoffs (vr_wofs_version 81: att1_*/att2_* in the gun's space, mnt_* the drawn mount's move
// from the bayonet's place in its units) as today's (mnt_* the mount on the gun, attN_* from it past the global offsets at
// their defaults): the same bayonet and dagger, except that both now sit where an old moved mount put the mount.
void convertToMountOffsets(MountValues& att1, MountValues& att2, MountValues& mnt);
// The dagger's old (gun-space) default from today's: `att2` (today's att2_* defaults) put on `mnt` past the dagger's global
// offset at its default (vr_wofs_version 81).
void composeMountDefaults(const MountValues& mnt, MountValues& att2);

// A weapon's mount for an attachment, in the gun model's own space (frame 0): its mount (mountValues), the kind's global
// offset and its own Weapon Offsets keys attN_* (fits: attN_ok), composed.
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
// at the gun's mount (mountValues). `model`: the gun's (a weapon slot's, or a map pickup's: pickupRails in vr_attach.cpp,
// the view gun's mount mapped onto the pickup).
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
