// vr_haze.cpp -- see vr_haze.hpp.

#include "vr_haze.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_profile.hpp"
#include "vr_water.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace qvr::haze
{
namespace
{

constexpr float kLavaHeight = 72.f;   // the hot layer over lava, in units
constexpr float kLavaReach = 192.f;   // the most of a view ray through it that counts
constexpr float kLavaStrength = 0.5f; // its bend, of an explosion's
constexpr float kNear = 12.f;         // closer to a volume than this (the near plane cuts its faces): drawn from the eye
constexpr float kBurstLife = 0.9f;    // an explosion's haze, in seconds (a rocket's)
constexpr std::size_t kMaxBursts = 16;
constexpr std::size_t kMaxFlames = 12;
constexpr float kFlameDistance = 640.f; // flames farther than this have none
constexpr float kFadeDistance = 1400.f; // nor anything wholly farther than this
constexpr int kMargin = 32;             // pixels round the hot air copied too (the most it bends)

// ----------------------------------------------------------------------------
// The shader. Vertex: the lava layer's vertices in the world (w the lava's height), or a unit box round an ellipsoid
// (mode 1: Center, Radii). Fragment: the view ray through the volume, the bend, the copy read.

constexpr const char* vertexSource = R"(#version 430
layout(location = 0) in vec4 Pos;
layout(location = 0) uniform mat4 ViewProj;
layout(location = 2) uniform vec4 Params;  // x strength, y the scene's distances bound (1), z mode (0 lava, 1 ellipsoid, 2 lava from the eye), w the layer's depth
layout(location = 3) uniform vec4 Center;  // ellipsoid: centre, w how hot (0..1); mode 2: w the lava's height
layout(location = 4) uniform vec4 Radii;   // ellipsoid: radii, w the shock ring's radius (0..1 of them; < 0 none); lava: x the reach
layout(location = 0) out vec4 WorldPos;
void main()
{
    WorldPos = Params.z == 1.0 ? vec4(Center.xyz + Pos.xyz * Radii.xyz, 0.0) : Pos;
    gl_Position = ViewProj * vec4(WorldPos.xyz, 1.0);
}
)";

constexpr const char* fragmentSource = R"(#version 430
layout(binding = 6) uniform sampler2D Scene;     // the copy (unit 6: linear, clamped)
layout(binding = 8) uniform sampler2D Distances; // how far the opaque scene is along the view (half size), with Params.y
layout(location = 0) uniform mat4 ViewProj;
layout(location = 1) uniform vec4 Eye;       // xyz, w the time
layout(location = 2) uniform vec4 Params;
layout(location = 3) uniform vec4 Center;
layout(location = 4) uniform vec4 Radii;
layout(location = 5) uniform vec4 Viewport;  // the scene's, in the framebuffer's pixels
layout(location = 6) uniform vec4 CopyRect;  // the part copied, in the viewport's pixels (x0, y0, x1, y1)
layout(location = 0) in vec4 WorldPos;
layout(location = 0) out vec4 Out;

// The air's shimmer at q in the world, about -1 .. 1 on each axis: per axis two sums of sines whose fronts rise
// (15 and 24 units apart), their coordinates warped by slower sines (no lattice to see).
vec3 Shimmer(vec3 q, float t)
{
    vec3 p = q * 0.13;
    p += 1.2 * sin(p.yzx * vec3(0.71, 0.53, 0.67) + vec3(t * 0.9, t * 1.1, t * 0.7));
    vec3 r;
    r.x = sin(dot(p, vec3(0.9, 0.4, 1.7)) - t * 4.3) + 0.6 * sin(dot(p, vec3(-1.6, 1.2, 2.6)) - t * 6.9 + 1.3);
    r.y = sin(dot(p, vec3(-0.5, 1.0, 1.9)) - t * 4.9 + 0.7) + 0.6 * sin(dot(p, vec3(1.8, 1.3, 2.8)) - t * 7.7 + 2.9);
    r.z = sin(dot(p, vec3(0.3, -0.9, 1.5)) - t * 3.7 + 1.9) + 0.6 * sin(dot(p, vec3(-1.3, -1.7, 2.4)) - t * 6.1 + 4.2);
    return r * 0.625;
}

float SceneDistance(vec2 p) // along the view
{
    return texelFetch(Distances, clamp(ivec2(p * 0.5), ivec2(0), textureSize(Distances, 0) - 1), 0).r;
}

void main()
{
    vec3 eye = Eye.xyz;
    vec3 ray = WorldPos.xyz - eye;
    float tFrag = length(ray);
    vec3 dir = ray / tFrag;
    float perT = (1.0 / gl_FragCoord.w) / tFrag; // distance along the view per unit along the ray
    float tScene = Params.y > 0. ? SceneDistance(gl_FragCoord.xy) / perT : 1e9;
    int mode = int(Params.z + 0.5);
    float t0, t1, amount;
    vec3 q, push = vec3(0.0);
    if (mode != 1)
    {
        // The layer over lava, Params.w deep: the ray from where it enters (this face, or the eye in it) until it
        // leaves the layer, meets the scene, or Radii.x; hotter near the lava (the square of the height left).
        float z0 = mode == 2 ? Center.w : WorldPos.w, depth = Params.w;
        float tIn = -1e9, tOut = 1e9;
        if (abs(dir.z) > 1e-5)
        {
            float ta = (z0 - eye.z) / dir.z, tb = (z0 + depth - eye.z) / dir.z;
            tIn = min(ta, tb);
            tOut = max(ta, tb);
        }
        else if (eye.z < z0 || eye.z > z0 + depth)
            discard;
        t0 = max(mode == 2 ? 0.0 : tFrag, tIn);
        t1 = min(min(tOut, tScene), t0 + Radii.x);
        if (t1 <= t0)
            discard;
        float ua = clamp(1.0 - (eye.z + dir.z * t0 - z0) / depth, 0.0, 1.0);
        float ub = clamp(1.0 - (eye.z + dir.z * t1 - z0) / depth, 0.0, 1.0);
        amount = 1.0 - exp(-(t1 - t0) * (ua * ua + ua * ub + ub * ub) / (3.0 * 40.0));
        q = eye + dir * mix(t0, t1, 0.4);
    }
    else
    {
        // An ellipsoid round Center, Radii: the ray's chord through it (from the eye if it is in it, to the scene),
        // its density 1 - r^2 in the unit sphere, 1 through the middle; and an explosion's shock ring pushing out.
        vec3 o = (eye - Center.xyz) / Radii.xyz, d = dir / Radii.xyz;
        float dd = dot(d, d), od = dot(o, d);
        float tc = -od / dd;
        float p2 = max(dot(o, o) - od * od / dd, 0.0);
        if (p2 >= 1.0)
            discard;
        float h = sqrt((1.0 - p2) / dd);
        t0 = max(tc - h, 0.0);
        t1 = min(tc + h, tScene);
        if (t1 <= t0)
            discard;
        float l = sqrt(dd), s0 = (t0 - tc) * l, s1 = (t1 - tc) * l;
        amount = ((1.0 - p2) * (s1 - s0) - (s1 * s1 * s1 - s0 * s0 * s0) / 3.0) * 0.75 * Center.w;
        q = eye + dir * clamp(tc, t0, t1);
        if (Radii.w >= 0.0)
        {
            float x = (sqrt(p2) - Radii.w) / 0.12;
            push = normalize(q - Center.xyz + vec3(1e-4)) * (exp(-x * x) * (1.0 - Radii.w) * 1.5);
        }
    }
    if (amount < 0.003 && dot(push, push) < 1e-6)
        discard;

    // The bend: a shift of q in the world (an angle of up to about a degree), projected.
    vec3 w = (Shimmer(q, Eye.w) * amount + push) * (Params.x * 0.012 * distance(q, eye));
    vec4 c0 = ViewProj * vec4(q, 1.0), c1 = ViewProj * vec4(q + w, 1.0);
    vec2 shift = (c1.xy / c1.w - c0.xy / c0.w) * 0.5 * Viewport.zw;
    if (Params.y > 0. && SceneDistance(gl_FragCoord.xy + shift) < t0 * perT)
        shift = vec2(0.0); // not what is in front of the hot air (a hand, a gun): its colours would smear round it
    vec2 p = clamp(gl_FragCoord.xy - Viewport.xy + shift, CopyRect.xy + 0.5, CopyRect.zw - 0.5);
    Out = vec4(texture(Scene, p / Viewport.zw).rgb, 1.0);
}
)";

