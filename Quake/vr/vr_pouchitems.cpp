// vr_pouchitems.cpp -- what the ammo pouch holds, laid out and wobbling (vr_pouchitems.hpp).

#include "vr_pouchitems.hpp"

#include "vr_cvars.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_props.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

namespace qvr::pouchitems
{

namespace
{

// make_ammo_pouch.py FRAMES (and make_pouch.py's outline: its back at BACK_X, its front a superellipse of power BULGE_P,
// the inside the outline drawn INNER of the way towards its middle, BACK_X + 0.45 of the depth out).
constexpr Shape shapes[] = {{4.6f, 1.3f, 0.6f}, {4.6f, 3.f, 0.8f}, {4.6f, 3.f, 0.6f}, {4.6f, 3.5f, 0.6f},
    {5.f, 5.f, 0.6f}, {4.6f, 3.6f, 0.6f}, {5.2f, 3.5f, -1.2f}};
constexpr float backX = 0.1f;
constexpr float inner = 0.93f;
constexpr float bulgeP = 6.f;
constexpr int ring = 22;

[[nodiscard]] float innerBack(float d)
{
    return backX + 0.45f * d * (1.f - inner);
}

[[nodiscard]] float innerFront(float d)
{
    return backX + 0.45f * d + 0.55f * d * inner;
}

// The magazines as held (make_mags.py at their shipped Size: the nailgun's 0.55, the others' 0.45): width across the
// pouch, thickness out of the body; the gaps between them.
struct MagFoot
{
    const char* model;
    float width, thick;
};
constexpr MagFoot magFeet[4] = {{"progs/vr_mag_nail.mdl", 2.068f, 1.111f}, {"progs/vr_mag_snail.mdl", 2.232f, 1.089f},
    {"progs/vr_mag_light.mdl", 1.665f, 1.44f}, {"progs/vr_mag_npistol.mdl", 1.726f, 1.227f}};
// Kind 2..4's (kind & 16: the nail pistol's magazines, standing as the nailgun's: make_nailpistol_mags.py at Size 0.52).
[[nodiscard]] const MagFoot& footFor(int kind)
{
    return (kind & 16) ? magFeet[3] : magFeet[za::clamp(kind & 7, 2, 4) - 2];
}
// A full magazine's rounds, for kinds 2..4 (QC VR_Reload_PouchMagCount).
[[nodiscard]] int magRounds(int kind)
{
    return (kind & 16) ? 12 : (kind & 7) == 2 ? 24 : 36;
}
constexpr float magGapY = 0.16f;
constexpr float magGapX = 0.12f;

// The shells' pile (Misc/quakevr/pouch_pile.py shells 0.04: each shell dropped in at a random turn, resting at the
// lowest place it fits without touching the others or the walls, the middle preferred; 0.05 apart), in the order they
// went in, at the shipped held Size 1.25: middle (pouch units), its axis's turn about up and its tilt (degrees).
struct PileShell
{
    float x, y, z, yaw, pitch;
};
constexpr PileShell pile[most] = {
    {1.579f, -0.003f, 1.182f, 162.9f, -2.6f}, {1.364f, -1.000f, 1.151f, 152.7f, -0.4f},
    {1.847f, 1.236f, 1.180f, 145.0f, -2.5f}, {1.645f, -2.127f, 1.237f, 149.2f, -6.6f},
    {1.681f, 2.392f, 1.210f, 158.7f, -4.6f}, {1.503f, -0.019f, 1.922f, 46.0f, 3.4f},
    {1.358f, -3.255f, 1.146f, 132.6f, -0.1f}, {1.957f, 1.813f, 1.855f, 143.1f, 4.3f},
    {1.372f, -1.265f, 1.891f, 34.3f, 0.4f}, {1.490f, 3.299f, 1.649f, 7.5f, -1.0f},
    {1.690f, -2.945f, 1.938f, 113.6f, 5.2f}, {0.816f, 1.127f, 2.246f, 93.9f, 0.1f},
    {2.415f, -2.200f, 2.306f, 86.5f, -2.9f}, {1.692f, 3.064f, 2.391f, 143.3f, 0.5f},
    {1.194f, -0.968f, 2.572f, 44.8f, -1.9f}, {0.938f, -3.065f, 2.270f, 111.8f, 3.2f},
    {2.459f, 0.961f, 2.604f, 90.7f, 1.4f}, {1.437f, 1.542f, 2.656f, 101.1f, 5.1f},
    {1.649f, -2.970f, 2.703f, 103.7f, 3.4f}, {0.712f, -0.042f, 3.097f, 81.5f, -1.6f}};
constexpr float pileSize = 1.25f; // the held Size it was made at
constexpr float pileDepth = 3.f;  // and its frame's depth (shapes[1])

// The grenades standing (Quake's own models, the user's game's: progs/grenade.mdl, the multi-grenade's mervup.mdl with
// the kind's 8, proxbomb.mdl), drawn as the hands take them (QC vr_grenade.qc VR_HandGrenade_Make): muted (skin 1,
// unarmed), nose up (the grenade's +x, the others' +z), at their size in the hand (vr_grenade_scale, vr_prox_scale) times
// a share to stay inside the leather (the grenade 0.96, was 0.9; the multi-grenade 0.805, was 0.7: the author's notes
// asked 5-8% and 15-20% bigger, nearer the held ones, and frame 4 grew from 4.8 x 4.4 to 5.0 x 5.0 so five fit; the
// proximity grenade a third, Quake's is near the pouch's width): their tops' height, their radius at the shipped scales
// (0.78, 0.47) and the share they were measured at (shareAt), the scale it was measured at.
struct GrenadeFoot
{
    const char* model;
    float share, top, radius, at, shareAt;
    bool alongZ;
};
constexpr GrenadeFoot grenadeFeet[3] = {{"progs/grenade.mdl", 0.96f, 3.4f, 1.08f, 0.78f, 0.9f, false},
    {"progs/mervup.mdl", 0.805f, 3.3f, 1.08f, 0.78f, 0.7f, true},
    {"progs/proxbomb.mdl", 0.36f, 3.1f, 0.92f, 0.47f, 0.36f, true}};
constexpr float grenadeGap = 0.1f;
constexpr float grenadeSpin[5] = {20.f, 140.f, 75.f, 250.f, 310.f};
constexpr float grenadeLift[5] = {0.f, -0.06f, 0.04f, -0.03f, 0.05f};

// The rockets lying (make_rounds.py's, nose along its +x): which way each points along the pouch, how far along it each
// lies off the middle (towards its nose: the butt's fins kept off the walls), a little turned.
constexpr float rocketDir[5] = {1.f, -1.f, -1.f, 1.f, -1.f};
constexpr float rocketShift[5] = {0.15f, 0.23f, 0.1f, 0.25f, 0.17f};
constexpr float rocketYaw[5] = {1.2f, 0.8f, 0.5f, 1.4f, 0.9f}; // (degrees: each butt turned off its wall)
constexpr float rocketGap = 0.06f;

// The wobble: damped springs (the rounds' lag behind the pouch, world units along its axes) driven by the pouch's
// acceleration (its velocity smoothed first: tracking's jitter is no shake). Their frequency, the time a swing takes to
// die down (to 5%) and how hard a move drives them are settings (vr_ammo_pouch_wobble_freq/_settle/_intensity).
constexpr float wobbleDrive = 0.25f;  // the share of the acceleration that moves them (at intensity 1)
constexpr float wobbleAmp = 0.3f;     // how far they sway at most (pouch units: their tops, a lying one's middle)
constexpr float wobbleAmpUp = 0.12f;  // and lift
constexpr float wobbleSlide = 0.12f;  // each its own: a lying one sways with the rest, its own spring this far off it
constexpr float wobbleTilt = 0.08f;   // and its ends either way of that (it tilts)
constexpr float wobbleSmooth = 15.f;  // the velocity's smoothing rate (1/s)
constexpr float wobbleGap = 0.02f;    // each its own: kept this clear of the others and the walls (or as at rest),
                                      // times the strength over 1
constexpr float wobbleSoft = 0.1f;    // the leather gives: they may press this far into it (0.1-0.3 thick)
constexpr int wobbleIters = 10;       // the passes keeping them clear
constexpr float wobbleEps = 0.02f;    // (the gaps' slopes: a step either way)

struct Spring
{
    glm::vec3 s{0.f}, v{0.f};
};

// Each round's own spring (vr_ammo_pouch_wobble_each, by its place in the layout): its frequency, damping and drive a
// little off the others', the pouch's acceleration turned a little about up for it; a lying one has a second, quicker
// one driven across, tilting it (its ends sway apart).
struct Own
{
    float omega, damping, drive, c, s;
};
Own own[most];

struct Wobble
{
    bool valid{false};
    double time{0.0};
    glm::vec3 last{0.f}, vel{0.f}; // world units, along the pouch's axes
    Spring shared;                 // one for all (vr_ammo_pouch_wobble_each 0; vr_dumpview's wobble)
    Spring each[most], tilt[most];
    float k{1.f};
};
Wobble wobbleState;

// How a round is moved: a standing one's top (e0's x, y: it leans about its foot), a lying one's two ends (e0 at p0,
// e1 at p1: it shifts and tilts); pouch units.
struct Ctl
{
    glm::vec3 e0{0.f}, e1{0.f};
};

[[nodiscard]] float& comp(Ctl& c, int k)
{
    return k < 3 ? c.e0[k] : c.e1[k - 3];
}

[[nodiscard]] float dot(const Ctl& a, const Ctl& b)
{
    return glm::dot(a.e0, b.e0) + glm::dot(a.e1, b.e1);
}

void addScaled(Ctl& a, const Ctl& b, float f)
{
    a.e0 += b.e0 * f;
    a.e1 += b.e1 * f;
}

// The rest layout's keep-clear rules (rebuilt when the layout changes): each near pair's gap and its slopes along both
// rounds' Ctl (the gap of two convex shapes never falls below that straight line as they shift: a safe guess); the
// inside's walls as lines (it is convex: how far inside a point is, is the least of its distances to them) and each
// round's points kept off them: a lying one's ends (and off the floor), a standing one's axis or a magazine's corners
// where the rim is (above it the leather is gone). Kept by a few passes of the smallest correction.
constexpr float rimZ = 1.6f; // (make_ammo_pouch.py Z_TOP 1.5 and its roll)
struct Pair
{
    int i, j;
    float g, inv;
    Ctl gi, gj; // (magazines; two lying ones or two standing capsules are kept apart where they are: no slopes)
    bool lying, standing;
};
struct WallPoint
{
    glm::vec3 p; // at rest (pouch units)
    float f;     // its share of e0 (a standing one's: its height's share of its top's), or 0: e0 all, -1: e1 all
    float r;
    unsigned reachable; // the walls it can reach (lines' bits)
};
static_assert(ring <= 32);
constexpr int pairsMost = most * (most - 1) / 2;
struct Rest
{
    int n{-1}, frame{0};
    float reach{0.f}, floor{0.f};
    Item items[most];
    Pair pairs[pairsMost];
    int pairCount{0};
    WallPoint walls[most][4];
    int wallCount[most];
    glm::vec3 ends[most][2]; // a capsule's p0, p1 at rest
    float lean[most][2];     // and a standing one's: their heights' shares of its top's
    glm::vec3 lines[ring]; // (inward x, y; offset: inside by x * px + y * py - z)
    float margin{0.f};     // kept this clear (wobbleGap, more as they sway further: the guesses err more)
};
Rest restCache;

[[nodiscard]] float rad(float deg)
{
    return deg * 3.14159265f / 180.f;
}

[[nodiscard]] glm::mat3 aboutZ(float deg)
{
    const float c = za::cos(rad(deg)), s = za::sin(rad(deg));
    return glm::mat3{glm::vec3{c, s, 0.f}, glm::vec3{-s, c, 0.f}, glm::vec3{0.f, 0.f, 1.f}};
}

[[nodiscard]] constexpr float frac(float x)
{
    return x - static_cast<float>(static_cast<int>(x));
}

[[nodiscard]] float softClamp(float x, float amp)
{
    return amp > 0.f ? x / za::sqrt(1.f + (x / amp) * (x / amp)) : 0.f;
}

[[nodiscard]] float heldSize(const qmodel_t* model)
{
    return props::drawnSize(model);
}

int layoutShells(int left, Item (&out)[most])
{
    qmodel_t* const model = view::viewModel("progs/vr_shell_live.mdl");
    if(!model)
    {
        return 0;
    }
    const int n = za::min(left, za::clamp(static_cast<int>(vr_ammo_pouch_shells_max.value), 1, most));
    const Shape& sh = shapes[1];
    const float ds = heldSize(model);
    const float sf = ds / pileSize;
    const glm::vec3 c0{innerCentreX(pileDepth), 0.f, sh.floor};
    const float r = za::max(za::max(-model->mins[1], model->maxs[1]), za::max(-model->mins[2], model->maxs[2]));
    for(int i = 0; i < n; i++)
    {
        const PileShell& p = pile[i];
        Item& it = out[i];
        it = Item{};
        it.model = model;
        it.lying = true;
        const float flip = (i * 7) % 3 == 0 ? -1.f : 1.f; // (brass either way: the same line)
        const glm::vec3 a = glm::vec3{za::cos(rad(p.pitch)) * za::cos(rad(p.yaw)),
                                za::cos(rad(p.pitch)) * za::sin(rad(p.yaw)), za::sin(rad(p.pitch))} *
                            flip;
        const glm::vec3 y = glm::normalize(glm::cross(glm::vec3{0.f, 0.f, 1.f}, a));
        const glm::vec3 z = glm::cross(a, y);
        const float roll = rad(static_cast<float>((i * 53) % 360));
        it.axes = glm::mat3{a, y * za::cos(roll) + z * za::sin(roll), z * za::cos(roll) - y * za::sin(roll)};
        it.size = ds;
        it.at = c0 + (glm::vec3{p.x, p.y, p.z} - c0) * sf;
        it.pivot = it.at;
        it.gain = 0.6f + 0.4f * za::clamp((it.at.z - sh.floor) / 2.5f, 0.f, 1.f);
        it.radius = r * ds;
        it.p0 = glm::vec3{model->mins[0] + r, 0.f, 0.f};
        it.p1 = glm::vec3{model->maxs[0] - r, 0.f, 0.f};
    }
    return n;
}

int layoutMags(int kind, bool variant, int left, Item (&out)[most])
{
    const MagFoot& f = footFor(kind);
    qmodel_t* const model = view::viewModel(f.model);
    if(!model)
    {
        return 0;
    }
    const int n = magsShown(kind, left);
    const Shape& sh = shapes[frameFor(kind, left)];
    const float ds = heldSize(model);
    for(int j = 0; j < n; j++)
    {
        const glm::vec2 at = magSlot(kind, j, n);
        Item& it = out[j];
        it = Item{};
        it.model = model;
        it.skin = variant ? 1 : 0;
        // (The last one shown the reserve's rest: one nail left, its top nails show it.)
        it.rounds = za::clamp(left - j * magRounds(kind), 0, magRounds(kind));
        // Feed end up, its width (+x) across the pouch, its thickness out of the body (make_ammo_pouch.py's way).
        it.axes = glm::mat3{glm::vec3{0.f, 1.f, 0.f}, glm::vec3{-1.f, 0.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}};
        it.size = ds;
        it.at = glm::vec3{at.x, at.y, magTop - model->maxs[2] * ds};
        it.pivot = glm::vec3{at.x, at.y, sh.floor};
        it.height = magTop - sh.floor;
        it.gain = 0.8f + 0.2f * frac(static_cast<float>(j) * 0.618f + 0.3f);
        it.box = true;
        it.lo = glm::vec3{model->mins[0], model->mins[1], model->mins[2]};
        it.hi = glm::vec3{model->maxs[0], model->maxs[1], model->maxs[2]};
    }
    return n;
}

int layoutGrenades(int kind, bool variant, int left, Item (&out)[most])
{
    const GrenadeFoot& g = grenadeFeet[kind == 7 ? 2 : variant ? 1 : 0];
    qmodel_t* const model = view::viewModel(g.model);
    if(!model)
    {
        return 0;
    }
    const int n = za::min(left, 5);
    const Shape& sh = shapes[frameFor(kind, left)];
    const float ds = heldSize(model);
    const float r = g.radius * ds / g.at * g.share / g.shareAt;
    const float d = 2.f * r + grenadeGap;
    const float cx = innerCentreX(sh.depth);
    // The back row as wide apart as the pouch lets (the front pair nests deeper into its gaps).
    const float pitch = za::min(sh.halfW * 0.93f * inner - r - 0.15f, d * 1.45f);
    const float dx = za::sqrt(za::max(0.f, d * d - pitch * pitch * 0.25f));
    for(int j = 0; j < n; j++)
    {
        glm::vec2 at;
        if(n <= 3)
        {
            at = glm::vec2{cx, (static_cast<float>(j) - static_cast<float>(n - 1) * 0.5f) * pitch};
        }
        else if(j < 3)
        {
            at = glm::vec2{cx - dx * 0.5f, static_cast<float>(j - 1) * pitch};
        }
        else
        {
            at = glm::vec2{cx + dx * 0.5f, (j == 3 ? -0.5f : 0.5f) * pitch};
        }
        Item& it = out[j];
        it = Item{};
        it.model = model;
        it.skin = 1; // (muted: unarmed; make_grenade_skins.py)
        const glm::mat3 spin = aboutZ(grenadeSpin[j]);
        // Nose up: the grenade's +x up (its +z out), the multi-grenade's and the proximity grenade's +z up.
        it.axes = g.alongZ ? spin
                           : glm::mat3{glm::vec3{0.f, 0.f, 1.f}, spin * glm::vec3{0.f, -1.f, 0.f}, spin * glm::vec3{1.f, 0.f, 0.f}};
        it.share = g.share;
        it.size = g.share * ds;
        const float nose = g.alongZ ? model->maxs[2] : model->maxs[0];
        const float tail = g.alongZ ? model->mins[2] : model->mins[0];
        const float top = g.top + grenadeLift[j];
        it.at = glm::vec3{at.x, at.y, top - nose * it.size};
        it.pivot = glm::vec3{at.x, at.y, sh.floor};
        it.height = top - sh.floor;
        it.gain = 0.8f + 0.2f * frac(static_cast<float>(j) * 0.618f + 0.3f);
        it.radius = r;
        // (A cylinder: from its foot or the floor, whichever is higher, to its top; model units along its up.)
        const float foot = za::max(tail, (sh.floor - it.at.z) / it.size);
        it.p0 = (g.alongZ ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f}) * (foot + r / it.size);
        it.p1 = (g.alongZ ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f}) * (nose - r / it.size);
    }
    return n;
}

