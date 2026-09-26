// vr_water.cpp -- see vr_water.hpp.

#include "vr_water.hpp"
#include "vr_haze.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_stereo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" size_t gl_bmodel_vbo_size; // r_brush.c

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
// read through it (gl_shaders.h, LiquidRefract); and for the soft particles and sprites (vr_particles.cpp:
// opaqueSceneDistances). Made once a view, as the first translucent liquid draws (or, with none, as the particles
// draw after the translucent pass), by a small pass: the scene's depth/stencil is the target then (depth tested, its
// stencil written), which a shader may not read. Half the size, in R32F, each texel the distance (along the view) of
// the nearest of its four: at 3292 x 3524 it reads the 46 MB of depth once and writes 12 MB. Multisampled depth
// (vid_fsaa, the particles only): its first sample.
constexpr const char* distanceVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char* distanceFs = R"(#version 430
#ifdef MULTISAMPLED
layout(binding = 0) uniform sampler2DMS Depth;
#define DEPTH_SIZE textureSize(Depth)
#else
layout(binding = 0) uniform sampler2D Depth;
#define DEPTH_SIZE textureSize(Depth, 0)
#endif
layout(location = 0) uniform vec3 Proj; // clip z = x * w + y; z 1: depth is clip z (reversed Z), 0: (clip z + 1) / 2
layout(location = 0) out float Out;
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy) * 2;
    ivec2 last = DEPTH_SIZE - 1;
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
GLuint distanceProgramMs = 0; // from multisampled depth
bool distanceFailedMs = false;

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

// This view's distances from the depth texture `source` (made now if not yet: then `restore` puts back the framebuffer
// drawn to), 0 if there is none.
GLuint makeDistances(GLuint source, bool multisampled, void (*restore)())
{
    bool& failed = multisampled ? distanceFailedMs : distanceFailed;
    if(!source || failed)
    {
        return 0;
    }
    if(distancesFrame == r_framecount && distances[distancesIndex].texture)
    {
        return distances[distancesIndex].texture;
    }
    GLuint& program = multisampled ? distanceProgramMs : distanceProgram;
    if(!program)
    {
        std::string fs = distanceFs;
        if(multisampled)
        {
            fs.insert(fs.find('\n') + 1, "#define MULTISAMPLED\n");
        }
        program = gfx::glProgram(distanceVs, fs.c_str(), "vr scene distances");
        failed = !program;
        if(failed)
        {
            return 0;
        }
    }
    const GLenum sourceTarget = multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    // The scene's size: the framebuffers' (vid's while they were made: an eye's, or the window's), asked of GL only
    // for another texture (a query a view cost a quarter of a millisecond of CPU), or another size of vid: GL
    // reuses a deleted texture's name, so framebuffers made anew at another size (vr_render_scale, the window) can
    // come back under the same name.
    static GLuint sizedSource = 0;
    static GLint sourceWidth = 0, sourceHeight = 0;
    static int sizedVidWidth = 0, sizedVidHeight = 0;
    if(source != sizedSource || vid.width != sizedVidWidth || vid.height != sizedVidHeight)
    {
        sizedVidWidth = vid.width;
        sizedVidHeight = vid.height;
        GL_BindNative(GL_TEXTURE0, sourceTarget, source);
        glGetTexLevelParameteriv(sourceTarget, 0, GL_TEXTURE_WIDTH, &sourceWidth);
        glGetTexLevelParameteriv(sourceTarget, 0, GL_TEXTURE_HEIGHT, &sourceHeight);
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
        GL_ObjectLabelFunc(GL_TEXTURE, target.texture, -1, "vr scene distances");
        GL_GenFramebuffersFunc(1, &target.fbo);
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
        GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
        target.width = width;
        target.height = height;
    }

    // Into its own framebuffer (no depth or stencil: the tests pass), then back to the pass's.
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, width, height);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(program);
    GL_BindNative(GL_TEXTURE0, sourceTarget, source);
    GL_Uniform3fFunc(0, r_matproj[0 * 4 + 2], r_matproj[3 * 4 + 2], gl_clipcontrol_able ? 1.f : 0.f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    restore();
    distancesFrame = r_framecount;
    return target.texture;
}

// The liquids' (unit 8), as a pass of them draws: for the refraction (translucent ones), and the shoreline foam (any,
// multisampled too). Made before opaque liquids draw, they are not in it: it is made again for whatever reads it
// later in the view (the translucent liquids, the soft particles, the haze), with them.
GLuint sceneDistances(bool translucent)
{
    const bool refract = translucent && r_framedata.water[2] > 0.f;
    if(r_framedata.causticsscale[3] == 0.f || (!refract && r_framedata.water3[0] <= 0.f))
    {
        return 0;
    }
    GLuint color = 0, depth = 0;
    int samples = 1, viewport[4];
    R_SceneTarget(&color, &depth, &samples, viewport);
    const GLuint made = makeDistances(depth, samples > 1, translucent ? R_RestoreTranslucentTarget : R_SetupGL);
    if(!translucent)
    {
        distancesFrame = -1; // the opaque liquids are about to be drawn into the depth
    }
    return made;
}

// ----------------------------------------------------------------------------
// Geometric waves (vr_water_geo_waves): the level faces of the world's water, slime and lava are cut by a world grid
// (vr_water_geo_cell units) as a map's first view is drawn, into vertex and index buffers of their own; the liquid
// programs' vertex shaders raise each vertex by the swells (gl_shaders.h, LiquidSwell) times its pin. The pin is 0 on
// a pool's rim, where the liquid meets a wall or another kind of liquid, rising to 1 kPinDistance units in: no gap
// opens there. Faces sharing an edge cut it alike (the grid is the world's, a cut point is computed the same from
// either face), and the pin is a function of the place alone, so they move together; a face's top and underside
// too. The swells fade out from half kFadeEnd to kFadeEnd units from the eye (in the shaders); a face wholly past
// that is drawn as its plain polygon. What is seen of it is picked each view (VR_WaterMarkVis: the PVS, the frustum,
// the side it's seen from), and r_world.c draws it with Ironwail's liquid programs in place of the world's own liquid
// faces (brush entities' liquids stay flat). The shading's normal and the fragments' positions stay the flat
// surface's (the swells are in LiquidWaves' normal): the faces only move on screen and in depth.

