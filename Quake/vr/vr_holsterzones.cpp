// vr_holsterzones.cpp -- see vr_holsterzones.hpp.

#include "vr_holsterzones.hpp"
#include "vr_backend.hpp"
#include "vr_bodyclone.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/Exchange.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_zancle.hpp"

namespace qvr::zones
{
namespace
{

constexpr int MAX_SAMPLES = 6;

// What vr_view.cpp drew last for each zone (publish): its frame, the model's origin in it from the pivot.
struct Drawn
{
    glm::vec3 out{1.f, 0.f, 0.f}, side{0.f, -1.f, 0.f}, up{0.f, 0.f, 1.f};
    glm::vec3 atLocal{0.f}; // the model's origin from the pivot, in the frame (out, side, up)
    bool occupied{false};
    int frame{-100}; // host_framecount when published
    glm::vec3 pivot{0.f};     // the pivot it was published with
    glm::vec3 gripLocal{0.f}; // the holstered gun's grip as drawn (its model's origin), from the pivot in the frame
    int gripFrame{-100};      // host_framecount when published (publishGrip)
};

// Each hand's last pick (the hysteresis, the wireframes, the report): its points, how deep each was in each zone.
struct HandPick
{
    int zone{-1};
    bool gun{false};
    bool item{false}; // carrying a thing to put away (STAT_QVR_CARRYITEM): the zones vr_holster_zone_item_* larger
    int samples{0};
    glm::vec3 sample[MAX_SAMPLES]{};
    float ratio[ZoneCount]{};
    int frame{-100};
};

// A hand dragging a zone (vr_holster_zone_<kind>_drag): where it took hold, in the body's frame from the zone's pivot
// (forward, outwards, up), and the kind's offsets then; the second hand on the same kind resizes it instead (the hands'
// spread then, and the kind's depth, width and height).
struct Drag
{
    int zone{-1};
    bool resize{false};
    glm::vec3 handStart{0.f};
    glm::vec3 offsetStart{0.f};
    float spreadStart{1.f};
    glm::vec3 sizeStart{1.f};
};

struct State
{
    Drawn drawn[ZoneCount];
    HandPick hands[HAND_COUNT];
    Drag drag[HAND_COUNT];
    // This frame's wireframes also go on the body's mirror image (vr_bodyclone.cpp) through this, while it is drawn.
    bool cloned{false};
    glm::mat4 mirror{1.f};
};
State zoneState;

// A zone's box: its middle, axes (out, side, up) and half sizes.
struct Box
{
    glm::vec3 centre{0.f};
    glm::vec3 axis[3]{};
    glm::vec3 half{1.f};
};

// A kind's settings: hip, upper, shoulder, ammo pouch, grenade pouch.
struct Kind
{
    cvar_t *x, *y, *z, *depth, *width, *height, *reach, *drag;
};

[[nodiscard]] Kind kindOf(int zone)
{
    switch(zone)
    {
        case body::LeftHip:
        case body::RightHip:
            return {&vr_holster_zone_hip_x, &vr_holster_zone_hip_y, &vr_holster_zone_hip_z, &vr_holster_zone_hip_depth,
                &vr_holster_zone_hip_width, &vr_holster_zone_hip_height, &vr_hip_holster_thresh,
                &vr_holster_zone_hip_drag};
        case body::LeftUpper:
        case body::RightUpper:
            return {&vr_holster_zone_upper_x, &vr_holster_zone_upper_y, &vr_holster_zone_upper_z,
                &vr_holster_zone_upper_depth, &vr_holster_zone_upper_width, &vr_holster_zone_upper_height,
                &vr_upper_holster_thresh, &vr_holster_zone_upper_drag};
        case body::LeftShoulder:
        case body::RightShoulder:
            return {&vr_holster_zone_shoulder_x, &vr_holster_zone_shoulder_y, &vr_holster_zone_shoulder_z,
                &vr_holster_zone_shoulder_depth, &vr_holster_zone_shoulder_width, &vr_holster_zone_shoulder_height,
                &vr_shoulder_holster_thresh, &vr_holster_zone_shoulder_drag};
        case ZoneAmmoPouch:
            return {&vr_holster_zone_ammo_x, &vr_holster_zone_ammo_y, &vr_holster_zone_ammo_z,
                &vr_holster_zone_ammo_depth, &vr_holster_zone_ammo_width, &vr_holster_zone_ammo_height,
                &vr_ammo_pouch_thresh, &vr_holster_zone_ammo_drag};
        default:
            return {&vr_holster_zone_grenade_x, &vr_holster_zone_grenade_y, &vr_holster_zone_grenade_z,
                &vr_holster_zone_grenade_depth, &vr_holster_zone_grenade_width, &vr_holster_zone_grenade_height,
                &vr_grenade_pouch_thresh, &vr_holster_zone_grenade_drag};
    }
}

// The kind a zone is of (its mirrored pair shares its settings): 0 hips, 1 chest, 2 shoulders, 3 ammo pouch, 4 grenade
// pouch.
[[nodiscard]] int kindIndex(int zone)
{
    switch(zone)
    {
        case body::LeftHip:
        case body::RightHip: return 0;
        case body::LeftUpper:
        case body::RightUpper: return 1;
        case body::LeftShoulder:
        case body::RightShoulder: return 2;
        case ZoneAmmoPouch: return 3;
        default: return 4;
    }
}

[[nodiscard]] bool leftSide(int zone)
{
    return zone == body::LeftHip || zone == body::LeftUpper || zone == body::LeftShoulder;
}

// A zone's frame and its model's origin as drawn (publish), else the body's frame at the pivot.
void frameOf(const hands::State& s, int zone, const glm::vec3& pivot, glm::vec3& out, glm::vec3& side, glm::vec3& up,
    glm::vec3& at)
{
    const Drawn& d = zoneState.drawn[zone];
    if(d.frame >= host_framecount - 2)
    {
        out = d.out;
        side = d.side;
        up = d.up;
        at = pivot + out * d.atLocal.x + side * d.atLocal.y + up * d.atLocal.z;
    }
    else
    {
        // Not drawn (dead, hidden): the body's frame at the reach point, out of the body (the shoulders' and the grenade
        // pouch's behind it).
        glm::vec3 fwd, right, u;
        hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, u);
        const bool back = zone == body::LeftShoulder || zone == body::RightShoulder || zone == ZoneGrenadePouch;
        out = back ? -fwd : fwd;
        side = leftSide(zone) ? -right : right;
        up = u;
        at = pivot;
    }
}

// A hand carrying an ammo box, a health box or a backpack: its zones' size, times (vr_holster_zone_item_*).
[[nodiscard]] float itemScale(int zone)
{
    const cvar_t& c = zone == body::LeftHip || zone == body::RightHip           ? vr_holster_zone_item_hip
                      : zone == body::LeftUpper || zone == body::RightUpper     ? vr_holster_zone_item_upper
                      : zone == body::LeftShoulder || zone == body::RightShoulder ? vr_holster_zone_item_shoulder
                      : zone == ZoneAmmoPouch                                   ? vr_holster_zone_item_ammo
                                                                                : vr_holster_zone_item_grenade;
    return za::max(0.05f, c.value);
}

[[nodiscard]] bool carriesItem(int hand)
{
    return (cl.stats[protocol::STAT_QVR_CARRYITEM] & (1 << hand)) != 0; // (bit 0 the off hand, HAND_OFF)
}

// The holstered gun's grip as drawn (publishGrip), placed with its holster's frame now; false: none drawn there.
[[nodiscard]] bool gripOf(const hands::State& s, int zone, const glm::vec3& pivot, glm::vec3& grip)
{
    if(zone >= body::HolsterCount || zoneState.drawn[zone].gripFrame < host_framecount - 2)
    {
        return false;
    }
    glm::vec3 out, side, up, at;
    frameOf(s, zone, pivot, out, side, up, at);
    const glm::vec3& g = zoneState.drawn[zone].gripLocal;
    grip = pivot + out * g.x + side * g.y + up * g.z;
    return true;
}

// `scale`: the box's size, times, about its middle (a hand carrying an item: itemScale).
[[nodiscard]] Box zoneBox(const hands::State& s, int zone, const glm::vec3& pivot, float scale = 1.f)
{
    glm::vec3 out, side, up, at;
    frameOf(s, zone, pivot, out, side, up, at);
    const Kind k = kindOf(zone);
    const float reach = za::max(0.f, k.reach->value) * za::max(0.05f, vr_holster_zone_scale.value);
    Box b;
    b.axis[0] = out;
    b.axis[1] = side;
    b.axis[2] = up;
    // Round the holster as drawn and its reach point both: the box stretched from one to the other (the hips' are drawn a
    // hand's width in from their reach points, towards the ammo pouch: the old spheres missed half of what was seen).
    const glm::vec3 apart = at - pivot;
    const glm::vec3 stretch{za::abs(glm::dot(apart, out)), za::abs(glm::dot(apart, side)), za::abs(glm::dot(apart, up))};
    // Moved in the body's frame (as the holsters' own offsets): forward, outwards (the pouches: right), up.
    glm::vec3 fwd, right, bodyUp;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, bodyUp);
    const glm::vec3 outwards = leftSide(zone) ? -right : right;
    b.centre = (at + pivot) * 0.5f + fwd * k.x->value + outwards * k.y->value + bodyUp * k.z->value;
    b.half = (glm::max(glm::vec3{0.25f}, reach * glm::vec3{k.depth->value, k.width->value, k.height->value}) +
                 stretch * 0.5f) *
             scale;
    return b;
}

