// Read-only, bounded OpenVR background client. No scene submits or settings writes.
// Build on Frame: clang++ -std=c++17 -Wall -Wextra -Werror frame-compositor-timings.cpp -ldl -o frame-compositor-timings
#include "openvr_capi.h"
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>

// Valve's C++ API packs Compositor_FrameTiming to 4 on Linux. The generated C
// header does not carry that pragma for this struct. Keep the native ABI exact.
#pragma pack(push, 4)
struct Timing {
    uint32_t size, index, presents, mispresented, dropped, flags;
    double systemTime;
    float preGpu, postGpu, totalGpu, compositorGpu, compositorCpu, idleCpu;
    float interval, presentCpu, waitPresentCpu, submitCpu, waitPoses, newPoses;
    float newFrame, updateStart, updateEnd, renderStart;
    TrackedDevicePose_t pose;
    uint32_t vsyncReady, vsyncFirst;
    float transferLatency;
};
#pragma pack(pop)
static_assert(sizeof(Timing) == 188 && offsetof(Timing, pose) == 96, "Valve Linux timing ABI");
static constexpr uint32_t motionFlag = 0x08; // Valve VRCompositor_ReprojectionMotion.
static volatile std::sig_atomic_t stopRequested = 0;

static void requestStop(int) { stopRequested = 1; }

bool serverRunning() {
    std::error_code error;
    for(const auto& entry : std::filesystem::directory_iterator("/proc", error)) {
        std::ifstream comm(entry.path() / "comm"); std::string name;
        if(std::getline(comm, name) && name == "vrserver") return true;
    }
    return false;
}

bool processAlive(uint32_t pid) {
    if(!pid) return false;
    std::error_code error;
    return std::filesystem::exists(std::filesystem::path("/proc") / std::to_string(pid), error);
}

