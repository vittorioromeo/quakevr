// vr_mpavatar.cpp -- other players' VR state (QVR_SVC_AVATAR; vr_mpavatar.hpp): kept, drawn, reported.

#include "vr_mpavatar.hpp"

#include "vr_client.hpp"

#include "vr_api.h"

#include "vr_agarmor.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_climb.hpp"

#include "vr_cvars.hpp"
#include "vr_handrig.hpp"
#include "vr_hands.hpp"
#include "vr_hull.hpp"
#include "vr_move.hpp"
#include "vr_netsnap.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_render.hpp"
#include "vr_seated.hpp"
#include "vr_server.hpp"
#include "vr_view.hpp"

namespace qvr::mpavatar
{

namespace
{

Avatar avatars[MAX_SCOREBOARD + 1]; // by player entity (0 unused)
// The one before each (its msgTime: the last message it held at): drawn between the two in step with his entity's lerp
// (vr_mp_avatar_lerp, blendedOf; MP_PLAN.md, B7).
Avatar previousAvatars[MAX_SCOREBOARD + 1];
// The last ones each, by their messages' times: drawn at the interpolation buffer's time (vr_netsnap.cpp; B7b).
constexpr int avatarHistoryLen = 8;
struct AvatarHistory
{
    Avatar of[avatarHistoryLen];
    int n{0}, head{0};
};
AvatarHistory avatarHistories[MAX_SCOREBOARD + 1];

void printCalibrations(); // (vr_mp_avatars: the calibrations, below)

// What is drawn of each avatar (addEntities): its hands (the jointed hand, posed by the curls it came with) and weapons.
struct Drawn
{
    view::ViewEntity hand[2];
    view::ViewEntity weapon[2];
    float skin[2][handrig::data::numJoints * 12]{};
    int frame{-1}; // host_framecount it was last set up
    glm::vec3 placedAt{0.f}; // his entity's drawn place then (the trace's: vr_mp_trace)
    // His body (vr_mp_body), as last posed (the report's: the skeleton, the wrists its arms reached for).
    view::ViewEntity body;
    bool bodyPosed{false};
    avatar::Skeleton skeleton;
    glm::vec3 handWrist[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float eyeHeight{0.f};
    bool seated{false};
    // His holstered weapons (B6b: as he drew them, body::Holster's order) and pauldrons on his body (his look).
    view::ViewEntity holster[VrDrawnHolsters::count];
    view::ViewEntity pauldron[2];
    view::ViewEntity pauldronArm[2];
    // His armour's plates and their joints' leather on his body (B6c: the server's word on him), their pieces, their bones
    // (made as they are drawn: view::otherPlateBonePoses; sized when first shown).
    view::ViewEntity plate;
    view::ViewEntity plateBridge;
    int plateType{0};
    int platePieces{0};
    int plateSlots{0}, plateShown{0}; // as last drawn (the report's)
    za::Vector<float> plateBones;
    za::Vector<float> bridgeBones;
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

// A posed body's joints (the harness's mp_body): its scale (world units a metre), the head it was posed under and its
// head bone, the pelvis, the wrists (per hand), the ankles (per side). No newline.
void printSkeleton(const char* what, int e, const char* model, const avatar::Skeleton& k)
{
    Con_Printf("%s: ent=%d model=%s m2w=%.3f", what, e, model, k.m2w);
    printVec("head", k.head);
    printVec("headjoint", k.headJoint);
    printVec("pelvis", k.pelvis);
    printVec("wrist0", k.wrist[HAND_OFF]);
    printVec("wrist1", k.wrist[HAND_MAIN]);
    printVec("shoulder0", k.shoulder[HAND_OFF]);
    printVec("shoulder1", k.shoulder[HAND_MAIN]);
    printVec("ankle0", k.ankle[0]);
    printVec("ankle1", k.ankle[1]);
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
    if(d.bodyPosed && d.body.ent.model && (d.frame == host_framecount - 1 || d.frame == host_framecount))
    {
        printSkeleton("mp_body", e, d.body.ent.model->name, d.skeleton);
        printVec("handwrist0", d.handWrist[0]);
        printVec("handwrist1", d.handWrist[1]);
        float pitch = 0.f;
        float lying = 0.f;
        (void)avatar::otherPitch(e, pitch, lying);
        float climbWeight = 0.f;
        (void)avatar::otherClimb(e, climbWeight);
        Con_Printf(" eyeheight=%.3f yaw=%.1f moves=%d pitch=%.1f lying=%.2f climb=%.2f seated=%d\n", d.eyeHeight, a.body.yaw,
            a.moves, pitch, lying, climbWeight, d.seated ? 1 : 0);
        view::printPauldrons("mp_pauldron", e, d.pauldron);
        if(d.plate.visible && d.plate.ent.model && !d.plateBones.empty())
        {
            // His plates as last drawn: their bone slots, and how many his body's bones showed (a hidden piece's collapsed).
            Con_Printf("mp_plates: ent=%d type=%d pieces=%d skin=%d slots=%d shown=%d leather=%d model=%s\n", e, d.plateType,
                d.platePieces, d.plate.ent.skinnum, d.plateSlots, d.plateShown, d.plateBridge.visible ? 1 : 0,
                d.plate.ent.model->name);
        }
    }
    if(d.frame == host_framecount - 1 || d.frame == host_framecount)
    {
        for(int i = 0; i < VrDrawnHolsters::count; i++)
        {
            const view::ViewEntity& ve = d.holster[i];
            if(ve.visible && ve.ent.model)
            {
                Con_Printf("mp_holster: ent=%d slot=%d model=%s mirrored=%d at=%.2f %.2f %.2f\n", e, i, ve.ent.model->name,
                    ve.mirrored ? 1 : 0, ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]);
            }
        }
    }
    if(d.frame == host_framecount - 1 || d.frame == host_framecount)
    {
        const float* const skin[2] = {d.hand[0].visible ? d.skin[0] : nullptr, d.hand[1].visible ? d.skin[1] : nullptr};
        view::printHands("mp_hands", e, d.hand, skin, d.weapon, d.holster, VrDrawnHolsters::count);
        for(int part = 0; part < 3; part++)
        {
            const view::ViewEntity& ve = part < 2 ? d.hand[part] : d.body;
            if(ve.visible && ve.ent.model && (part < 2 || d.bodyPosed))
            {
                view::printLook("mp_look", e, part, &ve.ent);
            }
        }
    }
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
        // And his own body as last posed (what the others' copies of it should match).
        if(avatar::Skeleton k; avatar::skeleton(k) && avatar::posedEntity() && avatar::posedEntity()->model)
        {
            printSkeleton("mp_body_own", cl.viewentity, avatar::posedEntity()->model->name, k);
            Con_Printf("\n");
        }
        view::printOwnLooks(cl.viewentity);
        view::printOwnPauldrons(cl.viewentity);
        view::printOwnPlates(cl.viewentity);
        view::printOwnHands(cl.viewentity);
        // His own holsters as drawn (what the others draw: the move's view::netHolsters).
        VrDrawnHolsters hs;
        view::netHolsters(hs);
        for(int i = 0; i < VrDrawnHolsters::count && hs.valid; i++)
        {
            Con_Printf("mp_holster_point_own: ent=%d slot=%d at=%.2f %.2f %.2f\n", cl.viewentity, i, hs.at[i].x, hs.at[i].y,
                hs.at[i].z);
            if(hs.weapons & (1 << i))
            {
                Con_Printf("mp_holster_own: ent=%d slot=%d model=%s mirrored=%d at=%.2f %.2f %.2f\n", cl.viewentity, i,
                    cl.model_precache[hs.model[i]] ? cl.model_precache[hs.model[i]]->name : "?", (hs.mirrored >> i) & 1,
                    hs.origin[i].x, hs.origin[i].y, hs.origin[i].z);
            }
        }
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
            Con_Printf(" flags=%d %d wmdl=%d %d wpn=%d %d\n", m->drawn[0].flags, m->drawn[1].flags, m->drawn[0].weaponModel,
                m->drawn[1].weaponModel, static_cast<int>(c.edict->v.weapon),
                static_cast<int>(progs::fieldFloatOr(c.edict, progs::fields().weapon2, 0.f)));
            // (QC's holsterpos for him: B6b)
            for(int slot = 0; slot < VrDrawnHolsters::count; slot++)
            {
                if(glm::vec3 at; server::holsterPoint(c.edict, slot, at))
                {
                    Con_Printf("mp_holster_sv: ent=%d slot=%d at=%.2f %.2f %.2f\n", i + 1, slot, at.x, at.y, at.z);
                }
            }
        }
        int sent = 0, skipped = 0;
        long long bytes = 0;
        server::avatarCounts(sent, skipped, bytes);
        Con_Printf("mp_avatar_sent: sent=%d skipped=%d bytes=%lld\n", sent, skipped, bytes);
        PR_SwitchQCVM(nullptr);
    }
    printCalibrations();
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

void setupBody(int e, const Avatar& a, const glm::vec3& origin);

// Avatar `e`'s hands and weapons placed on his entity as drawn now (its lerp: they ride it between messages).
void setup(int e, const Avatar& a)
{
    Drawn& d = drawn.of[e];
    d.frame = host_framecount;
    glm::vec3 origin, angles;
    render::drawnPlace(cl_entities[e], origin, angles);
    d.placedAt = origin;
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
    // His holstered weapons where he drew them (his holsters' places, their seated layout, his Holstered poses: B6b).
    const VrDrawnHolsters& hs = a.holsters;
    for(int i = 0; i < VrDrawnHolsters::count; i++)
    {
        d.holster[i].visible = false;
        qmodel_t* model = hs.valid && (hs.weapons & (1 << i)) && hs.model[i] > 0 && hs.model[i] < MAX_MODELS
                              ? cl.model_precache[hs.model[i]]
                              : nullptr;
        if(model && model->type == mod_alias)
        {
            placeEntity(d.holster[i], model, origin + hs.origin[i], hs.angles[i], za::min(static_cast<int>(hs.frame[i]),
                model->numframes - 1), hs.skin[i], (hs.mirrored & (1 << i)) != 0);
        }
    }
    setupBody(e, a, origin);
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

// The avatar whose plates (or their joints' leather) `e` is (B6c), or nullptr.
[[nodiscard]] Drawn* platesOf(const entity_t* e)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&drawn.of[0]);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(drawn.of))
    {
        return nullptr;
    }
    Drawn& d = drawn.of[(p - first) / sizeof(Drawn)];
    return e == &d.plate.ent || e == &d.plateBridge.ent ? &d : nullptr;
}