constexpr float kPinDistance = 32.f;
constexpr float kFadeEnd = 1024.f;   // gl_shaders.h, LiquidDisplace
constexpr float kMaxSwell = 40.f;    // the most the swells rise or sink (vr_water_geo_amplitude up to 24, lava 1.3 times), and the ripples
constexpr std::size_t kMaxCells = 2u << 20; // grid cells in a map at most (the cell grows past it)

struct MeshVert
{
    float pos[3];
    float st[4];
    float lmofs;
    unsigned styles;
    float pin; // 0: not held (a sloped or vertical face), -1: a level face under more liquid; else 1 + how far the rim is, in units (up to kPinDistance)
};
static_assert(sizeof(MeshVert) == 40, "the vertex layout VR_WaterMeshBind gives");

struct MeshFace
{
    int texnum = 0;
    unsigned full = 0, fullCount = 0; // its triangles, cut by the grid (a level face) or its polygon's fan
    unsigned flat = 0, flatCount = 0; // its polygon's fan (far away)
    float mins[3] = {}, maxs[3] = {};
    float normal[3] = {}, dist = 0.f; // facing where it is seen from
    int firstLeaf = 0, numLeaves = 0;
    int group = -1;       // its level group's (a kind, a side, a height), -1: not level
    int pvsFrame = -1;    // r_framecount it was last in the view's PVS (VR_WaterMarkVis)
    int lava = -1;        // its entry in Mesh::lava (the top of a level lava face), -1: none
};


struct Mesh
{
    const qmodel_t* model = nullptr;
    char name[MAX_QPATH] = {};
    int visframe = 0;     // r_visframecount last seen (R_NewMap sets it back to 0: rebuilt then)
    float cell = 0.f;     // vr_water_geo_cell it was cut at
    bool built = false;
    GLuint vbo = 0, ibo = 0;
    std::vector<MeshFace> faces; // by texnum
    std::vector<int> leaves;
    // this view's: (first index, count) pairs, a texture's from ranges[texBegin[texnum]] to ranges[texEnd[texnum]]
    int framecount = -1;
    std::vector<unsigned> ranges;
    std::vector<int> texBegin, texEnd;
    std::vector<LavaTop> lava; // the heat haze's (vr_haze.cpp)
    unsigned generation = 0;   // made anew each build
};
Mesh mesh;

using Poly = std::vector<glm::vec3>;

bool lexLess(const glm::vec3& a, const glm::vec3& b)
{
    return a.x != b.x ? a.x < b.x : a.y != b.y ? a.y < b.y : a.z < b.z;
}

// Where edge a-b crosses the plane axis = at, the same whichever way round the edge is.
glm::vec3 cutPoint(glm::vec3 a, glm::vec3 b, int axis, float at)
{
    if(lexLess(b, a))
    {
        std::swap(a, b);
    }
    const float t = (at - a[axis]) / (b[axis] - a[axis]);
    glm::vec3 p = a + (b - a) * t;
    p[axis] = at;
    return p;
}

// Splits a convex polygon by the plane axis = at (points within 0.01 of it go to both sides).
void splitPoly(const Poly& in, int axis, float at, Poly& back, Poly& front)
{
    back.clear();
    front.clear();
    const std::size_t n = in.size();
    const auto side = [&](const glm::vec3& p) { const float d = p[axis] - at; return d > 0.01f ? 1 : d < -0.01f ? -1 : 0; };
    for(std::size_t i = 0; i < n; i++)
    {
        const glm::vec3& a = in[i];
        const glm::vec3& b = in[(i + 1) % n];
        const int sa = side(a), sb = side(b);
        if(sa <= 0)
        {
            back.push_back(a);
        }
        if(sa >= 0)
        {
            front.push_back(a);
        }
        if(sa * sb < 0)
        {
            const glm::vec3 p = cutPoint(a, b, axis, at);
            back.push_back(p);
            front.push_back(p);
        }
    }
}

// Cuts a convex polygon along the grid lines of one axis (every `cell` units of the world), left to right.
void cutAlong(const Poly& poly, int axis, float cell, std::vector<Poly>& out)
{
    float lo = 1e30f, hi = -1e30f;
    for(const glm::vec3& p : poly)
    {
        lo = std::min(lo, p[axis]);
        hi = std::max(hi, p[axis]);
    }
    Poly rest = poly, back, front;
    for(long long k = static_cast<long long>(std::ceil(lo / cell)); k * cell < hi; k++)
    {
        splitPoly(rest, axis, static_cast<float>(k * cell), back, front);
        if(back.size() >= 3)
        {
            out.push_back(back);
        }
        rest.swap(front);
        if(rest.size() < 3)
        {
            return;
        }
    }
    out.push_back(rest);
}

float cross2(const glm::vec2& a, const glm::vec2& b)
{
    return a.x * b.y - a.y * b.x;
}

