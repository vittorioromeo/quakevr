// vr_attach.cpp -- weapon attachments: the kinds, the weapons' mounts, the clients told which weapon has which (vr_attach.hpp;
// docs/vr-port/ATTACHMENTS.md).

#include "vr_attach.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::attach
{
namespace
{
using weapons::Key;

// The kinds (the table every system reads): the bayonet's points from ATTACHMENTS.md, "On its own" (its tip, its blade's
// root; its origin is where it mounts under a barrel).
constexpr KindInfo kinds[Count] = {
    {"none", "", glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f}},
    // (made along +x, its edge down, its socket across +y: its model's axes are its mount frame)
    {"bayonet", "progs/bayonet.mdl", glm::vec3{6.5f, 0.f, -0.82f}, glm::vec3{1.2f, 0.f, -0.97f}, glm::vec3{1.f, 0.f, 0.f},
        glm::vec3{0.f, 1.f, 0.f}},
    // The dagger (daggerattach): its view model as held, its tip and its blade's root its weapons.txt muzzle_av and
    // wpnbtn_av (slot 51); its origin by its grip. Its frame (flatbayonet): forward its vertices' long axis (handle to tip,
    // in the model's x-z plane: the tip 0.43 off it, the muzzle_av line runs 13 degrees off the handle), across its guard
    // (square to it in that plane); up is the model's -y (scratch fb_frame.py measured it).
    {"dagger", "progs/v_dagger.mdl", glm::vec3{12.5665f, 0.0082f, 30.3419f}, glm::vec3{7.7205f, 0.0082f, 10.2118f},
        glm::vec3{0.35418f, 0.f, 0.93518f}, glm::vec3{-0.93518f, 0.f, 0.35418f}},
};

// The kind's frame: its model's axes in the mount frame (rows forward, across, up).
[[nodiscard]] glm::mat3 kindFrame(const KindInfo& k)
{
    const glm::vec3 f = glm::normalize(k.forward);
    const glm::vec3 a = glm::normalize(k.across - f * glm::dot(k.across, f));
    return glm::transpose(glm::mat3{f, a, glm::cross(f, a)});
}

// Each kind's Weapon Offsets keys (attN_*): fits, x, y, z, pitch, yaw, roll, scale.
struct MountKeys
{
    Key ok, x, y, z, pitch, yaw, roll, scale;
};
constexpr MountKeys mountKeys[Count] = {
    {Key::Att1Ok, Key::Att1X, Key::Att1Y, Key::Att1Z, Key::Att1Pitch, Key::Att1Yaw, Key::Att1Roll, Key::Att1Scale},
    {Key::Att1Ok, Key::Att1X, Key::Att1Y, Key::Att1Z, Key::Att1Pitch, Key::Att1Yaw, Key::Att1Roll, Key::Att1Scale},
    {Key::Att2Ok, Key::Att2X, Key::Att2Y, Key::Att2Z, Key::Att2Pitch, Key::Att2Yaw, Key::Att2Roll, Key::Att2Scale},
};

// The client's: which weapon (by its id) has which attachment.
struct Known
{
    int uid{0};
    int kind{0};
};
za::Vector<Known> known;

// The server's: per edict, what the clients were last told of the record there.
struct Sent
{
    int uid{0};
    int kind{0};
};
za::Vector<Sent> sent;

void writeKind(sizebuf_t* msg, int uid, int kind)
{
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_WEAPONATTACH);
    MSG_WriteLong(msg, uid);
    MSG_WriteByte(msg, kind);
}

void setKnown(int uid, int kind)
{
    for(za::SizeT i = 0; i < known.size(); i++)
    {
        if(known[i].uid == uid)
        {
            if(kind == None)
            {
                known[i] = known.back();
                known.popBack();
            }
            else
            {
                known[i].kind = kind;
            }
            return;
        }
    }
    if(kind != None)
    {
        known.pushBack(Known{uid, kind});
    }
}
} // namespace

const KindInfo* info(int kind)
{
    return kind > None && kind < Count ? &kinds[kind] : nullptr;
}

bool strikes(int kind)
{
    const KindInfo* const k = info(kind);
    return k && glm::length(k->tip) > 1e-3f;
}

Mount mount(int slot, int kind)
{
    Mount m;
    if(slot < 0 || !info(kind))
    {
        return m;
    }
    const MountKeys& k = mountKeys[kind];
    m.fits = weapons::value(slot, k.ok) != 0.f;
    m.pos = weapons::vec(slot, k.x, k.y, k.z);
    m.angles = weapons::vec(slot, k.pitch, k.yaw, k.roll);
    m.basis = view::mountBasis(m.angles) * kindFrame(*info(kind));
    m.scale = weapons::value(slot, k.scale);
    if(m.scale <= 0.f)
    {
        m.scale = 1.f;
    }
    // (a weapon model as an attachment, the dagger: drawn with its own Scale, weapons::modelTransform's; its world size,
    // modelScale, is the gun's: view::placeAttachment takes it out)
    const int ownSlot = weapons::slotForName(info(kind)->model);
    m.own = ownSlot >= 0 ? weapons::value(ownSlot, Key::Scale) : 1.f;
    if(m.own <= 0.f)
    {
        m.own = 1.f;
    }
    return m;
}

glm::vec3 onGun(const Mount& m, const glm::vec3& p)
{
    return m.pos + m.basis * (p * (m.scale * m.own));
}

