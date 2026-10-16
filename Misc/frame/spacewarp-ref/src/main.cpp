// spacewarp-ref: a standalone OpenXR + Vulkan reference application that submits
// analytically exact motion vectors and depth through XR_EXT_frame_synthesis or
// XR_FB_space_warp, with a controllable artificial GPU load and precise timing logs.
// See README.md for the CLI and the log/summary formats.
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "vr_openxr_synthesis_compat.hpp"
#include "xmath.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace
{
// ---------------------------------------------------------------------------
// Embedded SPIR-V (glslc -mfmt=c)
const uint32_t kSceneVert[] =
#include "scene.vert.inc"
    ;
const uint32_t kSceneFrag[] =
#include "scene.frag.inc"
    ;
const uint32_t kMotionVert[] =
#include "motion.vert.inc"
    ;
const uint32_t kMotionFrag[] =
#include "motion.frag.inc"
    ;
const uint32_t kFullscreenVert[] =
#include "fullscreen.vert.inc"
    ;
const uint32_t kLoadFrag[] =
#include "load.frag.inc"
    ;
const uint32_t kSkyMotionFrag[] =
#include "sky_motion.frag.inc"
    ;

constexpr int kExitOk = 0, kExitUsage = 2, kExitExtension = 3, kExitRuntime = 4;
constexpr uint32_t kSlots = 3;           // frames in flight (command buffers / UBOs / query sets)
constexpr uint32_t kQueriesPerSlot = 3;  // start, after colour passes, after motion passes
constexpr uint32_t kLoadChunk = 16;      // load-shader iterations per draw (keeps each draw short)
constexpr uint32_t kCalibFirstIters = 8; // first calibration probe (~1 ms per iteration at 1728x1728 on Adreno 750)
constexpr int kObjects = 3;              // static scene, moving cube, rotating cube
constexpr double kMoveAmplitude = 0.6;   // metres, moving cube x in [-A, A]
constexpr double kMoveSpeed = 0.6;       // metres / second, constant except at the two turnarounds
constexpr double kMoveDistance = 1.5;    // metres ahead of the anchor (cube centre)
constexpr double kCubeSize = 0.3;        // metres
constexpr double kRotateRate = 1.5707963267948966; // rad/s (90 deg/s)

std::atomic<int> g_signal{0};
FILE* g_log = nullptr;
const auto g_t0 = std::chrono::steady_clock::now();

double nowSec()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_t0).count();
}

