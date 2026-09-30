// vr_audiosim.cpp -- the spatial audio's scene and simulations; see vr_audiosim.hpp and docs/vr-port/ROUND21.md,
// "Spatial audio (Steam Audio)".

#include "vr_audiosim.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <utility>

namespace qvr::audio
{

// ----------------------------------------------------------------------------
// Space

IPLVector3 toSteam(const glm::vec3& q, float unitsPerMetre)
{
    const float s = 1.f / unitsPerMetre;
    return IPLVector3{-q.y * s, q.z * s, -q.x * s};
}

IPLVector3 dirToSteam(const glm::vec3& q)
{
    return IPLVector3{-q.y, q.z, -q.x};
}

IPLCoordinateSpace3 coordinates(
    const glm::vec3& pos, const glm::vec3& fwd, const glm::vec3& right, const glm::vec3& up, float unitsPerMetre)
{
    IPLCoordinateSpace3 c{};
    c.right = dirToSteam(right);
    c.up = dirToSteam(up);
    c.ahead = dirToSteam(fwd);
    c.origin = toSteam(pos, unitsPerMetre);
    return c;
}

// ----------------------------------------------------------------------------
// Materials

namespace
{

// Absorption, scattering, transmission (low, middle, high). Absorption after Steam Audio's own presets (concrete,
// wood, metal); transmission is the game's, not physics': a wall lets a monster behind it through quieter and duller,
// not silent (vr_snd_occlusion scales how much of it applies).
const IPLMaterial materials[static_cast<int>(SurfaceMaterial::Count)] = {
    {{0.05f, 0.07f, 0.08f}, 0.20f, {0.25f, 0.08f, 0.02f}}, // Stone
    {{0.11f, 0.07f, 0.06f}, 0.20f, {0.35f, 0.12f, 0.04f}}, // Wood
    {{0.20f, 0.07f, 0.06f}, 0.10f, {0.30f, 0.10f, 0.03f}}, // Metal
    {{0.05f, 0.05f, 0.05f}, 0.30f, {0.90f, 0.85f, 0.80f}}, // Grate
};

bool contains(const char* lower, const char* part)
{
    return std::strstr(lower, part) != nullptr;
}

} // namespace

SurfaceMaterial materialOf(const char* textureName, bool fence)
{
    if(fence)
    {
        return SurfaceMaterial::Grate;
    }
    char lower[32]{};
    for(int i = 0; i < 31 && textureName && textureName[i]; i++)
    {
        lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(textureName[i])));
    }
    if(contains(lower, "wood") || contains(lower, "plank") || contains(lower, "crate"))
    {
        return SurfaceMaterial::Wood;
    }
    if(contains(lower, "met") || contains(lower, "tech") || contains(lower, "plat") || contains(lower, "door") ||
       contains(lower, "comp"))
    {
        return SurfaceMaterial::Metal;
    }
    return SurfaceMaterial::Stone;
}

const IPLMaterial& material(SurfaceMaterial m)
{
    return materials[std::clamp(static_cast<int>(m), 0, static_cast<int>(SurfaceMaterial::Count) - 1)];
}

// ----------------------------------------------------------------------------
// Meshes

void Mesh::addPolygon(const glm::vec3* points, int count, const glm::vec3& normal, SurfaceMaterial m, float unitsPerMetre)
{
    if(count < 3)
    {
        return;
    }
    // Wound so that the triangles' normals (Steam Audio's: (b - a) x (c - a), right-handed; our change of axes is a
    // rotation, so the Quake winding carries over) face the level: its diffuse reflections go back into the room.
    glm::vec3 n{0.f};
    for(int k = 1; k + 1 < count && glm::dot(n, n) < 1e-6f; k++)
    {
        n = glm::cross(points[k] - points[0], points[k + 1] - points[0]);
    }
    const bool flip = glm::dot(n, normal) < 0.f;
    const int base = static_cast<int>(vertices.size());
    for(int k = 0; k < count; k++)
    {
        vertices.push_back(toSteam(points[k], unitsPerMetre));
    }
    for(int k = 1; k + 1 < count; k++)
    {
        IPLTriangle t{};
        t.indices[0] = base;
        t.indices[1] = base + (flip ? k + 1 : k);
        t.indices[2] = base + (flip ? k : k + 1);
        triangles.push_back(t);
        materials.push_back(static_cast<IPLint32>(m));
    }
}

