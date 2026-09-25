// vr_water.cpp -- see vr_water.hpp.

#include "vr_water.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_stereo.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace qvr::water
{
namespace
{

// ----------------------------------------------------------------------------
// Where the liquids are, for the caustics: a 3D texture over the box round the map's water and slime leaves, 1 in
// them, linearly filtered, 0 past its border. The shader reads it a cell out from a surface, on the side it is seen
// from: the floors and walls of a pool are lit, a dry ledge over the water or the far side of a pool's wall not.

GLuint volumeTex = 0;
const qmodel_t* volumeModel = nullptr;
char volumeName[MAX_QPATH] = {};
bool volumeWet = false;
float volumeOrigin[3] = {};
float volumeScale[3] = {};
float volumeCell = 16.f;

// A water or slime leaf, not a teleport's (the thin water brush a teleport's face is on).
bool wetLeaf(const qmodel_t* m, const mleaf_t* leaf)
{
    if(leaf->contents != CONTENTS_WATER && leaf->contents != CONTENTS_SLIME)
    {
        return false;
    }
    bool liquid = false, tele = false;
    for(int i = 0; i < leaf->nummarksurfaces; i++)
    {
        const int s = leaf->firstmarksurface[i];
        if(s < 0 || s >= m->numsurfaces)
        {
            continue;
        }
        const int flags = m->surfaces[s].flags;
        tele |= (flags & SURF_DRAWTELE) != 0;
        liquid |= (flags & (SURF_DRAWWATER | SURF_DRAWSLIME)) != 0;
    }
    return liquid || !tele;
}

void buildVolume(qmodel_t* m)
{
    volumeModel = m;
    q_strlcpy(volumeName, m->name, sizeof(volumeName));
    volumeWet = false;

    std::vector<char> wet(static_cast<std::size_t>(m->numleafs) + 1, 0);
    float mins[3] = {1e9f, 1e9f, 1e9f}, maxs[3] = {-1e9f, -1e9f, -1e9f};
    for(int i = 0; i <= m->numleafs; i++)
    {
        const mleaf_t* leaf = m->leafs + i;
        if(!wetLeaf(m, leaf))
        {
            continue;
        }
        wet[i] = 1;
        volumeWet = true;
        for(int a = 0; a < 3; a++)
        {
            mins[a] = std::min(mins[a], leaf->minmaxs[a]);
            maxs[a] = std::max(maxs[a], leaf->minmaxs[3 + a]);
        }
    }
    if(!volumeWet)
    {
        return;
    }

    // 16-unit cells, coarser for big water (at most 256 a side, 4M cells); a cell of margin all round.
    float cell = 16.f;
    int dims[3];
    for(;;)
    {
        std::size_t total = 1;
        bool fits = true;
        for(int a = 0; a < 3; a++)
        {
            dims[a] = static_cast<int>(std::ceil((maxs[a] - mins[a]) / cell)) + 2;
            fits &= dims[a] <= 256;
            total *= static_cast<std::size_t>(dims[a]);
        }
        if(fits && total <= (4u << 20))
        {
            break;
        }
        cell *= 1.25f;
    }
    for(int a = 0; a < 3; a++)
    {
        volumeOrigin[a] = mins[a] - cell;
        volumeScale[a] = 1.f / (dims[a] * cell);
    }

    volumeCell = cell;

    const int nx = dims[0], ny = dims[1], nz = dims[2];
    std::vector<unsigned char> texels(static_cast<std::size_t>(nx) * ny * nz);
    for(int z = 0; z < nz; z++)
    {
        for(int y = 0; y < ny; y++)
        {
            for(int x = 0; x < nx; x++)
            {
                vec3_t p = {volumeOrigin[0] + (x + 0.5f) * cell, volumeOrigin[1] + (y + 0.5f) * cell,
                    volumeOrigin[2] + (z + 0.5f) * cell};
                const std::ptrdiff_t li = Mod_PointInLeaf(p, m) - m->leafs;
                texels[(static_cast<std::size_t>(z) * ny + y) * nx + x] =
                    li >= 0 && li <= m->numleafs && wet[li] ? 255 : 0;
            }
        }
    }

    if(!volumeTex)
    {
        glGenTextures(1, &volumeTex);
    }
    GL_BindNative(GL_TEXTURE7, GL_TEXTURE_3D, volumeTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GL_TexImage3DFunc(GL_TEXTURE_3D, 0, GL_R8, nx, ny, nz, 0, GL_RED, GL_UNSIGNED_BYTE, texels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    const float border[4] = {0.f, 0.f, 0.f, 0.f};
    glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, border);
    GL_ObjectLabelFunc(GL_TEXTURE, volumeTex, -1, "vr liquid volume");
    Con_DPrintf("VR water: liquid volume %d x %d x %d, %g-unit cells from %g %g %g\n", nx, ny, nz, cell, volumeOrigin[0],
        volumeOrigin[1], volumeOrigin[2]);
}

void ensureVolume()
{
    qmodel_t* m = cl.worldmodel;
    if(!m)
    {
        return;
    }
    if(m != volumeModel || std::strcmp(m->name, volumeName) != 0)
    {
        buildVolume(m);
    }
}

// Unit 6 reads the opaque scene (refraction) and nothing else: smoothly, clamped.
GLuint linearSampler = 0;

void ensureSampler()
{
    if(linearSampler)
    {
        return;
    }
    GL_GenSamplersFunc(1, &linearSampler);
    GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_BindSamplerFunc(6, linearSampler);
}

// How far the opaque scene is, for the refraction (unit 8): what is in front of a translucent liquid's surface is not
// read through it (gl_shaders.h, LiquidRefract). Made once a view, as the first translucent liquid draws, by a small
// pass: the scene's depth/stencil is the translucent pass's target then (depth tested, its stencil written), which a
// shader may not read. Half the size, in R32F, each texel the distance (along the view) of the nearest of its four:
// at 3292 x 3524 it reads the 46 MB of depth once and writes 12 MB, only with translucent liquids in view and the
// refraction on.
constexpr const char* distanceVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char* distanceFs = R"(#version 430
layout(binding = 0) uniform sampler2D Depth;
layout(location = 0) uniform vec3 Proj; // clip z = x * w + y; z 1: depth is clip z (reversed Z), 0: (clip z + 1) / 2
layout(location = 0) out float Out;
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy) * 2;
    ivec2 last = textureSize(Depth, 0) - 1;
    float a = texelFetch(Depth, min(p, last), 0).r;
    float b = texelFetch(Depth, min(p + ivec2(1, 0), last), 0).r;
    float c = texelFetch(Depth, min(p + ivec2(0, 1), last), 0).r;
    float d = texelFetch(Depth, min(p + ivec2(1, 1), last), 0).r;
    float z = Proj.z > 0.5 ? max(max(a, b), max(c, d)) : min(min(a, b), min(c, d)) * 2.0 - 1.0;
    Out = Proj.y / (z - Proj.x);
}
)";

