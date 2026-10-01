// vr_selfcollide.cpp -- see vr_selfcollide.hpp.

#include "vr_selfcollide.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_flashlight.hpp"
#include "vr_gadget.hpp"
#include "vr_grasp.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_sightalign.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"
#include "vr_hands.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"


namespace qvr::selfcollide
{
namespace
{

constexpr float easeIn = 0.012f;   // seconds (time constants): pushed out at once, nearly
constexpr float easeOut = 0.03f;   // back as the tracked hand comes out
constexpr float easePass = 0.07f;  // through, once let go: a quick slide rather than a jump
constexpr float passHold = 0.3f;   // seconds the slower ease holds after a contact lets go
constexpr float passDepth = 15.f;  // cm: held out further than this times vr_body_collide_pass, it lets go whatever the share
constexpr int iterations = 4;      // Gauss-Seidel rounds over the contacts
constexpr float followCone = 0.82f; // cos 35 degrees: the way out follows the surface this far from the way it came in
constexpr float backMargin = 1.5f; // units behind the hand a weapon's capsule is its stock (not against the torso)

// A capsule: the segment a..b swept by r (a sphere: a == b). `back`: a weapon's stock. c, br: its bounding sphere.
struct Cap
{
    glm::vec3 a{0.f}, b{0.f};
    float r{0.f};
    bool back{false};
    glm::vec3 c{0.f};
    float br{0.f};
};

void finish(Cap& k)
{
    k.c = (k.a + k.b) * 0.5f;
    k.br = glm::distance(k.a, k.b) * 0.5f + k.r;
}

using Caps = za::Vector<Cap>;

struct Bound
{
    glm::vec3 c{0.f};
    float r{-1.f};
};

Bound boundOf(const Caps& caps)
{
    Bound b;
    if(caps.empty())
    {
        return b;
    }
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(const Cap& k : caps)
    {
        lo = glm::min(lo, k.c - glm::vec3{k.br});
        hi = glm::max(hi, k.c + glm::vec3{k.br});
    }
    b.c = (lo + hi) * 0.5f;
    b.r = 0.f;
    for(const Cap& k : caps)
    {
        b.r = za::max(b.r, glm::distance(k.c, b.c) + k.br);
    }
    return b;
}

[[nodiscard]] bool boundsMeet(const Bound& x, const glm::vec3& ox, const Bound& y, const glm::vec3& oy)
{
    return x.r >= 0.f && y.r >= 0.f && glm::distance(x.c + ox, y.c + oy) < x.r + y.r;
}

// The closest points of the segments p1..q1 and p2..q2 (Ericson, Real-Time Collision Detection, 5.1.9).
void closestPoints(const glm::vec3& p1, const glm::vec3& q1, const glm::vec3& p2, const glm::vec3& q2, glm::vec3& c1, glm::vec3& c2)
{
    const glm::vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = glm::dot(d1, d1), e = glm::dot(d2, d2), f = glm::dot(d2, r);
    float s = 0.f, t = 0.f;
    constexpr float eps = 1e-8f;
    if(a <= eps && e <= eps)
    {
        c1 = p1;
        c2 = p2;
        return;
    }
    if(a <= eps)
    {
        t = za::clamp(f / e, 0.f, 1.f);
    }
    else
    {
        const float c = glm::dot(d1, r);
        if(e <= eps)
        {
            s = za::clamp(-c / a, 0.f, 1.f);
        }
        else
        {
            const float b = glm::dot(d1, d2);
            const float denom = a * e - b * b;
            s = denom > eps ? za::clamp((b * f - c * e) / denom, 0.f, 1.f) : 0.f;
            t = (b * s + f) / e;
            if(t < 0.f)
            {
                t = 0.f;
                s = za::clamp(-c / a, 0.f, 1.f);
            }
            else if(t > 1.f)
            {
                t = 1.f;
                s = za::clamp((b - c) / a, 0.f, 1.f);
            }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

[[nodiscard]] float smooth(float e0, float e1, float x)
{
    const float t = za::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// A frame: an origin and orthonormal axes (a hand's forward, right, up; a body part's).
struct Frame
{
    glm::vec3 o{0.f};
    glm::mat3 m{1.f};
    [[nodiscard]] glm::vec3 world(const glm::vec3& p) const { return o + m * p; }
    [[nodiscard]] glm::vec3 local(const glm::vec3& p) const { return glm::transpose(m) * (p - o); }
};

Frame handFrame(const glm::vec3& pos, const glm::vec3& rot)
{
    glm::vec3 f, r, u;
    hands::angleVectors(rot, f, r, u);
    return {pos, glm::mat3{f, r, u}};
}

Cap toWorld(const Frame& fr, const Cap& k)
{
    Cap w = k;
    w.a = fr.world(k.a);
    w.b = fr.world(k.b);
    finish(w);
    return w;
}

// ----------------------------------------------------------------------------
// A held weapon's capsules, fitted once per model and frame to its shape (the grasp's triangles, in the shape's own
// coordinates): along its length (the principal axis of its vertices) in slabs, each slab's cross-section (its
// vertices' 3rd to 97th percentiles across the two other axes) as one capsule, or two or three side by side where it is
// flat (a gun's body is taller than it is wide).

struct FitKey
{
    const qmodel_t* model;
    int frame;
    bool operator==(const FitKey& o) const { return model == o.model && frame == o.frame; }
};
struct FitKeyHash
{
    za::SizeT operator()(const FitKey& k) const
    {
        return ankerl::unordered_dense::hash<const void*>{}(k.model) ^ (static_cast<za::SizeT>(k.frame) * 2654435761u);
    }
};
struct Fit
{
    za::String name;
    glm::vec3 axes{0.f};
    Caps caps; // the shape's axes, world units
};
ankerl::unordered_dense::map<FitKey, za::UniquePtr<Fit>, FitKeyHash> fits; // (pointers into it are kept)

glm::vec3 principal(const glm::mat3& cov, const glm::vec3& start)
{
    glm::vec3 v = start;
    for(int i = 0; i < 40; i++)
    {
        const glm::vec3 n = cov * v;
        const float len = glm::length(n);
        if(len < 1e-12f)
        {
            break;
        }
        v = n / len;
    }
    return v;
}

float percentile(za::Vector<float>& v, float q)
{
    if(v.empty())
    {
        return 0.f;
    }
    const za::SizeT i = za::min(v.size() - 1, static_cast<za::SizeT>(q * static_cast<float>(v.size() - 1) + 0.5f));
    za::quickSort(v.begin(), v.end()); // (sorted whole: v[i] is std::nth_element's, the same value)
    return v[i];
}

void fitCaps(const za::Vector<glm::vec3>& pts, Caps& out)
{
    out.clear();
    if(pts.size() < 6)
    {
        return;
    }
    glm::vec3 mean{0.f}, lo{1e30f}, hi{-1e30f};
    for(const glm::vec3& p : pts)
    {
        mean += p;
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    mean /= static_cast<float>(pts.size());
    glm::mat3 cov{0.f};
    for(const glm::vec3& p : pts)
    {
        cov += glm::outerProduct(p - mean, p - mean);
    }
    const glm::vec3 ext = hi - lo;
    const glm::vec3 start = ext.x >= ext.y && ext.x >= ext.z ? glm::vec3{1.f, 0.f, 0.f}
                            : ext.y >= ext.z                 ? glm::vec3{0.f, 1.f, 0.f}
                                                             : glm::vec3{0.f, 0.f, 1.f};
    const glm::vec3 ax = principal(cov, start);
    const glm::mat3 deflated = cov - glm::outerProduct(ax, ax) * glm::dot(ax, cov * ax);
    glm::vec3 guess = qza::abs(ax.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{0.f, 1.f, 0.f};
    guess = glm::normalize(guess - ax * glm::dot(guess, ax));
    glm::vec3 bx = principal(deflated, guess);
    bx = glm::normalize(bx - ax * glm::dot(bx, ax));
    const glm::vec3 cx = glm::cross(ax, bx);

    float s0 = 1e30f, s1 = -1e30f;
    za::Vector<float> along(pts.size());
    for(za::SizeT i = 0; i < pts.size(); i++)
    {
        along[i] = glm::dot(pts[i] - mean, ax);
        s0 = za::min(s0, along[i]);
        s1 = za::max(s1, along[i]);
    }
    // The slabs: about as long as the weapon is thick, 2 to 5 of them.
    za::Vector<float> us, vs;
    for(za::SizeT i = 0; i < pts.size(); i++)
    {
        us.pushBack(glm::dot(pts[i] - mean, bx));
        vs.pushBack(glm::dot(pts[i] - mean, cx));
    }
    const float thick = za::max(percentile(us, 0.97f) - percentile(us, 0.03f), percentile(vs, 0.97f) - percentile(vs, 0.03f));
    const float length = s1 - s0;
    const int slabs = za::clamp(static_cast<int>(za::lround(length / za::max(thick * 1.2f, 1e-3f))), 2, 5);
    const float step = length / static_cast<float>(slabs);
    for(int k = 0; k < slabs; k++)
    {
        const float a0 = s0 + step * static_cast<float>(k), a1 = a0 + step;
        us.clear();
        vs.clear();
        for(za::SizeT i = 0; i < pts.size(); i++)
        {
            if(along[i] >= a0 - 1e-4f && along[i] <= a1 + 1e-4f)
            {
                us.pushBack(glm::dot(pts[i] - mean, bx));
                vs.pushBack(glm::dot(pts[i] - mean, cx));
            }
        }
        if(us.size() < 3)
        {
            continue;
        }
        const float u0 = percentile(us, 0.03f), u1 = percentile(us, 0.97f);
        const float v0 = percentile(vs, 0.03f), v1 = percentile(vs, 0.97f);
        const bool uLong = u1 - u0 >= v1 - v0;
        const float longLo = uLong ? u0 : v0, longHi = uLong ? u1 : v1;
        const float shortLo = uLong ? v0 : u0, shortHi = uLong ? v1 : u1;
        const float r = za::max((shortHi - shortLo) * 0.5f, 0.05f);
        const int count = za::clamp(static_cast<int>(za::ceil((longHi - longLo) / (2.f * r) - 0.25f)), 1, 3);
        const float shortMid = (shortLo + shortHi) * 0.5f;
        const float half = za::min(r, step * 0.5f);
        for(int j = 0; j < count; j++)
        {
            const float l = count == 1 ? (longLo + longHi) * 0.5f
                                       : longLo + r + (longHi - longLo - 2.f * r) * static_cast<float>(j) / static_cast<float>(count - 1);
            const float u = uLong ? l : shortMid, v = uLong ? shortMid : l;
            const glm::vec3 across = mean + bx * u + cx * v;
            Cap cap;
            cap.a = across + ax * (a0 + half);
            cap.b = across + ax * (a1 - half);
            cap.r = r;
            out.pushBack(cap);
        }
    }
}

// `axes`: the world units each of the shape's axes is drawn at (an alias model's raw vertices are scaled per axis):
// the capsules are fitted in the shape's axes at those units, so that they are round in the world.
const Caps* weaponCaps(const entity_t& e, const glm::vec3& axes)
{
    if(!e.model)
    {
        return nullptr;
    }
    Fit& f = qza::stableAt<Fit>(fits, FitKey{e.model, e.frame});
    if(f.name != e.model->name || glm::any(glm::greaterThan(glm::abs(f.axes - axes), glm::vec3{1e-4f})))
    {
        f.name = e.model->name;
        f.axes = axes;
        f.caps.clear();
        if(const grasp::Shape* shape = grasp::shapeOf(e, e.frame))
        {
            za::Vector<glm::vec3> pts;
            pts.reserve(shape->tris.size() * 3);
            for(const grasp::Triangle& t : shape->tris)
            {
                pts.pushBackMultiple(t.p[0] * axes, t.p[1] * axes, t.p[2] * axes);
            }
            fitCaps(pts, f.caps);
        }
    }
    return f.caps.empty() ? nullptr : &f.caps;
}

// ---------------------------------------------------------------------------- What the view drew last frame, in frames
// that carry it to this one: a hand's spheres and its weapon's capsules in the hand's frame; the arms' shoulder and
// elbow in the chest's, their wrists in their hands'; the legs in the pelvis's; the gadget in its hand's.

struct Record
{
    int frame{-1};
    Caps hand[2];
    Caps weapon[2];
    const qmodel_t* weaponModel[2]{nullptr, nullptr};
    bool body{false};
    float m2w{1.f};
    glm::vec3 shoulder[2]{}, elbow[2]{}; // chest frame
    glm::vec3 wrist[2]{};                // the hand's frame
    bool wristLocal[2]{true, true};      // (else the world's: the hand was drawn elsewhere, on a grip)
    glm::vec3 wristWorld[2]{};
    bool legs{false};
    glm::vec3 hip[2]{}, knee[2]{}, ankle[2]{}; // pelvis frame
    bool gadget{false};
    int gadgetHand{HAND_OFF};
    bool gadgetLocal{true};
    Caps gadgetCaps; // the gadget hand's frame (or the world's)
};
Record rec;

// ----------------------------------------------------------------------------
// This frame's proxies and contacts.

enum Part : int
{
    Torso,
    Head,
    Legs,
    UpperArm,
    Forearm,
    Gadget,
    PartCount
};
constexpr const char* partNames[PartCount] = {"torso", "head", "legs", "upper arm", "forearm", "gadget"};

enum Sub : int
{
    SubHand,
    SubWeapon,
    SubCount
};
constexpr const char* subNames[SubCount] = {"hand", "weapon"};

constexpr int staticContacts = 2 * SubCount * PartCount;
constexpr int contactCount = staticContacts + SubCount * SubCount;

enum class Mode : int
{
    None,
    Blocking,
    Passing
};
constexpr const char* modeNames[3] = {"-", "block", "pass"};

struct Contact
{
    Mode mode{Mode::None};
    glm::vec3 n{0.f};  // the way out
    glm::vec3 n0{0.f}; // the way it came in
    // This frame:
    const Caps* A{nullptr};
    const Caps* B{nullptr};
    int a{0}, b{-1}; // the movers (b -1: A against something that stays)
    float w{0.f};
    float share{0.f}; // how far through (0 touching on the side it came in, 1 on the far side)
    float t0{0.f};    // the push the tracked pose needs
    float pushed{0.f}; // how far the solve moved the hand (the two apart) along its way out
    bool on{false};   // tested this frame
};
za::Array<Contact, contactCount> contacts;

struct Scene
{
    Caps hand[2], weapon[2], front[2];
    Caps part[PartCount][2]; // Torso, Head, Legs: [0]; the arms and the gadget per owner
    Bound handB[2], weaponB[2], frontB[2];
    Bound partB[PartCount][2];
    avatar::Torso torso;
    bool torsoValid{false};
};
Scene scene;

glm::vec3 drawn[2]{glm::vec3{0.f}, glm::vec3{0.f}};  // eased, what is added to the tracked hands
glm::vec3 target[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // this frame's
glm::vec3 tracked[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // the hands as beginView had them (endView puts them back)
double passedAt[2]{-1.0, -1.0};                      // when a contact of the hand last let go
int appliedFrame = -1;
double lastTime = -1.0;
bool viewOn = false; // this frame's beginView ran the solve (endView records)

[[nodiscard]] float cm(float units)
{
    return units / units::metresToUnits() * 100.f;
}

// The build's proportions (make_vrbody.py: muscularity; the torso a third as much).
void buildScales(float& m, float& torso)
{
    const int build = za::clamp(static_cast<int>(vr_body_build.value), 0, 2);
    m = build == 0 ? 0.9f : build == 2 ? 1.5f : 1.2f;
    torso = 1.f + (m - 1.f) * 0.35f;
}

void addCap(Caps& out, const glm::vec3& a, const glm::vec3& b, float r)
{
    Cap k;
    k.a = a;
    k.b = b;
    k.r = r;
    finish(k);
    out.pushBack(k);
}

[[nodiscard]] bool bodyDrawn()
{
    return vr_body_mode.value >= 1.f && cl.stats[STAT_HEALTH] > 0 && !cl.intermission;
}

// A hand whose drawn place is another's (a prop's grip in both hands, a ledge, a gun carried by its foregrip, the
// other gun's grip): how much of it moves with its controller.
[[nodiscard]] float mobility(const hands::State& s, int hand)
{
    glm::vec3 p = s.pos[hand], a = s.rot[hand];
    if(held::drawnHand(hand, p, a) || (cl.stats[protocol::STAT_QVR_CLIMB] & (1 << hand)) || twohand::carrying(hand))
    {
        return 0.f;
    }
    return s.grip2HValid[1 - hand] ? 1.f - za::clamp(twohand::transition(1 - hand), 0.f, 1.f) : 1.f;
}

// A hand holding nothing (it may be helping hold the other hand's gun).
[[nodiscard]] bool emptyHand(int hand)
{
    const int slot = weapons::heldSlot(hand);
    return (slot < 0 || slot == weapons::fistSlot()) && !held::heldEntity(hand) && !flashlight::holds(hand);
}

// vr_view.cpp's pushOut: a free hand brushing the other hand's weapon (vr_hand_collide).
[[nodiscard]] bool freeHand(const hands::State& s, int hand)
{
    const float gripBlend = s.grip2HValid[1 - hand] ? twohand::transition(1 - hand) : 0.f;
    return gripBlend <= 0.f && emptyHand(hand);
}

// The pairs between the two sides (the hands, a hand and the other arm or gadget, the weapons): eased out as a
// two-handed grip is taken, a free hand comes to the other gun's grip or to the torch in the other hand; none with a
// gun carried by its foregrip.
[[nodiscard]] float betweenHands(const hands::State& s)
{
    float w = 1.f;
    for(int k = 0; k < 2; k++)
    {
        const int j = 1 - k;
        if(twohand::carrying(k))
        {
            return 0.f;
        }
        if(s.grip2HValid[k])
        {
            w *= 1.f - za::clamp(twohand::transition(k), 0.f, 1.f);
            if(emptyHand(j)) // (coming to the grip, and still at it after letting go)
            {
                const glm::vec3 from = s.grip2HPalm[k] ? hands::palmPoint(s, j) : s.pos[j];
                w *= smooth(5.5f, 9.f, glm::distance(from, s.grip2H[k]) - s.grip2HBias[k]); // (taken within 5.5)
            }
        }
        glm::vec3 torch, angles;
        if(flashlight::holds(k) && flashlight::heldPlace(s, k, torch, angles))
        {
            w *= smooth(5.f, 10.f, glm::distance(hands::palmPoint(s, j), torch));
        }
    }
    return w;
}

// The body against a hand at a holster (its hotspot: the holsters stand out of the body, so only a hand deep in one is
// eased out, from its reach to half of it) and a weapon going into one (from 1.6 times its reach to its reach).
[[nodiscard]] float awayFromHolsters(
    const hands::State& s, int hand, const body::HolsterPositions& hp, const glm::vec3* pouch, bool weapon)
{
    float ratio = 1e9f;
    for(int h = 0; h < body::HolsterCount; h++)
    {
        const float reach = body::holsterReach(static_cast<body::Holster>(h));
        if(reach > 0.f)
        {
            ratio = za::min(ratio, glm::distance(s.pos[hand], hp[static_cast<za::SizeT>(h)]) / reach);
        }
    }
    if(pouch && body::pouchReach() > 0.f) // the grenade pouch at the small of the back, as a holster
    {
        ratio = za::min(ratio, glm::distance(s.pos[hand], *pouch) / body::pouchReach());
    }
    return weapon ? smooth(1.f, 1.6f, ratio) : smooth(0.5f, 1.f, ratio);
}

// ----------------------------------------------------------------------------
// The test of one contact: `A` moved by `oa` against `B` moved by `ob`.

struct Eval
{
    bool overlap{false};
    glm::vec3 m{0.f}; // the way out, weighted by depth (from B to A)
    float depth{0.f}; // the deepest overlap
    float t{0.f};     // with `n`: the push along it that takes A out of B
    float back{0.f};  // with `through`: the push the other way that takes all of A through and out of B's far side
};

// With `through`, the parts of A not in B yet count too (where going the other way takes them into it): how far the
// whole of A has yet to go through.
void evaluate(const Caps& A, const glm::vec3& oa, const Caps& B, const glm::vec3& ob, const glm::vec3* n, Eval& e,
    bool through = false)
{
    e = Eval{};
    for(const Cap& x : A)
    {
        const glm::vec3 xc = x.c + oa;
        for(const Cap& y : B)
        {
            const glm::vec3 dc = xc - (y.c + ob);
            if(!through && glm::dot(dc, dc) >= (x.br + y.br) * (x.br + y.br))
            {
                continue;
            }
            glm::vec3 px, py;
            closestPoints(x.a + oa, x.b + oa, y.a + ob, y.b + ob, px, py);
            const glm::vec3 d = px - py;
            const float R = x.r + y.r;
            const float dd = glm::dot(d, d);
            const bool in = dd < R * R;
            if(in)
            {
                e.overlap = true;
                const float len = za::sqrt(dd);
                e.depth = za::max(e.depth, R - len);
                if(len > 1e-4f)
                {
                    e.m += d / len * (R - len);
                }
            }
            if(n && (in || through))
            {
                // Moved by s along n, the two are apart once |d + s n| >= R: the roots of s^2 + 2 s dn + dd - R^2.
                const float dn = glm::dot(d, *n);
                const float disc2 = dn * dn - dd + R * R;
                if(disc2 < 0.f)
                {
                    continue; // (that way it misses)
                }
                const float disc = za::sqrt(disc2);
                if(in)
                {
                    e.t = za::max(e.t, -dn + disc);
                }
                if(through)
                {
                    e.back = za::max(e.back, dn + disc);
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// The scene from the tracked hands and last frame's record.

void buildScene(const hands::State& s)
{
    Scene& sc = scene;
    for(int h = 0; h < 2; h++)
    {
        sc.hand[h].clear();
        sc.weapon[h].clear();
        sc.front[h].clear();
        for(auto& p : sc.part)
        {
            p[h].clear();
        }
    }
    sc.torsoValid = false;
    const float unitsPerM = rec.body ? rec.m2w : units::metresToUnits() * units::bodyScale();
    if(rec.frame < 0 || host_framecount - rec.frame > 2)
    {
        return; // nothing recorded lately (a new map, collisions just turned on)
    }

    for(int h = 0; h < 2; h++)
    {
        // The movers at the tracked pose.
        const Frame hf = handFrame(s.pos[h], s.rot[h]);
        for(const Cap& k : rec.hand[h])
        {
            sc.hand[h].pushBack(toWorld(hf, k));
        }
        if(rec.weaponModel[h] && rec.weaponModel[h] == weapons::heldModel(h) && !twohand::carrying(h))
        {
            const Frame wf = handFrame(s.pos[h], s.visualRot[h]);
            for(const Cap& k : rec.weapon[h])
            {
                sc.weapon[h].pushBack(toWorld(wf, k));
                if(!k.back)
                {
                    sc.front[h].pushBack(sc.weapon[h].back());
                }
            }
        }
        sc.handB[h] = boundOf(sc.hand[h]);
        sc.weaponB[h] = boundOf(sc.weapon[h]);
        sc.frontB[h] = boundOf(sc.front[h]);
    }

    // The body.
    float m, t;
    buildScales(m, t);
    if(rec.body && bodyDrawn())
    {
        sc.torso = avatar::torso(s);
        sc.torsoValid = true;
        const avatar::Frame& ch = sc.torso.chest;
        const avatar::Frame& pv = sc.torso.pelvis;
        const float k = unitsPerM;
        // Columns: up the spine, the side, forward. The torso's elliptic rings (make_vrbody.py) as two upright capsules
        // side by side: the chest's (0.13 x 0.19 m at 1.35 m, times the build) and the belly's and hips' (0.11 x
        // 0.165).
        for(const float side : {-1.f, 1.f})
        {
            const glm::vec3 cs = ch.rot[1] * (side * 0.058f * t * k) - ch.rot[2] * (0.01f * k);
            addCap(sc.part[Torso][0], ch.pos + ch.rot[0] * (-0.06f * k) + cs, ch.pos + ch.rot[0] * (0.09f * k) + cs, 0.125f * t * k);
            const glm::vec3 ps = pv.rot[1] * (side * 0.055f * k) - pv.rot[2] * (0.01f * k);
            addCap(sc.part[Torso][0], pv.pos + ps, pv.pos + pv.rot[0] * (0.12f * k) + ps, 0.11f * k);
        }
        // The neck and head (not drawn in the eyes: the face you feel): from the top of the neck under the eyes.
        glm::vec3 hf, hr, hu;
        hands::angleVectors(s.headAngles, hf, hr, hu);
        const glm::vec3 top = s.head - (hf * vr_body_eye_forward.value + hu * vr_body_eye_up.value) * k;
        addCap(sc.part[Head][0], top - hu * (0.09f * k), top, 0.05f * t * k);
        addCap(sc.part[Head][0], top + hu * (0.08f * k), top + hu * (0.08f * k), 0.095f * k);
        // The legs (the full body): thighs and calves.
        if(rec.legs && vr_body_mode.value >= 3.f)
        {
            for(int side = 0; side < 2; side++)
            {
                const glm::vec3 hip = pv.pos + pv.rot * rec.hip[side], knee = pv.pos + pv.rot * rec.knee[side],
                                ankle = pv.pos + pv.rot * rec.ankle[side];
                addCap(sc.part[Legs][0], hip, knee, 0.085f * t * k);
                addCap(sc.part[Legs][0], knee, ankle, 0.06f * t * k);
            }
        }
        // The arms: the upper arm, the forearm's thick half and its bracer (their rings' mean semi-axes).
        for(int h = 0; h < 2; h++)
        {
            const glm::vec3 shoulder = ch.pos + ch.rot * rec.shoulder[h];
            const glm::vec3 elbow = ch.pos + ch.rot * rec.elbow[h];
            const glm::vec3 wrist =
                rec.wristLocal[h] ? handFrame(s.pos[h] + drawn[h], s.rot[h]).world(rec.wrist[h]) : rec.wristWorld[h];
            addCap(sc.part[UpperArm][h], shoulder, elbow, 0.056f * m * k);
            const glm::vec3 mid = glm::mix(elbow, wrist, 0.45f);
            addCap(sc.part[Forearm][h], elbow, mid, 0.047f * m * 1.08f * k);
            addCap(sc.part[Forearm][h], mid, wrist, 0.036f * k);
        }
    }
    // The gadget, on its hand's forearm.
    if(rec.gadget && gadget::active())
    {
        const int g = rec.gadgetHand;
        const Frame gf = rec.gadgetLocal ? handFrame(s.pos[g] + drawn[g], s.rot[g]) : Frame{};
        for(const Cap& k : rec.gadgetCaps)
        {
            sc.part[Gadget][g].pushBack(toWorld(gf, k));
        }
    }
    for(int p = 0; p < PartCount; p++)
    {
        for(int h = 0; h < 2; h++)
        {
            sc.partB[p][h] = boundOf(sc.part[p][h]);
        }
    }
}

// ----------------------------------------------------------------------------
// The solve.

struct Stats
{
    int tested{0}, blocking{0}, passing{0};
};

void solve(const hands::State& s, float dt, glm::vec3 out[2], Stats& stats)
{
    out[0] = out[1] = glm::vec3{0.f};
    buildScene(s);
    const Scene& sc = scene;
    const float passShare = za::clamp(vr_body_collide_pass.value, 0.05f, 1.f);
    const float passCap = passShare * passDepth * 0.01f * units::metresToUnits();
    const float mob[2] = {mobility(s, 0), mobility(s, 1)};
    // Between the hands: back in over a third of a second once a grip or a hand-over is done (the helping hand's arm
    // swings back from the grip onto its controller meanwhile).
    static float lastBetween = 1.f;
    const float between = za::min(betweenHands(s), lastBetween + dt / 0.3f);
    lastBetween = between;
    const bool freeH[2] = {freeHand(s, 0), freeHand(s, 1)};
    const bool brushes = vr_hand_collide.value > 0.f; // vr_view.cpp's pushOut takes a free hand against the other weapon

    body::HolsterPositions holsters{};
    glm::vec3 pouch{0.f};
    bool holstersMade = false;
    const auto away = [&](int h, bool weapon) {
        if(!holstersMade)
        {
            holsters = body::holsterPositions(s);
            if(body::pouchEnabled())
            {
                pouch = body::pouchPosition(s);
            }
            holstersMade = true;
        }
        return awayFromHolsters(s, h, holsters, body::pouchEnabled() ? &pouch : nullptr, weapon);
    };

    // Each contact's shapes and weight.
    for(int i = 0; i < contactCount; i++)
    {
        Contact& c = contacts[static_cast<za::SizeT>(i)];
        c.A = c.B = nullptr;
        c.on = false;
        c.w = 0.f;
        const Bound* ab = nullptr;
        const Bound* bb = nullptr;
        if(i < staticContacts)
        {
            const int h = i / (int{SubCount} * PartCount), sub = (i / PartCount) % SubCount, part = i % PartCount;
            c.a = h;
            c.b = -1;
            const bool bodyPart = part == Torso || part == Head || part == Legs;
            const int owner = bodyPart ? 0 : 1 - h;
            c.B = &sc.part[part][owner];
            bb = &sc.partB[part][owner];
            if(sub == SubHand)
            {
                c.A = &sc.hand[h];
                ab = &sc.handB[h];
            }
            else
            {
                c.A = bodyPart ? &sc.front[h] : &sc.weapon[h]; // a gun's stock rests on the shoulder
                ab = bodyPart ? &sc.frontB[h] : &sc.weaponB[h];
            }
            float w = mob[h] * (bodyPart ? 1.f : between);
            if(part == Head && (sub == SubWeapon || !freeH[h])) // aiming down the sights; the torch to the head
            {
                w = 0.f;
            }
            if(bodyPart && w > 0.f && boundsMeet(*ab, glm::vec3{0.f}, *bb, glm::vec3{0.f}))
            {
                w *= away(h, sub == SubWeapon);
                if(sub == SubHand && w > 0.f)
                {
                    // A hand coming to the flashlight on the body or the head (in its reach: lit, taken), as to a
                    // holster: drawn where it is, not held at the body's surface by it (and so off the lamp as the
                    // game reads it, the lamp lit for a hand drawn a few centimetres away from it, or the other way).
                    w *= smooth(1.f, 1.5f, flashlight::reachRatio(s, h));
                }
            }
            c.w = w;
        }
        else
        {
            const int k = i - staticContacts, sa = k / SubCount, sb = k % SubCount;
            c.a = 0;
            c.b = 1;
            c.A = sa == SubHand ? &sc.hand[0] : &sc.weapon[0];
            c.B = sb == SubHand ? &sc.hand[1] : &sc.weapon[1];
            ab = sa == SubHand ? &sc.handB[0] : &sc.weaponB[0];
            bb = sb == SubHand ? &sc.handB[1] : &sc.weaponB[1];
            c.w = between * za::min(mob[0], mob[1]);
            if(brushes && ((sa == SubHand && sb == SubWeapon && freeH[0]) || (sa == SubWeapon && sb == SubHand && freeH[1])))
            {
                c.A = nullptr; // vr_hand_collide's
            }
        }
        if(!c.A || !c.B || c.A->empty() || c.B->empty() || !boundsMeet(*ab, glm::vec3{0.f}, *bb, glm::vec3{0.f}))
        {
            c.mode = Mode::None;
            c.A = nullptr;
            continue;
        }
        c.on = true;
        stats.tested++;

        // Where it stands, from the tracked hands.
        Eval e;
        if(c.w <= 0.01f)
        {
            evaluate(*c.A, glm::vec3{0.f}, *c.B, glm::vec3{0.f}, nullptr, e);
            c.mode = e.overlap ? Mode::Passing : Mode::None; // eased out: through, until clear
            continue;
        }
        evaluate(*c.A, glm::vec3{0.f}, *c.B, glm::vec3{0.f}, c.mode == Mode::Blocking ? &c.n : nullptr, e);
        if(!e.overlap)
        {
            if(c.mode == Mode::Blocking && c.t0 > 0.5f * 0.01f * units::metresToUnits())
            {
                // Out of the far side in one go (held out until then): through, eased as a let go.
                passedAt[c.a] = realtime;
                if(c.b >= 0)
                {
                    passedAt[c.b] = realtime;
                }
            }
            c.mode = Mode::None;
            continue;
        }
        if(c.mode == Mode::Passing)
        {
            continue;
        }
        const float ml = glm::length(e.m);
        if(c.mode == Mode::None && e.depth < 0.1f * 0.01f * units::metresToUnits())
        {
            continue; // (a millimetre in before it counts: no contact coming and going while they graze)
        }
        if(c.mode == Mode::None)
        {
            // In: the way out is the way it came (the deepest parts' way out of it now).
            const glm::vec3 fallback = ab->c - bb->c;
            c.n = ml > 1e-5f ? e.m / ml : glm::length(fallback) > 1e-5f ? glm::normalize(fallback) : glm::vec3{0.f, 0.f, 1.f};
            c.n0 = c.n;
            c.mode = Mode::Blocking;
            if(c.w < 0.5f)
            {
                // Met while mostly eased out (coming to a grip, into a holster; a grip just let go): through, until
                // clear.
                c.mode = Mode::Passing;
                continue;
            }
        }
        else if(ml > 1e-5f && glm::dot(e.m / ml, c.n) > 0.f)
        {
            // Sliding over it: the way out follows its surface a little (a rounded one), never far from the way it came
            // in: pushed on, the hand stays where it met it rather than sliding round it (and never once past its
            // middle, where the far side's way out is the other way).
            c.n = glm::normalize(c.n + (e.m / ml - c.n) * 0.5f);
            if(const float d = glm::dot(c.n, c.n0); d < followCone)
            {
                const glm::vec3 side = c.n - c.n0 * d;
                const float sl = glm::length(side);
                c.n = sl > 1e-5f ? c.n0 * followCone + side / sl * za::sqrt(1.f - followCone * followCone) : c.n0;
            }
        }
        evaluate(*c.A, glm::vec3{0.f}, *c.B, glm::vec3{0.f}, &c.n, e, true);
        c.share = e.t + e.back > 1e-5f ? e.t / (e.t + e.back) : 0.f;
        c.t0 = e.t;
        // How far it must go along its way out to be clear, alone: further than the tracked pose is deep where parts of
        // the hand are on the far side already (a hand round an arm, its fingers past it), which would hold it out far.
        c.pushed = e.t;
        for(int round = 0; round < iterations && c.pushed <= passCap; round++)
        {
            Eval more;
            evaluate(*c.A, c.n * c.pushed, *c.B, glm::vec3{0.f}, &c.n, more);
            if(more.t <= 1e-4f)
            {
                break;
            }
            c.pushed += more.t;
        }
        if(c.share > passShare || c.pushed > passCap * (c.b >= 0 ? 2.f : 1.f)) // (two hands: each moves half)
        {
            c.mode = Mode::Passing; // pushed through: let go
            passedAt[c.a] = realtime;
            if(c.b >= 0)
            {
                passedAt[c.b] = realtime;
            }
        }
    }

    // The pushes: each contact's along its way out, round after round (Gauss-Seidel); two hands share theirs.
    for(int round = 0; round < iterations; round++)
    {
        bool moved = false;
        for(Contact& c : contacts)
        {
            if(!c.on || c.mode != Mode::Blocking)
            {
                continue;
            }
            Eval e;
            evaluate(*c.A, out[c.a], *c.B, c.b >= 0 ? out[c.b] : glm::vec3{0.f}, &c.n, e);
            const float need = e.t - (1.f - c.w) * c.t0;
            if(need <= 1e-4f)
            {
                continue;
            }
            moved = true;
            if(c.b < 0)
            {
                out[c.a] += c.n * need;
            }
            else
            {
                out[c.a] += c.n * (need * 0.5f);
                out[c.b] -= c.n * (need * 0.5f);
            }
        }
        if(!moved)
        {
            break;
        }
    }
    for(const Contact& c : contacts)
    {
        stats.blocking += c.on && c.mode == Mode::Blocking;
        stats.passing += c.on && c.mode == Mode::Passing;
    }
    // Never held out further than a contact lets go at: a hand that several contacts together hold out further (pressed
    // onto the gadget and into the forearm under it; two hands and their wrists pressed together) lets go of them all
    // from the next frame, and meanwhile is held out that far.
    const float most = za::max(passCap, 2.f);
    for(int h = 0; h < 2; h++)
    {
        if(const float l = glm::length(out[h]); l > most)
        {
            out[h] *= most / l;
            for(Contact& c : contacts)
            {
                if(c.on && c.mode == Mode::Blocking && (c.a == h || c.b == h))
                {
                    c.mode = Mode::Passing;
                    passedAt[c.a] = realtime;
                    if(c.b >= 0)
                    {
                        passedAt[c.b] = realtime;
                    }
                }
            }
        }
    }
}

za::String contactName(int i)
{
    if(i < staticContacts)
    {
        const int h = i / (int{SubCount} * PartCount), sub = (i / PartCount) % SubCount, part = i % PartCount;
        const bool bodyPart = part == Torso || part == Head || part == Legs;
        return za::String(h == HAND_MAIN ? "main " : "off ") + subNames[sub] + "/" + (bodyPart ? "" : "other ") + partNames[part];
    }
    const int k = i - staticContacts;
    return za::String("off ") + subNames[k / SubCount] + "/main " + subNames[k % SubCount];
}

void drawCaps(const Caps& caps, const glm::vec4& colour)
{
    for(const Cap& k : caps)
    {
        if(glm::distance(k.a, k.b) < 1e-3f)
        {
            lines::point(k.a, k.r * 2.f, colour);
        }
        else
        {
            lines::line(k.a, k.b, k.r * 2.f, colour, colour);
        }
    }
}

void debugDraw(const hands::State& s)
{
    const glm::vec4 body{0.3f, 0.6f, 1.f, 0.25f}, arm{0.3f, 1.f, 0.5f, 0.3f}, gad{1.f, 0.8f, 0.2f, 0.4f};
    const glm::vec4 handC{1.f, 1.f, 1.f, 0.35f}, weap{1.f, 0.4f, 0.3f, 0.35f}, stock{0.6f, 0.3f, 0.3f, 0.35f};
    drawCaps(scene.part[Torso][0], body);
    drawCaps(scene.part[Head][0], body);
    drawCaps(scene.part[Legs][0], body);
    for(int h = 0; h < 2; h++)
    {
        drawCaps(scene.part[UpperArm][h], arm);
        drawCaps(scene.part[Forearm][h], arm);
        drawCaps(scene.part[Gadget][h], gad);
        // The movers where they are drawn.
        Caps moved = scene.hand[h];
        for(Cap& k : moved)
        {
            k.a += drawn[h];
            k.b += drawn[h];
        }
        drawCaps(moved, handC);
        moved = scene.weapon[h];
        for(Cap& k : moved)
        {
            k.a += drawn[h];
            k.b += drawn[h];
        }
        for(const Cap& k : moved)
        {
            drawCaps(Caps{k}, k.back ? stock : weap);
        }
        if(glm::length(drawn[h]) > 1e-3f)
        {
            lines::line(s.pos[h], s.pos[h] + drawn[h], 0.2f, glm::vec4{1.f, 1.f, 0.f, 1.f}, glm::vec4{1.f, 1.f, 0.f, 1.f});
        }
    }
}

// vr_debug_body_collide: each contact's change printed (in, let go, clear), and one line a frame into
// body_collide_trace.txt (the game directory) while any is on or a hand is pushed: the time; per hand (main, off) its
// tracked place (world units), its drawn offset (x y z and length, cm) and this frame's target (cm); then each contact
// on: its name, block or pass, its weight, how far through (0..1) and the push the tracked pose needs (cm).
void trace(const hands::State& s, const Stats& stats)
{
    static FILE* file = nullptr;
    static Mode shown[contactCount]{};
    if(vr_debug_body_collide.value < 1.f)
    {
        if(file)
        {
            fclose(file);
            file = nullptr;
        }
        return;
    }
    for(int i = 0; i < contactCount; i++)
    {
        const Contact& c = contacts[static_cast<za::SizeT>(i)];
        const Mode now = c.on ? c.mode : Mode::None;
        if(now != shown[i])
        {
            Con_Printf("body collide: t %.3f %s: %s -> %s (through %.2f, push %.1f cm)\n", cl.time, contactName(i).cStr(),
                modeNames[static_cast<int>(shown[i])], modeNames[static_cast<int>(now)], c.share, cm(c.t0));
            shown[i] = now;
        }
    }
    if(!stats.blocking && !stats.passing && drawn[0] == glm::vec3{0.f} && drawn[1] == glm::vec3{0.f})
    {
        return;
    }
    if(!file && !(file = fopen(va("%s/body_collide_trace.txt", com_gamedir), "w")))
    {
        return;
    }
    fprintf(file, "%.4f", cl.time);
    for(int h = 1; h >= 0; h--)
    {
        fprintf(file, " %s %.2f %.2f %.2f drawn %.2f %.2f %.2f %.2f target %.2f", h == HAND_MAIN ? "main" : "off", s.pos[h].x, s.pos[h].y,
            s.pos[h].z, cm(drawn[h].x), cm(drawn[h].y), cm(drawn[h].z), cm(glm::length(drawn[h])), cm(glm::length(target[h])));
    }
    for(int i = 0; i < contactCount; i++)
    {
        const Contact& c = contacts[static_cast<za::SizeT>(i)];
        if(c.on && c.mode != Mode::None)
        {
            fprintf(file, " | %s: %s w %.2f through %.2f push %.2f out %.2f %.2f %.2f", contactName(i).cStr(),
                modeNames[static_cast<int>(c.mode)], c.w, c.share, cm(c.t0), c.n.x, c.n.y, c.n.z);
        }
    }
    fprintf(file, "\n");
    fflush(file);
}

} // namespace

void beginView(hands::State& s)
{
    const bool again = appliedFrame == host_framecount;
    if(!again)
    {
        if(appliedFrame >= 0 && host_framecount - appliedFrame > 2)
        {
            // Not drawn for a while (posing a weapon, the menu's pause): nothing held out from before.
            drawn[0] = drawn[1] = glm::vec3{0.f};
            for(Contact& c : contacts)
            {
                c.mode = Mode::None;
            }
        }
        appliedFrame = host_framecount;
        const float dt = lastTime >= 0.0 ? static_cast<float>(za::clamp(realtime - lastTime, 0.0, 0.1)) : 0.f;
        lastTime = realtime;
        viewOn = vr_body_collide.value != 0.f && s.valid && sightalign::phase() != sightalign::Phase::Capturing;
        Stats stats;
        if(viewOn)
        {
            QVR_PROFILE("body collide");
            solve(s, dt, target, stats);
        }
        else
        {
            target[0] = target[1] = glm::vec3{0.f};
            for(Contact& c : contacts)
            {
                c.mode = Mode::None;
                c.on = false;
            }
            scene = Scene{};
        }
        for(int h = 0; h < 2; h++)
        {
            // Out at once (nearly); back as the hand comes out; through, once let go, a quick slide.
            const bool deeper = glm::dot(target[h] - drawn[h], target[h]) > 0.f;
            const float tau = deeper ? easeIn : realtime - passedAt[h] < passHold ? easePass : easeOut;
            drawn[h] += (target[h] - drawn[h]) * (dt > 0.f ? 1.f - za::exp(-dt / tau) : 1.f);
            if(glm::length(drawn[h]) < 1e-3f && glm::length(target[h]) == 0.f)
            {
                drawn[h] = glm::vec3{0.f};
            }
        }
        trace(s, stats);
    }
    if(vr_debug_body_collide.value >= 2.f && viewOn)
    {
        debugDraw(s);
    }
    for(int h = 0; h < 2; h++)
    {
        tracked[h] = s.pos[h];
        s.pos[h] += drawn[h];
    }
}

void endView(hands::State& s, const Drawn& d)
{
    if(viewOn && rec.frame != host_framecount)
    {
        rec.frame = host_framecount;
        for(int h = 0; h < 2; h++)
        {
            // The hand and its weapon in the hand's frame (as drawn: the push in); not a hand drawn elsewhere than at
            // its controller (on a grip, a ledge: it doesn't move with it).
            const Frame hf = handFrame(s.pos[h], s.rot[h]);
            const bool atController = mobility(s, h) >= 1.f;
            rec.hand[h].clear();
            if(d.hand[h] && atController)
            {
                const int n = static_cast<int>(d.hand[h]->size());
                for(int i = 0; i < n; i++)
                {
                    const glm::vec4& sp = (*d.hand[h])[static_cast<za::SizeT>(i)];
                    Cap k;
                    k.a = k.b = hf.local(glm::vec3{sp});
                    k.r = sp.w;
                    rec.hand[h].pushBack(k);
                }
            }
            rec.weapon[h].clear();
            rec.weaponModel[h] = nullptr;
            const entity_t* e = d.weapon[h];
            if(e && e->model && atController)
            {
                const glm::mat4 m = grasp::shapeToWorld(*e, d.mirrored[h]);
                const glm::vec3 axes{glm::length(glm::vec3{m[0]}), glm::length(glm::vec3{m[1]}), glm::length(glm::vec3{m[2]})};
                if(const Caps* caps = axes.x > 1e-6f && axes.y > 1e-6f && axes.z > 1e-6f ? weaponCaps(*e, axes) : nullptr)
                {
                    const Frame wf = handFrame(s.pos[h], s.visualRot[h]);
                    for(const Cap& k : *caps)
                    {
                        Cap w;
                        w.a = wf.local(glm::vec3{m * glm::vec4{k.a / axes, 1.f}});
                        w.b = wf.local(glm::vec3{m * glm::vec4{k.b / axes, 1.f}});
                        w.r = k.r;
                        w.back = (w.a.x + w.b.x) * 0.5f < -backMargin;
                        rec.weapon[h].pushBack(w);
                    }
                    rec.weaponModel[h] = e->model;
                }
            }
        }
        // The body as posed this frame.
        avatar::Skeleton k;
        rec.body = scene.torsoValid && avatar::skeleton(k);
        if(!rec.body && bodyDrawn() && avatar::skeleton(k))
        {
            scene.torso = avatar::torso(s); // (the first frame: no torso made yet)
            scene.torsoValid = rec.body = true;
        }
        if(rec.body)
        {
            const avatar::Frame& ch = scene.torso.chest;
            const avatar::Frame& pv = scene.torso.pelvis;
            rec.m2w = k.m2w;
            rec.legs = k.legs;
            for(int h = 0; h < 2; h++)
            {
                rec.shoulder[h] = glm::transpose(ch.rot) * (k.shoulder[h] - ch.pos);
                rec.elbow[h] = glm::transpose(ch.rot) * (k.elbow[h] - ch.pos);
                rec.wristLocal[h] = mobility(s, h) >= 1.f;
                rec.wrist[h] = handFrame(s.pos[h], s.rot[h]).local(k.wrist[h]);
                rec.wristWorld[h] = k.wrist[h];
                rec.hip[h] = glm::transpose(pv.rot) * (k.hip[h] - pv.pos);
                rec.knee[h] = glm::transpose(pv.rot) * (k.knee[h] - pv.pos);
                rec.ankle[h] = glm::transpose(pv.rot) * (k.ankle[h] - pv.pos);
            }
        }
        // The gadget's casing (make_gadget.py: 3.8 x 2.6 x 0.7 round its middle, the lid's top at 0.41) as two capsules
        // along it.
        const gadget::Pose& gp = gadget::pose();
        rec.gadget = gp.valid;
        rec.gadgetCaps.clear();
        if(gp.valid)
        {
            rec.gadgetHand = hands::gadgetHand();
            // (In the world while its hand is drawn elsewhere than at its controller: on a grip, a ledge.)
            rec.gadgetLocal = mobility(s, rec.gadgetHand) >= 1.f;
            const Frame gf = rec.gadgetLocal ? handFrame(s.pos[rec.gadgetHand], s.rot[rec.gadgetHand]) : Frame{};
            for(const float y : {-0.9f, 0.9f})
            {
                Cap c;
                c.a = gf.local(gp.origin + gp.axes * (glm::vec3{-1.5f, y, 0.03f} * gp.scale));
                c.b = gf.local(gp.origin + gp.axes * (glm::vec3{1.5f, y, 0.03f} * gp.scale));
                c.r = 0.4f * gp.scale;
                rec.gadgetCaps.pushBack(c);
            }
        }
    }

    // What the game reads: as tracked.
    for(int h = 0; h < 2; h++)
    {
        s.pos[h] = tracked[h]; // (exactly: not less the push, a float's rounding off)
        if(s.muzzleValid[h])
        {
            s.muzzle[h] -= drawn[h];
        }
        if(s.grip2HValid[h])
        {
            s.grip2H[h] -= drawn[h];
        }
    }
}

glm::vec3 drawnOffset(int hand)
{
    return hand == 0 || hand == 1 ? drawn[hand] : glm::vec3{0.f};
}

float elbowSwing(const glm::vec3& shoulder, const glm::vec3& elbow, const glm::vec3& wrist, float radius)
{
    const Caps& torso = scene.part[Torso][0];
    if(!viewOn || !vr_body_collide_elbows.value || torso.empty())
    {
        return 0.f;
    }
    // How far the elbow's joint (with `radius`) is inside the torso, less a centimetre: an arm at rest lies against it.
    const float slack = 0.01f * units::metresToUnits() * units::bodyScale();
    const auto depth = [&](const glm::vec3& p) {
        float most = 0.f;
        for(const Cap& k : torso)
        {
            glm::vec3 a, b;
            closestPoints(p, p, k.a, k.b, a, b);
            most = za::max(most, k.r + radius - slack - glm::distance(a, b));
        }
        return most;
    };
    if(depth(elbow) <= 0.f)
    {
        return 0.f;
    }
    const glm::vec3 axis = wrist - shoulder;
    const float len = glm::length(axis);
    if(len < 1e-3f)
    {
        return 0.f;
    }
    const glm::vec3 ax = axis / len;
    const glm::vec3 centre = shoulder + ax * glm::dot(elbow - shoulder, ax);
    // The least swing either way (4 degree steps, up to 120) that takes it out; else the shallowest.
    float best = 0.f, bestDepth = depth(elbow);
    for(int step = 1; step <= 30; step++)
    {
        for(const float sign : {1.f, -1.f})
        {
            const float angle = glm::radians(4.f * static_cast<float>(step)) * sign;
            const glm::vec3 p = centre + glm::angleAxis(angle, ax) * (elbow - centre);
            const float dp = depth(p);
            if(dp <= 0.f)
            {
                return angle;
            }
            if(dp < bestDepth - 1e-3f)
            {
                bestDepth = dp;
                best = angle;
            }
        }
    }
    return best;
}

void reset()
{
    for(int h = 0; h < 2; h++)
    {
        drawn[h] = target[h] = glm::vec3{0.f};
        passedAt[h] = -1.0;
    }
    for(Contact& c : contacts)
    {
        c = Contact{};
    }
    rec = Record{};
    scene = Scene{};
    fits.clear(); // a model slot reused by the next map or game
    appliedFrame = -1;
    lastTime = -1.0;
    viewOn = false;
}

void bench_f()
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        Con_Printf("vr_body_collide_bench: no hands\n");
        return;
    }
    const int n = Cmd_Argc() > 1 ? za::max(1, Q_atoi(Cmd_Argv(1))) : 1000;
    // The contacts' state is kept as it was (the bench mustn't let go of a contact).
    const za::Array<Contact, contactCount> kept = contacts;
    const double keptPassed[2] = {passedAt[0], passedAt[1]};
    za::Vector<double> us;
    us.reserve(static_cast<za::SizeT>(n));
    Stats stats;
    glm::vec3 out[2];
    for(int i = 0; i < n; i++)
    {
        contacts = kept;
        stats = Stats{};
        const auto t0 = qza::nowNs();
        solve(s, 0.f, out, stats);
        us.pushBack(qza::usSince(t0));
    }
    contacts = kept;
    passedAt[0] = keptPassed[0];
    passedAt[1] = keptPassed[1];
    za::quickSort(us.begin(), us.end());
    int caps = 0;
    for(int h = 0; h < 2; h++)
    {
        caps += static_cast<int>(scene.hand[h].size() + scene.weapon[h].size());
        for(const auto& p : scene.part)
        {
            caps += static_cast<int>(p[h].size());
        }
    }
    Con_Printf("vr_body_collide_bench: %d runs: min %.2f us, median %.2f, max %.2f; %d capsules, %d contacts tested "
               "(%d blocking, %d passing); push main %.2f cm, off %.2f cm\n",
        n, us.front(), us[us.size() / 2], us.back(), caps, stats.tested, stats.blocking, stats.passing, cm(glm::length(out[HAND_MAIN])),
        cm(glm::length(out[HAND_OFF])));
    if(Cmd_Argc() > 2)
    {
        for(int h = 0; h < 2; h++)
        {
            Con_Printf("  %s: at (%.1f %.1f %.1f); hand %d capsules round (%.1f %.1f %.1f) r %.1f; weapon %d round (%.1f %.1f %.1f) r %.1f\n",
                h == HAND_MAIN ? "main" : "off", s.pos[h].x, s.pos[h].y, s.pos[h].z, static_cast<int>(scene.hand[h].size()),
                scene.handB[h].c.x, scene.handB[h].c.y, scene.handB[h].c.z, scene.handB[h].r, static_cast<int>(scene.weapon[h].size()),
                scene.weaponB[h].c.x, scene.weaponB[h].c.y, scene.weaponB[h].c.z, scene.weaponB[h].r);
            for(const Cap& k : scene.weapon[h])
            {
                Con_Printf("    weapon%s: (%.1f %.1f %.1f) - (%.1f %.1f %.1f) r %.2f\n", k.back ? " (stock)" : "", k.a.x, k.a.y, k.a.z,
                    k.b.x, k.b.y, k.b.z, k.r);
            }
            for(int p = 0; p < PartCount; p++)
            {
                for(const Cap& k : scene.part[p][h])
                {
                    Con_Printf("    %s[%d]: (%.1f %.1f %.1f) - (%.1f %.1f %.1f) r %.2f\n", partNames[p], h, k.a.x, k.a.y, k.a.z, k.b.x, k.b.y,
                        k.b.z, k.r);
                }
            }
        }
    }
}

} // namespace qvr::selfcollide