// The calibration's fields (vr_mpavatar.hpp, Calibration): each from its cvar, times `scale`, rounded, clamped to
// [lo, hi] (on both sides: what a client sends and what the server keeps are the same numbers).
struct CalField
{
    const char* name;
    const cvar_t* var; // (null: get's)
    float scale;
    int lo;
    int hi;
    float (*get)(){nullptr};
};
const CalField calFields[numCalFields] = {
    {"height", &vr_height_calibration, 1000.f, 500, 2500}, // mm (the eyes' standing height, calibrated)
    {"seated", &vr_seated_height, 1000.f, 0, 2500},        // mm (0: not measured)
    {"arm", &vr_body_arm_length, 1000.f, 250, 2000},       // thousandths of the proportions of his height
    {"scale", &vr_world_scale, 1000.f, 250, 4000},         // thousandths: how big he is in the world
    {"hand", &vr_handedness, 1.f, 0, 2},                   // 1 right-handed, 2 left-handed, 0 custom
    {"gadget", &vr_gadget_arm, 1.f, 0, 1},                 // the wrist gadget's arm: 0 left, 1 right
    {"gloves", &vr_gloves, 1.f, 0, 2},                     // 0 none, 1 fingerless, 2 full
    {"hue", &vr_glove_hue, 1.f, 0, 360},                   // degrees
    {"sat", &vr_glove_saturation, 255.f, 0, 255},
    {"light", &vr_glove_lightness, 255.f, 0, 255},
    {"nails", &vr_nails, 1.f, 0, 1},
    {"tone", &vr_skin_tone, 100.f, -100, 100},
    {"body", &vr_body_mode, 1.f, 0, 3},
    {"build", &vr_body_build, 1.f, 0, 3},
    {"bracers", &vr_body_bracers, 1.f, 0, 1},
    {"pauldrons", &vr_body_pauldrons, 1.f, 0, 1},
    // His arms as his body poses them (Body Calibration's measurements with his tweaks: bodycal::ownArms; MP_PLAN B3c).
    {"upper", nullptr, 10.f, 0, 600, [] { return bodycal::ownArms().measuredCm[0]; }}, // mm (0: not measured)
    {"fore", nullptr, 10.f, 0, 600, [] { return bodycal::ownArms().measuredCm[1]; }},
    {"tupper", nullptr, 10.f, -500, 500, [] { return bodycal::ownArms().tweakCm[0]; }}, // mm
    {"tfore", nullptr, 10.f, -500, 500, [] { return bodycal::ownArms().tweakCm[1]; }},
    {"sback", nullptr, 1000.f, -500, 500, [] { return bodycal::ownArms().shoulderOffset.x; }}, // mm from the model's
    {"sup", nullptr, 1000.f, -500, 500, [] { return bodycal::ownArms().shoulderOffset.y; }},
    {"sout", nullptr, 1000.f, -500, 500, [] { return bodycal::ownArms().shoulderOffset.z; }},
    {"srise", nullptr, 10.f, -1800, 1800, [] { return bodycal::ownArms().rise; }}, // tenths of a degree
    {"sswing", nullptr, 10.f, -1800, 1800, [] { return bodycal::ownArms().swing; }},
    // His painted nails' colour (B6).
    {"nhue", &vr_nail_hue, 1.f, 0, 360},
    {"nsat", &vr_nail_saturation, 255.f, 0, 255},
    {"nlight", &vr_nail_lightness, 255.f, 0, 255},
    {"ngloss", &vr_nail_gloss, 255.f, 0, 255},
    // Seated play (his body posed as his own seated: no leg fit), a flat-screen player's body height, his pauldrons (B6b).
    {"sitting", nullptr, 1.f, 0, 1, [] { return seated::active() ? 1.f : 0.f; }},
    {"flatheight", &vr_flat_body_height, 1000.f, 1000, 2200}, // mm
    {"pstyle", &vr_body_pauldron_style, 1.f, 0, 2},
    {"psize", &vr_body_pauldron_size, 100.f, 25, 400},       // hundredths
    {"pfollow", &vr_body_pauldron_follow, 255.f, 0, 255},
    {"pfwd", &vr_body_pauldron_forward, 1000.f, -500, 500}, // thousandths
    {"pout", &vr_body_pauldron_out, 1000.f, -500, 500},
    {"pup", &vr_body_pauldron_up, 1000.f, -500, 500},
};
constexpr int calCommandVersion = 4; // "vr_avatarcal <version> <the fields>": another version's is ignored