float segmentDistance(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
{
    const glm::vec2 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 0.f ? std::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    return glm::length(p - (a + ab * t));
}

struct SrcFace
{
    int surf = 0;
    bool level = false;
    bool submerged = false; // a level face with liquid over it (two liquid brushes stacked): no foam, no swells
    int group = -1;
    Poly poly;
    const glvert_t* verts = nullptr; // Ironwail's, for the attributes
    float normal[3] = {};
    float dist = 0.f;
    float mins[2] = {}, maxs[2] = {};
    float winding = 1.f; // the sign of its area in xy
};

bool insideXY(const SrcFace& f, const glm::vec2& p)
{
    if(p.x < f.mins[0] - 0.1f || p.x > f.maxs[0] + 0.1f || p.y < f.mins[1] - 0.1f || p.y > f.maxs[1] + 0.1f)
    {
        return false;
    }
    const std::size_t n = f.poly.size();
    for(std::size_t i = 0; i < n; i++)
    {
        const glm::vec2 a(f.poly[i]), b(f.poly[(i + 1) % n]);
        if(cross2(b - a, p - a) * f.winding < -0.01f)
        {
            return false;
        }
    }
    return true;
}

// The vertex attributes at p on face f: its texture and lightmap coordinates are affine over the plane (fitted to
// three of Ironwail's vertices), the lightmap offset and styles its own.
struct Affine
{
    glm::vec3 p0{}, e1{}, e2{};
    float g11 = 0.f, g12 = 0.f, g22 = 0.f, det = 0.f;
    const glvert_t *v0 = nullptr, *v1 = nullptr, *v2 = nullptr;

    explicit Affine(const SrcFace& f)
    {
        const std::size_t n = f.poly.size();
        v0 = v1 = v2 = f.verts;
        p0 = f.poly[0];
        float best = 0.f;
        for(std::size_t i = 1; i + 1 < n; i++)
        {
            for(std::size_t j = i + 1; j < n; j++)
            {
                const float area = glm::length(glm::cross(f.poly[i] - p0, f.poly[j] - p0));
                if(area > best)
                {
                    best = area;
                    e1 = f.poly[i] - p0;
                    e2 = f.poly[j] - p0;
                    v1 = f.verts + i;
                    v2 = f.verts + j;
                }
            }
        }
        g11 = glm::dot(e1, e1);
        g12 = glm::dot(e1, e2);
        g22 = glm::dot(e2, e2);
        det = g11 * g22 - g12 * g12;
    }

    MeshVert at(const glm::vec3& p, float pin) const
    {
        MeshVert v{};
        v.pos[0] = p.x;
        v.pos[1] = p.y;
        v.pos[2] = p.z;
        float a = 0.f, b = 0.f;
        if(det > 1e-6f)
        {
            const glm::vec3 d = p - p0;
            const float r1 = glm::dot(d, e1), r2 = glm::dot(d, e2);
            a = (r1 * g22 - r2 * g12) / det;
            b = (r2 * g11 - r1 * g12) / det;
        }
        for(int i = 0; i < 4; i++)
        {
            v.st[i] = v0->st[i] + a * (v1->st[i] - v0->st[i]) + b * (v2->st[i] - v0->st[i]);
        }
        v.lmofs = v0->lmofs;
        v.styles = v0->styles;
        v.pin = pin;
        return v;
    }
};

// The pins of the mesh's vertices on the grid's crossings (the tops of level faces), by crossing and height: where the
// liquid's surface is on the CPU (surfaceRise: the splashes' rings and foam lying on it, drops going back in).
std::unordered_map<std::uint64_t, float> gridPins;
float gridPinCell = 0.f;

[[nodiscard]] std::uint64_t gridKey(long long ix, long long iy, float z)
{
    return (static_cast<std::uint64_t>(ix & 0x1fffff) << 42) | (static_cast<std::uint64_t>(iy & 0x1fffff) << 21) |
           static_cast<std::uint64_t>(std::llround(z * 2.f) & 0x1fffff);
}

void recordPins(const std::vector<MeshVert>& verts, float cell)
{
    gridPins.clear();
    gridPinCell = cell;
    for(const MeshVert& v : verts)
    {
        const float gx = v.pos[0] / cell, gy = v.pos[1] / cell;
        if(v.pin >= 1.f && std::abs(gx - std::round(gx)) < 1e-3f && std::abs(gy - std::round(gy)) < 1e-3f)
        {
            gridPins[gridKey(std::llround(gx), std::llround(gy), v.pos[2])] = v.pin;
        }
    }
}

void freeMesh()
{
    if(mesh.vbo)
    {
        GL_DeleteBuffer(mesh.vbo);
    }
    if(mesh.ibo)
    {
        GL_DeleteBuffer(mesh.ibo);
    }
    mesh.vbo = mesh.ibo = 0;
    mesh.faces.clear();
    mesh.leaves.clear();
    mesh.lava.clear();
    mesh.generation++;
    mesh.built = false;
}

using GetBufferSubDataFn = void(APIENTRY*)(GLenum, GLintptr, GLsizeiptr, void*);

void buildMesh(qmodel_t* m, float cell)
{
    freeMesh();
    mesh.model = m;
    q_strlcpy(mesh.name, m->name, sizeof(mesh.name));
    mesh.cell = cell;
    mesh.built = true;

    static GetBufferSubDataFn getBufferSubData = nullptr;
    if(!getBufferSubData)
    {
        getBufferSubData = reinterpret_cast<GetBufferSubDataFn>(SDL_GL_GetProcAddress("glGetBufferSubData"));
    }
    if(!getBufferSubData || !gl_bmodel_vbo || gl_bmodel_vbo_size < sizeof(glvert_t))
    {
        return;
    }
    // Ironwail's vertices of the brush models (once a map: a wait for the GPU is fine)
    std::vector<glvert_t> src(gl_bmodel_vbo_size / sizeof(glvert_t));
    GL_BindBuffer(GL_ARRAY_BUFFER, gl_bmodel_vbo);
    getBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(src.size() * sizeof(glvert_t)), src.data());

    // The world's liquid faces; the level ones (not teleports') in groups: a kind, a side, a height.
    std::vector<SrcFace> faces;
    std::vector<int> faceOfSurface(static_cast<std::size_t>(m->numsurfaces), -1);
    std::vector<std::vector<int>> groups;
    std::vector<std::array<int, 3>> groupKeys;
    double area = 0.;
    for(int i = m->firstmodelsurface; i < m->firstmodelsurface + m->nummodelsurfaces; i++)
    {
        const msurface_t& s = m->surfaces[i];
        if(!(s.flags & SURF_DRAWTURB) || s.numedges < 3 || s.texinfo->texnum < 0 || s.texinfo->texnum >= m->numtextures)
        {
            continue;
        }
        const texture_t* t = m->textures[s.texinfo->texnum];
        if(!t || !TEXTYPE_ISLIQUID(t->type) || s.vbo_firstvert + s.numedges > static_cast<int>(src.size()))
        {
            continue;
        }
        SrcFace f;
        f.surf = i;
        f.verts = src.data() + s.vbo_firstvert;
        const float flip = (s.flags & SURF_PLANEBACK) ? -1.f : 1.f;
        for(int a = 0; a < 3; a++)
        {
            f.normal[a] = s.plane->normal[a] * flip;
        }
        f.dist = s.plane->dist * flip;
        f.mins[0] = f.mins[1] = 1e30f;
        f.maxs[0] = f.maxs[1] = -1e30f;
        float twice = 0.f;
        for(int k = 0; k < s.numedges; k++)
        {
            const float* p = f.verts[k].pos;
            f.poly.emplace_back(p[0], p[1], p[2]);
            for(int a = 0; a < 2; a++)
            {
                f.mins[a] = std::min(f.mins[a], p[a]);
                f.maxs[a] = std::max(f.maxs[a], p[a]);
            }
        }
        for(std::size_t k = 0; k < f.poly.size(); k++)
        {
            twice += cross2(glm::vec2(f.poly[k]), glm::vec2(f.poly[(k + 1) % f.poly.size()]));
        }
        f.winding = twice < 0.f ? -1.f : 1.f;
        f.level = std::fabs(f.normal[2]) > 0.99f && t->type != TEXTYPE_TELE;
        if(f.level)
        {
            vec3_t above = {0.f, 0.f, f.poly[0].z + 1.f};
            for(const glm::vec3& p : f.poly)
            {
                above[0] += p.x / static_cast<float>(f.poly.size());
                above[1] += p.y / static_cast<float>(f.poly.size());
            }
            const int contents = Mod_PointInLeaf(above, m)->contents;
            f.submerged = contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA;
        }
        if(f.level)
        {
            area += std::fabs(twice) * 0.5;
            const std::array<int, 3> key = {static_cast<int>(t->type), f.normal[2] > 0.f ? 1 : -1,
                static_cast<int>(std::lround(f.poly[0].z * 2.f))};
            std::size_t g = 0;
            while(g < groupKeys.size() && groupKeys[g] != key)
            {
                g++;
            }
            if(g == groupKeys.size())
            {
                groupKeys.push_back(key);
                groups.emplace_back();
            }
            groups[g].push_back(static_cast<int>(faces.size()));
            f.group = static_cast<int>(g);
        }
        faceOfSurface[static_cast<std::size_t>(i)] = static_cast<int>(faces.size());
        faces.push_back(std::move(f));
    }
    if(faces.empty())
    {
        return;
    }
    while(area / (static_cast<double>(cell) * cell) > kMaxCells)
    {
        cell *= 1.5f;
    }

    // Each group's rim: the parts of its faces' edges that no other face of the group continues past (sampled every
    // 2 units, half a unit out).
    struct Segment
    {
        glm::vec2 a, b;
    };
    std::vector<std::vector<Segment>> rims(groups.size());
    std::vector<std::vector<Segment>> faceRims(faces.size()); // the same, by the face whose edge it is on
    for(std::size_t fi = 0; fi < faces.size(); fi++)
    {
        const SrcFace& f = faces[fi];
        if(!f.level)
        {
            continue;
        }
        const std::size_t n = f.poly.size();
        for(std::size_t i = 0; i < n; i++)
        {
            const glm::vec2 a(f.poly[i]), b(f.poly[(i + 1) % n]);
            const float len = glm::length(b - a);
            if(len < 0.01f)
            {
                continue;
            }
            const glm::vec2 out = glm::vec2((b - a).y, -(b - a).x) / len * f.winding; // right of a->b: out of a CCW face
            const int samples = std::max(1, static_cast<int>(std::ceil(len / 2.f)));
            int runStart = -1;
            for(int k = 0; k <= samples; k++)
            {
                bool open = false;
                if(k < samples)
                {
                    const glm::vec2 p = a + (b - a) * ((k + 0.5f) / samples) + out * 0.5f;
                    open = true;
                    for(int other : groups[static_cast<std::size_t>(f.group)])
                    {
                        if(other != static_cast<int>(fi) && insideXY(faces[static_cast<std::size_t>(other)], p))
                        {
                            open = false;
                            break;
                        }
                    }
                }
                if(open && runStart < 0)
                {
                    runStart = k;
                }
                else if(!open && runStart >= 0)
                {
                    rims[static_cast<std::size_t>(f.group)].push_back(
                        {a + (b - a) * (static_cast<float>(runStart) / samples), a + (b - a) * (static_cast<float>(k) / samples)});
                    faceRims[fi].push_back(rims[static_cast<std::size_t>(f.group)].back());
                    runStart = -1;
                }
            }
        }
    }
    const auto pinAt = [&](int group, const glm::vec3& p) {
        const glm::vec2 q(p);
        float d = kPinDistance;
        for(const Segment& s : rims[static_cast<std::size_t>(group)])
        {
            if(q.x < std::min(s.a.x, s.b.x) - d || q.x > std::max(s.a.x, s.b.x) + d || q.y < std::min(s.a.y, s.b.y) - d ||
                q.y > std::max(s.a.y, s.b.y) + d)
            {
                continue;
            }
            d = std::min(d, segmentDistance(q, s.a, s.b));
        }
        return 1.f + d; // the shaders' pin: smoothstep(1, 1 + kPinDistance); the foam's distance to the shore
    };

    // The faces, by texture: their vertices, their triangles (cut, or the polygon's fan) and their fans.
    std::vector<int> order(faces.size());
    for(std::size_t i = 0; i < order.size(); i++)
    {
        order[i] = static_cast<int>(i);
    }
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return m->surfaces[faces[static_cast<std::size_t>(a)].surf].texinfo->texnum <
               m->surfaces[faces[static_cast<std::size_t>(b)].surf].texinfo->texnum;
    });
    std::vector<MeshVert> verts;
    std::vector<unsigned> fullIdx, flatIdx;
    std::vector<unsigned> faceFull, faceFlat;
    std::unordered_map<std::uint64_t, unsigned> index;
    std::vector<Poly> strips, pieces;
    std::vector<int> meshFaceOf(faces.size());
    for(int fi : order)
    {
        const SrcFace& f = faces[static_cast<std::size_t>(fi)];
        const msurface_t& s = m->surfaces[f.surf];
        const Affine affine(f);
        index.clear();
        const auto vertex = [&](const glm::vec3& p) {
            const std::uint64_t key = (static_cast<std::uint64_t>(std::llround(p.x * 16.f) & 0x1fffff) << 42) |
                                      (static_cast<std::uint64_t>(std::llround(p.y * 16.f) & 0x1fffff) << 21) |
                                      static_cast<std::uint64_t>(std::llround(p.z * 16.f) & 0x1fffff);
            const auto it = index.find(key);
            if(it != index.end())
            {
                return it->second;
            }
            const unsigned v = static_cast<unsigned>(verts.size());
            verts.push_back(affine.at(p, !f.level ? 0.f : f.submerged ? -1.f : pinAt(f.group, p)));
            index.emplace(key, v);
            return v;
        };

        MeshFace out;
        out.texnum = s.texinfo->texnum;
        for(int a = 0; a < 3; a++)
        {
            out.mins[a] = s.mins[a];
            out.maxs[a] = s.maxs[a];
            out.normal[a] = f.normal[a];
        }
        out.dist = f.dist;
        out.group = f.group;
        const texture_t* tex = m->textures[s.texinfo->texnum];
        if(f.level && tex->type == TEXTYPE_LAVA && f.normal[2] > 0.f)
        {
            LavaTop top;
            top.face = static_cast<int>(mesh.faces.size());
            top.z = f.poly[0].z;
            top.poly = f.poly;
            for(const Segment& r : faceRims[static_cast<std::size_t>(fi)])
            {
                top.rim.push_back(r.a);
                top.rim.push_back(r.b);
            }
            out.lava = static_cast<int>(mesh.lava.size());
            mesh.lava.push_back(std::move(top));
        }

        faceFlat.clear();
        for(std::size_t k = 2; k < f.poly.size(); k++)
        {
            faceFlat.push_back(vertex(f.poly[0]));
            faceFlat.push_back(vertex(f.poly[k - 1]));
            faceFlat.push_back(vertex(f.poly[k]));
        }
        faceFull.clear();
        if(f.level)
        {
            strips.clear();
            pieces.clear();
            cutAlong(f.poly, 0, cell, strips);
            for(const Poly& strip : strips)
            {
                cutAlong(strip, 1, cell, pieces);
            }
            for(const Poly& piece : pieces)
            {
                for(std::size_t k = 2; k < piece.size(); k++)
                {
                    faceFull.push_back(vertex(piece[0]));
                    faceFull.push_back(vertex(piece[k - 1]));
                    faceFull.push_back(vertex(piece[k]));
                }
            }
        }
        else
        {
            faceFull = faceFlat;
        }
        out.full = static_cast<unsigned>(fullIdx.size());
        out.fullCount = static_cast<unsigned>(faceFull.size());
        fullIdx.insert(fullIdx.end(), faceFull.begin(), faceFull.end());
        if(f.level)
        {
            out.flat = static_cast<unsigned>(flatIdx.size()); // offset by the full ones' below
            out.flatCount = static_cast<unsigned>(faceFlat.size());
            flatIdx.insert(flatIdx.end(), faceFlat.begin(), faceFlat.end());
        }
        else
        {
            out.flat = out.full;
            out.flatCount = out.fullCount;
            out.flatCount |= 0x80000000u; // marks "already in the full section" until the offsets are fixed
        }
        meshFaceOf[static_cast<std::size_t>(fi)] = static_cast<int>(mesh.faces.size());
        mesh.faces.push_back(out);
    }
    const unsigned flatBase = static_cast<unsigned>(fullIdx.size());
    for(MeshFace& face : mesh.faces)
    {
        if(face.flatCount & 0x80000000u)
        {
            face.flatCount &= 0x7fffffffu;
        }
        else
        {
            face.flat += flatBase;
        }
    }
    fullIdx.insert(fullIdx.end(), flatIdx.begin(), flatIdx.end());

    // The leaves each face is in, for the PVS.
    std::vector<std::vector<int>> faceLeaves(mesh.faces.size());
    for(int i = 1; i <= m->numleafs; i++)
    {
        const mleaf_t* leaf = m->leafs + i;
        for(int j = 0; j < leaf->nummarksurfaces; j++)
        {
            const int s = leaf->firstmarksurface[j];
            if(s >= 0 && s < m->numsurfaces && faceOfSurface[static_cast<std::size_t>(s)] >= 0)
            {
                faceLeaves[static_cast<std::size_t>(meshFaceOf[static_cast<std::size_t>(faceOfSurface[static_cast<std::size_t>(s)])])]
                    .push_back(i);
            }
        }
    }
    for(std::size_t i = 0; i < mesh.faces.size(); i++)
    {
        mesh.faces[i].firstLeaf = static_cast<int>(mesh.leaves.size());
        mesh.faces[i].numLeaves = static_cast<int>(faceLeaves[i].size());
        mesh.leaves.insert(mesh.leaves.end(), faceLeaves[i].begin(), faceLeaves[i].end());
    }

    mesh.vbo = GL_CreateBuffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW, "vr liquid mesh verts", verts.size() * sizeof(MeshVert), verts.data());
    mesh.ibo = GL_CreateBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, "vr liquid mesh indices", fullIdx.size() * sizeof(unsigned),
        fullIdx.data());
    std::size_t rimCount = 0;
    for(const auto& r : rims)
    {
        rimCount += r.size();
    }
    Con_DPrintf("VR water: geometric waves' mesh: %d faces, %d vertices, %d triangles, %g-unit cells, %d rim segments\n",
        static_cast<int>(mesh.faces.size()), static_cast<int>(verts.size()), static_cast<int>(flatBase / 3), cell,
        static_cast<int>(rimCount));
    recordPins(verts, cell); // the surface's height on the CPU (surfaceRise)
}