void Mesh::addBox(const glm::vec3& mins, const glm::vec3& maxs, bool inward, SurfaceMaterial m, float unitsPerMetre)
{
    const glm::vec3 c[8] = {
        {mins.x, mins.y, mins.z}, {maxs.x, mins.y, mins.z}, {maxs.x, maxs.y, mins.z}, {mins.x, maxs.y, mins.z},
        {mins.x, mins.y, maxs.z}, {maxs.x, mins.y, maxs.z}, {maxs.x, maxs.y, maxs.z}, {mins.x, maxs.y, maxs.z}};
    struct Face
    {
        int v[4];
        glm::vec3 out;
    };
    const Face faces[6] = {
        {{0, 1, 2, 3}, {0.f, 0.f, -1.f}}, {{4, 5, 6, 7}, {0.f, 0.f, 1.f}},  {{0, 1, 5, 4}, {0.f, -1.f, 0.f}},
        {{3, 2, 6, 7}, {0.f, 1.f, 0.f}},  {{0, 3, 7, 4}, {-1.f, 0.f, 0.f}}, {{1, 2, 6, 5}, {1.f, 0.f, 0.f}}};
    for(const Face& f : faces)
    {
        const glm::vec3 p[4] = {c[f.v[0]], c[f.v[1]], c[f.v[2]], c[f.v[3]]};
        addPolygon(p, 4, inward ? -f.out : f.out, m, unitsPerMetre);
    }
}

void appendBrushModel(const qmodel_t* model, Mesh& out, float unitsPerMetre)
{
    if(!model || model->type != mod_brush || !model->surfaces)
    {
        return;
    }
    glm::vec3 points[64];
    for(int s = 0; s < model->nummodelsurfaces; s++)
    {
        const msurface_t* surf = model->surfaces + model->firstmodelsurface + s;
        if(surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
        {
            continue; // the sky is open; liquids let sound through (their surface is no wall)
        }
        const int count = std::min<int>(surf->numedges, 64);
        for(int e = 0; e < count; e++)
        {
            const int edge = model->surfedges[surf->firstedge + e];
            const float* v = edge >= 0 ? model->vertexes[model->edges[edge].v[0]].position
                                       : model->vertexes[model->edges[-edge].v[1]].position;
            points[e] = glm::vec3{v[0], v[1], v[2]};
        }
        glm::vec3 normal{surf->plane->normal[0], surf->plane->normal[1], surf->plane->normal[2]};
        if(surf->flags & SURF_PLANEBACK)
        {
            normal = -normal;
        }
        const int texnum = surf->texinfo ? surf->texinfo->texnum : -1;
        const texture_t* tex = texnum >= 0 && texnum < model->numtextures ? model->textures[texnum] : nullptr;
        out.addPolygon(points, count, normal, materialOf(tex ? tex->name : "", (surf->flags & SURF_DRAWFENCE) != 0),
            unitsPerMetre);
    }
}

// ----------------------------------------------------------------------------
// Simulation

Simulation::~Simulation()
{
    destroy();
}

bool Simulation::create(int rate, int frameSize, IPLReflectionEffectType reflection, int maxOrder, float maxDuration, int maxRays)
{
    destroy();
    sa = steamaudio::api();
    if(!sa)
    {
        return false;
    }
    IPLSimulationSettings s{};
    s.flags = static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS);
    s.sceneType = IPL_SCENETYPE_DEFAULT;
    s.reflectionType = reflection;
    s.maxNumOcclusionSamples = 32;
    s.maxNumRays = maxRays;
    s.numDiffuseSamples = 32;
    s.maxDuration = maxDuration;
    s.maxOrder = maxOrder;
    s.maxNumSources = maxSources + 1;
    s.numThreads = 1; // the task that runs it is the pool's (Steam Audio's own threads are not wanted)
    s.samplingRate = rate;
    s.frameSize = frameSize;
    if(sa->iplSimulatorCreate(steamaudio::context(), &s, &simulator) != IPL_STATUS_SUCCESS)
    {
        simulator = nullptr;
        return false;
    }
    type = reflection;
    order = maxOrder;
    IPLSourceSettings direct{};
    direct.flags = IPL_SIMULATIONFLAGS_DIRECT;
    for(IPLSource& src : sources)
    {
        sa->iplSourceCreate(simulator, &direct, &src);
        sa->iplSourceAdd(src, simulator);
    }
    IPLSourceSettings reverb{};
    reverb.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
    sa->iplSourceCreate(simulator, &reverb, &reverbSource);
    sa->iplSourceAdd(reverbSource, simulator);
    sa->iplSimulatorCommit(simulator);
    {
        const std::lock_guard lock{mutex};
        directValid.fill(false);
        reflectionsValid = false;
    }
    lastDirect = lastReflections = -1e9;
    return true;
}

