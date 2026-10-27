// vr_imgui.cpp -- Dear ImGui in the game: see vr_imgui.hpp.

#include "vr_imgui.hpp"
#include "vr_engine.hpp"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_impl_opengl3.h"
#include "thirdparty/imgui/imgui_impl_sdl2.h"

#include <SDL.h>

#include <stdio.h>

extern "C" int VR_IsActive(void);

using namespace qvr;

namespace
{

// ImGui's state here (the main thread's: the input loop, the host frame, the swap).
struct ImState
{
    bool want{false};      // a tool is open (this frame)
    bool tried{false};     // ensure() ran (once: a failure is not retried every frame)
    bool ready{false};     // the context and both backends are made
    bool frameOpen{false}; // NewFrame called, Render not yet
    int theme{-1};
    char iniPath[MAX_OSPATH]{};
};
ImState im;

void endOpenFrame()
{
    if(im.frameOpen)
    {
        ImGui::EndFrame();
        im.frameOpen = false;
    }
}

} // namespace

namespace qvr::imgui
{

void want(bool on)
{
    im.want = on;
    if(!on)
    {
        endOpenFrame();
    }
}

bool wanted()
{
    return im.want;
}

bool ready()
{
    return im.ready;
}

bool frameOpen()
{
    return im.frameOpen;
}

bool ensure()
{
    if(im.ready || im.tried)
    {
        return im.ready;
    }
    if(VR_IsActive())
    {
        return false; // (a headset session: the desktop window is its mirror; the tools are flat-screen only)
    }
    SDL_Window* window = static_cast<SDL_Window*>(VID_GetWindow());
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if(!window || !context)
    {
        return false;
    }
    im.tried = true;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    q_snprintf(im.iniPath, sizeof(im.iniPath), "%s/imgui.ini", com_gamedir);
    io.IniFilename = im.iniPath;
    io.LogFilename = nullptr;
    if(!ImGui_ImplSDL2_InitForOpenGL(window, context))
    {
        Con_Printf("imgui: the SDL2 backend could not start\n");
        ImGui::DestroyContext();
        return false;
    }
    if(!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        Con_Printf("imgui: the OpenGL3 backend could not start\n");
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
    setTheme(im.theme < 0 ? 0 : im.theme);
    im.ready = true;
    Con_Printf("imgui: Dear ImGui %s ready (%s)\n", IMGUI_VERSION, im.iniPath);
    return true;
}

void setTheme(int theme)
{
    im.theme = theme;
    if(!ImGui::GetCurrentContext())
    {
        return;
    }
    switch(theme)
    {
    case 1: ImGui::StyleColorsLight(); break;
    case 2: ImGui::StyleColorsClassic(); break;
    default: ImGui::StyleColorsDark(); break;
    }
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 4.f;
    s.FrameRounding = 3.f;
    s.GrabRounding = 3.f;
    s.WindowBorderSize = 1.f;
}

bool beginFrame()
{
    endOpenFrame(); // (a frame begun and never drawn: a skipped screen update)
    if(!im.want || !ensure())
    {
        return false;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    im.frameOpen = true;
    return true;
}

void shutdown()
{
    endOpenFrame();
    if(!im.ready)
    {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    im.ready = false;
}

} // namespace qvr::imgui

// in_sdl.c's IN_SendKeyEvents, each event first: nonzero when ImGui takes it (a tool is open in the game: the game's
// own handling skips it). Zero cost while no tool is open (one flag).
extern "C" int VR_ImGuiEvent(const void* event)
{
    if(!im.want || !im.ready)
    {
        return 0;
    }
    const SDL_Event* ev = static_cast<const SDL_Event*>(event);
    const bool input = ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP || ev->type == SDL_TEXTINPUT ||
                       ev->type == SDL_MOUSEMOTION || ev->type == SDL_MOUSEBUTTONDOWN || ev->type == SDL_MOUSEBUTTONUP ||
                       ev->type == SDL_MOUSEWHEEL;
    if(key_dest != key_game)
    {
        // (the console or a menu over the tool: theirs; ImGui only told the keys let go, so none sticks)
        if(ev->type == SDL_KEYUP || ev->type == SDL_MOUSEBUTTONUP)
        {
            ImGui_ImplSDL2_ProcessEvent(ev);
        }
        return 0;
    }
    ImGui_ImplSDL2_ProcessEvent(ev);
    if(!input)
    {
        return 0; // (window events: the engine's too)
    }
    // The console's key still opens the console (unless ImGui types into a field).
    if((ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP) && ev->key.keysym.scancode == SDL_SCANCODE_GRAVE &&
        !ImGui::GetIO().WantTextInput)
    {
        return 0;
    }
    return 1;
}

// Key_TextEntry in the game (IN_UpdateInputMode, each frame): SDL's text input while a tool is open, or ImGui's fields
// get Backspace, Enter and the arrows but never a typed character (SDL sends SDL_TEXTINPUT only while it is started,
// and the game stops it in play). On while a field is being typed in (a device with an on-screen keyboard shows it),
// else on without the on-screen keyboard (a desktop's typing is ready from the first key: ImGui's WantTextInput comes
// a frame after the click).
extern "C" int VR_ImGuiTextMode(void)
{
    if(!im.want || !im.ready)
    {
        return -1;
    }
    return ImGui::GetIO().WantTextInput ? TEXTMODE_ON : TEXTMODE_NOPOPUP;
}

// GL_EndRendering, after the post-process and before the swap (and vr_screenshot_frames' shot): the frame's windows
// drawn into the window's framebuffer, the GL state the engine keeps (or caches) put back as it was.
extern "C" void VR_ImGuiRender(void)
{
    if(!im.frameOpen)
    {
        return;
    }
    im.frameOpen = false;
    ImGui::Render();
    ImDrawData* data = ImGui::GetDrawData();
    if(!data || data->CmdListsCount == 0)
    {
        return;
    }
    GLint drawFb = 0, readFb = 0, unpackAlign = 4, unpackRow = 0, unpackSkipRows = 0, unpackSkipPixels = 0;
    GLboolean depthMask = GL_TRUE, colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFb);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFb);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlign);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpackRow);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &unpackSkipRows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &unpackSkipPixels);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    const GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    if(srgb)
    {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    ImGui_ImplOpenGL3_RenderDrawData(data); // (its own state saved and put back: programs, buffers, textures, blend, ...)
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlign);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRow);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, unpackSkipRows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, unpackSkipPixels);
    glDepthMask(depthMask);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if(srgb)
    {
        glEnable(GL_FRAMEBUFFER_SRGB);
    }
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFb));
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFb));
}
