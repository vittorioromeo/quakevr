// vr_weaponfx.cpp -- see vr_weaponfx.hpp.

#include "vr_weaponfx.hpp"
#include "vr_engine.hpp"
#include "vr_anchor.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_render.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Vocabulary/Span.hpp"

#include <string.h>


namespace qvr::weaponfx
{
namespace
{

using weapons::Key;

constexpr const char* flashModelName = "progs/vr_muzzleflash.mdl"; // (one pointer: view::viewModel's key)
constexpr float flashModelLength = 7.6f; // the flash's length in its model (make_muzzleflash.py's bounds)

// A shot's kick: when, how hard (the weapon's Recoil Strength), how long it takes to come back, and a little sideways.
struct Kick
{
    double time{-1.0};
    float strength{0.f};
    float duration{0.f};
    float sideways{0.f};
};

constexpr int maxKicks = 8; // a burst's rounds overlapping
struct HandFx
{
    int slot{-1}; // the weapon slot that fired: another in the hand, and its effects are gone
    Kick kicks[maxKicks];
    int nextKick{0};
    double flashAt{-1.0}; // the flash shown since (cl.time); -1 none
    float flashRoll{0.f};
    float flashJitter{1.f};
};
HandFx handFx[2];

// A monster's flash (a grunt's or an enforcer's): its entity, since when, turned how.
struct EnemyFlash
{
    int ent{0};
    double at{-1.0};
    float roll{0.f};
};
EnemyFlash enemyFlashes[maxEnemyFlashes];

struct Tracer
{
    glm::vec3 from{0.f};
    glm::vec3 dir{0.f};
    float distance{0.f}; // to what it hit
    float speed{0.f};    // units a second
    float length{0.f};   // units
    float width{0.f};    // units
    glm::vec3 colour{0.f};
    double start{-1.0}; // cl.time; -1 none
};
constexpr int maxTracers = 128; // two hands of super shotgun pellets and a room of grunts
Tracer tracers[maxTracers];
int nextTracer = 0;
int liveTracers = 0;

int lastFrame = -1;

// Visual randomness (no sequence that matters): 0..1.
unsigned seed = 0x2545F491u;
[[nodiscard]] float random01()
{
    seed = seed * 1664525u + 1013904223u;
    return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);
}

// The draws' buffers (the main thread).
struct WeaponFxScratch
{
    za::Vector<glm::vec3> rest; // a monster's vertices (monsterMuzzle)
    za::Vector<glm::vec3> now;
    za::Vector<gfx::Vertex> ribbons; // the tracers (drawTranslucent)
    auto members() { return mem::list(rest, now, ribbons); }
};
mem::Scratch<WeaponFxScratch> scratch{"weaponfx"};

// The monsters' guns (Quake VR's models: each gun a separate piece of its model, vr_monstermods.cpp), rigid: the middle
// of the muzzle's ring or face and of its rear on the barrel's line give its muzzle and which way it points in any frame,
// its recoil animation's too.
// - soldier.mdl (vertices 463..548): found in its first fire frame, shoot5, where it points along +x; the gun's length
//   stays 28.4..28.6 model units in shoot1..9.
// - enforcer.mdl (vertices 22, 23, 100, 400..430, 455..478: the rifle make_enemyguns.py cuts out): its muzzle face (the 7
//   vertices furthest along the rifle) and 5 at its back whose middle lies on the rifle's long axis (0.02 degrees off it
//   in attack6, the fire frame; 0.3 at most in the others); 29.9..30.3 model units apart in every frame.
struct MonsterGun
{
    const char* model;
    int verts;
    za::Span<const int> muzzle;
    za::Span<const int> rear;
};
constexpr int soldierMuzzle[] = {498, 499, 501, 502, 503, 504, 505, 506, 514, 515, 521, 522, 547, 548};
constexpr int soldierRear[] = {465, 497, 500, 510, 544, 545};
constexpr int enforcerMuzzle[] = {22, 100, 400, 401, 402, 403, 407};
constexpr int enforcerRear[] = {404, 405, 423, 424, 425};
const MonsterGun monsterGuns[] = {
    {"progs/soldier.mdl", 555, soldierMuzzle, soldierRear},
    {"progs/enforcer.mdl", 479, enforcerMuzzle, enforcerRear},
};
constexpr float flashOfGun = 0.3f; // a monster's flash, as long as this share of its gun

// The monster gun `e` is drawn with (one of monsterGuns, its model's vertex count checked), or none.
[[nodiscard]] const MonsterGun* monsterGun(const entity_t& e)
{
    if(!e.model || e.model->type != mod_alias)
    {
        return nullptr;
    }
    for(const MonsterGun& g : monsterGuns)
    {
        if(strcmp(e.model->name, g.model) == 0)
        {
            const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
            return hdr->numverts == g.verts ? &g : nullptr;
        }
    }
    return nullptr;
}

// A monster's gun's muzzle and where it points (unit), as drawn; its length in the world. False when it has none.
[[nodiscard]] bool monsterMuzzle(const entity_t& e, glm::vec3& muzzle, glm::vec3& dir, float& gunLength)
{
    const MonsterGun* gun = monsterGun(e);
    if(!gun || !anchor::posedVertices(e, 0.f, scratch.rest, scratch.now) || static_cast<int>(scratch.now.size()) != gun->verts)
    {
        return false;
    }
    const auto middle = [](za::Span<const int> indices) {
        glm::vec3 sum{0.f};
        for(const int i : indices)
        {
            sum += scratch.now[i];
        }
        return sum / static_cast<float>(indices.size());
    };
    float m[16];
    render::entityMatrix(e, false, e.scale ? e.scale : ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
    const auto world = [&](const glm::vec3& v) {
        return glm::vec3{m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
    };
    muzzle = world(middle(gun->muzzle));
    const glm::vec3 back = muzzle - world(middle(gun->rear));
    gunLength = glm::length(back);
    if(gunLength < 1e-3f)
    {
        return false;
    }
    dir = back / gunLength;
    return true;
}

// A kick's share now: up quickly (at most 35 ms), then smoothly back to 0 at `duration`.
[[nodiscard]] float envelope(float t, float duration)
{
    if(t < 0.f || t >= duration)
    {
        return 0.f;
    }
    const float rise = za::min(0.035f, 0.3f * duration);
    if(t < rise)
    {
        const float x = 1.f - t / rise;
        return 1.f - x * x;
    }
    const float x = (t - rise) / (duration - rise);
    return 1.f - x * x * (3.f - 2.f * x);
}

void forget(int hand)
{
    HandFx& h = handFx[hand];
    h = HandFx{};
}

// `hand`'s weapon fired (the QC's, or vr_weaponfx_test).
void playerFired(int hand)
{
    const int slot = weapons::heldSlot(hand);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return;
    }
    HandFx& h = handFx[hand];
    if(h.slot != slot)
    {
        forget(hand);
        h.slot = slot;
    }
    const bool recoil = vr_weapon_recoil.value != 0.f && weapons::value(slot, Key::Recoil) != 0.f;
    if(recoil)
    {
        Kick& k = h.kicks[h.nextKick];
        h.nextKick = (h.nextKick + 1) % maxKicks;
        k.time = cl.time;
        k.strength = za::max(weapons::value(slot, Key::RecoilStrength), 0.f);
        k.duration = za::max(weapons::value(slot, Key::RecoilTime), 0.02f);
        k.sideways = random01() * 2.f - 1.f;
    }
    const bool flash = vr_muzzle_flash.value != 0.f && weapons::value(slot, Key::Flash) != 0.f;
    if(flash)
    {
        h.flashAt = cl.time;
        h.flashRoll = random01() * 360.f;
        h.flashJitter = 0.85f + 0.3f * random01();
    }
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("weaponfx fired t %.3f hand %d slot %d recoil %d (%.2f, %.3f s) flash %d (%.2f, %.3f s)\n", cl.time, hand,
            slot, recoil ? 1 : 0, weapons::value(slot, Key::RecoilStrength), weapons::value(slot, Key::RecoilTime), flash ? 1 : 0,
            weapons::value(slot, Key::FlashSize), weapons::value(slot, Key::FlashTime));
    }
}

void monsterFired(int ent)
{
    if(!vr_muzzle_flash.value || !vr_muzzle_flash_enemies.value || ent <= 0 || ent >= cl.num_entities ||
        !monsterGun(cl_entities[ent]))
    {
        return;
    }
    // Its own flash again, else the oldest.
    EnemyFlash* slot = &enemyFlashes[0];
    for(EnemyFlash& f : enemyFlashes)
    {
        if(f.at >= 0.0 && f.ent == ent)
        {
            slot = &f;
            break;
        }
        if(f.at < slot->at)
        {
            slot = &f;
        }
    }
    slot->ent = ent;
    slot->at = cl.time;
    slot->roll = random01() * 360.f;
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("weaponfx monster fired t %.3f ent %d (%s)\n", cl.time, ent, cl_entities[ent].model->name);
    }
    if(vr_debug_weaponfx.value >= 3)
    {
        // Tests (Misc/quakevr/weaponfx/shots_test.sh): the eyes' view of this flash (the next frame's), once.
        Cvar_SetQuick(&vr_eyeshot, "1");
        Cvar_SetQuick(&vr_debug_weaponfx, "1");
    }
}