GLuint program = 0;
bool programFailed = false;

// The copy of the scene: the viewport's size, the scene's format; only the part the hot air covers is copied.
struct Copy
{
    GLuint texture = 0, fbo = 0;
    int width = 0, height = 0;
    GLint format = 0;
};
Copy copy;

// The scene colour texture's format (asked of GL only for another texture or another size of vid).
GLint sceneFormat(GLuint color, int samples)
{
    static GLuint sized = 0;
    static int sizedSamples = 0, sizedW = 0, sizedH = 0;
    static GLint format = 0;
    if(color != sized || samples != sizedSamples || vid.width != sizedW || vid.height != sizedH)
    {
        const GLenum target = samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        GL_BindNative(GL_TEXTURE0, target, color);
        glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
        sized = color;
        sizedSamples = samples;
        sizedW = vid.width;
        sizedH = vid.height;
    }
    return format;
}

bool ensureCopy(int width, int height, GLint format)
{
    if(copy.texture && copy.width == width && copy.height == height && copy.format == format)
    {
        return true;
    }
    if(copy.texture)
    {
        GL_DeleteFramebuffersFunc(1, &copy.fbo);
        GL_DeleteNativeTexture(copy.texture);
        copy = {};
    }
    glGenTextures(1, &copy.texture);
    GL_BindNative(GL_TEXTURE6, GL_TEXTURE_2D, copy.texture);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, static_cast<GLenum>(format), width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    GL_ObjectLabelFunc(GL_TEXTURE, copy.texture, -1, "vr haze scene copy");
    GL_GenFramebuffersFunc(1, &copy.fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, copy.fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, copy.texture, 0);
    copy.width = width;
    copy.height = height;
    copy.format = format;
    return true;
}