// How deep `p` is in `b`: 0 at its middle, 1 on its surface, more outside (the largest of its axes' shares).
[[nodiscard]] float ratioIn(const Box& b, const glm::vec3& p)
{
    const glm::vec3 d = p - b.centre;
    float r = 0.f;
    for(int i = 0; i < 3; i++)
    {
        r = za::max(r, za::abs(glm::dot(d, b.axis[i])) / b.half[i]);
    }
    return r;
}

[[nodiscard]] bool holdsGun(int hand)
{
    return cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2] != 0; // (QC's WID_FIST)
}

// The pivots of every zone present this frame (false: absent).
[[nodiscard]] bool pivotOf(int zone, const body::HolsterPositions& holsters, const glm::vec3* grenadePouch,
    const glm::vec3* ammoPouch, glm::vec3& out)
{
    if(zone < body::HolsterCount)
    {
        out = holsters[zone];
        return true;
    }
    const glm::vec3* p = zone == ZoneAmmoPouch ? ammoPouch : grenadePouch;
    if(p)
    {
        out = *p;
    }
    return p != nullptr;
}

// A wireframe's line and dot: on the body, and on its mirror image while it is drawn (State::cloned).
void line(const glm::vec3& a, const glm::vec3& b, float width, const glm::vec4& color)
{
    lines::line(a, b, width, color, color);
    if(zoneState.cloned)
    {
        lines::line(glm::vec3{zoneState.mirror * glm::vec4{a, 1.f}}, glm::vec3{zoneState.mirror * glm::vec4{b, 1.f}},
            width, color, color);
    }
}

