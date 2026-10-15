// Opt-in EXT 2D synthesis producer: resolved eye depth plus camera-seeded
// pyramidal image motion. This estimates dynamic image correspondence; it is
// not a replacement for exact per-vertex geometry velocity and stays off by default.
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "vr_frame_synthesis.hpp"
#include "vr_synthesis_shaders.hpp"
#include "vr_synthesis_math.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_units.hpp"
#include "vr_portals.hpp"
#include "vr_water.hpp"
#include "vr_profile.hpp"
#include "vr_files.hpp"
#include "Zancle/Base/InitializerList.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <sstream>
#include <cstdint>

extern "C" qboolean water_warp;

namespace qvr::frame_synthesis
{
namespace
{
constexpr int levels = 3;
struct Eye
{
    GLuint color[2]{}, depth{0}, seed{0}, flow[2]{}, fbo{0};
    int width{0}, height{0}, slot{0};
    int sourceWidth{0}, sourceHeight{0};
    int viewport[4]{}, depthSamples{1};
    bool history{false}, captured{false}, rendered{false};
    glm::mat4 current{1.f}, previous{1.f}, application{1.f}, previousApplication{1.f};
    FrameSynthesisMetadata metadata;
};
Eye eyes[2];
GLuint depthProgram{0}, flowProgram{0}, exportProgram{0};
bool failed{false};
int generation{-1};
double previousTime{-1.0};
bool contiguous{false};
bool deltaMismatchLogged{false};
bool diagnosticCaptureAttempted{false};
unsigned diagnosticCaptureMask{0};
std::uint64_t diagnosticCaptureFrame{0};

struct GlState
{
    unsigned mask{glstate};
    GLint read{0}, draw{0}, viewport[4]{}, depthFunc{0}, program{0}, active{0};
    GLint texture[3]{}, multisample[3]{}, sampler[3]{};
    GLboolean srgb{false}, stencil{false}, scissor{false}, colors[4]{};
    GLdouble range[2]{};
    GlState()
    {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetBooleanv(GL_COLOR_WRITEMASK, colors);
        glGetDoublev(GL_DEPTH_RANGE, range);
        srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        stencil = glIsEnabled(GL_STENCIL_TEST);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        using GetIndexed = void(APIENTRY*)(GLenum, GLuint, GLint*);
        const auto getIndexed = reinterpret_cast<GetIndexed>(SDL_GL_GetProcAddress("glGetIntegeri_v"));
        for(int unit = 0; unit < 3; ++unit)
        {
            GL_SelectTexture(GL_TEXTURE0 + unit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture[unit]);
            glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &multisample[unit]);
            if(getIndexed) getIndexed(GL_SAMPLER_BINDING, unit, &sampler[unit]);
            GL_BindSamplerFunc(unit, 0);
        }
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthRange(0.0, 1.0);
    }
    ~GlState()
    {
        for(int unit = 0; unit < 3; ++unit)
        {
            GL_SelectTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texture[unit]);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, multisample[unit]);
            GL_InvalidateTextureBinding(GL_TEXTURE0 + unit);
            GL_BindSamplerFunc(unit, sampler[unit]);
        }
        GL_SelectTexture(active);
        GL_SetState(mask);
        glDepthFunc(depthFunc);
        glDepthRange(range[0], range[1]);
        glColorMask(colors[0], colors[1], colors[2], colors[3]);
        if(srgb) glEnable(GL_FRAMEBUFFER_SRGB);
        if(stencil) glEnable(GL_STENCIL_TEST);
        if(scissor) glEnable(GL_SCISSOR_TEST);
        GL_UseProgram(program);
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, read);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, draw);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    }
};

GLuint shader(GLenum type, const char* source)
{
    const GLuint id = GL_CreateShaderFunc(type);
    GL_ShaderSourceFunc(id, 1, &source, nullptr);
    GL_CompileShaderFunc(id);
    GLint ok = 0;
    GL_GetShaderivFunc(id, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[2048]{};
        GL_GetShaderInfoLogFunc(id, sizeof(log), nullptr, log);
        Con_Warning("VR frame synthesis shader: %s\n", log);
        GL_DeleteShaderFunc(id);
        return 0;
    }
    return id;
}

