// vr_lootbeams.cpp -- see vr_lootbeams.hpp.

#include "vr_lootbeams.hpp"
#include "vr_box3d.hpp"
#include "vr_client.hpp"
#include "vr_color.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_mem.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_portals.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include <string.h>

namespace qvr::lootbeams
{
namespace
{

enum class Category : int
{
    None = -1,
    Weapons,
    Mags,
    Ammo,
    Health,
    Armor,
    Powerups,
    Keys,
    Backpacks,
    Count
};
constexpr int categoryCount = static_cast<int>(Category::Count);
constexpr const char* categoryNames[categoryCount] = {"weapons", "mags", "ammo", "health", "armor", "powerups", "keys",
    "backpacks"};
cvar_t* const categoryOn[categoryCount] = {&vr_lootbeam_weapons, &vr_lootbeam_mags, &vr_lootbeam_ammo,
    &vr_lootbeam_health, &vr_lootbeam_armor, &vr_lootbeam_powerups, &vr_lootbeam_keys, &vr_lootbeam_backpacks};
cvar_t* const categoryHue[categoryCount] = {&vr_lootbeam_hue_weapons, &vr_lootbeam_hue_mags, &vr_lootbeam_hue_ammo,
    &vr_lootbeam_hue_health, &vr_lootbeam_hue_armor, &vr_lootbeam_hue_powerups, &vr_lootbeam_hue_keys,
    &vr_lootbeam_hue_backpacks};

// The power-ups' and runes' models (id's, the mission packs'), by a part of their names.
constexpr const char* powerupNames[] = {"/quaddama.mdl", "/invulner.mdl", "/invisibl.mdl", "/suit.mdl", "/empathy.mdl",
    "/wetsuit.mdl", "/horn.mdl", "/sphere.mdl"};
constexpr const char* runeNames[] = {"/end1.mdl", "/end2.mdl", "/end3.mdl", "/end4.mdl"};

constexpr float moveSlack = 4.f;      // units a thing may drift (a rigid body settling, a bob) and still be at rest
constexpr float fadeOutTime = 0.1f;   // seconds its beam takes to go as it is picked up and moves
constexpr double forgetAfter = 0.5;   // seconds out of sight after which a thing is seen afresh (its beam fades in again)

// A visible entity's rest (by entity number).
struct Track
{
    const qmodel_t* model{nullptr};
    glm::vec3 anchor{0.f}; // where it last came to (it moved: farther than moveSlack from here)
    double movedAt{0.0};   // when (cl.time)
    double seen{-1.0};     // when last seen
    float alpha{0.f};      // its beam's fade (0..1)
};

// This frame's beams.
struct Beam
{
    glm::vec3 base;  // the thing's middle
    glm::vec3 color;
    float alpha;     // its fade (0..1)
    int num;         // its entity (vr_lootbeams_report)
    Category category;
};

struct LootBeamScratch
{
    za::Vector<Track> tracks;          // by entity number
    za::Vector<Beam> beams;            // this frame's (update)
    za::Vector<gfx::Vertex> triangles; // a view's (drawTranslucent)
    auto members() { return mem::list(tracks, beams, triangles); }
};
mem::Scratch<LootBeamScratch> scratch{"lootbeams"};

// The frame's counts (vr_lootbeam_debug, vr_lootbeams_report).
struct Counts
{
    int drawn[categoryCount]{};
    int carried{0};
    int moving{0};
    int emptyMags{0};
};

struct State
{
    int builtFrame{-1};     // host_framecount of the last update
    double lastTime{-1.0};  // its cl.time
    double printedAt{-1.0}; // vr_lootbeam_debug's last line (realtime)
    Counts counts;
};
State state;

[[nodiscard]] bool nameHasAny(const char* name, const char* const* parts, int count)
{
    for(int i = 0; i < count; i++)
    {
        if(strstr(name, parts[i]))
        {
            return true;
        }
    }
    return false;
}

// What `e` is, as loot (None: not loot).
[[nodiscard]] Category classify(const entity_t& e, const client::EntityVr* net)
{
    using T = modelmeta::Trait;
    const qmodel_t* m = e.model;
    if(net && (net->weaponUid != 0 || net->spin))
    {
        return Category::Weapons; // a weapon prop (dropped, thrown, a map's drawn as its prop: any weapon, AD's too)
    }
    const modelmeta::ModelMetadata& meta = modelmeta::get(m);
    if(m->type == mod_brush)
    {
        if(meta.has(T::HealthBox))
        {
            return Category::Health;
        }
        if(meta.has(T::AmmoBox) && !meta.has(T::ExplosiveBox) && !meta.has(T::ExplosiveCrate))
        {
            return Category::Ammo;
        }
        return Category::None;
    }
    if(m->type != mod_alias || meta.has(T::Gib) || meta.has(T::Head) || meta.has(T::Ragdoll) || meta.has(T::Limb) ||
       meta.has(T::Body))
    {
        return Category::None;
    }
    if(meta.has(T::Magazine))
    {
        return Category::Mags;
    }
    if(meta.has(T::WorldWeapon))
    {
        return Category::Weapons; // a weapon pickup's own g_ model
    }
    if(meta.has(T::ContainsBackpack))
    {
        return Category::Backpacks;
    }
    if(meta.has(T::ContainsArmor))
    {
        return Category::Armor;
    }
    if(meta.has(T::ContainsKey) || nameHasAny(m->name, runeNames, static_cast<int>(sizeof(runeNames) / sizeof(runeNames[0]))))
    {
        return Category::Keys;
    }
    if(nameHasAny(m->name, powerupNames, static_cast<int>(sizeof(powerupNames) / sizeof(powerupNames[0]))))
    {
        return Category::Powerups;
    }
    return Category::None;
}

// The middle of `e`'s model's bounds, turned with it.
[[nodiscard]] glm::vec3 middleOf(const entity_t& e)
{
    const glm::vec3 local = (glm::vec3{e.model->mins[0], e.model->mins[1], e.model->mins[2]} +
                                glm::vec3{e.model->maxs[0], e.model->maxs[1], e.model->maxs[2]}) * 0.5f;
    vec3_t angles = {e.angles[0], e.angles[1], e.angles[2]};
    vec3_t f, r, u;
    AngleVectors(angles, f, r, u);
    return glm::vec3{e.origin[0], e.origin[1], e.origin[2]} + glm::vec3{f[0], f[1], f[2]} * local.x -
           glm::vec3{r[0], r[1], r[2]} * local.y + glm::vec3{u[0], u[1], u[2]} * local.z;
}

// Once a frame: the things to pick up in view, whether each rests (its fade).
void update()
{
    LootBeamScratch& s = scratch;
    s.beams.clear();
    state.counts = Counts{};
    const double now = cl.time;
    if(now < state.lastTime)
    {
        s.tracks.clear(); // a new map, a loaded game
    }
    const float dt = static_cast<float>(za::clamp(now - za::max(state.lastTime, 0.0), 0.0, 0.1));
    state.lastTime = now;
    if(vr_lootbeams.value == 0.f || cls.state != ca_connected || cl_max_edicts <= 0)
    {
        return;
    }
    QVR_PROFILE("loot beams");
    if(static_cast<int>(s.tracks.size()) < cl_max_edicts)
    {
        s.tracks.resize(static_cast<za::SizeT>(cl_max_edicts));
    }
    const float settle = za::max(vr_lootbeam_settle.value, 0.f);
    const float fadeIn = za::max(vr_lootbeam_fade_in.value, 0.01f);
    const float magMin = za::max(vr_lootbeam_mag_min.value, 0.f);
    const float saturation = za::clamp(vr_lootbeam_saturation.value, 0.f, 1.f);
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e < cl_entities || e >= cl_entities + cl_max_edicts)
        {
            continue;
        }
        const int num = static_cast<int>(e - cl_entities);
        if(num <= cl.maxclients || num == cl.viewentity || view::find(e))
        {
            continue; // a player, or drawn by the view (in a hand, a holster)
        }
        const client::EntityVr* net = client::entityVr(num);
        const Category c = classify(*e, net);
        if(c == Category::None || categoryOn[static_cast<int>(c)]->value == 0.f)
        {
            continue;
        }
        Track& t = s.tracks[static_cast<za::SizeT>(num)];
        const glm::vec3 origin{e->origin[0], e->origin[1], e->origin[2]};
        if(t.model != e->model || t.seen < 0.0 || now - t.seen > forgetAfter)
        {
            // Seen afresh (come into view, or a new thing): at rest unless it moves, fading in.
            t = Track{e->model, origin, now - static_cast<double>(settle) - 1.0, now, 0.f};
        }
        t.seen = now;
        if(glm::distance(origin, t.anchor) > moveSlack)
        {
            t.anchor = origin;
            t.movedAt = now;
        }
        bool show = true;
        if(net && net->loot >= 0 && (net->loot & 0x80))
        {
            show = false; // carried in a hand
            state.counts.carried++;
        }
        if(show && c == Category::Mags && net && net->loot >= 0 && (net->loot & 127) != 127 &&
           static_cast<float>(net->loot & 127) / 100.f <= magMin)
        {
            show = false; // empty, or nearly
            state.counts.emptyMags++;
        }
        const bool rested = now - t.movedAt >= static_cast<double>(settle);
        if(show && !rested)
        {
            state.counts.moving++;
        }
        const float target = show && rested ? 1.f : 0.f;
        t.alpha = t.alpha < target ? za::min(target, t.alpha + dt / fadeIn) : za::max(target, t.alpha - dt / fadeOutTime);
        if(t.alpha <= 0.001f)
        {
            continue;
        }
        const float hue = categoryHue[static_cast<int>(c)]->value;
        s.beams.pushBack(Beam{middleOf(*e), hsv(hue, saturation, 1.f), t.alpha, num, c});
        state.counts.drawn[static_cast<int>(c)]++;
    }

