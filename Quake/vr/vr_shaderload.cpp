// vr_shaderload.cpp -- the loading panel while the engine's shaders are queued (gl_shaders.c GL_CreateShadersAsync,
// QUEST_PLAN.md 6zi): each frame the set pumped (GL_ShadersPump: a few milliseconds of submits, the driver's threads
// polled), then "Compiling shaders: N / M" with a progress bar drawn on the CPU into a small image (the console font,
// no shader needed) and shown as the runtime's panel (an OpenXR quad layer: the headset keeps getting frames, no
// runtime dots) and in the window (a blit: small and centred over its last frame dimmed, flatloadbar2). Nothing else runs meanwhile (host.c _Host_Frame returns after it).
// The same panel during a map load: the hull's wait for the world's collision (VR_MapLoadingFrame), and the whole load's
// stages in player words once it has run a second (flatloadbar: qvr::loadbar, VR_LoadCount, VR_LoadPump), flat and VR.

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_startlog.hpp"

#include "vr_api.h"

#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace
{

constexpr int imageWidth = 768;
constexpr int imageHeight = 192;
constexpr double pumpBudget = 0.008; // seconds of submits a frame (one program at least)

struct LoadState
{
    unsigned char pixels[imageWidth * imageHeight * 4]; // RGBA, bottom row first (GL's)
    GLuint windowTexture = 0, windowFbo = 0;            // the window's copy (a blit)
    GLuint backdropTexture = 0, backdropFbo = 0;        // the window's last frame before the panel, dimmed (flatloadbar2)
    int backdropW = 0, backdropH = 0;                   // its size (0: none, black)
    unsigned backdropFrame = 0;                         // the frame it was taken from (VID_LoadingLastFrame; 0: none)
    char windowShot[64] = "";                           // vr_load_panel_shot: the window's next image saved too, as screenshots/<this>
    bool firstShown = false;                            // the first loading frame noted in the start-up trace
    double started = 0.0;                               // its time (Sys_DoubleTime)
    int frames = 0, panelFrames = 0, windowFrames = 0;  // this set's loading frames; those shown in the headset, in the window
    double swapped = 0.0;                               // s in the window's swaps (since the map load's start: its end line)
    int lastDone = -1, steps = 0;                       // the progress seen: how many times it moved
    bool earlyStarted = false;                          // the VR session started for it (qvr::startForLoading)
};
LoadState load;

// The window's pixels read back (the backdrop, the window's shot: the main thread, used within the call).
struct PanelScratch
{
    za::Vector<unsigned char> window;
    auto members() { return qvr::mem::list(window); }
};
qvr::mem::Scratch<PanelScratch> scratch{"loadpanel"};

void put(int x, int y, unsigned rgba)
{
    if(x < 0 || y < 0 || x >= imageWidth || y >= imageHeight)
    {
        return;
    }
    unsigned char* p = load.pixels + ((imageHeight - 1 - y) * imageWidth + x) * 4;
    ZA_MEMCPY(p, &rgba, 4);
}

void fill(int x0, int y0, int w, int h, unsigned rgba)
{
    for(int y = y0; y < y0 + h; y++)
    {
        for(int x = x0; x < x0 + w; x++)
        {
            put(x, y, rgba);
        }
    }
}

[[nodiscard]] unsigned rgba(int r, int g, int b)
{
    const unsigned char c[4] = {static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b), 255};
    unsigned v;
    ZA_MEMCPY(&v, c, 4);
    return v;
}