struct CalState
{
    Calibration of[MAX_SCOREBOARD + 1]; // the client's: by player entity
    bool has[MAX_SCOREBOARD + 1]{};
    Calibration lastSent;               // the client's own, as last sent up
    bool sent{false};
    double nextSend{0.0};
    Calibration slot[MAX_SCOREBOARD]; // the server's: by client slot
    bool slotHas[MAX_SCOREBOARD]{};
};
CalState cal;

// calFields' places (setupBody).
constexpr int calGloves = 6, calGloveHue = 7, calGloveSat = 8, calGloveLight = 9, calNails = 10, calTone = 11, calNailHue = 25,
              calNailSat = 26, calNailLight = 27, calNailGloss = 28;
constexpr int calHeight = 0, calArm = 2, calScale = 3, calBody = 12, calBuild = 13, calBracers = 14, calPauldrons = 15,
              calUpper = 16, calTweakUpper = 18, calShoulders = 20, calRise = 23, calSwing = 24, calSitting = 29,
              calFlatHeight = 30, calPauldronStyle = 31, calPauldronSize = 32, calPauldronFollow = 33, calPauldronOffset = 34;

// Avatar `e`'s body (vr_mp_body; MP_PLAN.md, B2d): posed by the code that poses your own (avatar::poseOther), from his
// head, his drawn hands (their wrists, as your own body's arms take yours) and how his client poses his body (a.body),
// at his height, arm length and world scale, his build's model and its bracers or bare forearms (his calibration's; the
// defaults until it comes). His body mode 0 (none): his player model, as before. Both his jointed hands must be drawn.
void setupBody(int e, const Avatar& a, const glm::vec3& origin)
{
    Drawn& d = drawn.of[e];
    d.bodyPosed = false;
    const Calibration* c = cal.has[e] ? &cal.of[e] : nullptr;
    view::BodyDraw b;
    b.shown = vr_mp_body.value != 0.f && a.body.valid && !(c && c->v[calBody] == 0) && d.hand[0].visible && d.hand[1].visible;
    b.player = e;
    avatar::Subject who;
    b.who = &who;
    if(c)
    {
        who.eyeHeight = static_cast<float>(c->v[calHeight]) / 1000.f;
        who.armLength = static_cast<float>(c->v[calArm]) / 1000.f;
        who.metresToUnits = units::perMetre * static_cast<float>(c->v[calScale]) / 1000.f;
        b.build = c->v[calBuild];
        b.bare = c->v[calBracers] == 0;
        for(int k = 0; k < 2; k++)
        {
            who.arms.measuredCm[k] = static_cast<float>(c->v[calUpper + k]) / 10.f;
            who.arms.tweakCm[k] = static_cast<float>(c->v[calTweakUpper + k]) / 10.f;
        }
        who.arms.shoulderOffset = glm::vec3{static_cast<float>(c->v[calShoulders]), static_cast<float>(c->v[calShoulders + 1]),
                                      static_cast<float>(c->v[calShoulders + 2])} /
                                  1000.f;
        who.arms.rise = static_cast<float>(c->v[calRise]) / 10.f;
        who.arms.swing = static_cast<float>(c->v[calSwing]) / 10.f;
        // (B6b: seated, posed as his own seated body is; a flat-screen player at his body height, as units::eyeHeight)
        who.seated = c->v[calSitting] != 0 && !a.flat;
        if(a.flat)
        {
            who.eyeHeight = static_cast<float>(c->v[calFlatHeight]) / 1000.f;
        }
    }
    // His velocity from his entity's last two messages; on the ground unless rising or falling fast (the client is not
    // told; a walk down stairs stays on its feet).
    const entity_t& ent = cl_entities[e];
    const double dt = cl.mtime[0] - cl.mtime[1];
    const glm::vec3 moved{ent.msg_origins[0][0] - ent.msg_origins[1][0], ent.msg_origins[0][1] - ent.msg_origins[1][1],
        ent.msg_origins[0][2] - ent.msg_origins[1][2]};
    who.velocity = dt > 1e-4 && dt < 0.5 && glm::length(moved) < 64.f ? moved / static_cast<float>(dt) : glm::vec3{0.f};
    // (B5b: on the ground, lying down and in the water as the server has him -- his body lying, swimming as he does)
    who.onGround = (a.moves & avatarOnGround) != 0;
    who.lying = (a.moves & avatarLying) != 0;
    who.waterlevel = (a.moves >> avatarWaterShift) & 3;
    // (B5c: his holds, the server's -- his climbing trunk and feet)
    for(int h = 0; h < 2; h++)
    {
        who.holds[h] = (a.moves & (h ? avatarHoldMain : avatarHoldOff)) != 0;
        who.holdAt[h] = origin + a.holdAt[h];
        who.holdOut[h] = a.holdOut[h];
    }
    who.player = e;

    hands::State& s = b.s;
    s.valid = true;
    s.head = origin + a.head;
    s.headAngles = a.headAngles;
    s.playerOrigin = origin;
    s.lean = glm::vec3{a.head.x, a.head.y, 0.f};
    s.bodyYaw = a.body.yaw;
    s.headHeight = a.body.headHeight;
    s.standingHeight = a.body.standingHeight;
    s.leanHold = a.body.leanHold;
    s.crouchRatio = s.headHeight > 0.f ? za::clamp(who.eyeHeight / s.headHeight - 1.f, 0.f, 1.f) : 0.f;
    avatar::HandPose poses[2];
    if(b.shown)
    {
        view::remoteHandPose(d.hand[0], poses[HAND_OFF]);
        view::remoteHandPose(d.hand[1], poses[HAND_MAIN]);
    }
    b.poses = poses;
    // His armour's plates (B6c): the server's word on him (QVR_SVC_ARMOR, as on you), drawn as yours are (your
    // vr_player_plates: whether you see plates at all), without the morphs.
    view::BodyPlates& p = b.plates;
    p.on = vr_player_plates.value != 0.f;
    p.type = p.on ? za::max(agarmor::clientState(e, 0), 0) : 0;
    p.pieces = p.type > 0 ? za::max(agarmor::clientState(e, 1), 0) : 0;
    d.plateType = p.type;
    d.platePieces = p.pieces;
    // His pauldrons on it (his calibration's look; the armour the server says he wears), as yours are placed.
    view::PauldronLook& look = b.pauldrons;
    if(c)
    {
        look.on = c->v[calPauldrons] != 0;
        look.style = c->v[calPauldronStyle];
        look.size = static_cast<float>(c->v[calPauldronSize]) / 100.f;
        look.follow = static_cast<float>(c->v[calPauldronFollow]) / 255.f;
        look.offset = glm::vec3{static_cast<float>(c->v[calPauldronOffset]), static_cast<float>(c->v[calPauldronOffset + 1]),
                          static_cast<float>(c->v[calPauldronOffset + 2])} /
                      1000.f;
    }
    look.build = b.build;
    look.armour = a.armour;
    view::BodyEntities out;
    out.body = &d.body;
    out.plates = &d.plate;
    out.numPlates = 1;
    out.bridge = &d.plateBridge;
    out.cap = d.pauldron;
    out.arm = d.pauldronArm;
    if(!view::drawBody(b, out) || !avatar::otherSkeleton(e, d.skeleton))
    {
        return;
    }
    d.bodyPosed = true;
    d.handWrist[0] = poses[HAND_OFF].wrist;
    d.handWrist[1] = poses[HAND_MAIN].wrist;
    d.eyeHeight = who.eyeHeight;
    d.seated = who.seated;
    if((d.plate.visible || d.plateBridge.visible) && d.plateBones.empty())
    {
        d.plateBones.resize(agarmor::playerPlatesMostBones * 12, 0.f);
        d.bridgeBones.resize(agarmor::playerBridgeMostBones * 12, 0.f);
    }
}