GLuint distanceProgram = 0;
bool distanceFailed = false;

// Two, used in turn: the next view's does not wait for the last one's reads.
struct Distances
{
    GLuint texture = 0;
    GLuint fbo = 0;
    int width = 0, height = 0;
};
Distances distances[2];
int distancesIndex = 0;
int distancesFrame = -1; // r_framecount the current one is for (one a view: each eye)

GLuint sceneDistances()
{
    const GLuint source = R_OpaqueSceneDepthTexture();
    if(!source || r_framedata.water[2] <= 0.f || r_framedata.causticsscale[3] == 0.f || distanceFailed)
    {
        return 0;
    }
    if(distancesFrame == r_framecount && distances[distancesIndex].texture)
    {
        return distances[distancesIndex].texture;
    }
    if(!distanceProgram)
    {
        distanceProgram = gfx::glProgram(distanceVs, distanceFs, "vr liquid scene distances");
        distanceFailed = !distanceProgram;
        if(distanceFailed)
        {
            return 0;
        }
    }

    // The scene's size: the framebuffers' (vid's while they were made: an eye's, or the window's), asked of GL only
    // for another texture (a query a view cost a quarter of a millisecond of CPU).
    static GLuint sizedSource = 0;
    static GLint sourceWidth = 0, sourceHeight = 0;
    if(source != sizedSource)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &sourceWidth);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &sourceHeight);
        sizedSource = source;
    }
    if(sourceWidth <= 0 || sourceHeight <= 0)
    {
        return 0;
    }
    const int width = (sourceWidth + 1) / 2, height = (sourceHeight + 1) / 2;

    distancesIndex ^= 1;
    Distances& target = distances[distancesIndex];
    if(!target.texture || width != target.width || height != target.height)
    {
        if(target.texture)
        {
            GL_DeleteFramebuffersFunc(1, &target.fbo);
            GL_DeleteNativeTexture(target.texture);
        }
        glGenTextures(1, &target.texture);
        GL_BindNative(GL_TEXTURE8, GL_TEXTURE_2D, target.texture);
        GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_R32F, width, height);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        GL_ObjectLabelFunc(GL_TEXTURE, target.texture, -1, "vr liquid scene distances");
        GL_GenFramebuffersFunc(1, &target.fbo);
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
        GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
        target.width = width;
        target.height = height;
    }

    // Into its own framebuffer (no depth or stencil: the tests pass), then back to the translucent pass's.
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, width, height);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(distanceProgram);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
    GL_Uniform3fFunc(0, r_matproj[0 * 4 + 2], r_matproj[3 * 4 + 2], gl_clipcontrol_able ? 1.f : 0.f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    R_RestoreTranslucentTarget();
    distancesFrame = r_framecount;
    return target.texture;
}

