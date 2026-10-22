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
    {4.8f, 4.4f, 0.6f}, {4.6f, 3.6f, 0.6f}, {5.2f, 3.5f, -1.2f}};
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
constexpr MagFoot magFeet[3] = {{"progs/vr_mag_nail.mdl", 2.068f, 1.111f}, {"progs/vr_mag_snail.mdl", 2.232f, 1.089f},
    {"progs/vr_mag_light.mdl", 1.665f, 1.44f}};
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
// a share to stay inside the leather (the grenade 0.9; the multi-grenade 0.7, was 0.8 when three stood in a row: five
// fit; the proximity grenade a third, Quake's is near the pouch's width): their tops' height, their radius at the
// shipped scales (0.78, 0.47) as drawn here, the scale it was measured at.
struct GrenadeFoot
{
    const char* model;
    float share, top, radius, at;
    bool alongZ;
};
constexpr GrenadeFoot grenadeFeet[3] = {{"progs/grenade.mdl", 0.9f, 3.4f, 1.08f, 0.78f, false},
    {"progs/mervup.mdl", 0.7f, 3.3f, 1.08f, 0.78f, true}, {"progs/proxbomb.mdl", 0.36f, 3.1f, 0.92f, 0.47f, true}};
constexpr float grenadeGap = 0.1f;
constexpr float grenadeSpin[5] = {20.f, 140.f, 75.f, 250.f, 310.f};
constexpr float grenadeLift[5] = {0.f, -0.06f, 0.04f, -0.03f, 0.05f};

// The rockets lying (make_rounds.py's, nose along its +x): which way each points along the pouch, how far along it each
// lies off the middle (towards its nose: the butt's fins kept off the walls), a little turned.
constexpr float rocketDir[5] = {1.f, -1.f, -1.f, 1.f, -1.f};
constexpr float rocketShift[5] = {0.15f, 0.23f, 0.1f, 0.25f, 0.17f};
constexpr float rocketYaw[5] = {1.2f, 0.8f, 0.5f, 1.4f, 0.9f}; // (degrees: each butt turned off its wall)
constexpr float rocketGap = 0.06f;

// The wobble: a damped spring (the rounds' lag behind the pouch, world units along its axes) driven by the pouch's
// acceleration (its velocity smoothed first: tracking's jitter is no shake).
constexpr float wobbleOmega = 2.f * 3.14159265f * 2.f;
constexpr float wobbleDamping = 0.18f;
constexpr float wobbleDrive = 0.25f;  // the share of the acceleration that moves them
constexpr float wobbleAmp = 0.3f;     // how far they sway at most (pouch units: their tops, a lying one's middle)
constexpr float wobbleAmpUp = 0.12f;  // and lift
constexpr float wobbleSmooth = 15.f;  // the velocity's smoothing rate (1/s)

struct Wobble
{
    bool valid{false};
    double time{0.0};
    glm::vec3 last{0.f}, vel{0.f}, s{0.f}, sv{0.f}; // world units, along the pouch's axes
    float k{1.f};
};
Wobble wobbleState;

[[nodiscard]] float rad(float deg)
{
    return deg * 3.14159265f / 180.f;
}

[[nodiscard]] glm::mat3 aboutZ(float deg)
{
    const float c = za::cos(rad(deg)), s = za::sin(rad(deg));
    return glm::mat3{glm::vec3{c, s, 0.f}, glm::vec3{-s, c, 0.f}, glm::vec3{0.f, 0.f, 1.f}};
}

[[nodiscard]] float frac(float x)
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
    const MagFoot& f = magFeet[kind - 2];
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
    const float r = g.radius * ds / g.at;
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