[[nodiscard]] int calClamp(int field, int v)
{
    return za::clamp(v, calFields[field].lo, calFields[field].hi);
}

[[nodiscard]] Calibration ownCalibration()
{
    Calibration c;
    for(int i = 0; i < numCalFields; i++)
    {
        const float value = calFields[i].var ? calFields[i].var->value : calFields[i].get();
        c.v[i] = calClamp(i, static_cast<int>(std::lround(value * calFields[i].scale)));
    }
    return c;
}

void printCal(const char* what, int ent, const Calibration& c)
{
    Con_Printf("%s: ent=%d", what, ent);
    for(int i = 0; i < numCalFields; i++)
    {
        Con_Printf(" %s=%d", calFields[i].name, c.v[i]);
    }
    Con_Printf("\n");
}

[[nodiscard]] bool serverSendsCal()
{
    return sv.active && svs.maxclients > 1 && (sv.protocolflags & PRFL_QUAKEVR) != 0; // (vr_api.h)
}

void writeCal(sizebuf_t* msg, int slot)
{
    if(msg->cursize + 4 + numCalFields * 2 > msg->maxsize)
    {
        return; // (a full reliable buffer: the next change or spawn sends it)
    }
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_AVATARCAL);
    MSG_WriteByte(msg, slot);
    MSG_WriteByte(msg, cal.slotHas[slot] ? 1 : 0);
    if(cal.slotHas[slot])
    {
        for(const int v : cal.slot[slot].v)
        {
            MSG_WriteShort(msg, v);
        }
    }
}

