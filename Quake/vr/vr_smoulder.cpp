// vr_smoulder.cpp -- see vr_smoulder.hpp.
//
// Each smouldering body is a slot (its entity number): how long lightning keeps it smoking, when its fire goes out and
// how long it smokes after. Its smoke is the stronger of the two, thinning out as either ends (the square root of the
// time left over the whole: steady for most of it, then thin to nothing). Its wisps are due at a rate (fractions carried
// over), each from a random point of its triangles as drawn now (vr_modelcollide.cpp), most from the parts facing up.

#include "vr_modelmetadata.hpp"
#include "vr_smoulder.hpp"
#include "vr_modelcollide.hpp"
#include "vr_shock.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::smoulder
{
namespace
{

struct Body
{
    int num{0};                     // 0: a free slot
    const qmodel_t* model{nullptr}; // as first drawn (another, not the same body's: it is gone, gibbed)
    double struckUntil{0.0};        // lightning: smoking until then,
    double struckLen{0.0};          // ... from this long before (thinning over it)
    double fireOut{0.0};            // a fire: burning until then (full smoke),
    double fireUntil{0.0};          // ... then smoking, thinning, until then
    double owed{0.0};               // wisps due (fractions carried to the next frame)
};

constexpr int maxBodies = 32;
Body bodies[maxBodies];

constexpr float wispsPerSecond = 18.f; // a fresh body's wisps a second at vr_smoulder 1
constexpr float range = 1500.f;        // units from the eye beyond which a body doesn't smoke
constexpr int maxWispsFrame = 4;       // a body's at most a frame (after a hitch)

// The bodies' buffers (the client's frame: the main thread).
struct SmoulderScratch
{
    za::Vector<glm::vec3> tris; // a body's triangles as drawn
    auto members() { return mem::list(tris); }
};
mem::Scratch<SmoulderScratch> scratch{"smoulder"};

int lastFrame = -1;    // once a frame, however often the view is set up
double lastTime = 0.0; // cl.time at the last frame
int made = 0;          // wisps made since the last vr_smoulder_info
unsigned seed = 0x9e3779b9u;

// 0..1, from a frame to the next.
[[nodiscard]] float rnd()
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return static_cast<float>(seed >> 8) * (1.f / 16777216.f);
}

// When it stops smoking (as things stand).
[[nodiscard]] double ends(const Body& b)
{
    return za::max(b.struckUntil, b.fireUntil);
}

// Its slot (a new one if `make`: a free one, else the one that stops smoking first); nullptr if none.
Body* find(int num, bool make)
{
    if(num <= 0)
    {
        return nullptr;
    }
    for(Body& b : bodies)
    {
        if(b.num == num)
        {
            return &b;
        }
    }
    if(!make)
    {
        return nullptr;
    }
    Body* spare = &bodies[0];
    for(Body& b : bodies)
    {
        if(!b.num)
        {
            spare = &b;
            break;
        }
        if(ends(b) < ends(*spare))
        {
            spare = &b;
        }
    }
    *spare = Body{};
    spare->num = num;
    return spare;
}

// How much it smokes now (0..1; 0: it has stopped).
[[nodiscard]] float strength(const Body& b, double now)
{
    float k = 0.f;
    if(b.struckUntil > now && b.struckLen > 0.0)
    {
        k = za::sqrt(za::clamp(static_cast<float>((b.struckUntil - now) / b.struckLen), 0.f, 1.f));
    }
    if(now < b.fireOut)
    {
        k = 1.f;
    }
    else if(b.fireUntil > now)
    {
        const double len = b.fireUntil - b.fireOut;
        k = za::max(k, len > 0.0 ? za::sqrt(za::clamp(static_cast<float>((b.fireUntil - now) / len), 0.f, 1.f)) : 0.f);
    }
    return k;
}

// Its surface as drawn now into `tris`; false if it isn't there to smoke (out of the last update: for now; another
// model, not its own: `b` ends).
[[nodiscard]] bool surface(Body& b, za::Vector<glm::vec3>& tris)
{
    if(b.num >= cl.num_entities)
    {
        return false;
    }
    const entity_t& ent = cl_entities[b.num];
    if(!ent.model || ent.model->type != mod_alias || ent.msgtime < cl.mtime[0] - 0.001)
    {
        return false;
    }
    if(!b.model)
    {
        b.model = ent.model;
    }
    else if(b.model != ent.model && !shock::sameBody(b.model, ent.model))
    {
        b = Body{};
        return false;
    }
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const glm::vec3 org{ent.origin[0], ent.origin[1], ent.origin[2]};
    if(glm::distance(org, eye) > range)
    {
        return false;
    }
    return modelcollide::drawnTriangles(ent, b.num, tris) && tris.size() >= 3;
}

// A random point of `tris` and the way out of the body there (its winding's normal, turned away from the middle),
// mostly from the parts facing up (smoke rises off the top of it); false on degenerate ones.
[[nodiscard]] bool wispPlace(const za::Vector<glm::vec3>& tris, const glm::vec3& mid, glm::vec3& at, glm::vec3& out)
{
    const size_t count = tris.size() / 3;
    bool found = false;
    for(int tries = 0; tries < 4; tries++)
    {
        const size_t tri = za::min(static_cast<size_t>(rnd() * static_cast<float>(count)), count - 1);
        const glm::vec3 a = tris[tri * 3], b = tris[tri * 3 + 1], c = tris[tri * 3 + 2];
        const glm::vec3 cross = glm::cross(b - a, c - a);
        const float area = glm::length(cross);
        if(area < 1e-5f)
        {
            continue;
        }
        float u = rnd(), v = rnd();
        if(u + v > 1.f)
        {
            u = 1.f - u;
            v = 1.f - v;
        }
        at = a + (b - a) * u + (c - a) * v;
        out = cross / area;
        if(glm::dot(out, at - mid) < 0.f)
        {
            out = -out;
        }
        found = true;
        if(out.z > -0.1f)
        {
            return true;
        }
    }
    return found;
}

void info_f()
{
    const double now = cl.time;
    int active = 0;
    for(const Body& b : bodies)
    {
        const float k = b.num ? strength(b, now) : 0.f;
        if(k <= 0.f)
        {
            continue;
        }
        active++;
        Con_Printf("smoulder: entity=%d strength=%.2f lightning_left=%.2f fire_out_in=%.2f smoke_left=%.2f\n", b.num, k,
            za::max(0.0, b.struckUntil - now), za::max(0.0, b.fireOut - now),
            za::max(0.0, ends(b) - now));
    }
    Con_Printf("smoulder: active=%d made=%d time=%.2f\n", active, made, now);
    made = 0;
}

// id's monsters' models (and their ragdolls' copies, "<model>#rag"): vr_smoulder_test's bodies.
constexpr const char* monsterModels[] = {"progs/soldier", "progs/dog", "progs/knight", "progs/hknight", "progs/ogre",
    "progs/demon", "progs/shambler", "progs/wizard", "progs/zombie", "progs/enforcer", "progs/fish", "progs/shalrath",
    "progs/tarbaby", "progs/oldone", "progs/boss"};

// vr_smoulder_test [seconds]: every monster and body within 1000 units smokes that long (5.5) as a lightning bolt's
// would, thinning: how it looks, and what many cost (vr_profile's "smoulder").
void test_f()
{
    const float len = Cmd_Argc() > 1 ? za::clamp(static_cast<float>(atof(Cmd_Argv(1))), 0.1f, 600.f) : 5.5f;
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    int started = 0;
    for(int num = 1; num < cl.num_entities; num++)
    {
        const entity_t& ent = cl_entities[num];
        if(!ent.model || ent.model->type != mod_alias || ent.msgtime < cl.mtime[0] - 0.001 ||
           glm::distance(glm::vec3{ent.origin[0], ent.origin[1], ent.origin[2]}, eye) > 1000.f)
        {
            continue;
        }
        bool monster = false;
        for(const char* m : monsterModels)
        {
            monster = monster || !strncmp(ent.model->name, m, strlen(m));
        }
        if(Body* b = monster ? find(num, true) : nullptr)
        {
            b->struckUntil = cl.time + len;
            b->struckLen = len;
            started++;
        }
    }
    Con_Printf("smoulder: test: %d bodies smoking for %.1f s\n", started, len);
}

} // namespace