// A line in the console font (gfx.wad's conchars: 16x16 characters of 8x8, palette 0 clear), `scale` image pixels a
// font pixel, centred; at most as many characters as fit.
void text(const char* s, int y, int scale, bool alt)
{
    lumpinfo_t* info = nullptr;
    const byte* font = static_cast<const byte*>(W_GetLumpName("conchars", &info));
    if(!font || !info || info->disksize < 128 * 128)
    {
        return;
    }
    const int fit = (imageWidth - 24) / (8 * scale); // (clear of the edge)
    const int n = za::min(static_cast<int>(ZA_STRLEN(s)), fit);
    const int x0 = (imageWidth - n * 8 * scale) / 2;
    for(int i = 0; i < n; i++)
    {
        const int c = (static_cast<unsigned char>(s[i]) & 127) | (alt ? 128 : 0);
        const byte* glyph = font + (c / 16) * 128 * 8 + (c % 16) * 8;
        for(int gy = 0; gy < 8; gy++)
        {
            for(int gx = 0; gx < 8; gx++)
            {
                const byte index = glyph[gy * 128 + gx];
                if(index)
                {
                    fill(x0 + (i * 8 + gx) * scale, y + gy * scale, scale, scale, d_8to24table[index] | 0xff000000u);
                }
            }
        }
    }
}

// The panel: a heading, a bar done/total full, a line under it and a footer (the shaders' and a map load's: shared).
void drawPanel(const char* heading, int done, int total, const char* last, const char* footer)
{
    const unsigned background = rgba(18, 14, 10);
    fill(0, 0, imageWidth, imageHeight, background);
    const unsigned edge = rgba(110, 80, 40);
    fill(0, 0, imageWidth, 3, edge);
    fill(0, imageHeight - 3, imageWidth, 3, edge);
    fill(0, 0, 3, imageHeight, edge);
    fill(imageWidth - 3, 0, 3, imageHeight, edge);

    text("QUAKE VR", 14, 4, true);
    text(heading, 58, 2, false);

    const int barX = 48, barY = 88, barW = imageWidth - 96, barH = 22;
    fill(barX - 2, barY - 2, barW + 4, barH + 4, edge);
    fill(barX, barY, barW, barH, rgba(30, 24, 18));
    const int filled = total > 0 ? barW * za::min(done, total) / total : 0;
    fill(barX, barY, filled, barH, rgba(200, 150, 60));

    text(last && *last ? last : "", 124, 2, false);
    text(footer, 156, 2, true);
}

void drawImage(int done, int total, const char* last, double seconds)
{
    char heading[64];
    q_snprintf(heading, sizeof(heading), "Compiling shaders: %d / %d", done, total);
    drawPanel(heading, done, total, last,
        VR_ProgCacheOn() ? va("%.0f s - the next starts take them from the program cache", seconds) : va("%.0f s", seconds));
}

void upload(GLuint texture)
{
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imageWidth, imageHeight, GL_RGBA, GL_UNSIGNED_BYTE, load.pixels);
}

// The headset: the image into the runtime's panel (its quad layer) and the frame ended with no eyes.
void toHeadset()
{
    qvr::Backend* be = qvr::backend();
    if(!be || !be->frameActive())
    {
        return;
    }
    const unsigned image = qvr::frameState().shouldRender ? be->acquirePanelImage(imageWidth, imageHeight) : 0;
    if(image)
    {
        upload(image);
        be->releasePanelImage();
        load.panelFrames++;
    }
    be->endFrame(false);
}

// The window's w x h pixels from `buffer` (GL_FRONT / GL_BACK of the window's framebuffer, bound for reading) into the
// scratch (RGBA, bottom row first), each scaled by `shift` (1: halved) and made opaque.
[[nodiscard]] za::Vector<unsigned char>& readWindow(int w, int h, GLenum buffer, int shift)
{
    za::Vector<unsigned char>& px = scratch.window;
    px.clear();
    px.resize(static_cast<za::SizeT>(w) * static_cast<za::SizeT>(h) * 4, static_cast<unsigned char>(0));
    glReadBuffer(buffer);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glReadBuffer(GL_BACK);
    const unsigned keep = shift ? 0x7f7f7f7fu : 0xffffffffu, opaque = rgba(0, 0, 0);
    for(za::SizeT i = 0; i < px.size(); i += 4)
    {
        unsigned v;
        ZA_MEMCPY(&v, px.data() + i, 4);
        v = ((v >> shift) & keep) | opaque;
        ZA_MEMCPY(px.data() + i, &v, 4);
    }
    return px;
}

