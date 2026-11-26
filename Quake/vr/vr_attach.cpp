// vr_attach.cpp -- weapon attachments: the kinds, the weapons' mounts, the clients told which weapon has which (vr_attach.hpp;
// docs/vr-port/ATTACHMENTS.md).

#include "vr_attach.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
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
    {"none", "", glm::vec3{0.f}, glm::vec3{0.f}},
    {"bayonet", "progs/bayonet.mdl", glm::vec3{6.5f, 0.f, -0.82f}, glm::vec3{1.2f, 0.f, -0.97f}},
};

// Each kind's Weapon Offsets keys (attN_*): fits, x, y, z, pitch, yaw, roll, scale.
struct MountKeys
{
    Key ok, x, y, z, pitch, yaw, roll, scale;
};
constexpr MountKeys mountKeys[Count] = {
    {Key::Att1Ok, Key::Att1X, Key::Att1Y, Key::Att1Z, Key::Att1Pitch, Key::Att1Yaw, Key::Att1Roll, Key::Att1Scale},
    {Key::Att1Ok, Key::Att1X, Key::Att1Y, Key::Att1Z, Key::Att1Pitch, Key::Att1Yaw, Key::Att1Roll, Key::Att1Scale},
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
    m.scale = weapons::value(slot, k.scale);
    if(m.scale <= 0.f)
    {
        m.scale = 1.f;
    }
    return m;
}

glm::vec3 onGun(const Mount& m, const glm::vec3& p)
{
    return m.pos + view::mountBasis(m.angles) * (p * m.scale);
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
    out = view::modelPoint(ve, onGun(m, what == 2 ? k->tip : glm::vec3{0.f})) - muzzle;
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