int layoutRockets(bool variant, int left, Item (&out)[most])
{
    qmodel_t* const model = view::viewModel("progs/vr_round_rocket.mdl");
    if(!model)
    {
        return 0;
    }
    const int n = za::min(left, 5);
    const Shape& sh = shapes[6];
    const float ds = heldSize(model);
    const float r = za::max(za::max(-model->mins[1], model->maxs[1]), za::max(-model->mins[2], model->maxs[2])) * ds;
    const float xb = innerBack(sh.depth) + r + 0.07f;
    const float xf = innerFront(sh.depth) - r - 0.07f;
    const float d = 2.f * r + rocketGap;
    // (Each on the one below it, against the other wall, and clear of the one below that: a hexagonal stack.)
    const float dz = za::max(za::sqrt(za::max(0.f, d * d - (xf - xb) * (xf - xb))), d * 0.5f);
    const float mid = (model->mins[0] + model->maxs[0]) * 0.5f;
    for(int j = 0; j < n; j++)
    {
        const glm::vec3 c = n == 1 ? glm::vec3{innerCentreX(sh.depth), 0.f, sh.floor + r}
                                   : glm::vec3{j % 2 == 0 ? xb : xf, 0.f, sh.floor + r + static_cast<float>(j) * dz};
        const float dir = n == 1 ? 1.f : rocketDir[j];
        const glm::mat3 turn = aboutZ(rocketYaw[j] * dir * (n > 1 && j % 2 == 1 ? -1.f : 1.f));
        Item& it = out[j];
        it = Item{};
        it.model = model;
        it.skin = variant ? 1 : 0;
        it.lying = true;
        it.axes = glm::mat3{turn * glm::vec3{0.f, dir, 0.f}, turn * glm::vec3{-dir, 0.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}};
        it.size = ds;
        it.at = c + it.axes[0] * ((rocketShift[j] - mid) * ds);
        it.pivot = c;
        it.gain = 0.6f + 0.4f * za::clamp((c.z - sh.floor) / 2.5f, 0.f, 1.f);
        it.radius = r;
        it.p0 = glm::vec3{model->mins[0] + r / ds, 0.f, 0.f};
        // (Its body: the nose's taper, make_rounds.py ROCKET_NOSE_LEN, 1.97 units, within the capsule's rounded end.)
        it.p1 = glm::vec3{model->maxs[0] - 1.97f, 0.f, 0.f};
    }
    return n;
}