// The panel's backdrop (flatloadbar2, his words: "the previous screen (e.g. console) visible underneath"): the window's
// last frame (the console, a menu, the map before: a load draws none), taken when the panel first shows after it and
// halved in brightness (a 50% black over it), then put under each of the panel's frames. Read once a load from the
// buffer that holds it (VID_LoadingLastFrame), so a frame of the panel never dims what it drew itself, whichever of the
// window's two buffers it lands in. None (the start's shaders: nothing drawn yet): black.
void takeBackdrop(int w, int h)
{
    int buffer = GL_BACK;
    const unsigned frame = VID_LoadingLastFrame(&buffer);
    if(frame == load.backdropFrame)
    {
        return; // (the same frame: kept; the window resized meanwhile: stretched to it)
    }
    load.backdropFrame = frame;
    load.backdropW = load.backdropH = 0;
    if(!frame)
    {
        return;
    }
    const double started = Sys_DoubleTime();
    const za::Vector<unsigned char>& px = readWindow(w, h, static_cast<GLenum>(buffer), 1);
    if(!load.backdropTexture)
    {
        glGenTextures(1, &load.backdropTexture);
        GL_GenFramebuffersFunc(1, &load.backdropFbo);
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, load.backdropTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, load.backdropFbo);
    GL_FramebufferTexture2DFunc(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, load.backdropTexture, 0);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
    load.backdropW = w;
    load.backdropH = h;
    if(developer.value) // (a safe print: no redraw from inside the panel's frame)
    {
        Con_SafePrintf("loading panel: the window's last frame (%dx%d, the %s buffer) taken as its backdrop in %.1f ms\n", w,
            h, buffer == GL_FRONT ? "front" : "back", (Sys_DoubleTime() - started) * 1000.0);
    }
}

// The backdrop's texture given back (the load over); its frame kept, so the panel's own frames never become one.
void dropBackdrop()
{
    if(load.backdropTexture)
    {
        GL_DeleteFramebuffersFunc(1, &load.backdropFbo);
        glDeleteTextures(1, &load.backdropTexture);
        load.backdropTexture = load.backdropFbo = 0;
    }
    load.backdropW = load.backdropH = 0;
}

// The panel's width in a w x h window (his words: "much smaller, centered"): a third of the window at most, scaled
// with it, at a ratio to the image that keeps the console font sharp: half (the image draws in 2x2 blocks: each a pixel)
// or a whole multiple (at least half: a small window).
[[nodiscard]] int panelWidth(int w, int h)
{
    const int most = za::min(w / 3, (h / 3) * imageWidth / imageHeight);
    return most < imageWidth ? imageWidth / 2 : imageWidth * (most / imageWidth);
}

