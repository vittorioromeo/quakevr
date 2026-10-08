// vr_explosiondebris.cpp -- an explosion's incandescent chunks (docs/vr-port/ROUND21.md, "Explosion debris as Box3D
// bodies"; until 2026-10-08 they were the client's own models, moved by seven line traces each a step).
//
// The server's: each explosion QuakeC broadcasts (its temp entity TE_EXPLOSION, TE_EXPLOSION2 or TE_TAREXPLOSION, seen
// whole by vr_server.cpp's VR_BroadcastWritten: noteBroadcast) launches vr_explosion_debris_count chunks at the server
// frame's end (serverFrame), as rubble's pieces are (vr_debris.cpp): entities of progs/vr_explosion_debris.mdl, .vr_rigid
// props that Box3D moves with the others (vr_box3d.cpp, isChunk: a small sphere of the chunk's size, a few grams
// (.vr_prop_mass), continuous collision while fast, asleep at rest). So they ride lifts and doors, later blasts throw
// them, they knock props without moving them much (their mass), and they meet neither the hands nor what the hands
// hold; they make no sounds, touches or impacts (catChunk). The launch is the client's was: a random direction biased
// up (vr_explosion_debris_up), speed, size and life between their settings' ends, a spin. A chunk's size is its
// entity's scale (a byte on the wire, as the client's drawn scale was), its fade its alpha over its last half second;
// then it goes (and SUB_Remove at its end, should the list here lose it: a saved game's). At most vr_explosion_debris_max
// at once, the oldest retired first; in multiplayer at most vr_explosion_debris_mp_max (-1: as single player, 0: none),
// as rubble's vr_debris_mp_max: each chunk in sight costs every remote client's 1400-byte datagram about 18 bytes a
// frame while it moves (MULTIPLAYER.md, "Explosion debris"). Never when fewer than edictReserve entities would be left.
//
// The client's: the chunks are entities, drawn and interpolated as any; their fire trail is drawn each frame from where
// the chunk was drawn last to where it is drawn now (VR_ExplosionDebrisTrail from CL_RelinkEntities: its trailorg), the
// nearest ones glow (vr_explosion_debris_lights). How hot one is comes from its age and the life settings' middle (the
// client doesn't know its life), and from its alpha once it fades.
//
// A future option (not done; PERF_DECISIONS.md item 3): a separate, client-only Box3D world for purely visual physics
// (these chunks, spent shell casings, sparks, small gore), built from cl.worldmodel (in single player its mesh shared
// read-only with the server world's, which builds the same one), with kinematic proxies for the brush entities (doors,
// lifts: their client entities' interpolated places) and for the server's props near the view. Effects there need no
// networking at all (no entity slots, no datagram bytes, no cap in multiplayer) and behave the same in single player and
// multiplayer; the price is a second broadphase and step on the client, and props that don't feel them (one-way).

#include "vr_explosiondebris.hpp"
#include <string.h>
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_lighting.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_particles.hpp"
#include "vr_progs.hpp"
#include "vr_units.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"