// ----------------------------------------------------------------------------
// The volumes.

// Triangles wound as the engine's front faces (clockwise seen from outside) for an outward normal n.
void addTriangle(std::vector<glm::vec4>& out, glm::vec4 a, glm::vec4 b, glm::vec4 c, const glm::vec3& n)
{
    if(glm::dot(glm::cross(glm::vec3(b) - glm::vec3(a), glm::vec3(c) - glm::vec3(a)), n) > 0.f)
    {
        std::swap(b, c);
    }
    out.push_back(a);
    out.push_back(b);
    out.push_back(c);
}

// The lava layers: each lava top's polygon raised kLavaHeight and, on the rim, its sides; w the lava's height.
struct LavaLayer
{
    GLint first = 0;
    GLsizei count = 0;
    glm::vec3 mins{}, maxs{};
    std::size_t top = 0;
};
GLuint lavaVbo = 0;
unsigned lavaGeneration = 0;
std::vector<LavaLayer> lavaLayers;

void ensureLava()
{
    unsigned generation = 0;
    const std::vector<water::LavaTop>& tops = water::lavaTops(generation);
    if(generation == lavaGeneration && (lavaVbo || tops.empty()))
    {
        return;
    }
    lavaGeneration = generation;
    lavaLayers.clear();
    if(lavaVbo)
    {
        GL_DeleteBuffer(lavaVbo);
        lavaVbo = 0;
    }
    std::vector<glm::vec4> verts;
    for(std::size_t i = 0; i < tops.size(); i++)
    {
        const water::LavaTop& top = tops[i];
        if(top.poly.size() < 3)
        {
            continue;
        }
        LavaLayer layer;
        layer.first = static_cast<GLint>(verts.size());
        layer.top = i;
        layer.mins = glm::vec3(1e30f);
        layer.maxs = glm::vec3(-1e30f);
        const float z = top.z, h = top.z + kLavaHeight;
        glm::vec2 centre(0.f);
        for(const glm::vec3& p : top.poly)
        {
            centre += glm::vec2(p);
            layer.mins = glm::min(layer.mins, glm::vec3(p.x, p.y, z));
            layer.maxs = glm::max(layer.maxs, glm::vec3(p.x, p.y, h));
        }
        centre /= static_cast<float>(top.poly.size());
        for(std::size_t k = 2; k < top.poly.size(); k++)
        {
            addTriangle(verts, glm::vec4(top.poly[0].x, top.poly[0].y, h, z), glm::vec4(top.poly[k - 1].x, top.poly[k - 1].y, h, z),
                glm::vec4(top.poly[k].x, top.poly[k].y, h, z), glm::vec3(0.f, 0.f, 1.f));
        }
        for(std::size_t k = 0; k + 1 < top.rim.size(); k += 2)
        {
            const glm::vec2 a = top.rim[k], b = top.rim[k + 1];
            glm::vec2 out(b.y - a.y, a.x - b.x);
            if(glm::dot(out, (a + b) * 0.5f - centre) < 0.f)
            {
                out = -out;
            }
            const glm::vec3 n(out, 0.f);
            const glm::vec4 a0(a, z, z), b0(b, z, z), a1(a, h, z), b1(b, h, z);
            addTriangle(verts, a0, b0, b1, n);
            addTriangle(verts, a0, b1, a1, n);
        }
        layer.count = static_cast<GLsizei>(verts.size()) - layer.first;
        lavaLayers.push_back(layer);
    }
    if(!verts.empty())
    {
        lavaVbo = GL_CreateBuffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW, "vr haze lava", verts.size() * sizeof(glm::vec4), verts.data());
    }
}