// The mesh for this map and cell size, built if need be; false if the waves are off or it has nothing.
bool ensureMesh()
{
    qmodel_t* m = cl.worldmodel;
    if(r_visframecount < mesh.visframe)
    {
        freeMesh(); // a map (re)loaded: its lightmaps and Ironwail's vertices made again (and not kept with the waves off)
    }
    mesh.visframe = r_visframecount;
    // wanted for the swells, the splashes' ripples, the shoreline foam (the distance to the shore, in the pins) or the
    // haze over lava
    const bool waves = (vr_water_geo_waves.value > 0.f && vr_water_geo_amplitude.value > 0.f) || vr_water_ripples.value > 0.f;
    if(!m || (!waves && vr_water_foam.value <= 0.f && vr_heat_haze.value <= 0.f))
    {
        return false;
    }
    const float cell = std::clamp(vr_water_geo_cell.value, 4.f, 128.f);
    if(!mesh.built || m != mesh.model || std::strcmp(m->name, mesh.name) != 0 || cell != mesh.cell)
    {
        buildMesh(m, cell);
        mesh.cell = cell;
    }
    return mesh.vbo != 0 && !mesh.faces.empty();
}

// This view's faces of the mesh: in the PVS, the frustum and seen from their side; cut near the eye, flat past it.
void markMesh(const byte* vis)
{
    mesh.framecount = -1;
    if(!ensureMesh())
    {
        return;
    }
    mesh.framecount = r_framecount;
    mesh.ranges.clear();
    const std::size_t numtex = static_cast<std::size_t>(mesh.model->numtextures);
    mesh.texBegin.assign(numtex, 0);
    mesh.texEnd.assign(numtex, 0);
    const float* eye = r_refdef.vieworg;
    int lastTex = -1;
    for(MeshFace& f : mesh.faces)
    {
        if(f.texnum != lastTex)
        {
            if(lastTex >= 0)
            {
                mesh.texEnd[static_cast<std::size_t>(lastTex)] = static_cast<int>(mesh.ranges.size());
            }
            lastTex = f.texnum;
            mesh.texBegin[static_cast<std::size_t>(lastTex)] = static_cast<int>(mesh.ranges.size());
        }
        if(eye[0] * f.normal[0] + eye[1] * f.normal[1] + eye[2] * f.normal[2] - f.dist < -2.f * kMaxSwell)
        {
            continue;
        }
        bool seen = vis == nullptr;
        for(int i = 0; i < f.numLeaves && !seen; i++)
        {
            const int l = mesh.leaves[static_cast<std::size_t>(f.firstLeaf + i)] - 1;
            seen = (vis[l >> 3] & (1 << (l & 7))) != 0;
        }
        if(seen)
        {
            f.pvsFrame = r_framecount;
        }
        vec3_t mins, maxs;
        float d2 = 0.f;
        for(int a = 0; a < 3; a++)
        {
            mins[a] = f.mins[a] - kMaxSwell;
            maxs[a] = f.maxs[a] + kMaxSwell;
            const float d = std::max({f.mins[a] - eye[a], 0.f, eye[a] - f.maxs[a]});
            d2 += d * d;
        }
        if(!seen || R_CullBox(mins, maxs))
        {
            continue;
        }
        const bool flat = d2 > (kFadeEnd + 16.f) * (kFadeEnd + 16.f);
        const unsigned first = flat ? f.flat : f.full, count = flat ? f.flatCount : f.fullCount;
        const std::size_t n = mesh.ranges.size();
        if(n > static_cast<std::size_t>(mesh.texBegin[static_cast<std::size_t>(lastTex)]) && mesh.ranges[n - 2] + mesh.ranges[n - 1] == first)
        {
            mesh.ranges[n - 1] += count;
        }
        else
        {
            mesh.ranges.push_back(first);
            mesh.ranges.push_back(count);
        }
    }
    if(lastTex >= 0)
    {
        mesh.texEnd[static_cast<std::size_t>(lastTex)] = static_cast<int>(mesh.ranges.size());
    }
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
    set(vr_water_geo_waves, more);
    set(vr_water_foam, more);
    set(vr_water_ripples, more);
    haze::applyPreset(preset); // the heat haze (vr_haze.cpp)
}