namespace qvr::explosiondebris
{
namespace
{
constexpr int capacity = 256;
constexpr int lightKey = -19000;
constexpr const char* modelName = "progs/vr_explosion_debris.mdl";
constexpr const char* className = "vr_explosion_debris";
constexpr float chunkMass = 0.02f;  // kg: a pebble's; a 6 kg box a chunk hits at 9 m/s takes 3 cm/s
constexpr int edictReserve = 512;   // entities always left to the game (ED_Alloc's "no free edicts" is fatal)
constexpr int maxPending = 32;      // explosions a frame (more: dropped)
constexpr float fadeMost = 0.5f;    // s: the fade, at most (a third of a short life)

struct VmScope
{
    qcvm_t* old{nullptr};
    VmScope() { PR_PushQCVM(&sv.qcvm, &old); }
    ~VmScope() { PR_PopQCVM(old); }
};

float lifeMin() { return za::clamp(vr_explosion_debris_life_min.value, 0.1f, 30.f); }
float lifeMax() { return za::clamp(vr_explosion_debris_life_max.value, 0.1f, 30.f); }

// ---------------------------------------------------------------------------------------------------------------------
// The server's chunks

struct Live
{
    int num{0};
    float serial{0.f}; // its .vr_xdebris (an entry whose entity no longer has it is gone)
    double born{0.0}, die{0.0};
};

struct Server
{
    Live live[capacity]; // the oldest first
    int count{0};
    glm::vec3 pending[maxPending];
    int pendingCount{0};
    bool rescan{true}; // a new map or a loaded game: its chunks (a save's) found again at the next frame
    float serial{0.f};
    int modelIndex{-1}; // the model's precache index (0: not precached; -1: not looked for yet)
    func_t subRemove{-1};
    za::FastNonCryptoRng rng{1};
    int made{0}, expired{0}, evicted{0}, blocked{0}, noRoom{0};
    float minSpeed{1e30f}, maxSpeed{0.f}, minLife{1e30f}, maxLife{0.f};
};
Server server;

float rnd(float lo, float hi) { return server.rng.getF(lo, hi); }

[[nodiscard]] int limit()
{
    if(!vr_explosion_debris.value)
    {
        return 0;
    }
    int most = static_cast<int>(za::clamp(vr_explosion_debris_max.value, 0.f, float(capacity)));
    if(svs.maxclients > 1 && vr_explosion_debris_mp_max.value >= 0.f) // (below 0: as in single player)
    {
        most = za::min(most, static_cast<int>(vr_explosion_debris_mp_max.value)); // (each in sight costs every client)
    }
    return most;
}

[[nodiscard]] bool usable()
{
    const progs::FieldOffsets& f = progs::fields();
    return sv.active && progs::bindings().isVrProgs && f.vr_xdebris >= 0 && f.vr_rigid >= 0 && f.vr_prop_mass >= 0;
}

[[nodiscard]] bool valid(const Live& c)
{
    if(c.num <= svs.maxclients || c.num >= qcvm->num_edicts)
    {
        return false;
    }
    edict_t* e = EDICT_NUM(c.num);
    return !e->free && progs::fieldFloat(e, progs::fields().vr_xdebris) == c.serial;
}

// Removes the entries `drop` says (freeing their entities if `free`), the order kept.
template <typename F>
int removeIf(F&& drop, bool free)
{
    Server& s = server;
    int kept = 0, gone = 0;
    for(int i = 0; i < s.count; i++)
    {
        if(drop(s.live[i]))
        {
            if(free && valid(s.live[i]))
            {
                ED_Free(EDICT_NUM(s.live[i].num));
            }
            gone++;
            continue;
        }
        s.live[kept++] = s.live[i];
    }
    s.count = kept;
    return gone;
}

void retireOldest()
{
    Server& s = server;
    if(s.count == 0)
    {
        return;
    }
    if(valid(s.live[0]))
    {
        ED_Free(EDICT_NUM(s.live[0].num));
    }
    for(int i = 1; i < s.count; i++)
    {
        s.live[i - 1] = s.live[i];
    }
    s.count--;
    s.evicted++;
}

// A loaded game's chunks (or a list lost): every entity with a serial, the oldest first.
void rescan()
{
    Server& s = server;
    s.count = 0;
    const int field = progs::fields().vr_xdebris;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts && s.count < capacity; num++)
    {
        edict_t* e = EDICT_NUM(num);
        const float serial = e->free ? 0.f : progs::fieldFloat(e, field);
        if(serial <= 0.f)
        {
            continue;
        }
        Live c;
        c.num = num;
        c.serial = serial;
        c.die = e->v.nextthink > 0.f ? e->v.nextthink - 1.0 : qcvm->time + 1.0; // (make's think, a second after its end)
        c.born = c.die - 0.5 * (lifeMin() + lifeMax());
        int at = s.count++;
        while(at > 0 && s.live[at - 1].serial > serial)
        {
            s.live[at] = s.live[at - 1];
            at--;
        }
        s.live[at] = c;
        s.serial = za::max(s.serial, serial);
    }
}

[[nodiscard]] int modelIndex()
{
    Server& s = server;
    if(s.modelIndex < 0)
    {
        s.modelIndex = 0;
        for(int i = 1; i < MAX_MODELS && sv.model_precache[i]; i++)
        {
            if(!strcmp(sv.model_precache[i], modelName))
            {
                s.modelIndex = i;
                break;
            }
        }
        if(!s.modelIndex)
        {
            Con_DPrintf("explosiondebris: %s not precached: no chunks\n", modelName);
        }
    }
    return s.modelIndex;
}

glm::vec3 direction()
{
    const float z = rnd(-1.f, 1.f), angle = rnd(0.f, 6.2831853f);
    const float r = za::sqrt(za::max(0.f, 1.f - z * z));
    glm::vec3 v{r * za::cos(angle), r * za::sin(angle), z + za::clamp(vr_explosion_debris_up.value, 0.f, 2.f)};
    return glm::length(v) > 1e-4f ? glm::normalize(v) : glm::vec3{0.f, 0.f, 1.f};
}

[[nodiscard]] bool solidAt(const glm::vec3& p)
{
    vec3_t at{p.x, p.y, p.z};
    const trace_t tr = SV_Move(at, vec3_origin, vec3_origin, at, MOVE_NOMONSTERS, nullptr);
    return tr.startsolid || tr.allsolid;
}

// A ball of `radius` at `p` clear of the level (the world and its brush entities): its middle and six points round it.
[[nodiscard]] bool ballFree(const glm::vec3& p, float radius)
{
    constexpr glm::vec3 offsets[] = {{0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for(const glm::vec3& axis : offsets)
    {
        if(solidAt(p + axis * radius))
        {
            return false;
        }
    }
    return true;
}

// One chunk: an entity Box3D takes at its next step (its velocity and spin then). Null without room.
edict_t* make(const glm::vec3& at, const glm::vec3& velocity, const glm::vec3& spin, float size, float life)
{
    Server& s = server;
    const int index = modelIndex();
    if(!index || !sv.models[index] || qcvm->num_edicts >= qcvm->max_edicts - edictReserve)
    {
        s.noRoom++;
        return nullptr;
    }
    if(s.count >= capacity)
    {
        retireOldest();
    }
    if(s.subRemove < 0)
    {
        s.subRemove = progs::findFunction("SUB_Remove");
    }
    const progs::FieldOffsets& f = progs::fields();
    edict_t* e = ED_Alloc();
    e->v.classname = PR_SetEngineString(className);
    e->v.model = PR_SetEngineString(modelName);
    e->v.modelindex = static_cast<float>(index);
    e->v.movetype = MOVETYPE_BOUNCE;
    e->v.solid = SOLID_NOT;
    for(int k = 0; k < 3; k++)
    {
        e->v.origin[k] = at[k];
        e->v.velocity[k] = velocity[k];
        e->v.angles[k] = rnd(0.f, 360.f);
        e->v.mins[k] = -0.5f * size;
        e->v.maxs[k] = 0.5f * size;
        e->v.size[k] = size;
    }
    // Its drawn size: the model's longest side scaled to it, rounded to the wire's sixteenths (ENTSCALE_ENCODE truncates).
    const qmodel_t* m = sv.models[index];
    const float extent = za::max(m->maxs[0] - m->mins[0], m->maxs[1] - m->mins[1], m->maxs[2] - m->mins[2], 1.f);
    const float steps = za::clamp(static_cast<float>(Q_rint(size / extent * ENTSCALE_DEFAULT)), 1.f, 255.f);
    progs::setFieldFloat(e, f.scale, (steps + 0.25f) / ENTSCALE_DEFAULT);
    progs::setFieldFloat(e, f.alpha, 0.f); // (0: opaque, as the baseline: nothing sent until it fades)
    progs::setFieldFloat(e, f.vr_rigid, 1.f);
    progs::setFieldFloat(e, f.vr_prop_mass, chunkMass);
    progs::setFieldVec(e, f.vr_spin, spin);
    s.serial += 1.f;
    progs::setFieldFloat(e, f.vr_xdebris, s.serial);
    if(s.subRemove > 0)
    {
        e->v.think = s.subRemove; // (a second after its end: serverFrame ends it, faded; this only if it was lost)
        e->v.nextthink = static_cast<float>(qcvm->time + life + 1.0);
    }
    SV_LinkEdict(e, false);

    Live& c = s.live[s.count++];
    c.num = NUM_FOR_EDICT(e);
    c.serial = s.serial;
    c.born = qcvm->time;
    c.die = qcvm->time + life;
    s.made++;
    return e;
}

// An explosion's chunks, at `org`.
void launch(const glm::vec3& org)
{
    Server& s = server;
    const int most = limit();
    const int count = za::min(static_cast<int>(za::clamp(vr_explosion_debris_count.value, 0.f, 64.f)), most);
    const float a = za::clamp(vr_explosion_debris_speed_min.value, 0.f, 40.f);
    const float b = za::clamp(vr_explosion_debris_speed_max.value, 0.f, 40.f);
    const float lo = za::min(a, b), hi = za::max(a, b);
    const float la = lifeMin(), lb = lifeMax();
    const float sa = za::clamp(vr_explosion_debris_size_min.value, 0.8f, 8.f);
    const float sb = za::clamp(vr_explosion_debris_size_max.value, 0.8f, 8.f);
    constexpr float degrees = 3.14159265f / 180.f;
    for(int n = 0; n < count; n++)
    {
        const float size = rnd(za::min(sa, sb), za::max(sa, sb)) * units::worldScale();
        const glm::vec3 dir = direction();
        // A blast exactly at a wall or floor: out along its way to a free launch point, or not this chunk (as before).
        glm::vec3 at = org;
        bool free = false;
        for(int attempt = 0; attempt < 6; ++attempt)
        {
            const glm::vec3 candidate = org + dir * (size + attempt * 2.f * units::worldScale());
            vec3_t from{org.x, org.y, org.z}, to{candidate.x, candidate.y, candidate.z};
            const trace_t route = SV_Move(from, vec3_origin, vec3_origin, to, MOVE_NOMONSTERS, nullptr);
            if(!route.startsolid && route.fraction < 1.f)
            {
                break;
            }
            if(ballFree(candidate, size * 0.5f))
            {
                at = candidate;
                free = true;
                break;
            }
        }
        if(!free)
        {
            s.blocked++;
            continue;
        }
        while(s.count >= most && s.count > 0)
        {
            retireOldest();
        }
        const float speed = rnd(lo, hi), life = rnd(za::min(la, lb), za::max(la, lb));
        const glm::vec3 spin{rnd(-600.f, 600.f) * degrees, rnd(-600.f, 600.f) * degrees, rnd(-600.f, 600.f) * degrees};
        if(!make(at, dir * speed * units::metresToUnits(), spin, size, life))
        {
            break;
        }
        s.minSpeed = za::min(s.minSpeed, speed);
        s.maxSpeed = za::max(s.maxSpeed, speed);
        s.minLife = za::min(s.minLife, life);
        s.maxLife = za::max(s.maxLife, life);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The client's view of them

struct Seen
{
    double born{-1.0}; // when it was first drawn
    int pass{-100};    // the last relink pass it was drawn in
};

struct Glow
{
    glm::vec3 pos{0.f};
    float heat{0.f};
};

struct Client
{
    za::Vector<Seen> seen; // by entity number
    int pass{0};
    int passFrame{-1}; // host_framecount of the pass
    Glow glows[capacity];
    int glowCount{0};
    int drawn{0}; // chunks drawn in the last pass
    int lastFrame{-1};
    int trailSamples{0}, lights{0};
};
Client client;

// How long the chunk has left (s), from its age (the client doesn't know its life: the settings' middle) and its alpha
// once it fades (its last fadeMost seconds: alpha 1 at their start).
[[nodiscard]] float remainingOf(float age, byte alpha)
{
    if(alpha != ENTALPHA_DEFAULT)
    {
        return ENTALPHA_DECODE(alpha) * fadeMost;
    }
    return za::max(0.5f * (lifeMin() + lifeMax()) - age, fadeMost); // (not fading yet: at least the fade)
}

void lights()
{
    Client& c = client;
    // Remove last frame's glows before choosing the nearest ones: disabling/reducing the budget takes effect now.
    for(dlight_t& dl : cl_dlights)
    {
        if(dl.key <= lightKey && dl.key > lightKey - capacity)
        {
            dl.radius = 0.f;
            dl.die = 0.f;
        }
    }
    c.lights = 0;
    bool chosen[capacity]{};
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const int count = static_cast<int>(za::clamp(vr_explosion_debris_lights.value, 0.f, 16.f));
    const float radius = za::clamp(vr_explosion_debris_light_radius.value, 0.f, 200.f) * units::worldScale();
    if(radius <= 0.f)
    {
        return;
    }
    for(int n = 0; n < count; ++n)
    {
        int nearest = -1;
        float distance = 1e30f;
        for(int i = 0; i < c.glowCount; ++i)
        {
            if(chosen[i])
            {
                continue;
            }
            const glm::vec3 delta = c.glows[i].pos - eye;
            const float d = glm::dot(delta, delta);
            if(d < distance)
            {
                distance = d;
                nearest = i;
            }
        }
        if(nearest < 0)
        {
            break;
        }
        chosen[nearest] = true;
        const Glow& g = c.glows[nearest];
        dlight_t* dl = CL_AllocDlight(lightKey - nearest);
        for(int k = 0; k < 3; ++k)
        {
            dl->origin[k] = g.pos[k];
        }
        dl->radius = radius * g.heat;
        dl->die = cl.time + 0.05;
        dl->color[0] = 1.f;
        dl->color[1] = vr_colored_lights.value ? 0.35f : 1.f;
        dl->color[2] = vr_colored_lights.value ? 0.05f : 1.f;
        lighting::dlightLook(dl, 0.f, 0.f);
        lighting::dlightNoShadow(dl);
        ++c.lights;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands

void stats_f()
{
    const Server& s = server;
    const Client& c = client;
    int asleep = 0;
    if(sv.active)
    {
        const VmScope vm;
        for(int i = 0; i < s.count; i++)
        {
            if(valid(s.live[i]) && (static_cast<int>(EDICT_NUM(s.live[i].num)->v.flags) & FL_ONGROUND))
            {
                asleep++;
            }
        }
    }
    Con_Printf("explosiondebris: live=%d resting=%d made=%d expired=%d evicted=%d blocked=%d noroom=%d speed=%.3f..%.3f "
               "life=%.3f..%.3f | client: drawn=%d trails=%d lights=%d health=%d fireballs=%d\n",
        s.count, asleep, s.made, s.expired, s.evicted, s.blocked, s.noRoom, s.made ? s.minSpeed : 0.f, s.maxSpeed,
        s.made ? s.minLife : 0.f, s.maxLife, c.drawn, c.trailSamples, c.lights, cl.stats[STAT_HEALTH],
        particles::largeExplosionCount());
}

// vr_explosion_debris_list: each chunk of the server's: its entity, place, speed, whether it rests (and on what), alpha.
void list_f()
{
    if(!sv.active)
    {
        return;
    }
    const VmScope vm;
    const Server& s = server;
    for(int i = 0; i < s.count; i++)
    {
        const Live& c = s.live[i];
        if(!valid(c))
        {
            continue;
        }
        edict_t* e = EDICT_NUM(c.num);
        edict_t* ground = PROG_TO_EDICT(e->v.groundentity);
        Con_Printf("chunk %d: at %.1f %.1f %.1f speed %.1f %s on %d (%s) age %.2f left %.2f alpha %.2f\n", c.num,
            e->v.origin[0], e->v.origin[1], e->v.origin[2], VectorLength(e->v.velocity),
            (static_cast<int>(e->v.flags) & FL_ONGROUND) ? "resting" : "moving", NUM_FOR_EDICT(ground),
            PR_GetString(ground->v.classname), qcvm->time - c.born, c.die - qcvm->time,
            progs::fieldFloat(e, progs::fields().alpha));
    }
    Con_Printf("%d chunks\n", s.count);
}

// vr_explosion_debris_launch <x y z> <vx vy vz> [size] [life]: one chunk of the server's at a place (units), at a
// velocity (units/s), `size` units (2) and `life` s (the settings' middle). For the tests (tunnelling, lifts).
void launch_f()
{
    if(!sv.active || Cmd_Argc() < 7)
    {
        Con_Printf("usage: vr_explosion_debris_launch <x y z> <vx vy vz> [size] [life] (in a game)\n");
        return;
    }
    const VmScope vm;
    if(!usable())
    {
        Con_Printf("vr_explosion_debris_launch: not Quake VR's progs\n");
        return;
    }
    glm::vec3 at, vel;
    for(int k = 0; k < 3; k++)
    {
        at[k] = static_cast<float>(Q_atof(Cmd_Argv(1 + k)));
        vel[k] = static_cast<float>(Q_atof(Cmd_Argv(4 + k)));
    }
    const float size = Cmd_Argc() > 7 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(7))), 0.5f, 16.f) : 2.f;
    const float life = Cmd_Argc() > 8 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(8))), 0.1f, 600.f) : 0.5f * (lifeMin() + lifeMax());
    if(edict_t* e = make(at, vel, glm::vec3{0.f}, size, life))
    {
        Con_Printf("vr_explosion_debris_launch: chunk %d\n", NUM_FOR_EDICT(e));
    }
}

void test_f()
{
    if(cls.state != ca_connected || !cl.worldmodel)
    {
        return;
    }
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "clear"))
    {
        serverClear();
        clear();
        return;
    }
    vec3_t forward, right, up;
    AngleVectors(r_refdef.viewangles, forward, right, up);
    vec3_t pos;
    for(int k = 0; k < 3; ++k)
    {
        pos[k] = r_refdef.vieworg[k] + forward[k] * 64.f;
    }
    Con_DPrintf("explosiondebris test: at %.1f %.1f %.1f, flags=%u enabled=%.0f\n", pos[0], pos[1], pos[2], cl.protocolflags,
        vr_explosion_debris.value);
    // The explosion as the client draws it (no blast, no message), and the server's chunks (a listen server's).
    const char* kind = Cmd_Argc() > 1 ? Cmd_Argv(1) : "normal";
    if(!strcmp(kind, "colored"))
    {
        R_ParticleExplosion2(pos, 229, 8);
    }
    else if(!strcmp(kind, "tar"))
    {
        R_BlobExplosion(pos);
    }
    else if(!strcmp(kind, "preset"))
    {
        particles::spawn({pos[0], pos[1], pos[2]}, glm::vec3{0.f}, particles::Preset::Explosion, 1);
    }
    else
    {
        R_ParticleExplosion(pos);
    }
    if(sv.active)
    {
        noteBroadcast({pos[0], pos[1], pos[2]});
    }
    else
    {
        Con_DPrintf("explosiondebris test: chunks are the server's (none on a remote one's client)\n");
    }
}
} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// The server's