// The window: its last frame dimmed (takeBackdrop), the image blitted over it, centred, then shown.
void toWindow()
{
    if(VR_NoWindow())
    {
        return;
    }
    if(!load.windowTexture)
    {
        glGenTextures(1, &load.windowTexture);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, load.windowTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL_GenFramebuffersFunc(1, &load.windowFbo);
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, load.windowFbo);
        GL_FramebufferTexture2DFunc(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, load.windowTexture, 0);
    }
    upload(load.windowTexture);
    int w = 0, h = 0;
    VID_LoadingSize(&w, &h);
    if(w <= 0 || h <= 0)
    {
        return;
    }
    // (mid-load, between a load's own GL work: the state this changes put back after it)
    GLint viewport[4] = {}, drawFbo = 0, readFbo = 0;
    GLfloat clearColour[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColour);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, w, h);
    takeBackdrop(w, h);
    if(load.backdropW > 0)
    {
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, load.backdropFbo);
        GL_BlitFramebufferFunc(0, 0, load.backdropW, load.backdropH, 0, 0, w, h, GL_COLOR_BUFFER_BIT,
            load.backdropW == w && load.backdropH == h ? GL_NEAREST : GL_LINEAR);
    }
    else
    {
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    const int dw = panelWidth(w, h);
    const int dh = dw * imageHeight / imageWidth;
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, load.windowFbo);
    GL_BlitFramebufferFunc(0, 0, imageWidth, imageHeight, (w - dw) / 2, (h - dh) / 2, (w - dw) / 2 + dw, (h - dh) / 2 + dh,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    if(load.windowShot[0]) // (a test aid: what the window shows, before it is shown)
    {
        char path[MAX_OSPATH];
        q_snprintf(path, sizeof(path), "%s/screenshots/%s", com_gamedir, load.windowShot);
        COM_CreatePath(path);
        const bool saved = Image_WritePNGPath(path, readWindow(w, h, GL_BACK, 0).data(), w, h, 32, false);
        Con_SafePrintf("loading panel: %s screenshots/%s (the window, %dx%d: the panel %dx%d, %s)\n",
            saved ? "saved" : "couldn't save", load.windowShot, w, h, dw, dh,
            load.backdropW > 0 ? "over the last frame dimmed" : "on black");
        load.windowShot[0] = '\0';
    }
    const double swapStart = Sys_DoubleTime();
    VID_LoadingSwap();
    load.swapped += Sys_DoubleTime() - swapStart;
    load.windowFrames++;
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFbo));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glClearColor(clearColour[0], clearColour[1], clearColour[2], clearColour[3]);
    if(scissor)
    {
        glEnable(GL_SCISSOR_TEST);
    }
}

// One frame of a map load's panel (the hull's wait and the whole load's stages: shared): drawn, into the headset (VR
// running, or started for it at a start) and the window. Returns the frames that reached the headset.
int mapPanelFrame(const char* heading, const char* detail, double share, const char* footer, bool& earlyStarted)
{
    if(!qvr::backend() && qvr::startForLoading())
    {
        earlyStarted = true;
    }
    constexpr int barSteps = 1000;
    drawPanel(heading, static_cast<int>(za::max(0.0, za::min(share, 1.0)) * barSteps), barSteps, detail, footer);
    int shown = 0;
    if(qvr::loadingFrameBegin())
    {
        const int before = load.panelFrames;
        toHeadset();
        shown = load.panelFrames - before;
    }
    toWindow();
    return shown;
}

// The next frame's window image saved too, as screenshots/<file's name>_window.png (vr_load_panel_shot: before the frame).
void armWindowShot(const char* file)
{
    const int stem = static_cast<int>(ZA_STRLEN(file)) - 4; // (less ".png")
    q_snprintf(load.windowShot, sizeof(load.windowShot), "%.*s_window.png", za::max(stem, 0), file);
}

// The panel's image as drawn last saved as screenshots/<file> (vr_load_panel_shot, a test aid).
void savePanel(const char* file, double share, const char* heading)
{
    load.windowShot[0] = '\0'; // (no window: none saved)
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/screenshots/%s", com_gamedir, file);
    COM_CreatePath(path);
    const bool saved = Image_WritePNGPath(path, load.pixels, imageWidth, imageHeight, 32, false); // (bottom row first: flipped)
    Con_SafePrintf("loading panel: %s screenshots/%s at %.0f%% (\"%s\")\n", saved ? "saved" : "couldn't save", file,
        share * 100.0, heading);
}

} // namespace