const std::vector<LavaTop>& lavaTops(unsigned& generation)
{
    generation = mesh.built ? mesh.generation : 0;
    return mesh.lava;
}

bool lavaTopInPvs(const LavaTop& top)
{
    return top.face >= 0 && static_cast<std::size_t>(top.face) < mesh.faces.size() &&
           mesh.faces[static_cast<std::size_t>(top.face)].pvsFrame == r_framecount;
}

// ----------------------------------------------------------------------------
// Ripples (vr_water_ripples): each splash the client is sent (particles::spawn: Preset::Splash, and an explosion under
// a surface) starts a ring of waves on the liquid, kept here (the last kMaxRipples, a stronger one close by just
// now merged into it) and put in each view's frame data: where, the surface's height, how old, how high it is now
// (rising over a tenth of a second, then dying down by e every vr_water_ripple_decay seconds). The shaders
// (gl_shaders.h, LiquidRipples) add a travelling ring of crests round each on the level liquid faces at that height:
// in the vertices of the geometric waves' mesh (held still at the rim, faded out with distance, as the swells), and
// in the shading's normal, with finer ripples riding on them. All from the client's time: the same in both eyes and
// at any frame rate.

namespace
{

constexpr int kMaxRipples = 32; // gl_shaders.h's RippleAt
constexpr float kMaxRipple = 8.f; // units, one ripple's height at most (kMaxSwell leaves room for it)

struct RippleEvent
{
    glm::vec3 at{0.f};
    double time = 0.0;
    float amp = 0.f; // units at its start (0: none)
    const qmodel_t* world = nullptr;
};
std::array<RippleEvent, kMaxRipples> rippleEvents;

// How high it is `age` seconds on.
[[nodiscard]] float rippleHeight(const RippleEvent& e, float age)
{
    if(e.amp <= 0.f || e.world != cl.worldmodel || age < 0.f)
    {
        return 0.f;
    }
    const float rise = std::min(age / 0.1f, 1.f);
    return e.amp * rise * rise * (3.f - 2.f * rise) * std::exp(-age / std::clamp(vr_water_ripple_decay.value, 0.1f, 10.f));
}

// This view's ripples into the frame data (the ones still there, packed).
void fillRipples()
{
    int n = 0;
    if(vr_water_ripples.value > 0.f)
    {
        for(RippleEvent& e : rippleEvents)
        {
            const float age = static_cast<float>(cl.time - e.time);
            const float h = rippleHeight(e, age);
            if(h < 0.02f)
            {
                if(age > 0.1f || age < 0.f || e.world != cl.worldmodel)
                {
                    e.amp = 0.f; // gone (or a map's before this one)
                }
                continue;
            }
            float* at = r_framedata.rippleat[n];
            at[0] = e.at.x;
            at[1] = e.at.y;
            at[2] = e.at.z;
            at[3] = age;
            r_framedata.rippleamp[n >> 2][n & 3] = h;
            n++;
        }
    }
    const float wavelength = std::clamp(vr_water_ripple_wavelength.value, 8.f, 512.f);
    const float cell = std::clamp(vr_water_geo_cell.value, 4.f, 128.f);
    const float cells = std::clamp((wavelength / cell - 2.f), 0.f, 1.f); // too short for the grid: in the shading only
    r_framedata.ripple[0] = static_cast<float>(n);
    r_framedata.ripple[1] = rippleSpeed(CONTENTS_WATER);
    r_framedata.ripple[2] = 6.2831853f / wavelength;
    r_framedata.ripple[3] = cells * cells * (3.f - 2.f * cells);
}

} // namespace

