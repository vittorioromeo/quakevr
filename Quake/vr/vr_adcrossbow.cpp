// vr_adcrossbow.cpp -- see vr_adcrossbow.hpp.

#include "vr_adcrossbow.hpp"

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_protocol.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <string.h>

namespace qvr::adcrossbow
{
namespace
{

// AD 1.80p1's progs/w_dcrossbow.mdl (its gib), turned (vr_adguns.cpp: front +x, the bow across y, up z; the tiller
// behind the bow the origin), as measured (scratch xb_an*.py): the string is vertices 218..235, a V from the limb tips
// (|y| 14.5, x 8.7..10) back to the latch (y 0, x 5.7..6.8); the limbs are everything wider than the stock (|y| over
// 2.6): they sweep back from the bow's root at the front (x ~20) to their tips (x ~8, |y| 14..17).
constexpr int modelVerts = 296, modelTris = 196;
constexpr int stringFirst = 218, stringLast = 235;
constexpr float stockHalfWidth = 2.6f; // |y| past it: a limb
constexpr float tipY = 14.5f;          // the string's ends on the limbs
constexpr float drawn = 3.0f;          // the string's middle this far behind its ends (cocked)

// The limbs' tips (at the string's ends) sprung forward, and the string's middle past the line between them, in the
// added frames (1 released, 2 the snap past it).
constexpr float tipForward[3] = {0.f, 2.f, 3.2f};
constexpr float stringBow[3] = {0.f, 0.f, 1.6f};

// The bolt in its groove: its back on the string at the latch, a little over the rail (z 3.43 - 0.1935 x: it falls
// towards the front, sightline_table.py), along it. proj_bolt1.mdl lies along +x, its back at -10, its tip at +10; drawn
// at boltScale, the size the crossbow knight's bolt is to his crossbow (ours: Scale 0.741), as the shot flies (QC
// VR_ADGun_FireCrossbow sets the projectile's scale the same, and launches it from this bolt's middle).
constexpr glm::vec3 nock{6.9f, 0.f, 3.43f - 0.1935f * 6.9f + 0.8f};
constexpr glm::vec3 railAhead{1.f, 0.f, -0.1935f};
constexpr glm::vec3 railUp{0.1935f, 0.f, 1.f};
constexpr float boltScale = 0.75f; // (12/16: exact as an entity's networked scale)
constexpr float boltBack = 10.f * boltScale;
constexpr const char* boltModelName = "progs/proj_bolt1.mdl";
constexpr const char* gunModelName = "progs/w_dcrossbow.mdl#gun0";

// A shot's look, in time: the string snaps forward (snapTime) to frame 2, the limbs wobble about frame 1 and settle;
// the string is drawn back (cock*, of the refire time) and the next bolt morphs in (bolt*).
constexpr float snapTime = 0.03f;
constexpr float wobbleDecay = 0.045f, wobblePeriod = 0.075f;
constexpr float cockFrom = 0.3f, cockTo = 0.65f, boltFrom = 0.55f, boltTo = 0.95f;
constexpr float morphKind = 1.f; // the seam's colour: the multi-rockets' (vr_view_weapon.cpp morphKind)

struct Hand
{
    double shotAt{-1.0};
    bool held{false}; // drawn holding the crossbow last frame
    glm::vec3 centre{0.f}, fwd{0.f}, up{0.f}; // its bolt's place last frame (drawn or not: vr_adgun_crossbow_info)
    glm::vec3 launch{0.f};                    // and its middle from the muzzle in the shot's frame (zero: not held)
    glm::vec3 firedCentre{0.f}, firedFwd{0.f}; // the bolt as the last shot left (vr_adgun_crossbow_info)
    bool boltIn{false};                        // its next bolt drawn (morphing in or there) last frame
};
Hand handState[2];

// The next bolt's shimmer as it morphs in (Misc/quakevr/make_adweapon_sounds.py; vr_adgun_crossbow_bolt_sound).
constexpr const char* boltSoundName = "vr/xbow_bolt_in.wav";
sfx_t* boltSound = nullptr;

// The model checked once: AD's crossbow with the added frames (another release, the stand-in: none).
struct Known
{
    const qmodel_t* model{nullptr};
    bool bow{false};
};
Known known;

[[nodiscard]] bool isBow(const qmodel_t* m)
{
    if(!m || m->type != mod_alias)
    {
        return false;
    }
    if(m != known.model)
    {
        known.model = m;
        known.bow = false;
        if(!strcmp(m->name, gunModelName))
        {
            const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(m)));
            known.bow = hdr && hdr->numframes == 1 + addedFrames && !strcmp(hdr->frames[1].name, frameNames[0]) &&
                        !strcmp(hdr->frames[2].name, frameNames[1]);
        }
    }
    return known.bow;
}