extern "C" int VR_ShadersLoadingFrame(void)
{
    if(!GL_ShadersPump(pumpBudget))
    {
        return 0;
    }
    const double now = Sys_DoubleTime();
    if(!load.firstShown)
    {
        load.firstShown = true;
        load.started = now;
        load.frames = load.panelFrames = load.windowFrames = load.steps = 0;
        load.lastDone = -1;
        load.earlyStarted = false;
        VR_StepBegin("shaders queued: the loading panel's frames"); // (the start-up trace: ended at VR_ShadersLoaded)
    }
    int done = 0, total = 0;
    const char* last = "";
    GL_ShadersProgress(&done, &total, &last);
    if(done != load.lastDone)
    {
        load.steps++;
        load.lastDone = done;
    }
    load.frames++;
    // A program compiled from its sources (a first start, a changed set: seconds, not the cache's fraction of one): the
    // VR session started now, before the configs, so the panel reaches the headset (QUEST_PLAN.md 6zv). All from the
    // cache: the start-up as before (the session after the configs).
    if(GL_ShadersCompiling() > 0 && qvr::startForLoading())
    {
        load.earlyStarted = true;
    }
    drawImage(done, total, last, now - load.started);
    toHeadset();
    toWindow();
    return 1;
}

// The set done (GL_ShadersPump): the trace's step ended; the pump's numbers (developer 1).
extern "C" void VR_ShadersLoaded(int programs, int frames, double ms, double maxwork, double maxgap)
{
    if(load.firstShown)
    {
        VR_StepEnd();
    }
    Con_SafePrintf("loading panel: %d frames (%d in the headset, %d in the window), progress moved %d times%s\n", load.frames,
        load.panelFrames, load.windowFrames, load.steps, load.earlyStarted ? "; the VR session started early for it" : "");
    Con_DPrintf("loading panel: %d programs, %d pumps, %.0f ms, longest frame's work %.1f ms (between frames %.1f ms)\n",
        programs, frames, ms, maxwork, maxgap);
    load.firstShown = false;
    dropBackdrop();
}

// A map load's long wait (vr_hull.cpp settle: the world's collision compiled on the pool; vrstart's first load after it
// changed, i.e. after every update: ~45 s on a Quest 3, the main thread idle meanwhile). Without frames the headset showed
// the runtime's dots that long (at a start the VR session isn't even up: it starts after the configs' frame), and a player
// who closed the "stuck" game lost the compile (no cache written) and met it again at every start (questrelaunch). Each
// call: one frame of the same panel (the session started for it at a start, as for the shaders), unless the frame the
// host began is still open, which it ends. The host frame's own frame stays ended (its views skipped: frameActive).
// Returns whether a frame reached the headset (the runtime paced it), so its caller needn't sleep.
namespace
{
struct MapWait
{
    int frames = 0, panelFrames = 0; // this wait's frames: pumped, shown in the headset
    bool earlyStarted = false;       // the VR session started for it
};
MapWait mapWait;
} // namespace

// The headset (VR running, or started for it at a start) and the window (flat-screen, or the desktop beside VR; not
// the Quest's immersive app: VR_NoWindow).
extern "C" int VR_MapLoadingWanted(void)
{
    return qvr::vr_load_panel.value != 0.f && cls.state != ca_dedicated &&
           (qvr::backend() || host_framecount == 0 || !VR_NoWindow());
}

extern "C" int VR_MapLoadingFrame(const char* heading, const char* detail, double done, double seconds)
{
    if(!VR_MapLoadingWanted())
    {
        return 0;
    }
    mapWait.frames++;
    const double share = za::max(0.0, za::min(done, 1.0));
    const bool shot = qvr::vr_load_panel_shot.value > 0.f && share >= qvr::vr_load_panel_shot.value;
    if(shot) // (a test aid: the image saved once, as drawn below)
    {
        Cvar_SetValueQuick(&qvr::vr_load_panel_shot, 0.f);
        armWindowShot("load_panel.png");
    }
    const int shown = mapPanelFrame(heading, detail, share, va("%.0f s - the next loads take it from the cache", seconds),
        mapWait.earlyStarted);
    mapWait.panelFrames += shown;
    if(shot)
    {
        savePanel("load_panel.png", share, heading);
    }
    return shown; // (none shown, e.g. flat-screen, the headset asleep or a mock backend that doesn't pace: the caller sleeps)
}

