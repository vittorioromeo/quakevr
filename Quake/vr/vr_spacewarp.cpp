// Application SpaceWarp (vr_spacewarp.hpp): each eye's motion vectors and depth for the runtime (XR_FB_space_warp).

#include "vr_spacewarp.hpp"
#include "vr_glutil.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_testkit.hpp"
#include "vr_units.hpp"
#include "vr_api_render.h"
#include "vr_api.h"

#include <glm/gtc/quaternion.hpp>

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

namespace qvr::spacewarp
{
namespace
{

// The eye's pass: one fragment an output texel, the scene's depth and motion at the texel's place (nearest).
constexpr const char* passFs = R"(#version 430
layout(binding = 0) uniform sampler2D SceneDepth;
layout(binding = 1) uniform sampler2D Motion; // xy uv a frame ago minus uv now, z the depth it was written at, w the stamp
layout(location = 0) uniform mat4 InvViewProj;
layout(location = 1) uniform mat4 PrevViewProj;
layout(location = 2) uniform vec4 Params; // x the eye's stamp, y 1: reversed Z, zw the eye's drawn size (texels)
layout(location = 3) uniform vec4 Planes; // xy the near and far planes, zw the output's size
layout(location = 4) uniform float ObjectMark; // the alpha of a model's own motion (the test's 2; the runtime's 1)
layout(location = 0) out vec4 OutMotion;
float GlZ(float w) { return (Planes.y + Planes.x) / (Planes.y - Planes.x) - 2.0 * Planes.y * Planes.x / ((Planes.y - Planes.x) * w); }
void main()
{
    bool reversed = Params.y > 0.5;
    ivec2 size = ivec2(Params.zw);
    ivec2 q = clamp(ivec2(gl_FragCoord.xy / Planes.zw * Params.zw), ivec2(0), size - 1);
    float d = texelFetch(SceneDepth, q, 0).r;
    if (reversed ? d >= 1.0 : d <= 0.0) // the lenses' hidden area (at the near plane): nothing there, far away
    {
        gl_FragDepth = reversed ? 0.0 : 1.0;
        OutMotion = vec4(0.0);
        return;
    }
    gl_FragDepth = d;
    vec2 uv = (vec2(q) + 0.5) / Params.zw;
    vec3 ndc = vec3(uv * 2.0 - 1.0, reversed ? d : d * 2.0 - 1.0);
    vec4 h = InvViewProj * vec4(ndc, 1.0);
    vec4 c = PrevViewProj * vec4(h.xyz / h.w, 1.0);
    // The camera's motion (head and locomotion), z as OpenGL's NDC depth (Meta's GLES sample's convention)
    vec3 mv = vec3(ndc.xy - c.xy / c.w, GlZ(1.0 / h.w) - GlZ(c.w));
    float mark = 1.0;
    vec4 m = texelFetch(Motion, q, 0);
    if (abs(floor(m.w) - Params.x) < 0.5 && abs(m.z - d) <= max(abs(d) * 2e-5, 1.5e-7))
    {
        mv = vec3(-2.0 * m.xy, 0.0); // a model's own (uv a frame ago minus uv now: NDC now minus then, depth unknown)
        mark = ObjectMark;
    }
    OutMotion = vec4(mv, mark);
}
)";

// A runtime image pair's framebuffer (Meta's samples: one per swapchain image, made once, kept attached).
struct Fbo
{
    GLuint motion{0};
    GLuint depth{0};
    GLuint fbo{0};
};
constexpr int fboSlots = 12;

// A rigid transform (rotation, then translation): world from something, in OpenXR's axes (x right, y up, z back) and
// metres.
struct Rigid
{
    glm::mat3 r{1.f};
    glm::vec3 t{0.f};
};

// vr_spacewarp_test's numbers: the last left eye's images read back.
struct Stats
{
    int frames{0};       // eye passes with images (both eyes)
    int skipped{0};      // frames marked skipped (jumps)
    int camera{0};       // texels of the camera's motion
    int object{0};       // texels of a model's own motion
    int hidden{0};       // the hidden area's
    float cameraMean{0.f}; // mean |xy| (NDC)
    float objectMean{0.f};
    float objectMax{0.f};
    float depthMin{1.f}, depthMax{0.f}; // over the texels not hidden
    glm::vec3 deltaT{0.f};  // the last frame's play-space delta: metres
    float deltaYaw{0.f};    // ... degrees about the up axis
    int width{0}, height{0};
};

struct State
{
    bool frameOn{false};
    bool failed{false};
    GLuint program{0};
    za::Array<Fbo, fboSlots> fbos{};
    // vr_spacewarp_test's own images (the eye's size over 4)
    GLuint testMotion[2]{0, 0};
    GLuint testDepth[2]{0, 0};
    int testWidth{0}, testHeight{0};
    // The frame's information: the play space in the world (eye 0's pass), the last frame's, the planes, a jump
    Rigid worldFromApp;
    Rigid prevWorldFromApp;
    bool havePrev{false};
    bool havePose{false};
    bool jump{false};
    float nearZ{0.f}, farZ{0.f};
    bool eyesDone[2]{false, false};
    bool wasOn{false};
    Stats stats;
    testkit::Tally tally;
    int skippedAtStill{0}; // (vr_spacewarp_check jumped: a skip since `still`)
    za::Vector<float> readback;
};
State state;

[[nodiscard]] GLuint compile(GLenum type, const char* source, const char* name)
{
    const GLuint shader = GL_CreateShaderFunc(type);
    VR_GLShaderSource(shader, 1, &source); // (-glescompat: as ES 3.2 GLSL)
    GL_CompileShaderFunc(shader);
    if(!gfx::compiled(shader, name))
    {
        GL_DeleteShaderFunc(shader);
        return 0;
    }
    return shader;
}

[[nodiscard]] bool ensureProgram()
{
    if(state.program || state.failed)
    {
        return state.program != 0;
    }
    const GLuint v = compile(GL_VERTEX_SHADER, gfx::fullscreenTriangleVs, "vr spacewarp");
    const GLuint f = v ? compile(GL_FRAGMENT_SHADER, passFs, "vr spacewarp") : 0;
    if(!v || !f)
    {
        if(v)
        {
            GL_DeleteShaderFunc(v);
        }
        state.failed = true;
        Con_Warning("VR: SpaceWarp's motion vector pass did not compile: SpaceWarp off\n");
        return false;
    }
    state.program = gfx::linkProgram(v, f, "vr spacewarp");
    state.failed = state.program == 0;
    return state.program != 0;
}

void deleteFbos()
{
    for(Fbo& f : state.fbos)
    {
        if(f.fbo)
        {
            GL_DeleteFramebuffersFunc(1, &f.fbo);
        }
        f = Fbo{};
    }
}

// The framebuffer of a motion and depth image pair (made the first time; all made again when the slots run out).
[[nodiscard]] GLuint fboFor(GLuint motion, GLuint depth)
{
    for(const Fbo& f : state.fbos)
    {
        if(f.fbo && f.motion == motion && f.depth == depth)
        {
            return f.fbo;
        }
    }
    Fbo* free = nullptr;
    for(Fbo& f : state.fbos)
    {
        free = !free && !f.fbo ? &f : free;
    }
    if(!free)
    {
        deleteFbos();
        free = &state.fbos[0];
    }
    free->motion = motion;
    free->depth = depth;
    GL_GenFramebuffersFunc(1, &free->fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, free->fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, motion, 0);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    const GLenum status = GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    {
        Con_Warning("VR: SpaceWarp's framebuffer (motion %u, depth %u) incomplete: 0x%x\n", motion, depth, status);
    }
    return free->fbo;
}

void deleteTestImages()
{
    for(int eye = 0; eye < 2; eye++)
    {
        if(state.testMotion[eye])
        {
            GL_DeleteNativeTexture(state.testMotion[eye]);
            GL_DeleteNativeTexture(state.testDepth[eye]);
        }
        state.testMotion[eye] = state.testDepth[eye] = 0;
    }
    state.testWidth = state.testHeight = 0;
}

[[nodiscard]] GLuint makeTestTexture(GLenum format, int width, int height, const char* label)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, format, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    GL_ObjectLabelFunc(GL_TEXTURE, tex, -1, label);
    return tex;
}