    if(vr_lootbeam_debug.value != 0.f && realtime >= state.printedAt + 1.0)
    {
        state.printedAt = realtime;
        Con_Printf("lootbeams: %d drawn (weapons %d mags %d ammo %d health %d armor %d powerups %d keys %d backpacks %d), "
                   "carried %d, moving %d, empty mags %d\n",
            static_cast<int>(s.beams.size()), state.counts.drawn[0], state.counts.drawn[1], state.counts.drawn[2],
            state.counts.drawn[3], state.counts.drawn[4], state.counts.drawn[5], state.counts.drawn[6],
            state.counts.drawn[7], state.counts.carried, state.counts.moving, state.counts.emptyMags);
    }
}

void pushQuad(za::Vector<gfx::Vertex>& out, const glm::vec3 (&p)[4], const glm::vec2 (&uv)[4], const glm::vec4 (&c)[4])
{
    for(const int i : {0, 1, 2, 0, 2, 3})
    {
        out.pushBack({p[i], uv[i], c[i]});
    }
}

// A beam's vertices for the view at `eye` (camera `right`, `up`): two shells turned towards the eye about the vertical
// (the core and a glow three times as wide and dimmer), each fading in from just under the thing's middle, brightest a
// little above it, then fading out up its length; and a flare at its base.
void appendBeam(za::Vector<gfx::Vertex>& out, const Beam& b, const glm::vec3& eye, const glm::vec3& right,
    const glm::vec3& up, float length, float half, float opacity, float m2u)
{
    glm::vec3 toEye = eye - b.base;
    toEye.z = 0.f;
    const float across = glm::length(toEye);
    float fade = b.alpha * opacity;
    const float nearFade = za::max(vr_lootbeam_near.value, 0.f) * m2u;
    if(nearFade > 0.f)
    {
        fade *= za::clamp((across - nearFade * 0.5f) / (nearFade * 0.5f), 0.f, 1.f);
    }
    const float farFade = za::max(vr_lootbeam_far.value, 0.f) * m2u;
    if(farFade > 0.f)
    {
        fade *= za::clamp((farFade - glm::distance(eye, b.base)) / (farFade * 0.25f), 0.f, 1.f);
    }
    if(fade <= 0.002f)
    {
        return;
    }
    const glm::vec3 side = across > 0.01f ? glm::vec3{-toEye.y, toEye.x, 0.f} / across : right;

    // The rows up the beam: their heights (shares of its length) and brightness.
    constexpr float rowAt[4] = {-0.04f, 0.06f, 0.4f, 1.f};
    constexpr float rowLight[4] = {0.f, 1.f, 0.35f, 0.f};
    constexpr float shellWidth[2] = {1.f, 3.f};
    constexpr float shellLight[2] = {1.f, 0.35f};
    for(int shell = 0; shell < 2; shell++)
    {
        const glm::vec3 w = side * (half * shellWidth[shell]);
        for(int row = 0; row < 3; row++)
        {
            const glm::vec3 lo = b.base + glm::vec3{0.f, 0.f, rowAt[row] * length};
            const glm::vec3 hi = b.base + glm::vec3{0.f, 0.f, rowAt[row + 1] * length};
            const glm::vec4 cl0{b.color, rowLight[row] * shellLight[shell] * fade};
            const glm::vec4 cl1{b.color, rowLight[row + 1] * shellLight[shell] * fade};
            const glm::vec3 p[4]{lo - w, hi - w, hi + w, lo + w};
            const glm::vec2 uv[4]{{-1.f, 0.f}, {-1.f, 0.f}, {1.f, 0.f}, {1.f, 0.f}};
            const glm::vec4 c[4]{cl0, cl1, cl1, cl0};
            pushQuad(out, p, uv, c);
        }
    }
    // The flare: a soft disc facing the eye, round the thing's middle (mostly behind it: a glow round its edges).
    const float r = half * 4.f;
    const glm::vec3 rx = right * r;
    const glm::vec3 ry = up * r;
    const glm::vec4 fc{b.color, 0.6f * fade};
    const glm::vec3 p[4]{b.base - rx - ry, b.base - rx + ry, b.base + rx + ry, b.base + rx - ry};
    const glm::vec2 uv[4]{{-1.f, -1.f}, {-1.f, 1.f}, {1.f, 1.f}, {1.f, -1.f}};
    const glm::vec4 c[4]{fc, fc, fc, fc};
    pushQuad(out, p, uv, c);
}