extern "C" void VR_MapLoadingDone(const char* map, double seconds)
{
    if(mapWait.frames)
    {
        Con_SafePrintf("loading panel: %s's collision, %.1f s: %d frames (%d in the headset)%s\n", map, seconds,
            mapWait.frames, mapWait.panelFrames, mapWait.earlyStarted ? "; the VR session started early for it" : "");
    }
    mapWait = MapWait{};
}

// The whole load's panel (flatloadbar, ROUND26.md): his flat vrstart took 13 s with nothing shown (the entities' spawn
// 6.2 s, the model light grid rebuilt 5.1 s). While a map load runs (from its command, vr_startup.cpp's timing, to its
// first frame drawn), the stages it is in (the trace's steps, VR_StepBegin; ended by VR_StepEnd or VR_TimeMark) are
// followed whether or not the trace runs; once the load has run a second, the innermost stage with words is shown
// with a bar: its own count where it counts (VR_LoadCount: the entities, the light grid's layers, the models, the
// sounds), else the share of the load's stages done (the last load's count of them the whole). Drawn from inside the
// long stages (their counts, the model loads: VR_LoadPump) and at each stage's start and end, at most every 100 ms.
namespace
{

constexpr double showAfter = 1.0;  // s a load runs before its panel shows (a fast load shows nothing)
constexpr double drawEvery = 0.1;  // s between its frames, at most (each a few ms: the image, the swap, the headset's frame)
constexpr int stageDepth = 8;      // stages nested (deeper ones: not followed)
constexpr int stageName = 96;      // a stage's name kept (VR_StepBegin's)
constexpr int defaultMarks = 60;   // the stages of a load, before one was counted (vrstart's: ~55)

// A stage's words (its name's start, VR_StepBegin's or VR_TimeMark's): the panel's heading; what it counts (VR_LoadCount;
// "%": shown as a share; nullptr: it doesn't) and the shot's name for it (vr_load_panel_shot); a note under the bar.
struct StageWords
{
    const char* stage;
    const char* heading;
    const char* unit;
    const char* shot;
    const char* note;
};
constexpr StageWords stageWords[] = {
    {"server: entities spawned", "Spawning the map's monsters and items", "entities", "load_panel_entities.png", nullptr},
    {"server: monsters' limbs", "Preparing the monsters' limbs", nullptr, nullptr, nullptr},
    {"VR_NewMap: model light grid", "Lighting the models", "%", "load_panel_lightgrid.png", "built once for each map version"},
    {"client: models precached", "Loading models", "models", "load_panel_models.png", nullptr},
    {"client: sounds precached", "Loading sounds", "sounds", "load_panel_sounds.png", nullptr},
    {"VR_NewMap: view models", "Preparing your hands and weapons", nullptr, nullptr, nullptr},
    {"VR_NewMap: liquids", "Preparing water, slime and lava", nullptr, nullptr, nullptr},
    {"VR_NewMap: ", "Preparing effects", nullptr, nullptr, nullptr},
    {"client: R_NewMap", "Preparing textures and lightmaps", nullptr, nullptr, nullptr},
    {"server: world model", "Loading the map's geometry", nullptr, nullptr, nullptr},
    {"server: progs", "Loading the game code", nullptr, nullptr, nullptr},
    {"server: 2 frames", "Settling the physics", nullptr, nullptr, nullptr},
};

struct BarStage
{
    char name[stageName];
    int done, total; // its count (VR_LoadCount; total 0: none)
};

struct LoadBar
{
    bool active = false;
    double start = 0.0;                  // the load's (its command's) start (Sys_DoubleTime)
    double lastDraw = 0.0, drawn = 0.0;  // the last frame's time; the time spent drawing them (s: the panel's cost)
    double longestGap = 0.0;             // s: the longest time between two of its frames (a stage that doesn't pump)
    char gapAt[stageName] = "";          // ... the stage it ended in
    char lastAt[stageName] = "";         // the stage of the last frame
    BarStage stages[stageDepth]{};
    int depth = 0, lost = 0;             // stages open (followed; past stageDepth)
    int marks = 0;                       // stages ended (VR_TimeMark)
    int frames = 0, panelFrames = 0;     // frames drawn; those in the headset
    unsigned shots = 0;                  // vr_load_panel_shot's images saved this load (a bit per stageWords entry)
    bool earlyStarted = false;           // the VR session started for it
};
LoadBar bar;
int expectedMarks = defaultMarks; // the last load's stages

[[nodiscard]] const StageWords* wordsOf(const char* name)
{
    for(const StageWords& w : stageWords)
    {
        if(!ZA_STRNCMP(name, w.stage, ZA_STRLEN(w.stage)))
        {
            return &w;
        }
    }
    return nullptr;
}

// The innermost stage with words (and it), or none.
[[nodiscard]] const StageWords* shownStage(const BarStage*& stage)
{
    for(int i = za::min(bar.depth, stageDepth); i-- > 0;)
    {
        if(const StageWords* w = wordsOf(bar.stages[i].name))
        {
            stage = &bar.stages[i];
            return w;
        }
    }
    stage = nullptr;
    return nullptr;
}

[[nodiscard]] BarStage* innermost()
{
    return bar.depth > 0 && bar.lost == 0 ? &bar.stages[bar.depth - 1] : nullptr;
}

// The panel's frame if one is due (vr_load_panel_shot: from the load's start, and at once when a counted stage reaches
// its share, its image saved).
void pump()
{
    if(!bar.active || !host_initialized || !VR_OnMainThread())
    {
        return;
    }
    const double now = Sys_DoubleTime();
    const float shotAt = qvr::vr_load_panel_shot.value;
    const StageWords* shotOf = nullptr;
    if(shotAt > 0.f)
    {
        const BarStage* s = innermost();
        const StageWords* w = s ? wordsOf(s->name) : nullptr;
        if(w && w->shot && s->total > 0 && s->done >= shotAt * static_cast<float>(s->total) &&
            !(bar.shots & (1u << static_cast<unsigned>(w - stageWords))))
        {
            shotOf = w;
        }
    }
    if(!shotOf && (now - bar.lastDraw < drawEvery || (shotAt <= 0.f && now - bar.start < showAfter)))
    {
        return;
    }
    if(!VR_MapLoadingWanted())
    {
        return;
    }
    const BarStage* stage = nullptr;
    const StageWords* words = shownStage(stage);
    const char* map = sv.name[0] ? sv.name : cl.mapname[0] ? cl.mapname : "the map";
    char heading[64], detail[96], footer[64];
    double share;
    if(words && words->unit && stage->total > 0)
    {
        share = static_cast<double>(stage->done) / stage->total;
        if(words->unit[0] == '%')
        {
            q_snprintf(detail, sizeof(detail), "%s: %.0f%%", map, share * 100.0);
        }
        else
        {
            q_snprintf(detail, sizeof(detail), "%s: %d of %d %s", map, stage->done, stage->total, words->unit);
        }
    }
    else
    {
        expectedMarks = za::max(expectedMarks, bar.marks + 1);
        share = static_cast<double>(bar.marks) / expectedMarks;
        q_snprintf(detail, sizeof(detail), "%s: step %d of about %d", map, bar.marks + 1, expectedMarks);
    }
    q_strlcpy(heading, words ? words->heading : va("Loading %s", map), sizeof(heading));
    if(words && words->note)
    {
        q_snprintf(footer, sizeof(footer), "%.0f s - %s", now - bar.start, words->note);
    }
    else
    {
        q_snprintf(footer, sizeof(footer), "%.0f s", now - bar.start);
    }
    if(shotOf)
    {
        armWindowShot(shotOf->shot);
    }
    bar.frames++; // (first: VR_LoadPanelOwnsWindow from its first frame)
    bar.panelFrames += mapPanelFrame(heading, detail, share, footer, bar.earlyStarted);
    if(shotOf)
    {
        bar.shots |= 1u << static_cast<unsigned>(shotOf - stageWords);
        savePanel(shotOf->shot, share, heading);
    }
    const char* at = bar.depth > 0 ? bar.stages[za::min(bar.depth, stageDepth) - 1].name : "the load";
    if(bar.frames > 1)
    {
        const double gap = now - bar.lastDraw;
        if(gap > bar.longestGap)
        {
            bar.longestGap = gap;
            q_strlcpy(bar.gapAt, at, sizeof(bar.gapAt));
        }
        if(gap > 0.5) // (developer 1: where the panel stood still)
        {
            Con_DPrintf("loading panel: %.2f s without a frame, from \"%s\" to \"%s\"\n", gap, bar.lastAt, at);
        }
    }
    q_strlcpy(bar.lastAt, at, sizeof(bar.lastAt));
    bar.lastDraw = Sys_DoubleTime();
    bar.drawn += bar.lastDraw - now;
}

} // namespace