void Simulation::finishTasks()
{
    if(directTask.valid())
    {
        directTask.get();
    }
    if(reflectionsTask.valid())
    {
        reflectionsTask.get();
    }
}

void Simulation::releaseScene()
{
    for(Instance& in : instances)
    {
        if(scene)
        {
            sa->iplInstancedMeshRemove(in.mesh, scene);
        }
        sa->iplInstancedMeshRelease(&in.mesh);
    }
    instances.clear();
    for(SubScene& sub : subScenes)
    {
        if(sub.mesh)
        {
            sa->iplStaticMeshRelease(&sub.mesh);
        }
        if(sub.scene)
        {
            sa->iplSceneRelease(&sub.scene);
        }
    }
    subScenes.clear();
    if(worldMesh)
    {
        sa->iplStaticMeshRelease(&worldMesh);
    }
    if(scene)
    {
        sa->iplSceneRelease(&scene);
    }
    worldMesh = nullptr;
    scene = nullptr;
    triangles = 0;
    dirty = false;
}

void Simulation::destroy()
{
    if(building.valid())
    {
        Built b = building.get();
        if(b.mesh)
        {
            sa->iplStaticMeshRelease(&b.mesh);
        }
        if(b.scene)
        {
            sa->iplSceneRelease(&b.scene);
        }
    }
    if(!simulator)
    {
        return;
    }
    finishTasks();
    releaseScene();
    for(IPLSource& src : sources)
    {
        if(src)
        {
            sa->iplSourceRemove(src, simulator);
            sa->iplSourceRelease(&src);
        }
        src = nullptr;
    }
    if(reverbSource)
    {
        sa->iplSourceRemove(reverbSource, simulator);
        sa->iplSourceRelease(&reverbSource);
    }
    reverbSource = nullptr;
    sa->iplSimulatorRelease(&simulator);
    simulator = nullptr;
}