// --- vr_ammo_pouch_layout: every kind's layouts, 1 to the most, checked ------------------------------------------

// The inside's outline at frame `frame` (make_pouch.py outline(depth, INNER), ring points).
void innerOutline(int frame, glm::vec2 (&pts)[ring])
{
    const Shape& sh = shapes[frame];
    const float mid = backX + sh.depth * 0.45f;
    constexpr int front = ring - 2;
    for(int k = 0; k < front; k++)
    {
        const float t = static_cast<float>(k) / static_cast<float>(front - 1);
        const float y = sh.halfW * za::cos(3.14159265f * t);
        const float a = za::min(1.f, za::fabs(y) / sh.halfW);
        const float x = backX + sh.depth * za::pow(za::max(0.f, 1.f - za::pow(a, bulgeP)), 1.f / bulgeP);
        pts[k] = glm::vec2{x, y};
    }
    pts[front] = glm::vec2{backX, -sh.halfW * 0.93f};
    pts[front + 1] = glm::vec2{backX, sh.halfW * 0.93f};
    for(glm::vec2& p : pts)
    {
        p = glm::vec2{mid + (p.x - mid) * inner, p.y * inner};
    }
}

// How far inside the outline `p` is (negative: outside).
[[nodiscard]] float insideBy(const glm::vec2 (&pts)[ring], const glm::vec2& p)
{
    bool in = false;
    float best = 1e9f;
    for(int i = 0; i < ring; i++)
    {
        const glm::vec2 a = pts[i], b = pts[(i + 1) % ring];
        if((a.y > p.y) != (b.y > p.y) && p.x < a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y))
        {
            in = !in;
        }
        const glm::vec2 e = b - a;
        const float t = za::clamp(glm::dot(p - a, e) / za::max(glm::dot(e, e), 1e-9f), 0.f, 1.f);
        best = za::min(best, glm::length(p - a - e * t));
    }
    return in ? best : -best;
}

// The closest points of two segments (at s along the first, t along the second) and how far apart they are.
[[nodiscard]] float segClosest(glm::vec3 p1, glm::vec3 q1, glm::vec3 p2, glm::vec3 q2, float& sOut, float& tOut);

[[nodiscard]] float segDist(glm::vec3 p1, glm::vec3 q1, glm::vec3 p2, glm::vec3 q2)
{
    float s, t;
    return segClosest(p1, q1, p2, q2, s, t);
}

float segClosest(glm::vec3 p1, glm::vec3 q1, glm::vec3 p2, glm::vec3 q2, float& sOut, float& tOut)
{
    const glm::vec3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    const float a = glm::dot(d1, d1), e = glm::dot(d2, d2), f = glm::dot(d2, r);
    const float c = glm::dot(d1, r), b = glm::dot(d1, d2);
    const float den = a * e - b * b;
    float s = den > 1e-9f ? za::clamp((b * f - c * e) / den, 0.f, 1.f) : 0.f;
    float t = e > 1e-9f ? (b * s + f) / e : 0.f;
    if(t < 0.f)
    {
        t = 0.f;
        s = a > 1e-9f ? za::clamp(-c / a, 0.f, 1.f) : 0.f;
    }
    else if(t > 1.f)
    {
        t = 1.f;
        s = a > 1e-9f ? za::clamp((b - c) / a, 0.f, 1.f) : 0.f;
    }
    sOut = s;
    tOut = t;
    return glm::length((p1 + d1 * s) - (p2 + d2 * t));
}

[[nodiscard]] glm::vec3 itemPoint(const Item& it, const glm::vec3& p)
{
    return it.at + it.axes * (p * it.size);
}

