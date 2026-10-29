// vr_weaponfx.cpp -- see vr_weaponfx.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_weaponfx.hpp"
#include "vr_adcrossbow.hpp"
#include "vr_adweapons.hpp"
#include "vr_adguns.hpp"
#include "vr_autopump.hpp"
#include "vr_bttrails.hpp"
#include "vr_engine.hpp"
#include "vr_anchor.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_mem.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_render.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"
#include "vr_weight.hpp"

#include "Zancle/Base/GetArraySize.hpp"
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
    int variant{-1};    // an AD gun variant's (vr_adguns.hpp FireFeel), else -1
    bool burst{false};  // its shot's particles still to leave its muzzle (frame: where the gun is drawn)
};
HandFx handFx[2];

// The AD gun variant `hand` holds (vr_adgunart.cpp), else -1.
[[nodiscard]] int heldVariant(int hand)
{
    const qmodel_t* m = weapons::heldModel(hand);
    return m ? adguns::variantOf(m->name) : -1;
}

// A monster's flash (a grunt's or an enforcer's): its entity, since when, turned how.
struct EnemyFlash
{
    int ent{0};
    double at{-1.0};
    float roll{0.f};
    // A weapon a monster holds (QC vr_armedgrunt.qc: an armed grunt's, drawn as a lying weapon): its slot's own flash at
    // its Muzzle anchor, as yours (-1: a monster's own gun, monsterGuns).
    int slot{-1};
    int variant{-1};
    bool told{false}; // (vr_debug_weaponfx: its place printed)
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
    za::Vector<gfx::Vertex> ribbons; // the tracers (drawTranslucent)
    auto members() { return mem::list(ribbons); }
};
mem::Scratch<WeaponFxScratch> scratch{"weaponfx"};

// The monsters' guns (Quake VR's models: each gun a separate piece of its model, vr_monstermods.cpp), rigid: the middle
// of the muzzle's ring or face and of its rear on the barrel's line give its muzzle and which way it points in any frame,
// its recoil animation's too. Anchors (vr_anchor.hpp) at those vertices, each once, with how many of the vertices it
// stands for (a UV seam's twins share a point: the middle is the vertices' mean, as when they were picked; none: one
// each); the model's vertex count says it is the model they were picked on.
// - soldier.mdl (vertices 463..548): found in its first fire frame, shoot5, where it points along +x; the gun's length
//   stays 28.4..28.6 model units in shoot1..9.
// - enforcer.mdl (vertices 22, 23, 100, 400..430, 455..478: the rifle make_enemyguns.py cuts out): its muzzle face (the 7
//   vertices furthest along the rifle) and 5 at its back whose middle lies on the rifle's long axis (0.02 degrees off it
//   in attack6, the fire frame; 0.3 at most in the others); 29.9..30.3 model units apart in every frame.
struct MonsterGun
{
    modelmeta::Id model;
    int verts;
    za::Span<const char* const> muzzle;
    za::Span<const char* const> rear;
    za::Span<const float> muzzleTwins; // per muzzle anchor, the vertices it stands for (empty: 1 each)
    za::Span<const float> rearTwins;
};
constexpr const char* soldierMuzzle[] = {"13.8246 14.6358 5.0417", "12.9522 14.3738 2.6583", "15.5694 13.8499 5.3065",
    "14.4062 13.3259 1.8638", "16.7327 12.8019 4.2472", "15.8603 12.5399 1.8638"};
constexpr float soldierMuzzleTwins[] = {2.f, 2.f, 3.f, 3.f, 2.f, 2.f}; // (14 vertices)
constexpr const char* soldierRear[] = {"2.1924 -10.5148 12.1918", "3.3556 -11.3008 11.927", "2.4832 -11.3008 9.014",
    "4.5188 -11.8248 11.3974"};
constexpr float soldierRearTwins[] = {1.f, 2.f, 2.f, 1.f}; // (6 vertices)
constexpr const char* enforcerMuzzle[] = {"4.8719 21.5425 14.2786", "4.8719 21.5425 18.402", "7.3325 21.5425 17.5184",
    "7.0249 21.5425 15.7512", "4.8719 21.5425 16.3403", "2.719 21.868 15.4567", "3.0266 21.868 17.2238"};
