// vr_attach.cpp -- weapon attachments: the kinds, the weapons' mounts, the clients told which weapon has which (vr_attach.hpp;
// docs/vr-port/ATTACHMENTS.md).

#include "vr_attach.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_mdldequant.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/Strcmp.hpp"
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

// The map pickups' bayonet mounts (bayomounts; ATTACHMENTS.md "The bayonet", make_bayonet.py MOUNTS): the pickup model, the
// weapon slot's model whose mount (mnt_*) it shares, the mount in the pickup's own space (level, along its +x) and its
// scale (the view model's bayonet to its gun, kept in the pickup's proportion), and the view gun's mount they were measured
// against (mountoffs: its place and size then, the old att1_*): the pickup draws the view gun's mount, wherever it is now,
// mapped by them (pos + (scale / refScale) * (mount - refPos)).
struct PickupRail
{
    const char* model;
    const char* gun;
    glm::vec3 pos;
    float scale;
    glm::vec3 refPos;
    float refScale;
};
constexpr PickupRail pickupRails[] = {
    {"progs/g_shot0.mdl", "progs/v_shot.mdl", {18.2f, 0.f, 33.79f}, 2.3048f, {32.9f, 0.f, 2.84f}, 2.2727f},
    {"progs/g_shot.mdl", "progs/v_shot2.mdl", {21.6f, 0.f, 34.86f}, 2.9398f, {26.f, 0.f, 3.62f}, 2.0833f},
    // bayomounts (scratch bm_auto.py: the underside at the front on the barrel's line; the scale the view model's bayonet
    // to its gun, as above)
    {"progs/g_nail.mdl", "progs/v_nail.mdl", {13.33f, -3.f, 34.24f}, 2.1315f, {20.f, -1.48f, 2.14f}, 2.083f},
    {"progs/g_rock.mdl", "progs/v_rock.mdl", {17.3f, 0.f, 31.21f}, 2.107f, {29.2f, 0.f, 0.55f}, 2.632f},
    {"progs/g_prox.mdl", "progs/v_prox.mdl", {17.4f, 0.f, 27.97f}, 2.1107f, {29.2f, 0.f, 0.58f}, 2.632f},
    {"progs/g_rock2.mdl", "progs/v_rock2.mdl", {21.9f, 0.f, 28.39f}, 2.249f, {50.6f, 0.f, -1.95f}, 2.941f},
    {"progs/g_light.mdl", "progs/v_light.mdl", {20.3f, 0.f, 34.63f}, 3.0316f, {33.4f, 0.f, 7.55f}, 2.857f},
};

// The global attachment offsets from the mount (mountoffs: vr_attach_<kind>_*, Weapon Offsets > Attachments).
struct GlobalCvars
{
    cvar_t *x, *y, *z, *pitch, *yaw, *roll, *scale;
};
const GlobalCvars globals[Count] = {
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {&vr_attach_bayonet_x, &vr_attach_bayonet_y, &vr_attach_bayonet_z, &vr_attach_bayonet_pitch, &vr_attach_bayonet_yaw,
        &vr_attach_bayonet_roll, &vr_attach_bayonet_scale},
    {&vr_attach_dagger_x, &vr_attach_dagger_y, &vr_attach_dagger_z, &vr_attach_dagger_pitch, &vr_attach_dagger_yaw,
        &vr_attach_dagger_roll, &vr_attach_dagger_scale},
};

// A placement (mountoffs): a point p of the child's space is at pos + basis * (scale * p) in the parent's.
struct Place
{
    glm::vec3 pos{0.f};
    glm::mat3 basis{1.f};
    float scale{1.f};
};
[[nodiscard]] Place then(const Place& parent, const Place& child)
{
    return Place{parent.pos + parent.basis * (child.pos * parent.scale), parent.basis * child.basis, parent.scale * child.scale};
}
[[nodiscard]] Place inverse(const Place& p)
{
    const glm::mat3 t = glm::transpose(p.basis);
    return Place{-(t * p.pos) / p.scale, t, 1.f / p.scale};
}
[[nodiscard]] Place placeOf(const MountValues& v)
{
    return Place{v.pos, view::mountBasis(v.angles), v.scale > 0.f ? v.scale : 1.f};
}
[[nodiscard]] MountValues valuesOf(const Place& p)
{
    return MountValues{p.pos, hands::anglesFromVectors(glm::normalize(p.basis[0]), glm::normalize(p.basis[2])), p.scale};
}
[[nodiscard]] MountValues slotValues(int slot, Key x, Key y, Key z, Key pitch, Key yaw, Key roll, Key scale)
{
    return MountValues{weapons::vec(slot, x, y, z), weapons::vec(slot, pitch, yaw, roll), weapons::value(slot, scale)};
}
[[nodiscard]] MountValues globalValues(int kind, bool defaults)
{
    const GlobalCvars& g = globals[kind];
    const auto v = [defaults](const cvar_t* c) { return defaults ? static_cast<float>(Q_atof(c->default_string)) : c->value; };
    return MountValues{glm::vec3{v(g.x), v(g.y), v(g.z)}, glm::vec3{v(g.pitch), v(g.yaw), v(g.roll)}, v(g.scale)};
}

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
    // (mountoffs) the gun's mount, then every gun's offset of this kind from it, then this gun's own
    const MountKeys& k = mountKeys[kind];
    m.fits = weapons::value(slot, k.ok) != 0.f;
    const Place own = placeOf(slotValues(slot, k.x, k.y, k.z, k.pitch, k.yaw, k.roll, k.scale));
    const Place p = then(then(placeOf(mountValues(slot)), placeOf(globalValues(kind, false))), own);
    m.pos = p.pos;
    m.angles = valuesOf(p).angles;
    m.basis = p.basis * kindFrame(*info(kind));
    m.scale = p.scale;
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

