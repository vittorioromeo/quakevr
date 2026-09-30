// vr_audiosim.hpp -- the spatial audio's acoustics (vr_audio.cpp): Steam Audio's scene made from the map (the world's
// and the brush models' faces; doors, lifts and trains moved as they move), and its simulations run on the game's
// thread pool (vr_jobs.hpp): each sound's direct path (occlusion by the walls, transmission through them, the air's
// absorption) and the listener's room (the reverb, as Steam Audio's listener-centric reverb: a source at the head).
//
// Threads. The main thread owns the simulator: it starts a task only when the last one of its kind is done, hands it
// its inputs in the task's own copy, and changes the scene (a new map, a mover's place: iplSceneCommit) only while no
// task runs (Steam Audio forbids a commit during a simulation). A task hands its results back under the mutex
// (direct(), reflections()); the direct and the reflections tasks may run at once (Steam Audio allows it: each sets
// only its own kind of input).
#pragma once

#include "vr_engine.hpp"
#include "vr_jobs.hpp"
#include "vr_steamaudio.hpp"

#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"

#include <mutex>

namespace qvr::audio
{

// Quake's axes and units (x forward, y left, z up; units) to Steam Audio's (x right, y up, -z ahead; metres).
[[nodiscard]] IPLVector3 toSteam(const glm::vec3& q, float unitsPerMetre);
[[nodiscard]] IPLVector3 dirToSteam(const glm::vec3& q);
// The listener (or a source) at `pos` looking along forward, right, up (Quake's).
[[nodiscard]] IPLCoordinateSpace3 coordinates(
    const glm::vec3& pos, const glm::vec3& fwd, const glm::vec3& right, const glm::vec3& up, float unitsPerMetre);

// What the map's faces are made of, by their textures' names (materialOf): how much they absorb, scatter and let
// through (low, middle, high frequencies).
enum class SurfaceMaterial : int
{
    Stone, // the default: rock, brick, concrete
    Wood,
    Metal,
    Grate, // fence textures ({...): mostly open
    Count
};
[[nodiscard]] SurfaceMaterial materialOf(const char* textureName, bool fence);
[[nodiscard]] const IPLMaterial& material(SurfaceMaterial m);

// Triangles in Steam Audio's space (metres).
struct Mesh
{
    za::Vector<IPLVector3> vertices;
    za::Vector<IPLTriangle> triangles;
    za::Vector<IPLint32> materials; // one per triangle (SurfaceMaterial)
    void clear()
    {
        vertices.clear();
        triangles.clear();
        materials.clear();
    }
    // A convex polygon (Quake's units and axes), fanned; its front (where the level is) along `normal`.
    void addPolygon(const glm::vec3* points, int count, const glm::vec3& normal, SurfaceMaterial m, float unitsPerMetre);
    // A box's six faces, facing out (a wall, a pillar) or in (a room).
    void addBox(const glm::vec3& mins, const glm::vec3& maxs, bool inward, SurfaceMaterial m, float unitsPerMetre);
};

// A brush model's faces (the world's own, or a submodel's in its own frame), skies and liquids left out.
void appendBrushModel(const qmodel_t* model, Mesh& out, float unitsPerMetre);

// A source's direct path, as simulated.
struct DirectResult
{
    float occlusion{1.f};                   // 1 in the open, 0 hidden
    float transmission[3]{1.f, 1.f, 1.f};   // what gets through what hides it (low, middle, high)
    float air[3]{1.f, 1.f, 1.f};            // the air's absorption over the distance
};

struct SimSettings
{
    bool occlusion{false};
    int occlusionSamples{8};   // 1: a single ray (on or off); more: partial occlusion (a sphere's points)
    float occlusionRadius{0.5f}; // metres
    bool air{false};
    bool reverb{false};
    int rays{2048};
    int bounces{16};
    float duration{1.5f};      // s of impulse response (convolution, hybrid)
    int order{1};              // its Ambisonics order
    float hybridTransition{0.1f}; // s: convolution for this much, parametric after (hybrid)
    double directInterval{1.0 / 30.0};
    double reverbInterval{0.25};
};

class Simulation
{
public:
    static constexpr int maxSources = 64;

    struct Source
    {
        bool active{false};
        unsigned serial{0}; // the voice's sound (a result for an older one is not given: direct())
        IPLVector3 pos{};
    };

    Simulation() = default;
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;
    ~Simulation();

    // `type`: the reverb's algorithm (fixed for the simulator's life; parametric needs no impulse response).
    bool create(int rate, int frameSize, IPLReflectionEffectType type, int maxOrder, float maxDuration, int maxRays);
    void destroy(); // waits for its tasks
    [[nodiscard]] bool valid() const
    {
        return simulator != nullptr;
    }
    [[nodiscard]] IPLReflectionEffectType reflectionType() const
    {
        return type;
    }
    [[nodiscard]] int maxOrder() const
    {
        return order;
    }