// A magazine's cross-section at height z (pouch units): its four corners (its box's, along its own axes about its
// axis's point at that height).
void magSection(const Item& it, float z, glm::vec2 (&c)[4])
{
    const glm::vec3 lo = itemPoint(it, glm::vec3{0.5f * (it.lo.x + it.hi.x), 0.5f * (it.lo.y + it.hi.y), it.lo.z});
    const glm::vec3 hi = itemPoint(it, glm::vec3{0.5f * (it.lo.x + it.hi.x), 0.5f * (it.lo.y + it.hi.y), it.hi.z});
    const float t = za::clamp((z - lo.z) / za::max(hi.z - lo.z, 1e-6f), 0.f, 1.f);
    const glm::vec3 m = lo + (hi - lo) * t;
    const glm::vec3 ex = it.axes[0] * (0.5f * (it.hi.x - it.lo.x) * it.size);
    const glm::vec3 ey = it.axes[1] * (0.5f * (it.hi.y - it.lo.y) * it.size);
    const glm::vec2 o{m.x, m.y}, a{ex.x, ex.y}, b{ey.x, ey.y};
    c[0] = o - a - b;
    c[1] = o + a - b;
    c[2] = o + a + b;
    c[3] = o - a + b;
}

// The gap between two convex quads (separating axes; negative: overlapping by).
[[nodiscard]] float quadGap(const glm::vec2 (&a)[4], const glm::vec2 (&b)[4])
{
    float best = -1e9f;
    for(int q = 0; q < 2; q++)
    {
        const glm::vec2(&p)[4] = q == 0 ? a : b;
        for(int i = 0; i < 4; i++)
        {
            const glm::vec2 e = p[(i + 1) % 4] - p[i];
            const glm::vec2 nrm = glm::normalize(glm::vec2{-e.y, e.x});
            float amin = 1e9f, amax = -1e9f, bmin = 1e9f, bmax = -1e9f;
            for(int k = 0; k < 4; k++)
            {
                amin = za::min(amin, glm::dot(a[k], nrm));
                amax = za::max(amax, glm::dot(a[k], nrm));
                bmin = za::min(bmin, glm::dot(b[k], nrm));
                bmax = za::max(bmax, glm::dot(b[k], nrm));
            }
            best = za::max(best, za::max(bmin - amax, amin - bmax));
        }
    }
    return best;
}

struct Check
{
    float gap, wall, cx, cy;
};

// How far inside the walls a round is (above the floor; negative: through them): a capsule's ends and its middle, a
// radius round; a magazine's corners at the floor and its top.
[[nodiscard]] float wallGap(const Item& a, const Shape& sh, const glm::vec2 (&outline)[ring])
{
    float wall = 1e9f;
    if(a.box)
    {
        for(const float z : {sh.floor, magTop})
        {
            glm::vec2 c[4];
            magSection(a, z, c);
            for(const glm::vec2& p : c)
            {
                wall = za::min(wall, insideBy(outline, p));
            }
        }
        return wall;
    }
    for(int s = 0; s <= 4; s++)
    {
        const glm::vec3 p = itemPoint(a, a.p0 + (a.p1 - a.p0) * (static_cast<float>(s) * 0.25f));
        wall = za::min(wall, insideBy(outline, glm::vec2{p.x, p.y}) - a.radius);
        if(a.lying)
        {
            wall = za::min(wall, p.z - a.radius - sh.floor + 0.03f);
        }
    }
    return wall;
}

// As wallGap, but a standing round only up to the rim (its leather's top: above it, nothing to go through).
[[nodiscard]] float rimWallGap(const Item& a, const Shape& sh, const glm::vec2 (&outline)[ring])
{
    if(a.lying)
    {
        return wallGap(a, sh, outline);
    }
    const float z = za::min(rimZ, a.pivot.z + a.height);
    if(a.box)
    {
        glm::vec2 c[4];
        magSection(a, z, c);
        float wall = 1e9f;
        for(const glm::vec2& p : c)
        {
            wall = za::min(wall, insideBy(outline, p));
        }
        return wall;
    }
    const glm::vec3 p0 = itemPoint(a, a.p0), p1 = itemPoint(a, a.p1);
    const float t = za::clamp((z - p0.z) / (za::fabs(p1.z - p0.z) > 1e-6f ? p1.z - p0.z : 1e-6f), 0.f, 1.f);
    const glm::vec3 p = p0 + (p1 - p0) * t;
    return insideBy(outline, glm::vec2{p.x, p.y}) - a.radius;
}

// The gap between two rounds (pouch units; negative: through each other).
[[nodiscard]] float pairGap(const Item& a, const Item& b, const Shape& sh)
{
    if(a.box && b.box)
    {
        float gap = 1e9f;
        for(const float z : {sh.floor, 0.5f * (sh.floor + magTop), magTop})
        {
            glm::vec2 ca[4], cb[4];
            magSection(a, z, ca);
            magSection(b, z, cb);
            gap = za::min(gap, quadGap(ca, cb));
        }
        return gap;
    }
    return segDist(itemPoint(a, a.p0), itemPoint(a, a.p1), itemPoint(b, b.p0), itemPoint(b, b.p1)) - a.radius - b.radius;
}

// The gap between the closest two (pouch units; negative: through each other), how far inside the walls the nearest one
// is (above the floor), and their middles' mean off the inside's middle.
[[nodiscard]] Check check(const Item* items, int n, int frame)
{
    const Shape& sh = shapes[frame];
    glm::vec2 outline[ring];
    innerOutline(frame, outline);
    Check out{1e9f, 1e9f, 0.f, 0.f};
    for(int i = 0; i < n; i++)
    {
        const Item& a = items[i];
        const glm::vec3 mid = a.box ? itemPoint(a, glm::vec3{0.f, 0.f, 0.f}) : 0.5f * (itemPoint(a, a.p0) + itemPoint(a, a.p1));
        out.cx += mid.x / static_cast<float>(n);
        out.cy += mid.y / static_cast<float>(n);
        out.wall = za::min(out.wall, wallGap(a, sh, outline));
        for(int j = i + 1; j < n; j++)
        {
            out.gap = za::min(out.gap, pairGap(a, items[j], sh));
        }
    }
    out.cx -= innerCentreX(sh.depth);
    return out;
}

// --- The wobble ---------------------------------------------------------------------------------------------------

// A round moved by `c`: a standing one leans about its foot (its top by e0's x, y), a lying one's ends go by e0 and e1
// (it shifts by their mean and turns from the one to the other).
void applyCtl(Item& it, const Ctl& c)
{
    if(!it.lying)
    {
        const glm::vec2 d{c.e0.x, c.e0.y};
        const float len = glm::length(d);
        if(len < 1e-5f)
        {
            return;
        }
        const float angle = za::atan2(len, za::max(it.height, 0.5f));
        const glm::vec3 axis{-d.y / len, d.x / len, 0.f}; // (up crossed with the lean: its top goes along d)
        const glm::mat3 turn = glm::mat3_cast(glm::angleAxis(angle, axis));
        it.at = it.pivot + turn * (it.at - it.pivot);
        it.axes = turn * it.axes;
        return;
    }
    const glm::vec3 p0 = itemPoint(it, it.p0), p1 = itemPoint(it, it.p1);
    const glm::vec3 a = p1 - p0, b = a + (c.e1 - c.e0);
    const glm::vec3 cr = glm::cross(a, b);
    const float sl = glm::length(cr);
    if(sl > 1e-6f)
    {
        const glm::mat3 turn = glm::mat3_cast(glm::angleAxis(za::atan2(sl, glm::dot(a, b)), cr / sl));
        const glm::vec3 mid = 0.5f * (p0 + p1);
        it.at = mid + turn * (it.at - mid);
        it.axes = turn * it.axes;
    }
    const glm::vec3 shift = 0.5f * (c.e0 + c.e1);
    it.at += shift;
    it.pivot += shift;
}

// A round's Ctl for a sway `a` (pouch units: out, left, up; a lying one only lifts) and a tilt `t` (a lying one's ends
// either way of it), times its share.
[[nodiscard]] Ctl ctlFor(const Item& it, const glm::vec3& a, const glm::vec3& t)
{
    if(!it.lying)
    {
        const glm::vec3 d = glm::vec3{a.x, a.y, 0.f} * it.gain;
        return Ctl{d, d};
    }
    const glm::vec3 base = glm::vec3{a.x, a.y, za::max(0.f, a.z)} * it.gain;
    return Ctl{base + t * it.gain, base - t * it.gain};
}