void noteBroadcast(const glm::vec3& at)
{
    Server& s = server;
    if(s.pendingCount < maxPending)
    {
        s.pending[s.pendingCount++] = at;
    }
}

void serverFrame()
{
    Server& s = server;
    if(!usable())
    {
        s.pendingCount = 0;
        return;
    }
    if(s.rescan)
    {
        s.rescan = false;
        rescan();
    }
    (void)removeIf([](const Live& c) { return !valid(c); }, false); // removed (SUB_Remove, a cleanup) or reused
    s.expired += removeIf([](const Live& c) { return qcvm->time >= c.die; }, true);
    const int most = limit();
    while(s.count > most)
    {
        retireOldest(); // (Most lowered, or the effect turned off: the excess goes now)
    }
    for(int i = 0; i < s.pendingCount; i++)
    {
        launch(s.pending[i]);
    }
    s.pendingCount = 0;
    // The fade: its last half second (a third of a short life), alpha 0 being Quake's "opaque".
    const int alphaField = progs::fields().alpha;
    for(int i = 0; i < s.count; i++)
    {
        const Live& c = s.live[i];
        const float fade = za::min(fadeMost, static_cast<float>(c.die - c.born) * 0.33f);
        const float alpha = za::clamp(static_cast<float>(c.die - qcvm->time) / za::max(fade, 1e-3f), 0.f, 1.f);
        progs::setFieldFloat(EDICT_NUM(c.num), alphaField, alpha >= 1.f ? 0.f : za::max(alpha, 1e-3f));
    }
}