constexpr const char* enforcerRear[] = {"3.6417 -8.3993 16.6348", "2.719 -7.7484 17.5184", "3.3341 -11.9793 11.9224",
    "4.8719 -7.0975 11.3333", "4.2568 -11.9793 11.9224"};
static_assert(za::getArraySize(soldierMuzzle) == za::getArraySize(soldierMuzzleTwins) &&
              za::getArraySize(soldierRear) == za::getArraySize(soldierRearTwins));
const MonsterGun monsterGuns[] = {
    {modelmeta::Id::Soldier, 555, soldierMuzzle, soldierRear, soldierMuzzleTwins, soldierRearTwins},
    {modelmeta::Id::Enforcer, 479, enforcerMuzzle, enforcerRear, {}, {}},
};
constexpr float flashOfGun = 0.3f; // a monster's flash, as long as this share of its gun

// The monster gun `e` is drawn with (one of monsterGuns, its model's vertex count checked), or none.
[[nodiscard]] const MonsterGun* monsterGun(const entity_t& e)
{
    if(!e.model || e.model->type != mod_alias)
    {
        return nullptr;
    }
    const auto& info = modelmeta::get(e.model);
    for(const MonsterGun& g : monsterGuns)
    {
        if(info.is(g.model))
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
    if(!gun)
    {
        return false;
    }
    const auto middle = [&](za::Span<const char* const> anchors, za::Span<const float> twins) {
        glm::vec3 sum{0.f};
        float count = 0.f;
        for(size_t i = 0; i < anchors.size(); i++)
        {
            const float w = twins.empty() ? 1.f : twins[i];
            sum += anchor::posedAnchor(e, anchors[i], 0.f) * w;
            count += w;
        }
        return sum / count;
    };
    float m[16];
    render::entityMatrix(e, false, e.scale ? e.scale : ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
    const auto world = [&](const glm::vec3& v) {
        return glm::vec3{m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
    };
    muzzle = world(middle(gun->muzzle, gun->muzzleTwins));
    const glm::vec3 back = muzzle - world(middle(gun->rear, gun->rearTwins));
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

// The recoil of a kick `kick` (its shots' strengths at their envelopes, summed and capped) and `sideways` of slot
// `slot`'s weapon: cm kicked back, degrees tipped up (pitch, negative) and turned (yaw): Weapon Effects' Kick Back and
// Muzzle Rise times the weapon's own (Recoil Kick Back, Recoil Muzzle Rise: 1, as before, by default).
struct RecoilAmount
{
    float backCm{0.f};
    float pitch{0.f};
    float yaw{0.f};
};

[[nodiscard]] RecoilAmount recoilAmount(int slot, float kick, float sideways)
{
    const float kickX = za::max(weapons::value(slot, Key::RecoilKick), 0.f);
    const float riseX = za::max(weapons::value(slot, Key::RecoilRise), 0.f);
    const float rise = vr_recoil_rise.value * riseX;
    return {vr_recoil_kick.value * kickX * kick, -rise * kick, 0.25f * rise * sideways};
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
    if(slot == weapons::slotForName("progs/v_shot.mdl"))
    {
        autopump::fired(hand); // (the shotgun's auto pump: vr_autopump.cpp)
    }
    adcrossbow::fired(hand); // AD support: the crossbow's string and bolt (vr_adcrossbow.cpp: if it is one)
    adweapons::fired(hand);  // AD support: the Widowmaker's barrels (vr_adweapons.cpp: if it is one)
    weight::heavyKick(hand); // heavy handling: a heavy weapon fired in one hand kicks up (the aim with it)
    HandFx& h = handFx[hand];
    if(h.slot != slot)
    {
        forget(hand);
        h.slot = slot;
    }
    // An AD gun variant's own feel over its slot's (the burst rifle's) settings.
    h.variant = heldVariant(hand);
    const adguns::FireFeel feel = h.variant >= 0 ? adguns::fireFeel(h.variant) : adguns::FireFeel{};
    h.burst = h.variant >= 0 && feel.burst != adguns::FireFeel::None;
    const bool recoil = vr_weapon_recoil.value != 0.f && weapons::value(slot, Key::Recoil) != 0.f;
    if(recoil)
    {
        Kick& k = h.kicks[h.nextKick];
        h.nextKick = (h.nextKick + 1) % maxKicks;
        k.time = cl.time;
        k.strength = za::max(weapons::value(slot, Key::RecoilStrength), 0.f) * feel.recoil;
        k.duration = za::max(weapons::value(slot, Key::RecoilTime) * feel.recoilTime, 0.02f);
        k.sideways = random01() * 2.f - 1.f;
    }
    const bool flash = vr_muzzle_flash.value != 0.f && weapons::value(slot, Key::Flash) != 0.f && feel.flash > 0.f;
    if(flash)
    {
        h.flashAt = cl.time;
        h.flashRoll = random01() * 360.f;
        h.flashJitter = 0.85f + 0.3f * random01();
    }
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("weaponfx fired t %.3f hand %d slot %d recoil %d (%.2f, %.3f s) flash %d (%.2f, %.3f s) adgun %d burst %d\n",
            cl.time, hand, slot, recoil ? 1 : 0, weapons::value(slot, Key::RecoilStrength) * feel.recoil,
            weapons::value(slot, Key::RecoilTime) * feel.recoilTime, flash ? 1 : 0, weapons::value(slot, Key::FlashSize) * feel.flash,
            weapons::value(slot, Key::FlashTime) * feel.flashTime, h.variant, static_cast<int>(feel.burst));
    }
}

// An enforcer's rifle or a grunt's gun fired: a puff of gun smoke out of its muzzle (as your guns' make, QC particle2
// GUNSMOKE; vr_muzzle_smoke_enemies; the grunts' since NOTES.md vrfiringrange_2026-10-02_15-52-01). Each of a burst's
// rounds is a shot (weapons.qc), and a puff.
void monsterSmoke(int ent)
{
    if(!vr_muzzle_smoke_enemies.value || ent <= 0 || ent >= cl.num_entities)
    {
        return;
    }
    const entity_t& e = cl_entities[ent];
    if(!e.model || (!modelmeta::is(e.model, modelmeta::Id::Enforcer) && !modelmeta::is(e.model, modelmeta::Id::Soldier)))
    {
        return;
    }
    glm::vec3 muzzle, dir;
    float gunLength = 0.f;
    if(!monsterMuzzle(e, muzzle, dir, gunLength))
    {
        return;
    }
    if(!particles::spawn(muzzle, glm::vec3{0.f}, particles::Preset::GunSmoke, 1))
    {
        vec3_t org{muzzle.x, muzzle.y, muzzle.z}, still{0.f, 0.f, 0.f};
        R_RunParticleEffect(org, still, 4, 1); // (vr_particles 0: Quake's, as the QC's GUNSMOKE then)
    }
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("weaponfx monster smoke ent %d at %.1f %.1f %.1f\n", ent, muzzle.x, muzzle.y, muzzle.z);
    }
}

// The weapon slot of a weapon held by a monster (an armed grunt's: a weapon model, not the empty hand's), else -1.
[[nodiscard]] int heldPropSlot(const entity_t& e)
{
    if(!e.model || e.model->type != mod_alias)
    {
        return -1;
    }
    const int slot = weapons::slotForModel(e.model);
    return slot == weapons::fistSlot() ? -1 : slot;
}

void monsterFired(int ent)
{
    const int propSlot = ent > 0 && ent < cl.num_entities ? heldPropSlot(cl_entities[ent]) : -1;
    if(propSlot < 0)
    {
        monsterSmoke(ent); // (a held weapon's: the QC's, at its muzzle)
    }
    if(!vr_muzzle_flash.value || !vr_muzzle_flash_enemies.value || ent <= 0 || ent >= cl.num_entities ||
        (propSlot < 0 && !monsterGun(cl_entities[ent])))
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
    slot->slot = propSlot;
    slot->told = false;
    slot->variant = propSlot >= 0 ? adguns::variantOf(cl_entities[ent].model->name) : -1;
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("weaponfx monster fired t %.3f ent %d (%s) slot %d\n", cl.time, ent, cl_entities[ent].model->name, propSlot);
    }
    if(vr_debug_weaponfx.value >= 3)
    {
        // Tests (Misc/quakevr/weaponfx/shots_test.sh): the eyes' view of this flash (the next frame's), once.
        Cvar_SetQuick(&vr_eyeshot, "1");
        Cvar_SetQuick(&vr_debug_weaponfx, "1");
    }
}

// The tracers' speed (units a second) for the weapon in `slot` (-1: a monster's, the settings' own).
[[nodiscard]] float tracerSpeed(int slot)
{
    const float mult = slot >= 0 ? za::max(weapons::value(slot, Key::TracerSpeed), 0.f) : 1.f;
    return za::max(vr_tracer_speed.value * mult, 1.f) * units::metresToUnits();
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
    t.speed = tracerSpeed(slot);
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
            const glm::vec3 to = m.muzzle + glm::normalize(fwd + spread * (count > 1 ? 0.06f : 0.f)) * 1000.f;
            addTracer(m.muzzle, to, slot);
            bttrails::hitscan(m.muzzle, to, tracerSpeed(slot), false);
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
    bool tracer = tracersOn(slot) && !(hand > 1 && !vr_tracers_enemies.value);
    if(tracer)
    {
        const float chance = vr_tracer_chance.value * (slot >= 0 ? za::max(weapons::value(slot, Key::TracerChance), 0.f) : 1.f);
        tracer = random01() < chance;
    }
    const bool trail = bttrails::wanted(hand > 1); // bullet time's distortion trail, tracer or not (vr_bttrails.cpp)
    if(!tracer && !trail)
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
    if(trail)
    {
        bttrails::hitscan(from, to, tracerSpeed(slot), hand > 1);
    }
    if(!tracer)
    {
        return;
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
    const RecoilAmount a = recoilAmount(h.slot, kick, sideways);
    pos = -fwd * (a.backCm * 0.01f * units::metresToUnits());
    angles.x = a.pitch;
    angles.y = a.yaw;
    if(vr_debug_weaponfx.value >= 2)
    {
        Con_Printf("weaponfx recoil t %.3f hand %d kick %.3f back %.3f cm pitch %.3f yaw %.3f\n", cl.time, hand, kick,
            a.backCm, angles.x, angles.y);
    }
}

// An AD gun variant's shot leaving `w` (drawn; its slot `slot`): sparks and smoke out of its muzzle as it fires (a
// launcher's a cloud and a shower, the shotgun's burning powder, the plasma guns' sparks in their colour), the rocket's
// back-blast out of its exhaust (flames, sparks, smoke). Nothing with vr_particles 0.
namespace
{
void adGunBurst(const view::ViewEntity& w, int slot, const adguns::FireFeel& feel)
{
    const glm::vec3 muzzle = view::anchorPosition(w, weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    const glm::vec3 o = view::modelPoint(w, glm::vec3{0.f});
    const glm::vec3 ax = view::modelPoint(w, glm::vec3{1.f, 0.f, 0.f}) - o;
    if(glm::length(ax) < 1e-5f)
    {
        return;
    }
    const glm::vec3 fwd = glm::normalize(ax);
    const auto jitter = [](float k) { return glm::vec3{random01() - 0.5f, random01() - 0.5f, random01() - 0.5f} * (2.f * k); };
    // `n` sparks from `at` along `dir`, `speed` units a second, spread `spread` of it.
    const auto sparks = [&](const glm::vec3& at, const glm::vec3& dir, int n, float speed, float spread, const glm::vec3& c) {
        for(int i = 0; i < n; i++)
        {
            const float s = speed * (0.5f + random01());
            particles::hotTrail(at, dir * s + jitter(s * spread), c, 1.f, 0.25f + 0.25f * random01(), true);
        }
    };
    switch(feel.burst)
    {
        case adguns::FireFeel::Rocket:
            (void)particles::spawn(muzzle, fwd * 0.5f, particles::Preset::GunSmoke, 5);
            sparks(muzzle, fwd, 10, 180.f, 0.35f, feel.colour);
            break;
        case adguns::FireFeel::Grenade:
            (void)particles::spawn(muzzle, fwd * 0.4f, particles::Preset::GunSmoke, 4);
            sparks(muzzle, fwd, 5, 120.f, 0.4f, feel.colour);
            break;
        case adguns::FireFeel::Shotgun: sparks(muzzle, fwd, 8, 220.f, 0.3f, feel.colour); break;
        case adguns::FireFeel::Electric:
            sparks(muzzle, fwd, 8, 140.f, 0.6f, feel.colour);
            sparks(muzzle, fwd, 3, 90.f, 0.8f, glm::vec3{0.9f, 0.95f, 1.f});
            break;
        default: break;
    }
    if(feel.backBlast)
    {
        const glm::vec3 at = view::modelPoint(w, feel.exhaust);
        const glm::vec3 back = glm::normalize(view::modelPoint(w, feel.exhaust + feel.exhaustDir) - at + glm::vec3{1e-6f});
        (void)particles::spawn(at, back * 0.8f, particles::Preset::AdFlame, 2);
        (void)particles::spawn(at, back * 0.6f, particles::Preset::GunSmoke, 6);
        sparks(at, back, 8, 150.f, 0.45f, feel.colour);
        for(int i = 0; i < 4; i++)
        {
            particles::hotTrail(at, back * (60.f + 60.f * random01()) + jitter(15.f), feel.colour, 1.f, 0.2f + 0.15f * random01(), false);
        }
    }
    if(vr_debug_weaponfx.value)
    {
        Con_Printf("weaponfx adgun burst %d at %.1f %.1f %.1f%s\n", static_cast<int>(feel.burst), muzzle.x, muzzle.y, muzzle.z,
            feel.backBlast ? ", back-blast" : "");
    }
}
} // namespace

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
        const view::ViewEntity& w = weapons[hand];
        const int slot = w.visible && w.ent.model ? weapons::slotForModel(w.ent.model) : -1;
        const adguns::FireFeel feel = h.variant >= 0 ? adguns::fireFeel(h.variant) : adguns::FireFeel{};
        if(h.burst)
        {
            h.burst = false;
            if(slot == h.slot && slot >= 0)
            {
                adGunBurst(w, slot, feel);
            }
        }
        if(h.flashAt < 0.0)
        {
            continue;
        }
        const float age = static_cast<float>(cl.time - h.flashAt);
        // Gone with the weapon (dropped, holstered, thrown, in the other hand now: this hand draws another, or nothing).
        if(!vr_muzzle_flash.value || slot != h.slot || slot < 0 || age < 0.f ||
            age >= za::max(weapons::value(slot, Key::FlashTime) * feel.flashTime, 0.01f))
        {
            if(vr_debug_weaponfx.value)
            {
                Con_Printf("weaponfx flash gone t %.3f hand %d after %.3f s: %s\n", cl.time, hand, age,
                    slot != h.slot ? "the weapon left the hand" : "its time");
            }
            h.flashAt = -1.0;
            continue;
        }
        const glm::vec3 muzzle = view::anchorPosition(w, weapons::anchor(slot, Key::MuzzleAnchor),
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
        // (an AD plasma gun's flash in its colour: vr_adgunart.cpp flashFile)
        qmodel_t* own = feel.flashModel ? view::viewModel(feel.flashModel) : nullptr;
        model = own ? own : view::viewModel(flashModelName);
        place(fve, muzzle, ax / perUnit, az, h.flashRoll,
            perUnit / weaponScale * za::max(weapons::value(slot, Key::FlashSize), 0.f) * feel.flash * h.flashJitter);
    }
    model = view::viewModel(flashModelName);

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
        if(f.slot >= 0)
        {
            // A held weapon's: its own flash (size, time, an AD gun's colour and feel) at its Muzzle anchor, as yours.
            const entity_t* e = f.ent > 0 && f.ent < cl.num_entities ? &cl_entities[f.ent] : nullptr;
            const adguns::FireFeel feel = f.variant >= 0 ? adguns::fireFeel(f.variant) : adguns::FireFeel{};
            if(!e || !vr_muzzle_flash.value || !vr_muzzle_flash_enemies.value || heldPropSlot(*e) != f.slot || age < 0.f ||
                age >= za::max(weapons::value(f.slot, Key::FlashTime) * feel.flashTime, 0.01f))
            {
                f.at = -1.0;
                continue;
            }
            const char* anchor = weapons::anchor(f.slot, Key::MuzzleAnchor);
            const glm::vec3 off = weapons::vec(f.slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ);
            muzzle = view::anchorPosition(*e, false, 1.f, anchor, off);
            const glm::vec3 ax = view::anchorPosition(*e, false, 1.f, anchor, off + glm::vec3{1.f, 0.f, 0.f}) - muzzle;
            const glm::vec3 az = view::anchorPosition(*e, false, 1.f, anchor, off + glm::vec3{0.f, 0.f, 1.f}) - muzzle;
            const float perUnit = glm::length(ax);
            const float weaponScale = weapons::value(f.slot, Key::Scale);
            if(perUnit < 1e-5f || weaponScale <= 0.f)
            {
                continue;
            }
            qmodel_t* own = feel.flashModel ? view::viewModel(feel.flashModel) : nullptr;
            model = own ? own : view::viewModel(flashModelName);
            place(fve, muzzle, ax / perUnit, az, f.roll,
                perUnit / weaponScale * za::max(weapons::value(f.slot, Key::FlashSize), 0.f) * feel.flash);
            model = view::viewModel(flashModelName);
            if(vr_debug_weaponfx.value && !f.told)
            {
                f.told = true;
                Con_Printf("weaponfx prop flash ent %d slot %d at %.1f %.1f %.1f\n", f.ent, f.slot, muzzle.x, muzzle.y, muzzle.z);
            }
            continue;
        }
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

namespace
{

// vr_recoil_test: each weapon's own recoil kick and rise (Key::RecoilKick, RecoilRise) move its recoil only. At their
// defaults every slot's recoil is Weapon Effects' Kick Back and Muzzle Rise times its strength, as before; the held
// weapon's (the main hand's) Kick Back 2 and Muzzle Rise 0.5 double its drawn kick back and halve its tip up and turn
// (recoilOffset, a kick put in its hand), and no other slot's recoil moves (but a slot inheriting its values); another
// slot's own 3 and 3 leave the held weapon's as it was. Every value it changes it puts back. Ends with
// "vr_recoil_test: N passed, M failed".
void recoilTest_f()
{
    int passed = 0, failed = 0;
    const auto check = [&](bool ok, const char* what, float got, float want)
    {
        (ok ? passed : failed)++;
        Con_Printf("vr_recoil_test: %s %s (%.4f, want %.4f)\n", ok ? "PASS" : "FAIL", what, got, want);
    };
    const auto approx = [](float a, float b) { return fabsf(a - b) <= 1e-4f * za::max(1.f, fabsf(b)); };

    const int hand = HAND_MAIN;
    const int held = weapons::heldSlot(hand);
    if(held < 0 || held == weapons::fistSlot() || !vr_weapon_recoil.value)
    {
        Con_Printf("vr_recoil_test: FAIL no weapon held (or vr_weapon_recoil 0): load a map first\n");
        Con_Printf("vr_recoil_test: 0 passed, 1 failed\n");
        return;
    }
    // Another slot, whose values are its own (it inherits none).
    int other = -1;
    for(int s = 0; s < weapons::numSlots && other < 0; s++)
    {
        if(s != held && s != weapons::fistSlot() && weapons::ownerSlot(s, Key::RecoilKick) == s)
        {
            other = s;
        }
    }

    // Every slot's recoil at a kick of 1 (sideways 1), before; the untuned ones' as before the per-weapon values.
    RecoilAmount before[weapons::numSlots];
    int untuned = 0, asBefore = 0;
    for(int s = 0; s < weapons::numSlots; s++)
    {
        before[s] = recoilAmount(s, 1.f, 1.f);
        if(weapons::value(s, Key::RecoilKick) != 1.f || weapons::value(s, Key::RecoilRise) != 1.f)
        {
            continue;
        }
        untuned++;
        if(before[s].backCm == vr_recoil_kick.value && before[s].pitch == -vr_recoil_rise.value &&
            before[s].yaw == 0.25f * vr_recoil_rise.value)
        {
            asBefore++;
        }
    }
    check(untuned > 0 && asBefore == untuned, "every slot at its defaults recoils as before (slots)",
        static_cast<float>(asBefore), static_cast<float>(untuned));

    // A kick in the held weapon's hand, drawn (recoilOffset).
    const auto drawn = [&](glm::vec3& pos, glm::vec3& angles)
    {
        forget(hand);
        HandFx& h = handFx[hand];
        h.slot = held;
        h.kicks[0] = Kick{cl.time - 0.05, 1.f, 0.15f, 0.5f};
        recoilOffset(hand, glm::vec3{0.f}, pos, angles);
        forget(hand);
    };
    glm::vec3 pos0, ang0;
    drawn(pos0, ang0);
    check(glm::length(pos0) > 0.f && ang0.x < 0.f, "the held weapon's kick is drawn (back, units)", glm::length(pos0), 0.f);

    cvar_t* kickVar = weapons::cvar(held, Key::RecoilKick);
    cvar_t* riseVar = weapons::cvar(held, Key::RecoilRise);
    const za::String oldKick{kickVar->string};
    const za::String oldRise{riseVar->string};
    Cvar_SetValueQuick(kickVar, 2.f * weapons::value(held, Key::RecoilKick));
    Cvar_SetValueQuick(riseVar, 0.5f * weapons::value(held, Key::RecoilRise));

    glm::vec3 pos1, ang1;
    drawn(pos1, ang1);
    check(approx(glm::length(pos1), 2.f * glm::length(pos0)), "held weapon's Kick Back 2x doubles its kick back",
        glm::length(pos1), 2.f * glm::length(pos0));
    check(approx(ang1.x, 0.5f * ang0.x), "its Muzzle Rise 0.5x halves its tip up", ang1.x, 0.5f * ang0.x);
    check(approx(ang1.y, 0.5f * ang0.y), "and halves its turn", ang1.y, 0.5f * ang0.y);

    int othersMoved = 0, inheritors = 0;
    for(int s = 0; s < weapons::numSlots; s++)
    {
        const RecoilAmount a = recoilAmount(s, 1.f, 1.f);
        if(s == held || weapons::ownerSlot(s, Key::RecoilKick) == held)
        {
            inheritors += s != held ? 1 : 0;
            continue;
        }
        if(a.backCm != before[s].backCm || a.pitch != before[s].pitch || a.yaw != before[s].yaw)
        {
            othersMoved++;
        }
    }
    check(othersMoved == 0, "no other slot's recoil moved (slots moved)", static_cast<float>(othersMoved), 0.f);
    Con_Printf("vr_recoil_test: held slot %d, other slot %d, %d slots inheriting the held one's\n", held, other, inheritors);
    Cvar_SetQuick(kickVar, oldKick.cStr());
    Cvar_SetQuick(riseVar, oldRise.cStr());

    if(other >= 0)
    {
        cvar_t* otherKick = weapons::cvar(other, Key::RecoilKick);
        cvar_t* otherRise = weapons::cvar(other, Key::RecoilRise);
        const za::String oldOtherKick{otherKick->string};
        const za::String oldOtherRise{otherRise->string};
        Cvar_SetValueQuick(otherKick, 3.f);
        Cvar_SetValueQuick(otherRise, 3.f);
        glm::vec3 pos2, ang2;
        drawn(pos2, ang2);
        check(glm::length(pos2) == glm::length(pos0) && ang2.x == ang0.x && ang2.y == ang0.y,
            "another slot's own 3x leaves the held weapon's kick back", glm::length(pos2), glm::length(pos0));
        check(approx(recoilAmount(other, 1.f, 1.f).backCm, 3.f * before[other].backCm), "and moves that slot's own",
            recoilAmount(other, 1.f, 1.f).backCm, 3.f * before[other].backCm);
        Cvar_SetQuick(otherKick, oldOtherKick.cStr());
        Cvar_SetQuick(otherRise, oldOtherRise.cStr());
    }
    else
    {
        check(false, "another slot to tune", -1.f, 0.f);
    }
    glm::vec3 pos3, ang3;
    drawn(pos3, ang3);
    check(glm::length(pos3) == glm::length(pos0) && ang3.x == ang0.x, "values put back: the held weapon's kick as before",
        glm::length(pos3), glm::length(pos0));
    Con_Printf("vr_recoil_test: %d passed, %d failed\n", passed, failed);
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_weaponfx_test", test_f);
    Cmd_AddCommand("vr_recoil_test", recoilTest_f);
}

} // namespace qvr::weaponfx