// vr_mp_avatars' calibrations: the other players' as this client has them ("mp_avatar_cal:"), its own now
// ("mp_avatar_cal_own:") and, hosting, the server's per client ("mp_avatar_cal_sv:").
void printCalibrations()
{
    if(cls.state == ca_connected && cl.viewentity > 0)
    {
        for(int e = 1; e <= MAX_SCOREBOARD; e++)
        {
            if(cal.has[e])
            {
                printCal("mp_avatar_cal", e, cal.of[e]);
            }
        }
        printCal("mp_avatar_cal_own", cl.viewentity, ownCalibration());
    }
    if(sv.active)
    {
        for(int slot = 0; slot < svs.maxclients && slot < MAX_SCOREBOARD; slot++)
        {
            if(cal.slotHas[slot])
            {
                printCal("mp_avatar_cal_sv", slot + 1, cal.slot[slot]);
            }
        }
    }
}

// "vr_avatarcal <version> <field>...": a client's calibration (sendCalibration), kept for his slot and, changed, passed on
// to every client.
void avatarcal_f()
{
    if(cmd_source != src_client || !sv.active || !host_client)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot < 0 || slot >= MAX_SCOREBOARD || Cmd_Argc() != 2 + numCalFields || Q_atoi(Cmd_Argv(1)) != calCommandVersion)
    {
        return;
    }
    Calibration c;
    for(int i = 0; i < numCalFields; i++)
    {
        c.v[i] = calClamp(i, Q_atoi(Cmd_Argv(2 + i)));
    }
    if(cal.slotHas[slot] && cal.slot[slot] == c)
    {
        return;
    }
    cal.slot[slot] = c;
    cal.slotHas[slot] = true;
    if(serverSendsCal())
    {
        writeCal(&sv.reliable_datagram, slot);
    }
}

} // namespace

bool Calibration::operator==(const Calibration& o) const
{
    for(int i = 0; i < numCalFields; i++)
    {
        if(v[i] != o.v[i])
        {
            return false;
        }
    }
    return true;
}

const Calibration* calibrationOf(int ent)
{
    return ent > 0 && ent <= MAX_SCOREBOARD && cal.has[ent] ? &cal.of[ent] : nullptr;
}

void sendCalibration()
{
    if(cls.state != ca_connected || cls.signon != SIGNONS || cls.demoplayback || cl.maxclients <= 1)
    {
        return;
    }
    const Calibration c = ownCalibration();
    if(cal.sent && (c == cal.lastSent || realtime < cal.nextSend))
    {
        return;
    }
    char text[256];
    int n = q_snprintf(text, sizeof(text), "vr_avatarcal %d", calCommandVersion);
    for(int i = 0; i < numCalFields && n > 0 && n < static_cast<int>(sizeof(text)); i++)
    {
        n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), " %d", c.v[i]);
    }
    if(n <= 0 || n >= static_cast<int>(sizeof(text)) || !qvr::client::roomForCommand(n))
    {
        return; // (no room: the next frame)
    }
    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, text);
    cal.lastSent = c;
    cal.sent = true;
    cal.nextSend = realtime + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
}

void parseCalibration()
{
    const int slot = MSG_ReadByte();
    const bool has = MSG_ReadByte() != 0;
    Calibration c;
    if(has)
    {
        for(int& v : c.v)
        {
            v = MSG_ReadShort();
        }
    }
    const int ent = slot + 1;
    if(ent > 0 && ent <= MAX_SCOREBOARD)
    {
        cal.of[ent] = c;
        cal.has[ent] = has;
    }
}

