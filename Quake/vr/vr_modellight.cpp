// vr_modellight.cpp -- see vr_modellight.hpp.

#include "vr_modellight.hpp"
#include "vr_cvars.hpp"
#include "vr_trace.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

using namespace qvr;

namespace
{

using Light = modellight::MapLight;

struct Cached
{
    glm::vec3 samplePos{0.f};
    glm::vec4 target{0.f}; // the last computed direction
    glm::vec4 dir{0.f};    // smoothed towards it
    double computedAt = -1.0;
    int frame = -1;
};

const qmodel_t* loadedWorld = nullptr;
std::vector<Light> lights;
std::unordered_map<const entity_t*, Cached> cache;

// The light entities of the map: classname light*, lit at the start (not "start off" with a
// target name to switch them on).
void loadLights()
{
    lights.clear();
    cache.clear();
    loadedWorld = cl.worldmodel;
    if(!cl.worldmodel || !cl.worldmodel->entities)
    {
        return;
    }

    const char* data = cl.worldmodel->entities;
    while((data = COM_Parse(data)) && com_token[0] == '{')
    {
        bool isLight = false;
        bool startsOff = false;
        bool hasTarget = false;
        glm::vec3 origin{0.f};
        float value = 300.f;
        float scale = 1.f;
        while((data = COM_Parse(data)) && com_token[0] != '}')
        {
            char key[64];
            q_strlcpy(key, com_token, sizeof(key));
            if(!(data = COM_Parse(data)))
            {
                break;
            }
            if(!strcmp(key, "classname"))
            {
                isLight = !strncmp(com_token, "light", 5);
            }
            else if(!strcmp(key, "origin"))
            {
                sscanf(com_token, "%f %f %f", &origin.x, &origin.y, &origin.z);
            }
            else if(!strcmp(key, "light") || !strcmp(key, "_light"))
            {
                value = static_cast<float>(atof(com_token));
            }
            else if(!strcmp(key, "wait"))
            {
                scale = static_cast<float>(atof(com_token));
            }
            else if(!strcmp(key, "spawnflags"))
            {
                startsOff = (atoi(com_token) & 1) != 0;
            }
            else if(!strcmp(key, "targetname"))
            {
                hasTarget = com_token[0] != '\0';
            }
        }
        if(!data)
        {
            break;
        }
        if(isLight && !(startsOff && hasTarget) && value > 0.f)
        {
            lights.push_back({origin, value, scale > 0.f ? scale : 1.f});
        }
    }
    Con_DPrintf("VR: %d lights for model shading\n", static_cast<int>(lights.size()));
}

// The direction towards the light reaching `p`: the strongest few lights in reach and in sight,
// weighted by their brightness there. w is how much they agree on a direction.
glm::vec4 compute(const glm::vec3& p)
{
    struct Candidate
    {
        float weight;
        const Light* light;
    };
    Candidate best[4];
    int count = 0;
    for(const Light& l : lights)
    {
        const float w = l.value - glm::distance(l.pos, p) * l.scale;
        if(w <= 0.f)
        {
            continue;
        }
        if(count < 4)
        {
            best[count++] = {w, &l};
        }
        else
        {
            Candidate* weakest = std::min_element(best, best + 4, [](auto& a, auto& b) { return a.weight < b.weight; });
            if(w > weakest->weight)
            {
                *weakest = {w, &l};
            }
        }
    }

    glm::vec3 sum{0.f};
    float total = 0.f;
    for(int i = 0; i < count; i++)
    {
        const glm::vec3 to = best[i].light->pos - p;
        const float dist = glm::length(to);
        if(dist < 1.f)
        {
            continue;
        }
        // In sight: nothing in between, or only the wall the light is mounted on.
        const float reached = worldtrace::line(p, best[i].light->pos);
        if(reached < 1.f && (1.f - reached) * dist > 24.f)
        {
            continue;
        }
        sum += to / dist * best[i].weight;
        total += best[i].weight;
    }

    if(total <= 0.f)
    {
        return glm::vec4{0.f};
    }
    const float len = glm::length(sum);
    if(len < 1e-3f)
    {
        return glm::vec4{0.f};
    }
    return glm::vec4{sum / len, len / total};
}

} // namespace

const std::vector<modellight::MapLight>& modellight::mapLights()
{
    if(cl.worldmodel != loadedWorld)
    {
        loadLights();
    }
    return lights;
}

glm::vec4 modellight::direction(const entity_t* e)
{
    const float amount = CLAMP(0.f, vr_model_lighting.value, 1.f);
    if(amount <= 0.f || !cl.worldmodel || !e || !e->model)
    {
        return glm::vec4{0.f};
    }
    if(cl.worldmodel != loadedWorld)
    {
        loadLights();
    }
    if(lights.empty())
    {
        return glm::vec4{0.f};
    }

    Cached& c = cache[e];
    if(c.frame == host_framecount)
    {
        return glm::vec4{glm::vec3{c.dir}, c.dir.w * amount};
    }

    // The middle of the model's height.
    const glm::vec3 p{e->origin[0], e->origin[1], e->origin[2] + (e->model->mins[2] + e->model->maxs[2]) * 0.5f};

    // Recomputed when it moved or a moment passed (lights switch, doors open), staggered
    // between entities; eased towards it so that it never pops.
    const double stagger = static_cast<double>(reinterpret_cast<uintptr_t>(e) % 97) * 0.002;
    const bool fresh = c.computedAt < 0.0;
    if(fresh || glm::distance(p, c.samplePos) > 12.f || realtime - c.computedAt > 0.3 + stagger)
    {
        c.target = compute(p);
        c.samplePos = p;
        c.computedAt = realtime;
    }

    const float dt = c.frame < 0 ? 1.f : static_cast<float>(host_frametime);
    const float k = fresh ? 1.f : 1.f - std::exp(-dt * 6.f);
    glm::vec3 dir = glm::vec3{c.dir} * c.dir.w;
    dir = glm::mix(dir, glm::vec3{c.target} * c.target.w, k);
    const float len = glm::length(dir);
    c.dir = len > 1e-4f ? glm::vec4{dir / len, std::min(1.f, len)} : glm::vec4{0.f};
    c.frame = host_framecount;
    const glm::vec4 result{glm::vec3{c.dir}, c.dir.w * amount};

    // Evict entities not drawn for a while (temporary entities come and go).
    if(cache.size() > 2048)
    {
        std::erase_if(cache, [](const auto& kv) { return kv.second.frame < host_framecount - 100; });
    }
    return result;
}

extern "C" void VR_AliasLightDir(const entity_t* e, float dir[4])
{
    const glm::vec4 d = modellight::direction(e);
    dir[0] = d.x;
    dir[1] = d.y;
    dir[2] = d.z;
    dir[3] = d.w;
}