// The dagger's mounts before its frame (vr_wofs_version 80, weapons.cpp): turned by its angles alone, its model's own axes.
// The same look in the frame, rolled a quarter turn about its long axis (the guard level: his flat note 2026-11-27, it was
// upright and cut into the muzzle), its tip where it was: the frame's angles and the mount's place.
void convertOldMount(int kind, glm::vec3& pos, glm::vec3& angles, float size)
{
    const KindInfo* const k = info(kind);
    if(!k)
    {
        return;
    }
    const glm::mat3 old = view::mountBasis(angles);
    const glm::vec3 f = glm::normalize(k->forward);
    // (a quarter turn about f: v -> f (f.v) + f x v)
    const glm::mat3 quarter{glm::vec3{f.x * f.x, f.x * f.y + f.z, f.x * f.z - f.y},
        glm::vec3{f.y * f.x - f.z, f.y * f.y, f.y * f.z + f.x}, glm::vec3{f.z * f.x + f.y, f.z * f.y - f.x, f.z * f.z}};
    const glm::mat3 rolled = old * quarter;
    const glm::mat3 turn = rolled * glm::transpose(kindFrame(*k)); // (mountBasis of the new angles)
    angles = hands::anglesFromVectors(glm::normalize(turn[0]), glm::normalize(turn[2]));
    pos += (old - rolled) * (k->tip * size);
}

bool fromMuzzle(qmodel_t* model, int kind, int what, glm::vec3& out)
{
    out = glm::vec3{0.f};
    const int slot = model ? weapons::slotForModel(model) : -1;
    const Mount m = mount(slot, kind);
    const KindInfo* const k = info(kind);
    glm::vec3 muzzle{0.f}, butt{0.f}, second{0.f};
    int secondType = 0;
    if(!m.fits || !k || !view::weaponLocalGeometry(model, muzzle, butt, second, secondType))
    {
        return false;
    }
    // (drawn as weaponLocalGeometry draws it: at the origin unturned, frame 0, its default scale)
    view::ViewEntity ve;
    ve.ent.model = model;
    ve.ent.scale = ENTSCALE_DEFAULT;
    ve.zeroBlend = 1.f;
    out = view::modelPoint(ve, onGun(m, what == 2 ? k->tip : what == 3 ? k->base : glm::vec3{0.f})) - muzzle;
    return true;
}

int kindOfUid(int uid)
{
    if(uid <= 0)
    {
        return None;
    }
    for(const Known& k : known)
    {
        if(k.uid == uid)
        {
            return k.kind;
        }
    }
    return None;
}

void parseEvent()
{
    const int uid = MSG_ReadLong();
    const int kind = MSG_ReadByte();
    if(uid < 0)
    {
        known.clear();
        return;
    }
    setKnown(uid, kind < Count ? kind : None);
}

void forgetUid(int uid)
{
    setKnown(uid, None);
}

void clientClear()
{
    known.clear();
}

int kindOfRecord(edict_t* rec)
{
    const progs::FieldOffsets& f = progs::fields();
    if(!rec || rec->free || f.wi_attach < 0)
    {
        return None;
    }
    const int kind = static_cast<int>(progs::fieldFloat(rec, f.wi_attach));
    return kind > None && kind < Count ? kind : None;
}

int kindOfRef(edict_t* ent, int refOfs)
{
    if(!ent || refOfs < 0 || !progs::bindings().isVrProgs)
    {
        return None;
    }
    const int v = progs::fieldInt(ent, refOfs);
    if(v <= 0)
    {
        return None;
    }
    return kindOfRecord(PROG_TO_EDICT(v));
}

void serverFrame()
{
    const progs::FieldOffsets& f = progs::fields();
    if(!sv.active || f.wi_attach < 0 || f.wi_uid < 0 || !progs::bindings().isVrProgs)
    {
        return;
    }
    const int n = sv.qcvm.num_edicts;
    if(static_cast<int>(sent.size()) < n)
    {
        sent.resize(static_cast<za::SizeT>(n), Sent{});
    }
    for(int i = 1; i < n; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const int uid = e->free ? 0 : static_cast<int>(progs::fieldFloat(e, f.wi_uid));
        const int kind = uid > 0 ? kindOfRecord(e) : None;
        Sent& s = sent[static_cast<za::SizeT>(i)];
        if(uid > 0 && (s.uid != uid || s.kind != kind) && sv.reliable_datagram.cursize + 8 < sv.reliable_datagram.maxsize)
        {
            // (a record taking this edict over: the clients told its kind, none included, as its id is new to them)
            writeKind(&sv.reliable_datagram, uid, kind);
            s = Sent{uid, kind};
        }
        else if(uid <= 0)
        {
            s = Sent{};
        }
    }
}

void serverWriteAll(sizebuf_t* msg)
{
    // (to the spawning client alone: the others' table is serverFrame's, left as it is)
    const progs::FieldOffsets& f = progs::fields();
    if(!sv.active || f.wi_attach < 0 || f.wi_uid < 0 || !progs::bindings().isVrProgs)
    {
        return;
    }
    writeKind(msg, -1, 0); // (a fresh table)
    const int n = sv.qcvm.num_edicts;
    for(int i = 1; i < n; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const int uid = e->free ? 0 : static_cast<int>(progs::fieldFloat(e, f.wi_uid));
        if(uid <= 0)
        {
            continue;
        }
        const int kind = kindOfRecord(e);
        if(kind != None && msg->cursize + 8 < msg->maxsize)
        {
            writeKind(msg, uid, kind);
        }
    }
}

} // namespace qvr::attach