// Whether p is in the polygon (xy) or within `margin` of its edges.
bool nearPolygon(const std::vector<glm::vec3>& poly, const glm::vec2& p, float margin)
{
    bool in = false;
    for(std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
    {
        const glm::vec2 a(poly[j]), b(poly[i]);
        if((b.y > p.y) != (a.y > p.y) && p.x < (a.x - b.x) * (p.y - b.y) / (a.y - b.y) + b.x)
        {
            in = !in;
        }
        const glm::vec2 ab = b - a;
        const float t = glm::dot(ab, ab) > 0.f ? std::clamp(glm::dot(p - a, ab) / glm::dot(ab, ab), 0.f, 1.f) : 0.f;
        if(glm::distance(p, a + ab * t) < margin)
        {
            return true;
        }
    }
    return in;
}

// A unit box's 36 vertices, its faces front outward.
GLuint boxVbo = 0;

void ensureBox()
{
    if(boxVbo)
    {
        return;
    }
    std::vector<glm::vec4> verts;
    for(int axis = 0; axis < 3; axis++)
    {
        for(float side : {-1.f, 1.f})
        {
            glm::vec3 n(0.f);
            n[axis] = side;
            const int u = (axis + 1) % 3, v = (axis + 2) % 3;
            glm::vec4 c[4];
            for(int k = 0; k < 4; k++)
            {
                glm::vec3 p = n;
                p[u] = (k == 1 || k == 2) ? 1.f : -1.f;
                p[v] = k >= 2 ? 1.f : -1.f;
                c[k] = glm::vec4(p, 0.f);
            }
            addTriangle(verts, c[0], c[1], c[2], n);
            addTriangle(verts, c[0], c[2], c[3], n);
        }
    }
    boxVbo = GL_CreateBuffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW, "vr haze box", verts.size() * sizeof(glm::vec4), verts.data());
}

// Explosions' haze (VR_HazeExplosion), and this view's ellipsoids.
struct Burst
{
    glm::vec3 pos{};
    float size = 1.f;
    double start = 0.;
};
std::vector<Burst> bursts;

struct Ellipsoid
{
    glm::vec3 centre{}, radii{};
    float heat = 0.f;  // 0..1
    float ring = -1.f; // the shock ring's radius (0..1 of the radii), < 0 none
};

// ----------------------------------------------------------------------------
// The part of the viewport the volumes cover: the union of their boxes' projections (all of it if one reaches
// behind the eye), with a margin.
struct Rect
{
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    bool full = false;
};