// vr_spacewarp_test: the eye's own images, a quarter of the eye's size each way.
[[nodiscard]] Backend::SpaceWarpImages testImages(int eye, int drawnWidth, int drawnHeight)
{
    const int w = za::max(16, drawnWidth / 4), h = za::max(16, drawnHeight / 4);
    if(state.testWidth != w || state.testHeight != h)
    {
        deleteFbos();
        deleteTestImages();
        for(int e = 0; e < 2; e++)
        {
            state.testMotion[e] = makeTestTexture(GL_RGBA16F, w, h, "vr spacewarp test motion");
            state.testDepth[e] = makeTestTexture(GL_DEPTH24_STENCIL8, w, h, "vr spacewarp test depth");
        }
        state.testWidth = w;
        state.testHeight = h;
    }
    Backend::SpaceWarpImages out;
    out.motion = state.testMotion[eye];
    out.depth = state.testDepth[eye];
    out.width = w;
    out.height = h;
    return out;
}

// OpenXR's axes from Quake's (x forward, y left, z up): x right, y up, z back.
[[nodiscard]] glm::vec3 fromQuake(const float* v)
{
    return {-v[1], v[2], -v[0]};
}

[[nodiscard]] Rigid inverse(const Rigid& a)
{
    Rigid out;
    out.r = glm::transpose(a.r);
    out.t = -(out.r * a.t);
    return out;
}

