// vr_verlet.cpp -- see vr_verlet.hpp. (Moved out of vr_rope.cpp stepChain, unchanged, to be shared with the magnetic
// holsters' tether.)

#include "vr_verlet.hpp"

#include "vr_trace.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr::verlet
{

void fall(glm::vec3* p, glm::vec3* prev, const char* pinned, za::SizeT n, float dt, float lastDt, float drag, float gravity)
{
    const float keep = za::exp(-drag * dt) * (lastDt > 0.f ? dt / lastDt : 1.f);
    const glm::vec3 down{0.f, 0.f, -gravity * dt * dt};
    for(za::SizeT i = 0; i < n; i++)
    {
        if(!pinned[i])
        {
            const glm::vec3 v = (p[i] - prev[i]) * keep;
            prev[i] = p[i];
            p[i] += v + down;
        }
    }
}

void holdLengths(glm::vec3* p, const char* pinned, za::SizeT n, const float* rest, int iterations, float stiffness)
{
    for(int it = 0; it < iterations; it++)
    {
        for(za::SizeT j = 0; j + 1 < n; j++)
        {
            glm::vec3& a = p[j];
            glm::vec3& b = p[j + 1];
            const float wa = pinned[j] ? 0.f : 1.f;
            const float wb = pinned[j + 1] ? 0.f : 1.f;
            const glm::vec3 d = b - a;
            const float len = glm::length(d);
            if(len <= rest[j] || wa + wb <= 0.f)
            {
                continue;
            }
            const glm::vec3 corr = d * ((len - rest[j]) / (len * (wa + wb))) * stiffness;
            a += corr * wa;
            b -= corr * wb;
        }
    }
}

void collide(glm::vec3* p, glm::vec3* prev, const glm::vec3* from, const char* pinned, za::SizeT n, float rad, float slide,
    int* traces, char* touching)
{
    int count = 0;
    const auto touch = [&](za::SizeT i) {
        if(touching)
        {
            touching[i] = 1;
        }
    };
    if(touching)
    {
        for(za::SizeT i = 0; i < n; i++)
        {
            touching[i] = 0;
        }
    }
    const auto trace = [&](const glm::vec3& a, const glm::vec3& b) {
        count++;
        return worldtrace::world(a, b);
    };
    for(za::SizeT i = 0; i < n; i++)
    {
        if(pinned[i])
        {
            continue;
        }
        // Lying on the floor: held its thickness over it, its speed into it gone and some of its speed along it lost.
        // (A point moved only by the trace below sank through the thickness each frame and was put back on top: the
        // slack on the floor bounced by the thickness, many times a second.)
        const auto settle = [&] {
            const trace_t down = trace(p[i], p[i] - glm::vec3{0.f, 0.f, rad});
            if(down.startsolid || down.allsolid || down.fraction >= 1.f)
            {
                return;
            }
            const glm::vec3 nrm = worldtrace::normal(down);
            if(nrm.z < 0.3f)
            {
                return;
            }
            touch(i);
            const glm::vec3 on = worldtrace::endPos(down) + nrm * rad;
            const float up = glm::dot(on - p[i], nrm);
            if(up <= 0.f || trace(p[i], on).fraction < 1.f)
            {
                return;
            }
            glm::vec3 v = p[i] - prev[i];
            v -= nrm * za::min(0.f, glm::dot(v, nrm));
            v -= (v - nrm * glm::dot(v, nrm)) * (1.f - slide);
            p[i] += nrm * up;
            prev[i] = p[i] - v;
        };
        if(glm::distance(from[i], p[i]) < 1e-4f)
        {
            settle();
            continue;
        }
        const glm::vec3 was = from[i], to = p[i];
        const trace_t tr = trace(was, to);
        if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f)
        {
            if(!tr.startsolid && !tr.allsolid)
            {
                settle();
            }
            continue;
        }
        touch(i);
        const glm::vec3 nrm = worldtrace::normal(tr);
        const glm::vec3 at = worldtrace::endPos(tr) + nrm * rad;
        glm::vec3 rest = to - at;
        rest -= nrm * glm::dot(rest, nrm);
        const trace_t tr2 = trace(at, at + rest);
        if(tr2.startsolid)
        {
            p[i] = at;
        }
        else if(tr2.fraction < 1.f)
        {
            p[i] = worldtrace::endPos(tr2) + worldtrace::normal(tr2) * rad;
        }
        else
        {
            p[i] = at + rest;
        }
        glm::vec3 along = to - was;
        along -= nrm * glm::dot(along, nrm);
        prev[i] = p[i] - along * slide;
        settle();
    }
    for(za::SizeT i = 0; i + 1 < n; i++)
    {
        if(pinned[i] && pinned[i + 1])
        {
            continue;
        }
        const trace_t tr = trace(p[i], p[i + 1]);
        if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f)
        {
            continue;
        }
        const glm::vec3 nrm = worldtrace::normal(tr);
        const glm::vec3 hit = worldtrace::endPos(tr);
        for(const za::SizeT j : {i, i + 1})
        {
            if(pinned[j])
            {
                continue;
            }
            const float under = glm::dot(hit - p[j], nrm) + rad;
            if(under > 0.f)
            {
                const glm::vec3 to = p[j] + nrm * under;
                if(trace(p[j], to).fraction >= 1.f)
                {
                    p[j] = to;
                    prev[j] += nrm * under; // (no speed from it)
                    touch(j);
                }
            }
        }
    }
    if(traces)
    {
        *traces += count;
    }
}

void resample(const glm::vec3* pts, int count, int n, za::Vector<glm::vec3>& out)
{
    float total = 0.f;
    for(int i = 1; i < count; i++)
    {
        total += glm::distance(pts[i - 1], pts[i]);
    }
    out.pushBack(pts[0]);
    int seg = 1;
    float segStart = 0.f;
    for(int k = 1; k < n; k++)
    {
        const float want = total * static_cast<float>(k) / static_cast<float>(n);
        while(seg + 1 < count && segStart + glm::distance(pts[seg - 1], pts[seg]) < want)
        {
            segStart += glm::distance(pts[seg - 1], pts[seg]);
            seg++;
        }
        const float len = count > 1 ? glm::distance(pts[seg - 1], pts[seg]) : 0.f;
        const float t = len > 1e-4f ? za::clamp((want - segStart) / len, 0.f, 1.f) : 0.f;
        out.pushBack(count > 1 ? glm::mix(pts[seg - 1], pts[seg], t) : pts[0]);
    }
    out.pushBack(pts[count - 1]);
}

} // namespace qvr::verlet
