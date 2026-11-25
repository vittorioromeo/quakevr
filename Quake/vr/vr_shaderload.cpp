// vr_shaderload.cpp -- the loading panel while the engine's shaders are queued (gl_shaders.c GL_CreateShadersAsync,
// QUEST_PLAN.md 6zi): each frame the set pumped (GL_ShadersPump: a few milliseconds of submits, the driver's threads
// polled), then "Compiling shaders: N / M" with a progress bar drawn on the CPU into a small image (the console font,
// no shader needed) and shown as the runtime's panel (an OpenXR quad layer: the headset keeps getting frames, no
// runtime dots) and in the window (a blit). Nothing else runs meanwhile (host.c _Host_Frame returns after it).

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"

#include "vr_api.h"

#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Strlen.hpp"
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
    bool firstShown = false;                            // the first loading frame noted in the start-up trace
    double started = 0.0;                               // its time (Sys_DoubleTime)
    int frames = 0, panelFrames = 0, windowFrames = 0;  // this set's loading frames; those shown in the headset, in the window
    int lastDone = -1, steps = 0;                       // the progress seen: how many times it moved
    bool earlyStarted = false;                          // the VR session started for it (qvr::startForLoading)
};
LoadState load;

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

// The window: the image blitted in, centred at its aspect, then shown.
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
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, w, h);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    const int dw = za::min(w * 9 / 10, (h * 9 / 10) * imageWidth / imageHeight);
    const int dh = dw * imageHeight / imageWidth;
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, load.windowFbo);
    GL_BlitFramebufferFunc(0, 0, imageWidth, imageHeight, (w - dw) / 2, (h - dh) / 2, (w - dw) / 2 + dw, (h - dh) / 2 + dh,
        GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
    VID_LoadingSwap();
    load.windowFrames++;
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

extern "C" int VR_MapLoadingWanted(void)
{
    return qvr::vr_load_panel.value != 0.f && cls.state != ca_dedicated && (qvr::backend() || host_framecount == 0);
}

extern "C" int VR_MapLoadingFrame(const char* map, int done, int total, double seconds)
{
    if(!VR_MapLoadingWanted())
    {
        return 0;
    }
    if(!qvr::backend() && qvr::startForLoading())
    {
        mapWait.earlyStarted = true;
    }
    if(!qvr::loadingFrameBegin())
    {
        return 0;
    }
    mapWait.frames++;
    char heading[96];
    q_snprintf(heading, sizeof(heading), "Preparing %s: %d / %d", map, done, total);
    drawPanel(heading, done, total, "Its collision is built once for this version",
        va("%.0f s - the next loads take it from the cache", seconds));
    const int before = load.panelFrames;
    toHeadset();
    const int shown = load.panelFrames - before;
    mapWait.panelFrames += shown;
    return shown; // (none shown, e.g. the headset asleep or a mock backend that doesn't pace: the caller sleeps)
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