GLuint program(const char* fragment)
{
    const GLuint vs = shader(GL_VERTEX_SHADER, synthesis::shaders::fullscreen);
    const GLuint fs = shader(GL_FRAGMENT_SHADER, fragment);
    if(!vs || !fs)
    {
        if(vs) GL_DeleteShaderFunc(vs);
        if(fs) GL_DeleteShaderFunc(fs);
        return 0;
    }
    const GLuint id = GL_CreateProgramFunc();
    GL_AttachShaderFunc(id, vs);
    GL_AttachShaderFunc(id, fs);
    GL_LinkProgramFunc(id);
    GL_DeleteShaderFunc(vs);
    GL_DeleteShaderFunc(fs);
    GLint ok = 0;
    GL_GetProgramivFunc(id, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        char log[2048]{};
        GL_GetProgramInfoLogFunc(id, sizeof(log), nullptr, log);
        Con_Warning("VR frame synthesis link: %s\n", log);
        GL_DeleteProgramFunc(id);
        return 0;
    }
    return id;
}

void destroy(Eye& e)
{
    for(GLuint t : {e.color[0], e.color[1], e.depth, e.seed, e.flow[0], e.flow[1]})
        if(t) GL_DeleteNativeTexture(t);
    if(e.fbo) GL_DeleteFramebuffersFunc(1, &e.fbo);
    e = {};
}

GLuint texture(int width, int height, GLenum format, int count)
{
    GLuint id = 0;
    glGenTextures(1, &id);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, id);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, count, format, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, count > 1 ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, count - 1);
    GL_ObjectLabelFunc(GL_TEXTURE, id, -1, "frame synthesis internal image");
    return id;
}

bool ensure(Eye& e, int width, int height)
{
    if(failed) return false;
    if(!depthProgram)
    {
        depthProgram = program(synthesis::shaders::depth);
        flowProgram = program(synthesis::shaders::flow);
        exportProgram = program(synthesis::shaders::exportImages);
        failed = !depthProgram || !flowProgram || !exportProgram;
        if(failed) return false;
    }
    if(e.width == width && e.height == height) return true;
    destroy(e);
    e.width = width;
    e.height = height;
    e.color[0] = texture(width, height, GL_RGBA8, levels);
    e.color[1] = texture(width, height, GL_RGBA8, levels);
    e.depth = texture(width, height, GL_DEPTH_COMPONENT32F, 1);
    e.seed = texture(width, height, GL_RGBA16F, 1);
    e.flow[0] = texture(width, height, GL_RGBA16F, levels);
    e.flow[1] = texture(width, height, GL_RGBA16F, levels);
    GL_GenFramebuffersFunc(1, &e.fbo);
    if(glGetError() != GL_NO_ERROR)
    {
        destroy(e);
        return false;
    }
    return true;
}

bool target(Eye& e, GLuint color, GLuint depth, int level = 0)
{
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, e.fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, level);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    const GLenum buffer = GL_COLOR_ATTACHMENT0;
    GL_DrawBuffersFunc(1, &buffer);
    return GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void detach(Eye& e)
{
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, e.fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
}

glm::mat4 applicationTransform(int eye)
{
    // Exact eye-world mapping minus runtime eye tracking. Metres in stage map
    // to scaled Quake units; real head movement therefore cancels out here.
    const Pose& p = frameState().eyes[eye].pose;
    const glm::mat4 tracked = glm::translate(glm::mat4{1.f}, p.position) * glm::mat4_cast(p.orientation);
    return synthesis::worldFromStage(glm::make_mat4(r_matview), tracked, units::metresToUnits());
}

extern "C" qboolean Image_WritePNG(const char*, byte*, int, int, int, qboolean);

bool capturePng(const za::String& path, GLuint image, int width, int height)
{
    za::Vector<byte> pixels(static_cast<za::SizeT>(width) * height * 4);
    GLint active = 0, binding = 0, alignment = 4;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    GL_SelectTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, image);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const bool ok = glGetError() == GL_NO_ERROR && Image_WritePNG(path.cStr(), pixels.data(), width, height, 32, false);
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, static_cast<GLuint>(binding));
    GL_SelectTexture(active);
    return ok;
}

bool captureFloat(const za::String& path, GLuint image, int width, int height, GLenum format, int channels)
{
    za::Vector<float> pixels(static_cast<za::SizeT>(width) * height * channels);
    GLint active = 0, binding = 0, alignment = 4;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    GL_SelectTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, image);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, format, GL_FLOAT, pixels.data());
    const bool ok = glGetError() == GL_NO_ERROR && files::writeBytes(path.cStr(), pixels.data(),
        pixels.size() * sizeof(float));
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, static_cast<GLuint>(binding));
    GL_SelectTexture(active);
    return ok;
}