Simulation::Built Simulation::build(const steamaudio::Api* sa, IPLContext context, Mesh&& mesh)
{
    Built b;
    const double start = Sys_DoubleTime();
    IPLSceneSettings settings{};
    settings.type = IPL_SCENETYPE_DEFAULT;
    if(sa->iplSceneCreate(context, &settings, &b.scene) != IPL_STATUS_SUCCESS)
    {
        b.scene = nullptr;
        return b;
    }
    if(!mesh.triangles.empty())
    {
        IPLStaticMeshSettings ms{};
        ms.numVertices = static_cast<IPLint32>(mesh.vertices.size());
        ms.numTriangles = static_cast<IPLint32>(mesh.triangles.size());
        ms.numMaterials = static_cast<IPLint32>(SurfaceMaterial::Count);
        ms.vertices = mesh.vertices.data();
        ms.triangles = mesh.triangles.data();
        ms.materialIndices = mesh.materials.data();
        ms.materials = const_cast<IPLMaterial*>(materials);
        if(sa->iplStaticMeshCreate(b.scene, &ms, &b.mesh) == IPL_STATUS_SUCCESS)
        {
            sa->iplStaticMeshAdd(b.mesh, b.scene);
            b.triangles = ms.numTriangles;
        }
        else
        {
            b.mesh = nullptr;
        }
    }
    sa->iplSceneCommit(b.scene);
    b.ms = (Sys_DoubleTime() - start) * 1000.0;
    return b;
}

void Simulation::useBuilt(Built b)
{
    releaseScene();
    scene = b.scene;
    worldMesh = b.mesh;
    triangles = b.triangles;
    buildMs = b.ms;
    if(scene)
    {
        sa->iplSimulatorSetScene(simulator, scene);
        sa->iplSimulatorCommit(simulator);
    }
    const std::lock_guard lock{mutex};
    directValid.fill(false);
    reflectionsValid = false;
}

void Simulation::buildScene(Mesh&& world)
{
    if(!simulator)
    {
        return;
    }
    if(building.valid())
    {
        Built old = building.get(); // (a newer map: the older build dropped)
        if(old.mesh)
        {
            sa->iplStaticMeshRelease(&old.mesh);
        }
        if(old.scene)
        {
            sa->iplSceneRelease(&old.scene);
        }
    }
    building = jobs::async([api = sa, context = steamaudio::context(), mesh = std::move(world)]() mutable {
        return build(api, context, std::move(mesh));
    });
}

void Simulation::buildSceneNow(Mesh&& world)
{
    if(!simulator)
    {
        return;
    }
    finishTasks();
    useBuilt(build(sa, steamaudio::context(), std::move(world)));
}

void Simulation::dropScene()
{
    if(!simulator)
    {
        return;
    }
    finishTasks();
    if(scene)
    {
        sa->iplSimulatorSetScene(simulator, nullptr);
        sa->iplSimulatorCommit(simulator);
    }
    releaseScene();
}

void Simulation::beginInstances()
{
    for(Instance& in : instances)
    {
        in.wanted = false;
    }
}

void Simulation::placeInstance(int key, int sub, const IPLMatrix4x4& transform, void (*make)(int, Mesh&, void*), void* user)
{
    if(!scene || sub < 0 || sub > 4096)
    {
        return;
    }
    if(static_cast<int>(subScenes.size()) <= sub)
    {
        subScenes.resize(sub + 1);
    }
    SubScene& s = subScenes[sub];
    if(!s.made)
    {
        s.made = true;
        Mesh mesh;
        make(sub, mesh, user);
        if(!mesh.triangles.empty())
        {
            Built b = build(sa, steamaudio::context(), std::move(mesh));
            s.scene = b.scene;
            s.mesh = b.mesh;
        }
    }
    if(!s.scene)
    {
        return;
    }
    // (Changes are applied by applyInstances: a simulation may be running now.)
    auto it = std::find_if(instances.begin(), instances.end(), [&](const Instance& in) { return in.key == key; });
    if(it == instances.end())
    {
        Instance in;
        in.key = key;
        in.sub = sub;
        in.transform = transform;
        instances.push_back(in);
        it = instances.end() - 1;
    }
    it->wanted = true;
    if(it->sub != sub)
    {
        it->sub = sub;
        it->remake = true; // (another model now)
    }
    if(std::memcmp(&it->transform, &transform, sizeof transform) != 0)
    {
        it->transform = transform;
        it->moved = true;
    }
}

