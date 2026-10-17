// vr_dynres.cpp -- see vr_dynres.hpp.

#include "vr_dynres.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_tracy.h"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::dynres
{

namespace
{

constexpr float lowestScale = 0.18f;    // vr_render_scale's floor (PR #75: the Steam Frame plays at 0.18)
constexpr float bucketStep = 0.05f;      // the scales tried: the ceiling, then 0.05 lower each
constexpr double unknownPeriodMs = 1000.0 / 90.0; // the display period when the backend gives none (the mock)
constexpr double dropFit = 0.95;         // a drop goes to the bucket whose predicted time is within this of the budget
constexpr double riseFit = 0.85;         // a rise only to a bucket predicted within this of the budget (hysteresis)
constexpr double holdAfterDrop = 1.0;    // seconds after a drop before a rise
constexpr double holdAfterRise = 0.5;    // seconds between rises
constexpr double slowRate = 0.05;        // the slow average's weight of each new frame (about 0.2 s at 90 Hz)
constexpr int ringSize = 32;             // frames remembered with the scale they were drawn at (read back 2-5 late)
constexpr double settleGap = 0.1;        // seconds between two frames' eyes that make the next ones settle (a load),
constexpr int settleFrames = 30;         // ... for this many frames

struct State
{
    int bucket = 0; // 0: the ceiling
    float drawnScale[ringSize]{};
    unsigned long long drawnSerial[ringSize]{};
    double lastCost = -1.0; // the eyes' GPU ms over the drawn scale's square: the last frame's,
    double slowCost = -1.0; // and its slow average
    double changedAt = -1e9; // nowSeconds of the last change
    bool lastWasDrop = false;
    unsigned long long changeSerial = 0; // the first frame drawn at the current bucket
    double lastMs = -1.0;   // the newest frame's eyes' GPU ms (the status line)
    double budgetMs = 0.0;
    bool on = false;        // was on at the last frame (off: reset when it comes back)
    double printedAt = 0.0; // vr_dynres_log 2: the last line's time, and the eyes' GPU ms summed since
    double printSum = 0.0;
    double printMax = 0.0;
    int printFrames = 0;
    double frameAt = 0.0;   // nowSeconds of the last beginFrame
    unsigned long long settledSerial = 0; // frames before this one are not read: VR's start, a load, a hitch
};
State st;

GLuint loadProgram = 0;
bool loadFailed = false;

constexpr const char* loadVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

// Work: the loop's iterations a pixel; Zero: 0 (a uniform, so the compiler can't drop the loop). Alpha 0 over alpha
// blending: no pixel changes.
constexpr const char* loadFs = R"(#version 430
layout(location = 0) uniform float Work;
layout(location = 1) uniform float Zero;
out vec4 out_color;
void main()
{
    float a = gl_FragCoord.x * 0.001, b = gl_FragCoord.y * 0.001;
    for (float i = 0.0; i < Work; i += 1.0)
    {
        a = fract(sin(a * 12.9898 + b) * 43758.5453);
        b = a + b * 0.5;
    }
    out_color = vec4(a, b, a + b, a * b) * Zero;
}
)";

[[nodiscard]] bool enabled()
{
    if(vr_dynres.value < 1.f)
    {
        return false;
    }
    const Backend* const be = backend();
    return be && (vr_dynres.value >= 2.f || ZA_STRCMP(be->name(), "mock") != 0);
}

[[nodiscard]] float ceilingScale()
{
    return za::clamp(vr_dynres_max.value < 0.f ? vr_render_scale.value : vr_dynres_max.value, lowestScale, 2.f);
}

[[nodiscard]] float floorScale()
{
    return za::clamp(vr_dynres_min.value, lowestScale, ceilingScale());
}

[[nodiscard]] int lowestBucket()
{
    return static_cast<int>(za::ceil((ceilingScale() - floorScale()) / bucketStep - 1e-3f));
}

[[nodiscard]] float bucketScale(int bucket)
{
    return za::max(floorScale(), ceilingScale() - static_cast<float>(bucket) * bucketStep);
}

[[nodiscard]] double budgetMs()
{
    const double period = profile::displayPeriodMs();
    return za::clamp(static_cast<double>(vr_dynres_target.value), 0.3, 1.0) * (period > 0.0 ? period : unknownPeriodMs);
}

void change(int bucket, unsigned long long serial, double ms, float drawn)
{
    const float from = bucketScale(st.bucket);
    st.lastWasDrop = bucket > st.bucket;
    st.bucket = bucket;
    st.changedAt = profile::nowSeconds();
    st.changeSerial = serial + ringSize; // (set to the next frame's by beginFrame)
    if(vr_dynres_log.value != 0.f)
    {
        Con_Printf("dynres: %.2f -> %.2f (eyes' GPU %.2f ms at %.2f, budget %.2f ms)\n", static_cast<double>(from),
            static_cast<double>(bucketScale(bucket)), ms, static_cast<double>(drawn), st.budgetMs);
    }
}

} // namespace

float scale()
{
    if(!enabled())
    {
        return za::clamp(vr_render_scale.value, lowestScale, 2.f);
    }
    return bucketScale(za::min(st.bucket, lowestBucket()));
}