void captureMatrix(std::ostringstream& json, const char* name, const glm::mat4& value)
{
    json << "  \"" << name << "\": [";
    for(int i = 0; i < 16; ++i)
    {
        if(i) json << ", ";
        json << value[i / 4][i % 4];
    }
    json << "],\n";
}

bool captureDiagnostic(int eye, Eye& e, const FrameSynthesisImages& images, int flow)
{
    diagnosticCaptureFrame = images.contentFrameId;
    const char* side = eye == 0 ? "L" : "R";
    const za::String dir = za::String{com_gamedir} + "/profile/frame_synthesis_capture";
    if(!files::createDirectories(dir.cStr())) return false;
    const za::String stem = va("%s_%llu_%s", cl.mapname[0] ? cl.mapname : "none",
        static_cast<unsigned long long>(images.contentFrameId), side);
    const za::String currentColor = "profile/frame_synthesis_capture/" + stem + "_color.png";
    const za::String previousColor = "profile/frame_synthesis_capture/" + stem + "_previous_color.png";
    const auto raw = [&](const char* suffix) { return dir + "/" + stem + suffix; };
    bool ok = capturePng(currentColor, e.color[e.slot], e.width, e.height);
    ok = capturePng(previousColor, e.color[e.slot ^ 1], e.width, e.height) && ok;
    ok = captureFloat(raw("_depth.r32f"), e.depth, e.width, e.height, GL_DEPTH_COMPONENT, 1) && ok;
    ok = captureFloat(raw("_camera_seed.rgba32f"), e.seed, e.width, e.height, GL_RGBA, 4) && ok;
    ok = captureFloat(raw("_flow.rgba32f"), e.flow[flow], e.width, e.height, GL_RGBA, 4) && ok;
    ok = captureFloat(raw("_export_motion.rgba32f"), static_cast<GLuint>(images.motion.image),
        images.motion.width, images.motion.height, GL_RGBA, 4) && ok;
    ok = captureFloat(raw("_export_depth.r32f"), static_cast<GLuint>(images.depth.image),
        images.depth.width, images.depth.height, GL_DEPTH_COMPONENT, 1) && ok;

    std::ostringstream json;
    json.precision(9);
    const Pose& delta = e.metadata.appSpaceDelta;
    json << "{\n  \"schema\": 1,\n  \"performance_valid\": false,\n"
        << "  \"frame_id\": " << images.contentFrameId << ",\n  \"eye\": \"" << side << "\",\n"
        << "  \"row_order\": \"OpenGL bottom-to-top; color PNG top-down\",\n"
        << "  \"raw_float_encoding\": \"IEEE754 binary32 little-endian\",\n"
        << "  \"matrix_layout\": \"GLM column-major\",\n"
        << "  \"source_size\": [" << e.sourceWidth << ", " << e.sourceHeight << "],\n"
        << "  \"synthesis_size\": [" << e.width << ", " << e.height << "],\n"
        << "  \"export_size\": [" << images.motion.width << ", " << images.motion.height << "],\n"
        << "  \"color_includes_ui\": true,\n  \"depth_source\": \"scene depth before postprocess and UI\",\n"
        << "  \"depth_samples\": " << e.depthSamples << ",\n  \"scene_viewport\": ["
        << e.viewport[0] << ", " << e.viewport[1] << ", " << e.viewport[2] << ", " << e.viewport[3] << "],\n"
        << "  \"near_z\": " << e.metadata.nearZ << ",\n  \"far_z\": " << e.metadata.farZ << ",\n"
        << "  \"motion_scale\": [" << images.motionScaleX << ", " << images.motionScaleY << "],\n"
        << "  \"motion_offset\": [" << images.motionOffsetX << ", " << images.motionOffsetY << "],\n"
        << "  \"motion_encoding\": \"" << (images.srgb8Motion ? "sRGB8" : "RGBA16F signed") << "\",\n"
        << "  \"app_space_delta_position\": [" << delta.position.x << ", " << delta.position.y << ", "
        << delta.position.z << "],\n  \"app_space_delta_orientation_xyzw\": [" << delta.orientation.x << ", "
        << delta.orientation.y << ", " << delta.orientation.z << ", " << delta.orientation.w << "],\n";
    captureMatrix(json, "current_view", glm::make_mat4(r_matview));
    captureMatrix(json, "current_projection", glm::make_mat4(r_matproj));
    captureMatrix(json, "current_view_projection", e.current);
    captureMatrix(json, "previous_view_projection", e.previous);
    captureMatrix(json, "current_application_transform", e.application);
    captureMatrix(json, "previous_application_transform", e.previousApplication);
    json << "  \"files\": {\"color\": \"" << currentColor.cStr() << "\", \"previous_color\": \""
        << previousColor.cStr() << "\", \"depth\": \"" << raw("_depth.r32f").cStr()
        << "\", \"camera_seed\": \"" << raw("_camera_seed.rgba32f").cStr()
        << "\", \"flow\": \"" << raw("_flow.rgba32f").cStr() << "\", \"export_motion\": \""
        << raw("_export_motion.rgba32f").cStr() << "\", \"export_depth\": \""
        << raw("_export_depth.r32f").cStr() << "\"}\n}\n";
    const std::string text = json.str();
    const za::String metadata = dir + "/" + stem + ".json";
    return files::writeText(metadata.cStr(), {text.data(), text.size()}) && ok;
}
} // namespace