    // A new scene made from `world` on the pool (in use from the first update after it is made; until then, none).
    void buildScene(Mesh&& world);
    // ... or at once (tests).
    void buildSceneNow(Mesh&& world);
    [[nodiscard]] bool hasScene() const
    {
        return scene != nullptr;
    }
    [[nodiscard]] int sceneTriangles() const
    {
        return triangles;
    }
    [[nodiscard]] double sceneBuildMs() const
    {
        return buildMs;
    }
    void dropScene(); // (a map left: no scene until the next)

    // Brush models placed in the scene: `sub` a submodel's triangles (made once per scene, by `make` if missing), the
    // instance `key` (an entity) at `transform` (Steam Audio's space). Applied at the next update with no task running;
    // those not given since the last beginInstances are taken out.
    void beginInstances();
    void placeInstance(int key, int sub, const IPLMatrix4x4& transform, void (*make)(int sub, Mesh& out, void* user), void* user);
    [[nodiscard]] int instanceCount() const
    {
        return static_cast<int>(instances.size());
    }

    // The main thread, each frame: results collected, the scene changed if nothing runs, the tasks started when due.
    void update(const IPLCoordinateSpace3& listener, const Source* sources, int count, const SimSettings& s, double now);

    // The latest results (any time).
    [[nodiscard]] bool direct(int slot, unsigned serial, DirectResult& out) const;
    [[nodiscard]] bool reflections(IPLReflectionEffectParams& out) const;

    // Tests: a simulation at once, on the calling thread (after the running tasks are done).
    void runDirectNow(const IPLCoordinateSpace3& listener, const Source* sources, int count, const SimSettings& s);
    void runReflectionsNow(const IPLCoordinateSpace3& listener, const SimSettings& s);

    // vr_snd_info
    [[nodiscard]] double directMs() const;
    [[nodiscard]] double reflectionsMs() const;
    void saveObj(const char* baseName);

private:
    struct DirectJob
    {
        IPLCoordinateSpace3 listener{};
        za::Array<Source, maxSources> sources{};
        int count{0};
        SimSettings settings;
    };
    struct ReflectionsJob
    {
        IPLCoordinateSpace3 listener{};
        SimSettings settings;
    };
    struct Built
    {
        IPLScene scene{nullptr};
        IPLStaticMesh mesh{nullptr};
        int triangles{0};
        double ms{0.0};
    };
    struct SubScene
    {
        IPLScene scene{nullptr};
        IPLStaticMesh mesh{nullptr};
        bool made{false};
    };
    struct Instance
    {
        int key{0};
        int sub{0};
        IPLInstancedMesh mesh{nullptr};
        IPLMatrix4x4 transform{};
        bool wanted{false};
        bool moved{false};
        bool remake{false};
    };

    static Built build(const steamaudio::Api* sa, IPLContext context, Mesh&& mesh);
    void runDirect(const DirectJob& job);
    void runReflections(const ReflectionsJob& job);
    void finishTasks(); // waits
    void useBuilt(Built b);
    void releaseScene();
    void applyInstances();

    const steamaudio::Api* sa{nullptr};
    IPLSimulator simulator{nullptr};
    IPLReflectionEffectType type{IPL_REFLECTIONEFFECTTYPE_PARAMETRIC};
    int order{0};
    za::Array<IPLSource, maxSources> sources{};
    IPLSource reverbSource{nullptr};

    IPLScene scene{nullptr};
    IPLStaticMesh worldMesh{nullptr};
    int triangles{0};
    double buildMs{0.0};
    jobs::Future<Built> building;
    za::Vector<SubScene> subScenes; // by submodel number
    za::Vector<Instance> instances;
    bool dirty{false};

    jobs::Future<void> directTask;
    jobs::Future<void> reflectionsTask;
    DirectJob directJob;          // the running direct task's (the main thread's between tasks)
    ReflectionsJob reflectionsJob; // the same for reflections
    double lastDirect{-1e9};
    double lastReflections{-1e9};

    mutable za::AtomicMutex mutex; // guards what follows
    za::Array<DirectResult, maxSources> directOut{};
    za::Array<bool, maxSources> directValid{};
    za::Array<unsigned, maxSources> directSerial{};
    IPLReflectionEffectParams reflectionsOut{};
    bool reflectionsValid{false};
    double directTaskMs{0.0};
    double reflectionsTaskMs{0.0};
};

// The brush entities the client sees (doors, lifts, trains, func_walls), placed in the simulation's scene; returns how
// many it saw.
int trackBrushEntities(Simulation& sim, float unitsPerMetre);

} // namespace qvr::audio