void addTracer(const glm::vec3& from, const glm::vec3& to, int slot)
{
    const glm::vec3 line = to - from;
    const float distance = glm::length(line);
    if(distance < 8.f) // (a pellet that hit what the muzzle was inside)
    {
        return;
    }
    const auto mult = [&](Key key) { return slot >= 0 ? za::max(weapons::value(slot, key), 0.f) : 1.f; };
    Tracer& t = tracers[nextTracer];
    nextTracer = (nextTracer + 1) % maxTracers;
    t.from = from;
    t.dir = line / distance;
    t.distance = distance;
    const float m2u = units::metresToUnits();
    t.speed = za::max(vr_tracer_speed.value * mult(Key::TracerSpeed), 1.f) * m2u;
    t.length = za::max(vr_tracer_length.value * mult(Key::TracerLength), 0.01f) * m2u;
    t.width = za::max(vr_tracer_width.value * mult(Key::TracerWidth), 0.05f) * 0.01f * m2u;
    const bool own = slot >= 0 && weapons::value(slot, Key::TracerOwnColour) != 0.f;
    t.colour = (own ? weapons::vec(slot, Key::TracerRed, Key::TracerGreen, Key::TracerBlue)
                    : glm::vec3{vr_tracer_r.value, vr_tracer_g.value, vr_tracer_b.value}) *
               za::max(vr_tracer_brightness.value, 0.f);
    t.start = cl.time;
    liveTracers++;
}