void reset()
{
    for(Eye& e : eyes) e.history = e.captured = e.rendered = false;
    previousTime = -1.0;
    contiguous = false;
    deltaMismatchLogged = false;
}

void beginFrame()
{
    diagnosticCaptureAttempted = false;
    diagnosticCaptureMask = 0;
    diagnosticCaptureFrame = 0;
    for(Eye& e : eyes) e.captured = e.rendered = false;
    Backend* be = backend();
    const double time = tracking().time;
    contiguous = be && be->frameSynthesisAvailable() && synthesis::continuousHistory(
        previousTime, be->previousProjectionTime(), time, generation == worldGeneration());
    if(!contiguous) reset();
    generation = worldGeneration();
}

void captureDepth(int eye, int width, int height, unsigned depthFormat)
{
    Backend* be = backend();
    if(!be || !be->frameSynthesisAvailable() || eye < 0 || eye > 1) return;
    // R_RenderView returns before drawing when this debug cvar is set. Do not
    // pair the retained depth target with an unchanged or stale color image.
    if(r_norefresh.value != 0.f) return;
    Eye& e = eyes[eye];
    // Depth and postprocess color do not describe the same ray through these modes.
    const glm::vec3 wobble = water::viewWobble();
    if(water_warp || wobble.y != 0.f || wobble.z != 0.f || portals::viewCount() > 0) return;
    const FrameSynthesisImages config = be->frameSynthesisConfiguration(eye);
    if(config.motion.api != GraphicsApi::OpenGL || config.depth.api != GraphicsApi::OpenGL ||
        config.motion.width < 32 || config.motion.height < 32 ||
        config.depth.width != config.motion.width || config.depth.height != config.motion.height) return;
    QVR_GPU_PROFILE("frame synthesis depth");
    const GlState state;
    if(!ensure(e, config.motion.width, config.motion.height) || !target(e, e.seed, e.depth)) return;
    if(e.sourceWidth != width || e.sourceHeight != height) e.history = false;
    e.sourceWidth = width;
    e.sourceHeight = height;
    e.current = glm::make_mat4(r_matviewproj);
    e.application = applicationTransform(eye);
    if(contiguous && e.history)
    {
        const glm::mat4 delta = synthesis::applicationDelta(e.previousApplication, e.application);
        if(glm::length(glm::vec3{delta[3]}) > 1.f ||
            glm::dot(glm::normalize(glm::vec3{e.application[0]}), glm::normalize(glm::vec3{e.previousApplication[0]})) < 0.85f ||
            std::abs(glm::length(glm::vec3{delta[0]}) - 1.f) > 0.001f)
            e.history = false; // teleport, snap turn or calibration/scale jump: warm up again
    }
    const bool scene = GL_NeedsSceneEffects() != 0;
    const GLuint source = scene ? framebufs.scene.depth_stencil_tex : framebufs.composite.depth_stencil_tex;
    const int samples = scene ? framebufs.scene.samples : 1;
    int viewport[4] = {0, 0, width, height};
    R_SceneViewport(viewport);
    for(int i = 0; i < 4; ++i) e.viewport[i] = viewport[i];
    e.depthSamples = samples;
    GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    glDepthFunc(GL_ALWAYS);
    glViewport(0, 0, e.width, e.height);
    GL_UseProgram(depthProgram);
    GL_Uniform2fFunc(0, e.width, e.height);
    GL_Uniform4fFunc(1, static_cast<float>(viewport[0]) / width, static_cast<float>(viewport[1]) / height,
        static_cast<float>(viewport[2]) / width, static_cast<float>(viewport[3]) / height);
    const glm::mat4 inverse = glm::inverse(e.current);
    GL_UniformMatrix4fvFunc(2, 1, GL_FALSE, glm::value_ptr(inverse));
    GL_UniformMatrix4fvFunc(6, 1, GL_FALSE, glm::value_ptr(e.history ? e.previous : e.current));
    GL_Uniform3fFunc(10, r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]);
    GL_Uniform1iFunc(11, gl_clipcontrol_able != 0);
    GL_Uniform1iFunc(12, samples);
    GL_BindNative(GL_TEXTURE0 + (samples > 1 ? 1 : 0), samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, source);
    // Immutable storage is undefined until written. A finite zero-motion seed
    // keeps any uncovered fragment safe for the optical-flow fallback and makes
    // diagnostic captures deterministic on drivers with incomplete raster coverage.
    const GLfloat seedClear[4] = {0.f, 0.f, 0.f, 1.f};
    GL_ClearBufferfvFunc(GL_COLOR, 0, seedClear);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    e.captured = glGetError() == GL_NO_ERROR;
    const float near = CLAMP(depthFormat == GL_DEPTH32F_STENCIL8 ? 0.02f : 1.f, vr_nearclip.value, 4.f) / units::metresToUnits();
    const float far = gl_farclip.value / units::metresToUnits();
    e.metadata.nearZ = gl_clipcontrol_able ? far : near;
    e.metadata.farZ = gl_clipcontrol_able ? near : far;
    e.metadata.appSpaceDelta = {};
    e.metadata.appSpaceDelta.valid = true;
    if(e.history)
    {
        const glm::mat4 delta = synthesis::applicationDelta(e.previousApplication, e.application);
    e.metadata.appSpaceDelta.position = glm::vec3{delta[3]};
    e.metadata.appSpaceDelta.orientation = glm::normalize(glm::quat_cast(glm::mat3{delta}));
    }
}