void serverReset(int slot)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD || !cal.slotHas[slot])
    {
        return;
    }
    cal.slotHas[slot] = false;
    if(serverSendsCal())
    {
        writeCal(&sv.reliable_datagram, slot); // (none now: the clients forget his)
    }
}

void serverWriteAll(sizebuf_t* msg)
{
    if(!serverSendsCal())
    {
        return;
    }
    for(int slot = 0; slot < svs.maxclients && slot < MAX_SCOREBOARD; slot++)
    {
        if(cal.slotHas[slot])
        {
            writeCal(msg, slot);
        }
    }
}

int movesByte(const edict_t* ent, bool flat)
{
    // (the server's VM: the hull's per-client boxes are numbered by its edicts -- a console command's report runs outside it)
    const progs::ServerVm vm;
    const int water = CLAMP(0, static_cast<int>(ent->v.waterlevel), 3);
    int bits = (flat ? avatarFlat : 0) | (hull::isCrouched(ent) ? avatarCrouched : 0) |
               (hull::isProne(ent) ? avatarLying : 0) |
               ((static_cast<int>(ent->v.flags) & FL_ONGROUND) ? avatarOnGround : 0) | (water << avatarWaterShift);
    edict_t* e = const_cast<edict_t*>(ent); // (climb's server calls take the edict as the VM's)
    for(int h = 0; h < 2; h++)
    {
        glm::vec3 at, out;
        if(climb::serverHold(e, h, at, out))
        {
            bits |= h ? avatarHoldMain : avatarHoldOff;
        }
    }
    return bits;
}

void writeHolds(sizebuf_t* msg, const edict_t* ent, const glm::vec3& origin)
{
    const progs::ServerVm vm;
    edict_t* e = const_cast<edict_t*>(ent);
    for(int h = 0; h < 2; h++)
    {
        glm::vec3 at, out;
        if(climb::serverHold(e, h, at, out))
        {
            const glm::vec3 v = at - origin;
            for(int i = 0; i < 3; i++)
            {
                MSG_WriteShort(msg, static_cast<int>(za::clamp(za::round(v[i] * 8.f), -32767.f, 32767.f)));
            }
            const float yaw = za::atan2(out.y, out.x) / glm::two_pi<float>() * 256.f;
            MSG_WriteByte(msg, static_cast<int>(za::lround(yaw)) & 255);
        }
    }
}

void parse()
{
    const int ent = MSG_ReadShort();
    Avatar a;
    a.valid = true;
    a.received = realtime;
    a.moves = MSG_ReadByte();
    a.flat = (a.moves & avatarFlat) != 0;
    a.head = readEighths();
    a.headAngles = readAngles16();
    a.body = readBodyPose();
    for(VrDrawnHand& h : a.hands)
    {
        h = readDrawnHand(glm::vec3{0.f}); // (offsets from his entity's origin)
    }
    a.weapon = MSG_ReadLong();
    a.weapon2 = MSG_ReadLong();
    for(int h = 0; h < 2; h++)
    {
        if(a.moves & (h ? avatarHoldMain : avatarHoldOff))
        {
            a.holdAt[h] = readEighths();
            const float yaw = static_cast<float>(MSG_ReadByte()) / 256.f * glm::two_pi<float>();
            a.holdOut[h] = glm::vec3{za::cos(yaw), za::sin(yaw), 0.f};
        }
    }
    a.holsters = readDrawnHolsters(glm::vec3{0.f}, false); // (offsets from his entity's origin)
    a.armour = MSG_ReadByte();
    a.msgTime = cl.mtime[0];
    if(ent > 0 && ent <= MAX_SCOREBOARD && ent != cl.viewentity)
    {
        // The one it replaces held until the message before this one (an unchanged avatar is not sent: its keyframe):
        // drawn from it to this one as his entity is lerped from that message to this.
        Avatar& was = avatars[ent];
        previousAvatars[ent] = was;
        previousAvatars[ent].msgTime = za::max(was.msgTime, cl.mtime[1]);
        previousAvatars[ent].valid = was.valid && was.msgTime < a.msgTime;
        was = a;
        // The interpolation buffer's history (B7b: a snapshot has every avatar in sight); not in the message before
        // (out of sight): it starts again.
        AvatarHistory& h = avatarHistories[ent];
        if(h.n > 0 && h.of[(h.head + avatarHistoryLen - 1) % avatarHistoryLen].msgTime != cl.mtime[1])
        {
            h.n = 0;
        }
        h.of[h.head] = a;
        h.head = (h.head + 1) % avatarHistoryLen;
        h.n = za::min(h.n + 1, avatarHistoryLen);
    }
}

glm::vec3 slerpAngles(const glm::vec3& a, const glm::vec3& b, float t)
{
    if(a == b)
    {
        return b;
    }
    const glm::quat qa = glm::normalize(glm::quat_cast(hands::basisOf(a)));
    glm::quat qb = glm::normalize(glm::quat_cast(hands::basisOf(b)));
    qb = glm::dot(qa, qb) < 0.f ? -qb : qb;
    const glm::mat3 m = glm::mat3_cast(glm::normalize(glm::slerp(qa, qb, t)));
    return hands::anglesFromVectors(m[0], m[2]);
}

