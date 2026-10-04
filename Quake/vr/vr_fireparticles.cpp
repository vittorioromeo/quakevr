#include "vr_fireparticles.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"
#include "vr_particles.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include <string.h>

namespace qvr::fireparticles
{
namespace
{
struct Clock
{
    double time{-1.0}, owed{0.0};
    const qmodel_t* model{nullptr};
};
struct Emitters
{
    ankerl::unordered_dense::map<int, Clock> clocks;
    auto members() { return mem::list(clocks); }
};
mem::Cache<Emitters> emitters{"fire emitters", mem::MapChange | mem::GameDirChange | mem::ModelReload};
int lastFrame = -1;
int particlesMade = 0, staticSources = 0, dynamicSources = 0, torchSources = 0;
int pendingTorches = 0;

bool enabled()
{
    return vr_fire_particles.value && vr_particles.value && r_particles.value && cl.worldmodel &&
        (cl.protocolflags & PRFL_QUAKEVR);
}
void emit(int id, const qmodel_t* model, const glm::vec3& at, float scale)
{
    if(!enabled()) { return; }
    Clock& clock = emitters.clocks[id];
    if(clock.model != model || clock.time < 0.0 || cl.time < clock.time || cl.time - clock.time > 0.5)
    {
        clock = Clock{}; clock.model = model; clock.time = cl.time;
        return; // no burst of accumulated particles on a newly visible/reused entity, load or rewind
    }
    const double dt = cl.time - clock.time;
    clock.time = cl.time;
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const float range = za::clamp(vr_fire_particles_range.value, 0.f, 4096.f);
    const float frequency = za::clamp(vr_fire_particles_frequency.value, 0.f, 60.f);
    if(glm::distance(at, eye) > range || frequency <= 0.f || vr_fire_particles_count.value <= 0.f)
    {
        clock.owed = 0.0; return;
    }
    clock.owed = za::min(clock.owed + dt * frequency, 8.0); // bound catch-up after a hitch
    while(clock.owed >= 1.0)
    {
        particlesMade += particles::fireSource(at, scale);
        clock.owed -= 1.0;
    }
}
bool flame(const entity_t& e)
{
    if(!e.model || e.model->type != mod_alias || e.alpha == ENTALPHA_ZERO) { return false; }
    const char* name = e.model->name;
    return !strcmp(name, "progs/flame.mdl") || !strcmp(name, "progs/flame2.mdl") ||
        !strcmp(name, "progs/candle.mdl") || !strcmp(name, "progs/lantern.mdl");
}
void entity(int id, const entity_t& e)
{
    const float scale = ENTSCALE_DECODE(e.scale);
    float height = 3.f;
    const char* name = e.model->name;
    if(!strcmp(name, "progs/flame.mdl")) { height = 14.f; }
    else if(!strcmp(name, "progs/flame2.mdl") && e.frame == 1) { height = 14.f; }
    else if(!strcmp(name, "progs/candle.mdl")) { height = 10.f; }
    else if(!strcmp(name, "progs/lantern.mdl")) { height = 4.f; }
    const glm::vec3 pos{e.origin[0], e.origin[1], e.origin[2] + height * scale};
    emit(id, e.model, pos, scale);
}
void stats_f()
{
    Con_Printf("fireparticles: static=%d dynamic=%d torches=%d made=%d clocks=%d\n",
        staticSources, dynamicSources, torchSources, particlesMade, int(emitters.clocks.size()));
}
}
void emitTorch(int id, const glm::vec3& at, float scale)
{
    if(!enabled()) { return; }
    ++pendingTorches;
    emit(0x20000 + id, nullptr, at, scale);
}
void frame()
{
    if(lastFrame == host_framecount) { return; }
    lastFrame = host_framecount;
    staticSources = dynamicSources = 0;
    torchSources = pendingTorches; pendingTorches = 0;
    if(!enabled()) { emitters.clocks.clear(); return; }
    for(int i = 0; i < cl.num_statics; ++i)
    {
        const entity_t& e = cl_static_entities[i];
        if(flame(e)) { entity(i, e); ++staticSources; }
    }
    for(int i = 1; i < cl.num_entities; ++i)
    {
        const entity_t& e = cl_entities[i];
        if(e.msgtime == cl.mtime[0] && flame(e)) { entity(0x10000 + i, e); ++dynamicSources; }
    }
    // Destroyed fires, extinguished torches and reused edicts do not leave a growing cache.
    erase_if(emitters.clocks, [](const auto& pair) { return cl.time - pair.second.time > 1.0; });
}
void clear()
{
    emitters.clocks.clear(); lastFrame = -1; particlesMade = staticSources = dynamicSources = torchSources = pendingTorches = 0;
}
void registerCommands() { Cmd_AddCommand("vr_fire_particles_stats", stats_f); }
}
