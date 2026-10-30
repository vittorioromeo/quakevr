// vr_box3d_diag_perf.cpp -- the physics timing commands: the step's and the frame's times, the multithreaded
// bench. Moved out of vr_box3d_diag.cpp; what they share with it is vr_box3d_diag_internal.hpp.

#include "vr_stats.hpp"
#include "vr_alloccount.h"
#include "vr_geom.hpp"
#include "vr_progs.hpp"

#include "vr_box3d_diag_internal.hpp"
#include "vr_box3d_internal.hpp"
#include "vr_hands.hpp"
#include "vr_units.hpp"

namespace box3d::detail
{

namespace
{

// The q quantile (0..1) of sorted samples (0: none).
[[nodiscard]] double quantile(const za::Vector<float>& sorted, double q)
{
    return static_cast<double>(vrstats::quantileSorted(sorted.data(), sorted.size(), q, vrstats::Rank::Count));
}

} // namespace

// vr_physics_steptime [bins]: Box3D's step time a frame since it last printed (the average, the median, the 95th
// percentile and the worst, ms), the awake bodies on average, and its workers (vr_box3d_threads); then starts counting
// again. A bench: run it, play, run it again (Misc/quakevr/box3dmt/pilebench.py). `bins`: also the frames by the awake
// bodies as their step began (what Physics Threads From compares), each range's frames, median and 95th percentile; and
// Box3D's own profile (ms a frame: where the step went).
void steptime_f()
{
    if(!world)
    {
        Con_Printf("vr_physics_steptime: no physics world\n");
        return;
    }
    const int n = za::max(world->stepFrames, 1);
    za::Vector<float> ms;
    ms.reserve(world->stepSamples.size());
    for(const World::StepSample& x : world->stepSamples)
    {
        ms.pushBack(x.ms);
    }
    za::quickSort(ms.begin(), ms.end());
    const za::SizeT over = static_cast<za::SizeT>(ms.end() - za::upperBound(ms.begin(), ms.end(), 2.f));
    Con_Printf("vr_physics_steptime: %d frames, step %.3f ms (median %.3f, p95 %.3f, p99 %.3f, worst %.3f; %d over 2 ms), "
               "%.1f bodies awake, %d bodies, %d workers\n",
        world->stepFrames, world->stepTime * 1000.0 / n, quantile(ms, 0.5), quantile(ms, 0.95), quantile(ms, 0.99),
        world->stepTimeMax * 1000.0, static_cast<int>(over), static_cast<double>(world->stepAwake) / n,
        b3World_GetCounters(world->id).bodyCount, world->tasks.workers);
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "bins"))
    {
        constexpr int edges[] = {0, 10, 25, 50, 75, 100, 125, 150, 200, 300, 500, 1000, 100000};
        for(za::SizeT b = 0; b + 1 < sizeof(edges) / sizeof(edges[0]); b++)
        {
            ms.clear();
            for(const World::StepSample& x : world->stepSamples)
            {
                if(x.awake >= edges[b] && x.awake < edges[b + 1])
                {
                    ms.pushBack(x.ms);
                }
            }
            if(ms.empty())
            {
                continue;
            }
            za::quickSort(ms.begin(), ms.end());
            Con_Printf("vr_physics_steptime bin %d %d: %d frames, median %.4f, p95 %.4f\n", edges[b], edges[b + 1],
                static_cast<int>(ms.size()), quantile(ms, 0.5), quantile(ms, 0.95));
        }
        const b3Profile& p = world->stepProfile;
        const float k = 1.f / static_cast<float>(n);
        Con_Printf("vr_physics_steptime profile: step %.3f pairs %.3f collide %.3f solve %.3f (setup %.3f constraints %.3f "
                   "[prepare %.3f intvel %.3f warm %.3f impulses %.3f intpos %.3f relax %.3f restitution %.3f store %.3f] "
                   "transforms %.3f split %.3f refit %.3f bullets %.3f sleep %.3f)\n",
            p.step * k, p.pairs * k, p.collide * k, p.solve * k, p.solverSetup * k, p.constraints * k,
            p.prepareConstraints * k, p.integrateVelocities * k, p.warmStart * k, p.solveImpulses * k,
            p.integratePositions * k, p.relaxImpulses * k, p.restitution * k, p.storeImpulses * k, p.transforms * k,
            p.splitIslands * k, p.refit * k, p.bullets * k, p.sleepIslands * k);
    }
    world->stepProfile = b3Profile{};
    world->stepSamples.clear();
    world->stepTime = 0.0;
    world->stepTimeMax = 0.0;
    world->stepFrames = 0;
    world->stepAwake = 0;
}