void addBox(Rect& rect, const glm::mat4& viewProj, const glm::vec3& mins, const glm::vec3& maxs, int w, int h)
{
    if(rect.full)
    {
        return;
    }
    for(int k = 0; k < 8; k++)
    {
        const glm::vec3 p((k & 1) ? maxs.x : mins.x, (k & 2) ? maxs.y : mins.y, (k & 4) ? maxs.z : mins.z);
        const glm::vec4 c = viewProj * glm::vec4(p, 1.f);
        if(c.w < 1.f)
        {
            rect.full = true;
            return;
        }
        const float x = (c.x / c.w * 0.5f + 0.5f) * w, y = (c.y / c.w * 0.5f + 0.5f) * h;
        rect.x0 = std::min(rect.x0, x);
        rect.y0 = std::min(rect.y0, y);
        rect.x1 = std::max(rect.x1, x);
        rect.y1 = std::max(rect.y1, y);
    }
}

float boxDistance(const glm::vec3& p, const glm::vec3& mins, const glm::vec3& maxs)
{
    return glm::length(glm::max(glm::max(mins - p, p - maxs), glm::vec3(0.f)));
}

bool culled(glm::vec3 mins, glm::vec3 maxs)
{
    vec3_t a = {mins.x, mins.y, mins.z}, b = {maxs.x, maxs.y, maxs.z};
    return R_CullBox(a, b);
}