void addRipple(const glm::vec3& at, float strength)
{
    if(vr_water_ripples.value <= 0.f || !cl.worldmodel)
    {
        return;
    }
    const float amp = std::min(std::clamp(vr_water_ripple_amplitude.value, 0.f, 8.f) * std::clamp(std::sqrt(strength / 10.f), 0.35f, 2.6f), kMaxRipple);
    if(amp <= 0.f)
    {
        return;
    }
    // One close by just now (both hands, a shotgun's pellets, the way out after the way in): that one stronger.
    for(RippleEvent& e : rippleEvents)
    {
        const double age = cl.time - e.time;
        if(e.amp > 0.f && e.world == cl.worldmodel && age >= 0.0 && age < 0.15 && std::abs(e.at.z - at.z) < 2.f &&
            glm::distance(glm::vec2{e.at}, glm::vec2{at}) < 16.f)
        {
            e.amp = std::min(std::max(e.amp, amp) + 0.3f * std::min(e.amp, amp), kMaxRipple);
            return;
        }
    }
    // Else in the place of the weakest now (one gone first; one just made is at its start's height).
    RippleEvent* slot = &rippleEvents[0];
    float weakest = 1e9f;
    for(RippleEvent& e : rippleEvents)
    {
        const float age = static_cast<float>(cl.time - e.time);
        const float h = rippleHeight(e, std::max(age, 0.1f)) * (age >= 0.f ? 1.f : 0.f);
        if(h < weakest)
        {
            weakest = h;
            slot = &e;
        }
    }
    *slot = {at, cl.time, amp, cl.worldmodel};
    if(developer.value >= 2)
    {
        Con_Printf("VR ripple: %.1f %.1f %.1f, %.2f units\n", at.x, at.y, at.z, amp);
    }
}