// vr_physics_frametime [<label>]: each server frame's physics since it last printed, by phase (framePhaseNames; ms: the
// mean, median, 95th and 99th percentiles and the worst), with the wall-clock time and the host frames between the two
// calls (the whole frame's cost, with vr_fixed_frames: a test run's same frames); then starts counting again (the first
// call starts it: no samples kept before). A bench: Misc/quakevr/physbench/physbench.py.
void frametime_f()
{
    frameTiming = true;
    const double now = Sys_DoubleTime();
    const double wall = frameLastWall > 0.0 ? now - frameLastWall : 0.0;
    const int hostFrames = host_framecount - frameLastHostFrame;
    frameLastWall = now;
    frameLastHostFrame = host_framecount;
    const char* label = Cmd_Argc() > 1 ? Cmd_Argv(1) : "-";
    if(!world)
    {
        Con_Printf("vr_physics_frametime %s: no physics world\n", label);
        return;
    }
    const za::Vector<World::FrameSample>& samples = world->frameSamples;
    double awake = 0.0;
    for(const World::FrameSample& x : samples)
    {
        awake += x.awake;
    }
    const b3Counters c = b3World_GetCounters(world->id);
    Con_Printf("vr_physics_frametime %s: %d frames, %d host frames in %.3f s (%.4f ms a host frame), %.1f awake, %d bodies, "
               "%d contacts\n",
        label, static_cast<int>(samples.size()), hostFrames, wall, hostFrames > 0 ? wall * 1000.0 / hostFrames : 0.0,
        samples.empty() ? 0.0 : awake / static_cast<double>(samples.size()), c.bodyCount, c.contactCount);
    za::Vector<float> ms;
    ms.reserve(samples.size());
    for(int p = 0; p < framePhases; p++)
    {
        ms.clear();
        double sum = 0.0;
        for(const World::FrameSample& x : samples)
        {
            ms.pushBack(x.ms[p]);
            sum += x.ms[p];
        }
        za::quickSort(ms.begin(), ms.end());
        Con_Printf("vr_physics_frametime %s %s: mean %.4f median %.4f p95 %.4f p99 %.4f worst %.4f\n", label,
            framePhaseNames[p], ms.empty() ? 0.0 : sum / static_cast<double>(ms.size()), quantile(ms, 0.5), quantile(ms, 0.95),
            quantile(ms, 0.99), ms.empty() ? 0.0 : static_cast<double>(ms.back()));
    }
    world->frameSamples.clear();
}

