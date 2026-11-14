// vr_mpavatar.cpp -- other players' VR state (QVR_SVC_AVATAR; vr_mpavatar.hpp): kept, drawn, reported.

#include "vr_mpavatar.hpp"

#include "vr_cvars.hpp"
#include "vr_handrig.hpp"
#include "vr_move.hpp"
#include "vr_render.hpp"
#include "vr_server.hpp"
#include "vr_view.hpp"

namespace qvr::mpavatar
{

namespace
{

Avatar avatars[MAX_SCOREBOARD + 1]; // by player entity (0 unused)

// What is drawn of each avatar (addEntities): its hands (the jointed hand, posed by the curls it came with) and weapons.
struct Drawn
{
    view::ViewEntity hand[2];
    view::ViewEntity weapon[2];
    float skin[2][handrig::data::numJoints * 12]{};
    int frame{-1}; // host_framecount it was last set up
};
struct DrawnAll
{
    Drawn of[MAX_SCOREBOARD + 1];
    int addedFrame{-1};
};
DrawnAll drawn;

// How long an avatar is drawn after its last message (an unchanged one comes every vr_net_avatar_keyframe frames).
constexpr double staleAfter = 1.0;

glm::vec3 readEighths()
{
    glm::vec3 v{0.f};
    for(int i = 0; i < 3; i++)
    {
        v[i] = static_cast<float>(MSG_ReadShort()) / 8.f;
    }
    return v;
}

glm::vec3 readAngles16()
{
    glm::vec3 a{0.f};
    for(int i = 0; i < 3; i++)
    {
        a[i] = MSG_ReadAngle16(cl.protocolflags);
    }
    return a;
}

glm::vec3 entityOrigin(int ent)
{
    return ent > 0 && ent < cl.num_entities ? glm::vec3{cl_entities[ent].msg_origins[0][0], cl_entities[ent].msg_origins[0][1],
                                                  cl_entities[ent].msg_origins[0][2]}
                                            : glm::vec3{0.f};
}

void printVec(const char* name, const glm::vec3& v)
{
    Con_Printf(" %s=%.1f %.1f %.1f", name, v.x, v.y, v.z);
}

void printCurls(const char* name, const VrDrawnHand& h)
{
    Con_Printf(" %s=", name);
    for(const auto& finger : h.curl)
    {
        for(const float c : finger)
        {
            Con_Printf("%.3f ", c);
        }
    }
    Con_Printf("%.3f %.3f %.3f", h.thumb.x, h.thumb.y, h.thumb.z);
}

// One line per avatar (the harness's mp_avatar): head and hands in the world (from his entity's origin now), the main
// hand's angles, the curls and thumbs, the weapons' models; then what was drawn of it last frame.
void printAvatar(int e, const Avatar& a)
{
    const glm::vec3 origin = entityOrigin(e);
    Con_Printf("mp_avatar: ent=%d self=%d flat=%d", e, e == cl.viewentity ? 1 : 0, a.flat ? 1 : 0);
    printVec("head", origin + a.head);
    printVec("hand0", origin + a.hands[0].handOrigin);
    printVec("hand1", origin + a.hands[1].handOrigin);
    printVec("rot1", a.hands[1].handAngles);
    printCurls("curl0", a.hands[0]);
    printCurls("curl1", a.hands[1]);
    Con_Printf(" flags=%d %d wmdl=%d %d wpn=%d %d age=%.0f\n", a.hands[0].flags, a.hands[1].flags, a.hands[0].weaponModel,
        a.hands[1].weaponModel, a.weapon, a.weapon2, (realtime - a.received) * 1000.0);
    const Drawn& d = drawn.of[e];
    for(int side = 0; side < 2; side++)
    {
        for(const view::ViewEntity* ve : {&d.hand[side], &d.weapon[side]})
        {
            if(d.frame == host_framecount - 1 || d.frame == host_framecount)
            {
                if(ve->visible && ve->ent.model)
                {
                    Con_Printf("mp_avatar_drawn: ent=%d side=%d kind=%s model=%s mirrored=%d at=%.1f %.1f %.1f\n", e, side,
                        ve == &d.hand[side] ? "hand" : "weapon", ve->ent.model->name, ve->mirrored ? 1 : 0, ve->ent.origin[0],
                        ve->ent.origin[1], ve->ent.origin[2]);
                }
            }
        }
    }
}

void avatars_f()
{
    if(cls.state == ca_connected && cl.viewentity > 0)
    {
        for(int e = 1; e <= MAX_SCOREBOARD; e++)
        {
            if(avatars[e].valid)
            {
                printAvatar(e, avatars[e]);
            }
        }
        // This client's own hands as drawn (what the others should draw: the move's view::netHands).
        VrDrawnHand own[2];
        view::netHands(own);
        Con_Printf("mp_avatar_own: ent=%d", cl.viewentity);
        printVec("hand0", own[0].handOrigin);
        printVec("hand1", own[1].handOrigin);
        printVec("weapon1", own[1].weaponOrigin);
        Con_Printf(" flags=%d %d wmdl=%d %d\n", own[0].flags, own[1].flags, own[0].weaponModel, own[1].weaponModel);
    }
    if(sv.active)
    {
        PR_SwitchQCVM(&sv.qcvm); // (server::clientMove: the edicts' numbers)
        for(int i = 0; i < svs.maxclients; i++)
        {
            const client_t& c = svs.clients[i];
            const VrMove* m = c.active && c.spawned && c.edict ? server::clientMove(c.edict) : nullptr;
            if(!m)
            {
                continue;
            }
            // (as the avatar places them: the drawn hands from the origin his client drew them at, on his entity now)
            const glm::vec3 origin{c.edict->v.origin[0], c.edict->v.origin[1], c.edict->v.origin[2]};
            Con_Printf("mp_avatar_sv: ent=%d", i + 1);
            printVec("head", m->headPos);
            printVec("hand0", origin + m->drawn[0].handOrigin - m->origin);
            printVec("hand1", origin + m->drawn[1].handOrigin - m->origin);
            printVec("rot1", m->drawn[1].handAngles);
            VrDrawnHand q[2] = {m->drawn[0], m->drawn[1]};
            for(VrDrawnHand& h : q)
            {
                for(auto& finger : h.curl)
                {
                    for(float& cv : finger)
                    {
                        cv = quantizedCurl(cv); // (as sent)
                    }
                }
            }
            printCurls("curl0", q[0]);
            printCurls("curl1", q[1]);
            Con_Printf(" flags=%d %d wmdl=%d %d wpn=%d\n", m->drawn[0].flags, m->drawn[1].flags, m->drawn[0].weaponModel,
                m->drawn[1].weaponModel, static_cast<int>(c.edict->v.weapon));
        }
        int sent = 0, skipped = 0;
        long long bytes = 0;
        server::avatarCounts(sent, skipped, bytes);
        Con_Printf("mp_avatar_sent: sent=%d skipped=%d bytes=%lld\n", sent, skipped, bytes);
        PR_SwitchQCVM(nullptr);
    }
    Con_Printf("mp_avatars: done\n");
}

void placeEntity(view::ViewEntity& ve, qmodel_t* model, const glm::vec3& origin, const glm::vec3& angles, int frame, int skin,
    bool mirrored)
{
    entity_t& e = ve.ent;
    if(model != ve.lastModel)
    {
        e.lerpflags |= LERP_RESETANIM;
        ve.lastModel = model;
    }
    e.model = model;
    e.frame = frame;
    e.skinnum = skin;
    e.colormap = vid.colormap;
    e.scale = ENTSCALE_DEFAULT;
    e.alpha = ENTALPHA_DEFAULT;
    for(int i = 0; i < 3; i++)
    {
        e.origin[i] = origin[i];
        e.angles[i] = angles[i];
    }
    ve.mirrored = mirrored;
    ve.scale = glm::vec3{1.f};
    ve.visible = model != nullptr;
}

void add(view::ViewEntity& ve)
{
    if(ve.visible && ve.ent.model && cl_numvisedicts < MAX_VISEDICTS)
    {
        cl_visedicts[cl_numvisedicts++] = &ve.ent;
    }
}

// Avatar `e`'s hands and weapons placed on his entity as drawn now (its lerp: they ride it between messages).
void setup(int e, const Avatar& a)
{
    Drawn& d = drawn.of[e];
    d.frame = host_framecount;
    glm::vec3 origin, angles;
    render::drawnPlace(cl_entities[e], origin, angles);
    for(int side = 0; side < 2; side++)
    {
        const VrDrawnHand& h = a.hands[side];
        d.hand[side].visible = false;
        d.weapon[side].visible = false;
        if(qmodel_t* rig = view::remoteHandSkin(side, h, d.skin[side]))
        {
            placeEntity(d.hand[side], rig, origin + h.handOrigin, h.handAngles, 0, 0, (h.flags & VrDrawnHand::handMirrorBit) != 0);
        }
        if((h.flags & VrDrawnHand::weaponBit) && h.weaponModel > 0 && h.weaponModel < MAX_MODELS)
        {
            qmodel_t* model = cl.model_precache[h.weaponModel];
            if(model && model->type == mod_alias)
            {
                placeEntity(d.weapon[side], model, origin + h.weaponOrigin, h.weaponAngles,
                    za::min(static_cast<int>(h.weaponFrame), model->numframes - 1), h.weaponSkin,
                    (h.flags & VrDrawnHand::weaponMirrorBit) != 0);
            }
        }
    }
}

[[nodiscard]] const Drawn* drawnOf(const entity_t* e, int& side, bool& hand)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&drawn.of[0]);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(drawn.of))
    {
        return nullptr;
    }
    const Drawn& d = drawn.of[(p - first) / sizeof(Drawn)];
    for(side = 0; side < 2; side++)
    {
        if(e == &d.hand[side].ent || e == &d.weapon[side].ent)
        {
            hand = e == &d.hand[side].ent;
            return &d;
        }
    }
    return nullptr;
}

} // namespace