// The weapon in `hand`'s tracer settings: whether it draws them (its Tracers: 0 as vr_tracers, 1 off, 2 on).
[[nodiscard]] bool tracersOn(int slot)
{
    const int mode = slot >= 0 ? static_cast<int>(weapons::value(slot, Key::Tracers)) : 0;
    return mode == 2 || (mode == 0 && vr_tracers.value != 0.f);
}

// vr_weaponfx_test [hand] [tracers]: the held weapon's effects as if it fired (no shot), and `tracers` tracers (its
// settings' chance aside) from its drawn muzzle straight ahead, 1000 units.
void test_f()
{
    const int hand = Cmd_Argc() > 1 ? (atoi(Cmd_Argv(1)) == 0 ? HAND_OFF : HAND_MAIN) : HAND_MAIN;
    const int count = Cmd_Argc() > 2 ? za::clamp(atoi(Cmd_Argv(2)), 0, 32) : 1;
    playerFired(hand);
    view::WeaponMount m;
    if(count > 0 && view::weaponMount(hand, m))
    {
        const int slot = weapons::heldSlot(hand);
        const glm::vec3 fwd = hands::forward(hands::current().rot[hand]);
        for(int i = 0; i < count; i++)
        {
            const glm::vec3 spread{random01() - 0.5f, random01() - 0.5f, random01() - 0.5f};
            addTracer(m.muzzle, m.muzzle + glm::normalize(fwd + spread * (count > 1 ? 0.06f : 0.f)) * 1000.f, slot);
        }
    }
    Con_Printf("vr_weaponfx_test: hand %d slot %d, %d tracers\n", hand, weapons::heldSlot(hand), count);
}

} // namespace