// vr_physics_mtbench [<bodies> [<steps>]]: Box3D on the pool, alone (ROUND21.md, "Box3D on the pool"). A world of its
// own: a floor, `bodies` boxes (60 cm and 90 cm cubes: a small explosive box, a crate; 200) in leaning columns of 10 that
// topple into a pile, stepped `steps` times (300) at 1/72 s with vr_box3d_substeps, from the same start with each worker
// count (1, 2, 3, 4, 6, 8, 16 and all the pool's, as far as the pool goes): each count's step time, its awake bodies and
// a hash of every body's place, turn and velocities after, which must be the same for every count.
void mtbench_f()
{
    const int bodies = Cmd_Argc() > 1 ? CLAMP(1, Q_atoi(Cmd_Argv(1)), 5000) : 200;
    const int steps = Cmd_Argc() > 2 ? CLAMP(1, Q_atoi(Cmd_Argv(2)), 100000) : 300;
    const int substeps = CLAMP(1, static_cast<int>(vr_box3d_substeps.value), 8);
    const int most = jobs::pool() ? za::min(jobs::workers() + 1, B3_MAX_WORKERS) : 1;
    za::Vector<int> counts;
    for(const int n : {1, 2, 3, 4, 6, 8, 16})
    {
        if(n < most)
        {
            counts.pushBack(n);
        }
    }
    counts.pushBack(most);
    const za::UniquePtr<StepTasks> tasks = za::makeUnique<StepTasks>();
    za::U64 first = 0;
    bool same = true;
    for(const int workers : counts)
    {
        tasks->workers = workers;
        b3WorldDef def = b3DefaultWorldDef();
        def.gravity = b3Vec3{0.f, 0.f, -9.81f};
        def.enableSleep = true;
        def.enableContinuous = true;
        def.isotropicInertiaTolerance = 0.f; // as the game's world (vr_box3d.cpp)
        def.workerCount = static_cast<uint32_t>(workers);
        def.enqueueTask = enqueueStepTask;
        def.finishTask = finishStepTask;
        def.userTaskContext = tasks.get();
        const b3WorldId id = b3CreateWorld(&def);
        b3BodyDef floorDef = b3DefaultBodyDef();
        floorDef.position = b3Pos{0.f, 0.f, -0.5f};
        const b3ShapeDef shape = b3DefaultShapeDef();
        const b3BoxHull floorHull = b3MakeBoxHull(100.f, 100.f, 0.5f);
        b3CreateHullShape(b3CreateBody(id, &floorDef), &shape, &floorHull.base);
        const b3BoxHull small = b3MakeCubeHull(0.3f), crate = b3MakeCubeHull(0.45f);
        za::Vector<b3BodyId> made;
        const int columns = (bodies + 9) / 10, side = za::max(1, static_cast<int>(za::ceil(za::sqrt(static_cast<float>(columns)))));
        for(int i = 0; i < bodies; i++)
        {
            const int column = i / 10, level = i % 10;
            const bool big = column % 2 == 1;
            const float half = big ? 0.45f : 0.3f;
            b3BodyDef body = b3DefaultBodyDef();
            body.type = b3_dynamicBody;
            // Each level 10 cm further along x than the one under it, and turned (as vr_physics_pile): they lean and fall.
            body.position = b3Pos{static_cast<float>(column % side) * 2.5f + static_cast<float>(level) * 0.1f,
                static_cast<float>(column / side) * 2.5f, 0.02f + static_cast<float>(level) * (2.f * half + 0.02f) + half};
            body.rotation = b3MakeQuatFromAxisAngle(b3Vec3{0.f, 0.f, 1.f}, static_cast<float>((i * 37) % 90) * (3.14159265f / 180.f));
            const b3BodyId b = b3CreateBody(id, &body);
            b3CreateHullShape(b, &shape, big ? &crate.base : &small.base);
            made.pushBack(b);
        }
        double time = 0.0;
        za::U64 awake = 0;
        for(int i = 0; i < steps; i++)
        {
            tasks->next.storeRelaxed(0);
            applySimdWidth(id);
            const double t0 = Sys_DoubleTime();
            b3World_Step(id, 1.f / 72.f, substeps);
            time += Sys_DoubleTime() - t0;
            awake += static_cast<za::U64>(b3World_GetAwakeBodyCount(id));
        }
        za::U64 h = 1469598103934665603ull;
        const auto mix = [&h](const void* p, size_t n)
        {
            for(size_t k = 0; k < n; k++)
            {
                h = (h ^ static_cast<const unsigned char*>(p)[k]) * 1099511628211ull;
            }
        };
        for(const b3BodyId b : made)
        {
            const b3WorldTransform x = b3Body_GetTransform(b);
            const b3Vec3 v[2] = {b3Body_GetLinearVelocity(b), b3Body_GetAngularVelocity(b)};
            mix(&x, sizeof(x));
            mix(v, sizeof(v));
        }
        b3DestroyWorld(id);
        if(workers == counts[0])
        {
            first = h;
        }
        same = same && h == first;
        Con_Printf("vr_physics_mtbench: %d bodies, %d steps of %d: %2d workers %.3f ms a step, %.1f awake, hash %016llx\n",
            bodies, steps, substeps, workers, time * 1000.0 / steps, static_cast<double>(awake) / steps,
            static_cast<unsigned long long>(h));
    }
    Con_Printf("vr_physics_mtbench: %s\n", same ? "the same with every worker count" : "DIFFERENT between worker counts");
}

} // namespace box3d::detail