void render(int eye, unsigned colorFramebuffer, int width, int height)
{
    Backend* be = backend();
    if(!be || !be->frameSynthesisAvailable() || eye < 0 || eye > 1) return;
    Eye& e = eyes[eye];
    if(!e.captured) return;
    QVR_GPU_PROFILE("frame synthesis motion");
    const GlState state;
    {
        QVR_GPU_PROFILE("synthesis downsample");
        if(!target(e, e.color[e.slot], 0)) return;
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, colorFramebuffer);
        GL_BlitFramebufferFunc(0, 0, width, height, 0, 0, e.width, e.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, e.color[e.slot]);
        GL_GenerateMipmapFunc(GL_TEXTURE_2D);
        e.rendered = glGetError() == GL_NO_ERROR;
    }
    if(!e.rendered || !e.history || !contiguous) return;
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(flowProgram);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, e.color[e.slot]);
    GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, e.color[e.slot ^ 1]);
    int last = 0;
    {
        QVR_GPU_PROFILE("synthesis optical flow");
        int output = 0;
        for(int level = levels - 1; level >= 0; --level)
        {
            const int w = e.width >> level, h = e.height >> level;
            for(int iteration = 0; iteration < synthesis::shaders::flowIterations; ++iteration)
            {
                output = last ^ 1;
                if(!target(e, e.flow[output], 0, level)) return;
                glViewport(0, 0, w, h);
                const bool first = level == levels - 1 && iteration == 0;
                GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, first ? e.seed : e.flow[last]);
                GL_Uniform2fFunc(0, w, h);
                GL_Uniform1iFunc(1, level);
                GL_Uniform1iFunc(2, first ? 0 : (iteration == 0 ? level + 1 : level));
                glDrawArrays(GL_TRIANGLES, 0, 3);
                last = output;
            }
        }
    }
    if(glGetError() != GL_NO_ERROR) return;
    FrameSynthesisImages images;
    {
        QVR_PROFILE("synthesis acquire");
        images = be->acquireFrameSynthesisImages(eye);
    }
    const auto release = [&](bool valid, const FrameSynthesisMetadata& metadata)
    {
        QVR_PROFILE("synthesis transport");
        be->releaseFrameSynthesisImages(eye, valid, metadata);
    };
    if(!images.motion || !images.depth) return;
    if(images.motion.api != GraphicsApi::OpenGL || images.depth.api != GraphicsApi::OpenGL)
    {
        release(false, {});
        return;
    }
    const int imageWidth = images.motion.width;
    const int imageHeight = images.motion.height;
    if(imageWidth <= 0 || imageHeight <= 0 || images.depth.width != imageWidth || images.depth.height != imageHeight)
    {
        release(false, {});
        return;
    }
    bool valid = target(e, static_cast<GLuint>(images.motion.image), static_cast<GLuint>(images.depth.image));
    if(valid)
    {
        QVR_GPU_PROFILE("synthesis encode");
        GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_NONE | GLS_ATTRIBS(0));
        glDepthFunc(GL_ALWAYS);
        glViewport(0, 0, imageWidth, imageHeight);
        GL_UseProgram(exportProgram);
        GL_Uniform2fFunc(0, imageWidth, imageHeight);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, e.flow[last]);
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, e.depth);
        GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, e.seed);
        GL_Uniform2fFunc(3, images.motionScaleX, images.motionScaleY);
        GL_Uniform2fFunc(4, images.motionOffsetX, images.motionOffsetY);
        GL_Uniform1iFunc(5, images.srgb8Motion ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        valid = glGetError() == GL_NO_ERROR;
    }
    detach(e); // detach runtime textures before ownership passes back
    e.metadata.contentFrameId = valid ? images.contentFrameId : 0;
    e.metadata.motionScaleX = images.motionScaleX;
    e.metadata.motionScaleY = images.motionScaleY;
    e.metadata.motionOffsetX = images.motionOffsetX;
    e.metadata.motionOffsetY = images.motionOffsetY;
    if(vr_xr_frame_synthesis_capture.value > 0.f)
    {
        diagnosticCaptureAttempted = true;
        if(captureDiagnostic(eye, e, images, last))
        {
            diagnosticCaptureMask |= 1u << eye;
            Con_Printf("VR synthesis diagnostic: saved frame %llu eye %s\n",
                static_cast<unsigned long long>(images.contentFrameId), eye == 0 ? "L" : "R");
        }
        else
        {
            Con_Warning("VR synthesis diagnostic: capture failed for frame %llu eye %s\n",
                static_cast<unsigned long long>(images.contentFrameId), eye == 0 ? "L" : "R");
        }
    }
    release(valid, e.metadata);
    if(eye == 1 && eyes[0].rendered && eyes[1].rendered)
    {
        const Pose& leftPose = eyes[0].metadata.appSpaceDelta;
        const Pose& rightPose = eyes[1].metadata.appSpaceDelta;
        const glm::mat4 leftDelta = glm::translate(glm::mat4_cast(leftPose.orientation), leftPose.position);
        const glm::mat4 rightDelta = glm::translate(glm::mat4_cast(rightPose.orientation), rightPose.position);
        if(!synthesis::sameApplicationDelta(leftDelta, rightDelta))
        {
            be->rejectFrameSynthesis();
            if(!deltaMismatchLogged)
                Con_Warning("VR frame synthesis: left and right application-space deltas differ; using color-only frame\n");
            deltaMismatchLogged = true;
        }
        else
        {
            deltaMismatchLogged = false;
        }
    }
}

void submitted(bool stereoSubmitted)
{
    if(diagnosticCaptureAttempted)
    {
        Cvar_SetQuick(&vr_xr_frame_synthesis_capture, "0");
        Con_Printf("VR synthesis diagnostic: one-shot frame=%llu eyes=0x%x, stereo_submitted=%d; exclude from performance data\n",
            static_cast<unsigned long long>(diagnosticCaptureFrame), diagnosticCaptureMask, stereoSubmitted);
    }
    if(!stereoSubmitted || !eyes[0].rendered || !eyes[1].rendered)
    {
        reset();
        return;
    }
    for(Eye& e : eyes)
    {
        e.previous = e.current;
        e.previousApplication = e.application;
        e.slot ^= 1;
        e.history = true;
    }
    previousTime = tracking().time;
}

void shutdown()
{
    for(Eye& e : eyes) destroy(e);
    for(GLuint* id : {&depthProgram, &flowProgram, &exportProgram})
    {
        if(*id) GL_DeleteProgramFunc(*id);
        *id = 0;
    }
    failed = false;
    diagnosticCaptureAttempted = false;
    diagnosticCaptureMask = 0;
    diagnosticCaptureFrame = 0;
    reset();
}
} // namespace qvr::frame_synthesis