void dot(const glm::vec3& p, float size, const glm::vec4& color)
{
    lines::point(p, size, color);
    if(zoneState.cloned)
    {
        lines::point(glm::vec3{zoneState.mirror * glm::vec4{p, 1.f}}, size, color);
    }
}

void edges(const Box& b, const glm::vec4& color, float width)
{
    glm::vec3 c[8];
    for(int i = 0; i < 8; i++)
    {
        c[i] = b.centre + b.axis[0] * (i & 1 ? b.half.x : -b.half.x) + b.axis[1] * (i & 2 ? b.half.y : -b.half.y) +
               b.axis[2] * (i & 4 ? b.half.z : -b.half.z);
    }
    for(int i = 0; i < 8; i++)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            if(!(i & bit))
            {
                line(c[i], c[i | bit], width, color);
            }
        }
    }
}

// Zone `zone`'s pivot this frame (false: a pouch not there).
[[nodiscard]] bool pivotNow(const hands::State& s, int zone, glm::vec3& out)
{
    if(zone < body::HolsterCount)
    {
        out = body::holsterPositions(s)[zone];
        return true;
    }
    if(zone == ZoneAmmoPouch ? !body::ammoPouchEnabled() : !body::pouchEnabled())
    {
        return false;
    }
    out = zone == ZoneAmmoPouch ? body::ammoPouchPosition(s) : body::pouchPosition(s);
    return true;
}

