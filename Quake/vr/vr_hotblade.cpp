// vr_hotblade.cpp -- see vr_hotblade.hpp.

#include "vr_random.hpp"
#include "vr_geom.hpp"
#include "vr_hotblade.hpp"
#include "vr_anchor.hpp"
#include "vr_client.hpp"
#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_emissive.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_particles.hpp"
#include "vr_protocol.hpp"
#include "vr_render.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"

#include <glm/gtc/type_ptr.hpp>


namespace qvr::hotblade
{
namespace
{

constexpr int maxLying = 8; // blades lying about or flying drawn glowing, lit and shedding embers (the nearest)
constexpr int maxSources = 2 + protocol::numHolsters + maxLying;
static_assert(maxSources <= emissive::hotBladeLights);
constexpr int maxTracks = 2 + maxLying;  // the trails' blades: the hands', then the lying (flying) ones'
constexpr float trailMaxStep = 64.f;     // units the tip may move in a frame and leave a trail (more: a snap, not a swing)
constexpr float trailAreaEach = 0.8f;    // square units of the swept blade a trail particle at vr_hotblade_trail 1
constexpr int trailMaxPerFrame = 48;     // trail particles a blade a frame at most
constexpr float trailSparkShare = 0.35f; // of them sparks (the rest small tongues of flame)
constexpr int poseCacheSize = 6;

// A glowing weapon drawn this frame.
struct Source
{
    const entity_t* e{nullptr};
    float glow{0.f};
    int kind{0}; // 0, 1 the hands; 2 + i holster i; 2 + numHolsters lying about (or flying)
    int num{0};  // a lying one's entity number
};

// The blade of a weapon drawn this frame (world): its grip (the hand), its tip (the muzzle anchor), where its heated part
// starts, and that part's mask plane in the pose's space (xyz . pose + w: 0 at its root, 1 a little past it).
struct Blade
{
    glm::vec3 grip{0.f};
    glm::vec3 tip{0.f};
    glm::vec3 root{0.f};
    glm::vec4 plane{0.f};
    bool known{false}; // a weapon whose blade is known (else all of it: the root is the grip)
};

// A trail's blade as it was last frame.
struct Track
{
    int key{0}; // -1 - hand; a lying one's entity number
    double seen{-1.0};
    glm::vec3 root{0.f};
    glm::vec3 tip{0.f};
    float m[16]{};                 // the matrix it was drawn by (its pose's space to the world)
    const qmodel_t* model{nullptr}; // and its model
    float due{0.f};                // trail particles owed (a fraction carried over)
};

// What vr_hotblade_report prints: the last frame's blades.
struct Seen
{
    const qmodel_t* model{nullptr};
    int kind{0};
    float glow{0.f};
    Blade blade;
    float light{0.f};
    float radius{0.f};
    const entity_t* e{nullptr}; // (its entity: the report counts its glowing vertices then, not every frame)
};

// The model's grip and tip in its pose's space (a frame's: worked out again each frame, the weapon's settings may change).
struct PoseBlade
{
    const qmodel_t* model{nullptr};
    double at{-1.0};
    glm::vec3 grip{0.f};
    glm::vec3 tip{0.f};
    bool ok{false};
};

float hot[2] = {0.f, 0.f}; // each hand's weapon's glow, 0..1 (STAT_QVR_HOTBLADE, eased)
float emberDue[maxSources] = {};
bool lit[maxSources] = {};
Track tracks[maxTracks];
PoseBlade poseCache[poseCacheSize];
int poseCacheNext = 0;
double lastQueue = -1.0;
// The player at the last frame (carry): the pivot of the play space's turn (hands::State playerOrigin + lean), its turn.
glm::vec3 lastPlayer{0.f};
float lastTurnYaw = 0.f;
double lastPlayerAt = -1.0;
Seen seen[maxSources];
int seenCount = 0;
int embersMade = 0; // this second's (since statFrom)
int trailMade = 0;
int embersLast = 0; // the last full second's
int trailLast = 0;
double statFrom = -1.0;

za::FastNonCryptoRng rng{0x407B1ADEu}; // a fixed seed: the same embers every run

[[nodiscard]] bool connected()
{
    return (cl.protocolflags & PRFL_QUAKEVR) && cls.state == ca_connected;
}

[[nodiscard]] glm::vec3 bytePoint(const float m[16], const glm::vec3& v)
{
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
}

// Where on the line from the grip (0) to the tip (1) the heated part starts: the QC's striking points' (vr_melee.qc
// VR_Melee_Layout and VR_Melee_PartAt: the edge, not the handle, hilt or engine); -1 for a weapon not known.
[[nodiscard]] float bladeShare(const qmodel_t* model)
{
    using modelmeta::Id;
    if(modelmeta::is(model, Id::VAxe) || modelmeta::is(model, Id::Mg3SuperAxe) || modelmeta::is(model, Id::AdVrAxe)) // (AD support)
    {
        return 0.6f; // the head (the Shadow Axe is the axe's model)
    }
    if(modelmeta::is(model, Id::VKsword) || modelmeta::is(model, Id::VHksword))
    {
        return 0.2f; // the blade past the guard
    }
    if(modelmeta::is(model, Id::VCrowbar))
    {
        return 0.05f; // the bar past the hand (its tape is under the fist)
    }
    if(modelmeta::is(model, Id::VChainsaw))
    {
        return 0.45f; // the bar past the engine block
    }
    return -1.f;
}

// The grip and tip of `model` in its pose's space: drawn at the origin, unturned, as a hand holds it (the grip at the
// entity's origin), its muzzle anchor (the melee's tip) brought back into the pose's space.
[[nodiscard]] const PoseBlade* poseBlade(qmodel_t* model)
{
    for(const PoseBlade& p : poseCache)
    {
        if(p.model == model && p.at == realtime)
        {
            return p.ok ? &p : nullptr;
        }
    }
    PoseBlade& p = poseCache[poseCacheNext];
    poseCacheNext = (poseCacheNext + 1) % poseCacheSize;
    p = PoseBlade{};
    p.model = model;
    p.at = realtime;
    const int slot = weapons::slotForModel(model);
    if(!model || model->type != mod_alias || slot < 0)
    {
        return nullptr;
    }
    view::ViewEntity ve;
    ve.ent.model = model;
    ve.ent.scale = ENTSCALE_DEFAULT;
    float m[16];
    render::anchorMatrix(ve, glm::vec3{0.f}, m);
    const glm::mat4 toPose = glm::inverse(glm::make_mat4(m));
    const glm::vec3 muzzle = view::anchorPosition(ve, weapons::anchor(slot, weapons::Key::MuzzleAnchor),
        weapons::vec(slot, weapons::Key::MuzzleOffsetX, weapons::Key::MuzzleOffsetY, weapons::Key::MuzzleOffsetZ));
    p.grip = glm::vec3{toPose * glm::vec4{0.f, 0.f, 0.f, 1.f}};
    p.tip = glm::vec3{toPose * glm::vec4{muzzle, 1.f}};
    p.ok = glm::distance(p.grip, p.tip) > 1e-3f;
    return p.ok ? &p : nullptr;
}

// The blade of `e` drawn by `m` (its pose's space to the world). False: not a weapon whose line is known.
[[nodiscard]] bool bladeOf(const entity_t& e, const float m[16], Blade& out)
{
    const PoseBlade* p = poseBlade(e.model);
    if(!p)
    {
        return false;
    }
    out.grip = bytePoint(m, p->grip);
    out.tip = bytePoint(m, p->tip);
    const glm::vec3 d = out.tip - out.grip;
    const float len = glm::length(d);
    if(len < 1e-3f)
    {
        return false;
    }
    const float share = bladeShare(e.model);
    out.known = share >= 0.f;
    out.root = out.grip + d * za::max(0.f, share);
    if(!out.known)
    {
        out.plane = glm::vec4{0.f};
        return true;
    }
    // t(x) = (dot(x - root, n) + feather / 2) / feather, x = A p + T: in the pose's space, (A^T n / feather) . p + w.
    const glm::vec3 n = d / len;
    const float feather = za::max(0.5f, 0.08f * len);
    const glm::vec3 k = n / feather;
    const glm::vec3 t{m[12], m[13], m[14]};
    out.plane = glm::vec4{k.x * m[0] + k.y * m[1] + k.z * m[2], k.x * m[4] + k.y * m[5] + k.z * m[6],
        k.x * m[8] + k.y * m[9] + k.z * m[10], glm::dot(k, t - out.root) + 0.5f};
    return true;
}

// The matrix `e` is drawn by (its pose's space to the world), as the attachments find it.
void matrixOf(const entity_t& e, float m[16])
{
    if(const view::ViewEntity* ve = view::find(&e))
    {
        render::anchorMatrix(*ve, glm::vec3{0.f}, m);
        return;
    }
    render::entityMatrix(e, false, e.scale ? e.scale : ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
}

[[nodiscard]] float zeroBlendOf(const entity_t& e)
{
    const view::ViewEntity* ve = view::find(&e);
    return ve ? ve->zeroBlend : 0.f;
}

// How `e` glows, 0..1 (0: not a glowing blade).
[[nodiscard]] float glowOf(const entity_t* e)
{
    if(!e || vr_hotblade_glow.value <= 0.f || !connected())
    {
        return 0.f;
    }
    bool weapon = false;
    const int hand = view::handOf(e, weapon);
    if(hand >= 0)
    {
        return weapon ? hot[hand] : 0.f;
    }
    if(const int bits = cl.stats[protocol::STAT_QVR_HOTBLADEHOLSTER]; bits != 0)
    {
        for(int i = 0; i < protocol::numHolsters; i++)
        {
            if(((bits >> (4 * i)) & 15) != 0 && view::holsteredWeapon(i) == e)
            {
                return static_cast<float>((bits >> (4 * i)) & 15) / 15.f;
            }
        }
    }
    if(e >= cl_entities && e < cl_entities + cl.num_entities)
    {
        const client::EntityVr* net = client::entityVr(static_cast<int>(e - cl_entities));
        return net ? net->hotGlow : 0.f;
    }
    return 0.f;
}

// A random vertex of the blade's heated part as drawn (the pose's space: its surface, the shape of the head or the
// blade), tried a few times; false: none found (the caller takes a point on its line).
[[nodiscard]] bool bladeVertex(const entity_t& e, const Blade& b, glm::vec3& posed)
{
    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    if(!hdr || hdr->numverts <= 0)
    {
        return false;
    }
    const float zeroBlend = zeroBlendOf(e);
    for(int attempt = 0; attempt < 16; attempt++)
    {
        posed = anchor::posedVertex(e, rng.getI(0, hdr->numverts - 1), zeroBlend);
        if(!b.known || glm::dot(glm::vec3{b.plane}, posed) + b.plane.w >= 0.75f)
        {
            return true; // (not the handle, the hilt, the engine)
        }
    }
    return false;
}

// The point of a blade's heated part an ember leaves from (its surface), and the way out of the metal there.
void emberPoint(const entity_t& e, const float m[16], const Blade& b, glm::vec3& at, glm::vec3& out)
{
    glm::vec3 posed;
    if(bladeVertex(e, b, posed))
    {
        const glm::vec3 along = b.tip - b.grip;
        const float len2 = glm::dot(along, along);
        at = bytePoint(m, posed);
        const glm::vec3 onLine = len2 > 1e-4f ? b.grip + along * (glm::dot(at - b.grip, along) / len2) : b.grip;
        out = at - onLine;
        const float l = glm::length(out);
        out = l > 1e-3f ? out / l : glm::vec3{0.f, 0.f, 1.f};
        return;
    }
    at = glm::mix(b.root, b.tip, vrand::range(rng, 0.f, 1.f));
    out = glm::normalize(glm::vec3{vrand::range(rng, -1.f, 1.f), vrand::range(rng, -1.f, 1.f), vrand::range(rng, 0.2f, 1.f)});
}

// The trail a moving blade `e` (drawn by `m`) leaves this frame between where its heated part was (`t`, last frame) and
// is (`b`): each particle somewhere on the heated part's shape (a vertex of it) between where it was and is. Its speed
// (units/s): `ownSpeed` (a hand's tip relative to the player), else (< 0) its tip's move since the last frame.
void trail(Track& t, const entity_t& e, const float m[16], const Blade& b, float glow, float dt, const glm::vec3& color,
    float ownSpeed)
{
    const float amount = za::max(0.f, vr_hotblade_trail.value);
    const float step = glm::distance(b.tip, t.tip);
    const float speed = ownSpeed >= 0.f ? ownSpeed : dt > 0.f ? step / dt : 0.f;
    const float slowest = za::max(0.f, vr_hotblade_trail_speed.value) * units::metresToUnits();
    if(amount <= 0.f || dt <= 0.f || speed <= slowest || step > trailMaxStep || !particles::enabled())
    {
        t.due = 0.f;
        return;
    }
    // The area the heated part swept (a quad: its root's and tip's moves), a faster swing more of it a second; eased in
    // just past the slowest speed.
    const float swept = 0.5f * (step + glm::distance(b.root, t.root)) * glm::distance(b.root, b.tip);
    const float ease = za::clamp((speed - slowest) / za::max(1.f, slowest), 0.f, 1.f);
    t.due += amount * glow * ease * swept / trailAreaEach;
    const float life = za::clamp(vr_hotblade_trail_linger.value, 0.05f, 5.f);
    const glm::vec3 rootVel = (b.root - t.root) / dt;
    const glm::vec3 tipVel = (b.tip - t.tip) / dt;
    int made = 0;
    for(; t.due >= 1.f && made < trailMaxPerFrame; t.due -= 1.f, made++)
    {
        const float u = vrand::range(rng, 0.f, 1.f);
        glm::vec3 was, now, posed;
        if(t.model == e.model && bladeVertex(e, b, posed))
        {
            was = bytePoint(t.m, posed);
            now = bytePoint(m, posed);
        }
        else
        {
            const float s = vrand::range(rng, 0.f, 1.f);
            was = glm::mix(t.root, t.tip, s);
            now = glm::mix(b.root, b.tip, s);
        }
        const glm::vec3 at = glm::mix(was, now, u);
        const glm::vec3 vel = glm::distance(was, now) < trailMaxStep ? (now - was) / dt : glm::mix(rootVel, tipVel, 0.5f);
        const bool spark = vrand::range(rng, 0.f, 1.f) < trailSparkShare;
        // Left behind in the air: a flame keeps a little of the blade's speed, a spark flies off with more of it.
        const glm::vec3 drift = spark ? vel * vrand::range(rng, 0.15f, 0.35f) + glm::vec3{vrand::range(rng, -12.f, 12.f), vrand::range(rng, -12.f, 12.f), vrand::range(rng, -4.f, 14.f)}
                                      : vel * 0.04f + glm::vec3{vrand::range(rng, -2.f, 2.f), vrand::range(rng, -2.f, 2.f), vrand::range(rng, 1.f, 5.f)};
        particles::hotTrail(at, drift, color, 0.6f + 0.4f * glow, life, spark);
    }
    if(t.due >= 1.f)
    {
        t.due = 0.f; // (capped: not owed)
    }
    trailMade += made;
}

// A hand's blade's last frame carried along with the player (his box's move `from` -> `to` and his turn `turn` degrees
// since: hands::State playerOrigin + lean, turnYaw), so its trail measures the swing relative to him: walking, running, riding
// a lift, falling or turning with the stick leaves none.
void carry(Track& t, const glm::vec3& from, const glm::vec3& to, float turn)
{
    const auto place = [&](const glm::vec3& p) { return to + hands::rotateYaw(p - from, turn); };
    t.root = place(t.root);
    t.tip = place(t.tip);
    for(int c = 0; c < 3; c++)
    {
        const glm::vec3 axis = hands::rotateYaw(glm::vec3{t.m[4 * c], t.m[4 * c + 1], t.m[4 * c + 2]}, turn);
        t.m[4 * c] = axis.x;
        t.m[4 * c + 1] = axis.y;
        t.m[4 * c + 2] = axis.z;
    }
    const glm::vec3 origin = place(glm::vec3{t.m[12], t.m[13], t.m[14]});
    t.m[12] = origin.x;
    t.m[13] = origin.y;
    t.m[14] = origin.z;
}

// Track `key`'s (a hand's or a lying blade's): its last frame, or a new one (an unused slot, else the oldest).
[[nodiscard]] Track& trackFor(int key, int first, int count)
{
    Track* oldest = &tracks[first];
    for(int i = first; i < first + count; i++)
    {
        if(tracks[i].key == key && tracks[i].seen >= 0.0)
        {
            return tracks[i];
        }
        if(tracks[i].seen < oldest->seen)
        {
            oldest = &tracks[i];
        }
    }
    *oldest = Track{};
    oldest->key = key;
    return *oldest;
}

} // namespace

void queue(const hands::State& s)
{
    const float dt = lastQueue < 0.0 || cl.time < lastQueue ? 0.f : static_cast<float>(za::clamp(cl.time - lastQueue, 0.0, 0.1));
    lastQueue = cl.time;
    if(statFrom < 0.0 || cl.time < statFrom || cl.time >= statFrom + 1.0)
    {
        embersLast = embersMade;
        trailLast = trailMade;
        embersMade = trailMade = 0;
        statFrom = cl.time;
    }

    // Each hand's glow eased towards the server's (it sends 72 a second, steps of 1/255).
    const bool on = connected();
    const int hotBits = on ? cl.stats[protocol::STAT_QVR_HOTBLADE] : 0;
    for(int hand = 0; hand < 2; hand++)
    {
        const float want = static_cast<float>((hotBits >> (8 * hand)) & 255) / 255.f;
        hot[hand] = want == 0.f ? 0.f : hot[hand] + (want - hot[hand]) * za::min(1.f, dt * 12.f);
    }

    // The glowing blades drawn this frame: the hands', the holsters', the nearest lying about.
    Source src[maxSources];
    int n = 0;
    if(on && vr_hotblade_glow.value > 0.f)
    {
        for(int hand = 0; hand < 2; hand++)
        {
            if(const view::ViewEntity* ve = view::heldWeapon(hand); ve && hot[hand] > 0.f)
            {
                src[n++] = {&ve->ent, hot[hand], hand, 0};
            }
        }
        if(const int bits = cl.stats[protocol::STAT_QVR_HOTBLADEHOLSTER]; bits != 0)
        {
            for(int i = 0; i < protocol::numHolsters; i++)
            {
                const entity_t* e = ((bits >> (4 * i)) & 15) != 0 ? view::holsteredWeapon(i) : nullptr;
                if(e && e->model)
                {
                    src[n++] = {e, static_cast<float>((bits >> (4 * i)) & 15) / 15.f, 2 + i, 0};
                }
            }
        }
        const int firstLying = n;
        float dist[maxSources] = {};
        for(int num = 1; num < cl.num_entities; num++)
        {
            const entity_t* e = &cl_entities[num];
            const client::EntityVr* net = e->model ? client::entityVr(num) : nullptr;
            if(!net || net->hotGlow <= 0.f || num == cl.viewentity)
            {
                continue;
            }
            const float d = glm::distance(geom::vec(e->origin), s.head);
            const int count = n - firstLying;
            if(count == maxLying && d >= dist[n - 1])
            {
                continue;
            }
            int at = count < maxLying ? n++ : n - 1; // kept nearest first
            while(at > firstLying && dist[at - 1] > d)
            {
                src[at] = src[at - 1];
                dist[at] = dist[at - 1];
                at--;
            }
            src[at] = {e, net->hotGlow, 2 + protocol::numHolsters, num};
            dist[at] = d;
        }
    }

    const glm::vec3 color = hsv(vr_hotblade_hue.value, 1.f, 1.f);
    const float rate = za::max(0.f, vr_hotblade_ember_rate.value);
    const int perPuff = za::clamp(static_cast<int>(vr_hotblade_embers.value + 0.5f), 0, 32);
    const float bright = za::clamp(vr_hotblade_glow.value, 0.f, 2.f);
    seenCount = 0;
    bool litNow[maxSources] = {};
    for(int i = 0; i < n; i++)
    {
        const Source& src1 = src[i];
        float m[16];
        matrixOf(*src1.e, m);
        Blade b;
        if(!bladeOf(*src1.e, m, b))
        {
            continue;
        }
        const float g = src1.glow;

        // Its light, from the middle of the heated part, following the glow (a slight flicker: hot metal, not a flame).
        const float flicker = 0.94f + 0.04f * static_cast<float>(za::sin(cl.time * 7.3 + i * 1.7)) +
                              0.02f * static_cast<float>(za::sin(cl.time * 17.9 + i * 2.9));
        const float light = za::max(0.f, vr_hotblade_light.value) * g * flicker;
        const float radius = za::max(0.f, vr_hotblade_light_radius.value) * (0.6f + 0.4f * g);
        emissive::hotBladeLight(i, glm::mix(b.root, b.tip, 0.5f), color, light, radius);
        litNow[i] = light > 0.f && radius > 0.f;

        // Embers off the heated part's surface: puffs of vr_hotblade_embers, vr_hotblade_ember_rate a second at full glow.
        emberDue[i] += dt * rate * g * g;
        if(perPuff == 0 || !particles::enabled())
        {
            emberDue[i] = 0.f;
        }
        for(int puffs = 0; emberDue[i] >= 1.f && puffs < 8; emberDue[i] -= 1.f, puffs++)
        {
            for(int k = 0; k < perPuff; k++)
            {
                glm::vec3 at, out;
                emberPoint(*src1.e, m, b, at, out);
                particles::hotEmber(at + out * 0.2f,
                    out * vrand::range(rng, 1.5f, 6.f) + glm::vec3{vrand::range(rng, -1.5f, 1.5f), vrand::range(rng, -1.5f, 1.5f), vrand::range(rng, 3.f, 9.f)}, color,
                    za::min(1.f, (0.45f + 0.55f * g) * bright));
                embersMade++;
            }
        }
        emberDue[i] = za::min(emberDue[i], 1.f);

        // A hand's blade swung, a thrown one flying: fire and sparks left behind its heated part.
        if(src1.kind < 2 || src1.num > 0)
        {
            Track& t = src1.kind < 2 ? trackFor(-1 - src1.kind, src1.kind, 1) : trackFor(src1.num, 2, maxLying);
            if(t.seen >= 0.0 && cl.time - t.seen < 0.15)
            {
                // A hand's: its swing relative to the player, not his own motion (walking, running, riding, falling,
                // turning with the stick): its tip's speed from the hand's own (play space) velocity and turn, and its
                // last frame carried along with him. A lying or flying one's: the world's.
                float ownSpeed = -1.f;
                if(src1.kind < 2 && s.valid)
                {
                    const float m2u = units::metresToUnits();
                    const glm::vec3 arm = (b.tip - s.pos[src1.kind]) / m2u;
                    ownSpeed = glm::length(s.vel[src1.kind] + glm::cross(s.angVel[src1.kind], arm)) * m2u;
                    if(lastPlayerAt >= 0.0)
                    {
                        carry(t, lastPlayer, s.playerOrigin + s.lean, za::remainder(s.turnYaw - lastTurnYaw, 360.f));
                    }
                }
                trail(t, *src1.e, m, b, g, dt, color, ownSpeed);
            }
            t.seen = cl.time;
            t.root = b.root;
            t.tip = b.tip;
            t.model = src1.e->model;
            for(int k = 0; k < 16; k++)
            {
                t.m[k] = m[k];
            }
        }

        Seen& w = seen[seenCount++];
        w = Seen{};
        w.model = src1.e->model;
        w.kind = src1.kind;
        w.glow = g;
        w.blade = b;
        w.light = light;
        w.radius = radius;
        w.e = src1.e;
    }
    lastPlayer = s.playerOrigin + s.lean;
    lastTurnYaw = s.turnYaw;
    lastPlayerAt = s.valid ? cl.time : -1.0;
    for(int i = 0; i < maxSources; i++)
    {
        if(lit[i] && !litNow[i])
        {
            emissive::hotBladeLight(i, glm::vec3{0.f}, color, 0.f, 0.f);
        }
        lit[i] = litNow[i];
        if(i >= n)
        {
            emberDue[i] = 0.f;
        }
    }
}

float entityGlow(const entity_t* e)
{
    const float g = glowOf(e);
    if(g <= 0.f)
    {
        return 0.f;
    }
    // Its colour in 16 levels a channel, its strength 0..2 (the alias shader's QVR_ALIAS_FS_GLOW reads them back); a
    // low glow raised (the build-up shows).
    const float flicker = 0.93f + 0.07f * static_cast<float>(za::sin(realtime * 7.3));
    const float strength = za::clamp(za::pow(g, 0.7f) * flicker * vr_hotblade_glow.value, 0.05f, 1.95f);
    const glm::vec3 c = hsv(vr_hotblade_hue.value, 1.f, 1.f);
    const auto level = [](float v) { return static_cast<int>(za::clamp(v, 0.f, 1.f) * 15.f + 0.5f); };
    const int code = level(c.r) + 16 * level(c.g) + 256 * level(c.b);
    return 4.f + 2.f * static_cast<float>(code) + strength;
}

void bladePlane(const entity_t* e, const float matrix[16], float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!e || !e->model || glowOf(e) <= 0.f)
    {
        return;
    }
    Blade b;
    if(bladeOf(*e, matrix, b) && b.known)
    {
        out[0] = b.plane.x;
        out[1] = b.plane.y;
        out[2] = b.plane.z;
        out[3] = b.plane.w;
    }
}

void report_f()
{
    Con_Printf("hotblade: client: %d glowing; embers %d, trail %d in the last second; %d, %d since\n", seenCount, embersLast,
        trailLast, embersMade, trailMade);
    for(int i = 0; i < seenCount; i++)
    {
        const Seen& w = seen[i];
        const Blade& b = w.blade;
        const char* where = w.kind == 0 ? "off" : w.kind == 1 ? "main" : w.kind < 2 + protocol::numHolsters ? "holster" : "lying";
        // (How much of the model glows: its vertices as the entity stands now, still the last frame's while it shows the
        // same model; posed only here, for the report.)
        int bladeVerts = 0, verts = 0;
        const aliashdr_t* hdr = connected() && w.e && w.model && w.e->model == w.model && w.model->type == mod_alias
                                    ? static_cast<const aliashdr_t*>(Mod_Extradata(w.e->model))
                                    : nullptr;
        if(hdr && b.known)
        {
            const float zb = zeroBlendOf(*w.e);
            for(int v = 0; v < hdr->numverts; v++)
            {
                const glm::vec3 posed = anchor::posedVertex(*w.e, v, zb);
                bladeVerts += glm::dot(glm::vec3{b.plane}, posed) + b.plane.w >= 0.5f ? 1 : 0;
            }
            verts = hdr->numverts;
        }
        // (Short: one console line.) The heated part's length of the grip-to-tip line, its vertices of the model's.
        Con_Printf("hotblade: client: %s %s glow %.2f part %.1f/%.1f verts %d/%d light %.2f reach %.0f\n", where,
            w.model ? COM_SkipPath(w.model->name) : "?", w.glow, glm::distance(b.root, b.tip), glm::distance(b.grip, b.tip),
            bladeVerts, verts, w.light, w.radius);
        view::WeaponMount mount;
        if(w.kind < 2 && view::weaponMount(w.kind, mount))
        {
            // The blade's ends against the hand's own (the melee's line: the grip, the muzzle).
            Con_Printf("hotblade: client: %s grip off the hand's %.2f, tip off its muzzle %.2f\n", where,
                glm::distance(b.grip, mount.pos), glm::distance(b.tip, mount.muzzle));
        }
    }
}

} // namespace qvr::hotblade