[[nodiscard]] Rigid compose(const Rigid& a, const Rigid& b) // a after b
{
    Rigid out;
    out.r = a.r * b.r;
    out.t = a.r * b.t + a.t;
    return out;
}

// The play space in the world, from the eye's place in the world (Quake's, in metres) and in the play space (the
// runtime's pose): world-from-app = world-from-eye * inverse(app-from-eye).
[[nodiscard]] Rigid worldFromApp(const float* origin, const float* angles, const Pose& eyeInApp)
{
    vec3_t forward, right, up;
    vec3_t a = {angles[0], angles[1], angles[2]};
    AngleVectors(a, forward, right, up);
    const float back[3] = {-forward[0], -forward[1], -forward[2]};
    Rigid worldFromEye;
    worldFromEye.r = glm::mat3(fromQuake(right), fromQuake(up), fromQuake(back));
    worldFromEye.t = fromQuake(origin) / units::metresToUnits();
    Rigid appFromEye;
    appFromEye.r = glm::mat3_cast(eyeInApp.orientation);
    appFromEye.t = eyeInApp.position;
    return compose(worldFromEye, inverse(appFromEye));
}

// vr_spacewarp_test: the left eye's images read back into the numbers vr_spacewarp_check reads.
void readStats(GLuint fbo, int w, int h, bool reversed)
{
    Stats& s = state.stats;
    s.width = w;
    s.height = h;
    const za::SizeT n = static_cast<za::SizeT>(w) * static_cast<za::SizeT>(h);
    state.readback.resize(n * 5);
    float* rgba = state.readback.data();
    float* depth = rgba + n * 4;
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, rgba);
    glReadPixels(0, 0, w, h, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
    s.camera = s.object = s.hidden = 0;
    double camSum = 0.0, objSum = 0.0;
    s.objectMax = 0.f;
    s.depthMin = 1.f;
    s.depthMax = 0.f;
    for(za::SizeT i = 0; i < n; i++)
    {
        const float* p = rgba + i * 4;
        const float len = za::sqrt(p[0] * p[0] + p[1] * p[1]);
        if(p[3] < 0.5f)
        {
            s.hidden++;
            continue;
        }
        s.depthMin = za::min(s.depthMin, depth[i]);
        s.depthMax = za::max(s.depthMax, depth[i]);
        if(p[3] > 1.5f)
        {
            s.object++;
            objSum += len;
            s.objectMax = za::max(s.objectMax, len);
        }
        else
        {
            s.camera++;
            camSum += len;
        }
    }
    s.cameraMean = s.camera ? static_cast<float>(camSum / s.camera) : 0.f;
    s.objectMean = s.object ? static_cast<float>(objSum / s.object) : 0.f;
    (void)reversed;
}

[[nodiscard]] bool testOn()
{
    return vr_spacewarp_test.value != 0.f;
}

} // namespace