// `p` from zone `zone`'s pivot in the body's frame as its offsets are (forward, outwards: mirrored for the left ones,
// up).
[[nodiscard]] glm::vec3 bodyLocal(const hands::State& s, int zone, const glm::vec3& pivot, const glm::vec3& p)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
    const glm::vec3 d = p - pivot;
    return {glm::dot(d, fwd), glm::dot(d, leftSide(zone) ? -right : right), glm::dot(d, up)};
}

[[nodiscard]] bool anyDrag()
{
    return vr_holster_zone_hip_drag.value != 0.f || vr_holster_zone_upper_drag.value != 0.f ||
           vr_holster_zone_shoulder_drag.value != 0.f || vr_holster_zone_ammo_drag.value != 0.f ||
           vr_holster_zone_grenade_drag.value != 0.f;
}

void dragHaptic(int hand)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value)
    {
        be->haptic(hand, 0.03f, 200.f, 0.3f);
    }
}

// A dragged value, to `step` and within the menu's extended range; set only when it changes.
void setSnapped(cvar_t* c, float v, float step, float lo, float hi)
{
    v = za::clamp(std::round(v / step) * step, lo, hi);
    if(c->value != v)
    {
        Cvar_SetValueQuick(c, v);
    }
}

constexpr const char* zoneNames[ZoneCount] = {
    "lshoulder", "rshoulder", "lhip", "rhip", "lupper", "rupper", "ammopouch", "grenadepouch"};

} // namespace

void publish(int zone, const glm::vec3& pivot, const glm::vec3& out, const glm::vec3& side, const glm::vec3& up,
    const glm::vec3& at, bool occupied)
{
    if(zone < 0 || zone >= ZoneCount)
    {
        return;
    }
    Drawn& d = zoneState.drawn[zone];
    d.out = out;
    d.side = side;
    d.up = up;
    const glm::vec3 v = at - pivot;
    d.atLocal = {glm::dot(v, out), glm::dot(v, side), glm::dot(v, up)};
    d.occupied = occupied;
    d.pivot = pivot;
    d.frame = host_framecount;
}

void publishGrip(int zone, const glm::vec3& grip)
{
    if(zone < 0 || zone >= body::HolsterCount)
    {
        return;
    }
    Drawn& d = zoneState.drawn[zone];
    if(d.frame != host_framecount)
    {
        return; // (after its publish: the same pivot and frame)
    }
    const glm::vec3 v = grip - d.pivot;
    d.gripLocal = {glm::dot(v, d.out), glm::dot(v, d.side), glm::dot(v, d.up)};
    d.gripFrame = host_framecount;
}

bool enabled()
{
    return vr_holster_zones.value != 0.f;
}