void parse()
{
    const int ent = MSG_ReadShort();
    Avatar a;
    a.valid = true;
    a.received = realtime;
    a.flat = (MSG_ReadByte() & 1) != 0;
    a.head = readEighths();
    a.headAngles = readAngles16();
    for(VrDrawnHand& h : a.hands)
    {
        h = readDrawnHand(glm::vec3{0.f}); // (offsets from his entity's origin)
    }
    a.weapon = MSG_ReadLong();
    a.weapon2 = MSG_ReadLong();
    if(ent > 0 && ent <= MAX_SCOREBOARD && ent != cl.viewentity)
    {
        avatars[ent] = a;
    }
}

const Avatar* of(int ent)
{
    return ent > 0 && ent <= MAX_SCOREBOARD && avatars[ent].valid ? &avatars[ent] : nullptr;
}

void addEntities()
{
    if(drawn.addedFrame == host_framecount)
    {
        return; // (the screen drawn again in a frame: added once)
    }
    drawn.addedFrame = host_framecount;
    if(cls.state != ca_connected || cl.maxclients <= 1 || cl.intermission || vr_mp_avatar_draw.value == 0.f)
    {
        return;
    }
    for(int e = 1; e <= MAX_SCOREBOARD && e < cl.num_entities; e++)
    {
        const Avatar& a = avatars[e];
        // (his entity in the last message: in sight, alive or not; a stale avatar is not drawn)
        if(!a.valid || e == cl.viewentity || realtime - a.received > staleAfter || !cl_entities[e].model ||
            cl_entities[e].msgtime != cl.mtime[0])
        {
            continue;
        }
        setup(e, a);
        Drawn& d = drawn.of[e];
        for(int side = 0; side < 2; side++)
        {
            add(d.weapon[side]);
            add(d.hand[side]);
        }
    }
}

int bonePoses(const entity_t* e, const float** matrices)
{
    int side = 0;
    bool hand = false;
    const Drawn* d = drawnOf(e, side, hand);
    if(!d || !hand)
    {
        return 0;
    }
    if(matrices)
    {
        *matrices = d->skin[side];
    }
    return handrig::data::numJoints;
}

bool mirrored(const entity_t* e)
{
    int side = 0;
    bool hand = false;
    const Drawn* d = drawnOf(e, side, hand);
    return d && (hand ? d->hand[side].mirrored : d->weapon[side].mirrored);
}

void clear()
{
    for(Avatar& a : avatars)
    {
        a = Avatar{};
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_avatars", avatars_f);
}

} // namespace qvr::mpavatar

extern "C" void VR_AddMpAvatars(void)
{
    qvr::mpavatar::addEntities();
}