void Simulation::applyInstances()
{
    if(!scene)
    {
        return;
    }
    for(Instance& in : instances)
    {
        if((!in.wanted || in.remake) && in.mesh)
        {
            sa->iplInstancedMeshRemove(in.mesh, scene);
            sa->iplInstancedMeshRelease(&in.mesh);
            in.mesh = nullptr;
            dirty = true;
        }
        in.remake = false;
        if(!in.wanted)
        {
            continue;
        }
        if(!in.mesh)
        {
            IPLInstancedMeshSettings s{};
            s.subScene = subScenes[in.sub].scene;
            s.transform = in.transform;
            if(sa->iplInstancedMeshCreate(scene, &s, &in.mesh) == IPL_STATUS_SUCCESS)
            {
                sa->iplInstancedMeshAdd(in.mesh, scene);
                dirty = true;
            }
            else
            {
                in.mesh = nullptr;
            }
        }
        else if(in.moved)
        {
            sa->iplInstancedMeshUpdateTransform(in.mesh, scene, in.transform);
            dirty = true;
        }
        in.moved = false;
    }
    std::erase_if(instances, [](const Instance& in) { return !in.wanted; });
    if(dirty)
    {
        sa->iplSceneCommit(scene);
        dirty = false;
    }
}

void Simulation::runDirect(const DirectJob& job)
{
    const double start = Sys_DoubleTime();
    IPLSimulationSharedInputs shared{};
    shared.listener = job.listener;
    sa->iplSimulatorSetSharedInputs(simulator, IPL_SIMULATIONFLAGS_DIRECT, &shared);
    int flags = 0;
    if(job.settings.occlusion)
    {
        flags |= IPL_DIRECTSIMULATIONFLAGS_OCCLUSION | IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION;
    }
    if(job.settings.air)
    {
        flags |= IPL_DIRECTSIMULATIONFLAGS_AIRABSORPTION;
    }
    for(int i = 0; i < maxSources; i++)
    {
        const bool active = i < job.count && job.sources[i].active;
        IPLSimulationInputs in{};
        in.flags = IPL_SIMULATIONFLAGS_DIRECT;
        in.directFlags = static_cast<IPLDirectSimulationFlags>(active ? flags : 0);
        in.source.right = IPLVector3{1.f, 0.f, 0.f};
        in.source.up = IPLVector3{0.f, 1.f, 0.f};
        in.source.ahead = IPLVector3{0.f, 0.f, -1.f};
        in.source.origin = active ? job.sources[i].pos : job.listener.origin;
        in.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
        in.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
        in.directivity.dipoleWeight = 0.f;
        in.directivity.dipolePower = 1.f;
        const int samples = std::clamp(job.settings.occlusionSamples, 1, 32);
        in.occlusionType = samples > 1 ? IPL_OCCLUSIONTYPE_VOLUMETRIC : IPL_OCCLUSIONTYPE_RAYCAST;
        in.occlusionRadius = job.settings.occlusionRadius;
        in.numOcclusionSamples = samples;
        in.numTransmissionRays = 1; // the nearest wall only (a Quake wall is two faces)
        in.reverbScale[0] = in.reverbScale[1] = in.reverbScale[2] = 1.f;
        sa->iplSourceSetInputs(sources[i], IPL_SIMULATIONFLAGS_DIRECT, &in);
    }
    sa->iplSimulatorRunDirect(simulator);
    std::array<DirectResult, maxSources> out{};
    std::array<bool, maxSources> valid{};
    std::array<unsigned, maxSources> serial{};
    for(int i = 0; i < job.count && i < maxSources; i++)
    {
        if(!job.sources[i].active)
        {
            continue;
        }
        IPLSimulationOutputs o{};
        sa->iplSourceGetOutputs(sources[i], IPL_SIMULATIONFLAGS_DIRECT, &o);
        DirectResult& r = out[i];
        if(job.settings.occlusion)
        {
            r.occlusion = std::clamp(o.direct.occlusion, 0.f, 1.f);
            for(int b = 0; b < 3; b++)
            {
                r.transmission[b] = std::clamp(o.direct.transmission[b], 0.f, 1.f);
            }
        }
        if(job.settings.air)
        {
            for(int b = 0; b < 3; b++)
            {
                r.air[b] = std::clamp(o.direct.airAbsorption[b], 0.f, 1.f);
            }
        }
        valid[i] = true;
        serial[i] = job.sources[i].serial;
    }
    const double ms = (Sys_DoubleTime() - start) * 1000.0;
    const std::lock_guard lock{mutex};
    directOut = out;
    directValid = valid;
    directSerial = serial;
    directTaskMs = ms;
}