int pick(const hands::State& s, int hand, const body::HolsterPositions& holsters, const glm::vec3* grenadePouch,
    const glm::vec3* ammoPouch)
{
    HandPick& h = zoneState.hands[hand];
    const int last = h.frame >= host_framecount - 2 ? h.zone : -1;
    h.gun = holdsGun(hand);
    h.item = !h.gun && carriesItem(hand);
    h.samples = 0;
    h.sample[h.samples++] = s.pos[hand];
    // A gun: its grip (the handle as drawn), and its body a third and half the way to its muzzle (what goes in a holster;
    // the rest of a long gun stands out of it).
    glm::vec3 handle, tip;
    if(h.gun && view::heldWeaponLine(hand, handle, tip))
    {
        h.sample[h.samples++] = handle;
        h.sample[h.samples++] = glm::mix(handle, tip, 1.f / 3.f);
        h.sample[h.samples++] = glm::mix(handle, tip, 0.5f);
        // And the hand holding it, as drawn (its palm): a hand in a holster while its gun's points were not let the gun
        // fall (2026-10-17). Tested for the holsters only, as the gun's points (the pouches still take the hand alone,
        // and no gun).
        glm::vec3 palm;
        if(vr_holster_zone_hand_point.value != 0.f && view::heldHandPoint(hand, palm))
        {
            h.sample[h.samples++] = palm;
        }
    }

    const float hyst = za::max(0.f, vr_holster_zone_hysteresis.value);
    int best = -1;
    float bestScore = 0.f;
    for(int z = 0; z < ZoneCount; z++)
    {
        h.ratio[z] = -1.f;
        glm::vec3 pivot;
        if(!pivotOf(z, holsters, grenadePouch, ammoPouch, pivot))
        {
            continue;
        }
        // Carrying an item to put away: larger (vr_holster_zone_item_*).
        const Box b = zoneBox(s, z, pivot, h.item ? itemScale(z) : 1.f);
        // The pouches give and take rounds, magazines and grenades: only the hand's own point, and only without a gun,
        // but for the ammo pouch's reloading (vr_reload.qc VR_Reload_PouchFrame: the gun's load point in it), which
        // the holsters win only when deeper.
        const bool pouch = z >= body::HolsterCount;
        float r = 1e9f;
        if(!pouch)
        {
            for(int i = 0; i < h.samples; i++)
            {
                r = za::min(r, ratioIn(b, h.sample[i]));
            }
            // A hand without a gun at the holstered gun's grip (vr_holster_zone_handle_radius): a sphere round it,
            // however far it stands out of the box (the shoulders' handles).
            glm::vec3 grip;
            const float radius = vr_holster_zone_handle_radius.value;
            if(!h.gun && radius > 0.f && gripOf(s, z, pivot, grip))
            {
                r = za::min(r, glm::distance(h.sample[0], grip) / radius);
            }
        }
        else if(!h.gun)
        {
            r = ratioIn(b, h.sample[0]);
        }
        else if(z == ZoneAmmoPouch && s.loadPortValid[hand])
        {
            r = ratioIn(b, s.loadPort[hand]); // (scored, never picked: a gun goes to no pouch)
        }
        h.ratio[z] = r;
        const bool kept = z == last;
        if(r > (kept ? 1.f + hyst : 1.f))
        {
            continue;
        }
        const float score = kept ? r / (1.f + hyst) : r;
        if(best < 0 || score < bestScore)
        {
            best = z;
            bestScore = score;
        }
    }
    if(best >= body::HolsterCount && h.gun)
    {
        best = -1; // (the load point deepest in the ammo pouch: no holster)
    }
    h.zone = best;
    h.frame = host_framecount;
    return best;
}

bool drawnPoint(const hands::State& s, int zone, const glm::vec3& offset, glm::vec3& out)
{
    if(zone < 0 || zone >= ZoneCount)
    {
        return false;
    }
    const body::HolsterPositions holsters = body::holsterPositions(s);
    const glm::vec3 grenade = body::pouchEnabled() ? body::pouchPosition(s) : glm::vec3{0.f};
    const glm::vec3 ammo = body::ammoPouchEnabled() ? body::ammoPouchPosition(s) : glm::vec3{0.f};
    glm::vec3 pivot;
    if(!pivotOf(zone, holsters, body::pouchEnabled() ? &grenade : nullptr, body::ammoPouchEnabled() ? &ammo : nullptr, pivot))
    {
        return false;
    }
    glm::vec3 o, side, up, at;
    frameOf(s, zone, pivot, o, side, up, at);
    out = at + o * offset.x + side * offset.y + up * offset.z;
    return true;
}

bool boxMiddle(const hands::State& s, int zone, glm::vec3& out, const glm::vec3& offset)
{
    if(zone < 0 || zone >= ZoneCount)
    {
        return false;
    }
    const body::HolsterPositions holsters = body::holsterPositions(s);
    const glm::vec3 grenade = body::pouchEnabled() ? body::pouchPosition(s) : glm::vec3{0.f};
    const glm::vec3 ammo = body::ammoPouchEnabled() ? body::ammoPouchPosition(s) : glm::vec3{0.f};
    glm::vec3 pivot;
    if(!pivotOf(zone, holsters, body::pouchEnabled() ? &grenade : nullptr, body::ammoPouchEnabled() ? &ammo : nullptr, pivot))
    {
        return false;
    }
    const Box b = zoneBox(s, zone, pivot);
    out = b.centre + b.axis[0] * offset.x + b.axis[1] * offset.y + b.axis[2] * offset.z;
    return true;
}