// ----------------------------------------------------------------------------
// The underwater view: no pass of its own. The fog is the engine's (Fog_SetupFrame: every shader has it), the tint
// Quake's colour shift in the liquid's colour, the wobble and blur the eye's post-processing (GL_PostProcess).

int viewLiquid = 0;    // the contents the eye is in this view (0: none, or the underwater view off)
float viewLight = 1.f; // how lit the place is (1: Quake's full light), for the fog's colour

struct Look
{
    float fog[3];
    float distance;     // units at which half the view is fogged
    unsigned char tint[3]; // the colour shift's colour
    int percent;        // and its strength (Quake's: 128 water, 150 slime and lava)
    bool lit;           // the fog is as lit as the place (lava glows)
};

// Water's greenish murk, slime's thick green, lava's glow.
constexpr Look looks[3] = {
    {{0.09f, 0.15f, 0.15f}, 400.f, {30, 80, 80}, 50, true},
    {{0.04f, 0.12f, 0.02f}, 160.f, {20, 70, 10}, 120, true},
    {{0.95f, 0.35f, 0.05f}, 45.f, {255, 100, 20}, 150, false},
};

[[nodiscard]] const Look& look()
{
    return looks[viewLiquid == CONTENTS_SLIME ? 1 : viewLiquid == CONTENTS_LAVA ? 2 : 0];
}

} // namespace

void applyPreset(int preset)
{
    // off: Quake's; low: the surfaces' look without refraction or caustics; medium and up: everything
    const bool on = preset > 0;
    const bool more = preset > 1;
    const auto set = [](cvar_t& var, bool enabled) { Cvar_SetQuick(&var, enabled ? var.default_string : "0"); };
    set(vr_water_waves, on);
    set(vr_water_fresnel, on);
    set(vr_water_glints, on);
    set(vr_water_lava_glow, on);
    set(vr_water_underwater, on);
    set(vr_water_wobble, on);
    set(vr_water_refraction, more);
    set(vr_water_caustics, more);
}

} // namespace qvr::water

using namespace qvr;