// A lying one's own sway off the rest's, as far as it may slide.
[[nodiscard]] glm::vec3 slideOff(const glm::vec3& d, float strength)
{
    const float m = wobbleSlide * strength;
    return glm::vec3{za::clamp(d.x, -m, m), za::clamp(d.y, -m, m), za::clamp(d.z, -m, m)};
}

// How far a round's Ctl reaches at wobble strength `strength` (two farther apart than twice it never meet).
[[nodiscard]] float reachAt(float strength)
{
    return strength * (wobbleAmp * 1.42f + wobbleAmpUp + wobbleTilt * 1.74f);
}

[[nodiscard]] float invOf(float x)
{
    return x > 1e-8f ? 1.f / x : 0.f;
}

// The keep-clear rules for a rest layout: each pair near enough to meet, its gap and slopes (a step either way along
// each of both rounds' Ctl: a standing one's 2, a lying one's 6); each round's room to the walls and its slope.
void buildRest(const Item* items, int n, int frame, float reach)
{
    Rest& r = restCache;
    r.n = n;
    r.frame = frame;
    r.reach = reach;
    r.margin = wobbleGap * za::max(1.f, 1.5f * reach / reachAt(1.f) - 0.5f);
    r.pairCount = 0;
    const Shape& sh = shapes[frame];
    glm::vec2 outline[ring];
    innerOutline(frame, outline);
    const auto moved = [&](int i, int k, float by) {
        Item it = items[i];
        Ctl c;
        comp(c, k) = by;
        applyCtl(it, c);
        return it;
    };
    // (A step either way: the steeper slope. At rest a gap is often the least of two equal ones, a magazine's at its
    // foot and its top, two lying side by side at either end: one shrinks one way, neither the other; a mean would
    // halve it.)
    const auto slope = [](float g, float up, float down) {
        const float a = (up - g) / wobbleEps, b = (g - down) / wobbleEps;
        return za::fabs(a) > za::fabs(b) ? a : b;
    };
    r.floor = sh.floor;
    glm::vec2 centre{0.f};
    for(const glm::vec2& p : outline)
    {
        centre += p / static_cast<float>(ring);
    }
    for(int k = 0; k < ring; k++)
    {
        const glm::vec2 a = outline[k], e = outline[(k + 1) % ring] - a;
        if(glm::dot(e, e) < 1e-10f)
        {
            r.lines[k] = glm::vec3{0.f, 0.f, -1e9f};
            continue;
        }
        glm::vec2 nrm = glm::normalize(glm::vec2{-e.y, e.x});
        nrm = glm::dot(nrm, centre - a) < 0.f ? -nrm : nrm;
        r.lines[k] = glm::vec3{nrm.x, nrm.y, glm::dot(nrm, a)};
    }
    for(int i = 0; i < n; i++)
    {
        const Item& it = items[i];
        r.items[i] = it;
        const int dims = it.lying ? 6 : 2;
        int& wc = r.wallCount[i];
        wc = 0;
        r.ends[i][0] = itemPoint(it, it.p0);
        r.ends[i][1] = itemPoint(it, it.p1);
        for(int k = 0; k < 2; k++)
        {
            r.lean[i][k] = za::max(r.ends[i][k].z - it.pivot.z, 0.f) / za::max(it.height, 0.5f);
        }
        if(it.lying)
        {
            r.walls[i][wc++] = WallPoint{itemPoint(it, it.p0), 0.f, it.radius};
            r.walls[i][wc++] = WallPoint{itemPoint(it, it.p1), -1.f, it.radius};
        }
        else
        {
            const float z = za::min(rimZ, it.pivot.z + it.height);
            const float f = za::max(z - it.pivot.z, 0.f) / za::max(it.height, 0.5f);
            if(it.box)
            {
                glm::vec2 c[4];
                magSection(it, z, c);
                for(const glm::vec2& p : c)
                {
                    r.walls[i][wc++] = WallPoint{glm::vec3{p, z}, f, 0.f};
                }
            }
            else
            {
                // (Its axis's point nearest the rim's height: a small one may stand wholly above it.)
                const glm::vec3 p0 = itemPoint(it, it.p0), p1 = itemPoint(it, it.p1);
                const float t = za::clamp((z - p0.z) / (za::fabs(p1.z - p0.z) > 1e-6f ? p1.z - p0.z : 1e-6f), 0.f, 1.f);
                const glm::vec3 q = p0 + (p1 - p0) * t;
                r.walls[i][wc++] = WallPoint{q, za::max(q.z - it.pivot.z, 0.f) / za::max(it.height, 0.5f), it.radius};
            }
        }
        for(int k = 0; k < wc; k++)
        {
            // (Only the walls it can reach.)
            WallPoint& w = r.walls[i][k];
            w.reachable = 0u;
            for(int q = 0; q < ring; q++)
            {
                const glm::vec3& l = r.lines[q];
                w.reachable |= l.x * w.p.x + l.y * w.p.y - l.z - w.r < reach + r.margin ? 1u << q : 0u;
            }
        }
        for(int j = i + 1; j < n; j++)
        {
            const float g = pairGap(items[i], items[j], sh);
            if(g > 2.f * reach + wobbleGap)
            {
                continue;
            }
            Pair& p = r.pairs[r.pairCount++];
            p = Pair{i, j, g, 0.f, Ctl{}, Ctl{}, it.lying && items[j].lying,
                !it.lying && !items[j].lying && !it.box && !items[j].box};
            for(int k = 0; k < (p.lying || p.standing ? 0 : dims); k++)
            {
                comp(p.gi, k) =
                    slope(g, pairGap(moved(i, k, wobbleEps), items[j], sh), pairGap(moved(i, k, -wobbleEps), items[j], sh));
            }
            for(int k = 0; k < (p.lying || p.standing ? 0 : items[j].lying ? 6 : 2); k++)
            {
                comp(p.gj, k) =
                    slope(g, pairGap(items[i], moved(j, k, wobbleEps), sh), pairGap(items[i], moved(j, k, -wobbleEps), sh));
            }
            p.inv = invOf(dot(p.gi, p.gi) + dot(p.gj, p.gj));
        }
    }
}

[[nodiscard]] bool sameRest(const Item* items, int n, int frame, float reach)
{
    const Rest& r = restCache;
    if(r.n != n || r.frame != frame || r.reach != reach)
    {
        return false;
    }
    for(int i = 0; i < n; i++)
    {
        const Item &a = items[i], &b = r.items[i];
        if(a.model != b.model || a.size != b.size || a.at != b.at || a.axes != b.axes)
        {
            return false;
        }
    }
    return true;
}

// The rest's sway `s` the lying ones (a pile, a stack: a block) share, as far as the walls let the block go: each of out
// and left cut so no end meets them (each its share of it).
[[nodiscard]] glm::vec3 blockSway(const glm::vec3& s, int n)
{
    const Rest& r = restCache;
    glm::vec3 out = s;
    for(int axis = 0; axis < 2; axis++)
    {
        float share = 1.f;
        for(int i = 0; i < n; i++)
        {
            if(!r.items[i].lying)
            {
                continue;
            }
            const float move = s[axis] * r.items[i].gain;
            for(int k = 0; k < r.wallCount[i]; k++)
            {
                const WallPoint& w = r.walls[i][k];
                for(int q = 0; q < ring; q++)
                {
                    if(!(w.reachable & (1u << q)))
                    {
                        continue;
                    }
                    const glm::vec3& l = r.lines[q];
                    const float still = l.x * w.p.x + l.y * w.p.y - l.z - w.r;
                    const float m = l[axis] * move;
                    if(m < 0.f && still + m < za::min(still, r.margin) - wobbleSoft)
                    {
                        share = za::min(share, za::max(0.f, (za::min(still, r.margin) - wobbleSoft - still) / m));
                    }
                }
            }
        }
        out[axis] = s[axis] * share;
    }
    return out;
}