bool gripPoint(const hands::State& s, int zone, glm::vec3& out)
{
    if(zone < 0 || zone >= body::HolsterCount)
    {
        return false;
    }
    return gripOf(s, zone, body::holsterPositions(s)[zone], out);
}

bool dragGrip(int hand, bool down)
{
    if(hand < 0 || hand >= HAND_COUNT)
    {
        return false;
    }
    Drag& d = zoneState.drag[hand];
    if(!down)
    {
        return za::exchange(d.zone, -1) >= 0; // (the press was not the game's: nor is its release)
    }
    const hands::State& s = hands::current();
    if(!enabled() || !anyDrag() || !s.valid || d.zone >= 0)
    {
        return false;
    }
    int best = -1;
    float bestRatio = 0.f;
    glm::vec3 bestPivot{0.f};
    for(int z = 0; z < ZoneCount; z++)
    {
        glm::vec3 pivot;
        if(kindOf(z).drag->value == 0.f || !pivotNow(s, z, pivot))
        {
            continue;
        }
        const float r = ratioIn(zoneBox(s, z, pivot), s.pos[hand]);
        if(r <= 1.f && (best < 0 || r < bestRatio))
        {
            best = z;
            bestRatio = r;
            bestPivot = pivot;
        }
    }
    if(best < 0)
    {
        return false;
    }
    const Kind k = kindOf(best);
    const Drag& other = zoneState.drag[1 - hand];
    d.zone = best;
    d.resize = other.zone >= 0 && !other.resize && kindIndex(other.zone) == kindIndex(best);
    d.handStart = bodyLocal(s, best, bestPivot, s.pos[hand]);
    d.offsetStart = {k.x->value, k.y->value, k.z->value};
    d.spreadStart = za::max(0.5f, glm::distance(s.pos[HAND_OFF], s.pos[HAND_MAIN]));
    d.sizeStart = {k.depth->value, k.width->value, k.height->value};
    dragHaptic(hand);
    return true;
}

void dragFrame()
{
    const hands::State& s = hands::current();
    for(int hand = 0; hand < HAND_COUNT; hand++)
    {
        const Drag& d = zoneState.drag[hand];
        glm::vec3 pivot;
        if(d.zone < 0 || !s.valid || kindOf(d.zone).drag->value == 0.f || !pivotNow(s, d.zone, pivot))
        {
            continue; // (switched off while held: kept still until the grip lets go)
        }
        const Kind k = kindOf(d.zone);
        if(d.resize)
        {
            const Drag& other = zoneState.drag[1 - hand];
            if(other.zone < 0 || kindIndex(other.zone) != kindIndex(d.zone))
            {
                continue; // (the hand that moved it let go: the size stays)
            }
            const float f = glm::distance(s.pos[HAND_OFF], s.pos[HAND_MAIN]) / d.spreadStart;
            setSnapped(k.depth, d.sizeStart.x * f, 0.01f, 0.02f, 10.f);
            setSnapped(k.width, d.sizeStart.y * f, 0.01f, 0.02f, 10.f);
            setSnapped(k.height, d.sizeStart.z * f, 0.01f, 0.02f, 10.f);
            continue;
        }
        const glm::vec3 off = d.offsetStart + bodyLocal(s, d.zone, pivot, s.pos[hand]) - d.handStart;
        setSnapped(k.x, off.x, 0.05f, -50.f, 50.f);
        setSnapped(k.y, off.y, 0.05f, -50.f, 50.f);
        setSnapped(k.z, off.z, 0.05f, -50.f, 50.f);
    }
}