void serverReset()
{
    Server& s = server;
    s.count = 0;
    s.pendingCount = 0;
    s.rescan = true;
    s.serial = 0.f;
    s.modelIndex = -1;
    s.subRemove = -1;
    s.made = s.expired = s.evicted = s.blocked = s.noRoom = 0;
    s.minSpeed = s.minLife = 1e30f;
    s.maxSpeed = s.maxLife = 0.f;
    s.rng = za::FastNonCryptoRng{vr_particle_seed.value > 0.f ? static_cast<za::U64>(vr_particle_seed.value)
                                                             : static_cast<za::U64>(za::Clock::nowNanoseconds())};
}

void serverClear()
{
    if(!sv.active)
    {
        return;
    }
    const VmScope vm;
    server.evicted += removeIf([](const Live&) { return true; }, true);
}

int serverCount() { return server.count; }

// ---------------------------------------------------------------------------------------------------------------------
// The client's

int liveCount() { return client.drawn; }

void frame()
{
    Client& c = client;
    if(c.lastFrame == host_framecount)
    {
        return;
    }
    c.lastFrame = host_framecount;
    if(c.passFrame != host_framecount)
    {
        c.glowCount = 0; // (no chunk drawn this frame)
        c.drawn = 0;
    }
    lights();
}

void prepare() { (void)Mod_ForName(modelName, false); }