void Simulation::runReflections(const ReflectionsJob& job)
{
    const double start = Sys_DoubleTime();
    IPLSimulationSharedInputs shared{};
    shared.listener = job.listener;
    shared.numRays = job.settings.rays;
    shared.numBounces = job.settings.bounces;
    shared.duration = job.settings.duration;
    shared.order = std::min(job.settings.order, order);
    shared.irradianceMinDistance = 1.f;
    sa->iplSimulatorSetSharedInputs(simulator, IPL_SIMULATIONFLAGS_REFLECTIONS, &shared);
    IPLSimulationInputs in{};
    in.flags = IPL_SIMULATIONFLAGS_REFLECTIONS;
    in.source = job.listener;
    in.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
    in.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
    in.reverbScale[0] = in.reverbScale[1] = in.reverbScale[2] = 1.f;
    in.hybridReverbTransitionTime = job.settings.hybridTransition;
    in.hybridReverbOverlapPercent = 0.25f;
    in.baked = IPL_FALSE;
    sa->iplSourceSetInputs(reverbSource, IPL_SIMULATIONFLAGS_REFLECTIONS, &in);
    sa->iplSimulatorRunReflections(simulator);
    IPLSimulationOutputs o{};
    sa->iplSourceGetOutputs(reverbSource, IPL_SIMULATIONFLAGS_REFLECTIONS, &o);
    const double ms = (Sys_DoubleTime() - start) * 1000.0;
    const std::lock_guard lock{mutex};
    reflectionsOut = o.reflections;
    reflectionsValid = true;
    reflectionsTaskMs = ms;
}

void Simulation::update(const IPLCoordinateSpace3& listener, const Source* src, int count, const SimSettings& s, double now)
{
    if(!simulator)
    {
        return;
    }
    const bool directIdle = !directTask.valid() || directTask.ready();
    const bool reflectionsIdle = !reflectionsTask.valid() || reflectionsTask.ready();
    if(directIdle && directTask.valid())
    {
        directTask.get();
    }
    if(reflectionsIdle && reflectionsTask.valid())
    {
        reflectionsTask.get();
    }
    if(directIdle && reflectionsIdle)
    {
        if(building.valid() && building.ready())
        {
            useBuilt(building.get());
        }
        applyInstances();
    }
    if(!scene)
    {
        return;
    }
    if(directIdle && (s.occlusion || s.air) && now - lastDirect >= s.directInterval)
    {
        lastDirect = now;
        directJob.listener = listener;
        directJob.count = std::min(count, maxSources);
        std::copy_n(src, directJob.count, directJob.sources.begin());
        directJob.settings = s;
        directTask = jobs::async([this] { runDirect(directJob); });
    }
    if(reflectionsIdle && s.reverb && now - lastReflections >= s.reverbInterval)
    {
        lastReflections = now;
        reflectionsJob.listener = listener;
        reflectionsJob.settings = s;
        reflectionsTask = jobs::async([this] { runReflections(reflectionsJob); });
    }
}

bool Simulation::direct(int slot, unsigned serial, DirectResult& out) const
{
    if(slot < 0 || slot >= maxSources)
    {
        return false;
    }
    const std::lock_guard lock{mutex};
    if(!directValid[slot] || directSerial[slot] != serial)
    {
        return false;
    }
    out = directOut[slot];
    return true;
}