void queueDebug(const hands::State& s)
{
    // Shown, or while Immersion > Holster Zones has one of its settings chosen, or a kind is dragged by hand; on the
    // body's mirror image too while it is drawn (vr_bodyclone.cpp).
    zoneState.cloned = bodyclone::mirror(zoneState.mirror);
    const cvar_t* chosen = menu::selectedSetting();
    const bool tweaking = chosen && !q_strncasecmp(chosen->name, "vr_holster_zone", 15);
    // The item-sized boxes (vr_holster_zones_show_items), or while an Item Size is chosen.
    const bool items = vr_holster_zones_show_items.value != 0.f ||
                       (chosen && !q_strncasecmp(chosen->name, "vr_holster_zone_item_", 21));
    if((!vr_holster_zones_show.value && !tweaking && !items && !anyDrag()) || !s.valid)
    {
        return;
    }
    const body::HolsterPositions holsters = body::holsterPositions(s);
    const bool withGrenade = body::pouchEnabled();
    const bool withAmmo = body::ammoPouchEnabled();
    const glm::vec3 grenade = withGrenade ? body::pouchPosition(s) : glm::vec3{0.f};
    const glm::vec3 ammo = withAmmo ? body::ammoPouchPosition(s) : glm::vec3{0.f};
    const glm::vec4 idle{0.65f, 0.65f, 0.65f, 0.6f}, occupied{0.3f, 0.6f, 1.f, 0.85f}, candidate{1.f, 0.85f, 0.15f, 0.9f},
        accept{0.2f, 1.f, 0.3f, 1.f};
    // A kind dragged by hand (vr_holster_zone_<kind>_drag): orange, cyan while held; the item-sized boxes magenta.
    const glm::vec4 draggable{1.f, 0.5f, 0.1f, 0.85f}, held{0.2f, 1.f, 1.f, 1.f}, itemBox{1.f, 0.25f, 0.9f, 0.8f};
    for(int z = 0; z < ZoneCount; z++)
    {
        glm::vec3 pivot;
        if(!pivotOf(z, holsters, withGrenade ? &grenade : nullptr, withAmmo ? &ammo : nullptr, pivot))
        {
            continue;
        }
        bool at = false, inside = false, item = false;
        for(int hand = 0; hand < HAND_COUNT; hand++)
        {
            const HandPick& h = zoneState.hands[hand];
            if(h.frame < host_framecount - 2)
            {
                continue;
            }
            at = at || h.zone == z;
            inside = inside || (h.ratio[z] >= 0.f && h.ratio[z] <= 1.f);
            item = item || h.item;
        }
        const bool full = zoneState.drawn[z].occupied && zoneState.drawn[z].frame >= host_framecount - 2;
        const bool dragged = zoneState.drag[HAND_OFF].zone == z || zoneState.drag[HAND_MAIN].zone == z;
        const bool canDrag = kindOf(z).drag->value != 0.f;
        const glm::vec4& color = dragged ? held
                                 : at    ? accept
                                 : inside ? candidate
                                 : canDrag ? draggable
                                 : full    ? occupied
                                           : idle;
        // (A hand carrying an item: its larger boxes, vr_holster_zone_item_*.)
        edges(zoneBox(s, z, pivot, item ? itemScale(z) : 1.f), color, at || dragged ? 0.2f : 0.12f);
        if(items && itemScale(z) != 1.f)
        {
            edges(zoneBox(s, z, pivot, itemScale(z)), itemBox, 0.08f);
        }
        dot(pivot, 0.6f, color);
        // The holstered gun's grip sphere (vr_holster_zone_handle_radius): three rings.
        glm::vec3 grip;
        const float radius = vr_holster_zone_handle_radius.value;
        if(radius > 0.f && gripOf(s, z, pivot, grip))
        {
            const Drawn& d = zoneState.drawn[z];
            const glm::vec3 axes[3][2] = {{d.out, d.side}, {d.side, d.up}, {d.up, d.out}};
            constexpr int segments = 16;
            for(const auto& a : axes)
            {
                for(int i = 0; i < segments; i++)
                {
                    const float t0 = 6.2831853f * static_cast<float>(i) / segments;
                    const float t1 = 6.2831853f * static_cast<float>(i + 1) / segments;
                    line(grip + radius * (a[0] * std::cos(t0) + a[1] * std::sin(t0)),
                        grip + radius * (a[0] * std::cos(t1) + a[1] * std::sin(t1)), 0.08f, color);
                }
            }
        }
    }
    for(int hand = 0; hand < HAND_COUNT; hand++)
    {
        const HandPick& h = zoneState.hands[hand];
        for(int i = 0; i < h.samples && h.frame >= host_framecount - 2; i++)
        {
            dot(h.sample[i], i == 0 ? 0.8f : 0.5f, glm::vec4{1.f, 1.f, 1.f, 0.9f});
        }
    }
}