void logLine(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logLine(const char* fmt, ...)
{
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
    fputc('\n', stdout);
    fflush(stdout);
    if(g_log)
    {
        fputs(buf, g_log);
        fputc('\n', g_log);
        fflush(g_log);
    }
}

// ---------------------------------------------------------------------------
// Options
enum class Api
{
    Ext,
    Fb,
    None
};

struct Options
{
    Api api{Api::Ext};
    double seconds{15}, warmup{3}, loadMs{0};
    long loadIters{-1};
    double scale{1.0}, motionResScale{1.0};
    bool mv2d{false}, mvCamera{true}, mvFovCur{false}, relaxed{false}, reversedZ{false}, srgbMotion{false};
    double mvGain{1.0}, motionRange{0.125};
    int skipEvery{0};
    int traceViews{0};
    std::string dumpMotion;
    double dumpAfter{3.0};
    std::string logPath{"spacewarp-ref.log"}, summaryPath{"spacewarp-ref-summary.json"};
    double focusTimeout{30};
    std::string space{"stage"};
    double nearZ{0.05}, farZ{100.0};
};

const char* apiName(Api a) { return a == Api::Ext ? "ext" : a == Api::Fb ? "fb" : "none"; }

void usage()
{
    fprintf(stderr,
        "usage: spacewarp-ref [options]\n"
        "  --api ext|fb|none          synthesis API (default ext = XR_EXT_frame_synthesis)\n"
        "  --seconds S                run time after the session first becomes FOCUSED (default 15)\n"
        "  --warmup-seconds W         first W of those seconds excluded from the summary (default 3)\n"
        "  --load-ms N                auto-calibrate artificial GPU load to ~N ms app GPU time/frame (default 0)\n"
        "  --load-iters K             fixed load: K loop iterations per pixel per eye (overrides --load-ms)\n"
        "  --scale F                  colour eye size = recommended * F (default 1.0)\n"
        "  --motion-res-scale F       motion/depth size = recommended motion size * F (default 1.0)\n"
        "  --mv-2d                    EXT: set XR_FRAME_SYNTHESIS_INFO_USE_2D_MOTION_VECTOR_BIT_EXT (z = 0)\n"
        "  --mv-camera on|off         include head/eye motion in the vectors (default on)\n"
        "  --mv-fov prev|cur          previous-frame projection: the FOV rendered last frame (prev, default,\n"
        "                             includes the runtime's per-frame FOV jitter) or this frame's FOV (cur)\n"
        "  --mv-gain G                multiply submitted vectors by G (default 1 = exact)\n"
        "  --relaxed                  EXT: set REQUEST_RELAXED_FRAME_INTERVAL bit\n"
        "  --reversed-z               reversed depth (near=1, far=0; nearZ>farZ in the submission)\n"
        "  --near M --far M           clip distances in metres (default 0.05 / 100)\n"
        "  --synthesis-skip-every N   every Nth frame: EXT omits the struct, FB sets FRAME_SKIP\n"
        "  --srgb-motion              force the sRGB8 motion encoding (Quake's experimental path)\n"
        "  --motion-range R           sRGB8 encoding positive range in NDC (default 0.125)\n"
        "  --dump-motion PREFIX       write eye-0 motion/depth + analysis after --dump-after seconds\n"
        "  --dump-after S             (default 3, seconds after FOCUSED)\n"
        "  --trace-views N            log the raw xrLocateViews result (full precision) for the first N rendered frames\n"
        "  --space stage|local        app reference space (default stage, falls back to local)\n"
        "  --focus-timeout S          give up (exit 4) if not FOCUSED within S seconds (default 30)\n"
        "  --log FILE                 per-second log (default ./spacewarp-ref.log)\n"
        "  --summary FILE             JSON summary (default ./spacewarp-ref-summary.json)\n");
}

bool parseArgs(int argc, char** argv, Options& o)
{
    auto need = [&](int& i) -> const char* {
        if(i + 1 >= argc)
        {
            fprintf(stderr, "missing value for %s\n", argv[i]);
            return nullptr;
        }
        return argv[++i];
    };
    auto num = [](const char* s, double& out) {
        char* end = nullptr;
        out = strtod(s, &end);
        return end && *end == 0 && std::isfinite(out);
    };
    for(int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const char* v = nullptr;
        double d = 0;
        if(a == "--help" || a == "-h")
            return false;
        else if(a == "--api")
        {
            if(!(v = need(i))) return false;
            if(!strcmp(v, "ext")) o.api = Api::Ext;
            else if(!strcmp(v, "fb")) o.api = Api::Fb;
            else if(!strcmp(v, "none")) o.api = Api::None;
            else return false;
        }
        else if(a == "--mv-camera")
        {
            if(!(v = need(i))) return false;
            if(!strcmp(v, "on")) o.mvCamera = true;
            else if(!strcmp(v, "off")) o.mvCamera = false;
            else return false;
        }
        else if(a == "--mv-fov")
        {
            if(!(v = need(i))) return false;
            if(!strcmp(v, "prev")) o.mvFovCur = false;
            else if(!strcmp(v, "cur")) o.mvFovCur = true;
            else return false;
        }
        else if(a == "--space")
        {
            if(!(v = need(i))) return false;
            o.space = v;
            if(o.space != "stage" && o.space != "local") return false;
        }
        else if(a == "--mv-2d") o.mv2d = true;
        else if(a == "--relaxed") o.relaxed = true;
        else if(a == "--reversed-z") o.reversedZ = true;
        else if(a == "--srgb-motion") o.srgbMotion = true;
        else if(a == "--dump-motion")
        {
            if(!(v = need(i))) return false;
            o.dumpMotion = v;
        }
        else if(a == "--log")
        {
            if(!(v = need(i))) return false;
            o.logPath = v;
        }
        else if(a == "--summary")
        {
            if(!(v = need(i))) return false;
            o.summaryPath = v;
        }
        else
        {
            static const char* numeric[] = {"--seconds", "--warmup-seconds", "--load-ms", "--load-iters", "--scale",
                "--motion-res-scale", "--mv-gain", "--motion-range", "--synthesis-skip-every", "--dump-after",
                "--focus-timeout", "--near", "--far", "--trace-views"};
            bool known = false;
            for(const char* n : numeric) known |= a == n;
            if(!known)
            {
                fprintf(stderr, "unknown option %s\n", a.c_str());
                return false;
            }
            if(!(v = need(i)) || !num(v, d))
            {
                fprintf(stderr, "bad number for %s\n", a.c_str());
                return false;
            }
            if(a == "--seconds") o.seconds = d;
            else if(a == "--warmup-seconds") o.warmup = d;
            else if(a == "--load-ms") o.loadMs = d;
            else if(a == "--load-iters") o.loadIters = static_cast<long>(d);
            else if(a == "--scale") o.scale = d;
            else if(a == "--motion-res-scale") o.motionResScale = d;
            else if(a == "--mv-gain") o.mvGain = d;
            else if(a == "--motion-range") o.motionRange = d;
            else if(a == "--synthesis-skip-every") o.skipEvery = static_cast<int>(d);
            else if(a == "--dump-after") o.dumpAfter = d;
            else if(a == "--focus-timeout") o.focusTimeout = d;
            else if(a == "--near") o.nearZ = d;
            else if(a == "--far") o.farZ = d;
            else if(a == "--trace-views") o.traceViews = static_cast<int>(d);
        }
    }
    if(o.seconds <= 0 || o.warmup < 0 || o.warmup >= o.seconds || o.loadMs < 0 || o.scale < 0.1 || o.scale > 2.0 ||
        o.motionResScale < 0.1 || o.motionResScale > 2.0 || o.nearZ <= 0 || o.farZ <= o.nearZ || o.skipEvery < 0 ||
        o.motionRange <= 0 || o.loadIters > 100000000 || o.traceViews < 0)
    {
        fprintf(stderr, "invalid option values (need seconds > warmup >= 0, 0.1 <= scale <= 2, 0 < near < far)\n");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Helpers
const char* vkFormatName(int64_t f)
{
    switch(f)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
    case VK_FORMAT_B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "A2B10G10R10_UNORM";
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return "B10G11R11_UFLOAT";
    case VK_FORMAT_R16G16B16A16_SFLOAT: return "R16G16B16A16_SFLOAT";
    case VK_FORMAT_R16G16B16A16_UNORM: return "R16G16B16A16_UNORM";
    case VK_FORMAT_R16G16_SFLOAT: return "R16G16_SFLOAT";
    case VK_FORMAT_R16G16B16_SFLOAT: return "R16G16B16_SFLOAT";
    case VK_FORMAT_R32G32B32A32_SFLOAT: return "R32G32B32A32_SFLOAT";
    case VK_FORMAT_R32G32_SFLOAT: return "R32G32_SFLOAT";
    case VK_FORMAT_R32G32B32_SFLOAT: return "R32G32B32_SFLOAT";
    case VK_FORMAT_R16_SFLOAT: return "R16_SFLOAT";
    case VK_FORMAT_R32_SFLOAT: return "R32_SFLOAT";
    case VK_FORMAT_R8G8_UNORM: return "R8G8_UNORM";
    case VK_FORMAT_D16_UNORM: return "D16_UNORM";
    case VK_FORMAT_D32_SFLOAT: return "D32_SFLOAT";
    case VK_FORMAT_D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return "D32_SFLOAT_S8_UINT";
    case VK_FORMAT_X8_D24_UNORM_PACK32: return "X8_D24_UNORM";
    default: return "?";
    }
}

bool isDepthFormat(int64_t f)
{
    return f == VK_FORMAT_D16_UNORM || f == VK_FORMAT_D32_SFLOAT || f == VK_FORMAT_D24_UNORM_S8_UINT ||
        f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_X8_D24_UNORM_PACK32;
}
bool hasStencil(int64_t f) { return f == VK_FORMAT_D24_UNORM_S8_UINT || f == VK_FORMAT_D32_SFLOAT_S8_UINT; }

float halfToFloat(uint16_t h)
{
    const uint32_t sign = (h >> 15) & 1, exp = (h >> 10) & 0x1f, mant = h & 0x3ff;
    float v;
    if(exp == 0) v = std::ldexp(static_cast<float>(mant), -24);
    else if(exp == 31) v = mant ? NAN : INFINITY;
    else v = std::ldexp(static_cast<float>(mant | 0x400), static_cast<int>(exp) - 25);
    return sign ? -v : v;
}

double srgbDecode(double e) { return e <= 0.04045 ? e / 12.92 : std::pow((e + 0.055) / 1.055, 2.4); }

struct Stats
{
    double avg{0}, p50{0}, p95{0}, p99{0}, max{0}, min{0};
    size_t n{0};
};
Stats stats(std::vector<double> v)
{
    Stats s;
    s.n = v.size();
    if(v.empty()) return s;
    std::sort(v.begin(), v.end());
    double sum = 0;
    for(double x : v) sum += x;
    s.avg = sum / v.size();
    auto pct = [&](double p) {
        const size_t i = std::min(v.size() - 1, static_cast<size_t>(std::ceil(p * v.size()) - 1));
        return v[i];
    };
    s.p50 = pct(0.50);
    s.p95 = pct(0.95);
    s.p99 = pct(0.99);
    s.max = v.back();
    s.min = v.front();
    return s;
}

double median(std::vector<double> v)
{
    if(v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// ---------------------------------------------------------------------------
// Scene animation (time t = seconds since the first rendered frame's display time)
double movingX(double t)
{
    const double period = 4 * kMoveAmplitude / kMoveSpeed;
    double ph = std::fmod(t, period);
    if(ph < 0) ph += period;
    return ph < period / 2 ? -kMoveAmplitude + kMoveSpeed * ph : kMoveAmplitude - kMoveSpeed * (ph - period / 2);
}
int movingLeg(double t)
{
    const double period = 4 * kMoveAmplitude / kMoveSpeed;
    return static_cast<int>(std::floor(t / (period / 2)));
}

struct Vertex
{
    float pos[3];
    float normal[3];
    float uv[2];
    float color[3];
};

struct Mesh
{
    std::vector<Vertex> v;
    std::vector<uint32_t> i;
    void quad(const xm::Vec3 p[4], xm::Vec3 n, const float uv[4][2], xm::Vec3 c)
    {
        const uint32_t base = static_cast<uint32_t>(v.size());
        for(int k = 0; k < 4; ++k)
            v.push_back({{float(p[k].x), float(p[k].y), float(p[k].z)}, {float(n.x), float(n.y), float(n.z)},
                {uv[k][0], uv[k][1]}, {float(c.x), float(c.y), float(c.z)}});
        for(uint32_t k : {0u, 1u, 2u, 0u, 2u, 3u}) i.push_back(base + k);
    }
    // Axis-aligned box; uvMetres: UV = face coordinates in metres, else 0..1 per face.
    void box(xm::Vec3 c, xm::Vec3 h, xm::Vec3 col, bool uvMetres)
    {
        struct Face
        {
            xm::Vec3 n, u, w; // normal, face u axis, face v axis
        };
        const Face faces[6] = {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
            {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}, {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
            {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}};
        for(const Face& f : faces)
        {
            const double hn = std::abs(xm::dot(f.n, h)), hu = std::abs(xm::dot(f.u, h)), hw = std::abs(xm::dot(f.w, h));
            const xm::Vec3 fc = c + xm::Vec3{f.n.x * hn, f.n.y * hn, f.n.z * hn};
            const xm::Vec3 p[4] = {fc - f.u * hu - f.w * hw, fc + f.u * hu - f.w * hw, fc + f.u * hu + f.w * hw,
                fc - f.u * hu + f.w * hw};
            const float su = uvMetres ? float(2 * hu) : 1.f, sv = uvMetres ? float(2 * hw) : 1.f;
            const float uv[4][2] = {{0, 0}, {su, 0}, {su, sv}, {0, sv}};
            quad(p, f.n, uv, col);
        }
    }
};

// std140 mirror of shaders/common.glsl
struct GpuDraw
{
    float mvp[16];
    float prevMvp[16];
    float model[16];
    float params[4];
};
struct GpuFrame
{
    float skyReproj[2][16];
    float eyeParams[2][4];
    GpuDraw draws[8];
};
static_assert(sizeof(GpuDraw) == 208, "std140 Draw");
static_assert(sizeof(GpuFrame) == 160 + 8 * 208, "std140 Frame");

struct PushConstants
{
    uint32_t drawIndex;
    uint32_t iters;
    float gain;
    uint32_t flags;
    float encScale[4];
    float encOffset[4];
};
static_assert(sizeof(PushConstants) == 48, "push constants");

struct Swapchain
{
    XrSwapchain handle{XR_NULL_HANDLE};
    int64_t format{0};
    int32_t w{0}, h{0};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    uint32_t acquired{0};
    bool isAcquired{false};
};

struct Slot
{
    VkCommandBuffer cmd{VK_NULL_HANDLE};
    VkFence fence{VK_NULL_HANDLE};
    bool pending{false};
    VkBuffer ubo{VK_NULL_HANDLE};
    VkDeviceMemory uboMem{VK_NULL_HANDLE};
    void* uboPtr{nullptr};
    VkDescriptorSet set{VK_NULL_HANDLE};
    double frameT{0};    // wall time of the xrWaitFrame return of the frame recorded here
    uint32_t iters{0};   // load iterations used in that frame
    bool motion{false};  // motion passes recorded
    bool measuring{false};
};

struct FrameRec
{
    double t;         // wall time of xrWaitFrame return (s since start)
    double frameMs;   // since the previous xrWaitFrame return
    double waitMs;    // time blocked inside xrWaitFrame
    double cpuMs;     // xrWaitFrame return -> xrEndFrame return (all frame work)
    double swapWaitMs;// part of cpuMs spent in xrWaitSwapchainImage
    double endMs;     // part of cpuMs spent in xrEndFrame
    double periodMs;  // predictedDisplayPeriod
    double dispDtMs;  // predictedDisplayTime delta to the previous frame
    bool rendered, synth, focused;
};
struct GpuRec
{
    double t; // frame's xrWaitFrame return time
    double totalMs, motionMs;
    uint32_t iters;
};

struct EyeHistory
{
    xm::Mat4 vp, vpRot, view;
};

// ---------------------------------------------------------------------------
class App
{
public:
    explicit App(const Options& o) : opt(o) {}
    int run();

private:
    // Fatal error helpers
    bool fail(int code, const std::string& reason)
    {
        if(exitCode == kExitOk)
        {
            exitCode = code;
            exitReason = reason;
        }
        logLine("ERROR %s (exit %d)", reason.c_str(), code);
        return false;
    }
    bool xrCheck(XrResult r, const char* what)
    {
        if(XR_SUCCEEDED(r)) return true;
        char name[XR_MAX_RESULT_STRING_SIZE] = "";
        if(instance) xrResultToString(instance, r, name);
        logLine("ERROR %s failed: %s (%d)", what, name, static_cast<int>(r));
        return false;
    }
    bool vkCheck(VkResult r, const char* what)
    {
        if(r == VK_SUCCESS) return true;
        logLine("ERROR %s failed: VkResult %d", what, static_cast<int>(r));
        return false;
    }

    bool initXr();
    bool initVulkan();
    bool initSession();
    bool createSwapchains();
    bool createSwapchain(Swapchain& sc, int64_t format, int w, int h, XrSwapchainUsageFlags usage, const char* what);
    bool initGpuResources();
    uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags props);
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf,
        VkDeviceMemory& mem, void** mapped);
    bool createImage(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect,
        VkImage& img, VkDeviceMemory& mem, VkImageView& view);
    VkShaderModule shaderModule(const uint32_t* code, size_t bytes);
    bool createPipelines();
    void pollEvents();
    bool frame();
    void readGpuResults(Slot& s, bool wait);
    void calibrationSample(const GpuRec& g);
    void perSecondLog(double now);
    void analyzeDump();
    void writeSummary();
    void teardown();
    void computeScene(double t, xm::Mat4 models[kObjects]);
    void anchorAt(XrTime time);

    Options opt;
    int exitCode{kExitOk};
    std::string exitReason{"running"};

    // XR
    XrInstance instance{XR_NULL_HANDLE};
    XrSystemId system{XR_NULL_SYSTEM_ID};
    XrSession session{XR_NULL_HANDLE};
    XrSpace appSpace{XR_NULL_HANDLE}, viewSpace{XR_NULL_HANDLE};
    XrReferenceSpaceType spaceType{XR_REFERENCE_SPACE_TYPE_LOCAL};
    XrViewConfigurationView configViews[2]{};
    XrFrameSynthesisConfigViewEXT synthConfig[2]{};
    XrSystemSpaceWarpPropertiesFB fbProps{XR_TYPE_SYSTEM_SPACE_WARP_PROPERTIES_FB};
    std::string runtimeName, systemName;
    bool hasExt{false}, hasFb{false};
    XrSessionState state{XR_SESSION_STATE_UNKNOWN};
    bool sessionRunning{false}, quit{false}, exitRequested{false};
    double exitRequestedAt{-1};
    std::vector<std::string> statesSeen;
    int focusLost{0};

    // Vulkan
    VkInstance vkInst{VK_NULL_HANDLE};
    VkPhysicalDevice phys{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    uint32_t queueFamily{0};
    VkQueue queue{VK_NULL_HANDLE};
    VkPhysicalDeviceMemoryProperties memProps{};
    double timestampPeriodNs{1};
    uint64_t timestampMask{~0ull};
    std::string gpuName;

    // Swapchains / targets
    Swapchain color[2], motion[2], depth[2];
    int64_t colorFormat{0}, motionFormat{0}, depthFormat{0};
    bool srgbEncoding{false};
    float encScale[4]{1, 1, 1, 1}, encOffset[4]{0, 0, 0, 0};
    VkImage eyeDepth[2]{};
    VkDeviceMemory eyeDepthMem[2]{};
    VkImageView eyeDepthView[2]{};
    std::vector<VkFramebuffer> colorFbs[2];  // per colour image
    std::vector<VkFramebuffer> motionFbs[2]; // per (motion image, depth image) pair
    VkRenderPass colorPass{VK_NULL_HANDLE}, motionPass{VK_NULL_HANDLE};
    VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
    VkPipelineLayout pipeLayout{VK_NULL_HANDLE};
    VkPipeline scenePipe{VK_NULL_HANDLE}, loadPipe{VK_NULL_HANDLE}, motionPipe{VK_NULL_HANDLE}, skyPipe{VK_NULL_HANDLE};
    VkDescriptorPool descPool{VK_NULL_HANDLE};
    VkCommandPool cmdPool{VK_NULL_HANDLE};
    VkQueryPool queryPool{VK_NULL_HANDLE};
    VkBuffer vbuf{VK_NULL_HANDLE}, ibuf{VK_NULL_HANDLE};
    VkDeviceMemory vmem{VK_NULL_HANDLE}, imem{VK_NULL_HANDLE};
    uint32_t staticIndexCount{0}, cubeIndexCount{0}, cubeFirstIndex{0};
    VkImage noiseImg{VK_NULL_HANDLE};
    VkDeviceMemory noiseMem{VK_NULL_HANDLE};
    VkImageView noiseView{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
    Slot slots[kSlots];
    uint64_t frameIndex{0};
    bool useMotion{false};

    // Scene state
    bool anchored{false};
    xm::Mat4 appFromWorld;
    double eyeHeight{1.6};
    XrTime t0Display{0};
    bool hasHistory{false};
    EyeHistory history[2];
    double historyT{0};
    XrTime lastDisplayTime{0};
    int tracedViews{0};
    bool posesValidLogged{false};
    XrViewStateFlags lastViewFlags{~0ull};

    // Load
    uint32_t loadIters{0};
    struct
    {
        bool active{false}, done{false};
        double start{-1};
        int phase{0};
        uint32_t testIters{0};
        std::vector<double> samples;
        double base{0};
        int rounds{0};
    } calib;

    // Stats
    std::vector<FrameRec> frames;
    std::vector<GpuRec> gpu;
    size_t logFrameIdx{0}, logGpuIdx{0};
    double lastLogT{-1};
    double lastWaitReturn{-1};
    double focusT{-1};
    double measureStart{-1}, measureEnd{-1};
    uint64_t rejectedEndFrames{0};

    // Dump
    struct
    {
        bool requested{false}, recorded{false}, done{false};
        uint32_t slot{0};
        VkBuffer motionBuf{VK_NULL_HANDLE}, depthBuf{VK_NULL_HANDLE};
        VkDeviceMemory motionMem{VK_NULL_HANDLE}, depthMem{VK_NULL_HANDLE};
        void *motionPtr{nullptr}, *depthPtr{nullptr};
        uint32_t motionBpp{0};
        xm::Mat4 vp, prevVp, vpRot, prevVpRot, view, proj;
        double tCur{0}, tPrev{0};
        double focusAge{0};
    } dump;
};

// ---------------------------------------------------------------------------
bool App::initXr()
{
    uint32_t count = 0;
    if(!xrCheck(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "xrEnumerateInstanceExtensionProperties"))
        return fail(kExitRuntime, "openxr_loader_or_runtime_unavailable");
    std::vector<XrExtensionProperties> props(count, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, count, &count, props.data());
    bool vk2 = false;
    std::string list;
    for(const auto& p : props)
    {
        list += std::string(list.empty() ? "" : " ") + p.extensionName + "/v" + std::to_string(p.extensionVersion);
        vk2 |= !strcmp(p.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
        hasExt |= !strcmp(p.extensionName, XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME);
        hasFb |= !strcmp(p.extensionName, XR_FB_SPACE_WARP_EXTENSION_NAME);
    }
    logLine("INFO runtime_extensions count=%u %s", count, list.c_str());
    logLine("INFO extension_offered XR_EXT_frame_synthesis=%d XR_FB_space_warp=%d XR_KHR_vulkan_enable2=%d", hasExt, hasFb, vk2);
    if(!vk2) return fail(kExitExtension, "XR_KHR_vulkan_enable2_missing");
    std::vector<const char*> exts{XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    if(opt.api == Api::Ext)
    {
        if(!hasExt) return fail(kExitExtension, "XR_EXT_frame_synthesis_not_offered");
        exts.push_back(XR_EXT_FRAME_SYNTHESIS_EXTENSION_NAME);
    }
    else if(opt.api == Api::Fb)
    {
        if(!hasFb) return fail(kExitExtension, "XR_FB_space_warp_not_offered");
        exts.push_back(XR_FB_SPACE_WARP_EXTENSION_NAME);
    }
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    snprintf(ci.applicationInfo.applicationName, sizeof(ci.applicationInfo.applicationName), "spacewarp-ref");
    ci.applicationInfo.applicationVersion = 1;
    snprintf(ci.applicationInfo.engineName, sizeof(ci.applicationInfo.engineName), "spacewarp-ref");
    ci.applicationInfo.engineVersion = 1;
    ci.applicationInfo.apiVersion = XR_API_VERSION_1_0;
    ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    ci.enabledExtensionNames = exts.data();
    const XrResult r = xrCreateInstance(&ci, &instance);
    if(!xrCheck(r, "xrCreateInstance"))
        return fail(r == XR_ERROR_EXTENSION_NOT_PRESENT ? kExitExtension : kExitRuntime, "xrCreateInstance_failed");
    XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES};
    xrGetInstanceProperties(instance, &ip);
    char rn[160];
    snprintf(rn, sizeof(rn), "%s %u.%u.%u", ip.runtimeName, XR_VERSION_MAJOR(ip.runtimeVersion),
        XR_VERSION_MINOR(ip.runtimeVersion), XR_VERSION_PATCH(ip.runtimeVersion));
    runtimeName = rn;
    logLine("INFO runtime=\"%s\" enabled_extensions=%s%s", rn, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME,
        opt.api == Api::Ext ? ",XR_EXT_frame_synthesis" : opt.api == Api::Fb ? ",XR_FB_space_warp" : "");

    XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
    si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrResult sr = XR_ERROR_RUNTIME_FAILURE;
    const double until = nowSec() + 10.0;
    while(true)
    {
        sr = xrGetSystem(instance, &si, &system);
        if(sr != XR_ERROR_FORM_FACTOR_UNAVAILABLE || nowSec() > until || g_signal) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(250)); // HMD not yet available: poll (not frame sync)
    }
    if(!xrCheck(sr, "xrGetSystem")) return fail(kExitRuntime, "xrGetSystem_failed");

    XrSystemProperties sp{XR_TYPE_SYSTEM_PROPERTIES};
    if(opt.api == Api::Fb) sp.next = &fbProps;
    if(!xrCheck(xrGetSystemProperties(instance, system, &sp), "xrGetSystemProperties"))
        return fail(kExitRuntime, "xrGetSystemProperties_failed");
    systemName = sp.systemName;
    logLine("INFO system=\"%s\" vendor=0x%x max_swapchain=%ux%u max_layers=%u orientation_tracking=%d position_tracking=%d",
        sp.systemName, sp.vendorId, sp.graphicsProperties.maxSwapchainImageWidth,
        sp.graphicsProperties.maxSwapchainImageHeight, sp.graphicsProperties.maxLayerCount,
        sp.trackingProperties.orientationTracking, sp.trackingProperties.positionTracking);
    if(opt.api == Api::Fb)
        logLine("INFO fb_space_warp_properties recommended_motion=%ux%u", fbProps.recommendedMotionVectorImageRectWidth,
            fbProps.recommendedMotionVectorImageRectHeight);

    uint32_t viewCount = 0;
    if(!xrCheck(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr),
           "xrEnumerateViewConfigurationViews") || viewCount != 2)
        return fail(kExitRuntime, "stereo_view_configuration_unavailable");
    for(int e = 0; e < 2; ++e)
    {
        configViews[e] = {XR_TYPE_VIEW_CONFIGURATION_VIEW};
        synthConfig[e] = {};
        synthConfig[e].type = XR_TYPE_FRAME_SYNTHESIS_CONFIG_VIEW_EXT;
        if(opt.api == Api::Ext) configViews[e].next = &synthConfig[e];
    }
    if(!xrCheck(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewCount, configViews),
           "xrEnumerateViewConfigurationViews"))
        return fail(kExitRuntime, "xrEnumerateViewConfigurationViews_failed");
    for(int e = 0; e < 2; ++e)
        logLine("INFO view%d recommended=%ux%u max=%ux%u samples=%u synthesis_recommended_motion=%ux%u", e,
            configViews[e].recommendedImageRectWidth, configViews[e].recommendedImageRectHeight,
            configViews[e].maxImageRectWidth, configViews[e].maxImageRectHeight,
            configViews[e].recommendedSwapchainSampleCount, synthConfig[e].recommendedMotionVectorImageRectWidth,
            synthConfig[e].recommendedMotionVectorImageRectHeight);
    return true;
}

bool App::initVulkan()
{
    PFN_xrGetVulkanGraphicsRequirements2KHR getReq = nullptr;
    PFN_xrCreateVulkanInstanceKHR createInst = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR getDev = nullptr;
    PFN_xrCreateVulkanDeviceKHR createDev = nullptr;
    xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&getReq));
    xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&createInst));
    xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR", reinterpret_cast<PFN_xrVoidFunction*>(&getDev));
    xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&createDev));
    if(!getReq || !createInst || !getDev || !createDev) return fail(kExitRuntime, "vulkan_enable2_entry_points_missing");
    XrGraphicsRequirementsVulkan2KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    if(!xrCheck(getReq(instance, system, &req), "xrGetVulkanGraphicsRequirements2KHR"))
        return fail(kExitRuntime, "vulkan_requirements_failed");
    const uint32_t api = VK_API_VERSION_1_1;
    logLine("INFO vulkan_requirements min=%u.%u max=%u.%u using=1.1", XR_VERSION_MAJOR(req.minApiVersionSupported),
        XR_VERSION_MINOR(req.minApiVersionSupported), XR_VERSION_MAJOR(req.maxApiVersionSupported),
        XR_VERSION_MINOR(req.maxApiVersionSupported));

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "spacewarp-ref";
    app.apiVersion = api;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    XrVulkanInstanceCreateInfoKHR xci{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xci.systemId = system;
    xci.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xci.vulkanCreateInfo = &ici;
    VkResult vr = VK_ERROR_INITIALIZATION_FAILED;
    if(!xrCheck(createInst(instance, &xci, &vkInst, &vr), "xrCreateVulkanInstanceKHR") || !vkCheck(vr, "vkCreateInstance"))
        return fail(kExitRuntime, "vulkan_instance_failed");
    XrVulkanGraphicsDeviceGetInfoKHR gdi{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    gdi.systemId = system;
    gdi.vulkanInstance = vkInst;
    if(!xrCheck(getDev(instance, &gdi, &phys), "xrGetVulkanGraphicsDevice2KHR") || !phys)
        return fail(kExitRuntime, "vulkan_physical_device_failed");
    VkPhysicalDeviceProperties pp{};
    vkGetPhysicalDeviceProperties(phys, &pp);
    gpuName = pp.deviceName;
    timestampPeriodNs = pp.limits.timestampPeriod;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    uint32_t qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qn);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qn, qf.data());
    queueFamily = ~0u;
    for(uint32_t i = 0; i < qn; ++i)
        if(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamily = i;
            break;
        }
    if(queueFamily == ~0u) return fail(kExitRuntime, "no_graphics_queue");
    const uint32_t validBits = qf[queueFamily].timestampValidBits;
    timestampMask = validBits >= 64 ? ~0ull : ((1ull << validBits) - 1);
    logLine("INFO gpu=\"%s\" driver=0x%x vk_api=%u.%u.%u timestamp_period_ns=%.4f timestamp_valid_bits=%u queue_family=%u",
        pp.deviceName, pp.driverVersion, VK_VERSION_MAJOR(pp.apiVersion), VK_VERSION_MINOR(pp.apiVersion),
        VK_VERSION_PATCH(pp.apiVersion), timestampPeriodNs, validBits, queueFamily);
    if(!validBits) logLine("WARN timestamps unsupported on this queue: gpu_ms will be 0");

    const float prio = 1.f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    XrVulkanDeviceCreateInfoKHR xdi{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xdi.systemId = system;
    xdi.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xdi.vulkanPhysicalDevice = phys;
    xdi.vulkanCreateInfo = &dci;
    vr = VK_ERROR_INITIALIZATION_FAILED;
    if(!xrCheck(createDev(instance, &xdi, &device, &vr), "xrCreateVulkanDeviceKHR") || !vkCheck(vr, "vkCreateDevice"))
        return fail(kExitRuntime, "vulkan_device_failed");
    vkGetDeviceQueue(device, queueFamily, 0, &queue);
    return true;
}