// Each round's own sway kept clear of the others and the walls (restCache's rules): a few passes, each rule broken
// (its gap's straight-line guess) mended by the smallest change to the two rounds' Ctl.
void solve(Ctl (&c)[most], int n)
{
    const Rest& r = restCache;
    bool mended = true;
    for(int pass = 0; pass < wobbleIters && mended; pass++)
    {
        mended = false;
        // (A lying one's ends never drawn nearer each other along it: it is drawn rigid, so its ends would go past
        // where they were kept clear; apart, they are drawn inside them. The walls next, each other last: through each
        // other shows more than into the soft leather.)
        for(int i = 0; i < n; i++)
        {
            if(r.items[i].lying)
            {
                const glm::vec3 axis = glm::normalize(r.ends[i][1] - r.ends[i][0]);
                const float in = glm::dot(c[i].e1 - c[i].e0, axis);
                if(in < -1e-4f)
                {
                    c[i].e0 += axis * (in * 0.5f);
                    c[i].e1 -= axis * (in * 0.5f);
                    mended = true;
                }
            }
        }
        for(int i = 0; i < n; i++)
        {
            for(int k = 0; k < r.wallCount[i]; k++)
            {
                const WallPoint& w = r.walls[i][k];
                glm::vec3& e = w.f == -1.f ? c[i].e1 : c[i].e0;
                const float f = w.f > 0.f ? w.f : 1.f;
                if(w.f <= 0.f)
                {
                    // (A lying one's end: off the floor too.)
                    const float still = w.p.z - w.r - r.floor + 0.03f;
                    const float v = za::min(still, r.margin) - (still + e.z);
                    if(v > 1e-4f)
                    {
                        e.z += v;
                        mended = true;
                    }
                }
                for(int q = 0; q < ring; q++)
                {
                    if(!(w.reachable & (1u << q)))
                    {
                        continue;
                    }
                    const glm::vec3& l = r.lines[q];
                    const float still = l.x * w.p.x + l.y * w.p.y - l.z - w.r;
                    const float v = za::min(still, r.margin) - wobbleSoft - (still + (l.x * e.x + l.y * e.y) * f);
                    if(v > 1e-4f)
                    {
                        e.x += l.x * v / f;
                        e.y += l.y * v / f;
                        mended = true;
                    }
                }
            }
        }
        for(int q = 0; q < r.pairCount; q++)
        {
            const Pair& p = r.pairs[q];
            if(p.lying)
            {
                // Two lying ones: their axes where their ends are now, pushed apart at their closest points (each end
                // its share of it).
                Ctl &a = c[p.i], &b = c[p.j];
                const glm::vec3 a0 = r.ends[p.i][0] + a.e0, a1 = r.ends[p.i][1] + a.e1;
                const glm::vec3 b0 = r.ends[p.j][0] + b.e0, b1 = r.ends[p.j][1] + b.e1;
                float s, t;
                const float d = segClosest(a0, a1, b0, b1, s, t);
                const float v = za::min(p.g, r.margin) - (d - r.items[p.i].radius - r.items[p.j].radius);
                if(v > 1e-4f && d > 1e-6f)
                {
                    mended = true;
                    const glm::vec3 nrm = ((b0 + (b1 - b0) * t) - (a0 + (a1 - a0) * s)) / d;
                    const float k = v / ((1.f - s) * (1.f - s) + s * s + (1.f - t) * (1.f - t) + t * t);
                    a.e0 -= nrm * (k * (1.f - s));
                    a.e1 -= nrm * (k * s);
                    b.e0 += nrm * (k * (1.f - t));
                    b.e1 += nrm * (k * t);
                }
                continue;
            }
            if(p.standing)
            {
                // Two standing capsules (grenades): their axes leant (each point by its height's share of the top's),
                // pushed apart at their closest points.
                Ctl &a = c[p.i], &b = c[p.j];
                const glm::vec3 da{a.e0.x, a.e0.y, 0.f}, db{b.e0.x, b.e0.y, 0.f};
                const glm::vec3 a0 = r.ends[p.i][0] + da * r.lean[p.i][0], a1 = r.ends[p.i][1] + da * r.lean[p.i][1];
                const glm::vec3 b0 = r.ends[p.j][0] + db * r.lean[p.j][0], b1 = r.ends[p.j][1] + db * r.lean[p.j][1];
                float s, t;
                const float d = segClosest(a0, a1, b0, b1, s, t);
                const float v = za::min(p.g, r.margin) - (d - r.items[p.i].radius - r.items[p.j].radius);
                if(v > 1e-4f && d > 1e-6f)
                {
                    const glm::vec3 nrm = ((b0 + (b1 - b0) * t) - (a0 + (a1 - a0) * s)) / d;
                    const glm::vec2 flat{nrm.x, nrm.y};
                    const float ga = r.lean[p.i][0] * (1.f - s) + r.lean[p.i][1] * s;
                    const float gb = r.lean[p.j][0] * (1.f - t) + r.lean[p.j][1] * t;
                    const float den = (ga * ga + gb * gb) * glm::dot(flat, flat);
                    if(den > 1e-6f)
                    {
                        mended = true;
                        a.e0 -= glm::vec3{flat * (v / den * ga), 0.f};
                        b.e0 += glm::vec3{flat * (v / den * gb), 0.f};
                    }
                }
                continue;
            }
            const float v = za::min(p.g, r.margin) - (p.g + dot(p.gi, c[p.i]) + dot(p.gj, c[p.j]));
            if(v > 1e-4f)
            {
                addScaled(c[p.i], p.gi, v * p.inv);
                addScaled(c[p.j], p.gj, v * p.inv);
                mended = true;
            }
        }
    }
}

// The springs stepped by `dt` (s) under the pouch's acceleration `acc` (world units/s^2 along its axes).
void stepSprings(Wobble& w, float dt, const glm::vec3& acc)
{
    const float omega = 2.f * 3.14159265f * za::clamp(vr_ammo_pouch_wobble_freq.value, 0.2f, 8.f);
    const float damping = za::clamp(3.f / (omega * za::clamp(vr_ammo_pouch_wobble_settle.value, 0.1f, 10.f)), 0.02f, 2.f);
    const glm::vec3 drive = acc * (wobbleDrive * za::clamp(vr_ammo_pouch_wobble_intensity.value, 0.f, 5.f));
    const bool each = vr_ammo_pouch_wobble_each.value != 0.f;
    const int steps = za::clamp(static_cast<int>(dt * 240.f) + 1, 1, 60);
    const float h = dt / static_cast<float>(steps);
    // Each round's this frame: its stiffness and damping (its sway's, its tilt's), its drive turned.
    float k[most], c[most], kt[most], ct[most];
    glm::vec3 d[most], dt2[most];
    for(int i = 0; each && i < most; i++)
    {
        const Own& o = own[i];
        const float om = omega * o.omega, ze = damping * o.damping, omt = om * 1.35f;
        k[i] = om * om;
        c[i] = 2.f * ze * om;
        kt[i] = omt * omt;
        ct[i] = 2.f * ze * omt;
        d[i] = glm::vec3{o.c * drive.x - o.s * drive.y, o.s * drive.x + o.c * drive.y, drive.z} * o.drive;
        dt2[i] = glm::vec3{-d[i].y, d[i].x, d[i].z * 0.5f} * 0.6f;
    }
    const float k0 = omega * omega, c0 = 2.f * damping * omega;
    for(int step = 0; step < steps; step++)
    {
        w.shared.v += (-k0 * w.shared.s - c0 * w.shared.v - drive) * h;
        w.shared.s += w.shared.v * h;
        for(int i = 0; each && i < most; i++)
        {
            Spring& s = w.each[i];
            s.v += (-k[i] * s.s - c[i] * s.v - d[i]) * h;
            s.s += s.v * h;
            Spring& t = w.tilt[i];
            t.v += (-kt[i] * t.s - ct[i] * t.v - dt2[i]) * h;
            t.s += t.v * h;
        }
    }
}