struct Look
{
    int a{0}, b{0};   // frames, blended
    float blend{0.f}; // of b
    float bolt{-1.f}; // the bolt's morph in (0..1, 1 there); -1 none
};

[[nodiscard]] float smooth(float x)
{
    x = za::clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

[[nodiscard]] Look look(float since, float refire, bool loaded)
{
    Look l;
    if(since < 0.f)
    {
        since = 1e9f;
    }
    if(since < snapTime)
    {
        const float s = 1.f - since / snapTime;
        l.a = 0;
        l.b = 2;
        l.blend = 1.f - s * s;
        return l;
    }
    const float cock0 = cockFrom * refire, cock1 = cockTo * refire;
    if(!loaded || since < cock0)
    {
        // Sprung: about frame 1, swinging to the snap (2) and back towards drawn (0) less, dying away.
        const float u = since - snapTime;
        const float o = za::exp(-u / wobbleDecay) * za::cos(u * (2.f * 3.14159265f / wobblePeriod));
        l.a = 1;
        l.b = o >= 0.f ? 2 : 0;
        l.blend = o >= 0.f ? o : -0.5f * o;
        return l;
    }
    l.a = 1;
    l.b = 0;
    l.blend = smooth((since - cock0) / za::max(cock1 - cock0, 1e-3f));
    const float bolt0 = boltFrom * refire, bolt1 = boltTo * refire;
    if(since >= bolt0)
    {
        l.bolt = since >= bolt1 ? 1.f : (since - bolt0) / za::max(bolt1 - bolt0, 1e-3f);
    }
    return l;
}

// The bolt's place on hand `hand`'s gun `w` (drawn): where its middle is, which way it points and its up. Its back on the
// string; along the shot (the way the projectile flies: the default hold's shot runs along the rail to 0.3 degrees; a
// hold re-posed off it shows the bolt off the rail rather than a shot leaving at another angle than the bolt lay).
void boltPose(int hand, const view::ViewEntity& w, glm::vec3& centre, glm::vec3& fwd, glm::vec3& up)
{
    const glm::vec3 at = view::modelPoint(w, nock);
    fwd = glm::normalize(view::modelPoint(w, nock + railAhead) - at + glm::vec3{1e-6f});
    const int slot = weapons::heldSlot(hand);
    if(slot >= 0)
    {
        glm::vec3 r, u;
        hands::angleVectors(weapons::shotAngles(hands::current().rot[hand], slot, hand == HAND_OFF), fwd, r, u);
    }
    up = view::modelPoint(w, nock + railUp) - at;
    up = glm::normalize(up - fwd * glm::dot(up, fwd) + glm::vec3{0.f, 0.f, 1e-6f});
    centre = at + fwd * boltBack;
}

void placeBolt(view::ViewEntity& b, const view::ViewEntity& w, qmodel_t* model, float t, const glm::vec3& centre,
    const glm::vec3& fwd, const glm::vec3& up)
{
    const glm::vec3 a = hands::anglesFromVectors(fwd, up);
    entity_t& e = b.ent;
    if(model != b.lastModel)
    {
        e.lerpflags |= LERP_RESETANIM;
        b.lastModel = model;
    }
    e.model = model;
    e.frame = 0;
    e.skinnum = 0;
    e.colormap = vid.colormap;
    e.alpha = w.ent.alpha;
    e.scale = ENTSCALE_DEFAULT; // (the size: b.scale)
    e.origin[0] = centre.x;
    e.origin[1] = centre.y;
    e.origin[2] = centre.z;
    e.angles[0] = -a.x;
    e.angles[1] = a.y;
    e.angles[2] = a.z;
    b.scale = glm::vec3{boltScale}; // the shot that leaves is this bolt
    b.mirrored = false;
    b.zeroBlend = 0.f;
    b.lightMultiply = false;
    b.morph = t < 1.f ? 2.f * morphKind + za::clamp(t, 1e-3f, 1.f) : 0.f;
    b.visible = true;
}

[[nodiscard]] int clipOf(int hand)
{
    return cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONCLIP : protocol::STAT_QVR_WEAPONCLIP2];
}