void draw()
{
    const float strength = std::clamp(vr_heat_haze.value, 0.f, 1.f);
    if(strength <= 0.f || !cl.worldmodel || programFailed)
    {
        bursts.clear();
        return;
    }
    GLuint color = 0, depth = 0;
    int samples = 1, viewport[4];
    const GLuint sceneFbo = R_SceneTarget(&color, &depth, &samples, viewport);
    if(!color || viewport[2] <= 0 || viewport[3] <= 0)
    {
        return;
    }
    const glm::vec3 eye(r_framedata.eyepos[0], r_framedata.eyepos[1], r_framedata.eyepos[2]);
    const glm::mat4 viewProj = glm::make_mat4(r_framedata.viewproj);
    const int vw = viewport[2], vh = viewport[3];
    Rect rect;

    // Lava: the layers over the tops in this view's PVS and frustum; the eye in one: that pool's, from the eye.
    ensureLava();
    static std::vector<GLint> lavaFirst;
    static std::vector<GLsizei> lavaCount;
    lavaFirst.clear();
    lavaCount.clear();
    bool lavaInside = false;
    float lavaInsideZ = 0.f;
    {
        unsigned generation = 0;
        const std::vector<water::LavaTop>& tops = water::lavaTops(generation);
        for(const LavaLayer& layer : lavaLayers)
        {
            if(layer.top >= tops.size() || !water::lavaTopInPvs(tops[layer.top]) || culled(layer.mins, layer.maxs) ||
                boxDistance(eye, layer.mins, layer.maxs) > kFadeDistance)
            {
                continue;
            }
            const water::LavaTop& top = tops[layer.top];
            if(eye.z >= top.z - kNear && eye.z <= top.z + kLavaHeight + kNear && nearPolygon(top.poly, glm::vec2(eye), kNear))
            {
                lavaInside = true;
                lavaInsideZ = top.z;
                rect.full = true;
            }
            lavaFirst.push_back(layer.first);
            lavaCount.push_back(layer.count);
            addBox(rect, viewProj, layer.mins, layer.maxs, vw, vh);
        }
    }

    // Explosions, then the flames near the eye.
    static std::vector<Ellipsoid> ellipsoids;
    ellipsoids.clear();
    std::erase_if(bursts, [](const Burst& b) { return cl.time < b.start || cl.time > b.start + kBurstLife * b.size; });
    for(const Burst& b : bursts)
    {
        const float age = static_cast<float>(cl.time - b.start) / (kBurstLife * b.size);
        Ellipsoid e;
        const float grow = 1.f - (1.f - age) * (1.f - age);
        e.radii = glm::vec3(b.size * (36.f + 60.f * grow));
        e.centre = b.pos + glm::vec3(0.f, 0.f, 24.f * age * b.size); // the hot air rises
        e.heat = std::pow(1.f - age, 1.5f);
        e.ring = age < 0.3f ? age / 0.3f : -1.f;
        ellipsoids.push_back(e);
    }
    std::size_t flames = 0;
    for(int i = 0; i < cl_numvisedicts && flames < kMaxFlames; i++)
    {
        const entity_t* ent = cl_visedicts[i];
        if(!ent || !ent->model || std::strncmp(ent->model->name, "progs/flame", 11) != 0)
        {
            continue;
        }
        const bool big = std::strcmp(ent->model->name, "progs/flame2.mdl") == 0;
        const glm::vec3 origin(ent->origin[0], ent->origin[1], ent->origin[2]);
        if(glm::distance(origin, eye) > kFlameDistance)
        {
            continue;
        }
        Ellipsoid e;
        e.centre = origin + glm::vec3(0.f, 0.f, big ? 34.f : 22.f);
        e.radii = big ? glm::vec3(15.f, 15.f, 34.f) : glm::vec3(8.f, 8.f, 22.f);
        e.heat = big ? 0.55f : 0.4f;
        ellipsoids.push_back(e);
        flames++;
    }
    std::erase_if(ellipsoids, [&](const Ellipsoid& e) {
        return culled(e.centre - e.radii, e.centre + e.radii) || boxDistance(eye, e.centre - e.radii, e.centre + e.radii) > kFadeDistance;
    });
    for(const Ellipsoid& e : ellipsoids)
    {
        addBox(rect, viewProj, e.centre - e.radii, e.centre + e.radii, vw, vh);
    }
    if(lavaFirst.empty() && ellipsoids.empty())
    {
        return;
    }

    QVR_GPU_PROFILE("haze");
    if(!program)
    {
        program = gfx::glProgram(vertexSource, fragmentSource, "vr heat haze");
        programFailed = !program;
        if(programFailed)
        {
            return;
        }
    }
    ensureBox();

    int x0 = 0, y0 = 0, x1 = vw, y1 = vh;
    if(!rect.full)
    {
        x0 = std::max(0, static_cast<int>(std::floor(rect.x0)) - kMargin);
        y0 = std::max(0, static_cast<int>(std::floor(rect.y0)) - kMargin);
        x1 = std::min(vw, static_cast<int>(std::ceil(rect.x1)) + kMargin);
        y1 = std::min(vh, static_cast<int>(std::ceil(rect.y1)) + kMargin);
        if(x1 <= x0 || y1 <= y0)
        {
            return;
        }
    }

    // How far the scene is (made now if the liquids did not make it this view; binds the scene's framebuffer again).
    GLuint distances = 0;
    {
        QVR_GPU_PROFILE("distances");
        distances = water::opaqueSceneDistances();
    }

    // The copy: the part covered, resolved.
    const GLint format = sceneFormat(color, samples);
    if(!format || !ensureCopy(vw, vh, format))
    {
        return;
    }
    {
        QVR_GPU_PROFILE("copy");
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, sceneFbo);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, copy.fbo);
        GL_BlitFramebufferFunc(viewport[0] + x0, viewport[1] + y0, viewport[0] + x1, viewport[1] + y1, x0, y0, x1, y1,
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
        R_SetupGL();
    }

    glEnable(GL_SCISSOR_TEST);
    glScissor(viewport[0] + x0, viewport[1] + y0, x1 - x0, y1 - y0);
    GL_UseProgram(program);
    GL_BindNative(GL_TEXTURE6, GL_TEXTURE_2D, copy.texture);
    GL_BindNative(GL_TEXTURE8, GL_TEXTURE_2D, distances);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, r_framedata.viewproj);
    GL_Uniform4fFunc(1, eye.x, eye.y, eye.z, static_cast<float>(cl.time));
    GL_Uniform4fFunc(5, static_cast<float>(viewport[0]), static_cast<float>(viewport[1]), static_cast<float>(vw), static_cast<float>(vh));
    GL_Uniform4fFunc(6, static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1), static_cast<float>(y1));
    const float hasDistances = distances ? 1.f : 0.f;

    // Lava: its layers' front faces, depth tested; the eye in one: a quad over the view, the ray from the eye.
    if(!lavaFirst.empty())
    {
        GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZWRITE | GLS_CULL_BACK | GLS_ATTRIBS(1));
        GL_Uniform4fFunc(2, strength * kLavaStrength, hasDistances, 0.f, kLavaHeight);
        GL_Uniform4fFunc(4, kLavaReach, 0.f, 0.f, -1.f);
        GL_BindBuffer(GL_ARRAY_BUFFER, lavaVbo);
        GL_VertexAttribPointerFunc(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
        for(std::size_t i = 0; i < lavaFirst.size(); i++)
        {
            glDrawArrays(GL_TRIANGLES, lavaFirst[i], lavaCount[i]);
        }
        if(lavaInside)
        {
            const glm::mat4 inv = glm::inverse(viewProj);
            glm::vec4 corners[4];
            for(int k = 0; k < 4; k++)
            {
                glm::vec4 p = inv * glm::vec4((k == 1 || k == 2) ? 1.f : -1.f, k >= 2 ? 1.f : -1.f, 0.5f, 1.f);
                const glm::vec3 d = glm::normalize(glm::vec3(p) / p.w - eye);
                corners[k] = glm::vec4(eye + d * 64.f, 0.f);
            }
            const glm::vec4 quad[6] = {corners[0], corners[1], corners[2], corners[0], corners[2], corners[3]};
            GLuint buf;
            GLbyte* ofs;
            GL_Upload(GL_ARRAY_BUFFER, quad, sizeof(quad), &buf, &ofs);
            GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZWRITE | GLS_NO_ZTEST | GLS_CULL_NONE | GLS_ATTRIBS(1));
            GL_BindBuffer(GL_ARRAY_BUFFER, buf);
            GL_VertexAttribPointerFunc(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), ofs);
            GL_Uniform4fFunc(2, strength * kLavaStrength, hasDistances, 2.f, kLavaHeight);
            GL_Uniform4fFunc(3, 0.f, 0.f, 0.f, lavaInsideZ);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    // Ellipsoids: their boxes' front faces, depth tested; the eye in one: its back faces, not (the scene's distances
    // stop the ray).
    if(!ellipsoids.empty())
    {
        GL_BindBuffer(GL_ARRAY_BUFFER, boxVbo);
        GL_VertexAttribPointerFunc(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);
        for(const Ellipsoid& e : ellipsoids)
        {
            const bool in = boxDistance(eye, e.centre - e.radii, e.centre + e.radii) <= kNear;
            GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZWRITE | (in ? GLS_NO_ZTEST | GLS_CULL_FRONT : GLS_CULL_BACK) | GLS_ATTRIBS(1));
            GL_Uniform4fFunc(2, strength, hasDistances, 1.f, 0.f);
            GL_Uniform4fFunc(3, e.centre.x, e.centre.y, e.centre.z, e.heat);
            GL_Uniform4fFunc(4, e.radii.x, e.radii.y, e.radii.z, e.ring);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
    glDisable(GL_SCISSOR_TEST);
}

} // namespace

void applyPreset(int preset)
{
    Cvar_SetQuick(&vr_heat_haze, preset > 1 ? vr_heat_haze.default_string : "0");
}

} // namespace qvr::haze

using namespace qvr;

// cl_tent.c: an explosion at pos (size 1 a rocket's); none in water or slime (lava: yes).
extern "C" void VR_HazeExplosion(const float* pos, float size)
{
    if(vr_heat_haze.value <= 0.f || !cl.worldmodel)
    {
        return;
    }
    vec3_t p = {pos[0], pos[1], pos[2]};
    const int contents = Mod_PointInLeaf(p, cl.worldmodel)->contents;
    if(contents == CONTENTS_WATER || contents == CONTENTS_SLIME)
    {
        return;
    }
    if(haze::bursts.size() >= haze::kMaxBursts)
    {
        haze::bursts.erase(haze::bursts.begin());
    }
    haze::bursts.push_back({glm::vec3(pos[0], pos[1], pos[2]), std::clamp(size, 0.3f, 2.f), cl.time});
}

// R_RenderScene, after the translucent pass.
extern "C" void VR_DrawHeatHaze(void)
{
    haze::draw();
}