// vr_lootbeams_report: this frame's beams (entity, category, model, fade, where) and the counts.
void report_f()
{
    const LootBeamScratch& s = scratch;
    for(const Beam& b : s.beams)
    {
        const entity_t& e = cl_entities[b.num];
        const client::EntityVr* net = client::entityVr(b.num);
        // (A local game's edict: who carries it, as the server sees it.)
        int carrier = -1;
        if(sv.active && b.num < sv.qcvm.num_edicts && progs::fields().carry_player >= 0)
        {
            qcvm_t* old = nullptr;
            PR_PushQCVM(&sv.qcvm, &old);
            carrier = progs::fieldInt(EDICT_NUM(b.num), progs::fields().carry_player);
            carrier = carrier > 0 ? NUM_FOR_EDICT(PROG_TO_EDICT(carrier)) : 0;
            PR_PopQCVM(old);
        }
        Con_Printf("lootbeam %d %s %s fade %.2f at %.0f %.0f %.0f (server's byte %d, carried by %d)\n", b.num,
            categoryNames[static_cast<int>(b.category)], e.model ? e.model->name : "?", static_cast<double>(b.alpha),
            static_cast<double>(b.base.x), static_cast<double>(b.base.y), static_cast<double>(b.base.z),
            net ? net->loot : -1, carrier);
    }
    const Counts& n = state.counts;
    Con_Printf("lootbeams: %d drawn (weapons %d mags %d ammo %d health %d armor %d powerups %d keys %d backpacks %d), "
               "carried %d, moving %d, empty mags %d\n",
        static_cast<int>(s.beams.size()), n.drawn[0], n.drawn[1], n.drawn[2], n.drawn[3], n.drawn[4], n.drawn[5],
        n.drawn[6], n.drawn[7], n.carried, n.moving, n.emptyMags);
}