void report_f()
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        Con_Printf("vr_holster_zones_report: no hands yet\n");
        return;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, s.bodyYaw, 0.f}, fwd, right, up);
    const auto rel = [&](const glm::vec3& p) {
        const glm::vec3 d = p - s.head;
        return glm::vec3{glm::dot(d, fwd), glm::dot(d, right), glm::dot(d, up)};
    };
    const body::HolsterPositions holsters = body::holsterPositions(s);
    const bool withGrenade = body::pouchEnabled();
    const bool withAmmo = body::ammoPouchEnabled();
    const glm::vec3 grenade = withGrenade ? body::pouchPosition(s) : glm::vec3{0.f};
    const glm::vec3 ammo = withAmmo ? body::ammoPouchPosition(s) : glm::vec3{0.f};
    Con_Printf("holster zones: %s (vr_holster_zones %g), units from the eyes: forward right up\n",
        enabled() ? "boxes" : "old spheres", vr_holster_zones.value);
    for(int z = 0; z < ZoneCount; z++)
    {
        glm::vec3 pivot;
        if(!pivotOf(z, holsters, withGrenade ? &grenade : nullptr, withAmmo ? &ammo : nullptr, pivot))
        {
            continue;
        }
        const Box b = zoneBox(s, z, pivot);
        const glm::vec3 p = rel(pivot), c = rel(b.centre);
        const Drawn& d = zoneState.drawn[z];
        Con_Printf("zone %s: pivot %.1f %.1f %.1f middle %.1f %.1f %.1f half %.1f %.1f %.1f drawn %d at %.1f %.1f %.1f%s",
            zoneNames[z], p.x, p.y, p.z, c.x, c.y, c.z, b.half.x, b.half.y, b.half.z, d.frame >= host_framecount - 2 ? 1 : 0,
            d.atLocal.x, d.atLocal.y, d.atLocal.z, d.occupied ? " (a gun in it)" : "");
        glm::vec3 grip;
        if(gripOf(s, z, pivot, grip))
        {
            const glm::vec3 g = rel(grip);
            Con_Printf(" grip %.1f %.1f %.1f (%.1f off the box: %.2f)", g.x, g.y, g.z, glm::distance(grip, b.centre),
                ratioIn(b, grip));
        }
        Con_Printf(" item x%.2f\n", itemScale(z));
    }
    for(int hand = 0; hand < HAND_COUNT; hand++)
    {
        const HandPick& h = zoneState.hands[hand];
        Con_Printf("zone hand %s: hotspot %d zone %s gun %d item %d points", hand == HAND_MAIN ? "main" : "off",
            s.hotspot[hand], h.zone >= 0 ? zoneNames[h.zone] : "none", h.gun ? 1 : 0, h.item ? 1 : 0);
        for(int i = 0; i < h.samples; i++)
        {
            const glm::vec3 v = rel(h.sample[i]);
            Con_Printf(" (%.1f %.1f %.1f)", v.x, v.y, v.z);
        }
        Con_Printf("\nzone hand %s depth:", hand == HAND_MAIN ? "main" : "off");
        for(int z = 0; z < ZoneCount; z++)
        {
            if(h.ratio[z] >= 0.f && h.ratio[z] < 3.f)
            {
                Con_Printf(" %s %.2f", zoneNames[z], h.ratio[z]);
            }
        }
        // The old spheres' share (the hand's distance over the reach) for the hips and the ammo pouch.
        Con_Printf(" | old: lhip %.2f rhip %.2f ammopouch %.2f\n",
            glm::distance(s.pos[hand], holsters[body::LeftHip]) / za::max(0.01f, vr_hip_holster_thresh.value),
            glm::distance(s.pos[hand], holsters[body::RightHip]) / za::max(0.01f, vr_hip_holster_thresh.value),
            withAmmo ? glm::distance(s.pos[hand], ammo) / za::max(0.01f, vr_ammo_pouch_thresh.value) : -1.f);
    }
}

} // namespace qvr::zones