void info_f()
{
    hands::State& s = hands::current();
    for(int hand = 0; hand < 2; hand++)
    {
        if(!handState[hand].held)
        {
            Con_Printf("crossbow hand %d: not held (or no bow frames)\n", hand);
            continue;
        }
        const Hand& h = handState[hand];
        Con_Printf("crossbow hand %d: shot %.3f s ago, clip %d\n", hand, h.shotAt < 0.0 ? -1.0 : cl.time - h.shotAt,
            clipOf(hand));
        if(!s.muzzleValid[hand])
        {
            continue;
        }
        // The bolt's middle from the muzzle, in the shot's frame (sent: QC VR_ADGun_FireCrossbow launches from there).
        glm::vec3 f, r, u;
        hands::angleVectors(weapons::shotAngles(s.rot[hand], weapons::heldSlot(hand), hand == HAND_OFF), f, r, u);
        Con_Printf("  bolt middle %.2f %.2f %.2f, muzzle %.2f %.2f %.2f: from the muzzle %.3f forward %.3f right %.3f up; "
                   "bolt to shot %.2f deg\n",
            h.centre.x, h.centre.y, h.centre.z, s.muzzle[hand].x, s.muzzle[hand].y, s.muzzle[hand].z, h.launch.x,
            h.launch.y, h.launch.z, glm::degrees(za::acos(za::clamp(glm::dot(h.fwd, f), -1.f, 1.f))));
        if(h.shotAt >= 0.0)
        {
            Con_Printf("  last shot: bolt middle %.2f %.2f %.2f, way %.4f %.4f %.4f\n", h.firedCentre.x, h.firedCentre.y,
                h.firedCentre.z, h.firedFwd.x, h.firedFwd.y, h.firedFwd.z);
        }
    }
}

// The bolt's middle from the hand's muzzle, in its shot's frame (forward, right, up).
[[nodiscard]] glm::vec3 offsetFromMuzzle(int hand, const glm::vec3& centre)
{
    hands::State& s = hands::current();
    if(!s.muzzleValid[hand])
    {
        return glm::vec3{0.f};
    }
    glm::vec3 f, r, u;
    hands::angleVectors(weapons::shotAngles(s.rot[hand], weapons::heldSlot(hand), hand == HAND_OFF), f, r, u);
    const glm::vec3 d = centre - s.muzzle[hand];
    return {glm::dot(d, f), glm::dot(d, r), glm::dot(d, u)};
}

} // namespace

bool knownModel(int numVerts, int numTris)
{
    return numVerts == modelVerts && numTris == modelTris;
}

glm::vec3 framePoint(int frame, int v, const glm::vec3& p)
{
    frame = za::clamp(frame, 0, addedFrames);
    const float ay = za::abs(p.y);
    glm::vec3 q = p;
    if(v >= stringFirst && v <= stringLast)
    {
        // The string: its ends with the limb tips, its middle straightened to the line between them (and past it).
        const float u = za::clamp(ay / tipY, 0.f, 1.f);
        q.x += tipForward[frame] + (1.f - u) * (drawn + stringBow[frame]);
        return q;
    }
    if(ay > stockHalfWidth)
    {
        // A limb, bent: its tip forward most, its root not at all.
        const float k = (ay - stockHalfWidth) / (tipY - stockHalfWidth);
        q.x += tipForward[frame] * k * k;
    }
    return q;
}

void fired(int hand)
{
    if(hand >= 0 && hand < 2 && handState[hand].held)
    {
        handState[hand].shotAt = cl.time;
        handState[hand].firedCentre = handState[hand].centre;
        handState[hand].firedFwd = handState[hand].fwd;
    }
}