void beginFrame()
{
    Backend* be = backend();
    int w = 0, h = 0;
    const bool offered = be && be->spaceWarpOffered(w, h);
    const bool want = (vr_xr_spacewarp.value != 0.f && offered) || testOn();
    // (a menu, the console: the UI over the eyes is not the scene's, it has no motion vectors of its own)
    state.frameOn = want && key_dest == key_game && !con_forcedup && !state.failed;
    state.eyesDone[0] = state.eyesDone[1] = false;
    if(state.frameOn != state.wasOn)
    {
        state.wasOn = state.frameOn;
        Con_DPrintf("VR: SpaceWarp %s\n", state.frameOn ? (offered ? "on (the game at half the display's rate)" : "on (vr_spacewarp_test)")
                                                        : "off this frame");
        state.havePrev = false; // (no delta across the time it was off)
    }
}

bool frameOn()
{
    return state.frameOn;
}

void eyePass(const EyeInputs& in)
{
    if(!state.frameOn || in.eye < 0 || in.eye > 1 || !in.depthTex || !in.motionTex || in.drawnWidth <= 0 || in.drawnHeight <= 0 ||
        !ensureProgram())
    {
        return;
    }
    Backend* be = backend();
    QVR_GPU_PROFILE("spacewarp");
    const bool test = testOn();
    const Backend::SpaceWarpImages images = test ? testImages(in.eye, in.drawnWidth, in.drawnHeight)
                                            : be ? be->acquireSpaceWarpImages(in.eye) : Backend::SpaceWarpImages{};
    if(!images.motion || !images.depth || images.width <= 0 || images.height <= 0)
    {
        return;
    }
    const GLuint bound = GL_BoundDrawFramebuffer();
    const GLuint fbo = fboFor(images.motion, images.depth);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    {
        constexpr GLenum all[] = {GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT}; // (every texel written: nothing loaded)
        GL_InvalidateFramebufferFunc(GL_FRAMEBUFFER, 2, all);
    }
    glViewport(0, 0, images.width, images.height);
    GL_UseProgram(state.program);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    glDepthFunc(GL_ALWAYS);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, in.depthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
    GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, in.motionTex);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, in.invViewProj);
    GL_UniformMatrix4fvFunc(1, 1, GL_FALSE, in.prevViewProj);
    GL_Uniform4fFunc(2, in.stamp, in.reversed ? 1.f : 0.f, static_cast<float>(in.drawnWidth), static_cast<float>(in.drawnHeight));
    GL_Uniform4fFunc(3, in.nearUnits, in.farUnits, static_cast<float>(images.width), static_cast<float>(images.height));
    GL_Uniform1fFunc(4, test ? 2.f : 1.f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthFunc(gl_clipcontrol_able ? GL_GEQUAL : GL_LEQUAL); // (the engine's own: GL_Init's)
    if(test && in.eye == 0)
    {
        readStats(fbo, images.width, images.height, in.reversed);
    }
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, bound);
    if(!test && be)
    {
        be->releaseSpaceWarpImages(in.eye);
    }
    state.stats.frames++;
    state.eyesDone[in.eye] = true;

    // The frame's information, from the left eye (the right's is the same play space)
    if(in.eye == 0 && in.origin && in.angles)
    {
        const float upm = units::metresToUnits();
        state.nearZ = in.reversed ? in.farUnits / upm : in.nearUnits / upm; // (the distance at depth 0, then at 1)
        state.farZ = in.reversed ? in.nearUnits / upm : in.farUnits / upm;
        state.worldFromApp = worldFromApp(in.origin, in.angles, frameState().eyes[0].pose);
        state.havePose = true;
        state.jump = in.jump;
    }
}