namespace qvr::loadbar
{

void begin(double start)
{
    if(bar.active)
    {
        return; // (a command's load: begun at the command; its spawn and its client's start continue it)
    }
    bar = LoadBar{};
    bar.active = true;
    bar.start = start;
    load.swapped = 0.0;
}

void stepBegin(const char* step)
{
    if(!bar.active)
    {
        return;
    }
    if(bar.depth >= stageDepth)
    {
        bar.lost++;
        return;
    }
    BarStage& s = bar.stages[bar.depth++];
    q_strlcpy(s.name, step, sizeof(s.name));
    s.done = s.total = 0;
    pump();
}

void stepEnd()
{
    if(!bar.active)
    {
        return;
    }
    if(bar.lost > 0)
    {
        bar.lost--;
    }
    else if(bar.depth > 0)
    {
        bar.depth--;
    }
    pump();
}

void mark(const char* stage)
{
    if(!bar.active)
    {
        return;
    }
    bar.marks++;
    if(const BarStage* s = innermost(); s && !ZA_STRNCMP(s->name, stage, sizeof(s->name) - 1))
    {
        bar.depth--;
    }
    pump();
}

void end()
{
    if(!bar.active)
    {
        return;
    }
    const double seconds = Sys_DoubleTime() - bar.start;
    if(bar.frames)
    {
        Con_SafePrintf("loading panel: %s's load, %.1f s: %d frames (%d in the headset), %.1f ms drawing them (%.1f in the "
                       "window's swaps), the longest gap %.2f s (ending in \"%s\")%s\n",
            sv.name[0] ? sv.name : cl.mapname, seconds, bar.frames, bar.panelFrames, bar.drawn * 1000.0, load.swapped * 1000.0,
            bar.longestGap, bar.gapAt, bar.earlyStarted ? "; the VR session started early for it" : "");
    }
    if(bar.marks >= 10)
    {
        expectedMarks = bar.marks; // (the next load's whole: a map's loads differ little)
    }
    dropBackdrop();
    bar = LoadBar{};
}

} // namespace qvr::loadbar

extern "C" void VR_LoadCount(int done, int total)
{
    if(!VR_OnMainThread() || !bar.active)
    {
        return;
    }
    if(BarStage* s = innermost())
    {
        s->done = done;
        s->total = total;
    }
    pump();
}

extern "C" void VR_LoadPump(void)
{
    if(VR_OnMainThread() && bar.active)
    {
        pump();
    }
}

// A flat load over the console (not connected: each print redraws it, and so does each host frame before the signon,
// a whole frame shown) would flash the console between the panel's frames: while the panel is up, neither draws (the
// load spares their frames). With VR running the frames go on as before (they feed the headset's frame loop).
extern "C" int VR_LoadPanelOwnsWindow(void)
{
    return !qvr::backend() && bar.active && bar.frames > 0 ? 1 : 0; // (the load's end, or its failure, ends it: never left on)
}