float rippleSpeed(int contents)
{
    const float speed = std::clamp(vr_water_ripple_speed.value, 1.f, 500.f);
    return speed * (contents == CONTENTS_LAVA ? 0.35f : contents == CONTENTS_SLIME ? 0.7f : 1.f); // as LiquidRipples
}

namespace
{

// gl_shaders.h's LiquidSwell's height and LiquidRipples' (fine 0), from this view's frame data; kind the shaders'.
[[nodiscard]] float swellHeight(float x, float y, int kind)
{
    const float water2w = r_framedata.water2[3];
    if(water2w <= 0.f || kind == 3)
    {
        return 0.f;
    }
    const float scale = kind == 1 ? 1.8f : kind == 2 ? 1.2f : 1.f;
    const float speed = kind == 1 ? 0.25f : kind == 2 ? 0.4f : 1.f;
    const float amp = water2w * (kind == 1 ? 1.3f : kind == 2 ? 0.6f : 1.f);
    constexpr float dirs[3][2] = {{0.96f, 0.28f}, {-0.45f, 0.89f}, {0.6f, -0.8f}};
    constexpr float lens[3] = {173.f, 109.f, 71.f};
    constexpr float shares[3] = {0.5f, 0.3f, 0.2f};
    float h = 0.f;
    for(int i = 0; i < 3; i++)
    {
        const float k = 6.2831853f / (lens[i] * scale);
        const float t = (dirs[i][0] * x + dirs[i][1] * y) * k + r_framedata.time * std::sqrt(200.f * k) * speed + static_cast<float>(i) * 2.3f;
        h += amp * shares[i] * std::sin(t);
    }
    return h;
}

[[nodiscard]] float rippleHeightAt(float x, float y, float z, int kind)
{
    const int n = static_cast<int>(r_framedata.ripple[0]);
    if(n <= 0 || kind == 3)
    {
        return 0.f;
    }
    const float speed = r_framedata.ripple[1] * (kind == 1 ? 0.35f : kind == 2 ? 0.7f : 1.f);
    const float k = r_framedata.ripple[2] / (kind == 1 ? 1.5f : 1.f);
    const float amp = kind == 1 ? 0.6f : kind == 2 ? 0.8f : 1.f;
    const float width = 4.4f / k;
    float h = 0.f;
    for(int i = 0; i < n; i++)
    {
        const float* e = r_framedata.rippleat[i];
        const float dist = std::sqrt((x - e[0]) * (x - e[0]) + (y - e[1]) * (y - e[1]));
        const float front = speed * e[3];
        const float u = (dist - front) / width;
        if(std::abs(z - e[2]) > 4.f || std::abs(u) > 3.f)
        {
            continue;
        }
        const float a = amp * r_framedata.rippleamp[i >> 2][i & 3] / std::sqrt(1.f + dist * k * 0.16f);
        h -= a * std::exp(-u * u) * std::cos(k * (dist - 2.f * front));
    }
    return h;
}

// LiquidDisplace's rise at a crossing of the grid (0 off the mesh's level tops); each one worked out once a view (the
// splashes' rings and foam share their crossings, and their drops).
std::unordered_map<std::uint64_t, float> gridRises;
int gridRisesFrame = -1;

[[nodiscard]] float gridRise(long long ix, long long iy, float z, int kind, const glm::vec3& eye)
{
    if(gridRisesFrame != r_framecount)
    {
        gridRisesFrame = r_framecount;
        gridRises.clear();
    }
    const std::uint64_t key = gridKey(ix, iy, z);
    const auto cached = gridRises.find(key);
    if(cached != gridRises.end())
    {
        return cached->second;
    }
    float& rise = gridRises[key];
    const auto it = gridPins.find(key);
    if(it == gridPins.end())
    {
        return rise = 0.f;
    }
    const float x = static_cast<float>(ix) * gridPinCell, y = static_cast<float>(iy) * gridPinCell;
    const float t = std::clamp((it->second - 1.f) / 32.f, 0.f, 1.f);
    const float pin = t * t * (3.f - 2.f * t);
    const float d = glm::distance(glm::vec3{x, y, z}, eye);
    const float f = std::clamp((d - 512.f) / 512.f, 0.f, 1.f);
    const float fade = 1.f - f * f * (3.f - 2.f * f);
    return rise = (swellHeight(x, y, kind) + rippleHeightAt(x, y, z, kind) * r_framedata.ripple[3]) * pin * fade;
}

} // namespace