bool App::initSession()
{
    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    binding.instance = vkInst;
    binding.physicalDevice = phys;
    binding.device = device;
    binding.queueFamilyIndex = queueFamily;
    binding.queueIndex = 0;
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = system;
    if(!xrCheck(xrCreateSession(instance, &sci, &session), "xrCreateSession")) return fail(kExitRuntime, "xrCreateSession_failed");

    uint32_t n = 0;
    xrEnumerateReferenceSpaces(session, 0, &n, nullptr);
    std::vector<XrReferenceSpaceType> spaces(n);
    xrEnumerateReferenceSpaces(session, n, &n, spaces.data());
    bool stage = false;
    for(auto s : spaces) stage |= s == XR_REFERENCE_SPACE_TYPE_STAGE;
    spaceType = opt.space == "stage" && stage ? XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL;
    XrReferenceSpaceCreateInfo rci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rci.referenceSpaceType = spaceType;
    rci.poseInReferenceSpace.orientation.w = 1;
    if(!xrCheck(xrCreateReferenceSpace(session, &rci, &appSpace), "xrCreateReferenceSpace(app)"))
        return fail(kExitRuntime, "reference_space_failed");
    rci.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    if(!xrCheck(xrCreateReferenceSpace(session, &rci, &viewSpace), "xrCreateReferenceSpace(view)"))
        return fail(kExitRuntime, "view_space_failed");
    logLine("INFO app_space=%s (stage_offered=%d requested=%s)", spaceType == XR_REFERENCE_SPACE_TYPE_STAGE ? "STAGE" : "LOCAL",
        stage, opt.space.c_str());
    return true;
}

bool App::createSwapchain(Swapchain& sc, int64_t format, int w, int h, XrSwapchainUsageFlags usage, const char* what)
{
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = usage;
    ci.format = format;
    ci.sampleCount = 1;
    ci.width = static_cast<uint32_t>(w);
    ci.height = static_cast<uint32_t>(h);
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1;
    if(!xrCheck(xrCreateSwapchain(session, &ci, &sc.handle), what)) return false;
    sc.format = format;
    sc.w = w;
    sc.h = h;
    uint32_t n = 0;
    xrEnumerateSwapchainImages(sc.handle, 0, &n, nullptr);
    std::vector<XrSwapchainImageVulkan2KHR> imgs(n, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    if(!xrCheck(xrEnumerateSwapchainImages(sc.handle, n, &n, reinterpret_cast<XrSwapchainImageBaseHeader*>(imgs.data())),
           "xrEnumerateSwapchainImages"))
        return false;
    const bool isDepth = isDepthFormat(format);
    for(auto& im : imgs)
    {
        sc.images.push_back(im.image);
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = im.image;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = static_cast<VkFormat>(format);
        vi.subresourceRange.aspectMask = isDepth ? (VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil(format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0))
                                                 : VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        VkImageView view = VK_NULL_HANDLE;
        if(!vkCheck(vkCreateImageView(device, &vi, nullptr, &view), "vkCreateImageView")) return false;
        sc.views.push_back(view);
    }
    logLine("INFO swapchain %s format=%s(%" PRId64 ") size=%dx%d images=%u usage=0x%" PRIx64, what, vkFormatName(format), format, w,
        h, n, static_cast<uint64_t>(usage));
    return true;
}

bool App::createSwapchains()
{
    uint32_t n = 0;
    xrEnumerateSwapchainFormats(session, 0, &n, nullptr);
    std::vector<int64_t> formats(n);
    xrEnumerateSwapchainFormats(session, n, &n, formats.data());
    std::string list;
    for(int64_t f : formats) list += std::string(list.empty() ? "" : " ") + vkFormatName(f) + "(" + std::to_string(f) + ")";
    logLine("INFO swapchain_formats_offered %s", list.c_str());
    auto offered = [&](int64_t f) { return std::find(formats.begin(), formats.end(), f) != formats.end(); };

    for(int64_t f : {VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM})
        if(!colorFormat && offered(f)) colorFormat = f;
    if(!colorFormat) return fail(kExitRuntime, "no_rgba8_color_format");
    if(colorFormat == VK_FORMAT_R8G8B8A8_UNORM || colorFormat == VK_FORMAT_B8G8R8A8_UNORM)
        logLine("WARN colour swapchain is UNORM, not sRGB");

    const int ew = std::max(64, static_cast<int>(std::lround(configViews[0].recommendedImageRectWidth * opt.scale)));
    const int eh = std::max(64, static_cast<int>(std::lround(configViews[0].recommendedImageRectHeight * opt.scale)));
    const int cw = std::min<int>(ew, configViews[0].maxImageRectWidth), ch = std::min<int>(eh, configViews[0].maxImageRectHeight);
    for(int e = 0; e < 2; ++e)
        if(!createSwapchain(color[e], colorFormat, cw, ch,
               XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT, e ? "color1" : "color0"))
            return fail(kExitRuntime, "color_swapchain_failed");

    useMotion = opt.api != Api::None;
    if(!useMotion) return true;

    // Motion: Quake's Vulkan (interop) path uses RGBA16F + D32; GL path prefers RGBA16F, then RG16F, then 32F.
    if(!opt.srgbMotion)
        for(int64_t f : {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT})
            if(!motionFormat && offered(f)) motionFormat = f;
    if(!motionFormat)
    {
        if(offered(VK_FORMAT_R8G8B8A8_SRGB))
        {
            motionFormat = VK_FORMAT_R8G8B8A8_SRGB;
            srgbEncoding = true;
        }
        else return fail(kExitExtension, "no_supported_motion_vector_format");
    }
    for(int64_t f : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D16_UNORM})
        if(!depthFormat && offered(f)) depthFormat = f;
    if(!depthFormat) return fail(kExitExtension, "no_supported_depth_format");
    if(motionFormat == VK_FORMAT_R16G16_SFLOAT && !opt.mv2d && opt.api == Api::Ext)
        logLine("WARN motion format has no blue channel: z motion is lost; consider --mv-2d");

    if(srgbEncoding)
    {
        // Quake's sRGB8 encoding (vr_synthesis_math.hpp): zero anchored to byte 188.
        const double sampleZero = 0.502886458;
        const double s = opt.motionRange / (1.0 - sampleZero);
        for(int k = 0; k < 3; ++k)
        {
            encScale[k] = static_cast<float>(s);
            encOffset[k] = static_cast<float>(-sampleZero * s);
        }
        logLine("INFO motion_encoding=sRGB8 range=+%.4f/-%.4f NDC scale=%.6f offset=%.6f (quantized, clipped)", opt.motionRange,
            sampleZero * s, s, -sampleZero * s);
    }
    else
        logLine("INFO motion_encoding=float scale=1 offset=0");

    uint32_t rw = 0, rh = 0;
    if(opt.api == Api::Ext)
    {
        rw = synthConfig[0].recommendedMotionVectorImageRectWidth;
        rh = synthConfig[0].recommendedMotionVectorImageRectHeight;
    }
    else
    {
        rw = fbProps.recommendedMotionVectorImageRectWidth;
        rh = fbProps.recommendedMotionVectorImageRectHeight;
    }
    if(!rw || !rh)
    {
        logLine("WARN runtime recommended motion size is %ux%u; using the colour size", rw, rh);
        rw = static_cast<uint32_t>(cw);
        rh = static_cast<uint32_t>(ch);
    }
    const int mw = std::max(32, static_cast<int>(std::lround(rw * opt.motionResScale)));
    const int mh = std::max(32, static_cast<int>(std::lround(rh * opt.motionResScale)));
    const XrSwapchainUsageFlags extra = opt.dumpMotion.empty() ? 0 : XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
    for(int e = 0; e < 2; ++e)
    {
        if(!createSwapchain(motion[e], motionFormat, mw, mh,
               XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | extra, e ? "motion1" : "motion0") ||
            !createSwapchain(depth[e], depthFormat, mw, mh,
                XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT | extra, e ? "depth1" : "depth0"))
            return fail(kExitRuntime, "synthesis_swapchain_failed");
    }
    return true;
}

uint32_t App::memoryType(uint32_t bits, VkMemoryPropertyFlags props)
{
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if((bits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) return i;
    return ~0u;
}

bool App::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf,
    VkDeviceMemory& mem, void** mapped)
{
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    if(!vkCheck(vkCreateBuffer(device, &bi, nullptr, &buf), "vkCreateBuffer")) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buf, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memoryType(req.memoryTypeBits, props);
    if(ai.memoryTypeIndex == ~0u) return false;
    if(!vkCheck(vkAllocateMemory(device, &ai, nullptr, &mem), "vkAllocateMemory")) return false;
    vkBindBufferMemory(device, buf, mem, 0);
    if(mapped && !vkCheck(vkMapMemory(device, mem, 0, VK_WHOLE_SIZE, 0, mapped), "vkMapMemory")) return false;
    return true;
}