int railSlot(const qmodel_t* model)
{
    if(!model)
    {
        return -1;
    }
    for(const PickupRail& p : pickupRails)
    {
        if(!ZA_STRCMP(model->name, p.model))
        {
            return weapons::slotForName(p.gun);
        }
    }
    return weapons::slotForModel(model);
}

bool railMount(const qmodel_t* model, Mount& out)
{
    out = Mount{};
    if(!model || vr_attach_mount_show.value == 0.f)
    {
        return false;
    }
    const int slot = railSlot(model);
    if(slot < 0 || weapons::value(slot, Key::Att1Ok) == 0.f)
    {
        return false;
    }
    const MountValues v = mountValues(slot);
    Place p = placeOf(v);
    for(const PickupRail& r : pickupRails)
    {
        if(!ZA_STRCMP(model->name, r.model))
        {
            p = then(Place{r.pos - r.refPos * (r.scale / r.refScale), glm::mat3{1.f}, r.scale / r.refScale}, p);
        }
    }
    out.fits = true;
    out.pos = p.pos;
    out.angles = v.angles;
    out.basis = p.basis; // (the bayonet's frame is its model's axes: kindFrame is 1)
    out.scale = p.scale;
    out.own = 1.f;
    return true;
}

MountValues mountValues(int slot)
{
    if(slot < 0)
    {
        return MountValues{};
    }
    MountValues v = slotValues(slot, Key::MntX, Key::MntY, Key::MntZ, Key::MntPitch, Key::MntYaw, Key::MntRoll, Key::MntSize);
    v.scale = v.scale > 0.f ? v.scale : 1.f;
    return v;
}

MountValues globalOffset(int kind)
{
    return info(kind) ? globalValues(kind, false) : MountValues{};
}

void convertToMountOffsets(MountValues& att1, MountValues& att2, MountValues& mnt)
{
    const Place a1 = placeOf(att1), a2 = placeOf(att2), old = placeOf(mnt);
    // The old mount was the bayonet's place moved: the new one is where that put it (the bayonet's place, then the old
    // mount's move). The bayonet and the dagger keep their sizes (unscaled by an old Mount Size); the dagger keeps its place
    // from the bayonet's place (so a moved mount takes it along); every gun's offsets at their defaults (an old config has
    // none of its own) taken out.
    const Place unsize{glm::vec3{0.f}, glm::mat3{1.f}, 1.f / old.scale};
    mnt = valuesOf(then(a1, old));
    att1 = valuesOf(then(inverse(placeOf(globalValues(Bayonet, true))), unsize));
    att2 = valuesOf(then(then(inverse(placeOf(globalValues(Dagger, true))), unsize), then(inverse(a1), a2)));
}

void composeMountDefaults(const MountValues& mnt, MountValues& att2)
{
    att2 = valuesOf(then(then(placeOf(mnt), placeOf(globalValues(Dagger, true))), placeOf(att2)));
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

namespace
{
// vr_attach_mdl_dump <model> <file>: an alias model's frame 0 as the engine has it (made models too: AD's "#vr" guns), as
// an OBJ (v x y z, f a b c): bayomounts measured the guns' mounts from these (scratch bm_measure.py).
void mdlDump_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_attach_mdl_dump <model> <file>\n");
        return;
    }
    qmodel_t* const model = Mod_ForName(Cmd_Argv(1), false);
    if(!model || model->type != mod_alias)
    {
        Con_Printf("mdl_dump: %s: no alias model\n", Cmd_Argv(1));
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || hdr->numframes <= 0 || hdr->numverts <= 0)
    {
        Con_Printf("mdl_dump: %s: not a Quake pose model\n", Cmd_Argv(1));
        return;
    }
    FILE* f = fopen(Cmd_Argv(2), "w");
    if(!f)
    {
        Con_Printf("mdl_dump: can't write %s\n", Cmd_Argv(2));
        return;
    }
    mdldequant::prepare(model);
    const int pose = hdr->frames[0].firstpose;
    const glm::vec3 so{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    const glm::vec3 hs{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    for(int i = 0; i < hdr->numverts; i++)
    {
        const glm::vec3 p = so + hs * mdldequant::poseVertex(hdr, pose, i);
        fprintf(f, "v %.4f %.4f %.4f\n", static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z));
    }
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    for(int t = 0; t + 2 < hdr->numindexes; t += 3)
    {
        fprintf(f, "f %d %d %d\n", desc[indexes[t]].vertindex + 1, desc[indexes[t + 1]].vertindex + 1,
            desc[indexes[t + 2]].vertindex + 1);
    }
    fclose(f);
    Con_Printf("mdl_dump: %s: %d vertices -> %s\n", Cmd_Argv(1), hdr->numverts, Cmd_Argv(2));
}
} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_attach_mdl_dump", mdlDump_f);
}

} // namespace qvr::attach