void layoutCheck_f()
{
    struct Case
    {
        int kind;
        bool variant;
        const char* name;
        int each, most;
    };
    constexpr Case cases[] = {{1, false, "shells", 1, most}, {2, false, "nail mags", 24, magsMost},
        {3, false, "super nail mags", 36, magsMost}, {4, false, "cells", 36, magsMost}, {5, false, "rockets", 1, 5},
        {6, false, "grenades", 1, 5}, {6, true, "multi-grenades", 1, 5}, {7, false, "prox grenades", 1, 5}};
    const bool verbose = Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) > 0;
    const float strength = za::clamp(vr_ammo_pouch_wobble.value, 0.f, 3.f);
    const float amp = wobbleAmp * strength, tilt = wobbleTilt * strength;
    int bad = 0;
    Item items[most];
    for(const Case& c : cases)
    {
        float worstGap = 1e9f, worstWall = 1e9f, worstWobble = 1e9f, worstCentre = 0.f, worstEach = 1e9f,
              worstEachWall = 1e9f, worstWobbleWall = 1e9f;
        for(int n = 1; n <= c.most; n++)
        {
            const int left = n * c.each;
            const int got = layout(c.kind, c.variant, left, items);
            const int frame = frameFor(c.kind, left);
            if(got != n)
            {
                Con_Printf("pouchlayout: %s: %d drawn for %d (want %d)\n", c.name, got, left, n);
                bad++;
                continue;
            }
            const Check rest = check(items, n, frame);
            // How much nearer the walls (up to the rim) the nearest one comes than still (or than inside, if out still).
            glm::vec2 outline[ring];
            innerOutline(frame, outline);
            const auto wallDrop = [&](const Item* moved) {
                float worst = 1e9f;
                for(int i = 0; i < n; i++)
                {
                    worst = za::min(worst, rimWallGap(moved[i], shapes[frame], outline) -
                                               za::min(rimWallGap(items[i], shapes[frame], outline), 0.f));
                }
                return worst;
            };
            // The wobble's extremes: every corner of its reach (one spring for all, each its own share of it; kept clear
            // as drawn).
            float wob = 1e9f, wobWall = 1e9f;
            if(amp > 0.f)
            {
                buildRest(items, n, frame, reachAt(strength));
            }
            for(int s = 0; s < 8 && amp > 0.f; s++)
            {
                Item moved[most];
                Ctl ctl[most];
                const glm::vec3 lag{(s & 1) ? amp : -amp, (s & 2) ? amp : -amp, (s & 4) ? wobbleAmpUp * strength : 0.f};
                const glm::vec3 block = blockSway(lag, n);
                for(int i = 0; i < n; i++)
                {
                    ctl[i] = ctlFor(items[i], items[i].lying ? block : lag, glm::vec3{0.f});
                }
                solve(ctl, n);
                for(int i = 0; i < n; i++)
                {
                    moved[i] = items[i];
                    applyCtl(moved[i], ctl[i]);
                }
                wob = za::min(wob, check(moved, n, frame).gap);
                wobWall = za::min(wobWall, wallDrop(moved));
            }
            worstWobbleWall = za::min(worstWobbleWall, wobWall);
            // Each its own (vr_ammo_pouch_wobble_each): every round at a corner of its reach and tilt, all the same way
            // (the first 8) or each its own way, then kept clear as drawn (solve); how close two come, how near the walls
            // against at rest.
            float each = 1e9f, eachWall = 1e9f;
            if(amp > 0.f)
            {
                buildRest(items, n, frame, reachAt(strength));
                for(int p = 0; p < 40; p++)
                {
                    Ctl ctl[most];
                    Item moved[most];
                    for(int i = 0; i < n; i++)
                    {
                        const unsigned b = p < 8 ? static_cast<unsigned>(p)
                                                 : (static_cast<unsigned>(i) * 2654435761u ^ static_cast<unsigned>(p) * 2246822519u) >> 9;
                        const glm::vec3 a{(b & 1) ? amp : -amp, (b & 2) ? amp : -amp, (b & 4) ? wobbleAmpUp * strength : 0.f};
                        const glm::vec3 t{(b & 8) ? tilt : -tilt, (b & 16) ? tilt : -tilt, (b & 32) ? tilt : -tilt};
                        // (Lying: the rest's corner, p's, and each its own slide off it at a corner of its own.)
                        const glm::vec3 shared = blockSway(
                            glm::vec3{(p & 1) ? amp : -amp, (p & 2) ? amp : -amp, (p & 4) ? wobbleAmpUp * strength : 0.f}, n);
                        ctl[i] = ctlFor(items[i], items[i].lying ? shared + slideOff(a - shared, strength) : a, t);
                    }
                    solve(ctl, n);
                    for(int i = 0; i < n; i++)
                    {
                        moved[i] = items[i];
                        applyCtl(moved[i], ctl[i]);
                    }
                    const Check m = check(moved, n, frame);
                    each = za::min(each, m.gap);
                    eachWall = za::min(eachWall, wallDrop(moved));
                }
                restCache.n = -1; // (drawn: built again)
            }
            worstEach = za::min(worstEach, each);
            worstEachWall = za::min(worstEachWall, eachWall);
            bad += (n > 1 && each < 0.f) || eachWall < -wobbleSoft - 0.05f ? 1 : 0;
            worstGap = za::min(worstGap, rest.gap);
            worstWall = za::min(worstWall, rest.wall);
            worstWobble = za::min(worstWobble, wob);
            const bool centred = c.kind == 5 ? true : za::fabs(rest.cy) < 0.6f && za::fabs(rest.cx) < 0.6f;
            worstCentre = za::max(worstCentre, za::max(za::fabs(rest.cx), za::fabs(rest.cy)));
            if(verbose)
            {
                Con_Printf("pouchlayout: %s %d (frame %d): gap %.2f, walls %.2f, wobbling %.2f (each %.2f, walls %+.2f), "
                           "middle off %+.2f %+.2f\n",
                    c.name, n, frame, n > 1 ? rest.gap : 0.f, rest.wall, n > 1 ? wob : 0.f, n > 1 ? each : 0.f, eachWall,
                    rest.cx, rest.cy);
                for(int i = 0; verbose && Q_atoi(Cmd_Argv(1)) > 1 && i < n; i++)
                {
                    const Item& it = items[i];
                    Con_Printf("  %d: at %.2f %.2f %.2f x %.2f %.2f %.2f z %.2f %.2f %.2f size %.3f\n", i, it.at.x, it.at.y,
                        it.at.z, it.axes[0].x, it.axes[0].y, it.axes[0].z, it.axes[2].x, it.axes[2].y, it.axes[2].z,
                        it.size);
                }
            }
            bad += (n > 1 && (rest.gap < 0.f || wob < 0.f)) || rest.wall < -0.05f || wobWall < -wobbleSoft - 0.05f || !centred ? 1 : 0;
        }
        Con_Printf("pouchlayout: %-16s 1..%2d: closest %.2f apart (wobbling %.2f, each its own %.2f), %.2f inside the "
                   "walls (to the rim wobbling %+.2f, each its own %+.2f), middle off %.2f at most\n",
            c.name, c.most, worstGap >= 1e8f ? 0.f : worstGap, worstWobble >= 1e8f ? 0.f : worstWobble,
            worstEach >= 1e8f ? 0.f : worstEach, worstWall, worstWobbleWall >= 1e8f ? 0.f : worstWobbleWall,
            worstEachWall >= 1e8f ? 0.f : worstEachWall, worstCentre);
    }
    // The sizes: each round in the pouch against the same model held (props' Size; drawnSize), in cm.
    constexpr float cm = 3.81f; // (a Quake unit)
    for(const char* name : {"progs/vr_shell_live.mdl", "progs/vr_mag_nail.mdl", "progs/vr_mag_snail.mdl",
            "progs/vr_mag_light.mdl", "progs/vr_round_rocket.mdl"})
    {
        const qmodel_t* m = view::viewModel(name);
        if(m)
        {
            const float ds = heldSize(m);
            Con_Printf("pouchsize: %-26s held and in the pouch x%.3f: %.1f x %.1f x %.1f cm\n", name, ds,
                (m->maxs[0] - m->mins[0]) * ds * cm, (m->maxs[1] - m->mins[1]) * ds * cm, (m->maxs[2] - m->mins[2]) * ds * cm);
        }
    }
    for(const GrenadeFoot& g : grenadeFeet)
    {
        const qmodel_t* m = view::viewModel(g.model);
        if(m)
        {
            const float ds = heldSize(m);
            const float len = (g.alongZ ? m->maxs[2] - m->mins[2] : m->maxs[0] - m->mins[0]) * cm;
            Con_Printf("pouchsize: %-26s held x%.3f %.1f cm long, in the pouch x%.3f (%.2f of it) %.1f cm\n", g.model, ds,
                len * ds, ds * g.share, g.share, len * ds * g.share);
        }
    }
    // The cost a frame of 20 shells each on its own spring: the springs stepped (90 fps), the layout, wobbled and kept
    // clear (its rules built again only when the layout changes: that once).
    {
        const Wobble saved = wobbleState;
        wobbleState.valid = true;
        wobbleState.k = 1.f;
        constexpr int reps = 2000;
        const double t0 = Sys_DoubleTime();
        for(int r = 0; r < reps; r++)
        {
            const float t = static_cast<float>(r) * 0.07f;
            stepSprings(wobbleState, 1.f / 90.f, glm::vec3{za::sin(t * 1.8f), za::sin(t * 1.1f), za::sin(t * 3.6f)} * 900.f);
        }
        const double t1 = Sys_DoubleTime();
        for(int r = 0; r < reps; r++)
        {
            (void)layout(1, false, most, items);
        }
        const double t2 = Sys_DoubleTime();
        restCache.n = -1;
        (void)posed(1, false, most, items);
        const double t3 = Sys_DoubleTime();
        for(int r = 0; r < reps; r++)
        {
            (void)posed(1, false, most, items);
        }
        const double t4 = Sys_DoubleTime();
        Con_Printf("pouchwobble: %d shells a frame: springs %.2f us, layout %.2f us, layout + wobble %.2f us (%d pairs kept "
                   "clear; their rules built once %.0f us), each %d\n",
            layout(1, false, most, items), (t1 - t0) * 1e6 / reps, (t2 - t1) * 1e6 / reps, (t4 - t3) * 1e6 / reps,
            restCache.pairCount, (t3 - t2) * 1e6, vr_ammo_pouch_wobble_each.value != 0.f ? 1 : 0);
        wobbleState = saved;
        restCache.n = -1;
    }
    Con_Printf("pouchlayout: %s\n", bad ? "FAIL" : "OK");
}

} // namespace