void endFrame()
{
    if(!state.frameOn || !state.havePose)
    {
        return;
    }
    Backend::SpaceWarpFrame f;
    f.nearZ = state.nearZ;
    f.farZ = state.farZ;
    f.skip = state.jump || !state.havePrev;
    if(state.havePrev)
    {
        // Inv(previous) * current (Meta's guide and sample): the current play space as the previous one saw it
        const Rigid d = compose(inverse(state.prevWorldFromApp), state.worldFromApp);
        f.appSpaceDelta.position = d.t;
        f.appSpaceDelta.orientation = glm::normalize(glm::quat_cast(d.r));
        f.appSpaceDelta.valid = true;
        state.stats.deltaT = d.t;
        // (the yaw: the turn of the delta's back axis about up)
        state.stats.deltaYaw = glm::degrees(za::atan2(d.r[2][0], d.r[2][2]));
    }
    state.stats.skipped += f.skip ? 1 : 0;
    state.prevWorldFromApp = state.worldFromApp;
    state.havePrev = true;
    state.havePose = false;
    if(Backend* be = backend(); be && !testOn())
    {
        be->setSpaceWarpFrame(f);
    }
}

void releaseTargets()
{
    deleteFbos();
    deleteTestImages();
    state.havePrev = state.havePose = false;
}

namespace
{

// vr_spacewarp_check begin | still | moving | jumped | verdict: the test's checks on the last left eye's images
// (Misc/quakevr/spacewarp_test.sh).
void check_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    const Stats& s = state.stats;
    testkit::Tally& t = state.tally;
    Con_Printf("spacewarp: %s: frames %d, skipped %d, %dx%d: camera %d (mean %.5f), models %d (mean %.5f, max %.5f), hidden "
               "%d, depth %.5f..%.5f; delta %.4f %.4f %.4f m, yaw %.3f\n",
        what, s.frames, s.skipped, s.width, s.height, s.camera, static_cast<double>(s.cameraMean), s.object,
        static_cast<double>(s.objectMean), static_cast<double>(s.objectMax), s.hidden, static_cast<double>(s.depthMin),
        static_cast<double>(s.depthMax), static_cast<double>(s.deltaT.x), static_cast<double>(s.deltaT.y),
        static_cast<double>(s.deltaT.z), static_cast<double>(s.deltaYaw));
    if(!ZA_STRCMP(what, "begin"))
    {
        t.reset();
        t.check(state.frameOn, "spacewarp", "not on (vr_spacewarp_test 1, in the game, no menu)");
        t.check(s.frames > 0, "spacewarp", "no eye pass made the images");
    }
    else if(!ZA_STRCMP(what, "still"))
    {
        t.check(s.camera > s.width * s.height / 4, "spacewarp", "under a quarter of the texels are the scene's");
        t.check(s.cameraMean < 0.002f, "spacewarp", "the still world moves");
        t.check(s.object == 0 || s.objectMean < 0.002f, "spacewarp", "the still models move");
        t.check(s.depthMin < s.depthMax && s.depthMin >= 0.f && s.depthMax <= 1.f, "spacewarp", "no depth range");
        t.check(glm::length(s.deltaT) < 1e-4f && za::abs(s.deltaYaw) < 0.01f, "spacewarp", "the play space moves while still");
        state.skippedAtStill = s.skipped;
    }
    else if(!ZA_STRCMP(what, "moving"))
    {
        t.check(s.cameraMean > 0.002f, "spacewarp", "the world does not stream by while walking");
        t.check(glm::length(s.deltaT) > 1e-3f, "spacewarp", "the play space's delta is not the walk's");
        // (walking forward, the head facing forward: the play space goes to -z in its own axes)
        t.check(s.deltaT.z < 0.f, "spacewarp", "the play space's delta is not forward");
        // (the gun and hands go with the player: still on the eyes)
        t.check(s.object == 0 || s.objectMean < s.cameraMean * 0.5f, "spacewarp", "the held models move on the eyes");
    }
    else if(!ZA_STRCMP(what, "turning"))
    {
        t.check(za::abs(s.deltaYaw) > 0.05f, "spacewarp", "the play space's delta has no turn");
        t.check(s.cameraMean > 0.002f, "spacewarp", "the world does not turn on the eyes");
    }
    else if(!ZA_STRCMP(what, "jumped"))
    {
        t.check(s.skipped > state.skippedAtStill, "spacewarp", "no frame marked skipped after the jump");
    }
    else if(!ZA_STRCMP(what, "verdict"))
    {
        t.verdict("spacewarp");
    }
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_spacewarp_check", check_f);
}

} // namespace qvr::spacewarp
