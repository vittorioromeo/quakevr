// vr_coil.cpp -- see vr_coil.hpp.

#include "vr_coil.hpp"

#include "vr_units.hpp"

#include <algorithm>
#include <cmath>

namespace qvr::coil
{
namespace
{

constexpr int segments = 12;          // the line's springs, the two stubs included (nodes 0 .. segments)
constexpr float stub = 0.012f;        // metres the cord leaves each end straight along its direction
constexpr float mass = 0.03f;         // kilograms, the whole cord (a telephone cord's)
constexpr float stiffness = 1.5f;     // newtons a metre it is stretched by, the whole cord (its turns opening)
constexpr float compression = 3.f;    // times stiffer squeezed below its relaxed length (its turns touching)
constexpr float airDamping = 1.5f;    // per second, against its swing relative to its ends
constexpr float springDamping = 20.f; // per second, along each spring (the wobble along it dies quickly)
constexpr float gravity = 9.81f;      // metres per second squared
constexpr float substep = 1.f / 300.f;
constexpr float taper = 0.015f;       // metres over which the coil narrows to the wire's lead into each end
constexpr float pi = 3.14159265f;

// The springs between the stubs: that many, each relaxed at the cord's relaxed length over them.
constexpr int springs = segments - 2;

float relaxedLength(const Style& style)
{
    // Turns touching: the wire's thickness a turn; a plain cable as long as the coiled one relaxed.
    return static_cast<float>(std::max(style.turns, 64)) * 2.f * style.wireRadius;
}

glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t)
{
    const float t2 = t * t, t3 = t2 * t;
    return 0.5f * ((2.f * p1) + (-p0 + p2) * t + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2 + (-p0 + 3.f * p1 - 3.f * p2 + p3) * t3);
}

glm::vec3 anyPerpendicular(const glm::vec3& t)
{
    const glm::vec3 ref = std::fabs(t.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
    return glm::normalize(glm::cross(ref, t));
}

// The light at a point as the alias models get it (R_SetupAliasLighting with the lightmap's contrast and the view
// models' least light; 1 is Quake's full light).
glm::vec3 worldLight(const glm::vec3& p)
{
    if(!cl.worldmodel)
    {
        return glm::vec3{0.5f};
    }
    vec3_t v{p.x, p.y, p.z};
    lightcache_t cache{};
    R_LightPoint(v, 0.f, &cache);
    float c[3] = {lightcolor[0], lightcolor[1], lightcolor[2]};
    VR_AliasLightCurve(c);
    const float add = 3.f * VR_ViewModelMinLight() - (c[0] + c[1] + c[2]);
    glm::vec3 out{c[0], c[1], c[2]};
    if(add > 0.f)
    {
        out += glm::vec3{add / 3.f};
    }
    return out / 128.f;
}

struct NearLight
{
    const gpulight_t* l;
    glm::vec3 pos;
    glm::vec3 color;
    float radius;
};

} // namespace

void Cord::reset(const glm::vec3& a, const glm::vec3& b)
{
    pos_.assign(segments + 1, a);
    vel_.assign(segments + 1, glm::vec3{0.f});
    for(int i = 0; i <= segments; i++)
    {
        pos_[i] = glm::mix(a, b, static_cast<float>(i) / segments);
    }
    lastA_ = a;
    lastB_ = b;
    time_ = realtime;
    valid_ = true;
}

void Cord::update(const glm::vec3& a, const glm::vec3& aDir, const glm::vec3& b, const glm::vec3& bDir, const Style& style)
{
    style_ = style;
    const float m2u = units::metresToUnits();
    const float dt = static_cast<float>(realtime - time_);
    // Afresh: first drawn, a long pause, or an end jumping away from the other (a teleport of one, a respawn).
    if(!valid_ || dt > 0.25f || glm::distance(b - lastB_, a - lastA_) > 1.f * m2u || !std::isfinite(pos_[segments / 2].x))
    {
        reset(a, b);
        return;
    }
    if(dt <= 0.f)
    {
        return;
    }

    // The body's movement (walking, turning, riding a lift) carries the cord along: only the free end's own
    // movement relative to it swings it.
    const glm::vec3 carry = a - lastA_;
    for(glm::vec3& p : pos_)
    {
        p += carry;
    }
    const glm::vec3 fromB = lastB_ + carry;
    const glm::vec3 vb = (b - fromB) / std::max(dt, 1e-4f); // the free end's velocity relative to the body
    lastA_ = a;
    lastB_ = b;
    time_ = realtime;

    const float rest = relaxedLength(style) * m2u / springs;
    const float nodeMass = mass / (segments - 3); // the free nodes 2 .. segments - 2
    const float k = stiffness * springs / nodeMass;
    const glm::vec3 down{0.f, 0.f, -gravity * m2u};
    const int steps = std::clamp(static_cast<int>(std::ceil(dt / substep)), 1, 60);
    const float h = dt / steps;
    for(int step = 1; step <= steps; step++)
    {
        // The pinned ends (the free one moved on in a straight line through the frame) and their stubs.
        const glm::vec3 endB = glm::mix(fromB, b, static_cast<float>(step) / steps);
        pos_[0] = a;
        pos_[1] = a + aDir * (stub * m2u);
        pos_[segments] = endB;
        pos_[segments - 1] = endB + bDir * (stub * m2u);
        vel_[0] = vel_[1] = glm::vec3{0.f};
        vel_[segments] = vel_[segments - 1] = vb;

        static glm::vec3 force[segments + 1];
        for(int i = 0; i <= segments; i++)
        {
            force[i] = down;
        }
        for(int i = 1; i < segments - 1; i++)
        {
            const glm::vec3 d = pos_[i + 1] - pos_[i];
            const float len = glm::length(d);
            if(len < 1e-5f)
            {
                continue;
            }
            const glm::vec3 dir = d / len;
            const float ext = len - rest;
            const float f = (ext > 0.f ? k : k * compression) * ext + springDamping * glm::dot(vel_[i + 1] - vel_[i], dir);
            force[i] += f * dir;
            force[i + 1] -= f * dir;
        }
        for(int i = 2; i <= segments - 2; i++)
        {
            // Air against its swing, relative to the ends (at the carry's frame, the free end's share along it).
            const float t = static_cast<float>(i) / segments;
            force[i] -= airDamping * (vel_[i] - vb * t);
            vel_[i] += force[i] * h;
            pos_[i] += vel_[i] * h;
        }
    }
}

bool Cord::build(const glm::vec3& eye, std::vector<gfx::TubeRing>& out, int& sides) const
{
    rings_ = 0;
    if(!valid_ || pos_.size() != static_cast<size_t>(segments + 1))
    {
        return false;
    }
    const float m2u = units::metresToUnits();

    // The line: a Catmull-Rom spline through the nodes, sampled finely, with its length along it.
    constexpr int perSegment = 6;
    static std::vector<glm::vec3> line;
    static std::vector<float> along;
    line.clear();
    along.clear();
    for(int i = 0; i < segments; i++)
    {
        const glm::vec3& p1 = pos_[i];
        const glm::vec3& p2 = pos_[i + 1];
        const glm::vec3 p0 = i > 0 ? pos_[i - 1] : 2.f * p1 - p2;
        const glm::vec3 p3 = i + 2 <= segments ? pos_[i + 2] : 2.f * p2 - p1;
        for(int j = 0; j < perSegment; j++)
        {
            line.push_back(catmullRom(p0, p1, p2, p3, static_cast<float>(j) / perSegment));
        }
    }
    line.push_back(pos_[segments]);
    along.resize(line.size());
    along[0] = 0.f;
    for(size_t i = 1; i < line.size(); i++)
    {
        along[i] = along[i - 1] + glm::distance(line[i - 1], line[i]);
    }
    const float length = along.back();
    if(length < 0.5f)
    {
        return false;
    }

    // Frames carried along the line without twisting (parallel transport).
    static std::vector<glm::vec3> tangent, normal;
    tangent.resize(line.size());
    normal.resize(line.size());
    for(size_t i = 0; i < line.size(); i++)
    {
        const glm::vec3 d = line[std::min(i + 1, line.size() - 1)] - line[i > 0 ? i - 1 : 0];
        tangent[i] = glm::length(d) > 1e-6f ? glm::normalize(d) : (i > 0 ? tangent[i - 1] : glm::vec3{0.f, 0.f, -1.f});
    }
    normal[0] = anyPerpendicular(tangent[0]);
    for(size_t i = 1; i < line.size(); i++)
    {
        const glm::vec3 n = normal[i - 1] - tangent[i] * glm::dot(normal[i - 1], tangent[i]);
        normal[i] = glm::length(n) > 1e-6f ? glm::normalize(n) : anyPerpendicular(tangent[i]);
    }

    // The detail by how far it is: segments a turn and sides round the wire.
    float nearest = 1e9f;
    for(const glm::vec3& p : pos_)
    {
        nearest = std::min(nearest, glm::distance(p, eye));
    }
    const float metres = nearest / m2u;
    const int perTurn = metres < 0.6f ? 8 : metres < 1.2f ? 6 : 4;
    const int sidesAt = metres < 0.6f ? 6 : metres < 1.2f ? 5 : 4;
    const bool coiled = style_.turns > 0;
    const int turns = std::max(style_.turns, 1);
    const int rings = coiled ? turns * perTurn : std::max(static_cast<int>(line.size()) - 1, 1);
    rings_ = rings + 1;

    // The coil keeps its wire's length a turn: stretched, it opens out and narrows.
    const float pitch = length / static_cast<float>(turns);
    const float wireTurn = 2.f * pi * style_.coilRadius * m2u;
    const float coilRadius =
        coiled ? std::sqrt(std::max(wireTurn * wireTurn - pitch * pitch, 0.04f * wireTurn * wireTurn)) / (2.f * pi) : 0.f;
    const float wire = style_.wireRadius * m2u;

    // The light: the world's at three points along it, and the dynamic lights that reach it.
    const glm::vec3 lightAt[3] = {worldLight(line[line.size() / 6]), worldLight(line[line.size() / 2]),
        worldLight(line[line.size() * 5 / 6])};
    static std::vector<NearLight> lights;
    lights.clear();
    glm::vec3 centre{0.f};
    for(const glm::vec3& p : pos_)
    {
        centre += p;
    }
    centre /= static_cast<float>(pos_.size());
    const float reach = length * 0.5f + wire * 4.f;
    for(int i = 0; i < r_framedata.numlights; i++)
    {
        const gpulight_t& l = r_lightbuffer.lights[i];
        if(l.shadow[3] != 0.f) // a map light's shadow entry, not a light
        {
            continue;
        }
        const glm::vec3 lp{l.pos[0], l.pos[1], l.pos[2]};
        if(glm::distance(lp, centre) < l.radius + reach)
        {
            lights.push_back({&l, lp, glm::vec3{l.color[0], l.color[1], l.color[2]} / 128.f, l.radius});
        }
    }

    // The rings: their middles on the helix, the wire's direction and an axis across it, and the light reaching them
    // (the dynamic lights' summed, from their mean direction by strength). The GPU makes the wire round them and
    // shades it (gfx::drawTube).
    static std::vector<glm::vec3> mid, radial;
    mid.resize(rings + 1);
    radial.resize(rings + 1);
    out.resize(rings + 1);
    const int numLights = std::min(static_cast<int>(lights.size()), 4);
    size_t seg = 0;
    for(int r = 0; r <= rings; r++)
    {
        const float u = static_cast<float>(r) / rings;
        const float s = u * length;
        while(seg + 2 < along.size() && along[seg + 1] < s)
        {
            seg++;
        }
        const float span = std::max(along[seg + 1] - along[seg], 1e-6f);
        const float f = std::clamp((s - along[seg]) / span, 0.f, 1.f);
        const glm::vec3 c = glm::mix(line[seg], line[seg + 1], f);
        const glm::vec3 t = glm::normalize(glm::mix(tangent[seg], tangent[seg + 1], f));
        glm::vec3 n = glm::mix(normal[seg], normal[seg + 1], f);
        n = glm::normalize(n - t * glm::dot(n, t));
        const glm::vec3 b = glm::cross(t, n);
        const float phi = 2.f * pi * static_cast<float>(turns) * u;
        radial[r] = coiled ? std::cos(phi) * n + std::sin(phi) * b : n;
        const float edge = std::min(s, length - s) / (taper * m2u);
        const float narrow = coiled ? std::clamp(edge, 0.f, 1.f) : 0.f;
        mid[r] = c + radial[r] * (coilRadius * narrow * narrow * (3.f - 2.f * narrow));

        gfx::TubeRing& ring = out[r];
        const float w = u * 2.f;
        const glm::vec3 ambient = w < 1.f ? glm::mix(lightAt[0], lightAt[1], w) : glm::mix(lightAt[1], lightAt[2], w - 1.f);
        ring.ambient = glm::vec4{ambient, 0.f};
        glm::vec3 lamp{0.f}, from{0.f};
        for(int k = 0; k < numLights; k++)
        {
            const NearLight& l = lights[k];
            const glm::vec3 d = l.pos - mid[r];
            const float dist = glm::length(d);
            if(dist >= l.radius || dist < 1e-4f)
            {
                continue;
            }
            const float p[3] = {mid[r].x, mid[r].y, mid[r].z};
            const glm::vec3 add = l.color * ((l.radius - dist) * VR_SpotCone(l.l, p));
            lamp += add;
            from += d * ((add.x + add.y + add.z) / dist);
        }
        ring.lamp = glm::vec4{lamp, 0.f};
        ring.lampDir = glm::vec4{glm::length(from) > 1e-6f ? glm::normalize(from) : glm::vec3{0.f, 0.f, 1.f}, 0.f};
    }
    for(int r = 0; r <= rings; r++)
    {
        // The wire's own direction (along the helix), and the axis across it from the line out to the wire.
        const glm::vec3 t = glm::normalize(mid[std::min(r + 1, rings)] - mid[std::max(r - 1, 0)]);
        glm::vec3 e = radial[r] - t * glm::dot(radial[r], t);
        e = glm::length(e) > 1e-6f ? glm::normalize(e) : anyPerpendicular(t);
        out[r].mid = glm::vec4{mid[r], wire};
        out[r].across = glm::vec4{e, 0.f};
        out[r].along = glm::vec4{t, 0.f};
    }
    sides = sidesAt;
    return true;
}

} // namespace qvr::coil