int frameFor(int kind, int left)
{
    constexpr int frames[8] = {0, 1, 2, 2, 3, 6, 4, 5};
    kind &= 7; // (16: the nail pistol's magazines, the nailgun's frame)
    return left > 0 && kind >= 1 && kind <= 7 ? frames[kind] : 0;
}

const Shape& shapeOf(int frame)
{
    return shapes[za::clamp(frame, 0, 6)];
}

float innerCentreX(float depth)
{
    return 0.5f * (innerBack(depth) + innerFront(depth));
}

int magsShown(int kind, int left)
{
    if(left <= 0 || (kind & 7) < 2 || (kind & 7) > 4)
    {
        return 0;
    }
    const int each = magRounds(kind);
    return za::min((left + each - 1) / each, za::clamp(static_cast<int>(vr_ammo_pouch_mags_max.value), 1, magsMost));
}

glm::vec2 magSlot(int kind, int j, int n)
{
    const MagFoot& f = footFor(kind);
    const float cx = innerCentreX(shapes[(kind & 7) == 4 ? 3 : 2].depth);
    const bool two = n > 3;
    const int row = two && j >= 3 ? 1 : 0;
    const int inRow = two ? (row == 0 ? 3 : n - 3) : n;
    const int col = row == 0 ? j : j - 3;
    const float x = two ? cx + (row == 0 ? -0.5f : 0.5f) * (f.thick + magGapX) : cx;
    return glm::vec2{x, (static_cast<float>(col) - static_cast<float>(inRow - 1) * 0.5f) * (f.width + magGapY)};
}

int layout(int kind, bool variant, int left, Item (&out)[most])
{
    if(left <= 0)
    {
        return 0;
    }
    switch(kind & 7)
    {
        case 1: return layoutShells(left, out);
        case 2:
        case 3:
        case 4: return layoutMags(kind, variant, left, out);
        case 5: return layoutRockets(variant, left, out);
        case 6:
        case 7: return layoutGrenades(kind, variant, left, out);
        default: return 0;
    }
}

void wobbleStep(const glm::vec3& origin, const glm::mat3& axes, float k)
{
    Wobble& w = wobbleState;
    const float dt = w.valid ? static_cast<float>(cl.time - w.time) : 0.f;
    w.k = za::max(k, 1e-3f);
    if(!w.valid || dt < 0.f || dt > 0.25f || glm::distance(origin, w.last) > 40.f)
    {
        const float kept = w.k;
        w = Wobble{};
        w.valid = true;
        w.time = cl.time;
        w.last = origin;
        w.k = kept;
        return;
    }
    if(dt <= 0.f)
    {
        return;
    }
    const glm::vec3 v = glm::transpose(axes) * ((origin - w.last) / dt); // (along the pouch's axes)
    const glm::vec3 vs = w.vel + (v - w.vel) * (1.f - za::exp(-dt * wobbleSmooth));
    glm::vec3 acc = (vs - w.vel) / dt;
    w.vel = vs;
    w.last = origin;
    w.time = cl.time;
    if(vr_ammo_pouch_wobble_test.value != 0.f)
    {
        // (Debug: shaken as if walking briskly, turning and stopping.)
        const float t = static_cast<float>(cl.time) * 2.f * 3.14159265f;
        acc += glm::vec3{za::sin(t * 1.8f), 0.7f * za::sin(t * 1.1f + 1.f), 0.5f * za::sin(t * 3.6f)} * 900.f;
    }
    stepSprings(w, dt, acc);
}

glm::vec3 wobble()
{
    const Wobble& w = wobbleState;
    const float strength = za::clamp(vr_ammo_pouch_wobble.value, 0.f, 3.f);
    if(!w.valid || strength <= 0.f)
    {
        return glm::vec3{0.f};
    }
    const glm::vec3 s = w.shared.s / w.k;
    return glm::vec3{softClamp(s.x, wobbleAmp * strength), softClamp(s.y, wobbleAmp * strength),
        softClamp(s.z, wobbleAmpUp * strength)};
}

int posed(int kind, bool variant, int left, Item (&out)[most])
{
    const int n = layout(kind, variant, left, out);
    const Wobble& w = wobbleState;
    const float strength = za::clamp(vr_ammo_pouch_wobble.value, 0.f, 3.f);
    if(n <= 0 || !w.valid || strength <= 0.f)
    {
        return n;
    }
    const float amp = wobbleAmp * strength, up = wobbleAmpUp * strength, tilt = wobbleTilt * strength;
    const auto lag = [&](const glm::vec3& s, float a, float u) {
        const glm::vec3 x = s / w.k;
        return glm::vec3{softClamp(x.x, a), softClamp(x.y, a), softClamp(x.z, u)};
    };
    const int frame = frameFor(kind, left);
    const float reach = reachAt(strength);
    if(!sameRest(out, n, frame, reach))
    {
        buildRest(out, n, frame, reach);
    }
    // (Standing ones lean each its own way; lying ones, a pile or a stack, sway with the rest and slide a little. One
    // spring for all: all the rest's, each its own share of it. Either way kept clear.)
    const bool each = vr_ammo_pouch_wobble_each.value != 0.f;
    const glm::vec3 shared = blockSway(lag(w.shared.s, amp, up), n);
    Ctl c[most];
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 a = each ? lag(w.each[i].s, amp, up) : out[i].lying ? shared : lag(w.shared.s, amp, up);
        c[i] = ctlFor(out[i], out[i].lying ? shared + slideOff(a - shared, strength) : a,
            each ? lag(w.tilt[i].s, tilt, tilt) : glm::vec3{0.f});
    }
    solve(c, n);
    for(int i = 0; i < n; i++)
    {
        applyCtl(out[i], c[i]);
    }
    return n;
}

void init()
{
    Cmd_AddCommand("vr_ammo_pouch_layout", layoutCheck_f);
    // Each round's spring a little off the others' (golden-ratio steps: no two alike, the first few spread wide).
    for(int i = 0; i < most; i++)
    {
        const float f = static_cast<float>(i);
        const float skew = rad((frac(f * 0.236068f + 0.29f) - 0.5f) * 60.f);
        own[i] = Own{0.8f + 0.45f * frac(f * 0.618034f + 0.11f), 0.75f + 0.6f * frac(f * 0.414214f + 0.47f),
            0.8f + 0.4f * frac(f * 0.732051f + 0.83f), za::cos(skew), za::sin(skew)};
    }
}

} // namespace qvr::pouchitems