void parseFired()
{
    const int ent = MSG_ReadShort();
    const int hand = MSG_ReadByte();
    if(hand <= 1)
    {
        if(ent == cl.viewentity)
        {
            playerFired(hand);
        }
        return;
    }
    monsterFired(ent);
}

void parseTracer()
{
    const int ent = MSG_ReadShort();
    const int hand = MSG_ReadByte();
    glm::vec3 from, to;
    for(int i = 0; i < 3; i++)
    {
        from[i] = MSG_ReadCoord(cl.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        to[i] = MSG_ReadCoord(cl.protocolflags);
    }

    const bool own = hand <= 1 && ent == cl.viewentity;
    const int slot = own ? weapons::heldSlot(hand) : -1;
    if(!tracersOn(slot) || (hand > 1 && !vr_tracers_enemies.value))
    {
        return;
    }
    const float chance = vr_tracer_chance.value * (slot >= 0 ? za::max(weapons::value(slot, Key::TracerChance), 0.f) : 1.f);
    if(random01() >= chance)
    {
        return;
    }
    // A grunt's from its gun's muzzle (the game's from its middle).
    glm::vec3 muzzle, dir;
    float gunLength;
    if(hand > 1 && ent > 0 && ent < cl.num_entities && monsterMuzzle(cl_entities[ent], muzzle, dir, gunLength))
    {
        from = muzzle;
    }
    addTracer(from, to, slot);
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("weaponfx tracer t %.3f ent %d hand %d slot %d from %.1f %.1f %.1f to %.1f %.1f %.1f\n", cl.time, ent, hand, slot,
            from.x, from.y, from.z, to.x, to.y, to.z);
    }
}