// What vr_lootbeam_test lays out, in turn: weapon props, magazines (full, nearly empty), boxes, armour, a power-up.
struct TestThing
{
    const char* classname; // nullptr: a magazine (QC VR_Reload_MakeRound)
    const char* key;
    float value; // the key's (a magazine: its rounds of the nailgun's 24)
    float lift;
};
constexpr TestThing testThings[] = {
    {"func_weapon_grabbable", "weapon", 4.f, 8.f},  // shotgun
    {nullptr, nullptr, 24.f, 4.f},                  // a full nailgun magazine
    {"func_weapon_grabbable", "weapon", 6.f, 8.f},  // nailgun
    {"item_shells", nullptr, 0.f, 0.f},
    {"func_weapon_grabbable", "weapon", 10.f, 8.f}, // rocket launcher
    {nullptr, nullptr, 2.f, 4.f},                   // a nearly empty one (no beam at vr_lootbeam_mag_min 0.25)
    {"item_health", nullptr, 0.f, 0.f},
    {"func_weapon_grabbable", "weapon", 7.f, 8.f},  // super nailgun
    {"item_armor1", nullptr, 0.f, 0.f},
    {"func_weapon_grabbable", "weapon", 11.f, 8.f}, // thunderbolt
    {"item_artifact_super_damage", nullptr, 0.f, 24.f},
    {"func_weapon_grabbable", "weapon", 5.f, 8.f},  // super shotgun
};