bool Simulation::reflections(IPLReflectionEffectParams& out) const
{
    const std::lock_guard lock{mutex};
    if(!reflectionsValid)
    {
        return false;
    }
    out = reflectionsOut;
    return true;
}

void Simulation::runDirectNow(const IPLCoordinateSpace3& listener, const Source* src, int count, const SimSettings& s)
{
    if(!simulator || !scene)
    {
        return;
    }
    finishTasks();
    directJob.listener = listener;
    directJob.count = std::min(count, maxSources);
    std::copy_n(src, directJob.count, directJob.sources.begin());
    directJob.settings = s;
    runDirect(directJob);
}

void Simulation::runReflectionsNow(const IPLCoordinateSpace3& listener, const SimSettings& s)
{
    if(!simulator || !scene)
    {
        return;
    }
    finishTasks();
    reflectionsJob.listener = listener;
    reflectionsJob.settings = s;
    runReflections(reflectionsJob);
}

double Simulation::directMs() const
{
    const std::lock_guard lock{mutex};
    return directTaskMs;
}

double Simulation::reflectionsMs() const
{
    const std::lock_guard lock{mutex};
    return reflectionsTaskMs;
}

void Simulation::saveObj(const char* baseName)
{
    if(scene)
    {
        finishTasks();
        sa->iplSceneSaveOBJ(scene, baseName);
    }
}

// ----------------------------------------------------------------------------
// Brush entities

namespace
{

struct MakeContext
{
    float unitsPerMetre;
};

void makeSubmodel(int sub, Mesh& out, void* user)
{
    const float upm = static_cast<const MakeContext*>(user)->unitsPerMetre;
    char name[16];
    q_snprintf(name, sizeof name, "*%d", sub);
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        if(!std::strcmp(cl.model_precache[i]->name, name))
        {
            appendBrushModel(cl.model_precache[i], out, upm);
            return;
        }
    }
}

} // namespace

int trackBrushEntities(Simulation& sim, float unitsPerMetre)
{
    if(!sim.hasScene() || !cl_entities)
    {
        return 0;
    }
    int seen = 0;
    MakeContext ctx{unitsPerMetre};
    sim.beginInstances();
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t* e = &cl_entities[i];
        const qmodel_t* m = e->model;
        if(!m || m->type != mod_brush || m->name[0] != '*')
        {
            continue;
        }
        const int sub = std::atoi(m->name + 1);
        // Quake's rotation (angle vectors: forward, left, up as columns), turned into Steam Audio's axes (C R C^T, C
        // the change of axes), and the offset.
        vec3_t f, r, u;
        vec3_t angles{e->angles[0], e->angles[1], e->angles[2]};
        AngleVectors(angles, f, r, u);
        const glm::vec3 col[3] = {{f[0], f[1], f[2]}, {-r[0], -r[1], -r[2]}, {u[0], u[1], u[2]}};
        // C: steam = (-q.y, q.z, -q.x). Steam's axis j is Quake's axis qa[j] times sg[j].
        const int qa[3] = {1, 2, 0};
        const float sg[3] = {-1.f, 1.f, -1.f};
        IPLMatrix4x4 t{};
        for(int row = 0; row < 3; row++)
        {
            for(int c = 0; c < 3; c++)
            {
                // (C R C^T)[row][c] = sg[row] * R[qa[row]][qa[c]] * sg[c]; R[a][b] = col[b][a]
                t.elements[row][c] = sg[row] * col[qa[c]][qa[row]] * sg[c];
            }
        }
        const IPLVector3 o = toSteam(glm::vec3{e->origin[0], e->origin[1], e->origin[2]}, unitsPerMetre);
        t.elements[0][3] = o.x;
        t.elements[1][3] = o.y;
        t.elements[2][3] = o.z;
        t.elements[3][3] = 1.f;
        sim.placeInstance(i, sub, t, makeSubmodel, &ctx);
        seen++;
    }
    return seen;
}

} // namespace qvr::audio