bool App::createImage(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImage& img,
    VkDeviceMemory& mem, VkImageView& view)
{
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = fmt;
    ci.extent = {w, h, 1};
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = usage;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if(!vkCheck(vkCreateImage(device, &ci, nullptr, &img), "vkCreateImage")) return false;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, img, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(ai.memoryTypeIndex == ~0u) ai.memoryTypeIndex = memoryType(req.memoryTypeBits, 0);
    if(!vkCheck(vkAllocateMemory(device, &ai, nullptr, &mem), "vkAllocateMemory(image)")) return false;
    vkBindImageMemory(device, img, mem, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = fmt;
    vi.subresourceRange = {aspect, 0, 1, 0, 1};
    return vkCheck(vkCreateImageView(device, &vi, nullptr, &view), "vkCreateImageView");
}

VkShaderModule App::shaderModule(const uint32_t* code, size_t bytes)
{
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes;
    ci.pCode = code;
    VkShaderModule m = VK_NULL_HANDLE;
    vkCreateShaderModule(device, &ci, nullptr, &m);
    return m;
}

bool App::createPipelines()
{
    // Render passes. Swapchain images are acquired in COLOR_ATTACHMENT_OPTIMAL / DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    // and must be released in the same layout (XR_KHR_vulkan_enable2); everything is cleared, so start UNDEFINED.
    auto makePass = [&](VkFormat colorFmt, VkFormat depthFmt, bool storeDepth, VkRenderPass& rp) {
        VkAttachmentDescription att[2]{};
        att[0].format = colorFmt;
        att[0].samples = VK_SAMPLE_COUNT_1_BIT;
        att[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        att[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att[1].format = depthFmt;
        att[1].samples = VK_SAMPLE_COUNT_1_BIT;
        att[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att[1].storeOp = storeDepth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att[1].stencilStoreOp = storeDepth ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference cref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference dref{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &cref;
        sub.pDepthStencilAttachment = &dref;
        VkSubpassDependency dep[2]{};
        dep[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dep[0].dstSubpass = 0;
        dep[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        dep[1].srcSubpass = 0;
        dep[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dep[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
        dep[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        ci.attachmentCount = 2;
        ci.pAttachments = att;
        ci.subpassCount = 1;
        ci.pSubpasses = &sub;
        ci.dependencyCount = 2;
        ci.pDependencies = dep;
        return vkCheck(vkCreateRenderPass(device, &ci, nullptr, &rp), "vkCreateRenderPass");
    };
    if(!makePass(static_cast<VkFormat>(colorFormat), VK_FORMAT_D32_SFLOAT, false, colorPass)) return false;
    if(useMotion && !makePass(static_cast<VkFormat>(motionFormat), static_cast<VkFormat>(depthFormat), true, motionPass))
        return false;

    VkDescriptorSetLayoutBinding b[2]{};
    b[0].binding = 0;
    b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b[0].descriptorCount = 1;
    b[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    b[1].binding = 1;
    b[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b[1].descriptorCount = 1;
    b[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dl.bindingCount = 2;
    dl.pBindings = b;
    if(!vkCheck(vkCreateDescriptorSetLayout(device, &dl, nullptr, &setLayout), "vkCreateDescriptorSetLayout")) return false;
    VkPushConstantRange pcr{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &setLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;
    if(!vkCheck(vkCreatePipelineLayout(device, &pl, nullptr, &pipeLayout), "vkCreatePipelineLayout")) return false;

    VkShaderModule sv = shaderModule(kSceneVert, sizeof(kSceneVert)), sf = shaderModule(kSceneFrag, sizeof(kSceneFrag));
    VkShaderModule mv = shaderModule(kMotionVert, sizeof(kMotionVert)), mf = shaderModule(kMotionFrag, sizeof(kMotionFrag));
    VkShaderModule fv = shaderModule(kFullscreenVert, sizeof(kFullscreenVert)), lf = shaderModule(kLoadFrag, sizeof(kLoadFrag));
    VkShaderModule kf = shaderModule(kSkyMotionFrag, sizeof(kSkyMotionFrag));
    if(!sv || !sf || !mv || !mf || !fv || !lf || !kf) return fail(kExitRuntime, "shader_module_failed");

    auto makePipe = [&](VkShaderModule vs, VkShaderModule fs, bool meshInput, bool depthTest, VkRenderPass rp, VkPipeline& out) {
        VkPipelineShaderStageCreateInfo st[2]{};
        st[0].sType = st[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        st[0].module = vs;
        st[0].pName = "main";
        st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        st[1].module = fs;
        st[1].pName = "main";
        VkVertexInputBindingDescription vb{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription va[4] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)}, {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
            {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}};
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        if(meshInput)
        {
            vi.vertexBindingDescriptionCount = 1;
            vi.pVertexBindingDescriptions = &vb;
            vi.vertexAttributeDescriptionCount = vs == mv ? 1 : 4;
            vi.pVertexAttributeDescriptions = va;
        }
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vps{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vps.viewportCount = 1;
        vps.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rs.lineWidth = 1.f;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable = depthTest;
        ds.depthWriteEnable = depthTest;
        ds.depthCompareOp = depthTest ? (opt.reversedZ ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS) : VK_COMPARE_OP_ALWAYS;
        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = 0xf;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = 1;
        cb.pAttachments = &cba;
        VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dy.dynamicStateCount = 2;
        dy.pDynamicStates = dyn;
        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        ci.stageCount = 2;
        ci.pStages = st;
        ci.pVertexInputState = &vi;
        ci.pInputAssemblyState = &ia;
        ci.pViewportState = &vps;
        ci.pRasterizationState = &rs;
        ci.pMultisampleState = &ms;
        ci.pDepthStencilState = &ds;
        ci.pColorBlendState = &cb;
        ci.pDynamicState = &dy;
        ci.layout = pipeLayout;
        ci.renderPass = rp;
        return vkCheck(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &out), "vkCreateGraphicsPipelines");
    };
    bool ok = makePipe(sv, sf, true, true, colorPass, scenePipe) && makePipe(fv, lf, false, false, colorPass, loadPipe);
    if(ok && useMotion) ok = makePipe(mv, mf, true, true, motionPass, motionPipe) && makePipe(fv, kf, false, false, motionPass, skyPipe);
    for(VkShaderModule m : {sv, sf, mv, mf, fv, lf, kf}) vkDestroyShaderModule(device, m, nullptr);
    return ok;
}

bool App::initGpuResources()
{
    if(!createPipelines()) return fail(kExitRuntime, "pipeline_creation_failed");
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Scene geometry (world space, metres; floor at y = 0, user looks along -Z).
    Mesh mesh;
    {
        const xm::Vec3 floorP[4] = {{-10, 0, 5}, {10, 0, 5}, {10, 0, -15}, {-10, 0, -15}};
        const float floorUv[4][2] = {{-10, 5}, {10, 5}, {10, -15}, {-10, -15}};
        mesh.quad(floorP, {0, 1, 0}, floorUv, {0.80, 0.80, 0.72});
        const xm::Vec3 wallP[4] = {{-6, 0, -5}, {6, 0, -5}, {6, 4, -5}, {-6, 4, -5}};
        const float wallUv[4][2] = {{-6, 0}, {6, 0}, {6, 4}, {-6, 4}};
        mesh.quad(wallP, {0, 0, 1}, wallUv, {0.70, 0.78, 0.92});
        for(const xm::Vec3 c : {xm::Vec3{-2.0, 1.5, -3.0}, xm::Vec3{2.0, 1.5, -3.0}, xm::Vec3{-1.3, 1.5, -4.3},
                xm::Vec3{1.3, 1.5, -4.3}})
            mesh.box(c, {0.15, 1.5, 0.15}, {0.92, 0.70, 0.48}, true);
    }
    staticIndexCount = static_cast<uint32_t>(mesh.i.size());
    cubeFirstIndex = staticIndexCount;
    const uint32_t cubeBaseVertex = static_cast<uint32_t>(mesh.v.size());
    {
        Mesh cube;
        cube.box({0, 0, 0}, {0.5, 0.5, 0.5}, {1, 1, 1}, false);
        for(uint32_t& idx : cube.i) idx += cubeBaseVertex;
        mesh.v.insert(mesh.v.end(), cube.v.begin(), cube.v.end());
        mesh.i.insert(mesh.i.end(), cube.i.begin(), cube.i.end());
        cubeIndexCount = static_cast<uint32_t>(cube.i.size());
    }
    void* p = nullptr;
    if(!createBuffer(mesh.v.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, host, vbuf, vmem, &p)) return fail(kExitRuntime, "vertex_buffer");
    memcpy(p, mesh.v.data(), mesh.v.size() * sizeof(Vertex));
    if(!createBuffer(mesh.i.size() * 4, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, host, ibuf, imem, &p)) return fail(kExitRuntime, "index_buffer");
    memcpy(p, mesh.i.data(), mesh.i.size() * 4);

    // Noise texture (256x256 RGBA8): scene detail and the load shader's dependent fetches.
    const uint32_t N = 256;
    VkBuffer staging;
    VkDeviceMemory stagingMem;
    if(!createBuffer(N * N * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, host, staging, stagingMem, &p)) return fail(kExitRuntime, "staging");
    {
        uint32_t s = 0x12345678u;
        auto* px = static_cast<uint8_t*>(p);
        for(uint32_t i = 0; i < N * N * 4; ++i)
        {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            px[i] = static_cast<uint8_t>(s >> 24);
        }
    }
    if(!createImage(N, N, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
           VK_IMAGE_ASPECT_COLOR_BIT, noiseImg, noiseMem, noiseView))
        return fail(kExitRuntime, "noise_image");
    for(int e = 0; e < 2; ++e)
        if(!createImage(static_cast<uint32_t>(color[e].w), static_cast<uint32_t>(color[e].h), VK_FORMAT_D32_SFLOAT,
               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, eyeDepth[e], eyeDepthMem[e], eyeDepthView[e]))
            return fail(kExitRuntime, "eye_depth_image");

    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpi.queueFamilyIndex = queueFamily;
    if(!vkCheck(vkCreateCommandPool(device, &cpi, nullptr, &cmdPool), "vkCreateCommandPool")) return fail(kExitRuntime, "command_pool");
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cmdPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    for(Slot& s : slots)
    {
        if(!vkCheck(vkAllocateCommandBuffers(device, &cai, &s.cmd), "vkAllocateCommandBuffers")) return fail(kExitRuntime, "cmd");
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if(!vkCheck(vkCreateFence(device, &fi, nullptr, &s.fence), "vkCreateFence")) return fail(kExitRuntime, "fence");
        if(!createBuffer(sizeof(GpuFrame), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host, s.ubo, s.uboMem, &s.uboPtr))
            return fail(kExitRuntime, "ubo");
    }

    // One-off upload of the noise texture.
    {
        VkCommandBuffer c = slots[0].cmd;
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(c, &bi);
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = noiseImg;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
        VkBufferImageCopy r{};
        r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        r.imageExtent = {N, N, 1};
        vkCmdCopyBufferToImage(c, staging, noiseImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
        vkEndCommandBuffer(c);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &c;
        if(!vkCheck(vkQueueSubmit(queue, 1, &si, slots[0].fence), "vkQueueSubmit(upload)")) return fail(kExitRuntime, "upload");
        vkWaitForFences(device, 1, &slots[0].fence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &slots[0].fence);
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
    }

    VkSamplerCreateInfo smp{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    smp.magFilter = smp.minFilter = VK_FILTER_LINEAR;
    smp.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    smp.addressModeU = smp.addressModeV = smp.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    smp.maxLod = 0;
    if(!vkCheck(vkCreateSampler(device, &smp, nullptr, &sampler), "vkCreateSampler")) return fail(kExitRuntime, "sampler");

    VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kSlots}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kSlots}};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpi.maxSets = kSlots;
    dpi.poolSizeCount = 2;
    dpi.pPoolSizes = ps;
    if(!vkCheck(vkCreateDescriptorPool(device, &dpi, nullptr, &descPool), "vkCreateDescriptorPool")) return fail(kExitRuntime, "desc_pool");
    for(Slot& s : slots)
    {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = descPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &setLayout;
        if(!vkCheck(vkAllocateDescriptorSets(device, &ai, &s.set), "vkAllocateDescriptorSets")) return fail(kExitRuntime, "desc_set");
        VkDescriptorBufferInfo bi{s.ubo, 0, sizeof(GpuFrame)};
        VkDescriptorImageInfo ii{sampler, noiseView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet w[2]{};
        w[0].sType = w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[0].dstSet = w[1].dstSet = s.set;
        w[0].dstBinding = 0;
        w[0].descriptorCount = 1;
        w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w[0].pBufferInfo = &bi;
        w[1].dstBinding = 1;
        w[1].descriptorCount = 1;
        w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[1].pImageInfo = &ii;
        vkUpdateDescriptorSets(device, 2, w, 0, nullptr);
    }

    VkQueryPoolCreateInfo qpi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpi.queryCount = kSlots * kQueriesPerSlot;
    if(!vkCheck(vkCreateQueryPool(device, &qpi, nullptr, &queryPool), "vkCreateQueryPool")) return fail(kExitRuntime, "query_pool");

    // Framebuffers
    for(int e = 0; e < 2; ++e)
    {
        for(VkImageView v : color[e].views)
        {
            VkImageView att[2] = {v, eyeDepthView[e]};
            VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fi.renderPass = colorPass;
            fi.attachmentCount = 2;
            fi.pAttachments = att;
            fi.width = static_cast<uint32_t>(color[e].w);
            fi.height = static_cast<uint32_t>(color[e].h);
            fi.layers = 1;
            VkFramebuffer fb;
            if(!vkCheck(vkCreateFramebuffer(device, &fi, nullptr, &fb), "vkCreateFramebuffer")) return fail(kExitRuntime, "fb");
            colorFbs[e].push_back(fb);
        }
        if(!useMotion) continue;
        for(VkImageView mvv : motion[e].views)
            for(VkImageView dv : depth[e].views)
            {
                VkImageView att[2] = {mvv, dv};
                VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                fi.renderPass = motionPass;
                fi.attachmentCount = 2;
                fi.pAttachments = att;
                fi.width = static_cast<uint32_t>(motion[e].w);
                fi.height = static_cast<uint32_t>(motion[e].h);
                fi.layers = 1;
                VkFramebuffer fb;
                if(!vkCheck(vkCreateFramebuffer(device, &fi, nullptr, &fb), "vkCreateFramebuffer")) return fail(kExitRuntime, "fb");
                motionFbs[e].push_back(fb);
            }
    }

    // Dump buffers
    if(!opt.dumpMotion.empty())
    {
        if(!useMotion)
            logLine("WARN --dump-motion ignored with --api none (no motion pass)");
        else
        {
            dump.requested = true;
            dump.motionBpp = motionFormat == VK_FORMAT_R16G16B16A16_SFLOAT ? 8 : motionFormat == VK_FORMAT_R16G16_SFLOAT ? 4
                : motionFormat == VK_FORMAT_R32G32B32A32_SFLOAT                                                       ? 16
                                                                                                                      : 4;
            const VkDeviceSize px = static_cast<VkDeviceSize>(motion[0].w) * motion[0].h;
            const VkMemoryPropertyFlags rb = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            if(!createBuffer(px * dump.motionBpp, VK_BUFFER_USAGE_TRANSFER_DST_BIT, rb, dump.motionBuf, dump.motionMem, &dump.motionPtr) ||
                !createBuffer(px * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, rb, dump.depthBuf, dump.depthMem, &dump.depthPtr))
                return fail(kExitRuntime, "dump_buffers");
        }
    }

    // Fixed load
    if(opt.loadIters >= 0) loadIters = static_cast<uint32_t>(opt.loadIters);
    return true;
}

// ---------------------------------------------------------------------------
void App::anchorAt(XrTime time)
{
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    const XrResult r = xrLocateSpace(viewSpace, appSpace, time, &loc);
    bool posValid = XR_SUCCEEDED(r) && (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT);
    const bool oriValid = XR_SUCCEEDED(r) && (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
    xm::Vec3 head{0, 0, 0};
    double yaw = 0;
    if(posValid) head = {loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
    // Position: the midpoint of the two rendered eye poses (what the user actually sees from). In standby the
    // SteamVR runtime reports VIEW at y = 1.002 m but the eye views at y = 0, so the VIEW-space position alone would
    // put the scene a metre above the rendered eyes.
    {
        XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
        li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        li.displayTime = time;
        li.space = appSpace;
        XrViewState vs{XR_TYPE_VIEW_STATE};
        XrView v[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
        uint32_t vc = 0;
        if(XR_SUCCEEDED(xrLocateViews(session, &li, &vs, 2, &vc, v)) && vc == 2 && (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT))
        {
            const xm::Vec3 mid{(v[0].pose.position.x + v[1].pose.position.x) * 0.5, (v[0].pose.position.y + v[1].pose.position.y) * 0.5,
                (v[0].pose.position.z + v[1].pose.position.z) * 0.5};
            logLine("INFO anchor view_space_head=(%.4f, %.4f, %.4f) valid=%d eye_midpoint=(%.4f, %.4f, %.4f) -> using eye midpoint", head.x, head.y,
                head.z, posValid, mid.x, mid.y, mid.z);
            head = mid;
            posValid = true;
        }
    }
    if(oriValid)
    {
        const xm::Quat q{loc.pose.orientation.x, loc.pose.orientation.y, loc.pose.orientation.z, loc.pose.orientation.w};
        xm::Vec3 f = xm::transformDir(xm::rotation(q), {0, 0, -1});
        if(std::hypot(f.x, f.z) < 0.2) f = xm::transformDir(xm::rotation(q), {0, 1, 0}); // looking straight up/down
        if(std::hypot(f.x, f.z) > 1e-6) yaw = std::atan2(-f.x, -f.z);
    }
    // Floor: STAGE floor (y = 0) when the head is at a plausible standing/sitting height, else 1.6 m below the head.
    double floorY = head.y - 1.6;
    if(spaceType == XR_REFERENCE_SPACE_TYPE_STAGE && posValid && head.y > 0.8 && head.y < 2.4) floorY = 0;
    eyeHeight = head.y - floorY;
    appFromWorld = xm::mul(xm::translation({head.x, floorY, head.z}), xm::rotationY(yaw));
    anchored = true;
    logLine("INFO anchor head_valid(pos=%d ori=%d) head=(%.3f, %.3f, %.3f) yaw_deg=%.2f floor_y=%.3f eye_height=%.3f "
            "(world: origin on the floor below the start head position, -Z = start view direction)",
        posValid, oriValid, head.x, head.y, head.z, yaw * 57.29577951308232, floorY, eyeHeight);
}

void App::computeScene(double t, xm::Mat4 models[kObjects])
{
    models[0] = xm::identity();
    models[1] = xm::mul(xm::translation({movingX(t), eyeHeight, -kMoveDistance}), xm::scaling({kCubeSize, kCubeSize, kCubeSize}));
    models[2] = xm::mul(xm::mul(xm::translation({0, eyeHeight - 0.45, -kMoveDistance}), xm::rotationY(kRotateRate * t)),
        xm::scaling({kCubeSize, kCubeSize, kCubeSize}));
}

void App::pollEvents()
{
    XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
    while(xrPollEvent(instance, &ev) == XR_SUCCESS)
    {
        switch(ev.type)
        {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
            const auto& e = *reinterpret_cast<XrEventDataSessionStateChanged*>(&ev);
            static const char* names[] = {"UNKNOWN", "IDLE", "READY", "SYNCHRONIZED", "VISIBLE", "FOCUSED", "STOPPING",
                "LOSS_PENDING", "EXITING"};
            const XrSessionState old = state;
            state = e.state;
            const char* name = state <= 8 ? names[state] : "?";
            statesSeen.push_back(name);
            logLine("STATE t=%.3f %s -> %s", nowSec(), old <= 8 ? names[old] : "?", name);
            if(old == XR_SESSION_STATE_FOCUSED && state != XR_SESSION_STATE_FOCUSED && !exitRequested) ++focusLost;
            if(state == XR_SESSION_STATE_READY)
            {
                XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if(xrCheck(xrBeginSession(session, &bi), "xrBeginSession")) sessionRunning = true;
                else
                {
                    fail(kExitRuntime, "xrBeginSession_failed");
                    quit = true;
                }
            }
            else if(state == XR_SESSION_STATE_FOCUSED)
            {
                if(focusT < 0)
                {
                    focusT = nowSec();
                    measureStart = focusT + opt.warmup;
                    measureEnd = focusT + opt.seconds;
                    logLine("INFO focused t=%.3f measure_window=[%.3f, %.3f]", focusT, measureStart, measureEnd);
                }
            }
            else if(state == XR_SESSION_STATE_STOPPING)
            {
                xrCheck(xrEndSession(session), "xrEndSession");
                sessionRunning = false;
            }
            else if(state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING)
            {
                if(!exitRequested) fail(kExitRuntime, state == XR_SESSION_STATE_EXITING ? "runtime_requested_exit" : "session_loss_pending");
                quit = true;
            }
            break;
        }
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            fail(kExitRuntime, "instance_loss_pending");
            quit = true;
            break;
        case XR_TYPE_EVENT_DATA_EVENTS_LOST:
            logLine("WARN events lost: %u", reinterpret_cast<XrEventDataEventsLost*>(&ev)->lostEventCount);
            break;
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
            logLine("WARN reference space change pending (type %d)",
                static_cast<int>(reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&ev)->referenceSpaceType));
            break;
        default:
            logLine("INFO event type %d", static_cast<int>(ev.type));
            break;
        }
        ev = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

void App::readGpuResults(Slot& s, bool wait)
{
    if(!s.pending) return;
    if(wait) vkWaitForFences(device, 1, &s.fence, VK_TRUE, UINT64_MAX);
    else if(vkGetFenceStatus(device, s.fence) != VK_SUCCESS) return;
    s.pending = false;
    const uint32_t idx = static_cast<uint32_t>(&s - slots);
    uint64_t q[kQueriesPerSlot]{};
    const VkResult r = vkGetQueryPoolResults(device, queryPool, idx * kQueriesPerSlot, kQueriesPerSlot, sizeof(q), q, sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT);
    if(r != VK_SUCCESS) return;
    const double ms = timestampPeriodNs * 1e-6;
    GpuRec g;
    g.t = s.frameT;
    g.totalMs = static_cast<double>((q[2] - q[0]) & timestampMask) * ms;
    g.motionMs = s.motion ? static_cast<double>((q[2] - q[1]) & timestampMask) * ms : 0;
    g.iters = s.iters;
    gpu.push_back(g);
    calibrationSample(g);
}

void App::calibrationSample(const GpuRec& g)
{
    if(!calib.active || calib.done) return;
    const uint32_t current = calib.phase == 0 ? 0 : calib.testIters;
    if(g.iters != current) return; // a frame recorded before the last change
    if(g.totalMs > 250)
    {
        loadIters = std::max<uint32_t>(1, loadIters / 4);
        calib.done = true;
        logLine("WARN calibration: GPU frame %.1f ms is too long; stopping with iters=%u", g.totalMs, loadIters);
        return;
    }
    calib.samples.push_back(g.totalMs);
    const double target = opt.loadMs;
    if(calib.phase == 0)
    {
        if(calib.samples.size() < 20) return;
        calib.base = median(std::vector<double>(calib.samples.begin() + 5, calib.samples.end()));
        calib.phase = 1;
        calib.testIters = kCalibFirstIters;
        loadIters = calib.testIters;
        calib.samples.clear();
        logLine("CALIB base_gpu_ms=%.3f (no load), trying iters=%u", calib.base, calib.testIters);
        if(calib.base >= target)
        {
            loadIters = 0;
            calib.done = true;
            logLine("CALIB target %.2f ms <= base %.3f ms: no load", target, calib.base);
        }
        return;
    }
    if(calib.samples.size() < 10) return;
    const double m = median(std::vector<double>(calib.samples.begin() + 2, calib.samples.end()));
    const double perIter = std::max(1e-7, (m - calib.base) / calib.testIters);
    double want = (target - calib.base) / perIter;
    want = std::min(want, calib.testIters * 8.0);
    want = std::max(want, 1.0);
    want = std::min(want, 100000000.0);
    ++calib.rounds;
    const double err = std::abs(m - target) / target;
    const double elapsed = nowSec() - calib.start;
    logLine("CALIB round=%d iters=%u gpu_ms=%.3f per_iter_us=%.4f -> iters=%.0f (err %.1f%%, %.2fs)", calib.rounds, calib.testIters, m,
        perIter * 1000, want, err * 100, elapsed);
    calib.samples.clear();
    if((err < 0.03 && calib.rounds >= 2) || elapsed > 2.5 || calib.rounds >= 8)
    {
        if(err >= 0.03) calib.testIters = static_cast<uint32_t>(std::lround(want)); // use the last estimate
        loadIters = calib.testIters;
        calib.done = true;
        logLine("CALIB done iters=%u target_ms=%.2f last_measured_ms=%.3f base_ms=%.3f", loadIters, target, m, calib.base);
        return;
    }
    calib.testIters = static_cast<uint32_t>(std::lround(want));
    loadIters = calib.testIters;
}

bool App::frame()
{
    XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState fs{XR_TYPE_FRAME_STATE};
    const double tw0 = nowSec();
    if(!xrCheck(xrWaitFrame(session, &wi, &fs), "xrWaitFrame")) return fail(kExitRuntime, "xrWaitFrame_failed");
    const double tw1 = nowSec();
    double cpuSwapWait = 0;
    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
    const XrResult br = xrBeginFrame(session, &bi);
    if(!xrCheck(br, "xrBeginFrame")) return fail(kExitRuntime, "xrBeginFrame_failed");
    if(br == XR_FRAME_DISCARDED) logLine("WARN xrBeginFrame returned XR_FRAME_DISCARDED");

    // Opportunistically collect finished GPU timings.
    for(Slot& s : slots) readGpuResults(s, false);

    const bool measuring = measureStart >= 0 && tw1 >= measureStart && tw1 < measureEnd;
    XrCompositionLayerProjectionView pv[2]{};
    XrFrameSynthesisInfoEXT extInfo[2]{};
    XrCompositionLayerSpaceWarpInfoFB fbInfo[2]{};
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    const XrCompositionLayerBaseHeader* layers[1] = {reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer)};
    bool rendered = false, synth = false;
    xm::Mat4 vp[2], vpRot[2], view[2], proj[2], prevVpUsed[2], prevVpRotUsed[2];
    bool dumpThisFrame = false;
    uint32_t slotIdx = static_cast<uint32_t>(frameIndex % kSlots);

    if(fs.shouldRender)
    {
        if(!anchored)
        {
            anchorAt(fs.predictedDisplayTime);
            t0Display = fs.predictedDisplayTime;
            if(opt.loadMs > 0 && opt.loadIters < 0)
            {
                calib.active = true;
                calib.start = nowSec();
                loadIters = 0;
            }
        }
        XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
        li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        li.displayTime = fs.predictedDisplayTime;
        li.space = appSpace;
        XrViewState vs{XR_TYPE_VIEW_STATE};
        XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
        uint32_t vc = 0;
        if(!xrCheck(xrLocateViews(session, &li, &vs, 2, &vc, views), "xrLocateViews")) return fail(kExitRuntime, "xrLocateViews_failed");
        if(vs.viewStateFlags != lastViewFlags)
        {
            logLine("INFO view_state_flags=0x%" PRIx64 " (orientation_valid=%d position_valid=%d tracked=%d/%d)",
                static_cast<uint64_t>(vs.viewStateFlags), !!(vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT),
                !!(vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT), !!(vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_TRACKED_BIT),
                !!(vs.viewStateFlags & XR_VIEW_STATE_POSITION_TRACKED_BIT));
            lastViewFlags = vs.viewStateFlags;
        }
        if(tracedViews < opt.traceViews)
        {
            ++tracedViews;
            for(int e = 0; e < 2; ++e)
                logLine("VIEWTRACE frame=%" PRIu64 " display_time=%" PRId64 " eye=%d flags=0x%" PRIx64
                        " pos=(%.7f, %.7f, %.7f) quat=(%.8f, %.8f, %.8f, %.8f) fov_rad=(%.8f, %.8f, %.8f, %.8f)",
                    frameIndex, static_cast<int64_t>(fs.predictedDisplayTime), e, static_cast<uint64_t>(vs.viewStateFlags),
                    views[e].pose.position.x, views[e].pose.position.y, views[e].pose.position.z, views[e].pose.orientation.x,
                    views[e].pose.orientation.y, views[e].pose.orientation.z, views[e].pose.orientation.w, views[e].fov.angleLeft,
                    views[e].fov.angleRight, views[e].fov.angleUp, views[e].fov.angleDown);
        }
        if(!posesValidLogged)
        {
            posesValidLogged = true;
            for(int e = 0; e < 2; ++e)
                logLine("INFO eye%d pose pos=(%.4f, %.4f, %.4f) quat=(%.4f, %.4f, %.4f, %.4f) fov_deg L=%.2f R=%.2f U=%.2f D=%.2f", e,
                    views[e].pose.position.x, views[e].pose.position.y, views[e].pose.position.z, views[e].pose.orientation.x,
                    views[e].pose.orientation.y, views[e].pose.orientation.z, views[e].pose.orientation.w,
                    views[e].fov.angleLeft * 57.2958, views[e].fov.angleRight * 57.2958, views[e].fov.angleUp * 57.2958,
                    views[e].fov.angleDown * 57.2958);
        }

        const double t = static_cast<double>(fs.predictedDisplayTime - t0Display) * 1e-9;
        const double tPrev = hasHistory ? historyT : t;
        xm::Mat4 models[kObjects], prevModels[kObjects];
        computeScene(t, models);
        computeScene(tPrev, prevModels);

        Slot& s = slots[slotIdx];
        readGpuResults(s, true); // also guarantees the slot's command buffer and UBO are free
        if(s.pending == false) vkResetFences(device, 1, &s.fence);
        GpuFrame* gf = static_cast<GpuFrame*>(s.uboPtr);
        for(int e = 0; e < 2; ++e)
        {
            const XrPosef& p = views[e].pose;
            const xm::Mat4 eyePose = xm::pose({p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
                {p.position.x, p.position.y, p.position.z});
            view[e] = xm::rigidInverse(eyePose);
            proj[e] = xm::projectionFov(views[e].fov.angleLeft, views[e].fov.angleRight, views[e].fov.angleUp, views[e].fov.angleDown,
                opt.nearZ, opt.farZ, opt.reversedZ);
            vp[e] = xm::mul(proj[e], view[e]);
            vpRot[e] = xm::mul(proj[e], xm::rotationPart(view[e]));
            // Previous-frame view-projection: the one actually rendered and submitted last frame (with camera)
            // or the current one (--mv-camera off: object motion only).
            // --mv-fov cur: previous pose with this frame's FOV, i.e. the runtime's per-frame FOV jitter removed.
            if(opt.mvCamera && hasHistory)
            {
                prevVpUsed[e] = opt.mvFovCur ? xm::mul(proj[e], history[e].view) : history[e].vp;
                prevVpRotUsed[e] = opt.mvFovCur ? xm::mul(proj[e], xm::rotationPart(history[e].view)) : history[e].vpRot;
            }
            else
            {
                prevVpUsed[e] = vp[e];
                prevVpRotUsed[e] = vpRot[e];
            }
            const xm::Mat4& prevVp = prevVpUsed[e];
            const xm::Mat4& prevVpRot = prevVpRotUsed[e];
            xm::Mat4 invRot;
            if(!xm::inverse(vpRot[e], invRot)) invRot = xm::identity();
            xm::toFloat(xm::mul(prevVpRot, invRot), gf->skyReproj[e]);
            for(int k = 0; k < 4; ++k) gf->eyeParams[e][k] = 0;
            for(int o = 0; o < kObjects; ++o)
            {
                GpuDraw& d = gf->draws[e * 4 + o];
                const xm::Mat4 m = xm::mul(appFromWorld, models[o]);
                const xm::Mat4 pm = xm::mul(appFromWorld, prevModels[o]);
                xm::toFloat(xm::mul(vp[e], m), d.mvp);
                xm::toFloat(xm::mul(prevVp, pm), d.prevMvp);
                xm::toFloat(m, d.model);
                d.params[0] = o == 0 ? 2.f : 4.f; // checker cells per metre (static) / per face (cubes)
                d.params[1] = static_cast<float>(o);
                d.params[2] = d.params[3] = 0;
            }
        }

        // Dump decision: eye 0 motion after --dump-after seconds of focus, with the moving cube on a straight leg and
        // its front-face centre well inside the eye-0 image, and no pose jump since the previous frame (the headset's
        // pose teleports when it enters or leaves standby).
        auto cubeInView = [&]() {
            const xm::Vec4 p{movingX(t), eyeHeight, -kMoveDistance + kCubeSize / 2, 1};
            const xm::Vec4 c = xm::mul(xm::mul(vp[0], appFromWorld), p);
            const xm::Vec4 q = xm::mul(xm::mul(prevVpUsed[0], appFromWorld), p);
            if(c.w <= 0 || q.w <= 0) return false;
            const double camShift = std::hypot(c.x / c.w - q.x / q.w, c.y / c.w - q.y / q.w);
            return std::abs(c.x / c.w) < 0.8 && std::abs(c.y / c.w) < 0.8 && camShift < 0.02;
        };
        if(dump.requested && !dump.recorded && hasHistory && focusT >= 0 && tw1 - focusT >= opt.dumpAfter &&
            movingLeg(t) == movingLeg(tPrev) && std::abs(movingX(t)) < 0.45 && cubeInView())
        {
            dumpThisFrame = true;
            dump.recorded = true;
            dump.slot = slotIdx;
            dump.vp = vp[0];
            dump.prevVp = prevVpUsed[0];
            dump.vpRot = vpRot[0];
            dump.prevVpRot = prevVpRotUsed[0];
            dump.view = view[0];
            dump.proj = proj[0];
            dump.tCur = t;
            dump.tPrev = tPrev;
            dump.focusAge = tw1 - focusT;
        }

        // Acquire images
        auto acquire = [&](Swapchain& sc) {
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            if(!xrCheck(xrAcquireSwapchainImage(sc.handle, &ai, &sc.acquired), "xrAcquireSwapchainImage")) return false;
            sc.isAcquired = true;
            XrSwapchainImageWaitInfo wi2{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wi2.timeout = XR_INFINITE_DURATION;
            const double a = nowSec();
            const bool ok = xrCheck(xrWaitSwapchainImage(sc.handle, &wi2), "xrWaitSwapchainImage");
            cpuSwapWait += nowSec() - a;
            return ok;
        };
        for(int e = 0; e < 2; ++e)
        {
            if(!acquire(color[e])) return fail(kExitRuntime, "acquire_color_failed");
            if(useMotion && (!acquire(motion[e]) || !acquire(depth[e]))) return fail(kExitRuntime, "acquire_synthesis_failed");
        }

        // Record
        VkCommandBuffer c = s.cmd;
        vkResetCommandBuffer(c, 0);
        VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(c, &cbi);
        const uint32_t q0 = slotIdx * kQueriesPerSlot;
        vkCmdResetQueryPool(c, queryPool, q0, kQueriesPerSlot);
        vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, q0 + 0);
        vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, 0, 1, &s.set, 0, nullptr);
        const VkDeviceSize zero = 0;
        PushConstants pc{};
        pc.gain = static_cast<float>(opt.mvGain);
        pc.flags = (opt.api == Api::Ext && opt.mv2d ? 1u : 0u) | (srgbEncoding ? 2u : 0u);
        memcpy(pc.encScale, encScale, sizeof(encScale));
        memcpy(pc.encOffset, encOffset, sizeof(encOffset));
        auto viewport = [&](int w, int h) {
            VkViewport v{0, 0, float(w), float(h), 0, 1};
            VkRect2D sc{{0, 0}, {uint32_t(w), uint32_t(h)}};
            vkCmdSetViewport(c, 0, 1, &v);
            vkCmdSetScissor(c, 0, 1, &sc);
        };
        auto drawScene = [&](int e) {
            vkCmdBindVertexBuffers(c, 0, 1, &vbuf, &zero);
            vkCmdBindIndexBuffer(c, ibuf, 0, VK_INDEX_TYPE_UINT32);
            for(int o = 0; o < kObjects; ++o)
            {
                pc.drawIndex = static_cast<uint32_t>(e * 4 + o);
                vkCmdPushConstants(c, pipeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
                if(o == 0) vkCmdDrawIndexed(c, staticIndexCount, 1, 0, 0, 0);
                else vkCmdDrawIndexed(c, cubeIndexCount, 1, cubeFirstIndex, 0, 0);
            }
        };
        const float clearDepth = opt.reversedZ ? 0.f : 1.f;
        for(int e = 0; e < 2; ++e)
        {
            VkClearValue cv[2]{};
            cv[0].color = {{0.20f, 0.30f, 0.45f, 1.f}};
            cv[1].depthStencil = {clearDepth, 0};
            VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            rbi.renderPass = colorPass;
            rbi.framebuffer = colorFbs[e][color[e].acquired];
            rbi.renderArea = {{0, 0}, {uint32_t(color[e].w), uint32_t(color[e].h)}};
            rbi.clearValueCount = 2;
            rbi.pClearValues = cv;
            vkCmdBeginRenderPass(c, &rbi, VK_SUBPASS_CONTENTS_INLINE);
            viewport(color[e].w, color[e].h);
            if(loadIters > 0)
            {
                vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, loadPipe);
                for(uint32_t done = 0; done < loadIters; done += kLoadChunk)
                {
                    pc.drawIndex = static_cast<uint32_t>(e);
                    pc.iters = std::min(kLoadChunk, loadIters - done);
                    vkCmdPushConstants(c, pipeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
                    vkCmdDraw(c, 3, 1, 0, 0);
                }
                pc.iters = 0;
            }
            vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipe);
            drawScene(e);
            vkCmdEndRenderPass(c);
        }
        vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, q0 + 1);
        if(useMotion)
        {
            for(int e = 0; e < 2; ++e)
            {
                VkClearValue cv[2]{};
                // Clear to the encoding of zero motion (overwritten everywhere by the sky pass anyway).
                cv[0].color = {{-encOffset[0] / encScale[0], -encOffset[1] / encScale[1], -encOffset[2] / encScale[2], 1.f}};
                cv[1].depthStencil = {clearDepth, 0};
                VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
                rbi.renderPass = motionPass;
                rbi.framebuffer = motionFbs[e][motion[e].acquired * depth[e].images.size() + depth[e].acquired];
                rbi.renderArea = {{0, 0}, {uint32_t(motion[e].w), uint32_t(motion[e].h)}};
                rbi.clearValueCount = 2;
                rbi.pClearValues = cv;
                vkCmdBeginRenderPass(c, &rbi, VK_SUBPASS_CONTENTS_INLINE);
                viewport(motion[e].w, motion[e].h);
                vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, skyPipe);
                pc.drawIndex = static_cast<uint32_t>(e);
                vkCmdPushConstants(c, pipeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
                vkCmdDraw(c, 3, 1, 0, 0);
                vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_GRAPHICS, motionPipe);
                drawScene(e);
                vkCmdEndRenderPass(c);
                if(dumpThisFrame && e == 0)
                {
                    VkImageMemoryBarrier b[2]{};
                    for(auto& x : b)
                    {
                        x.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                        x.srcQueueFamilyIndex = x.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        x.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                        x.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    }
                    b[0].image = motion[0].images[motion[0].acquired];
                    b[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                    b[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    b[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    const VkImageAspectFlags da = VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil(depthFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
                    b[1].image = depth[0].images[depth[0].acquired];
                    b[1].subresourceRange = {da, 0, 1, 0, 1};
                    b[1].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    b[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, b);
                    VkBufferImageCopy r{};
                    r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                    r.imageExtent = {uint32_t(motion[0].w), uint32_t(motion[0].h), 1};
                    vkCmdCopyImageToBuffer(c, b[0].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dump.motionBuf, 1, &r);
                    r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    vkCmdCopyImageToBuffer(c, b[1].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dump.depthBuf, 1, &r);
                    for(auto& x : b)
                    {
                        std::swap(x.oldLayout, x.newLayout);
                        x.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                        x.dstAccessMask = 0;
                    }
                    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 2, b);
                    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    mb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                    vkCmdPipelineBarrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
                }
            }
        }
        vkCmdWriteTimestamp(c, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, q0 + 2);
        vkEndCommandBuffer(c);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &c;
        if(!vkCheck(vkQueueSubmit(queue, 1, &si, s.fence), "vkQueueSubmit")) return fail(kExitRuntime, "vkQueueSubmit_failed");
        s.pending = true;
        s.frameT = tw1;
        s.iters = loadIters;
        s.motion = useMotion;
        s.measuring = measuring;

        // Release
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        for(int e = 0; e < 2; ++e)
        {
            for(Swapchain* sc : {&color[e], &motion[e], &depth[e]})
                if(sc->isAcquired)
                {
                    if(!xrCheck(xrReleaseSwapchainImage(sc->handle, &ri), "xrReleaseSwapchainImage"))
                        return fail(kExitRuntime, "release_failed");
                    sc->isAcquired = false;
                }
        }

        // Layer
        const bool skipThis = opt.skipEvery > 0 && frameIndex % static_cast<uint64_t>(opt.skipEvery) == 0;
        const XrPosef identityPose{{0, 0, 0, 1}, {0, 0, 0}};
        for(int e = 0; e < 2; ++e)
        {
            pv[e] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            pv[e].pose = views[e].pose;
            pv[e].fov = views[e].fov;
            pv[e].subImage.swapchain = color[e].handle;
            pv[e].subImage.imageRect = {{0, 0}, {color[e].w, color[e].h}};
            pv[e].subImage.imageArrayIndex = 0;
            if(!useMotion || !hasHistory) continue;
            const XrSwapchainSubImage msub{motion[e].handle, {{0, 0}, {motion[e].w, motion[e].h}}, 0};
            const XrSwapchainSubImage dsub{depth[e].handle, {{0, 0}, {depth[e].w, depth[e].h}}, 0};
            const float nearZ = static_cast<float>(opt.reversedZ ? opt.farZ : opt.nearZ);
            const float farZ = static_cast<float>(opt.reversedZ ? opt.nearZ : opt.farZ);
            if(opt.api == Api::Ext && !skipThis)
            {
                XrFrameSynthesisInfoEXT& x = extInfo[e];
                x.type = XR_TYPE_FRAME_SYNTHESIS_INFO_EXT;
                x.next = nullptr;
                x.layerFlags = (opt.mv2d ? XR_FRAME_SYNTHESIS_INFO_USE_2D_MOTION_VECTOR_BIT_EXT : 0) |
                    (opt.relaxed ? XR_FRAME_SYNTHESIS_INFO_REQUEST_RELAXED_FRAME_INTERVAL_BIT_EXT : 0);
                x.motionVectorSubImage = msub;
                x.motionVectorScale = {encScale[0], encScale[1], encScale[2], 1.f};
                x.motionVectorOffset = {encOffset[0], encOffset[1], encOffset[2], 0.f};
                x.appSpaceDeltaPose = identityPose; // the app never moves its space (no locomotion)
                x.depthSubImage = dsub;
                x.minDepth = 0.f;
                x.maxDepth = 1.f;
                x.nearZ = nearZ;
                x.farZ = farZ;
                pv[e].next = &x;
                synth = true;
            }
            else if(opt.api == Api::Fb)
            {
                XrCompositionLayerSpaceWarpInfoFB& x = fbInfo[e];
                x.type = XR_TYPE_COMPOSITION_LAYER_SPACE_WARP_INFO_FB;
                x.next = nullptr;
                x.layerFlags = skipThis ? XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB : 0;
                x.motionVectorSubImage = msub;
                x.appSpaceDeltaPose = identityPose;
                x.depthSubImage = dsub;
                x.minDepth = 0.f;
                x.maxDepth = 1.f;
                x.nearZ = nearZ;
                x.farZ = farZ;
                pv[e].next = &x;
                synth = !skipThis;
            }
        }
        layer.space = appSpace;
        layer.viewCount = 2;
        layer.views = pv;
        rendered = true;

        // Next frame's "previous" data (committed below only if xrEndFrame accepts the frame).
        for(int e = 0; e < 2; ++e)
        {
            history[e].vp = vp[e];
            history[e].vpRot = vpRot[e];
            history[e].view = view[e];
        }
    }

    XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
    ei.displayTime = fs.predictedDisplayTime;
    ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    ei.layerCount = rendered ? 1 : 0;
    ei.layers = rendered ? layers : nullptr;
    const double te0 = nowSec();
    const XrResult er = xrEndFrame(session, &ei);
    const double te1 = nowSec();
    if(XR_FAILED(er))
    {
        xrCheck(er, "xrEndFrame");
        if(synth)
        {
            ++rejectedEndFrames;
            return fail(kExitExtension, opt.api == Api::Fb ? "xrEndFrame_rejected_XrCompositionLayerSpaceWarpInfoFB"
                                                           : "xrEndFrame_rejected_XrFrameSynthesisInfoEXT");
        }
        return fail(kExitRuntime, "xrEndFrame_failed");
    }
    if(rendered)
    {
        hasHistory = true;
        historyT = static_cast<double>(fs.predictedDisplayTime - t0Display) * 1e-9;
    }

    FrameRec fr;
    fr.t = tw1;
    fr.frameMs = lastWaitReturn >= 0 ? (tw1 - lastWaitReturn) * 1000 : 0;
    fr.waitMs = (tw1 - tw0) * 1000;
    fr.cpuMs = (te1 - tw1) * 1000;
    fr.swapWaitMs = cpuSwapWait * 1000;
    fr.endMs = (te1 - te0) * 1000;
    fr.periodMs = static_cast<double>(fs.predictedDisplayPeriod) * 1e-6;
    fr.dispDtMs = lastDisplayTime ? static_cast<double>(fs.predictedDisplayTime - lastDisplayTime) * 1e-6 : 0;
    fr.rendered = rendered;
    fr.synth = synth;
    fr.focused = state == XR_SESSION_STATE_FOCUSED;
    if(lastWaitReturn >= 0) frames.push_back(fr);
    lastWaitReturn = tw1;
    lastDisplayTime = fs.predictedDisplayTime;
    ++frameIndex;

    if(dumpThisFrame) logLine("INFO dump frame recorded (t=%.4f s, focus age %.3f s); analysed after the run", dump.tCur, dump.focusAge);
    return true;
}

void App::perSecondLog(double now)
{
    if(lastLogT < 0)
    {
        lastLogT = now;
        logFrameIdx = frames.size();
        logGpuIdx = gpu.size();
        return;
    }
    if(now - lastLogT < 1.0) return;
    std::vector<double> fm, wm, cm, sw, em, pm, dd, gm, gmm;
    int rendered = 0, synth = 0;
    for(size_t i = logFrameIdx; i < frames.size(); ++i)
    {
        const FrameRec& f = frames[i];
        fm.push_back(f.frameMs);
        wm.push_back(f.waitMs);
        cm.push_back(f.cpuMs);
        sw.push_back(f.swapWaitMs);
        em.push_back(f.endMs);
        pm.push_back(f.periodMs);
        dd.push_back(f.dispDtMs);
        rendered += f.rendered;
        synth += f.synth;
    }
    for(size_t i = logGpuIdx; i < gpu.size(); ++i)
    {
        gm.push_back(gpu[i].totalMs);
        gmm.push_back(gpu[i].motionMs);
    }
    const Stats F = stats(fm), W = stats(wm), C = stats(cm), SW = stats(sw), E = stats(em), P = stats(pm), D = stats(dd), G = stats(gm),
                GM = stats(gmm);
    static const char* names[] = {"UNKNOWN", "IDLE", "READY", "SYNCHRONIZED", "VISIBLE", "FOCUSED", "STOPPING", "LOSS_PENDING", "EXITING"};
    const bool meas = measureStart >= 0 && now > measureStart && lastLogT < measureEnd;
    logLine("SEC t=%.3f tf=%.3f state=%s frames=%zu fps=%.2f frame_avg=%.3f frame_p95=%.3f frame_max=%.3f wait_avg=%.3f "
            "cpu_avg=%.3f cpu_max=%.3f swapwait_avg=%.3f endframe_avg=%.3f gpu_avg=%.3f gpu_p95=%.3f gpu_max=%.3f gpu_motion_avg=%.3f "
            "gpu_n=%zu period=%.3f disp_dt_avg=%.3f disp_dt_max=%.3f rendered=%d synth=%d iters=%u measuring=%d",
        now, focusT >= 0 ? now - focusT : -1.0, state <= 8 ? names[state] : "?", fm.size(), fm.size() / (now - lastLogT), F.avg, F.p95,
        F.max, W.avg, C.avg, C.max, SW.avg, E.avg, G.avg, G.p95, G.max, GM.avg, gm.size(), P.avg, D.avg, D.max, rendered, synth,
        loadIters, meas ? 1 : 0);
    lastLogT = now;
    logFrameIdx = frames.size();
    logGpuIdx = gpu.size();
}

void App::analyzeDump()
{
    const int w = motion[0].w, h = motion[0].h;
    const size_t px = static_cast<size_t>(w) * h;
    std::vector<float> mv(px * 3), dep(px);
    const auto* mb = static_cast<const uint8_t*>(dump.motionPtr);
    for(size_t i = 0; i < px; ++i)
    {
        double raw[3] = {0, 0, 0};
        if(motionFormat == VK_FORMAT_R16G16B16A16_SFLOAT || motionFormat == VK_FORMAT_R16G16_SFLOAT)
        {
            const auto* hp = reinterpret_cast<const uint16_t*>(mb + i * dump.motionBpp);
            const int ch = motionFormat == VK_FORMAT_R16G16_SFLOAT ? 2 : 3;
            for(int k = 0; k < ch; ++k) raw[k] = halfToFloat(hp[k]);
        }
        else if(motionFormat == VK_FORMAT_R32G32B32A32_SFLOAT)
        {
            const auto* fp = reinterpret_cast<const float*>(mb + i * dump.motionBpp);
            for(int k = 0; k < 3; ++k) raw[k] = fp[k];
        }
        else
            for(int k = 0; k < 3; ++k) raw[k] = srgbDecode(mb[i * 4 + k] / 255.0);
        for(int k = 0; k < 3; ++k) mv[i * 3 + k] = static_cast<float>(raw[k] * encScale[k] + encOffset[k]);
        float d = 0;
        if(depthFormat == VK_FORMAT_D32_SFLOAT || depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT)
            memcpy(&d, static_cast<const uint8_t*>(dump.depthPtr) + i * 4, 4);
        else if(depthFormat == VK_FORMAT_D16_UNORM)
            d = static_cast<const uint16_t*>(dump.depthPtr)[i] / 65535.f;
        else
            d = (static_cast<const uint32_t*>(dump.depthPtr)[i] & 0xffffff) / 16777215.f;
        dep[i] = d;
    }
    const std::string prefix = opt.dumpMotion;
    if(FILE* f = fopen((prefix + ".motion.f32").c_str(), "wb"))
    {
        fwrite(mv.data(), sizeof(float), mv.size(), f);
        fclose(f);
    }
    if(FILE* f = fopen((prefix + ".depth.f32").c_str(), "wb"))
    {
        fwrite(dep.data(), sizeof(float), dep.size(), f);
        fclose(f);
    }
    // Visualisation: R/G = 128 + motion * 2540 (0.05 NDC -> full scale), B = depth.
    if(FILE* f = fopen((prefix + ".motion.ppm").c_str(), "wb"))
    {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        std::vector<uint8_t> row(static_cast<size_t>(w) * 3);
        for(int y = 0; y < h; ++y)
        {
            for(int x = 0; x < w; ++x)
            {
                const size_t i = static_cast<size_t>(y) * w + x;
                auto q = [](double v) { return static_cast<uint8_t>(std::clamp(v, 0.0, 255.0)); };
                row[x * 3 + 0] = q(128 + mv[i * 3 + 0] * 2540);
                row[x * 3 + 1] = q(128 + mv[i * 3 + 1] * 2540);
                const double lin = opt.reversedZ ? dep[i] : 1.0 - dep[i];
                row[x * 3 + 2] = q(std::pow(std::clamp(lin, 0.0, 1.0), 0.25) * 255);
            }
            fwrite(row.data(), 1, row.size(), f);
        }
        fclose(f);
    }

    // Analysis: the moving cube's face towards the user, centre point.
    auto sample = [&](xm::Vec3 ndc, int half, double out[3], double& depthOut, int& pxX, int& pxY) {
        pxX = static_cast<int>(std::floor((ndc.x * 0.5 + 0.5) * w));
        pxY = static_cast<int>(std::floor((ndc.y * 0.5 + 0.5) * h));
        std::vector<double> comp[3];
        depthOut = -1;
        for(int dy = -half; dy <= half; ++dy)
            for(int dx = -half; dx <= half; ++dx)
            {
                const int x = pxX + dx, y = pxY + dy;
                if(x < 0 || y < 0 || x >= w || y >= h) continue;
                const size_t i = static_cast<size_t>(y) * w + x;
                for(int k = 0; k < 3; ++k) comp[k].push_back(mv[i * 3 + k]);
                if(dx == 0 && dy == 0) depthOut = dep[i];
            }
        for(int k = 0; k < 3; ++k) out[k] = comp[k].empty() ? NAN : median(comp[k]);
        return !comp[0].empty();
    };
    const double dt = dump.tCur - dump.tPrev;
    const double xc = movingX(dump.tCur), xp = movingX(dump.tPrev);
    const double vel = (xc - xp) / dt;
    const xm::Vec3 faceCur{xc, eyeHeight, -kMoveDistance + kCubeSize / 2}, facePrev{xp, eyeHeight, -kMoveDistance + kCubeSize / 2};
    const xm::Mat4 vpCur = xm::mul(dump.vp, appFromWorld), vpPrev = xm::mul(dump.prevVp, appFromWorld);
    const xm::Vec3 nCur = xm::ndc(vpCur, faceCur), nPrev = xm::ndc(vpPrev, facePrev);
    const double g = opt.mvGain;
    const bool zeroZ = opt.api == Api::Ext && opt.mv2d;
    const xm::Vec3 expFull{(nCur.x - nPrev.x) * g, (nCur.y - nPrev.y) * g, zeroZ ? 0 : (nCur.z - nPrev.z) * g};
    // Simple pinhole estimate: dNDC = P00 * (velocity_view * dt) / depth (head assumed static).
    const xm::Mat4 viewFromWorld = xm::mul(dump.view, appFromWorld);
    const xm::Vec3 pView = xm::transformPoint(viewFromWorld, faceCur);
    const xm::Vec3 vView = xm::transformDir(viewFromWorld, {vel, 0, 0});
    const double depthM = -pView.z;
    const double expSimpleX = dump.proj.m[0] * vView.x * dt / depthM * g, expSimpleY = dump.proj.m[5] * vView.y * dt / depthM * g;
    double meas[3], measC[3], dC = 0, dM = 0;
    int px0, py0, px1, py1;
    const bool inside = sample(nCur, 4, meas, dM, px0, py0) && sample(nCur, 0, measC, dC, px1, py1);
    // Static reference points: the back wall and the floor (camera motion only).
    const xm::Vec3 wallP{-0.5, eyeHeight + 0.7, -5.0}, floorP{0.6, 0.0, -2.5};
    double mWall[3], mFloor[3], dW, dF;
    int wx, wy, fx, fy;
    const xm::Vec3 wCur = xm::ndc(vpCur, wallP), wPrev = xm::ndc(vpPrev, wallP);
    const xm::Vec3 fCur = xm::ndc(vpCur, floorP), fPrev = xm::ndc(vpPrev, floorP);
    const bool wallIn = sample(wCur, 2, mWall, dW, wx, wy);
    const bool floorIn = sample(fCur, 2, mFloor, dF, fx, fy);
    const double errX = std::abs(meas[0] - expFull.x), relX = std::abs(expFull.x) > 1e-9 ? errX / std::abs(expFull.x) : NAN;
    logLine("DUMP eye=0 size=%dx%d focus_age_s=%.3f dt_ms=%.4f cube_x=%.4f cube_v=%.4f m/s face_depth_m=%.4f P00=%.5f "
            "pixel=(%d,%d) in_image=%d",
        w, h, dump.focusAge, dt * 1000, xc, vel, depthM, dump.proj.m[0], px0, py0, inside);
    logLine("DUMP moving_cube expected_simple=(%.6f, %.6f) expected_full=(%.6f, %.6f, %.7f) measured_center=(%.6f, %.6f, %.7f) "
            "measured_median9x9=(%.6f, %.6f, %.7f) abs_err_x=%.2e rel_err_x=%.4f%% depth_expected=%.6f depth_measured=%.6f",
        expSimpleX, expSimpleY, expFull.x, expFull.y, expFull.z, measC[0], measC[1], measC[2], meas[0], meas[1], meas[2], errX, relX * 100,
        nCur.z, dC);
    logLine("DUMP static_wall pixel=(%d,%d) in=%d expected=(%.6f, %.6f) measured=(%.6f, %.6f)  static_floor pixel=(%d,%d) in=%d "
            "expected=(%.6f, %.6f) measured=(%.6f, %.6f)",
        wx, wy, wallIn, (wCur.x - wPrev.x) * g, (wCur.y - wPrev.y) * g, mWall[0], mWall[1], fx, fy, floorIn, (fCur.x - fPrev.x) * g,
        (fCur.y - fPrev.y) * g, mFloor[0], mFloor[1]);
    // Whole-image exactness check (eye 0): rebuild each pixel's app-space point from its pixel-centre NDC and
    // stored depth, find the object it lies on, move it to the previous time with that object's previous model,
    // project with the previous view-projection and compare with the stored vector. Sky pixels (depth == clear)
    // use the rotation-only reprojection the sky shader uses.
    struct ClassStat
    {
        const char* name;
        size_t n;
        double maxErr[3], sumSq[3], maxMag;
    };
    ClassStat cls[4] = {{"static", 0, {0, 0, 0}, {0, 0, 0}, 0}, {"moving_cube", 0, {0, 0, 0}, {0, 0, 0}, 0},
        {"rotating_cube", 0, {0, 0, 0}, {0, 0, 0}, 0}, {"sky", 0, {0, 0, 0}, {0, 0, 0}, 0}};
    {
        xm::Mat4 curModels[kObjects], prevModels[kObjects], invModel[kObjects], back[kObjects], invVp, invRot;
        computeScene(dump.tCur, curModels);
        computeScene(dump.tPrev, prevModels);
        for(int o = 0; o < kObjects; ++o)
        {
            if(!xm::inverse(xm::mul(appFromWorld, curModels[o]), invModel[o])) invModel[o] = xm::identity();
            back[o] = xm::mul(xm::mul(appFromWorld, prevModels[o]), invModel[o]);
        }
        if(!xm::inverse(dump.vp, invVp)) invVp = xm::identity();
        if(!xm::inverse(dump.vpRot, invRot)) invRot = xm::identity();
        const xm::Mat4 skyRe = xm::mul(dump.prevVpRot, invRot);
        const float clearDepth = opt.reversedZ ? 0.f : 1.f;
        const double tol = 0.5 + 0.005 / kCubeSize; // 5 mm slack in cube-local units
        for(int y = 0; y < h; ++y)
            for(int x = 0; x < w; ++x)
            {
                const size_t i = static_cast<size_t>(y) * w + x;
                const double nx = (x + 0.5) / w * 2 - 1, ny = (y + 0.5) / h * 2 - 1, d = dep[i];
                double e[3];
                int c = 0;
                if(dep[i] == clearDepth)
                {
                    c = 3;
                    const xm::Vec4 p = xm::mul(skyRe, xm::Vec4{nx, ny, 0.5, 1});
                    e[0] = nx - p.x / p.w;
                    e[1] = ny - p.y / p.w;
                    e[2] = 0;
                }
                else
                {
                    const xm::Vec4 a = xm::mul(invVp, xm::Vec4{nx, ny, d, 1});
                    const xm::Vec3 pa{a.x / a.w, a.y / a.w, a.z / a.w};
                    for(int o = 1; o < kObjects && c == 0; ++o)
                    {
                        const xm::Vec3 l = xm::transformPoint(invModel[o], pa);
                        if(std::abs(l.x) <= tol && std::abs(l.y) <= tol && std::abs(l.z) <= tol) c = o;
                    }
                    const xm::Vec3 pp = xm::transformPoint(back[c], pa);
                    const xm::Vec4 q = xm::mul(dump.prevVp, xm::Vec4{pp.x, pp.y, pp.z, 1});
                    e[0] = nx - q.x / q.w;
                    e[1] = ny - q.y / q.w;
                    e[2] = d - q.z / q.w;
                }
                for(int k = 0; k < 3; ++k) e[k] *= g;
                if(zeroZ) e[2] = 0;
                ClassStat& st = cls[c];
                ++st.n;
                st.maxMag = std::max(st.maxMag, std::hypot(e[0], e[1]));
                for(int k = 0; k < 3; ++k)
                {
                    const double diff = mv[i * 3 + k] - e[k];
                    st.maxErr[k] = std::max(st.maxErr[k], std::abs(diff));
                    st.sumSq[k] += diff * diff;
                }
            }
    }
    std::string pixelJson = "{";
    for(int c = 0; c < 4; ++c)
    {
        const ClassStat& st = cls[c];
        const double nn = static_cast<double>(std::max<size_t>(1, st.n));
        logLine("DUMP pixel_check class=%s pixels=%zu max_expected_mag=%.6f max_abs_err=(%.2e, %.2e, %.2e) rms_err=(%.2e, %.2e, %.2e)",
            st.name, st.n, st.maxMag, st.maxErr[0], st.maxErr[1], st.maxErr[2], std::sqrt(st.sumSq[0] / nn), std::sqrt(st.sumSq[1] / nn),
            std::sqrt(st.sumSq[2] / nn));
        char buf[512];
        snprintf(buf, sizeof(buf),
            "%s\"%s\": {\"pixels\": %zu, \"max_expected_mag\": %.8f, \"max_abs_err\": [%.3e, %.3e, %.3e], \"rms_err\": [%.3e, %.3e, %.3e]}",
            c ? ", " : "", st.name, st.n, st.maxMag, st.maxErr[0], st.maxErr[1], st.maxErr[2], std::sqrt(st.sumSq[0] / nn),
            std::sqrt(st.sumSq[1] / nn), std::sqrt(st.sumSq[2] / nn));
        pixelJson += buf;
    }
    pixelJson += "}";
    if(FILE* f = fopen((prefix + ".json").c_str(), "w"))
    {
        fprintf(f,
            "{\n  \"width\": %d, \"height\": %d,\n  \"motion_file\": \"%s.motion.f32\", \"motion_layout\": \"float32 LE, 3 per pixel (dx, dy, dz) "
            "decoded NDC (rgb*scale+offset), row 0 = top = Vulkan NDC y -1\",\n  \"depth_file\": \"%s.depth.f32\", \"depth_layout\": \"float32 LE window depth 0..1\",\n"
            "  \"motion_format\": \"%s\", \"depth_format\": \"%s\", \"mv_2d\": %s, \"mv_camera\": %s, \"mv_gain\": %.6f, \"reversed_z\": %s,\n"
            "  \"dt_ms\": %.6f, \"cube_x\": %.6f, \"cube_velocity_mps\": %.6f, \"face_depth_m\": %.6f, \"P00\": %.6f, \"P11\": %.6f,\n"
            "  \"cube_pixel\": [%d, %d],\n  \"expected_simple\": [%.7f, %.7f],\n  \"expected_full\": [%.7f, %.7f, %.8f],\n"
            "  \"measured_center\": [%.7f, %.7f, %.8f],\n  \"measured_median9x9\": [%.7f, %.7f, %.8f],\n  \"rel_err_x\": %.6f,\n"
            "  \"depth_expected\": %.7f, \"depth_measured\": %.7f,\n"
            "  \"wall_pixel\": [%d, %d], \"wall_expected\": [%.7f, %.7f], \"wall_measured\": [%.7f, %.7f],\n"
            "  \"floor_pixel\": [%d, %d], \"floor_expected\": [%.7f, %.7f], \"floor_measured\": [%.7f, %.7f],\n"
            "  \"pixel_check\": %s\n}\n",
            w, h, prefix.c_str(), prefix.c_str(), vkFormatName(motionFormat), vkFormatName(depthFormat), zeroZ ? "true" : "false",
            opt.mvCamera ? "true" : "false", g, opt.reversedZ ? "true" : "false", dt * 1000, xc, vel, depthM, dump.proj.m[0],
            dump.proj.m[5], px0, py0, expSimpleX, expSimpleY, expFull.x, expFull.y, expFull.z, measC[0], measC[1], measC[2], meas[0],
            meas[1], meas[2], relX, nCur.z, dC, wx, wy, (wCur.x - wPrev.x) * g, (wCur.y - wPrev.y) * g, mWall[0], mWall[1], fx, fy,
            (fCur.x - fPrev.x) * g, (fCur.y - fPrev.y) * g, mFloor[0], mFloor[1], pixelJson.c_str());
        fclose(f);
    }
    dump.done = true;
}

void App::writeSummary()
{
    std::vector<double> fm, wm, cm, sw, em, pm, dd, gm, gmm;
    size_t n = 0, rendered = 0, synth = 0, unfocused = 0;
    for(const FrameRec& f : frames)
    {
        if(measureStart < 0 || f.t < measureStart || f.t >= measureEnd) continue;
        ++n;
        fm.push_back(f.frameMs);
        wm.push_back(f.waitMs);
        cm.push_back(f.cpuMs);
        sw.push_back(f.swapWaitMs);
        em.push_back(f.endMs);
        pm.push_back(f.periodMs);
        dd.push_back(f.dispDtMs);
        rendered += f.rendered;
        synth += f.synth;
        unfocused += !f.focused;
    }
    for(const GpuRec& g : gpu)
        if(measureStart >= 0 && g.t >= measureStart && g.t < measureEnd)
        {
            gm.push_back(g.totalMs);
            gmm.push_back(g.motionMs);
        }
    double secs = 0;
    for(double x : fm) secs += x;
    secs /= 1000.0; // sum of frame intervals in the window
    const Stats F = stats(fm), W = stats(wm), C = stats(cm), SW = stats(sw), E = stats(em), P = stats(pm), D = stats(dd), G = stats(gm),
                GM = stats(gmm);
    FILE* f = fopen(opt.summaryPath.c_str(), "w");
    if(!f)
    {
        logLine("ERROR cannot write summary %s", opt.summaryPath.c_str());
        return;
    }
    auto st = [&](const char* name, const Stats& s, bool comma) {
        fprintf(f, "  \"%s\": {\"avg\": %.4f, \"p50\": %.4f, \"p95\": %.4f, \"p99\": %.4f, \"max\": %.4f, \"min\": %.4f, \"n\": %zu}%s\n", name,
            s.avg, s.p50, s.p95, s.p99, s.max, s.min, s.n, comma ? "," : "");
    };
    std::string states;
    for(const auto& s : statesSeen) states += std::string(states.empty() ? "\"" : ", \"") + s + "\"";
    fprintf(f, "{\n");
    fprintf(f, "  \"api\": \"%s\",\n  \"exit_reason\": \"%s\",\n  \"exit_code\": %d,\n", apiName(opt.api), exitReason.c_str(), exitCode);
    fprintf(f, "  \"frames\": %zu,\n  \"seconds\": %.4f,\n  \"fps\": %.3f,\n", n, secs, secs > 0 ? n / secs : 0.0);
    fprintf(f, "  \"rendered_frames\": %zu,\n  \"synthesis_frames\": %zu,\n  \"unfocused_frames\": %zu,\n", rendered, synth, unfocused);
    st("frame_ms", F, true);
    st("wait_ms", W, true);
    st("cpu_busy_ms", C, true);
    st("swapchain_wait_ms", SW, true);
    st("end_frame_ms", E, true);
    st("gpu_ms", G, true);
    st("gpu_motion_ms", GM, true);
    st("display_dt_ms", D, true);
    fprintf(f, "  \"display_period_ms\": %.4f,\n", P.avg);
    fprintf(f, "  \"load_ms_target\": %.3f,\n  \"load_iters\": %u,\n  \"load_calibrated\": %s,\n  \"load_base_gpu_ms\": %.4f,\n", opt.loadMs,
        loadIters, calib.done ? "true" : "false", calib.base);
    fprintf(f, "  \"scale\": %.4f,\n  \"eye_w\": %d,\n  \"eye_h\": %d,\n  \"motion_w\": %d,\n  \"motion_h\": %d,\n", opt.scale, color[0].w, color[0].h,
        motion[0].w, motion[0].h);
    fprintf(f, "  \"color_format\": \"%s\",\n  \"motion_format\": \"%s\",\n  \"depth_format\": \"%s\",\n  \"motion_encoding\": \"%s\",\n",
        vkFormatName(colorFormat), useMotion ? vkFormatName(motionFormat) : "none", useMotion ? vkFormatName(depthFormat) : "none",
        !useMotion ? "none" : srgbEncoding ? "srgb8" : "float");
    fprintf(f, "  \"mv_fov\": \"%s\",\n", opt.mvFovCur ? "cur" : "prev");
    fprintf(f, "  \"mv_2d\": %s,\n  \"mv_camera\": %s,\n  \"mv_gain\": %.4f,\n  \"relaxed\": %s,\n  \"reversed_z\": %s,\n  \"near_z\": %.4f,\n  \"far_z\": %.4f,\n",
        opt.mv2d ? "true" : "false", opt.mvCamera ? "true" : "false", opt.mvGain, opt.relaxed ? "true" : "false",
        opt.reversedZ ? "true" : "false", opt.nearZ, opt.farZ);
    fprintf(f, "  \"synthesis_skip_every\": %d,\n  \"space\": \"%s\",\n", opt.skipEvery,
        spaceType == XR_REFERENCE_SPACE_TYPE_STAGE ? "STAGE" : "LOCAL");
    fprintf(f, "  \"focused_at_s\": %.4f,\n  \"warmup_seconds\": %.3f,\n  \"run_seconds\": %.3f,\n  \"focus_lost\": %d,\n  \"states\": [%s],\n",
        focusT, opt.warmup, opt.seconds, focusLost, states.c_str());
    fprintf(f, "  \"rejected_end_frames\": %" PRIu64 ",\n  \"dump\": %s,\n", rejectedEndFrames, dump.done ? "true" : "false");
    fprintf(f, "  \"runtime\": \"%s\",\n  \"system\": \"%s\",\n  \"gpu\": \"%s\"\n}\n", runtimeName.c_str(), systemName.c_str(), gpuName.c_str());
    fclose(f);
    logLine("SUMMARY frames=%zu seconds=%.3f fps=%.2f frame_avg=%.3f frame_p50=%.3f frame_p95=%.3f frame_max=%.3f wait_avg=%.3f "
            "cpu_avg=%.3f gpu_avg=%.3f gpu_p95=%.3f period=%.3f synth=%zu iters=%u exit_reason=%s file=%s",
        n, secs, secs > 0 ? n / secs : 0.0, F.avg, F.p50, F.p95, F.max, W.avg, C.avg, G.avg, G.p95, P.avg, synth, loadIters,
        exitReason.c_str(), opt.summaryPath.c_str());
}

void App::teardown()
{
    if(device) vkDeviceWaitIdle(device);
    for(Slot& s : slots) readGpuResults(s, false);
    if(device)
    {
        for(int e = 0; e < 2; ++e)
        {
            for(VkFramebuffer fb : colorFbs[e]) vkDestroyFramebuffer(device, fb, nullptr);
            for(VkFramebuffer fb : motionFbs[e]) vkDestroyFramebuffer(device, fb, nullptr);
            for(Swapchain* sc : {&color[e], &motion[e], &depth[e]})
                for(VkImageView v : sc->views) vkDestroyImageView(device, v, nullptr);
            if(eyeDepthView[e]) vkDestroyImageView(device, eyeDepthView[e], nullptr);
            if(eyeDepth[e]) vkDestroyImage(device, eyeDepth[e], nullptr);
            if(eyeDepthMem[e]) vkFreeMemory(device, eyeDepthMem[e], nullptr);
        }
        for(Slot& s : slots)
        {
            if(s.fence) vkDestroyFence(device, s.fence, nullptr);
            if(s.ubo) vkDestroyBuffer(device, s.ubo, nullptr);
            if(s.uboMem) vkFreeMemory(device, s.uboMem, nullptr);
        }
        for(VkPipeline p : {scenePipe, loadPipe, motionPipe, skyPipe})
            if(p) vkDestroyPipeline(device, p, nullptr);
        if(pipeLayout) vkDestroyPipelineLayout(device, pipeLayout, nullptr);
        if(setLayout) vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        if(descPool) vkDestroyDescriptorPool(device, descPool, nullptr);
        if(colorPass) vkDestroyRenderPass(device, colorPass, nullptr);
        if(motionPass) vkDestroyRenderPass(device, motionPass, nullptr);
        if(cmdPool) vkDestroyCommandPool(device, cmdPool, nullptr);
        if(queryPool) vkDestroyQueryPool(device, queryPool, nullptr);
        if(sampler) vkDestroySampler(device, sampler, nullptr);
        if(noiseView) vkDestroyImageView(device, noiseView, nullptr);
        if(noiseImg) vkDestroyImage(device, noiseImg, nullptr);
        if(noiseMem) vkFreeMemory(device, noiseMem, nullptr);
        for(VkBuffer b : {vbuf, ibuf, dump.motionBuf, dump.depthBuf})
            if(b) vkDestroyBuffer(device, b, nullptr);
        for(VkDeviceMemory m : {vmem, imem, dump.motionMem, dump.depthMem})
            if(m) vkFreeMemory(device, m, nullptr);
    }
    for(int e = 0; e < 2; ++e)
        for(Swapchain* sc : {&color[e], &motion[e], &depth[e]})
            if(sc->handle) xrDestroySwapchain(sc->handle);
    if(viewSpace) xrDestroySpace(viewSpace);
    if(appSpace) xrDestroySpace(appSpace);
    if(session) xrDestroySession(session);
    if(device)
    {
        vkQueueWaitIdle(queue);
        vkDestroyDevice(device, nullptr);
    }
    if(vkInst) vkDestroyInstance(vkInst, nullptr);
    if(instance) xrDestroyInstance(instance);
}

int App::run()
{
    if(!getenv("XR_RUNTIME_JSON")) setenv("XR_RUNTIME_JSON", "/opt/steamvr/steamxr_linuxarm64.json", 1);
    logLine("INFO spacewarp-ref api=%s seconds=%.2f warmup=%.2f load_ms=%.2f load_iters=%ld scale=%.3f motion_res_scale=%.3f mv_2d=%d "
            "mv_camera=%d mv_fov=%s mv_gain=%.3f relaxed=%d reversed_z=%d near=%.3f far=%.1f skip_every=%d XR_RUNTIME_JSON=%s",
        apiName(opt.api), opt.seconds, opt.warmup, opt.loadMs, opt.loadIters, opt.scale, opt.motionResScale, opt.mv2d, opt.mvCamera,
        opt.mvFovCur ? "cur" : "prev", opt.mvGain, opt.relaxed, opt.reversedZ, opt.nearZ, opt.farZ, opt.skipEvery, getenv("XR_RUNTIME_JSON"));
    if(opt.mv2d && opt.api != Api::Ext) logLine("WARN --mv-2d only applies to --api ext");
    bool ok = initXr() && initVulkan() && initSession() && createSwapchains() && initGpuResources();
    if(ok)
    {
        logLine("INFO ready: eye=%dx%d color=%s motion=%dx%d %s depth=%s; waiting for READY", color[0].w, color[0].h,
            vkFormatName(colorFormat), motion[0].w, motion[0].h, useMotion ? vkFormatName(motionFormat) : "none",
            useMotion ? vkFormatName(depthFormat) : "none");
        const double start = nowSec();
        while(!quit)
        {
            pollEvents();
            if(quit) break;
            const double now = nowSec();
            if(g_signal && !exitRequested)
            {
                exitReason = "signal";
                exitRequested = true;
                exitRequestedAt = now;
                if(sessionRunning) xrRequestExitSession(session);
                else quit = true;
            }
            if(!exitRequested && focusT < 0 && now - start > opt.focusTimeout)
            {
                fail(kExitRuntime, "focus_timeout");
                exitRequested = true;
                exitRequestedAt = now;
                if(sessionRunning) xrRequestExitSession(session);
                else quit = true;
            }
            if(!exitRequested && focusT >= 0 && now >= measureEnd)
            {
                exitReason = "completed";
                exitRequested = true;
                exitRequestedAt = now;
                logLine("INFO run complete, xrRequestExitSession");
                if(!xrCheck(xrRequestExitSession(session), "xrRequestExitSession")) quit = true;
            }
            if(exitRequested && now - exitRequestedAt > 5.0)
            {
                logLine("WARN runtime did not stop the session within 5 s of the exit request; leaving");
                quit = true;
            }
            if(!sessionRunning)
            {
                // Before READY / after STOPPING: nothing to pace against, poll events at 100 Hz.
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if(!frame())
            {
                quit = true;
                break;
            }
            perSecondLog(nowSec());
        }
        // The dump analysis (~1.5 s of CPU for a 1728x1728 eye plus ~57 MB of files) runs after the loop so it never
        // stalls the frame loop; the readback buffers are written only on the dump frame.
        if(dump.recorded && !dump.done)
        {
            if(vkDeviceWaitIdle(device) == VK_SUCCESS) analyzeDump();
            else logLine("WARN vkDeviceWaitIdle failed; dump not analysed");
        }
        if(dump.requested && !dump.done) logLine("WARN --dump-motion requested but no dump was taken");
    }
    if(exitCode == kExitOk && exitReason == "running") exitReason = "unknown";
    writeSummary();
    teardown();
    logLine("INFO exit code=%d reason=%s", exitCode, exitReason.c_str());
    return exitCode;
}
} // namespace

int main(int argc, char** argv)
{
    Options o;
    if(!parseArgs(argc, argv, o))
    {
        usage();
        return kExitUsage;
    }
    g_log = fopen(o.logPath.c_str(), "w");
    signal(SIGINT, [](int) { g_signal = 1; });
    signal(SIGTERM, [](int) { g_signal = 1; });
    App app(o);
    const int rc = app.run();
    if(g_log) fclose(g_log);
    return rc;
}