namespace
{

[[nodiscard]] float lerpAngle(float a, float b, float t)
{
    float d = b - a;
    d -= 360.f * za::round(d / 360.f);
    return a + d * t;
}

void blendHand(VrDrawnHand& h, const VrDrawnHand& p, float t)
{
    if(p.flags != h.flags || p.weaponModel != h.weaponModel)
    {
        return; // (a hand or weapon come or gone: as it came)
    }
    h.handOrigin = glm::mix(p.handOrigin, h.handOrigin, t);
    h.handAngles = slerpAngles(p.handAngles, h.handAngles, t);
    for(int f = 0; f < VrDrawnHand::fingers; f++)
    {
        for(int j = 0; j < VrDrawnHand::joints; j++)
        {
            h.curl[f][j] = p.curl[f][j] + (h.curl[f][j] - p.curl[f][j]) * t;
        }
    }
    h.thumb = glm::normalize(glm::slerp(p.thumb, glm::dot(p.thumb, h.thumb) < 0.f ? -h.thumb : h.thumb, t));
    h.weaponOrigin = glm::mix(p.weaponOrigin, h.weaponOrigin, t);
    h.weaponAngles = slerpAngles(p.weaponAngles, h.weaponAngles, t);
}

[[nodiscard]] Avatar blend(const Avatar& p, const Avatar& a, float t);

// Avatar `e` as drawn now (vr_mp_avatar_lerp; off: the last as it came). With the interpolation buffer (a remote server's
// snapshots, vr_net_interp: B7b): between the two around its time, as his entity is. Else between the one before and the
// last, at his entity's lerp (CL_LerpPoint's: the client's time between the last two messages; B7); without one before
// (just come into sight): the last as it came. Places from his entity's origin lerped, turns slerped, curls lerped.
[[nodiscard]] Avatar blendedOf(int e)
{
    const Avatar& a = avatars[e];
    if(vr_mp_avatar_lerp.value == 0.f)
    {
        return a;
    }
    if(netsnap::interpActive())
    {
        const AvatarHistory& h = avatarHistories[e];
        const double rt = netsnap::interpTime();
        const Avatar* newer = nullptr;
        const Avatar* older = nullptr;
        for(int k = 0; k < h.n; k++)
        {
            const Avatar& s = h.of[(h.head - 1 - k + 2 * avatarHistoryLen) % avatarHistoryLen];
            if(s.msgTime <= rt)
            {
                older = &s;
                break;
            }
            newer = &s;
        }
        if(!older || !newer)
        {
            return older ? *older : newer ? *newer : a; // (before the oldest kept, or past the newest: held)
        }
        const double gap = newer->msgTime - older->msgTime;
        return blend(*older, *newer, gap > 0.0 ? static_cast<float>(za::clamp((rt - older->msgTime) / gap, 0.0, 1.0)) : 1.f);
    }
    const Avatar& p = previousAvatars[e];
    const double span = cl.mtime[0] - cl.mtime[1];
    if(!p.valid || a.msgTime != cl.mtime[0] || p.msgTime != cl.mtime[1] || span <= 0.0)
    {
        return a;
    }
    return blend(p, a, static_cast<float>(za::clamp((cl.time - cl.mtime[1]) / span, 0.0, 1.0)));
}

// `p` to `a`, `t` of the way.
Avatar blend(const Avatar& p, const Avatar& a, float t)
{
    if(t >= 1.f)
    {
        return a;
    }
    Avatar b = a;
    b.head = glm::mix(p.head, a.head, t);
    b.headAngles = slerpAngles(p.headAngles, a.headAngles, t);
    if(p.body.valid && a.body.valid)
    {
        b.body.yaw = lerpAngle(p.body.yaw, a.body.yaw, t);
        b.body.headHeight = glm::mix(p.body.headHeight, a.body.headHeight, t);
        b.body.standingHeight = glm::mix(p.body.standingHeight, a.body.standingHeight, t);
        b.body.leanHold = glm::mix(p.body.leanHold, a.body.leanHold, t);
    }
    for(int side = 0; side < 2; side++)
    {
        blendHand(b.hands[side], p.hands[side], t);
        if((p.moves & (side ? avatarHoldMain : avatarHoldOff)) && (a.moves & (side ? avatarHoldMain : avatarHoldOff)))
        {
            b.holdAt[side] = glm::mix(p.holdAt[side], a.holdAt[side], t);
        }
    }
    const VrDrawnHolsters& ph = p.holsters;
    VrDrawnHolsters& bh = b.holsters;
    if(ph.valid && bh.valid && ph.weapons == bh.weapons)
    {
        for(int i = 0; i < VrDrawnHolsters::count; i++)
        {
            if((bh.weapons & (1 << i)) && ph.model[i] == bh.model[i])
            {
                bh.origin[i] = glm::mix(ph.origin[i], bh.origin[i], t);
                bh.angles[i] = slerpAngles(ph.angles[i], bh.angles[i], t);
            }
        }
    }
    return b;
}

} // namespace