void recoilOffset(int hand, const glm::vec3& handAngles, glm::vec3& pos, glm::vec3& angles)
{
    pos = glm::vec3{0.f};
    angles = glm::vec3{0.f};
    HandFx& h = handFx[hand];
    if(h.slot < 0)
    {
        return;
    }
    if(!vr_weapon_recoil.value || weapons::heldSlot(hand) != h.slot)
    {
        // Gone from the hand (or recoil turned off): its kicks with it.
        for(Kick& k : h.kicks)
        {
            k = Kick{};
        }
        return;
    }
    float kick = 0.f, sideways = 0.f;
    for(const Kick& k : h.kicks)
    {
        if(k.time >= 0.0)
        {
            const float e = envelope(static_cast<float>(cl.time - k.time), k.duration) * k.strength;
            kick += e;
            sideways += e * k.sideways;
        }
    }
    // A burst's kicks adding up go no further than two and a half.
    if(kick > 2.5f)
    {
        sideways *= 2.5f / kick;
        kick = 2.5f;
    }
    if(kick <= 0.f)
    {
        return;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors(handAngles, fwd, right, up);
    const float back = vr_recoil_kick.value * kick * 0.01f * units::metresToUnits();
    pos = -fwd * back;
    angles.x = -vr_recoil_rise.value * kick; // tipped up (pitch down is positive)
    angles.y = 0.25f * vr_recoil_rise.value * sideways;
    if(vr_debug_weaponfx.value >= 2)
    {
        Con_Printf("weaponfx recoil t %.3f hand %d kick %.3f back %.3f cm pitch %.3f yaw %.3f\n", cl.time, hand, kick,
            vr_recoil_kick.value * kick, angles.x, angles.y);
    }
}

void frame(const view::ViewEntity (&weapons)[2], view::ViewEntity (&flashes)[2],
    view::ViewEntity (&enemies)[maxEnemyFlashes])
{
    if(lastFrame == host_framecount)
    {
        return;
    }
    lastFrame = host_framecount;
    QVR_PROFILE("weapon fx");

    qmodel_t* model = view::viewModel(flashModelName);
    const auto place = [&](view::ViewEntity& ve, const glm::vec3& origin, const glm::vec3& fwd, const glm::vec3& upIn,
                           float roll, float scale) {
        // The flash's +x along the gun, turned about it by `roll`.
        const glm::vec3 up0 = glm::normalize(upIn - fwd * glm::dot(upIn, fwd));
        const float r = roll * (3.14159265f / 180.f);
        const glm::vec3 up = up0 * za::cos(r) + glm::cross(fwd, up0) * za::sin(r);
        const glm::vec3 a = hands::anglesFromVectors(fwd, up);
        entity_t& e = ve.ent;
        if(model != ve.lastModel)
        {
            e.lerpflags |= LERP_RESETANIM;
            ve.lastModel = model;
        }
        e.model = model;
        e.frame = 0;
        e.skinnum = 0;
        e.colormap = vid.colormap;
        e.alpha = ENTALPHA_DEFAULT;
        e.scale = ENTSCALE_DEFAULT;
        e.origin[0] = origin.x;
        e.origin[1] = origin.y;
        e.origin[2] = origin.z;
        e.angles[0] = -a.x;
        e.angles[1] = a.y;
        e.angles[2] = a.z;
        ve.scale = glm::vec3{scale};
        ve.mirrored = false;
        ve.zeroBlend = 0.f;
        ve.lightMultiply = false;
        ve.visible = model != nullptr;
    };

    // The hands' weapons.
    for(int hand = 0; hand < 2; hand++)
    {
        HandFx& h = handFx[hand];
        view::ViewEntity& fve = flashes[hand];
        fve.visible = false;
        if(h.flashAt < 0.0)
        {
            continue;
        }
        const view::ViewEntity& w = weapons[hand];
        const int slot = w.visible && w.ent.model ? weapons::slotForModel(w.ent.model) : -1;
        const float age = static_cast<float>(cl.time - h.flashAt);
        // Gone with the weapon (dropped, holstered, thrown, in the other hand now: this hand draws another, or nothing).
        if(!vr_muzzle_flash.value || slot != h.slot || slot < 0 || age < 0.f ||
            age >= za::max(weapons::value(slot, Key::FlashTime), 0.01f))
        {
            if(vr_debug_weaponfx.value)
            {
                Con_Printf("weaponfx flash gone t %.3f hand %d after %.3f s: %s\n", cl.time, hand, age,
                    slot != h.slot ? "the weapon left the hand" : "its time");
            }
            h.flashAt = -1.0;
            continue;
        }
        const glm::vec3 muzzle = view::anchorPosition(w, static_cast<int>(weapons::value(slot, Key::MuzzleAnchorVertex)),
            weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
        const glm::vec3 o = view::modelPoint(w, glm::vec3{0.f});
        const glm::vec3 ax = view::modelPoint(w, glm::vec3{1.f, 0.f, 0.f}) - o;
        const glm::vec3 az = view::modelPoint(w, glm::vec3{0.f, 0.f, 1.f}) - o;
        const float perUnit = glm::length(ax); // the weapon's world units a model unit: its Scale times the models' scale
        const float weaponScale = weapons::value(slot, Key::Scale);
        if(perUnit < 1e-5f || weaponScale <= 0.f)
        {
            continue;
        }
        place(fve, muzzle, ax / perUnit, az, h.flashRoll,
            perUnit / weaponScale * za::max(weapons::value(slot, Key::FlashSize), 0.f) * h.flashJitter);
    }

    // The grunts'.
    const float enemyTime = 0.06f;
    for(int i = 0; i < maxEnemyFlashes; i++)
    {
        EnemyFlash& f = enemyFlashes[i];
        view::ViewEntity& fve = enemies[i];
        fve.visible = false;
        if(f.at < 0.0)
        {
            continue;
        }
        const float age = static_cast<float>(cl.time - f.at);
        glm::vec3 muzzle, dir;
        float gunLength;
        if(!vr_muzzle_flash.value || !vr_muzzle_flash_enemies.value || age < 0.f || age >= enemyTime ||
            f.ent <= 0 || f.ent >= cl.num_entities || !monsterMuzzle(cl_entities[f.ent], muzzle, dir, gunLength))
        {
            f.at = -1.0;
            continue;
        }
        place(fve, muzzle, dir, glm::vec3{0.f, 0.f, 1.f}, f.roll,
            flashOfGun * gunLength / flashModelLength * za::max(vr_muzzle_flash_enemy_size.value, 0.f));
    }

    // The tracers that ended.
    liveTracers = 0;
    for(Tracer& t : tracers)
    {
        if(t.start < 0.0)
        {
            continue;
        }
        const float flown = static_cast<float>(cl.time - t.start) * t.speed;
        if(flown - t.length >= t.distance || cl.time < t.start)
        {
            t.start = -1.0;
            continue;
        }
        liveTracers++;
    }
}

void drawTranslucent()
{
    if(liveTracers == 0)
    {
        return;
    }
    QVR_PROFILE("tracers");
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    za::Vector<gfx::Vertex>& out = scratch.ribbons;
    out.clear();
    for(const Tracer& t : tracers)
    {
        if(t.start < 0.0)
        {
            continue;
        }
        const float flown = static_cast<float>(cl.time - t.start) * t.speed;
        const float head = za::min(flown, t.distance);
        const float tail = za::clamp(flown - t.length, 0.f, t.distance);
        if(head - tail < 0.5f)
        {
            continue;
        }
        const glm::vec3 a = t.from + t.dir * tail;
        const glm::vec3 b = t.from + t.dir * head;
        // A ribbon along it, turned to face the eye; faint at its tail, full at its head (added onto the scene).
        glm::vec3 side = glm::cross(b - a, eye - (a + b) * 0.5f);
        if(glm::length(side) < 1e-4f)
        {
            continue;
        }
        side = glm::normalize(side) * (t.width * 0.5f);
        // The streak's tail end fades in from where the full length would start.
        const float tailShare = t.length > 0.f ? za::clamp(1.f - (head - tail) / t.length, 0.f, 1.f) : 0.f;
        const glm::vec4 cb{t.colour, 0.f};
        const glm::vec4 ca{t.colour * (0.1f + 0.5f * tailShare), 0.f};
        const glm::vec3 p[4]{a - side, b - side, b + side, a + side};
        const glm::vec4 c[4]{ca, cb, cb, ca};
        const glm::vec2 uv[4]{{0.f, -1.f}, {0.f, -1.f}, {0.f, 1.f}, {0.f, 1.f}};
        for(const int i : {0, 1, 2, 0, 2, 3})
        {
            out.pushBack({p[i], uv[i], c[i]});
        }
    }
    if(out.empty())
    {
        return;
    }
    gfx::State state;
    state.shade = gfx::Shade::SoftEdge;
    state.blend = gfx::Blend::Premultiplied;
    state.depthTest = true;
    state.depthWrite = false;
    gfx::draw(out, gfx::sceneViewProjection(), state);
}

void prepare()
{
    (void)view::viewModel(flashModelName);
}

void clear()
{
    forget(HAND_OFF);
    forget(HAND_MAIN);
    for(EnemyFlash& f : enemyFlashes)
    {
        f = EnemyFlash{};
    }
    for(Tracer& t : tracers)
    {
        t.start = -1.0;
    }
    nextTracer = 0;
    liveTracers = 0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_weaponfx_test", test_f);
}

} // namespace qvr::weaponfx