// vr_lootbeam_test [count] [gibs]: `count` things to pick up (12) laid out in rows ahead of the player, 40 units apart,
// in turn weapon props, nailgun magazines (full, nearly empty), ammo, health, armour, quad damage; with `gibs` (0..),
// that many gibs thrown among them (the look among the gore; the cost with Performance Profile).
void test_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("vr_lootbeam_test: no game running\n");
        return;
    }
    const int count = za::clamp(Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 12, 1, 256);
    const int gibs = za::clamp(Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 0, 0, 256);
    glm::vec3 feet{0.f};
    float yaw = 0.f;
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    edict_t* player = EDICT_NUM(1);
    feet = glm::vec3{player->v.origin[0], player->v.origin[1], player->v.origin[2] + player->v.mins[2]};
    yaw = player->v.angles[1];
    PR_PopQCVM(old);
    const float a = glm::radians(yaw);
    const glm::vec3 fwd{za::cos(a), za::sin(a), 0.f};
    const glm::vec3 side{-fwd.y, fwd.x, 0.f};
    constexpr int perRow = 8;
    constexpr float spacing = 40.f;
    int made = 0, mags = 0;
    for(int i = 0; i < count; i++)
    {
        const TestThing& thing = testThings[i % static_cast<int>(sizeof(testThings) / sizeof(testThings[0]))];
        const int row = i / perRow;
        const int col = i % perRow;
        const glm::vec3 at = feet + fwd * (96.f + spacing * static_cast<float>(row)) +
                             side * (spacing * (static_cast<float>(col) - (perRow - 1) * 0.5f)) +
                             glm::vec3{0.f, 0.f, thing.lift + 1.f};
        const glm::vec3 angles{0.f, yaw + 180.f + 37.f * static_cast<float>(i), 0.f};
        if(thing.classname)
        {
            made += box3d::spawnClass(thing.classname, at, angles, nullptr, thing.key, thing.value) != nullptr;
            continue;
        }
        PR_PushQCVM(&sv.qcvm, &old);
        const func_t fn = progs::findFunction("VR_Reload_MakeRound");
        if(fn > 0)
        {
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(EDICT_NUM(1));
            G_FLOAT(OFS_PARM0) = 6.f; // WID_NAILGUN
            G_FLOAT(OFS_PARM1) = 2.f; // AID_NAILS
            G_FLOAT(OFS_PARM2) = thing.value;
            G_VECTOR(OFS_PARM3)[0] = at.x;
            G_VECTOR(OFS_PARM3)[1] = at.y;
            G_VECTOR(OFS_PARM3)[2] = at.z;
            G_VECTOR(OFS_PARM4)[0] = 0.f;
            G_VECTOR(OFS_PARM4)[1] = angles.y;
            G_VECTOR(OFS_PARM4)[2] = 90.f;
            PR_ExecuteProgram(static_cast<func_t>(fn));
            made += PROG_TO_EDICT(G_INT(OFS_RETURN)) != qcvm->edicts;
            mags++;
        }
        PR_PopQCVM(old);
    }
    int thrown = 0;
    if(gibs > 0)
    {
        PR_PushQCVM(&sv.qcvm, &old);
        const func_t fn = progs::findFunction("ThrowGib");
        edict_t* from = fn > 0 ? ED_Alloc() : nullptr;
        if(from)
        {
            const char* const gibModels[] = {"progs/gib1.mdl", "progs/gib2.mdl", "progs/gib3.mdl"};
            for(int g = 0; g < gibs; g++)
            {
                const int row = g % za::max(1, (count + perRow - 1) / perRow);
                const glm::vec3 at = feet + fwd * (96.f + spacing * static_cast<float>(row)) +
                                     side * (spacing * (static_cast<float>(g % perRow) - (perRow - 1) * 0.5f + 0.5f)) +
                                     glm::vec3{0.f, 0.f, 16.f};
                from->v.origin[0] = at.x;
                from->v.origin[1] = at.y;
                from->v.origin[2] = at.z;
                pr_global_struct->time = qcvm->time;
                pr_global_struct->self = EDICT_TO_PROG(from);
                G_INT(OFS_PARM0) = PR_SetEngineString(gibModels[g % 3]);
                G_FLOAT(OFS_PARM1) = -20.f;
                PR_ExecuteProgram(static_cast<func_t>(fn));
                thrown++;
            }
            ED_Free(from);
        }
        PR_PopQCVM(old);
    }
    Con_Printf("lootbeam test: %d of %d made (%d magazines), %d gibs thrown\n", made, count, mags, thrown);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_lootbeams_report", report_f);
    Cmd_AddCommand("vr_lootbeam_test", test_f);
}

void drawTranslucent()
{
    if(state.builtFrame != host_framecount)
    {
        state.builtFrame = host_framecount;
        update();
    }
    LootBeamScratch& s = scratch;
    if(s.beams.empty() || vr_lootbeams.value == 0.f || portals::viewing())
    {
        return;
    }
    QVR_GPU_PROFILE("loot beams");
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float m2u = units::metresToUnits();
    const float length = za::max(vr_lootbeam_length.value, 0.f) * m2u;
    const float half = za::max(vr_lootbeam_thickness.value, 0.f) * m2u * 0.5f;
    const float opacity = za::clamp(vr_lootbeam_opacity.value, 0.f, 1.f);
    if(length <= 0.f || half <= 0.f || opacity <= 0.f)
    {
        return;
    }
    za::Vector<gfx::Vertex>& out = s.triangles;
    out.clear();
    out.reserve(s.beams.size() * 42);
    for(const Beam& b : s.beams)
    {
        appendBeam(out, b, eye, right, up, length, half, opacity, m2u);
    }
    if(out.empty())
    {
        return;
    }
    gfx::draw(out, gfx::sceneViewProjection(),
        gfx::State{.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Additive, .depthTest = true, .depthWrite = false});
}

} // namespace qvr::lootbeams