void clear()
{
    Client& c = client;
    c.seen.clear();
    c.glowCount = 0;
    c.drawn = 0;
    c.lastFrame = -1;
    c.passFrame = -1;
    c.trailSamples = c.lights = 0;
    for(dlight_t& dl : cl_dlights)
    {
        if(dl.key <= lightKey && dl.key > lightKey - capacity)
        {
            dl.radius = 0.f;
            dl.die = 0.f;
        }
    }
}

void registerCommands()
{
    Cmd_AddCommand("vr_explosion_debris_test", test_f);
    Cmd_AddCommand("vr_explosion_debris_stats", stats_f);
    Cmd_AddCommand("vr_explosion_debris_list", list_f);
    Cmd_AddCommand("vr_explosion_debris_launch", launch_f);
}
} // namespace qvr::explosiondebris

// CL_RelinkEntities, each entity drawn without a trail of Quake's (before its trailorg is set to where it is now): an
// explosion's chunk draws its fire trail from where it was drawn last, and is a glow to choose from (frame: lights).
extern "C" void VR_ExplosionDebrisTrail(int num)
{
    using namespace qvr;
    using namespace qvr::explosiondebris;
    entity_t* e = &cl_entities[num];
    if(!e->model || !modelmeta::is(e->model, modelmeta::Id::VrExplosionDebris))
    {
        return;
    }
    Client& c = client;
    if(c.passFrame != host_framecount)
    {
        c.passFrame = host_framecount;
        c.pass++;
        c.glowCount = 0;
        c.drawn = 0;
    }
    if(num >= static_cast<int>(c.seen.size()))
    {
        c.seen.resize(static_cast<za::SizeT>(num) + 64);
    }
    Seen& s = c.seen[static_cast<za::SizeT>(num)];
    if(s.pass < c.pass - 1)
    {
        s.born = cl.time; // not drawn in the last pass: a new chunk (an entity slot is reused only half a second after)
    }
    s.pass = c.pass;
    c.drawn++;
    const float age = static_cast<float>(cl.time - s.born);
    const float remaining = remainingOf(age, e->alpha);
    const glm::vec3 from{e->trailorg[0], e->trailorg[1], e->trailorg[2]}, to{e->origin[0], e->origin[1], e->origin[2]};
    if(c.glowCount < capacity)
    {
        c.glows[c.glowCount++] = Glow{to, za::clamp(remaining / za::min(1.5f, age + remaining), 0.f, 1.f)};
    }
    if(glm::length(to - from) > 0.01f && vr_explosion_debris_trail.value > 0.f && vr_particles.value && r_particles.value)
    {
        const float extent = za::max(e->model->maxs[0] - e->model->mins[0], e->model->maxs[1] - e->model->mins[1],
            e->model->maxs[2] - e->model->mins[2], 1.f);
        const float size = extent * static_cast<float>(e->scale) / ENTSCALE_DEFAULT;
        particles::explosionDebrisTrail(from, to, size, za::clamp(remaining / za::max(age + remaining, 0.1f), 0.f, 1.f));
        ++c.trailSamples;
    }
}
