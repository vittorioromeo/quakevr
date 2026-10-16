// CPU-only regression test for the actual profiler's query management. No GL context,
// OpenXR instance, SteamVR connection, engine startup, or game data is used.
// Build with the engine's include paths, -DNDEBUG -DZA_STATIC -DUSE_SDL2
// -std=c++23 -fno-exceptions -ffunction-sections -fdata-sections, link
// libqvr_zancle.a -pthread -Wl,--gc-sections to discard unused engine entry points.
#include "vr_profile.cpp"

#include <stdlib.h>

namespace
{
int timestampCalls = 0;
int generatedQueries = 0;
int resultReads = 0;
int gpuFramesDone = 0;
bool queryAvailable = false;

void require(bool ok, const char* what)
{
    if(!ok)
    {
        fprintf(stderr, "FAIL: %s\n", what);
        exit(1);
    }
}

void APIENTRY timestamp(GLuint, GLenum)
{
    ++timestampCalls;
}

void APIENTRY genQueries(GLsizei n, GLuint* ids)
{
    for(int i = 0; i < n; ++i)
    {
        ids[i] = static_cast<GLuint>(++generatedQueries);
    }
}

void APIENTRY available(GLuint, GLenum pname, GLint* value)
{
    require(pname == GL_QUERY_RESULT_AVAILABLE, "readback checks availability");
    *value = queryAvailable ? 1 : 0;
}

void APIENTRY result(GLuint id, GLenum pname, GLuint64* value)
{
    require(queryAvailable, "never read unavailable GPU results");
    require(pname == GL_QUERY_RESULT, "timestamp result type");
    ++resultReads;
    *value = static_cast<GLuint64>(id) * 1'000'000;
}
} // namespace

extern "C"
{
decltype(GL_QueryCounterFunc) GL_QueryCounterFunc = timestamp;
decltype(GL_GenQueriesFunc) GL_GenQueriesFunc = genQueries;
decltype(GL_GetQueryObjectivFunc) GL_GetQueryObjectivFunc = available;
decltype(GL_GetQueryObjectui64vFunc) GL_GetQueryObjectui64vFunc = result;
}

namespace qvr
{
cvar_t vr_profile_gpu{};
namespace bench
{
bool recording = false;
void frame(double, double, double, const za::I64 (&)[profile::PhaseCount]) {}
void gpuFrame(const double (&)[profile::PhaseCount]) {}
} // namespace bench
namespace profile::systems
{
void gpu(int, int, double) {}
void gpuFrameDone(int) { ++gpuFramesDone; }
} // namespace profile::systems
} // namespace qvr

int main()
{
    using namespace qvr::profile;
    // Zero from startup must not even allocate phase query objects.
    qvr::vr_profile_gpu.value = 0.f;
    endPhaseFrame(1'000'000'000, 0, 1'000'000'000);
    require(!phaseGpuEnabled && !phaseGpuMade && generatedQueries == 0, "GPU zero at startup");

    qvr::vr_profile_gpu.value = 4.f;
    endPhaseFrame(1'010'000'000, 0, 1'010'000'000);
    require(phaseGpuEnabled && phaseGpuMade, "phase GPU enabled at frame boundary");
    require(generatedQueries == phaseGpuSlots * phaseGpuQueries, "allocate phase ring once");
    gpuOk = false; // detailed tree is unsampled on this frame
    beginPhase("eye L", true);
    qvr::vr_profile_gpu.value = 0.f; // change during scope: its end still belongs to this frame
    endPhase();
    require(timestampCalls == 2 && phaseGpuOpen == 0, "phase query pairing is frame-latched");
    endPhaseFrame(1'020'000'000, 1'010'000'000, 1'019'000'000);
    require(!phaseGpuEnabled, "zero disables phase GPU on the next frame");
    const int allocations = generatedQueries;
    beginPhase("eye L", true);
    endPhase();
    require(timestampCalls == 2 && phaseFrameNs[EyeL] > 0, "CPU phases continue with no GPU queries");
    endPhaseFrame(1'030'000'000, 1'020'000'000, 1'029'000'000);
    require(generatedQueries == allocations && timestampCalls == 2, "zero stays free of new queries");

    qvr::vr_profile_gpu.value = 4.f;
    for(int i = 0; i < 4; ++i)
    {
        endPhaseFrame(1'040'000'000 + i * 10'000'000, 0, 1'040'000'000 + i * 10'000'000);
        beginPhase("eye R", true);
        endPhase();
    }
    require(timestampCalls == 10, "positive N preserves every-frame phase cadence");
    GL_QueryCounterFunc = nullptr;
    endPhaseFrame(1'080'000'000, 0, 1'080'000'000);
    require(!phaseGpuEnabled, "unavailable GL query support disables phase queries");
    GL_QueryCounterFunc = timestamp;

    Node root;
    root.name = "frame";
    nodes.pushBack(root);
    GpuSlot s;
    s.queries.pushBack(1);
    s.queries.pushBack(3);
    s.used = 2;
    s.recs.pushBack({0, 0, 1, -1});
    s.pending = true;
    require(!resolve(s) && s.pending && resultReads == 0, "unresolved sample never blocks on results");
    recycleGpuSlot(s);
    require(!s.pending && s.used == 0 && s.recs.empty() && intervalGpuDropped == 1,
        "reuse drops sample and clears ownership");
    recycleGpuSlot(s);
    require(intervalGpuDropped == 1, "empty slot reuse does not double-count drops");

    s.used = 2;
    s.recs.pushBack({0, 0, 1, -1});
    s.pending = true;
    queryAvailable = true;
    require(resolve(s) && !s.pending && resultReads == 2 && gpuFramesDone == 1,
        "available sample is read exactly once");
    require(intervalGpuFrames == 1 && nodes[0].gpu, "available GPU data remains reportable");
    resetInterval(1'100'000'000);
    require(!nodes[0].gpu && intervalGpuFrames == 0 && intervalGpuDropped == 0,
        "new interval cannot report stale GPU zeroes as measured data");

    intervalFrames = 1;
    nodes[0].cpuSum = 100.0;
    const char* waits[] = {"xr wait", "xr acquire", "xr release", "xr submit", "swap"};
    const double ms[] = {10.0, 15.0, 5.0, 40.0, 2.0};
    for(int i = 0; i < 5; ++i)
    {
        Node n;
        n.name = waits[i];
        n.cpuSum = ms[i];
        nodes.pushBack(n);
    }
    require(hostOutsideWaits() == 28.0, "host estimate excludes all five measured wait scopes");
    nodes[0].cpuSum = 1.0;
    require(hostOutsideWaits() == 0.0, "host estimate clamps measurement overlap");

    puts("PASS: profiler GPU gating, frame latch, phase cadence, nonblocking readback, drop ownership, interval reset, host wait estimate");
}