void struck(int num)
{
    const float len = za::clamp(vr_smoulder_time.value, 0.f, 60.f);
    if(len <= 0.f)
    {
        return;
    }
    if(Body* b = find(num, true))
    {
        b->struckUntil = cl.time + len;
        b->struckLen = len;
    }
}

void burning(int num, float left, bool doused)
{
    if(doused)
    {
        if(Body* b = find(num, false))
        {
            *b = Body{};
        }
        return;
    }
    if(Body* b = find(num, true))
    {
        b->fireOut = cl.time + za::max(0.f, left);
        b->fireUntil = b->fireOut + za::clamp(vr_smoulder_burn_time.value, 0.f, 60.f);
    }
}

void frame()
{
    if(host_framecount == lastFrame)
    {
        return;
    }
    lastFrame = host_framecount;
    const double now = cl.time;
    const float dt = za::clamp(static_cast<float>(now - lastTime), 0.f, 0.1f);
    lastTime = now;
    const float density = za::max(0.f, vr_smoulder.value);
    const bool on = density > 0.f && vr_particles.value && r_particles.value && cl.worldmodel;
    QVR_PROFILE("smoulder");
    for(Body& b : bodies)
    {
        if(!b.num)
        {
            continue;
        }
        const float k = strength(b, now);
        if(k <= 0.f)
        {
            if(b.struckUntil <= now && b.fireUntil <= now)
            {
                b = Body{};
            }
            continue;
        }
        if(!on)
        {
            b.owed = 0.0;
            continue;
        }
        b.owed = za::min(b.owed + static_cast<double>(dt * wispsPerSecond * density * k), static_cast<double>(maxWispsFrame));
        const int due = static_cast<int>(b.owed);
        if(due <= 0)
        {
            continue;
        }
        za::Vector<glm::vec3>& tris = scratch.tris;
        tris.clear();
        if(!surface(b, tris))
        {
            b.owed = 0.0; // (out of sight or gone: none saved up for its return)
            continue;
        }
        b.owed -= due;
        glm::vec3 mid{0.f};
        for(const glm::vec3& p : tris)
        {
            mid += p;
        }
        mid /= static_cast<float>(tris.size());
        const float alpha = za::clamp(vr_smoulder_alpha.value, 0.f, 1.f) * (0.4f + 0.6f * k);
        for(int i = 0; i < due; i++)
        {
            glm::vec3 at, out;
            if(wispPlace(tris, mid, at, out))
            {
                particles::smoulderSmoke(at, out, 1, alpha);
                made++;
            }
        }
    }
}

void clear()
{
    for(Body& b : bodies)
    {
        b = Body{};
    }
    lastTime = 0.0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_smoulder_info", info_f);
    Cmd_AddCommand("vr_smoulder_test", test_f);
}

} // namespace qvr::smoulder
