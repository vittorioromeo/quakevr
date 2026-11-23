// vr_mpavatar.cpp -- other players' VR state (QVR_SVC_AVATAR; vr_mpavatar.hpp): kept, drawn, reported.

#include "vr_mpavatar.hpp"

#include "vr_client.hpp"

#include "vr_api.h"

#include "vr_agarmor.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_climb.hpp"
#include "vr_climbrope.hpp"

#include "vr_cvars.hpp"
#include "vr_disarmviz.hpp"
#include "vr_flashlight.hpp"
#include "vr_handrig.hpp"
#include "vr_hands.hpp"
#include "vr_hitmodel.hpp"
#include "vr_hull.hpp"
#include "vr_move.hpp"
#include "vr_netsnap.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_render.hpp"
#include "vr_seated.hpp"
#include "vr_server.hpp"
#include "vr_slide.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_geom.hpp"
#include "vr_ragdoll.hpp"
#include "vr_emissive.hpp"
#include "vr_body.hpp"
#include "vr_sightalign.hpp"
#include "vr_view.hpp"
#include "vr_mem.hpp"
#include "vr_gadget.hpp"
#include "vr_geom.hpp"
#include "vr_grasp.hpp"

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
    int owner{0};  // ... as whose copy (his entity: a player's slot is his own; a doll slot's, the dead body's owner)
    int doll{0};   // ... as his dead body: its entity (addDolls; 0 his living body)
    glm::vec3 placedAt{0.f}; // his entity's drawn place then (the trace's: vr_mp_trace)
    // His body (vr_mp_body), as last posed (the report's: the skeleton, the wrists its arms reached for).
    view::ViewEntity body;
    bool bodyPosed{false};
    avatar::Skeleton skeleton;
    glm::vec3 handWrist[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float eyeHeight{0.f};
    float eyeUnits{0.f}; // ... in world units, its feet to its eyes (eyeHeight at its world scale: a bot's, the default player's)
    bool seated{false};
    double posedAt{0.0}; // the server's time of the avatar it was posed from (interpolated: the time drawn; the report's)
    // His holstered weapons (B6b: as he drew them, body::Holster's order) and pauldrons on his body (his look).
    view::ViewEntity holster[VrDrawnHolsters::count];
    // His guns' parts as yours are drawn (wpnsync: the revolver's crane and cylinder, the super shotgun broken open, the
    // magazines, the ammo screens; view::drawGunParts), in his hands and holsters; his torch.
    view::GunParts gun[2];
    view::GunParts holsterGun[VrDrawnHolsters::count];
    view::ViewEntity lamp;
    view::PowerupLook powerups; // his power-ups' look as drawn (syncfix)
    view::ViewEntity gear[VrDrawnGear::count]; // his worn gear (syncfix: VrDrawnGear's order)
    view::ViewEntity pouchItem[pouchitems::most]; // the rounds in his ammo pouch (syncfix2: view::posePouchItems)
    int pouchItems{0};
    view::ViewEntity pauldron[2];
    view::ViewEntity pauldronArm[2];
    // His armour's plates and their joints' leather on his body (B6c: the server's word on him), their pieces, their bones
    // (made as they are drawn: view::otherPlateBonePoses; sized when first shown); their morphs as yours (B11c part 2:
    // view::morphPlates, his state; the settled pieces, then each morph's).
    static constexpr int plateCount = 1 + view::BodyPlates::maxMorphs;
    view::ViewEntity plate[plateCount];
    view::ViewEntity plateBridge;
    int plateType{0};
    int platePieces{0};               // all he wears (the leather's)
    int entityPieces[plateCount]{};   // each plates entity's
    view::PlateMorphs morph;
    int plateReport{-1};              // the plates entity the report reads (the settled ones, else the first coming in)
    int plateSlots{0}, plateShown{0}; // as last drawn (the report's)
    za::Vector<float> plateBones[plateCount];
    za::Vector<float> bridgeBones;
    // (ropes4) On a rope: the copy's move onto the server's holds and how far it is on (ropeShift), each held hand's
    // palm from its place as last drawn, what is left of its way to its hold and how far on (ropePinHands); cl.time then.
    glm::vec3 shift{0.f};
    float shiftWeight{0.f};
    bool palmKnown[2]{false, false};
    glm::vec3 palmFromHand[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 handPin[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float handPinWeight[2]{0.f, 0.f};
    // ... and each hand on a rope this frame, where its palm goes (eased from hold to hold: its hold from his origin as last
    // seen, the one it eases from and how far)
    bool ropeHeld[2]{false, false};
    glm::vec3 palmTarget[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 holdWas[2]{glm::vec3{1e9f}, glm::vec3{1e9f}};
    glm::vec3 regrabFrom[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float regrabAlong[2]{1.f, 1.f};
    double pinTime{-1.0};
};
// By player entity (0 unused), then the doll slots (avatar::otherKeys - avatar::dollSlots on: a dead player's bodies while
// his own slot is taken, by his living copy or a nearer body of his; mpbugs4, 2026-11-21: once he respawned, his old body
// was id's ranger again).
struct DrawnAll
{
    Drawn of[avatar::otherKeys];
    int dollEnt[avatar::dollSlots]{}; // the body each doll slot last drew (its entity; kept while it is out of sight)
    int addedFrame{-1};
};
constexpr int firstDollSlot = avatar::otherKeys - avatar::dollSlots;
DrawnAll drawn;

// How long an avatar is drawn after its last message (an unchanged one comes every vr_net_avatar_keyframe frames).
constexpr double staleAfter = 1.0;
// Frames undrawn after which his copy counts as first drawn again (his plates' morphs: resync).
constexpr int freshAfterFrames = 36;
constexpr float ropePinTime = 0.15f;  // s: his copy eased onto the server's rope holds and off (ropeShift, ropePinHands)
constexpr float ropeShiftMost = 48.f; // units: ropeShift moves his copy at most
constexpr float ropeRegrab = 8.f;     // units: a hand's hold (from his origin) moved this far in a frame went hand over hand

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
    printVec("headeye", k.headEye);
    printVec("pelvis", k.pelvis);
    printVec("wrist0", k.wrist[HAND_OFF]);
    printVec("wrist1", k.wrist[HAND_MAIN]);
    printVec("shoulder0", k.shoulder[HAND_OFF]);
    printVec("shoulder1", k.shoulder[HAND_MAIN]);
    printVec("ankle0", k.ankle[0]);
    printVec("ankle1", k.ankle[1]);
    printVec("elbow0", k.elbow[HAND_OFF]);
    printVec("elbow1", k.elbow[HAND_MAIN]);
}

// His guns and torch as last drawn (wpnsync; the harness's mp_wpnsync): each hand's look as received and what was drawn
// of it ("mp_gun:": in parts or whole, the super shotgun's barrels' angle, the revolver's crane and cylinder and shells
// drawn, the magazine in (its frame: the rounds shown), sliding out, the ammo screen's button), each holstered gun's
// ("mp_gun_holster:"), the torch ("mp_lamp:": placed, its model drawn, lit, its lights alive, its lens and beam).
void printGuns(int e, const Avatar& a, const Drawn& d)
{
    const auto vis = [](const view::ViewEntity& ve) { return ve.visible && ve.ent.model ? 1 : 0; };
    for(int side = 0; side < 2; side++)
    {
        const VrGunLook& l = a.look[side];
        const view::GunParts& g = d.gun[side];
        const view::ViewEntity* p = g.part;
        int shells = 0;
        for(int i = 0; i < 6; i++)
        {
            shells += vis(p[view::GunParts::RvShell + i]);
        }
        Con_Printf("mp_gun: ent=%d side=%d bits=%d flags=%d clip=%d rv=%d cyl=%.1f model=%s whole=%d parts=%d ssg=%.1f "
                   "swing=%.1f turn=%.1f shells=%d mag=%d magframe=%d magout=%d well=%d button=%d\n",
            e, side, l.bits, l.flags, l.clip, l.revolver, static_cast<double>(l.cylinder),
            d.weapon[side].ent.model ? d.weapon[side].ent.model->name : "-", vis(d.weapon[side]), g.inParts ? 1 : 0,
            static_cast<double>(g.ssg.angle), static_cast<double>(g.rv.swing), static_cast<double>(g.rv.cyl), shells,
            vis(p[view::GunParts::MagIn]), p[view::GunParts::MagIn].ent.frame, vis(p[view::GunParts::MagOut]),
            vis(p[view::GunParts::Well]), vis(p[view::GunParts::Button]));
    }
    for(int i = 0; i < VrDrawnHolsters::count; i++)
    {
        const view::GunParts& g = d.holsterGun[i];
        if(vis(d.holster[i]) || g.inParts)
        {
            Con_Printf("mp_gun_holster: ent=%d slot=%d flags=%d clip=%d parts=%d ssg=%.1f mag=%d magframe=%d\n", e, i,
                a.holsterFlags[i], a.holsterClip[i], g.inParts ? 1 : 0, static_cast<double>(g.ssg.angle),
                vis(g.part[view::GunParts::MagIn]), g.part[view::GunParts::MagIn].ent.frame);
        }
    }
    const VrDrawnLamp& lamp = a.lamp;
    const glm::vec3 at = d.placedAt + lamp.origin;
    const glm::vec3 dir = lamp.rot * glm::vec3{1.f, 0.f, 0.f};
    Con_Printf("mp_lamp: ent=%d flags=%d shown=%d lit=%d lights=%d range=%.0f at=%.1f %.1f %.1f dir=%.3f %.3f %.3f\n", e,
        lamp.flags, vis(d.lamp), (lamp.flags & VrDrawnLamp::litBit) ? 1 : 0, flashlight::otherLights(e),
        static_cast<double>(lamp.range), static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
        static_cast<double>(dir.x), static_cast<double>(dir.y), static_cast<double>(dir.z));
}

// (bots3) A held weapon as drawn (`w`: his hand's weapon entity): its butt and muzzle in the world (its model's own,
// view::weaponLocalGeometry, on the entity's place, turn and mirroring). False: none drawn, no weapon slot.
[[nodiscard]] bool drawnWeaponLine(const view::ViewEntity& w, glm::vec3& butt, glm::vec3& muzzle)
{
    glm::vec3 second{0.f};
    int secondType = 0;
    if(!w.visible || !w.ent.model || !view::weaponLocalGeometry(w.ent.model, muzzle, butt, second, secondType))
    {
        return false;
    }
    float m[16];
    vec3_t origin, angles;
    VectorCopy(w.ent.origin, origin);
    VectorCopy(w.ent.angles, angles);
    R_EntityMatrix(m, origin, angles, ENTSCALE_DEFAULT);
    const auto world = [&](const glm::vec3& p) {
        const glm::vec3 q{p.x, w.mirrored ? -p.y : p.y, p.z};
        return glm::vec3{m[0] * q.x + m[4] * q.y + m[8] * q.z + m[12], m[1] * q.x + m[5] * q.y + m[9] * q.z + m[13],
            m[2] * q.x + m[6] * q.y + m[10] * q.z + m[14]};
    };
    butt = world(butt);
    muzzle = world(muzzle);
    return true;
}

// One line per avatar (the harness's mp_avatar): head and hands in the world (from his entity's origin now), the main
// hand's angles, the curls and thumbs, the weapons' models; then what was drawn of it last frame.
void printAvatar(int e, const Avatar& a, bool hands)
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
    if(((a.hands[0].flags | a.hands[1].flags) & VrDrawnHand::solveBit) && d.frame >= host_framecount - 1)
    {
        // A bot's hands as placed here (view::solveHands): each jointed hand drawn, how far from its controller (as
        // received, on his entity as drawn), and its weapon's model and how far from the hand.
        Con_Printf("mp_solved: ent=%d", e);
        for(int side = 0; side < 2; side++)
        {
            const view::ViewEntity& h = d.hand[side];
            const view::ViewEntity& w = d.weapon[side];
            const glm::vec3 ho{h.ent.origin[0], h.ent.origin[1], h.ent.origin[2]};
            const glm::vec3 wo{w.ent.origin[0], w.ent.origin[1], w.ent.origin[2]};
            Con_Printf(" hand%d=%d gap%d=%.2f wpn%d=%s grip%d=%.2f", side, h.visible ? 1 : 0, side,
                glm::length(ho - (d.placedAt + a.hands[side].handOrigin)), side,
                w.visible && w.ent.model ? w.ent.model->name : "-", side, w.visible ? glm::length(wo - ho) : -1.f);
        }
        Con_Printf("\n");
    }
    for(int side = 0; side < 2 && d.frame >= host_framecount - 1; side++)
    {
        // (bots3) Each weapon as drawn here: its butt and muzzle (the harness's grab at the drawn weapon: mp_test.py
        // botgrab), a bot's or a player's.
        glm::vec3 butt, muzzle;
        if(drawnWeaponLine(d.weapon[side], butt, muzzle))
        {
            Con_Printf("mp_drawnwpn: ent=%d side=%d mdl=%s", e, side, d.weapon[side].ent.model->name);
            printVec("butt", butt);
            printVec("muzzle", muzzle);
            Con_Printf("\n");
        }
    }
    {
        // (bots4) His Show Disarm Volumes and Show Hit Zones as drawn here (a host's from its QC, a client's rebuilt): how
        // many, the first grab capsule's ends, the head zone's centre (mp_test.py debugviz).
        int volumes = 0, zoneCount = 0;
        glm::vec3 grabA{0.f}, grabB{0.f}, head{0.f};
        const bool shownV = disarmviz::playerShown(e, volumes, grabA, grabB);
        const bool shownZ = hitmodel::playerZonesDrawn(e, zoneCount, head);
        if(shownV || shownZ)
        {
            Con_Printf("mp_debugviz: ent=%d volumes=%d zones=%d", e, shownV ? volumes : 0, shownZ ? zoneCount : 0);
            printVec("grab0", grabA);
            printVec("grab1", grabB);
            printVec("head", head);
            Con_Printf("\n");
        }
    }
    if(d.bodyPosed && d.body.ent.model && (d.frame == host_framecount - 1 || d.frame == host_framecount))
    {
        printSkeleton("mp_body", e, d.body.ent.model->name, d.skeleton);
        printVec("handwrist0", d.handWrist[0]);
        printVec("handwrist1", d.handWrist[1]);
        float pitch = 0.f;
        float lying = 0.f;
        float sliding = 0.f;
        (void)avatar::otherPitch(e, pitch, lying, sliding);
        float climbWeight = 0.f;
        (void)avatar::otherClimb(e, climbWeight);
        Con_Printf(" eyeheight=%.3f eyeunits=%.1f yaw=%.1f moves=%d pitch=%.1f lying=%.2f sliding=%.2f slide=%d climb=%.2f seated=%d posedat=%.3f\n",
            d.eyeHeight, d.eyeUnits, a.body.yaw, a.moves, pitch, lying, sliding, a.sliding ? 1 : 0, climbWeight, d.seated ? 1 : 0, d.posedAt);
        if((a.moves & (avatarHoldOff | avatarHoldMain)) != 0)
        {
            // (ropes3) Holding on, a climbable rope by him: his drawn wrists, ankles and toes, how far each is from the rope as
            // this client has it (hands on it, the feet round it: mp_test.py ropeclimb).
            const auto near = [](const glm::vec3& p) { return climbrope::clientRopeDistance(p); };
            if(za::min(near(d.handWrist[0]), near(d.handWrist[1])) < 32.f)
            {
                // (a hand that doesn't hold, -1: a flat player's weapon hand keeps its weapon, off the rope)
                const float w0 = (a.moves & avatarHoldOff) ? near(d.handWrist[0]) : -1.f;
                const float w1 = (a.moves & avatarHoldMain) ? near(d.handWrist[1]) : -1.f;
                // (and each held wrist's distance from the server's hold, as sent: his drawn hand against the hold)
                const float h0 = (a.moves & avatarHoldOff) ? glm::distance(d.handWrist[0], origin + a.holdAt[0]) : -1.f;
                const float h1 = (a.moves & avatarHoldMain) ? glm::distance(d.handWrist[1], origin + a.holdAt[1]) : -1.f;
                const float r0 = (a.moves & avatarHoldOff) ? near(origin + a.holdAt[0]) : -1.f;
                const float r1 = (a.moves & avatarHoldMain) ? near(origin + a.holdAt[1]) : -1.f;
                Con_Printf("mp_ropepose: ent=%d wrist0=%.1f wrist1=%.1f ankle0=%.1f ankle1=%.1f toe0=%.1f toe1=%.1f climb=%.2f "
                           "hold0=%.1f hold1=%.1f holdrope0=%.1f holdrope1=%.1f pelvis=%.1f org=%.1f head=%.1f placed=%.1f neck=%.1f\n",
                    e, w0, w1, near(d.skeleton.ankle[0]), near(d.skeleton.ankle[1]), near(d.skeleton.toe[0]), near(d.skeleton.toe[1]),
                    climbWeight, h0, h1, r0, r1, near(d.skeleton.pelvis), near(origin), near(d.skeleton.head), near(d.placedAt), near(d.skeleton.headJoint));
            }
        }
        view::printPauldrons("mp_pauldron", e, d.pauldron);
        if(const view::ViewEntity* plate = d.plateReport >= 0 ? &d.plate[d.plateReport] : nullptr;
            plate && plate->visible && plate->ent.model && !d.plateBones[d.plateReport].empty())
        {
            // His plates as last drawn: their bone slots, and how many his body's bones showed (a hidden piece's collapsed);
            // their morphs (begun so far, the pieces coming in, the newest coming in's shader value).
            int coming = 0;
            float morph = 0.f;
            for(int i = 1; i < Drawn::plateCount; i++)
            {
                const bool in = d.plate[i].visible && d.plate[i].morph > 0.f;
                coming |= in ? d.entityPieces[i] : 0;
                morph = in ? za::max(morph, d.plate[i].morph) : morph;
            }
            Con_Printf("mp_plates: ent=%d type=%d pieces=%d skin=%d slots=%d shown=%d leather=%d begun=%d coming=%d morph=%.3f "
                       "model=%s\n",
                e, d.plateType, d.platePieces, plate->ent.skinnum, d.plateSlots, d.plateShown, d.plateBridge.visible ? 1 : 0,
                d.morph.begun, coming, morph, plate->ent.model->name);
        }
    }
    if(d.frame == host_framecount - 1 || d.frame == host_framecount)
    {
        printGuns(e, a, d);
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
        if(hands)
        {
            view::printHands("mp_hands", e, d.hand, skin, d.weapon, d.holster, VrDrawnHolsters::count);
        }
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
    // ("vr_mp_avatars hands": each player's hands, held and holstered weapons as drawn too, mp_hands and mp_hands_own)
    const bool hands = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "hands");
    if(cls.state == ca_connected && cl.viewentity > 0)
    {
        for(int e = 1; e <= MAX_SCOREBOARD; e++)
        {
            if(avatars[e].valid)
            {
                printAvatar(e, avatars[e], hands);
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
            float pitch = 0.f, lying = 0.f, sliding = 0.f;
            (void)avatar::ownPitch(pitch, lying, sliding);
            Con_Printf(" pitch=%.1f lying=%.2f sliding=%.2f\n", pitch, lying, sliding);
            // (ropepose) Climbing a rope: his own body against the rope as this client draws it, as mp_ropepose has
            // another's copy (mp_test.py ropeclimb: the copy hangs as he does).
            const auto near = [](const glm::vec3& p) { return climbrope::clientRopeDistance(p); };
            if((climb::holding(HAND_MAIN) || climb::holding(HAND_OFF)) && za::min(near(k.wrist[0]), near(k.wrist[1])) < 32.f)
            {
                Con_Printf("mp_ropepose_own: ent=%d wrist0=%.1f wrist1=%.1f ankle0=%.1f ankle1=%.1f pelvis=%.1f head=%.1f neck=%.1f\n",
                    cl.viewentity, near(k.wrist[0]), near(k.wrist[1]), near(k.ankle[0]), near(k.ankle[1]), near(k.pelvis),
                    near(k.head), near(k.headJoint));
            }
            // Flat-screen play (adspose): the eye the view and the sights are solved from, the held gun's rear sight, the
            // ironsight's travel: against the drawn head's eyes (headeye) others see.
            if(flatplay::active())
            {
                const hands::State& s = hands::current();
                glm::vec3 org{0.f}, dir{0.f}, rear{0.f};
                flatplay::crosshairRay(s, geom::vec(cl.viewangles), org, dir);
                const char* model = "none";
                const bool sight = flatsight::rearSight(s, HAND_MAIN, rear, &model);
                Con_Printf("mp_adspose_own: ent=%d", cl.viewentity);
                printVec("eye", org);
                printVec("rear", sight ? rear : org);
                // (stime: the server's time of his last message -- the harness waits for another player's copy posed
                // from an avatar later than it, adspose3)
                Con_Printf(" sight=%d ads=%.2f look=%.1f eyeup=%.2f stime=%.3f wmodel=%s\n", sight ? 1 : 0, flatsight::adsShare(),
                    cl.viewangles[0], org.z - s.playerOrigin.z, cl.mtime[0], model);
                // The drawn head (with it, as seen from outside) against the eye: its top and its lowest point above the
                // head joint, and how far its face stands ahead of the eye (along the look, level).
                za::Vector<glm::vec3> verts;
                za::Vector<unsigned> tris;
                za::Vector<unsigned char> parts;
                if(avatar::posedMesh(verts, tris, parts, true))
                {
                    const glm::vec3 fwd = glm::normalize(glm::vec3{dir.x, dir.y, 0.f} + glm::vec3{1e-6f, 0.f, 0.f});
                    float top = -1e9f, low = 1e9f, front = -1e9f;
                    for(za::SizeT i = 0; i + 2 < tris.size(); i += 3)
                    {
                        for(za::SizeT j = 0; parts[i / 3] == 1 && j < 3; j++)
                        {
                            const glm::vec3 v = verts[tris[i + j]] - org;
                            top = za::max(top, v.z);
                            low = v.z > k.headJoint.z - org.z ? za::min(low, v.z) : low;
                            front = za::abs(v.z) < 1.f ? za::max(front, glm::dot(v, fwd)) : front;
                        }
                    }
                    Con_Printf("mp_adspose_head: top=%.2f low=%.2f front=%.2f\n", top, low, front);
                }
            }
        }
        // The dead players' bodies drawn as theirs (addDolls): whose, the model, the joints, the doll's entity, the look.
        for(int k = 1; k < avatar::otherKeys; k++) // (the players' slots, then the doll slots: slot=)
        {
            const Drawn& d = drawn.of[k];
            if(d.doll > 0 && d.bodyPosed && d.body.ent.model && (d.frame == host_framecount - 1 || d.frame == host_framecount))
            {
                printSkeleton("mp_doll", d.owner, d.body.ent.model->name, d.skeleton);
                Con_Printf(" slot=%d plates=%d %d", k, d.plateType, d.platePieces); // (vrnotes1122a: the body's own)
                view::SkinLook look;
                const int kind = mpavatar::lookOf(&d.body.ent, look);
                const entity_t& de = cl_entities[d.doll];
                Con_Printf(" doll=%d dollmodel=%s ragdoll=%d dollat=%.1f %.1f %.1f look=%d tone=%.2f gloves=%d hands=%d %d\n",
                    d.doll, de.model ? de.model->name : "-", ragdoll::published(d.doll) ? 1 : 0, de.origin[0], de.origin[1],
                    de.origin[2], kind, look.tone, look.gloves, d.hand[0].visible ? 1 : 0, d.hand[1].visible ? 1 : 0);
                // (avatardoll: its ragdoll's parts, their scale, and its left leg's length as drawn: hip to knee to ankle)
                glm::quat rot;
                glm::vec3 pos;
                float scale = 1.f;
                const ragdoll::Rig* rig = nullptr;
                int parts = 0;
                float leg = 0.f, arm = 0.f, restArm = 0.f;
                if(ragdoll::drawnPart(d.doll, 0, rot, pos, scale, &rig) && rig)
                {
                    parts = rig->numBones;
                    // (a chain's length as drawn: its three bones' pivots; backlog1122c: the left arm, shoulder to elbow
                    // to wrist, in his proportions, beside the model's own arm at his size)
                    const auto chain = [&](const char* const (&names)[3], float& rest) {
                        glm::vec3 at[3], was[3];
                        bool all = true;
                        const qmodel_t* source = ragdoll::sourceModel(&de); // (drawn with its skinned copy: its .mdl)
                        qmodel_t* mdl = const_cast<qmodel_t*>(source ? source : de.model);
                        const ragdoll::Rig* base = mdl ? ragdoll::rigFor(mdl) : nullptr;
                        for(int j = 0; j < 3; j++)
                        {
                            int b = 0;
                            while(b < rig->numBones && ZA_STRCMP(rig->bones[b].name, names[j]) != 0)
                            {
                                b++;
                            }
                            float s = 1.f;
                            all = all && b < rig->numBones && ragdoll::drawnPart(d.doll, b, rot, pos, s);
                            at[j] = all ? rot * (s * rig->bones[b].pivot) + pos : glm::vec3{0.f};
                            was[j] = all && base && b < base->numBones ? s * base->bones[b].pivot : glm::vec3{0.f};
                        }
                        rest = glm::distance(was[0], was[1]) + glm::distance(was[1], was[2]);
                        return all ? glm::distance(at[0], at[1]) + glm::distance(at[1], at[2]) : 0.f;
                    };
                    const char* const legNames[3] = {"thigh_l", "shin_l", "foot_l"};
                    const char* const armNames[3] = {"upperarm_l", "forearm_l", "hand_l"};
                    float restLeg = 0.f;
                    leg = chain(legNames, restLeg);
                    arm = chain(armNames, restArm);
                }
                Con_Printf("mp_doll_rig: ent=%d doll=%d parts=%d dollscale=%.3f leg=%.2f arm=%.2f restarm=%.2f\n", d.owner, d.doll,
                    parts, scale, leg, arm, restArm);
            }
        }
        view::printOwnLooks(cl.viewentity);
        view::printOwnPauldrons(cl.viewentity);
        view::printOwnPlates(cl.viewentity);
        if(hands)
        {
            view::printOwnHands(cl.viewentity);
        }
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
        // His own guns' looks (what the server sends the others of them: his stats) and torch (the move's: wpnsync).
        for(int side = 0; side < 2; side++)
        {
            const VrGunLook l = view::ownGunLook(side);
            Con_Printf("mp_gun_own: ent=%d side=%d bits=%d flags=%d clip=%d rv=%d cyl=%.1f\n", cl.viewentity, side, l.bits,
                l.flags, l.clip, l.revolver, static_cast<double>(l.cylinder));
        }
        VrDrawnLamp lamp;
        flashlight::netLamp(lamp);
        const glm::vec3 dir = lamp.rot * glm::vec3{1.f, 0.f, 0.f};
        Con_Printf("mp_lamp_own: ent=%d flags=%d at=%.1f %.1f %.1f dir=%.3f %.3f %.3f\n", cl.viewentity, lamp.flags,
            static_cast<double>(lamp.origin.x), static_cast<double>(lamp.origin.y), static_cast<double>(lamp.origin.z),
            static_cast<double>(dir.x), static_cast<double>(dir.y), static_cast<double>(dir.z));
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

// The yaw of a body's trunk as posed: its shoulders' line, level (`fallback` if they are on top of each other).
[[nodiscard]] float trunkYaw(const avatar::Skeleton& k, float fallback)
{
    const glm::vec2 across{k.shoulder[1].x - k.shoulder[0].x, k.shoulder[1].y - k.shoulder[0].y}; // (left to right)
    if(glm::length(across) < 1e-3f)
    {
        return fallback;
    }
    return glm::degrees(za::atan2(across.x, -across.y)); // (forward: right turned a quarter left, up the z axis)
}

// vr_mp_gearcheck [ent] (mpbothands; his report 2026-11-22: bots drawn with an extra pair of fists, a gun and a shovel
// floating beside the body; Debug > Reports > Network: Other Players' Gear): each other player's copy as drawn last frame
// (or `ent`'s): how many hand models it draws (its two hands, nothing else: an empty holster -- the fists' -- draws none),
// its holstered weapons and sheaths, and how far the farthest of them is from its trunk (pelvis to neck, as posed) and
// its hands from its arms' wrists. "mp_gear:" per player (ok=0: more or fewer than two hands, or a piece off the body),
// then "mp_gearcheck: N players, M bad".
void gearcheck_f()
{
    const int only = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 0;
    int players = 0, bad = 0;
    for(int e = 1; cls.state == ca_connected && e <= MAX_SCOREBOARD; e++)
    {
        const Drawn& d = drawn.of[e];
        if((only && e != only) || e == cl.viewentity || d.frame < host_framecount - 1 || !d.bodyPosed)
        {
            continue;
        }
        const auto shown = [](const view::ViewEntity& ve) { return ve.visible && ve.ent.model != nullptr; };
        const avatar::Skeleton& k = d.skeleton;
        const glm::vec3 neck = 0.5f * (k.shoulder[0] + k.shoulder[1]);
        int hands = 0, holsters = 0, sheaths = 0;
        float trunk = 0.f, wrist = 0.f;
        for(int side = 0; side < 2; side++)
        {
            hands += shown(d.hand[side]) && view::handModel(d.hand[side].ent.model) ? 1 : 0;
            hands += shown(d.weapon[side]) && view::handModel(d.weapon[side].ent.model) ? 1 : 0;
            if(shown(d.hand[side]))
            {
                wrist = za::max(wrist, glm::distance(glm::vec3{d.hand[side].ent.origin[0], d.hand[side].ent.origin[1],
                                                         d.hand[side].ent.origin[2]},
                                           k.wrist[side]));
            }
        }
        const auto at = [](const view::ViewEntity& ve) { return glm::vec3{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]}; };
        bool finite = true;
        const auto num = [](const glm::vec3& v) { return ZA_ISFINITE(v.x) && ZA_ISFINITE(v.y) && ZA_ISFINITE(v.z); };
        const auto onTrunk = [&](const view::ViewEntity& ve) {
            const glm::vec3 p = at(ve), a{ve.ent.angles[0], ve.ent.angles[1], ve.ent.angles[2]};
            finite = finite && num(p) && num(a);
            trunk = za::max(trunk, geom::segmentDistance(p, k.pelvis, neck));
        };
        // (vrnotes1122a) Each sheath against the trunk: its plate's up along the trunk's (`tilt`, the worst, degrees) and
        // its plate facing out of the body (`facing`: its out from the trunk's line, level), as yours lie (holsterOnBody:
        // the angles' right is the plate's out, the left ones' its opposite, drawn mirrored); each gun hanging in its
        // sheath (`hang`: the farthest from it) and every piece's place and angles numbers (`finite`).
        const glm::vec3 trunkUp = glm::normalize(neck - k.pelvis);
        float tilt = 0.f, facing = 0.f, hang = 0.f;
        for(int i = 0; i < VrDrawnHolsters::count; i++)
        {
            if(shown(d.holster[i]))
            {
                holsters++;
                hands += view::handModel(d.holster[i].ent.model) ? 1 : 0;
                onTrunk(d.holster[i]);
            }
            if(shown(d.gear[i]))
            {
                sheaths++;
                onTrunk(d.gear[i]);
                glm::vec3 f, r, u;
                hands::angleVectors({-d.gear[i].ent.angles[0], d.gear[i].ent.angles[1], d.gear[i].ent.angles[2]}, f, r, u);
                const glm::vec3 p = at(d.gear[i]);
                glm::vec3 radial = p - geom::closestOnSegment(p, k.pelvis, neck);
                radial.z = 0.f;
                const glm::vec3 out = d.gear[i].mirrored ? -r : r;
                tilt = za::max(tilt, glm::degrees(za::acos(CLAMP(-1.f, glm::dot(u, trunkUp), 1.f))));
                if(glm::length(radial) > 1e-3f)
                {
                    facing = za::max(facing, glm::degrees(za::acos(CLAMP(-1.f, glm::dot(out, glm::normalize(radial)), 1.f))));
                }
                if(shown(d.holster[i]))
                {
                    hang = za::max(hang, glm::distance(at(d.holster[i]), p));
                }
            }
        }
        // (on the body: within half a metre of the trunk's line -- a hip holster stands ~0.2 m out, a held hand's
        // model origin a few units from the wrist its arm reached for; a sheath at most 70 degrees off the trunk's up --
        // the chest's lean back 20 and the uppers' turn 40 (vr_upper_holster_pitch), a player's copy ~50 -- and facing
        // within 60 of straight out (the hips turned 40 outwards); a gun within 0.25 m of its sheath)
        const float trunkMost = 0.5f * k.m2w, wristMost = 0.15f * k.m2w, hangMost = 0.25f * k.m2w;
        constexpr float tiltMost = 70.f, facingMost = 60.f;
        const bool turned = finite && tilt <= tiltMost && facing <= facingMost && hang <= hangMost;
        const bool ok = hands == 2 && trunk <= trunkMost && wrist <= wristMost && turned;
        const bool bot = ((avatars[e].hands[0].flags | avatars[e].hands[1].flags) & VrDrawnHand::solveBit) != 0;
        Con_Printf("mp_gear: ent=%d bot=%d hands=%d holsters=%d sheaths=%d trunk=%.1f trunkmost=%.1f wrist=%.1f wristmost=%.1f "
                   "finite=%d tilt=%.1f facing=%.1f hang=%.1f hangmost=%.1f turned=%d ok=%d\n",
            e, bot ? 1 : 0, hands, holsters, sheaths, static_cast<double>(trunk), static_cast<double>(trunkMost),
            static_cast<double>(wrist), static_cast<double>(wristMost), finite ? 1 : 0, static_cast<double>(tilt),
            static_cast<double>(facing), static_cast<double>(hang), static_cast<double>(hangMost), turned ? 1 : 0, ok ? 1 : 0);
        players++;
        bad += ok ? 0 : 1;
    }
    Con_Printf("mp_gearcheck: %d players, %d bad\n", players, bad);
}

void add(view::ViewEntity& ve)
{
    if(ve.visible && ve.ent.model && cl_numvisedicts < MAX_VISEDICTS)
    {
        cl_visedicts[cl_numvisedicts++] = &ve.ent;
    }
}

void setupBody(int slot, int e, const Avatar& a, const glm::vec3& origin, const avatar::DollPose* doll);
void drawPowerups(int slot, int powerups);

// A bot's holstered weapons and their sheaths (syncfix; his voice note 2026-11-21: "it should be possible to see the
// holsters of other players and bots ... so that I can see what they're actually carrying"): no client drew them, so they
// go on its body as last drawn (its skeleton), posed as yours are (view::holsterOnOther): the hips' outside the hip
// joints, the uppers' on the chest, the shoulders' behind them. None until its body is first posed.
void solveBotGear(const Drawn& d, float yaw, VrDrawnHolsters& hs, view::GearDraw gear[VrDrawnGear::count])
{
    if(!d.bodyPosed)
    {
        hs.weapons = 0;
        return;
    }
    const avatar::Skeleton& k = d.skeleton;
    // (vrnotes1122a; his voice note 2026-11-22: a bot's holsters "not properly rotated on its body") Turned with its
    // trunk as drawn (trunkYaw: its shoulders' line), the hips' and uppers' on the front of its belly and chest, each
    // facing the ring's surface there, as yours at their defaults (body::frontRingPlate). The hips stood beside the hip
    // joints facing straight out, the side the holster's frame takes for its outwards: no frame (NaN), the guns lost.
    const float bodyYaw = trunkYaw(k, yaw);
    glm::vec3 fwd, right, up;
    hands::angleVectors(glm::vec3{0.f, bodyYaw, 0.f}, fwd, right, up);
    const float m = k.m2w;
    const glm::vec3 neck = 0.5f * (k.shoulder[0] + k.shoulder[1]);
    const glm::vec3 chest = glm::mix(k.pelvis, neck, 0.55f);
    const glm::vec3 hips = (k.legs ? 0.5f * (k.hip[0] + k.hip[1]) : k.pelvis) - up * (0.04f * m);
    for(int h = 0; h < VrDrawnHolsters::count; h++)
    {
        const int side = h == body::LeftShoulder || h == body::LeftHip || h == body::LeftUpper ? 0 : 1;
        glm::vec3 pos, out = -fwd, surfaceUp = up;
        if(h == body::LeftShoulder || h == body::RightShoulder)
        {
            pos = k.shoulder[side] - fwd * (0.08f * m) - up * (0.1f * m);
        }
        else
        {
            const bool hip = h == body::LeftHip || h == body::RightHip;
            glm::vec2 at;
            const body::HolsterPlate plate = body::frontRingPlate(fwd, right, up, static_cast<body::Holster>(h), m, at);
            pos = (hip ? hips : chest) + fwd * at.x + right * at.y;
            out = plate.out;
            surfaceUp = plate.up;
        }
        view::GearDraw gun;
        const int model = (hs.weapons & (1 << h)) ? hs.model[h] : 0;
        view::holsterOnOther(h, pos, out, surfaceUp, bodyYaw, model > 0 && model < MAX_MODELS ? cl.model_precache[model] : nullptr,
            gear[h], gun);
        hs.origin[h] = gun.origin - glm::vec3{d.placedAt}; // (offsets from his entity's origin, as received)
        hs.angles[h] = gun.angles;
        hs.mirrored = static_cast<za::U8>((hs.mirrored & ~(1 << h)) | (gun.mirrored ? 1 << h : 0));
    }
}

// setup's hands, weapons, gear and torch (his body after, once his held hands are pinned: ropePinHands).
void setupHands(int slot, int e, const Avatar& a, const glm::vec3& origin, const avatar::DollPose* doll)
{
    Drawn& d = drawn.of[slot];
    d.frame = host_framecount;
    d.owner = e;
    d.placedAt = origin;
    // His hands, held and holstered weapons as his client drew them, by the routines that draw yours (view::drawHands).
    view::HandsDraw in;
    in.origin = origin;
    in.hands = a.hands;
    // A bot's (VrDrawnHand::solveBit: its controllers and weapons, no client of its own drew them): placed here by the
    // code that places yours (view::solveHands), then drawn as any player's (B11 part 3).
    VrDrawnHand solved[2];
    if((a.hands[0].flags | a.hands[1].flags) & VrDrawnHand::solveBit)
    {
        glm::vec3 pos[2], rot[2];
        int model[2], frame[2];
        VrDrawnHand placed[2]; // (bots3: each weapon where the server placed it, world)
        for(int h = 0; h < 2; h++)
        {
            pos[h] = origin + a.hands[h].handOrigin;
            rot[h] = a.hands[h].handAngles;
            model[h] = a.hands[h].weaponModel;
            frame[h] = a.hands[h].weaponFrame;
            placed[h] = a.hands[h];
            placed[h].weaponOrigin = origin + a.hands[h].weaponOrigin;
        }
        view::solveHands(e, pos, rot, model, frame, solved, placed);
        for(VrDrawnHand& h : solved)
        {
            h.handOrigin -= origin; // (offsets from his entity's origin, as received)
            h.weaponOrigin -= origin;
        }
        in.hands = solved;
    }
    in.holsters = &a.holsters;
    // His worn gear as he drew it; a bot's holstered weapons and sheaths placed on its body as drawn (syncfix).
    VrDrawnHolsters botHolsters;
    view::GearDraw gear[VrDrawnGear::count];
    if(a.gear.shown & VrDrawnGear::solveBit)
    {
        botHolsters = a.holsters;
        solveBotGear(d, a.body.yaw, botHolsters, gear);
        in.holsters = &botHolsters;
    }
    else
    {
        for(int i = 0; i < VrDrawnGear::count; i++)
        {
            if(a.gear.shown & (1 << i))
            {
                gear[i].model = view::gearModel(a.gear.model[i]);
                gear[i].origin = origin + a.gear.origin[i];
                gear[i].angles = a.gear.angles[i];
                gear[i].frame = a.gear.frame[i];
                gear[i].skin = a.gear.skin[i];
                gear[i].mirrored = (a.gear.mirrored & (1 << i)) != 0;
            }
        }
    }
    for(int i = 0; i < VrDrawnGear::count; i++)
    {
        view::drawGear(gear[i], d.gear[i]);
        const bool scaled = !(a.gear.shown & VrDrawnGear::solveBit) && a.gear.scale[i] > 0;
        d.gear[i].scale = glm::vec3{scaled ? static_cast<float>(a.gear.scale[i]) / 32.f : 1.f};
    }
    // The rounds in his ammo pouch (what the server says it holds) and his wrist gadget's screen glow, by your pouch's and
    // your gadget's code (syncfix2).
    const view::ViewEntity& ammoPouch = d.gear[VrDrawnGear::ammoPouch];
    d.pouchItems = view::posePouchItems(ammoPouch, (a.gear.shown & (1 << VrDrawnGear::ammoPouch)) != 0, a.gear.pouchKind,
        a.gear.pouchCount, d.pouchItem);
    const view::ViewEntity& gadgetEnt = d.gear[VrDrawnGear::gadget];
    if(!doll && gadgetEnt.visible && gadgetEnt.ent.model && !(a.gear.shown & VrDrawnGear::solveBit))
    {
        gadget::otherGlow(e, geom::vec(gadgetEnt.ent.origin), geom::vec(gadgetEnt.ent.angles), gadgetEnt.scale.x);
    }
    view::HandsEntities out;
    out.hand = d.hand;
    out.weapon = d.weapon;
    out.holster = d.holster;
    out.bones[0] = d.skin[0];
    out.bones[1] = d.skin[1];
    view::drawHands(in, out);
    // His guns as the server has them, drawn by the routines that draw yours (wpnsync): each hand's (its ammo screen within
    // the reach a lying gun's shows at), each holstered one's (his reload mode's bits: either hand's look carries them).
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const float reach = za::max(vr_weapon_world_attach_range.value, 0.f);
    for(int side = 0; side < 2; side++)
    {
        view::ViewEntity& w = d.weapon[side];
        const bool near = glm::distance(glm::vec3{w.ent.origin[0], w.ent.origin[1], w.ent.origin[2]}, eye) <= reach;
        view::drawGunParts(w, a.look[side], d.gun[side], true, near, doll ? 0 : e, side);
    }
    constexpr za::U8 modeBits = VrGunLook::magsShownBit | VrGunLook::clipFullBit | VrGunLook::ssgBreaksBit;
    for(int i = 0; i < VrDrawnHolsters::count; i++)
    {
        VrGunLook l;
        l.bits = static_cast<za::U8>(VrGunLook::gunBit | (a.look[1].bits & modeBits));
        l.flags = a.holsterFlags[i];
        l.clip = a.holsterClip[i];
        view::drawGunParts(d.holster[i], l, d.holsterGun[i], false, false);
    }
    if(doll)
    {
        d.lamp.visible = false; // (a dead body's torch: none; its lights are keyed by player, his living copy's)
    }
    else
    {
        flashlight::drawOther(e, a.lamp, origin, d.lamp);
    }
}

// (ropes4) A hand of his on a rope's hold (the server's, on a rope as this client draws it): where its palm's middle goes
// (on the rope as drawn this frame, by climb::ropePalm as yours: ropes5), world. False: not holding a rope with it.
[[nodiscard]] bool ropePalmTarget(const Avatar& a, const glm::vec3& origin, int side, glm::vec3& target)
{
    return (a.moves & (side ? avatarHoldMain : avatarHoldOff)) &&
           climb::ropePalm(origin + a.holdAt[side], a.holdOut[side], target, nullptr);
}

// (ropes4) His places (head, hands, gear) come from his client's origin, which on a rope (the server moves him with it)
// isn't where the server has him: his hands on the rope as his client drew them were 15-60 units off the server's holds
// here, his body and feet off the rope with them. Holding a rope, the whole copy is moved by his held palms' mean way to
// the server's holds (the trunk and feet go with it; ropeShiftMost at most), eased on and off over ropePinTime; `d`
// keeps each hand's palm from its place as last drawn. A hand gone hand over hand to a new hold (its place from his origin
// moved over ropeRegrab) eases there over ropePinTime, as yours does (climb::drawnHand), not in a message's step. Sets
// each hand's target (`d.ropeHeld`, `d.palmTarget`, world). Returns the move (units, world).
[[nodiscard]] glm::vec3 ropeShift(Drawn& d, const Avatar& a, const glm::vec3& origin, float dt, bool doll)
{
    glm::vec3 sum{0.f};
    int n = 0;
    for(int side = 0; side < 2; side++)
    {
        glm::vec3 target;
        d.ropeHeld[side] = !doll && ropePalmTarget(a, origin, side, target);
        if(!d.ropeHeld[side])
        {
            d.regrabAlong[side] = 1.f;
            d.holdWas[side] = glm::vec3{1e9f};
            continue;
        }
        const glm::vec3 off = target - origin; // (from his origin: the hold rides his swing)
        if(glm::distance(off, d.holdWas[side]) > ropeRegrab && d.holdWas[side].x < 1e8f)
        {
            d.regrabFrom[side] = glm::mix(d.regrabFrom[side], d.holdWas[side], geom::smooth01(d.regrabAlong[side]));
            d.regrabAlong[side] = 0.f;
        }
        d.holdWas[side] = off;
        d.regrabAlong[side] = za::min(1.f, d.regrabAlong[side] + dt / ropePinTime);
        d.palmTarget[side] = origin + glm::mix(d.regrabFrom[side], off, geom::smooth01(d.regrabAlong[side]));
        if(d.regrabAlong[side] >= 1.f)
        {
            d.regrabFrom[side] = off;
        }
        if(d.palmKnown[side])
        {
            sum += d.palmTarget[side] - (origin + a.hands[side].handOrigin + d.palmFromHand[side]);
            n++;
        }
    }
    if(n > 0)
    {
        d.shift = sum / static_cast<float>(n);
        if(const float l = glm::length(d.shift); l > ropeShiftMost)
        {
            d.shift *= ropeShiftMost / l;
        }
        d.shiftWeight = za::min(1.f, d.shiftWeight + dt / ropePinTime);
    }
    else
    {
        d.shiftWeight = za::max(0.f, d.shiftWeight - dt / ropePinTime);
    }
    return d.shift * geom::smooth01(d.shiftWeight);
}

// (ropes4) Each hand of his on a rope's hold, as drawn (moved by ropeShift): what is left of its palm's way to the hold
// (`origin`: his entity's, the server's), its hand and weapon moved by it, eased on and off as ropeShift; its palm from its
// place kept for ropeShift's next frame.
void ropePinHands(Drawn& d, const Avatar& a, const glm::vec3& origin, const glm::vec3& shift, float dt)
{
    for(int side = 0; side < 2; side++)
    {
        view::ViewEntity& h = d.hand[side];
        const bool held = d.ropeHeld[side] && h.visible && h.ent.model;
        d.palmKnown[side] = held;
        if(held)
        {
            const glm::vec3 palm = view::modelPoint(h, grasp::palmCentre());
            d.palmFromHand[side] = palm - (origin + shift + a.hands[side].handOrigin);
            d.handPin[side] = d.palmTarget[side] - palm;
            d.handPinWeight[side] = za::min(1.f, d.handPinWeight[side] + dt / ropePinTime);
        }
        else
        {
            d.handPinWeight[side] = za::max(0.f, d.handPinWeight[side] - dt / ropePinTime);
        }
        if(d.handPinWeight[side] > 0.f)
        {
            const glm::vec3 by = d.handPin[side] * geom::smooth01(d.handPinWeight[side]);
            for(view::ViewEntity* ve : {&h, &d.weapon[side]})
            {
                ve->ent.origin[0] += by.x;
                ve->ent.origin[1] += by.y;
                ve->ent.origin[2] += by.z;
            }
        }
    }
}

// Avatar `e`'s hands and weapons placed on his entity as drawn now at `origin` (its lerp: they ride it between messages).
// `doll`: his dead body's pose (addDolls), the hands at its wrists. Drawn in `slot` (his own, e; a dead body's: a doll
// slot, firstDollSlot on, whose keyed effects -- lights, glows -- are not his: a doll has none). On a rope, moved onto the
// server's holds (ropeShift, ropePinHands).
void setup(int slot, int e, const Avatar& a, const glm::vec3& origin, const avatar::DollPose* doll = nullptr)
{
    Drawn& d = drawn.of[slot];
    const float dt = d.pinTime >= 0.0 ? static_cast<float>(za::clamp(cl.time - d.pinTime, 0.0, 0.1)) : 0.f;
    d.pinTime = cl.time;
    const glm::vec3 shift = ropeShift(d, a, origin, dt, doll != nullptr);
    if(shift == glm::vec3{0.f})
    {
        setupHands(slot, e, a, origin, doll);
        ropePinHands(d, a, origin, shift, dt);
        setupBody(slot, e, a, origin, doll);
    }
    else
    {
        // (his places from the moved origin; the server's holds -- world points -- kept where they are)
        Avatar moved = a;
        for(glm::vec3& at : moved.holdAt)
        {
            at -= shift;
        }
        setupHands(slot, e, moved, origin + shift, doll);
        ropePinHands(d, a, origin, shift, dt);
        setupBody(slot, e, moved, origin + shift, doll);
    }
    drawPowerups(slot, doll ? 0 : a.powerups);
}

// Each of his entities as drawn (his copy's whole: hands, weapons and their parts, holsters, torch, body, pauldrons, plates).
template<typename F>
void forEachDrawn(Drawn& d, F&& f)
{
    for(int side = 0; side < 2; side++)
    {
        f(d.hand[side]);
        f(d.weapon[side]);
        f(d.pauldron[side]);
        f(d.pauldronArm[side]);
        for(view::ViewEntity& ve : d.gun[side].part)
        {
            f(ve);
        }
    }
    for(int i = 0; i < VrDrawnHolsters::count; i++)
    {
        f(d.holster[i]);
        for(view::ViewEntity& ve : d.holsterGun[i].part)
        {
            f(ve);
        }
    }
    f(d.lamp);
    for(view::ViewEntity& ve : d.gear)
    {
        f(ve);
    }
    for(view::ViewEntity& ve : d.pouchItem)
    {
        f(ve);
    }
    f(d.body);
    for(view::ViewEntity& ve : d.plate)
    {
        f(ve);
    }
    f(d.plateBridge);
}

// His power-ups on his copy as yours on you (syncfix; view::powerupLookOf, vr_body_powerups): the glow on his hands and
// body (powerupGlow, the shader's), its light from his hands, Quad's arcs on his forearms, the ring's fade over all of him.
// `slot`: his copy's (the lights' and arcs' key: a doll slot's past the players' lights, none).
void drawPowerups(int slot, int powerups)
{
    Drawn& d = drawn.of[slot];
    const int e = slot;
    d.powerups = view::powerupLookOf(powerups);
    const view::PowerupLook& look = d.powerups;
    if(look.alpha < 1.f)
    {
        forEachDrawn(d, [&](view::ViewEntity& ve) { ve.ent.alpha = ENTALPHA_ENCODE(ENTALPHA_DECODE(ve.ent.alpha) * look.alpha); });
    }
    glm::vec3 wrist[2], along[2];
    for(int side = 0; side < 2; side++)
    {
        const entity_t& h = d.hand[side].ent;
        wrist[side] = d.bodyPosed ? d.handWrist[side] : glm::vec3{h.origin[0], h.origin[1], h.origin[2]};
        glm::vec3 fwd, right, up;
        hands::angleVectors(glm::vec3{-h.angles[0], h.angles[1], h.angles[2]}, fwd, right, up);
        along[side] = fwd;
        const bool lit = look.shown > 0 && d.hand[side].visible;
        emissive::otherPowerupLight(e, side, wrist[side] + fwd * (0.1f * units::metresToUnits()), lit ? look.light * 0.8f : 0.f, 110.f, look.lightColor);
    }
    if(look.arcs && d.hand[0].visible && d.hand[1].visible)
    {
        view::otherQuadArcs(e, wrist, along);
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

// The avatar whose plates (or their joints' leather) `e` is (B6c), or nullptr; `index` its plates entity (-1 the leather).
[[nodiscard]] Drawn* platesOf(const entity_t* e, int& index)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&drawn.of[0]);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(drawn.of))
    {
        return nullptr;
    }
    Drawn& d = drawn.of[(p - first) / sizeof(Drawn)];
    for(index = -1; index < Drawn::plateCount; index++)
    {
        if(e == (index < 0 ? &d.plateBridge.ent : &d.plate[index].ent))
        {
            return &d;
        }
    }
    return nullptr;
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
    Calibration own; // this client's own, as its settings are now (addEntities: your dead body drawn as yours)
};
CalState cal;

// Player entity `e`'s calibration as this client has it: yours your own settings', another's as he sent it (null: none).
[[nodiscard]] const Calibration* calOf(int e)
{
    if(e > 0 && e == cl.viewentity)
    {
        return &cal.own;
    }
    return e > 0 && e <= MAX_SCOREBOARD && cal.has[e] ? &cal.of[e] : nullptr;
}

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
void setupBody(int slot, int e, const Avatar& a, const glm::vec3& origin, const avatar::DollPose* doll)
{
    Drawn& d = drawn.of[slot];
    d.bodyPosed = false;
    const Calibration* c = calOf(e);
    view::BodyDraw b;
    b.shown = vr_mp_body.value != 0.f && a.body.valid && !(c && c->v[calBody] == 0) && d.hand[0].visible && d.hand[1].visible;
    b.player = slot; // (the posed body's state's key, avatar::poseOther: his own, or a doll slot's)
    avatar::Subject who;
    who.doll = doll;
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
    else if(a.body.standingHeight > 0.5f)
    {
        // (no calibration: his standing eye height as his body pose has it -- a bot's, the default player's, the
        // server's: units::botEyeUnits)
        who.eyeHeight = za::min(a.body.standingHeight, calFields[calHeight].hi / 1000.f); // (mpfix C3: a guest's 30 m drew a giant)
        if((a.hands[0].flags | a.hands[1].flags) & VrDrawnHand::solveBit)
        {
            // (a bot, at the default player's world scale: his notes 2026-11-21, bots drawn at scale 1 looked small)
            who.metresToUnits = units::perMetre * units::defaultWorldScale();
        }
    }
    // His velocity from his entity's last two messages; on the ground unless rising or falling fast (the client is not
    // told; a walk down stairs stays on its feet).
    const entity_t& ent = cl_entities[e];
    const double dt = cl.mtime[0] - cl.mtime[1];
    const glm::vec3 moved{ent.msg_origins[0][0] - ent.msg_origins[1][0], ent.msg_origins[0][1] - ent.msg_origins[1][1],
        ent.msg_origins[0][2] - ent.msg_origins[1][2]};
    who.velocity = !doll && dt > 1e-4 && dt < 0.5 && glm::length(moved) < 64.f ? moved / static_cast<float>(dt) : glm::vec3{0.f};
    // (B5b: on the ground, lying down and in the water as the server has him -- his body lying, swimming as he does)
    who.onGround = (a.moves & avatarOnGround) != 0;
    who.lying = (a.moves & avatarLying) != 0;
    who.sliding = a.sliding; // (mpslide: his slide pose, the server's word, as his own client poses it)
    who.waterlevel = (a.moves >> avatarWaterShift) & 3;
    // (B5c: his holds, the server's -- his climbing trunk and feet)
    for(int h = 0; h < 2; h++)
    {
        who.holds[h] = (a.moves & (h ? avatarHoldMain : avatarHoldOff)) != 0;
        who.holdAt[h] = origin + a.holdAt[h];
        who.holdOut[h] = a.holdOut[h];
    }
    who.player = e;
    // (adspose: a flat-screen player's gun with a sight line as drawn here, the one nearest his head: on its sights his
    // drawn head goes behind the rear sight, the cheek on the stock -- avatar::solveTorso's seenAds)
    who.flat = a.flat;
    if(a.flat && !doll)
    {
        float best = 1e9f;
        for(int side = 0; side < 2; side++)
        {
            const view::ViewEntity& w = d.weapon[side];
            const sightalign::SightLine line = w.visible && w.ent.model ? sightalign::sightLine(w.ent.model) : sightalign::SightLine{};
            const glm::vec3 rear = line.valid ? view::modelPoint(w, line.rear) : glm::vec3{0.f};
            if(line.valid && glm::distance(rear, origin + a.head) < best)
            {
                best = glm::distance(rear, origin + a.head);
                who.sight = true;
                who.sightRear = rear;
                who.sightFront = view::modelPoint(w, line.front);
                who.sightHand = side == 0 ? HAND_OFF : HAND_MAIN;
            }
        }
    }

    hands::State& s = b.s;
    s.valid = true;
    s.head = origin + a.head;
    s.headAngles = a.headAngles;
    s.playerOrigin = origin;
    s.lean = glm::vec3{a.head.x, a.head.y, 0.f};
    s.bodyYaw = a.body.yaw;
    s.headHeight = za::clamp(a.body.headHeight, 0.f, calFields[calHeight].hi / 1000.f); // (mpfix C3)
    s.standingHeight = za::clamp(a.body.standingHeight, 0.f, calFields[calHeight].hi / 1000.f);
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
    // vr_player_plates: whether you see plates at all), coming in and going out by their morphs as yours do (B11c part 2:
    // his own morphs' state; taken as they come when his copy is first drawn). His dead body's: its own, his as he died
    // (vrnotes1122a: the server's word on the body's entity, d.doll; never his living plates again).
    view::BodyPlates& p = b.plates;
    const bool on = vr_player_plates.value != 0.f;
    const int worn = doll ? d.doll : e;
    const int type = on && worn > 0 ? za::max(agarmor::clientState(worn, 0), 0) : 0;
    const int pieces = type > 0 ? za::max(agarmor::clientState(worn, 1), 0) : 0;
    view::morphPlates(d.morph, on, type, pieces, vr_gametime, p);
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
    // (his dead body's: its plates' colour as he died -- the armour he wore then; his living gear byte is not its own)
    look.armour = doll ? (worn > 0 ? za::max(agarmor::clientState(worn, 0), 0) : 0) : a.armour;
    view::BodyEntities out;
    out.body = &d.body;
    out.plates = d.plate;
    out.numPlates = Drawn::plateCount;
    out.platePieces = d.entityPieces;
    out.bridge = &d.plateBridge;
    out.cap = d.pauldron;
    out.arm = d.pauldronArm;
    d.plateReport = -1;
    if(!view::drawBody(b, out) || !avatar::otherSkeleton(slot, d.skeleton))
    {
        return;
    }
    d.bodyPosed = true;
    d.handWrist[0] = poses[HAND_OFF].wrist;
    d.handWrist[1] = poses[HAND_MAIN].wrist;
    d.eyeHeight = who.eyeHeight;
    d.eyeUnits = who.eyeHeight * who.metresToUnits;
    d.seated = who.seated;
    for(int i = Drawn::plateCount - 1; i >= 0; i--)
    {
        d.plateReport = d.plate[i].visible ? i : d.plateReport;
        if((d.plate[i].visible || d.plateBridge.visible) && d.plateBones[i].empty())
        {
            d.plateBones[i].resize(agarmor::playerPlatesMostBones * 12, 0.f);
        }
    }
    if(d.plateBridge.visible && d.bridgeBones.empty())
    {
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
        c.v[i] = calClamp(i, static_cast<int>(za::lround(value * calFields[i].scale)));
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

bool serverWorldScale(int slot, float& scale)
{
    constexpr int scaleField = 3; // (calFields' "scale": thousandths)
    static_assert(scaleField < numCalFields);
    if(slot < 0 || slot >= MAX_SCOREBOARD || !cal.slotHas[slot] || calFields[scaleField].var != &vr_world_scale)
    {
        return false;
    }
    scale = static_cast<float>(cal.slot[slot].v[scaleField]) / calFields[scaleField].scale;
    return true;
}

bool serverBody(int slot, int& build, float& scale)
{
    if(slot < 0 || slot >= MAX_SCOREBOARD || vr_mp_body.value == 0.f)
    {
        return false;
    }
    // (single player: no calibration is sent, your own settings are the server's)
    const bool own = svs.maxclients <= 1;
    const Calibration c = own ? ownCalibration() : cal.slot[slot];
    if(!own && !cal.slotHas[slot])
    {
        // (none came: a bot, or a client that sends none -- the default build at the default player's height and world
        // scale, as setupBody draws one: units::botEyeUnits)
        build = view::BodyDraw{}.build;
        scale = units::defaultWorldScale();
        return true;
    }
    if(c.v[calBody] == 0)
    {
        return false; // (his body off: id's ranger, as his living copy is his player model)
    }
    build = za::clamp(c.v[calBuild], 0, 3);
    // (the body's size as setupBody draws it: his world scale's units a metre, his eyes' height over the model's; a
    // flat-screen player's his flat body height, as his living avatar is drawn: the server knows him flat from his moves)
    const bool flat = (own && flatplay::active()) || (sv.active && slot < svs.maxclients && hull::isFlat(EDICT_NUM(slot + 1)));
    const int height = c.v[flat ? calFlatHeight : calHeight];
    scale = (static_cast<float>(c.v[calScale]) / 1000.f) * (static_cast<float>(height) / 1000.f) / units::modelEyeHeight;
    scale = za::clamp(scale, 0.5f, 2.f);
    return true;
}

namespace
{

// Calibration `c`'s (null: none came, a bot's: the default body's) arms and shoulders on the doll rig `rig` (its rest
// units at size `scale` are world units): his upper arm's and forearm's drawn lengths over the rig's (setupBody's
// Subject, avatar::armBoneLength: measured, else the model's times his arm length, his tweaks on top), his shoulders'
// offset (metres of the model at his scale, as solveArm's clavicle: back, up, out) in its rest units.
[[nodiscard]] ragdoll::Proportions dollProportions(const Calibration* c, const ragdoll::Rig& rig, float scale)
{
    ragdoll::Proportions p;
    int upper = -1, fore = -1;
    for(int b = 0; b < rig.numBones; b++)
    {
        upper = ZA_STRCMP(rig.bones[b].name, "upperarm_l") == 0 ? b : upper;
        fore = ZA_STRCMP(rig.bones[b].name, "forearm_l") == 0 ? b : fore;
    }
    if(upper < 0 || fore < 0 || scale <= 0.f)
    {
        return p;
    }
    const float worldScale = c ? static_cast<float>(c->v[calScale]) / 1000.f : units::defaultWorldScale();
    const float cm = units::perMetre * worldScale * 0.01f;
    const float armLength = c ? static_cast<float>(c->v[calArm]) / 1000.f : 1.f;
    const int bones[2] = {upper, fore};
    float ratio[2];
    for(int k = 0; k < 2; k++)
    {
        const ragdoll::Bone& bone = rig.bones[bones[k]];
        const float modelLen = glm::distance(bone.pivot, bone.end) * scale;
        const float measured = c ? static_cast<float>(c->v[calUpper + k]) / 10.f : 0.f;
        const float tweak = c ? static_cast<float>(c->v[calTweakUpper + k]) / 10.f : 0.f;
        ratio[k] = modelLen > 1e-3f ? avatar::armBoneLength(modelLen, armLength, measured, tweak, cm) / modelLen : 1.f;
    }
    p.upper = ratio[0];
    p.fore = ratio[1];
    const glm::vec3 so = c ? glm::vec3{static_cast<float>(c->v[calShoulders]), static_cast<float>(c->v[calShoulders + 1]),
                                 static_cast<float>(c->v[calShoulders + 2])} /
                                 1000.f
                           : bodycal::defaultArms().shoulderOffset;
    p.shoulder = glm::vec3{-so.x, so.y, so.z} * units::perMetre; // (back -> forward)
    return p;
}

[[nodiscard]] bool isDollRig(const ragdoll::Rig* rig)
{
    return rig && rig->model && ZA_STRNCMP(rig->model->name, "progs/vrdoll", 12) == 0;
}

} // namespace

const ragdoll::Rig* serverDollRig(const ragdoll::Rig* rig, int owner, float scale)
{
    if(!isDollRig(rig) || owner < 1 || owner > MAX_SCOREBOARD)
    {
        return rig;
    }
    // (as serverBody: single player your own settings, a client his calibration, none (a bot) the default body)
    const int slot = owner - 1;
    const bool own = svs.maxclients <= 1;
    const Calibration c = own ? ownCalibration() : cal.slot[slot];
    const ragdoll::Proportions p = dollProportions(own || cal.slotHas[slot] ? &c : nullptr, *rig, scale);
    const ragdoll::Rig* out = ragdoll::proportioned(rig, p);
    Con_DPrintf("dollrig: owner %d, %s, scale %.3f: upper arm x%.3f, forearm x%.3f, shoulders %.2f %.2f %.2f%s\n", owner,
        rig->model->name, scale, p.upper, p.fore, p.shoulder.x, p.shoulder.y, p.shoulder.z, out == rig ? " (the model's own)" : "");
    return out;
}

const ragdoll::Rig* clientDollRig(const ragdoll::Rig* rig, const entity_t* e, float scale)
{
    if(!isDollRig(rig) || !e)
    {
        return rig;
    }
    int p = 0;
    for(int k = 0; k < cl.maxclients && k < MAX_SCOREBOARD && !p; k++)
    {
        p = e->colormap == cl.scores[k].translations ? k + 1 : 0;
    }
    return p > 0 ? ragdoll::proportioned(rig, dollProportions(calOf(p), *rig, scale)) : rig;
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
        for(int i = 0; i < numCalFields; i++)
        {
            c.v[i] = calClamp(i, MSG_ReadShort()); // (mpfix C4: the server's rule, here too: a 0 scale divided)
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
    const int gear = MSG_ReadByte();
    a.armour = gear & avatarGearArmour;
    a.sliding = (gear & avatarGearSliding) != 0;
    a.powerups = ((gear & avatarGearQuad) ? IT_QUAD : 0) | ((gear & avatarGearPent) ? IT_INVULNERABILITY : 0) |
                 ((gear & avatarGearRing) ? IT_INVISIBILITY : 0) | ((gear & avatarGearSuit) ? IT_SUIT : 0);
    // His guns as the server has them, his torch as he drew it (wpnsync).
    for(VrGunLook& l : a.look)
    {
        l = readGunLook();
    }
    for(int i = 0; i < VrDrawnHolsters::count; i++)
    {
        if(a.holsters.valid && (a.holsters.weapons & (1 << i)))
        {
            a.holsterFlags[i] = static_cast<za::U8>(MSG_ReadByte());
            a.holsterClip[i] = static_cast<za::U8>(MSG_ReadByte());
        }
    }
    a.lamp = readDrawnLamp(glm::vec3{0.f}); // (from his entity's origin)
    a.gear = readDrawnGear(glm::vec3{0.f}, a.body.valid ? a.body.yaw : 0.f, a.hands); // (from his entity's origin: syncfix)
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
    if((p.lamp.flags & VrDrawnLamp::placedBit) && (b.lamp.flags & VrDrawnLamp::placedBit)) // (his torch: wpnsync)
    {
        b.lamp.origin = glm::mix(p.lamp.origin, b.lamp.origin, t);
        b.lamp.rot = glm::slerp(p.lamp.rot, b.lamp.rot, t);
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

bool drawnNow(int ent)
{
    return ent > 0 && ent <= MAX_SCOREBOARD && drawn.of[ent].frame >= 0 && drawn.of[ent].frame >= host_framecount - 1;
}

bool drawnSkeleton(int ent, avatar::Skeleton& out)
{
    if(!drawnNow(ent) || !drawn.of[ent].bodyPosed)
    {
        return false;
    }
    out = drawn.of[ent].skeleton;
    return true;
}

bool woundTargets(int ent, entity_t* out[3], bool& doll)
{
    out[0] = out[1] = out[2] = nullptr;
    doll = false;
    if(ent <= 0 || ent > MAX_SCOREBOARD)
    {
        return false;
    }
    Drawn& d = drawn.of[ent];
    const bool now = drawnNow(ent);
    const auto pick = [&](view::ViewEntity& ve) { return !now || (ve.visible && ve.ent.model) ? &ve.ent : nullptr; };
    out[0] = pick(d.body);
    out[1] = pick(d.hand[0]);
    out[2] = pick(d.hand[1]);
    doll = now && d.doll != 0;
    return now;
}

int weaponOwner(const entity_t* e, int& side)
{
    bool hand = false;
    const Drawn* d = drawnOf(e, side, hand);
    return d && !hand ? d->owner : 0;
}

bool drawnWeapon(int ent, int side, glm::vec3& butt, glm::vec3& muzzle, glm::vec3& hand)
{
    if(!drawnNow(ent) || side < 0 || side > 1 || !drawnWeaponLine(drawn.of[ent].weapon[side], butt, muzzle))
    {
        return false;
    }
    const Drawn& d = drawn.of[ent];
    const view::ViewEntity& ve = d.hand[side].visible ? d.hand[side] : d.weapon[side];
    hand = glm::vec3{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
    return true;
}

const Avatar* of(int ent)
{
    return ent > 0 && ent <= MAX_SCOREBOARD && avatars[ent].valid ? &avatars[ent] : nullptr;
}

const view::ViewEntity* drawnWeapon(int ent, int hand)
{
    if(!of(ent) || hand < 0 || hand > 1)
    {
        return nullptr;
    }
    const Drawn& d = drawn.of[ent];
    const view::ViewEntity& w = d.weapon[hand];
    // (drawn in its parts -- the revolver open, his auto pump stroking (mpopen) -- the gun is hidden but placed still)
    return d.frame >= host_framecount - 1 && (w.visible || d.gun[hand].inParts) && w.ent.model ? &w : nullptr;
}

// Player `e`'s copy as set up this frame (setup) into the drawn entities: his hands, weapons, holsters, pauldrons, plates
// and body, the body in `replace`'s place in the list (his player model, or his dead body's: it would stand round it).
void addDrawn(int e, const entity_t* replace)
{
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
    // (his guns' parts and his torch: wpnsync)
    for(view::GunParts* g : {&d.gun[0], &d.gun[1]})
    {
        for(view::ViewEntity& ve : g->part)
        {
            add(ve);
        }
    }
    for(view::GunParts& g : d.holsterGun)
    {
        for(view::ViewEntity& ve : g.part)
        {
            add(ve);
        }
    }
    add(d.lamp);
    for(view::ViewEntity& ve : d.gear)
    {
        add(ve);
    }
    for(view::ViewEntity& ve : d.pouchItem)
    {
        add(ve);
    }
    for(view::ViewEntity& ve : d.plate)
    {
        add(ve);
    }
    add(d.plateBridge);
    if(d.body.visible && d.body.ent.model)
    {
        bool replaced = false;
        for(int i = 0; i < cl_numvisedicts && !replaced; i++)
        {
            replaced = cl_visedicts[i] == replace;
            cl_visedicts[i] = replaced ? &d.body.ent : cl_visedicts[i];
        }
        if(!replaced)
        {
            add(d.body);
        }
    }
}

// Rig bone `b` of the doll `e` (edict `num`) as drawn now: its ragdoll's part (a listen server's, single player's), else
// its frame's pose on the entity (Quake's alias turn: yaw, the pitch negated, roll). p_world = rot * p_rest + pos.
[[nodiscard]] bool dollBone(const entity_t* e, int num, qmodel_t* model, const ragdoll::Rig& rig, int b, glm::quat& rot,
    glm::vec3& pos, float& scale)
{
    if(ragdoll::drawnPart(num, b, rot, pos, scale))
    {
        return true;
    }
    glm::quat r;
    glm::vec3 t;
    ragdoll::bonePose(rig, ragdoll::poseOfFrame(model, e->frame), b, r, t);
    const glm::quat turn = glm::angleAxis(glm::radians(e->angles[1]), glm::vec3{0.f, 0.f, 1.f}) *
                           glm::angleAxis(glm::radians(-e->angles[0]), glm::vec3{0.f, 1.f, 0.f}) *
                           glm::angleAxis(glm::radians(e->angles[2]), glm::vec3{1.f, 0.f, 0.f});
    rot = turn * r;
    pos = glm::vec3{e->origin[0], e->origin[1], e->origin[2]} + turn * t;
    scale = 1.f;
    return true;
}

// The pose of the doll `e` (edict `num`, progs/player.mdl: its rig's bones by their names, player_bones.json) and the
// hands it is drawn with (at its wrists, open, along its forearms); false: no rig, or not the player's.
[[nodiscard]] bool dollPose(const entity_t* e, int num, qmodel_t* model, avatar::DollPose& out, VrDrawnHand hands[2])
{
    const ragdoll::Rig* rig = ragdoll::rigFor(model);
    {
        // (its ragdoll's own rig where it has one: a dead player's doll in his proportions, serverDollRig/clientDollRig)
        glm::quat q;
        glm::vec3 t;
        float s = 1.f;
        const ragdoll::Rig* drawnRig = nullptr;
        rig = ragdoll::drawnPart(num, 0, q, t, s, &drawnRig) && drawnRig ? drawnRig : rig;
    }
    if(!rig)
    {
        return false;
    }
    const char* const names[] = {"pelvis", "chest", "head", "upperarm_l", "forearm_l", "upperarm_r", "forearm_r", "thigh_l",
        "shin_l", "thigh_r", "shin_r"};
    constexpr int boneCount = static_cast<int>(sizeof(names) / sizeof(names[0]));
    glm::quat rot[boneCount];
    glm::vec3 pivot[boneCount], end[boneCount], rest[boneCount], restPivot[boneCount];
    for(int i = 0; i < boneCount; i++)
    {
        int b = 0;
        while(b < rig->numBones && ZA_STRCMP(rig->bones[b].name, names[i]) != 0)
        {
            b++;
        }
        glm::vec3 pos;
        float scale = 1.f;
        if(b >= rig->numBones || !dollBone(e, num, model, *rig, b, rot[i], pos, scale))
        {
            return false;
        }
        const ragdoll::Bone& bone = rig->bones[b];
        pivot[i] = rot[i] * (scale * bone.pivot) + pos;
        end[i] = rot[i] * (scale * bone.end) + pos;
        rest[i] = bone.end - bone.pivot;
        restPivot[i] = bone.pivot;
    }
    out.pelvis = pivot[0];
    out.pelvisRot = rot[0];
    out.pelvisRest = restPivot[1] - restPivot[0]; // (from it to the chest's joint, as the body's pelvis points to its spine)
    out.waist = pivot[1];
    out.neck = pivot[2];
    out.head = 0.5f * (pivot[2] + end[2]);
    out.chestRot = rot[1];
    out.headRot = rot[2];
    out.chestRest = rest[1];
    out.headRest = rest[2];
    for(int side = 0; side < 2; side++)
    {
        const int upper = 3 + 2 * side, fore = upper + 1, thigh = 7 + 2 * side, shin = thigh + 1;
        out.hip[side] = pivot[thigh];
        out.knee[side] = pivot[shin];
        out.ankle[side] = end[shin];
        out.shinRot[side] = rot[shin];
        out.shinRest[side] = rest[shin];
        // The hand (HAND_OFF the left) at the wrist, along the forearm, its back the forearm's turned rest up.
        VrDrawnHand& h = hands[side == 0 ? HAND_OFF : HAND_MAIN];
        h = VrDrawnHand{};
        h.flags = VrDrawnHand::handBit;
        h.handOrigin = end[fore];
        const glm::vec3 along = glm::normalize(end[fore] - pivot[upper] + 2.f * (end[fore] - pivot[fore]));
        glm::vec3 up = rot[fore] * glm::vec3{0.f, 0.f, 1.f};
        up = up - along * glm::dot(up, along);
        up = glm::length(up) > 1e-3f ? glm::normalize(up) : glm::vec3{0.f, 0.f, 1.f};
        h.handAngles = hands::anglesFromVectors(along, up);
    }
    return true;
}
// The dead players' bodies (QC vr_deathdoll.qc's player_body; Quake's body queue): progs/player.mdl in a player's
// colours (its colormap his; not his own entity). Each drawn as his own body (mpslide, 2026-11-20: they were id's ranger,
// not the body he is seen in): his build's model in his look, posed from the doll (Subject::doll), his jointed hands at
// its wrists; every body of his, his living copy drawn too or not (mpbugs4: a respawned player's old bodies were id's
// ranger; assignDollSlots: the nearest avatar::dollSlots past his own slot, the rest the ranger). For every viewer: a listen host and single player (its ragdoll), a client
// of another server (its death animation's frame), the dead player himself (his own settings; headless while the
// Immersive death view is in its head).
// A dead body addDolls draws: its entity, edict, model, owner, distance from the eyes and the slot it is drawn in.
struct DollBody
{
    entity_t* e{nullptr};
    int num{0};
    qmodel_t* model{nullptr};
    int owner{0};
    float dist{0.f};
    int slot{0};
};
constexpr int mostDollBodies = 64; // (the bodies looked at a frame: Quake's body queue holds 8)

// The slots of the bodies (nearest first): the owner's own while his copy is not drawn (he lies dead: the nearest of his
// bodies), else the doll slot it had (its pose's state goes on), else a free one, given afresh; 0: none left (id's ranger).
void assignDollSlots(DollBody* bodies, int count)
{
    bool ownTaken[MAX_SCOREBOARD + 1]{};
    bool slotTaken[avatar::dollSlots]{};
    for(int i = 0; i < count; i++)
    {
        DollBody& b = bodies[i];
        if(drawn.of[b.owner].frame != host_framecount && !ownTaken[b.owner])
        {
            ownTaken[b.owner] = true;
            b.slot = b.owner;
            continue;
        }
        for(int k = 0; k < avatar::dollSlots && !b.slot; k++)
        {
            if(!slotTaken[k] && drawn.dollEnt[k] == b.num)
            {
                slotTaken[k] = true;
                b.slot = firstDollSlot + k;
            }
        }
    }
    for(int i = 0; i < count; i++)
    {
        DollBody& b = bodies[i];
        for(int k = 0; k < avatar::dollSlots && !b.slot; k++)
        {
            if(!slotTaken[k])
            {
                slotTaken[k] = true;
                b.slot = firstDollSlot + k;
                drawn.dollEnt[k] = b.num;
                avatar::forgetOther(b.slot); // (another body's feet and eases: afresh)
                drawn.of[b.slot].morph.resync = true;
            }
        }
    }
}

void addDolls()
{
    if(vr_mp_body.value == 0.f)
    {
        return;
    }
    DollBody bodies[mostDollBodies];
    int count = 0;
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const int listed = cl_numvisedicts; // (the copies' own entities are added after them)
    for(int i = 0; i < listed && count < mostDollBodies; i++)
    {
        entity_t* e = cl_visedicts[i];
        if(e < cl_entities || e >= cl_entities + cl.num_entities || !e->model)
        {
            continue;
        }
        const int num = static_cast<int>(e - cl_entities);
        const qmodel_t* source = ragdoll::sourceModel(e); // (a ragdoll drawn with its skinned copy: its .mdl)
        qmodel_t* model = const_cast<qmodel_t*>(source ? source : e->model);
        if(ZA_STRCMP(model->name, "progs/player.mdl") != 0 && ZA_STRNCMP(model->name, "progs/vrdoll", 12) != 0)
        {
            continue; // (id's ranger, or his avatar's own ragdoll: avatardoll's progs/vrdoll*.mdl)
        }
        int p = 0;
        for(int k = 0; k < cl.maxclients && k < MAX_SCOREBOARD && !p; k++)
        {
            p = e->colormap == cl.scores[k].translations ? k + 1 : 0;
        }
        if(p <= 0 || p == num)
        {
            continue; // (not a player's colours, or his own entity)
        }
        // (nearest first: an insertion into the sorted list)
        DollBody b{e, num, model, p, glm::distance(glm::vec3{e->origin[0], e->origin[1], e->origin[2]}, eye), 0};
        int at = count++;
        for(; at > 0 && bodies[at - 1].dist > b.dist; at--)
        {
            bodies[at] = bodies[at - 1];
        }
        bodies[at] = b;
    }
    assignDollSlots(bodies, count);
    for(int i = 0; i < count; i++)
    {
        entity_t* e = bodies[i].e;
        const int num = bodies[i].num;
        qmodel_t* model = bodies[i].model;
        const int p = bodies[i].owner;
        const int slot = bodies[i].slot;
        if(!slot)
        {
            continue; // (every doll slot taken by nearer bodies)
        }
        avatar::DollPose pose;
        Avatar a;
        if(!dollPose(e, num, model, pose, a.hands))
        {
            continue;
        }
        pose.hideHead = ragdoll::headHidden(num);
        // His copy's avatar: the hands (from the doll's pelvis, as an avatar's from his entity), the chest's yaw.
        const Avatar& known = avatars[p];
        a.valid = true;
        a.flat = known.valid ? known.flat : p == cl.viewentity && flatplay::active();
        a.body.valid = true;
        const glm::vec3 fwd = glm::mat3_cast(pose.chestRot) * glm::vec3{1.f, 0.f, 0.f};
        a.body.yaw = glm::degrees(za::atan2(fwd.y, fwd.x));
        const glm::vec3 origin = pose.pelvis;
        a.head = pose.head - origin;
        for(VrDrawnHand& h : a.hands)
        {
            h.handOrigin -= origin;
        }
        Drawn& d = drawn.of[slot];
        if(d.frame < 0 || host_framecount - d.frame > freshAfterFrames)
        {
            d.morph.resync = true;
        }
        d.doll = num; // (before setup: setupBody reads the body's own plates)
        setup(slot, p, a, origin, &pose);
        addDrawn(slot, e);
    }
}

void addEntities()
{
    if(drawn.addedFrame == host_framecount)
    {
        return; // (the screen drawn again in a frame: added once)
    }
    drawn.addedFrame = host_framecount;
    if(cls.state != ca_connected || cl.intermission || vr_mp_avatar_draw.value == 0.f)
    {
        return;
    }
    cal.own = ownCalibration(); // (your dead body's: addDolls)
    for(int e = 1; cl.maxclients > 1 && e <= MAX_SCOREBOARD && e < cl.num_entities; e++)
    {
        const Avatar& a = avatars[e];
        // (his entity in the last message: in sight, alive or not; a stale avatar is not drawn)
        if(!a.valid || e == cl.viewentity || realtime - a.received > staleAfter || !cl_entities[e].model ||
            cl_entities[e].msgtime != cl.mtime[0])
        {
            continue;
        }
        Drawn& d = drawn.of[e];
        if(d.frame < 0 || host_framecount - d.frame > freshAfterFrames)
        {
            d.morph.resync = true; // (his copy first drawn, or again: his plates as they are, no morph)
        }
        glm::vec3 origin, angles;
        render::drawnPlace(cl_entities[e], origin, angles);
        d.doll = 0;
        setup(e, e, blendedOf(e), origin);
        // (the server's time it shows: blendedOf's -- the interpolation's, the lerp's, or the last message as it came)
        d.posedAt = vr_mp_avatar_lerp.value == 0.f ? a.msgTime
                                                    : za::min(netsnap::interpActive() ? netsnap::interpTime() : cl.time, a.msgTime);
        addDrawn(e, &cl_entities[e]); // (in his player model's place: it would stand round the body)
    }
    addDolls();
    // Their skin masks (a skin tone, gloves, nails: their own look, view::lookOf) found now, outside the draw loops.
    entity_t* shown[avatar::otherKeys * 3];
    int count = 0;
    for(int e = 1; e < avatar::otherKeys; e++) // (the doll slots too)
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

float powerupGlow(const entity_t* e)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&drawn.of[0]);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(drawn.of))
    {
        return 0.f;
    }
    const Drawn& d = drawn.of[(p - first) / sizeof(Drawn)];
    if(d.frame < 0 || d.powerups.level <= 0.f)
    {
        return 0.f;
    }
    if(e == &d.body.ent)
    {
        return view::powerupGlowCode(d.powerups, true);
    }
    return e == &d.hand[0].ent || e == &d.hand[1].ent ? view::powerupGlowCode(d.powerups, false) : 0.f;
}

int lookOf(const entity_t* e, view::SkinLook& look)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&drawn.of[0]);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(drawn.of))
    {
        return 0;
    }
    const Drawn& d = drawn.of[(p - first) / sizeof(Drawn)];
    const int owner = d.owner; // (a doll slot's: the dead body's owner)
    const int kind = e == &d.hand[0].ent || e == &d.hand[1].ent ? 1 : e == &d.body.ent ? 2 : 0;
    const Calibration* c = calOf(owner);
    if(!kind || !c)
    {
        return 0; // (a weapon; no calibration yet: drawn bare)
    }
    const int* v = c->v;
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
    int index = 0;
    const Drawn* p = platesOf(e, index);
    return p ? &p->body.ent : nullptr;
}

int bonePoses(const entity_t* e, const float** matrices)
{
    int index = 0;
    if(Drawn* p = platesOf(e, index); p && !(index < 0 ? p->bridgeBones : p->plateBones[index]).empty())
    {
        // His plates (settled, or a morph's) or their leather: his body's bones as drawn (B6c), by your plates' own code.
        const bool bridge = index < 0;
        const int n = view::otherPlateBonePoses(p->body.ent, bridge, bridge ? p->platePieces : p->entityPieces[index],
            bridge ? p->bridgeBones.data() : p->plateBones[index].data(), matrices);
        if(index == p->plateReport && matrices && *matrices)
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

const view::ViewEntity* find(const entity_t* e)
{
    const auto* first = reinterpret_cast<const unsigned char*>(&drawn.of[0]);
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    if(p < first || p >= first + sizeof(drawn.of))
    {
        return nullptr;
    }
    const Drawn& d = drawn.of[(p - first) / sizeof(Drawn)];
    const auto in = [e](const view::ViewEntity* ves, int n) -> const view::ViewEntity* {
        for(int i = 0; i < n; i++)
        {
            if(e == &ves[i].ent)
            {
                return &ves[i];
            }
        }
        return nullptr;
    };
    const view::ViewEntity* found = in(d.hand, 2);
    found = found ? found : in(d.weapon, 2);
    found = found ? found : in(d.holster, VrDrawnHolsters::count);
    for(int side = 0; side < 2 && !found; side++)
    {
        found = in(d.gun[side].part, view::GunParts::Count);
    }
    for(int i = 0; i < VrDrawnHolsters::count && !found; i++)
    {
        found = in(d.holsterGun[i].part, view::GunParts::Count);
    }
    found = found ? found : in(&d.lamp, 1);
    found = found ? found : in(d.pauldron, 2);
    found = found ? found : in(d.pauldronArm, 2);
    found = found ? found : in(d.plate, Drawn::plateCount);
    found = found ? found : in(&d.plateBridge, 1);
    return found ? found : in(&d.body, 1);
}

int gearByte(const edict_t* ent)
{
    const int items = static_cast<int>(ent->v.items);
    const int armour = ent->v.armorvalue <= 0.f ? 0 : (items & IT_ARMOR3) ? 3 : (items & IT_ARMOR2) ? 2 : (items & IT_ARMOR1) ? 1 : 0;
    const progs::ServerVm vm; // (slide's slot is by the server's edicts)
    const int powerups = ((items & IT_QUAD) ? avatarGearQuad : 0) | ((items & IT_INVULNERABILITY) ? avatarGearPent : 0) |
                         ((items & IT_INVISIBILITY) ? avatarGearRing : 0) | ((items & IT_SUIT) ? avatarGearSuit : 0);
    return armour | powerups | (slide::onServer(ent) ? avatarGearSliding : 0);
}

void clear()
{
    for(Avatar& a : avatars)
    {
        a = Avatar{};
    }
    for(Drawn& d : drawn.of)
    {
        d.morph = view::PlateMorphs{}; // (his plates as they come on the new server)
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

namespace
{
// Forgotten when the client's state is cleared (vr_mem.hpp's resets): the other players' avatars: the new server's come.
mem::Reset clientStateReset{"mpavatar", mem::ResetEvent::ClientState, mem::ResetPhase::State, &clear};
} // namespace

void init()
{
    Cmd_AddCommand("vr_mp_avatars", avatars_f);
    Cmd_AddCommand("vr_mp_gearcheck", gearcheck_f); // (mpbothands)
    Cmd_AddCommand_ClientCommand("vr_avatarcal", avatarcal_f);
}

} // namespace qvr::mpavatar

extern "C" void VR_AddMpAvatars(void)
{
    qvr::mpavatar::addEntities();
}