[[nodiscard]] float segDist(glm::vec3 p1, glm::vec3 q1, glm::vec3 p2, glm::vec3 q2)
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
        // Walls: a capsule's ends and its middle, a radius round; a magazine's corners at the floor and its top.
        if(a.box)
        {
            for(const float z : {sh.floor, magTop})
            {
                glm::vec2 c[4];
                magSection(a, z, c);
                for(const glm::vec2& p : c)
                {
                    out.wall = za::min(out.wall, insideBy(outline, p));
                }
            }
        }
        else
        {
            for(int s = 0; s <= 4; s++)
            {
                const glm::vec3 p = itemPoint(a, a.p0 + (a.p1 - a.p0) * (static_cast<float>(s) * 0.25f));
                out.wall = za::min(out.wall, insideBy(outline, glm::vec2{p.x, p.y}) - a.radius);
                if(a.lying)
                {
                    out.wall = za::min(out.wall, p.z - a.radius - sh.floor + 0.03f);
                }
            }
        }
        for(int j = i + 1; j < n; j++)
        {
            const Item& b = items[j];
            float gap;
            if(a.box && b.box)
            {
                gap = 1e9f;
                for(const float z : {sh.floor, 0.5f * (sh.floor + magTop), magTop})
                {
                    glm::vec2 ca[4], cb[4];
                    magSection(a, z, ca);
                    magSection(b, z, cb);
                    gap = za::min(gap, quadGap(ca, cb));
                }
            }
            else
            {
                gap = segDist(itemPoint(a, a.p0), itemPoint(a, a.p1), itemPoint(b, b.p0), itemPoint(b, b.p1)) - a.radius -
                      b.radius;
            }
            out.gap = za::min(out.gap, gap);
        }
    }
    out.cx -= innerCentreX(sh.depth);
    return out;
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
    const float amp = wobbleAmp * za::clamp(vr_ammo_pouch_wobble.value, 0.f, 3.f);
    int bad = 0;
    Item items[most];
    for(const Case& c : cases)
    {
        float worstGap = 1e9f, worstWall = 1e9f, worstWobble = 1e9f, worstCentre = 0.f;
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
            // The wobble's extremes: every corner of its reach (the same lag for all, each its own share of it).
            float wob = 1e9f;
            for(int s = 0; s < 8 && amp > 0.f; s++)
            {
                Item moved[most];
                const glm::vec3 lag{(s & 1) ? amp : -amp, (s & 2) ? amp : -amp, (s & 4) ? wobbleAmpUp : 0.f};
                for(int i = 0; i < n; i++)
                {
                    moved[i] = items[i];
                    applyWobble(moved[i], lag);
                }
                wob = za::min(wob, check(moved, n, frame).gap);
            }
            worstGap = za::min(worstGap, rest.gap);
            worstWall = za::min(worstWall, rest.wall);
            worstWobble = za::min(worstWobble, wob);
            const bool centred = c.kind == 5 ? true : za::fabs(rest.cy) < 0.6f && za::fabs(rest.cx) < 0.6f;
            worstCentre = za::max(worstCentre, za::max(za::fabs(rest.cx), za::fabs(rest.cy)));
            if(verbose)
            {
                Con_Printf("pouchlayout: %s %d (frame %d): gap %.2f, walls %.2f, wobbling %.2f, middle off %+.2f %+.2f\n",
                    c.name, n, frame, n > 1 ? rest.gap : 0.f, rest.wall, n > 1 ? wob : 0.f, rest.cx, rest.cy);
                for(int i = 0; verbose && Q_atoi(Cmd_Argv(1)) > 1 && i < n; i++)
                {
                    const Item& it = items[i];
                    Con_Printf("  %d: at %.2f %.2f %.2f x %.2f %.2f %.2f z %.2f %.2f %.2f size %.3f\n", i, it.at.x, it.at.y,
                        it.at.z, it.axes[0].x, it.axes[0].y, it.axes[0].z, it.axes[2].x, it.axes[2].y, it.axes[2].z,
                        it.size);
                }
            }
            bad += (n > 1 && (rest.gap < 0.f || wob < 0.f)) || rest.wall < -0.05f || !centred ? 1 : 0;
        }
        Con_Printf("pouchlayout: %-16s 1..%2d: closest %.2f apart (wobbling %.2f), %.2f inside the walls, middle off "
                   "%.2f at most\n",
            c.name, c.most, worstGap >= 1e8f ? 0.f : worstGap, worstWobble >= 1e8f ? 0.f : worstWobble, worstWall,
            worstCentre);
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
    Con_Printf("pouchlayout: %s\n", bad ? "FAIL" : "OK");
}

} // namespace

int frameFor(int kind, int left)
{
    constexpr int frames[8] = {0, 1, 2, 2, 3, 6, 4, 5};
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
    if(left <= 0 || kind < 2 || kind > 4)
    {
        return 0;
    }
    const int each = kind == 2 ? 24 : 36;
    return za::min((left + each - 1) / each, za::clamp(static_cast<int>(vr_ammo_pouch_mags_max.value), 1, magsMost));
}

glm::vec2 magSlot(int kind, int j, int n)
{
    const MagFoot& f = magFeet[za::clamp(kind, 2, 4) - 2];
    const float cx = innerCentreX(shapes[kind == 4 ? 3 : 2].depth);
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
    switch(kind)
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
        w = Wobble{true, cl.time, origin, glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{0.f}, w.k};
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
    const int steps = za::clamp(static_cast<int>(dt * 240.f) + 1, 1, 60);
    const float h = dt / static_cast<float>(steps);
    for(int i = 0; i < steps; i++)
    {
        w.sv += (-wobbleOmega * wobbleOmega * w.s - 2.f * wobbleDamping * wobbleOmega * w.sv - acc * wobbleDrive) * h;
        w.s += w.sv * h;
    }
}

glm::vec3 wobble()
{
    const Wobble& w = wobbleState;
    const float strength = za::clamp(vr_ammo_pouch_wobble.value, 0.f, 3.f);
    if(!w.valid || strength <= 0.f)
    {
        return glm::vec3{0.f};
    }
    const glm::vec3 s = w.s / w.k;
    return glm::vec3{softClamp(s.x, wobbleAmp * strength), softClamp(s.y, wobbleAmp * strength),
        softClamp(s.z, wobbleAmpUp * strength)};
}

void applyWobble(Item& it, const glm::vec3& s)
{
    if(it.lying)
    {
        const glm::vec3 d = glm::vec3{s.x, s.y, za::max(0.f, s.z)} * it.gain;
        it.at += d;
        it.pivot += d;
        return;
    }
    const glm::vec2 d = glm::vec2{s.x, s.y} * it.gain;
    const float len = glm::length(d);
    if(len < 1e-5f)
    {
        return;
    }
    const float angle = za::atan2(len, za::max(it.height, 0.5f));
    const glm::vec3 axis = glm::normalize(glm::vec3{-d.y, d.x, 0.f}); // (up crossed with the lean: its top goes along d)
    const glm::mat3 turn = glm::mat3_cast(glm::angleAxis(angle, axis));
    it.at = it.pivot + turn * (it.at - it.pivot);
    it.axes = turn * it.axes;
}

void init()
{
    Cmd_AddCommand("vr_ammo_pouch_layout", layoutCheck_f);
}

} // namespace qvr::pouchitems