float surfaceRise(const glm::vec3& p, int contents, const glm::vec3& eye)
{
    if(mesh.framecount != r_framecount || gridPinCell <= 0.f || gridPins.empty())
    {
        return 0.f; // the world's liquids are flat this view
    }
    const int kind = contents == CONTENTS_LAVA ? 1 : contents == CONTENTS_SLIME ? 2 : 4;
    const float gx = p.x / gridPinCell, gy = p.y / gridPinCell;
    const long long ix = static_cast<long long>(std::floor(gx)), iy = static_cast<long long>(std::floor(gy));
    const float fx = gx - static_cast<float>(ix), fy = gy - static_cast<float>(iy);
    const float h00 = gridRise(ix, iy, p.z, kind, eye), h10 = gridRise(ix + 1, iy, p.z, kind, eye);
    const float h01 = gridRise(ix, iy + 1, p.z, kind, eye), h11 = gridRise(ix + 1, iy + 1, p.z, kind, eye);
    return (h00 * (1.f - fx) + h10 * fx) * (1.f - fy) + (h01 * (1.f - fx) + h11 * fx) * fy;
}

void sceneDepthChanged()
{
    distancesFrame = -1;
}

unsigned opaqueSceneDistances()
{
    // The depth the scene is drawn with (none: the window's own, not a texture): the liquids' when they made it.
    GLuint source = 0;
    bool multisampled = false;
    if(GL_NeedsSceneEffects())
    {
        source = framebufs.scene.depth_stencil_tex;
        multisampled = framebufs.scene.samples > 1;
    }
    else if(GL_NeedsPostprocess())
    {
        source = framebufs.composite.depth_stencil_tex;
    }
    return makeDistances(source, multisampled, R_SetupGL);
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
    // the geometric waves' height (0 off): the swells in the shading of every liquid, the mesh's vertices raised by them
    r_framedata.water2[3] = vr_water_geo_waves.value > 0.f ? std::clamp(vr_water_geo_amplitude.value, 0.f, 24.f) : 0.f;
    water::fillRipples(); // the splashes' ripples (vr_water_ripples)
    for(int a = 0; a < 3; a++)
    {
        r_framedata.causticsorigin[a] = water::volumeOrigin[a];
        r_framedata.causticsscale[a] = water::volumeScale[a];
    }
    r_framedata.causticsorigin[3] = water::volumeCell;
    // The shoreline foam (vr_water_foam); the scene's distances for the refraction and the foam (VR_WaterSceneDepth:
    // made as liquids draw), if the scene's depth can be read.
    r_framedata.water3[0] = std::clamp(vr_water_foam.value, 0.f, 2.f);
    GLuint sceneColor = 0, sceneDepth = 0;
    int sceneSamples = 1, sceneViewport[4];
    R_SceneTarget(&sceneColor, &sceneDepth, &sceneSamples, sceneViewport);
    const bool distancesOk = sceneDepth && !(sceneSamples > 1 ? water::distanceFailedMs : water::distanceFailed);
    r_framedata.causticsscale[3] = (r_framedata.water[2] > 0.f || r_framedata.water3[0] > 0.f) && distancesOk ? 1.f : 0.f;
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

// R_DrawBrushModels_Water: how far the opaque scene is, to refract by and for the foam, on unit 8 (0: none).
extern "C" unsigned VR_WaterSceneDepth(int translucent)
{
    return water::sceneDistances(translucent != 0);
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

// R_MarkSurfaces, the view's PVS chosen (null: all): the geometric waves' mesh faces seen in this view (built first if
// need be, as a map's first view is drawn).
extern "C" void VR_WaterMarkVis(const byte* vis)
{
    water::markMesh(vis);
}

// R_DrawBrushModels_Water: nonzero if the world's liquids are drawn from the geometric waves' mesh this view.
extern "C" int VR_WaterMeshActive(void)
{
    return water::mesh.framecount == r_framecount && water::mesh.vbo != 0;
}

// The (first index, count) pairs of the mesh's triangles of the world's texture texnum seen this view.
extern "C" int VR_WaterMeshRanges(int texnum, const unsigned** ranges)
{
    if(texnum < 0 || static_cast<std::size_t>(texnum) >= water::mesh.texBegin.size())
    {
        return 0;
    }
    const int begin = water::mesh.texBegin[static_cast<std::size_t>(texnum)];
    *ranges = water::mesh.ranges.data() + begin;
    return (water::mesh.texEnd[static_cast<std::size_t>(texnum)] - begin) / 2;
}

// Binds the mesh's buffers and points attributes 0-3 as Ironwail's glvert_t and 4 at the pin (GLS_ATTRIBS(5)).
extern "C" void VR_WaterMeshBind(void)
{
    using water::MeshVert;
    GL_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, water::mesh.ibo);
    GL_BindBuffer(GL_ARRAY_BUFFER, water::mesh.vbo);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVert), reinterpret_cast<void*>(offsetof(MeshVert, pos)));
    GL_VertexAttribPointerFunc(1, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVert), reinterpret_cast<void*>(offsetof(MeshVert, st)));
    GL_VertexAttribPointerFunc(2, 1, GL_FLOAT, GL_FALSE, sizeof(MeshVert), reinterpret_cast<void*>(offsetof(MeshVert, lmofs)));
    GL_VertexAttribIPointerFunc(3, 4, GL_UNSIGNED_BYTE, sizeof(MeshVert), reinterpret_cast<void*>(offsetof(MeshVert, styles)));
    GL_VertexAttribPointerFunc(4, 1, GL_FLOAT, GL_FALSE, sizeof(MeshVert), reinterpret_cast<void*>(offsetof(MeshVert, pin)));
}