bool drawnPlaces(int ent, glm::vec3& body, glm::vec3 handsAt[2], int& frame)
{
    if(ent <= 0 || ent > MAX_SCOREBOARD || drawn.of[ent].frame < 0)
    {
        return false;
    }
    const Drawn& d = drawn.of[ent];
    body = d.placedAt;
    for(int side = 0; side < 2; side++)
    {
        const view::ViewEntity& ve = d.hand[side].visible ? d.hand[side] : d.weapon[side];
        handsAt[side] = glm::vec3{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
    }
    frame = d.frame;
    return true;
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
        setup(e, blendedOf(e));
        Drawn& d = drawn.of[e];
        for(int side = 0; side < 2; side++)
        {
            add(d.weapon[side]);
            add(d.hand[side]);
            add(d.pauldron[side]);
            add(d.pauldronArm[side]);
        }
        for(view::ViewEntity& ve : d.holster)
        {
            add(ve);
        }
        add(d.plate);
        add(d.plateBridge);
        if(d.body.visible && d.body.ent.model)
        {
            // In his player model's place (it would stand round the body), or added.
            bool replaced = false;
            for(int i = 0; i < cl_numvisedicts && !replaced; i++)
            {
                replaced = cl_visedicts[i] == &cl_entities[e];
                cl_visedicts[i] = replaced ? &d.body.ent : cl_visedicts[i];
            }
            if(!replaced)
            {
                add(d.body);
            }
        }
    }
    // Their skin masks (a skin tone, gloves, nails: their own look, view::lookOf) found now, outside the draw loops.
    entity_t* shown[(MAX_SCOREBOARD + 1) * 3];
    int count = 0;
    for(int e = 1; e <= MAX_SCOREBOARD; e++)
    {
        Drawn& d = drawn.of[e];
        if(d.frame == host_framecount)
        {
            for(view::ViewEntity* ve : {&d.hand[0], &d.hand[1], &d.body})
            {
                if(ve->visible && ve->ent.model)
                {
                    shown[count++] = &ve->ent;
                }
            }
        }
    }
    render::precacheSkinMasks(shown, count);
}

int lookOf(const entity_t* e, view::SkinLook& look)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&drawn.of[0]);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(drawn.of))
    {
        return 0;
    }
    const int owner = static_cast<int>((p - first) / sizeof(Drawn));
    const Drawn& d = drawn.of[owner];
    const int kind = e == &d.hand[0].ent || e == &d.hand[1].ent ? 1 : e == &d.body.ent ? 2 : 0;
    if(!kind || !cal.has[owner])
    {
        return 0; // (a weapon; no calibration yet: drawn bare)
    }
    const int* v = cal.of[owner].v;
    look.tone = static_cast<float>(v[calTone]) / 100.f;
    look.gloves = v[calGloves];
    look.glove = glm::vec3{static_cast<float>(v[calGloveHue]), static_cast<float>(v[calGloveSat]) / 255.f,
        static_cast<float>(v[calGloveLight]) / 255.f};
    look.nails = v[calNails] != 0;
    look.nail = glm::vec3{static_cast<float>(v[calNailHue]), static_cast<float>(v[calNailSat]) / 255.f,
        static_cast<float>(v[calNailLight]) / 255.f};
    look.nailGloss = static_cast<float>(v[calNailGloss]) / 255.f;
    look.bare = v[calBracers] == 0;
    return kind;
}

const entity_t* plateBody(const entity_t* e)
{
    const Drawn* p = platesOf(e);
    return p ? &p->body.ent : nullptr;
}

int bonePoses(const entity_t* e, const float** matrices)
{
    if(Drawn* p = platesOf(e); p && !p->plateBones.empty())
    {
        // His plates or their leather: his body's bones as drawn (B6c), by your plates' own code.
        const bool bridge = e == &p->plateBridge.ent;
        const int n = view::otherPlateBonePoses(p->body.ent, bridge, p->platePieces,
            bridge ? p->bridgeBones.data() : p->plateBones.data(), matrices);
        if(!bridge && matrices && *matrices)
        {
            // (the report's: the slots drawn, and those his body's bones showed)
            p->plateSlots = n;
            p->plateShown = 0;
            for(int i = 0; i < n; i++)
            {
                bool any = false;
                for(int k = 0; k < 12; k++)
                {
                    any = any || (*matrices)[i * 12 + k] != 0.f;
                }
                p->plateShown += any ? 1 : 0;
            }
        }
        return n;
    }
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
    if(const Drawn* d = drawnOf(e, side, hand))
    {
        return hand ? d->hand[side].mirrored : d->weapon[side].mirrored;
    }
    // (his holstered weapons, his pauldrons: B6b)
    const auto* first = reinterpret_cast<const unsigned char*>(&drawn.of[0]);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(drawn.of))
    {
        return false;
    }
    const Drawn& o = drawn.of[(p - first) / sizeof(Drawn)];
    for(const view::ViewEntity& ve : o.holster)
    {
        if(e == &ve.ent)
        {
            return ve.mirrored;
        }
    }
    for(int s = 0; s < 2; s++)
    {
        if(e == &o.pauldron[s].ent || e == &o.pauldronArm[s].ent)
        {
            return o.pauldron[s].mirrored || o.pauldronArm[s].mirrored;
        }
    }
    return false;
}

int gearByte(const edict_t* ent)
{
    const int items = static_cast<int>(ent->v.items);
    return ent->v.armorvalue <= 0.f ? 0 : (items & IT_ARMOR3) ? 3 : (items & IT_ARMOR2) ? 2 : (items & IT_ARMOR1) ? 1 : 0;
}

void clear()
{
    for(Avatar& a : avatars)
    {
        a = Avatar{};
    }
    for(Avatar& a : previousAvatars)
    {
        a = Avatar{};
    }
    for(AvatarHistory& h : avatarHistories)
    {
        h.n = 0;
    }
    for(bool& h : cal.has)
    {
        h = false; // (the server sends them all at the spawn)
    }
    cal.sent = false; // (this client's own goes up again)
}

void init()
{
    Cmd_AddCommand("vr_mp_avatars", avatars_f);
    Cmd_AddCommand_ClientCommand("vr_avatarcal", avatarcal_f);
}

} // namespace qvr::mpavatar

extern "C" void VR_AddMpAvatars(void)
{
    qvr::mpavatar::addEntities();
}