glm::vec3 launchOffset(int hand)
{
    return hand >= 0 && hand < 2 && handState[hand].held ? handState[hand].launch : glm::vec3{0.f};
}

void frame(view::ViewEntity (&weapons)[2], view::ViewEntity (&bolts)[2])
{
    qmodel_t* boltModel = nullptr;
    for(int hand = 0; hand < 2; hand++)
    {
        view::ViewEntity& w = weapons[hand];
        view::ViewEntity& b = bolts[hand];
        b.visible = false;
        w.poseFrame[0] = w.poseFrame[1] = -1;
        Hand& h = handState[hand];
        const bool bow = w.visible && isBow(w.ent.model);
        if(!bow || vr_adgun_crossbow_anim.value == 0.f)
        {
            h.held = false;
            h.shotAt = -1.0;
            if(bow)
            {
                w.poseFrame[0] = w.poseFrame[1] = 0; // (as AD's: the shot's weapon frames never show the added ones)
                w.poseBlend = 0.f;
            }
            continue;
        }
        h.held = true;
        const float refire = za::max(vr_adgun_crossbow_refire.value, 0.05f);
        float since = h.shotAt < 0.0 ? -1.f : static_cast<float>(cl.time - h.shotAt);
        bool loaded = clipOf(hand) > 0;
        if(vr_adgun_crossbow_anim_at.value >= 0.f)
        {
            since = vr_adgun_crossbow_anim_at.value;
            loaded = true;
        }
        const Look l = look(since, refire, loaded);
        // The next bolt's coming: a soft shimmer as it starts to morph in (his notes 2026-10-19), from the groove.
        const bool boltIn = l.bolt >= 0.f;
        if(boltIn && !h.boltIn && h.shotAt >= 0.0)
        {
            const float volume = za::clamp(vr_adgun_crossbow_bolt_sound.value, 0.f, 1.f);
            if(boltSound && volume > 0.f)
            {
                vec3_t org{h.centre.x, h.centre.y, h.centre.z};
                S_StartSound(0, 0, boltSound, org, volume, 2.f);
            }
            if(vr_debug_weaponfx.value)
            {
                Con_Printf("crossbow hand %d: bolt morphs in %.3f s after the shot (%s, volume %.2f)\n", hand, since,
                    boltSound ? boltSoundName : "no sound system", volume);
            }
        }
        h.boltIn = boltIn;
        w.poseFrame[0] = l.a;
        w.poseFrame[1] = l.b;
        w.poseBlend = l.blend;
        w.zeroBlend = 0.f; // (the slot's blend towards frame 0 would damp the string)
        boltPose(hand, w, h.centre, h.fwd, h.up);
        h.launch = offsetFromMuzzle(hand, h.centre);
        if(l.bolt < 0.f)
        {
            continue;
        }
        boltModel = boltModel ? boltModel : view::viewModel(boltModelName);
        if(boltModel && boltModel->type == mod_alias)
        {
            placeBolt(b, w, boltModel, l.bolt, h.centre, h.fwd, h.up);
        }
    }
}

void clear()
{
    for(Hand& h : handState)
    {
        h = Hand{};
    }
    known = Known{};
}

void registerCommands()
{
    Cmd_AddCommand("vr_adgun_crossbow_info", info_f);
}

void prepare()
{
    boltSound = S_PrecacheSound(boltSoundName);
}

} // namespace qvr::adcrossbow

// The renderer's frame blending of a view entity drawn as two of its frames blended (ViewEntity::poseFrame: the AD
// crossbow's string and limbs, vr_adcrossbow.cpp), after its own (R_SetupAliasFrame).
extern "C" void VR_AliasPoseOverride(const entity_t* e, const void* aliashdr, short* pose1, short* pose2, float* blend)
{
    const qvr::view::ViewEntity* ve = qvr::view::find(e);
    if(!ve || ve->poseFrame[0] < 0)
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const int a = ve->poseFrame[0], b = ve->poseFrame[1];
    if(a >= hdr->numframes || b < 0 || b >= hdr->numframes)
    {
        return;
    }
    *pose1 = static_cast<short>(hdr->frames[a].firstpose);
    *pose2 = static_cast<short>(hdr->frames[b].firstpose);
    *blend = za::clamp(ve->poseBlend, 0.f, 1.f);
}