int main(int argc, char** argv) {
    if(argc == 2 && !std::strcmp(argv[1], "--self-test")) {
        Timing t{}; t.flags = 0x04;
        if(t.flags & motionFlag) return 2;
        t.flags |= motionFlag;
        if(!(t.flags & motionFlag)) return 2;
        std::printf("PASS timing_size=%zu pose_offset=%zu async_is_not_motion=1 runtime_initialized=0\n", sizeof(t), offsetof(Timing, pose));
        return 0;
    }
    char* end = nullptr;
    const long seconds = argc == 3 ? std::strtol(argv[2], &end, 10) : 0;
    if(argc != 3 || std::strcmp(argv[1], "--run") || !end || *end || seconds < 1 || seconds > 60) {
        std::fprintf(stderr, "usage: --self-test | --run SECONDS (1..60; existing SteamVR only)\n"); return 2;
    }
    if(!serverRunning()) { std::fprintf(stderr, "REFUSED no existing vrserver; runtime will not be started\n"); return 2; }
    std::signal(SIGINT, requestStop);
    std::signal(SIGTERM, requestStop);
    void* library = dlopen("/opt/steamvr/bin/linuxarm64/libopenvr_api.so", RTLD_NOW | RTLD_LOCAL);
    if(!library) { std::fprintf(stderr, "FAIL dlopen: %s\n", dlerror()); return 2; }
    using Init = uint32_t (*)(EVRInitError*, EVRApplicationType);
    using Shutdown = void (*)();
    using GetInterface = void* (*)(const char*, EVRInitError*);
    const auto init = reinterpret_cast<Init>(dlsym(library, "VR_InitInternal"));
    const auto shutdown = reinterpret_cast<Shutdown>(dlsym(library, "VR_ShutdownInternal"));
    const auto getInterface = reinterpret_cast<GetInterface>(dlsym(library, "VR_GetGenericInterface"));
    if(!init || !shutdown || !getInterface) { std::fprintf(stderr, "FAIL OpenVR exports absent\n"); dlclose(library); return 2; }
    EVRInitError error = EVRInitError_VRInitError_None;
    init(&error, EVRApplicationType_VRApplication_Background);
    if(error != EVRInitError_VRInitError_None) { std::fprintf(stderr, "FAIL background initialization=%d\n", error); dlclose(library); return 2; }
    const std::string version = std::string("FnTable:") + IVRCompositor_Version;
    auto* compositor = reinterpret_cast<VR_IVRCompositor_FnTable*>(getInterface(version.c_str(), &error));
    if(!compositor || error != EVRInitError_VRInitError_None) {
        std::fprintf(stderr, "FAIL compositor interface=%d\n", error); shutdown(); dlclose(library); return 2;
    }
    const uint32_t focus = compositor->GetCurrentSceneFocusProcess();
    const uint32_t renderer = compositor->GetLastFrameRenderer();
    std::printf("{\"type\":\"start\",\"interface\":\"%s\",\"seconds\":%ld,\"timing_size\":%zu,\"focus_pid\":%u,\"renderer_pid\":%u,\"smoothing_enabled\":%s,\"smoothing_supported\":%s}\n",
        IVRCompositor_Version, seconds, sizeof(Timing), focus, renderer,
        compositor->IsMotionSmoothingEnabled() ? "true" : "false", compositor->IsMotionSmoothingSupported() ? "true" : "false");
    Timing timings[128]{};
    std::map<std::pair<double, uint32_t>, Timing> captured;
    double baseline = 0;
    bool first = true, sceneChanged = false, sceneChangedWhileAppAlive = false;
    unsigned polls = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    do {
        std::memset(timings, 0, sizeof(timings));
        for(auto& t : timings) t.size = sizeof(Timing);
        const unsigned count = compositor->GetFrameTimings(reinterpret_cast<Compositor_FrameTiming*>(timings), 128);
        if(count > 128) { std::fprintf(stderr, "FAIL invalid timing count\n"); shutdown(); dlclose(library); return 2; }
        ++polls;
        const bool focusOrRendererChanged = compositor->GetCurrentSceneFocusProcess() != focus ||
                                            compositor->GetLastFrameRenderer() != renderer;
        sceneChanged |= focusOrRendererChanged;
        // Returning to the dashboard after Quake exits is expected. A focus or
        // renderer change while the original focused process is still alive
        // invalidates attribution and must fail the measurement.
        sceneChangedWhileAppAlive |= focusOrRendererChanged && processAlive(focus);
        for(unsigned i = 0; i < count; ++i) {
            const auto& t = timings[i];
            if(first) { if(t.systemTime > baseline) baseline = t.systemTime; }
            else if(t.systemTime > baseline) captured[{t.systemTime, t.index}] = t;
        }
        first = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while(!stopRequested && std::chrono::steady_clock::now() < deadline);
    unsigned motion = 0, validPoses = 0;
    for(const auto& entry : captured) {
        const auto& t = entry.second;
        motion += (t.flags & motionFlag) != 0; validPoses += t.pose.bPoseIsValid;
        std::printf("{\"type\":\"frame\",\"frame_index\":%u,\"system_seconds\":%.9f,\"flags\":%u,\"motion_triggered\":%s,\"presents\":%u,\"mispresented\":%u,\"dropped\":%u,\"pose_valid\":%s,\"pre_gpu_ms\":%.6f,\"total_gpu_ms\":%.6f,\"interval_ms\":%.6f,\"transfer_ms\":%.6f}\n",
            t.index, t.systemTime, t.flags, t.flags & motionFlag ? "true" : "false", t.presents, t.mispresented, t.dropped,
            t.pose.bPoseIsValid ? "true" : "false", t.preGpu, t.totalGpu, t.interval, t.transferLatency);
    }
    const char* stopReason = sceneChangedWhileAppAlive ? "scene_changed_while_app_alive" :
                             stopRequested ? "runner_finished" : sceneChanged ? "scene_changed_after_app_exit" : "duration_limit";
    std::printf("{\"type\":\"summary\",\"polls\":%u,\"unique_app_frames\":%zu,\"motion_flag_frames\":%u,\"valid_pose_frames\":%u,\"scene_changed\":%s,\"scene_changed_while_app_alive\":%s,\"stop_reason\":\"%s\",\"ext_input_use_proven\":false}\n",
        polls, captured.size(), motion, validPoses, sceneChanged ? "true" : "false",
        sceneChangedWhileAppAlive ? "true" : "false", stopReason);
    shutdown(); // Only this background client, never SteamVR itself.
    dlclose(library);
    return sceneChangedWhileAppAlive ? 3 : stopRequested ? 0 : 4;
}