// R_SetupView: this view's liquid settings for the shaders (the frame data), the caustics' volume on unit 7, the
// refraction's sampler on unit 6. With an eye in a liquid and the underwater view on, r_waterwarp's screen warp is
// off (the post-processing wobbles instead) and Quake's colour shift takes the liquid's colour.
extern "C" void VR_WaterView(int contents, int* waterwarp)
{
    water::ensureVolume();
    water::ensureSampler();

    const bool liquid = contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA;
    const float underwater = std::clamp(vr_water_underwater.value, 0.f, 2.f);
    water::viewLiquid = liquid && underwater > 0.f && stereo::isRenderingEye() ? contents : 0;
    if(water::viewLiquid)
    {
        *waterwarp = false;
        const water::Look& look = water::look();
        cshift_t& shift = cl.cshifts[CSHIFT_CONTENTS];
        for(int i = 0; i < 3; i++)
        {
            shift.destcolor[i] = look.tint[i];
        }
        shift.percent = static_cast<int>(look.percent * std::min(underwater, 1.f));
        V_CalcBlend();

        lightcache_t cache{};
        vec3_t p = {r_origin[0], r_origin[1], r_origin[2]};
        water::viewLight = std::clamp(R_LightPoint(p, 0.f, &cache) / 128.f, 0.15f, 1.5f);
    }
    Fog_SetupFrame(); // again, with the liquid's fog (VR_WaterFog)

    r_framedata.water[0] = std::clamp(vr_water_waves.value, 0.f, 3.f);
    r_framedata.water[1] = std::clamp(vr_water_fresnel.value, 0.f, 2.f);
    r_framedata.water[2] = R_OpaqueSceneTexture() ? std::clamp(vr_water_refraction.value, 0.f, 3.f) : 0.f;
    r_framedata.water[3] = std::clamp(vr_water_glints.value, 0.f, 3.f);
    r_framedata.water2[0] = std::clamp(vr_water_lava_glow.value, 0.f, 3.f);
    r_framedata.water2[1] = water::volumeWet ? std::clamp(vr_water_caustics.value, 0.f, 2.f) : 0.f;
    r_framedata.water2[2] = liquid ? 1.f : 0.f; // surfaces seen from inside
    r_framedata.water2[3] = 0.f;
    for(int a = 0; a < 3; a++)
    {
        r_framedata.causticsorigin[a] = water::volumeOrigin[a];
        r_framedata.causticsscale[a] = water::volumeScale[a];
    }
    r_framedata.causticsorigin[3] = water::volumeCell;
    // The scene's distances for the refraction (VR_WaterSceneDepth: made as translucent liquids draw).
    r_framedata.causticsscale[3] = r_framedata.water[2] > 0.f && !water::distanceFailed ? 1.f : 0.f;
    GL_BindNative(GL_TEXTURE7, GL_TEXTURE_3D, water::volumeWet ? water::volumeTex : 0);
}

// Fog_SetupFrame: under water, the liquid's fog (the thicker of it and the map's), the sky wholly fogged.
extern "C" void VR_WaterFog(float fog[4], float skyfog[4])
{
    if(!water::viewLiquid)
    {
        return;
    }
    const water::Look& look = water::look();
    const float strength = std::clamp(vr_water_underwater.value, 0.f, 2.f);
    const float light = look.lit ? water::viewLight : 1.f;
    const float density = strength / look.distance; // ApplyFog: 1 - exp2(-(density * distance)^2)
    for(int i = 0; i < 3; i++)
    {
        fog[i] = skyfog[i] = look.fog[i] * light;
    }
    fog[3] = std::max(fog[3], density * density);
    skyfog[3] = 1.f;
}

// R_DrawBrushModels_Water, translucent liquids: how far the opaque scene is, to refract by, on unit 8 (0: none).
extern "C" unsigned VR_WaterSceneDepth(void)
{
    return water::sceneDistances();
}

// GL_PostProcess (its program in use): under water, the wobble and blur (gl_shaders.h), reading the scene smoothly
// on unit 6. Only the scene is in it: the HUD, the menu, the lasers and the wrist's log are drawn over the eye's
// image after it (vr_stereo.cpp), the wrist gadget and the rest of the world wobble.
extern "C" void VR_PostProcessWater(void)
{
    const bool on = water::viewLiquid && stereo::isRenderingEye();
    const float wobble = on ? std::clamp(vr_water_wobble.value, 0.f, 3.f) * 0.004f : 0.f;
    const float blur = on ? std::clamp(vr_water_underwater.value, 0.f, 2.f) * 0.0008f : 0.f;
    GL_Uniform4fFunc(2, static_cast<float>(cl.time), wobble, blur, 0.f);
    if(wobble + blur <= 0.f)
    {
        return;
    }
    water::ensureSampler();
    GL_BindNative(GL_TEXTURE6, GL_TEXTURE_2D, framebufs.composite.color_tex);
    GL_Uniform4fFunc(3, r_matproj[0], r_matproj[4], r_matproj[1], r_matproj[9]);
    GL_Uniform3fFunc(4, vpn[0], vpn[1], vpn[2]);
    GL_Uniform3fFunc(5, -vright[0], -vright[1], -vright[2]);
    GL_Uniform3fFunc(6, vup[0], vup[1], vup[2]);
}
