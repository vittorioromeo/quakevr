// vr_panel.cpp -- Ironwail's 2D layer (menus, console, HUD) in the headset.
//
// While the eyes are rendered, the 2D pass draws into an offscreen canvas instead of the
// window; the canvas is then composited over the desktop mirror, and shown in each eye as a
// panel floating in front of the player while a menu or the console is open.
// TODO VR: (P6) status bar attached to a hand, menu laser pointer.

#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_panel.hpp"

using namespace qvr;

namespace
{

constexpr const char* vertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
out vec2 uv;
void main()
{
    const vec2 corners[6] = vec2[](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));
    uv = corners[gl_VertexID];
    gl_Position = MVP * vec4(uv, 0.0, 1.0);
}
)";

constexpr const char* fragmentShader = R"(#version 430
layout(binding = 0) uniform sampler2D Canvas;
in vec2 uv;
out vec4 color;
void main()
{
    color = texture(Canvas, uv);
}
)";

GLuint program = 0;
GLuint canvasTexture = 0;
GLuint canvasFbo = 0;
int canvasWidth = 0;
int canvasHeight = 0;
bool drawingToCanvas = false;
bool stereoThisFrame = false;

// Panel placement, frozen when it appears: in front of the head, turning with the player.
bool panelWasVisible = false;
float panelYawOffset = 0.f;

[[nodiscard]] GLuint compile(GLenum type, const char* source)
{
    const GLuint shader = GL_CreateShaderFunc(type);
    GL_ShaderSourceFunc(shader, 1, &source, nullptr);
    GL_CompileShaderFunc(shader);

    GLint ok = 0;
    GL_GetShaderivFunc(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[1024];
        GL_GetShaderInfoLogFunc(shader, sizeof(log), nullptr, log);
        Con_Warning("VR: panel shader: %s\n", log);
    }
    return shader;
}

bool ensureProgram()
{
    if(program)
    {
        return true;
    }

    const GLuint vs = compile(GL_VERTEX_SHADER, vertexShader);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentShader);
    program = GL_CreateProgramFunc();
    GL_AttachShaderFunc(program, vs);
    GL_AttachShaderFunc(program, fs);
    GL_LinkProgramFunc(program);
    GL_DeleteShaderFunc(vs);
    GL_DeleteShaderFunc(fs);

    GLint ok = 0;
    GL_GetProgramivFunc(program, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        Con_Warning("VR: panel shader failed to link\n");
        GL_DeleteProgramFunc(program);
        program = 0;
        return false;
    }

    return true;
}

void ensureCanvas(int width, int height)
{
    if(canvasTexture && canvasWidth == width && canvasHeight == height)
    {
        return;
    }

    if(canvasTexture)
    {
        glDeleteTextures(1, &canvasTexture);
    }
    if(!canvasFbo)
    {
        GL_GenFramebuffersFunc(1, &canvasFbo);
    }

    glGenTextures(1, &canvasTexture);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, canvasTexture);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, canvasFbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvasTexture, 0);

    canvasWidth = width;
    canvasHeight = height;
}

// Draws the canvas as a quad; `mvp` maps the quad's (0..1, 0..1) to clip space. The canvas
// holds colours already multiplied by their alpha (2D was drawn over transparent black).
void drawCanvas(const glm::mat4& mvp)
{
    if(!canvasTexture || !ensureProgram())
    {
        return;
    }

    GL_UseProgram(program);
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, canvasTexture);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // what GLS_BLEND_ALPHA expects
}

[[nodiscard]] bool panelVisible()
{
    return key_dest != key_game || con_forcedup || scr_drawloading || cl.intermission;
}

} // namespace

namespace qvr::panel
{

void setStereoThisFrame(bool stereo)
{
    stereoThisFrame = stereo;
}

void drawInEye(const hands::State& s)
{
    const bool visible = panelVisible();
    if(visible && !panelWasVisible)
    {
        panelYawOffset = s.headAngles.y - hands::playSpaceYaw();
    }
    panelWasVisible = visible;

    if(!visible || !canvasTexture)
    {
        return;
    }

    const float yaw = panelYawOffset + hands::playSpaceYaw();
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, yaw, 0.f}, fwd, right, up);

    const float height = 200.f * vr_menu_scale.value;
    const float width = height * static_cast<float>(canvasWidth) / canvasHeight;
    const glm::vec3 centre = s.head + fwd * vr_menu_distance.value;
    const glm::vec3 corner = centre - right * (width * 0.5f) - up * (height * 0.5f);

    glm::mat4 model{1.f};
    model[0] = glm::vec4{right * width, 0.f};
    model[1] = glm::vec4{up * height, 0.f};
    model[2] = glm::vec4{0.f};
    model[3] = glm::vec4{corner, 1.f};

    glm::mat4 viewProj;
    memcpy(&viewProj[0][0], r_matviewproj, sizeof(r_matviewproj));

    drawCanvas(viewProj * model);
}

} // namespace qvr::panel

extern "C" void VR_Begin2D()
{
    drawingToCanvas = stereoThisFrame;
    if(!drawingToCanvas)
    {
        return;
    }

    ensureCanvas(vid.width, vid.height);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, canvasFbo);
    glViewport(0, 0, vid.width, vid.height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

extern "C" void VR_End2D()
{
    if(!drawingToCanvas)
    {
        return;
    }
    drawingToCanvas = false;

    // Back to the window, with the 2D layer over the mirrored eye.
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, GL_NeedsPostprocess() ? framebufs.composite.fbo : 0);
    glViewport(glx, gly, glwidth, glheight);

    glm::mat4 toNdc{1.f};
    toNdc[0][0] = 2.f;
    toNdc[1][1] = 2.f;
    toNdc[3] = glm::vec4{-1.f, -1.f, 0.f, 1.f};
    drawCanvas(toNdc);

    stereoThisFrame = false;
}