void beginFrame()
{
    const bool on = enabled();
    if(!on)
    {
        if(st.on)
        {
            st = State{};
        }
        return;
    }
    st.bucket = za::clamp(st.bucket, 0, lowestBucket());
    const unsigned long long serial = profile::nextFrameSerial();
    // Its start, or after a gap (a map loaded, a hitch): the next frames' GPU times (textures uploaded, shaders made on
    // first use) are not the scene's, and are left out.
    const double now = profile::nowSeconds();
    if(!st.on || now - st.frameAt > settleGap)
    {
        st.settledSerial = serial + settleFrames;
        st.lastCost = st.slowCost = -1.0;
    }
    st.on = true;
    st.frameAt = now;
    st.drawnScale[serial % ringSize] = bucketScale(st.bucket);
    st.drawnSerial[serial % ringSize] = serial;
    if(st.changeSerial > serial)
    {
        st.changeSerial = serial; // the first frame at the new bucket
    }
    VR_TracyPlot("Dynres scale", bucketScale(st.bucket));
}

void gpuFrame(unsigned long long serial, double eyesMs)
{
    if(!st.on || eyesMs <= 0.0 || st.drawnSerial[serial % ringSize] != serial || serial < st.settledSerial)
    {
        return;
    }
    const float drawn = st.drawnScale[serial % ringSize];
    const double cost = eyesMs / (static_cast<double>(drawn) * drawn);
    st.lastMs = eyesMs;
    st.budgetMs = budgetMs();
    if(st.lastCost < 0.0)
    {
        st.lastCost = st.slowCost = cost;
        return;
    }
    // Two frames in a row over: the lesser of them (one late frame, the compositor's, does not drop it).
    const double attack = za::min(cost, st.lastCost);
    st.lastCost = cost;
    st.slowCost += slowRate * (cost - st.slowCost);
    VR_TracyPlot("Dynres budget ms", st.budgetMs);
    if(vr_dynres_log.value >= 2.f) // two lines a second: the scale, the eyes' GPU time (mean, worst)
    {
        st.printSum += eyesMs;
        st.printMax = za::max(st.printMax, eyesMs);
        ++st.printFrames;
        const double now = profile::nowSeconds();
        if(now - st.printedAt >= 0.5)
        {
            Con_Printf("dynres: at %.2f, eyes' GPU %.2f ms mean %.2f max (%d frames), budget %.2f ms\n",
                static_cast<double>(bucketScale(st.bucket)), st.printSum / st.printFrames, st.printMax, st.printFrames,
                st.budgetMs);
            st.printedAt = now;
            st.printSum = st.printMax = 0.0;
            st.printFrames = 0;
        }
    }

    const float current = bucketScale(st.bucket);
    const bool atCurrent = serial >= st.changeSerial; // drawn at the current bucket (else in flight before the change)
    const int lowest = lowestBucket();
    if(atCurrent && attack * current * current > st.budgetMs && st.bucket < lowest)
    {
        int bucket = st.bucket + 1;
        while(bucket < lowest && attack * bucketScale(bucket) * bucketScale(bucket) > st.budgetMs * dropFit)
        {
            ++bucket;
        }
        change(bucket, serial, eyesMs, drawn);
        return;
    }
    if(atCurrent && st.bucket > 0 &&
        profile::nowSeconds() - st.changedAt >= (st.lastWasDrop ? holdAfterDrop : holdAfterRise))
    {
        const float up = bucketScale(st.bucket - 1);
        if(za::max(st.slowCost, attack) * up * up <= st.budgetMs * riseFit)
        {
            change(st.bucket - 1, serial, eyesMs, drawn);
        }
    }
}

bool statusLine(char* out, int size)
{
    if(!st.on)
    {
        return false;
    }
    if(st.lastMs > 0.0)
    {
        q_snprintf(out, size, "Dynamic res x%.2f of %.2f, GPU %.1f of %.1f ms", static_cast<double>(scale()),
            static_cast<double>(ceilingScale()), st.lastMs, st.budgetMs);
    }
    else
    {
        q_snprintf(out, size, "Dynamic res x%.2f of %.2f", static_cast<double>(scale()), static_cast<double>(ceilingScale()));
    }
    return true;
}

void testLoad(int eye, unsigned fbo, int width, int height)
{
    const float load = za::min(za::abs(vr_dynres_test_load.value), 1000.f);
    if(load <= 0.f || width <= 0 || height <= 0)
    {
        return;
    }
    if(vr_dynres_test_load.value < 0.f && eye == 1)
    {
        Cvar_SetValueQuick(&vr_dynres_test_load, 0.f); // a one-frame pulse: this frame's eyes, then none
    }
    if(!loadProgram && !loadFailed)
    {
        loadProgram = gfx::glProgram(loadVs, loadFs, "vr dynres test load");
        loadFailed = !loadProgram;
    }
    if(!loadProgram)
    {
        return;
    }
    const GLuint bound = GL_BoundDrawFramebuffer();
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(loadProgram);
    GL_Uniform1fFunc(0, load * 64.f);
    GL_Uniform1fFunc(1, 0.f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, bound);
}

} // namespace qvr::dynres
