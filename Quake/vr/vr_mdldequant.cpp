// vr_mdldequant.cpp -- .mdl vertices dequantised at load (vr_mdl_dequant; docs/vr-port/MDL_DEQUANT.md).
//
// A .mdl stores each pose's vertices as one byte per axis (times the model's scale plus its origin): every position is
// rounded to a 1/255th of the model's size, each frame on its own, so a vertex moving smoothly in the artist's animation
// jitters by up to half a step from frame to frame (the rippling, the wobble). At load, each vertex's path along each
// animation is replaced by the smoothest path (the least squared second differences, per axis) that stays within half a
// step of every stored value: no vertex is ever moved further than the data's own rounding error, and what was really
// there (a fast swing, a hold) stays.
//
// - Animations: a frame group (several poses in one frame: a flame's flicker) is one looping run; single frames run on
//   while their names' stems (the name less its trailing digits: walk1..walk8) stay the same and their poses follow,
//   split where the model jumps much further between two frames than it moves along the rest of the run (an unnamed
//   model's unrelated animations). A run whose last pose comes back near its first (as near as the run moves from frame
//   to frame) is a loop: its ends are smoothed into each other. Runs of one or two poses stay as stored.
// - The solver: projected accelerated gradient descent (FISTA with adaptive restart) per vertex and axis, a few dozen
//   sweeps; the result, as an offset from the stored byte in [-0.5, 0.5] steps, is kept per pose and vertex as three
//   signed bytes (1/254th of a step: 0.004 units on a 60-unit model) next to the model's poses (aliashdr_t's dequant).
// - The renderer (gl_mesh.c): the offsets go into the model's buffer after its poses (zero while vr_mdl_dequant is 0: the
//   shader then draws exactly the stored bytes) and the alias shader adds them (SSBO 11, PoseFrac). Toggling the cvar
//   uploads every loaded model's buffer again. Nothing is done per frame on the CPU.
// - Normals: kept (each pose's stored normal index): the positions move by half a step at most, which turns no face
//   noticeably; the normals' own coarseness (162 directions) is another matter.
// - The CPU's copies: anchors (vr_anchor.cpp: things mounted on a vertex) read the drawn positions (poseVertex); the rig
//   fits, hit zones, collisions and AO bakes keep the stored bytes (half a step is far below their tolerances).

#include "vr_mdldequant.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <string.h>

using namespace qvr;

namespace
{

constexpr float halfStep = 0.5f;     // the box round each stored value, in steps
constexpr int maxSweeps = 400;       // the solver's limit per vertex and axis
constexpr float tolerance = 2e-4f;   // ... it stops once no value moves more than this (steps)
constexpr float lipschitz = 16.f;    // the largest eigenvalue of D2'D2 (second differences)
constexpr float cutFactor = 4.f;     // a jump this many times the run's median step splits it
constexpr float loopFactor = 1.5f;   // a run whose last pose is this near its first (times its median step) loops

struct Run
{
    int first{0};
    int count{0};
    bool loop{false};
};

// The model's load totals (vr_mdl_dequant_check).
struct LoadTotals
{
    int models{0};
    double seconds{0.0};
    size_t bytes{0};
};

LoadTotals loadTotals;

[[nodiscard]] const trivertx_t* posesOf(const aliashdr_t* hdr)
{
    return reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
}

// A frame's name less its trailing digits.
void stemOf(const char* name, char out[17])
{
    size_t n = strnlen(name, 16);
    memcpy(out, name, n);
    while(n > 0 && out[n - 1] >= '0' && out[n - 1] <= '9')
    {
        n--;
    }
    out[n] = 0;
}

// The mean distance (steps) between two poses' vertices.
[[nodiscard]] float poseDistance(const trivertx_t* verts, int numverts, int a, int b)
{
    const trivertx_t* pa = verts + static_cast<size_t>(a) * numverts;
    const trivertx_t* pb = verts + static_cast<size_t>(b) * numverts;
    double sum = 0.0;
    for(int v = 0; v < numverts; v++)
    {
        const float dx = static_cast<float>(pa[v].v[0]) - pb[v].v[0];
        const float dy = static_cast<float>(pa[v].v[1]) - pb[v].v[1];
        const float dz = static_cast<float>(pa[v].v[2]) - pb[v].v[2];
        sum += za::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return numverts > 0 ? static_cast<float>(sum / numverts) : 0.f;
}

[[nodiscard]] float medianOf(za::Vector<float> values)
{
    if(values.empty())
    {
        return 0.f;
    }
    for(size_t i = 1; i < values.size(); i++) // (a run's few steps)
    {
        for(size_t j = i; j > 0 && values[j - 1] > values[j]; j--)
        {
            const float t = values[j];
            values[j] = values[j - 1];
            values[j - 1] = t;
        }
    }
    return values[values.size() / 2];
}

// A named run split at its jumps, each part's loop found.
void pushNamedRun(za::Vector<Run>& runs, const trivertx_t* verts, int numverts, Run run)
{
    if(run.count < 3)
    {
        runs.pushBack(run);
        return;
    }
    za::Vector<float> steps;
    for(int i = 0; i + 1 < run.count; i++)
    {
        steps.pushBack(poseDistance(verts, numverts, run.first + i, run.first + i + 1));
    }
    const float cut = cutFactor * za::max(medianOf(steps), 1.f);
    int start = 0;
    for(int i = 0; i < run.count; i++)
    {
        const bool end = i + 1 == run.count || steps[static_cast<size_t>(i)] > cut;
        if(!end)
        {
            continue;
        }
        Run part{run.first + start, i + 1 - start, false};
        if(part.count >= 3)
        {
            za::Vector<float> own;
            for(int k = start; k < i; k++)
            {
                own.pushBack(steps[static_cast<size_t>(k)]);
            }
            const float closing = poseDistance(verts, numverts, part.first + part.count - 1, part.first);
            part.loop = closing <= loopFactor * za::max(medianOf(own), halfStep);
        }
        runs.pushBack(part);
        start = i + 1;
    }
}

// The model's animations as runs of poses.
[[nodiscard]] za::Vector<Run> runsOf(const aliashdr_t* hdr)
{
    const trivertx_t* verts = posesOf(hdr);
    za::Vector<Run> runs;
    Run cur;
    char curStem[17] = {0};
    for(int f = 0; f < hdr->numframes; f++)
    {
        const maliasframedesc_t& frame = hdr->frames[f];
        if(frame.numposes > 1)
        {
            if(cur.count > 0)
            {
                pushNamedRun(runs, verts, hdr->numverts, cur);
                cur.count = 0;
            }
            runs.pushBack(Run{frame.firstpose, frame.numposes, true});
            continue;
        }
        char stem[17];
        stemOf(frame.name, stem);
        if(cur.count > 0 && frame.firstpose == cur.first + cur.count && !strcmp(stem, curStem))
        {
            cur.count++;
            continue;
        }
        if(cur.count > 0)
        {
            pushNamedRun(runs, verts, hdr->numverts, cur);
        }
        cur = Run{frame.firstpose, 1, false};
        memcpy(curStem, stem, sizeof(curStem));
    }
    if(cur.count > 0)
    {
        pushNamedRun(runs, verts, hdr->numverts, cur);
    }
    return runs;
}

// The solver's buffers for one vertex and axis along a run.
struct Solve
{
    za::Vector<float> v, x, y, g, d, next;
};

// g = D2'D2 y (second differences; a loop's wrap round).
void gradient(Solve& s, int n, bool loop)
{
    for(int i = 0; i < n; i++)
    {
        s.g[static_cast<size_t>(i)] = 0.f;
    }
    const int first = loop ? 0 : 1;
    const int last = loop ? n : n - 1;
    for(int i = first; i < last; i++)
    {
        const int a = (i + n - 1) % n;
        const int b = (i + 1) % n;
        const float d = s.y[static_cast<size_t>(a)] - 2.f * s.y[static_cast<size_t>(i)] + s.y[static_cast<size_t>(b)];
        s.g[static_cast<size_t>(a)] += d;
        s.g[static_cast<size_t>(i)] -= 2.f * d;
        s.g[static_cast<size_t>(b)] += d;
    }
}

// The smoothest values within half a step of s.v (n of them) into s.x; the sweeps taken.
int solve(Solve& s, int n, bool loop)
{
    for(int i = 0; i < n; i++)
    {
        s.x[static_cast<size_t>(i)] = s.v[static_cast<size_t>(i)];
        s.y[static_cast<size_t>(i)] = s.v[static_cast<size_t>(i)];
    }
    float t = 1.f;
    int sweep = 0;
    for(; sweep < maxSweeps; sweep++)
    {
        gradient(s, n, loop);
        float moved = 0.f;
        float restart = 0.f;
        for(int i = 0; i < n; i++)
        {
            const size_t k = static_cast<size_t>(i);
            const float stepped = s.y[k] - s.g[k] / lipschitz;
            s.next[k] = za::clamp(stepped, s.v[k] - halfStep, s.v[k] + halfStep);
            moved = za::max(moved, za::abs(s.next[k] - s.x[k]));
            restart += s.g[k] * (s.next[k] - s.x[k]);
        }
        float tn = 0.5f * (1.f + za::sqrt(1.f + 4.f * t * t));
        if(restart > 0.f) // the momentum points uphill: start it again
        {
            t = 1.f;
            tn = 1.f;
        }
        const float beta = (t - 1.f) / tn;
        for(int i = 0; i < n; i++)
        {
            const size_t k = static_cast<size_t>(i);
            s.y[k] = s.next[k] + beta * (s.next[k] - s.x[k]);
            s.x[k] = s.next[k];
        }
        t = tn;
        if(moved < tolerance)
        {
            break;
        }
    }
    return sweep;
}

[[nodiscard]] signed char encode(float offset)
{
    const float q = za::clamp(offset, -halfStep, halfStep) * 254.f;
    return static_cast<signed char>(q >= 0.f ? static_cast<int>(q + 0.5f) : -static_cast<int>(-q + 0.5f));
}

[[nodiscard]] float decode(unsigned packed, int axis)
{
    return static_cast<float>(static_cast<signed char>((packed >> (8 * axis)) & 255u)) / 254.f;
}

// Every pose's offsets (numposes x numverts packed; zero where a run is too short to smooth).
void compute(const aliashdr_t* hdr, unsigned* out)
{
    const trivertx_t* verts = posesOf(hdr);
    const size_t total = static_cast<size_t>(hdr->numposes) * hdr->numverts;
    for(size_t i = 0; i < total; i++)
    {
        out[i] = 0u;
    }
    const za::Vector<Run> runs = runsOf(hdr);
    Solve s;
    int longest = 0;
    for(const Run& r : runs)
    {
        longest = za::max(longest, r.count);
    }
    for(za::Vector<float>* b : {&s.v, &s.x, &s.y, &s.g, &s.d, &s.next})
    {
        b->resize(static_cast<size_t>(longest));
    }
    for(const Run& r : runs)
    {
        if(r.count < 3)
        {
            continue;
        }
        for(int vert = 0; vert < hdr->numverts; vert++)
        {
            for(int axis = 0; axis < 3; axis++)
            {
                bool moves = false;
                for(int i = 0; i < r.count; i++)
                {
                    s.v[static_cast<size_t>(i)] = verts[static_cast<size_t>(r.first + i) * hdr->numverts + vert].v[axis];
                    moves = moves || s.v[static_cast<size_t>(i)] != s.v[0];
                }
                if(!moves)
                {
                    continue; // (a still vertex stays: nothing to smooth)
                }
                solve(s, r.count, r.loop);
                for(int i = 0; i < r.count; i++)
                {
                    const float offset = s.x[static_cast<size_t>(i)] - s.v[static_cast<size_t>(i)];
                    out[static_cast<size_t>(r.first + i) * hdr->numverts + vert] |=
                        static_cast<unsigned>(static_cast<unsigned char>(encode(offset))) << (8 * axis);
                }
            }
        }
    }
}

// A model's numbers (vr_mdl_dequant_check): jitter is the mean length of each vertex's second difference along its runs
// (model units), before and after; the largest offset (steps) and whether the stored offsets are the ones computed now.
struct Check
{
    int runs{0};
    int loops{0};
    double jitterBefore{0.0};
    double jitterAfter{0.0};
    double worstJitterBefore{0.0};
    double worstJitterAfter{0.0};
    float maxOffset{0.f};
    bool same{true};
    double ms{0.0};
};

[[nodiscard]] Check checkModel(const aliashdr_t* hdr)
{
    Check c;
    const size_t total = static_cast<size_t>(hdr->numposes) * hdr->numverts;
    za::Vector<unsigned> fresh(total);
    const double t0 = Sys_DoubleTime();
    compute(hdr, fresh.data());
    c.ms = (Sys_DoubleTime() - t0) * 1000.0;
    if(hdr->dequant)
    {
        const auto* stored = reinterpret_cast<const unsigned*>(reinterpret_cast<const byte*>(hdr) + hdr->dequant);
        c.same = !memcmp(stored, fresh.data(), total * sizeof(unsigned));
    }
    else
    {
        c.same = false;
    }
    const trivertx_t* verts = posesOf(hdr);
    for(size_t i = 0; i < total; i++)
    {
        for(int axis = 0; axis < 3; axis++)
        {
            c.maxOffset = za::max(c.maxOffset, za::abs(decode(fresh[i], axis)));
        }
    }
    const za::Vector<Run> runs = runsOf(hdr);
    double sumBefore = 0.0, sumAfter = 0.0;
    long points = 0;
    const auto at = [&](int pose, int vert, bool after) {
        const size_t k = static_cast<size_t>(pose) * hdr->numverts + vert;
        glm::vec3 p;
        for(int axis = 0; axis < 3; axis++)
        {
            p[axis] = (verts[k].v[axis] + (after ? decode(fresh[k], axis) : 0.f)) * hdr->scale[axis];
        }
        return p;
    };
    for(const Run& r : runs)
    {
        c.runs++;
        c.loops += r.loop ? 1 : 0;
        if(r.count < 3)
        {
            continue;
        }
        const int first = r.loop ? 0 : 1;
        const int last = r.loop ? r.count : r.count - 1;
        for(int vert = 0; vert < hdr->numverts; vert++)
        {
            for(int i = first; i < last; i++)
            {
                const int a = r.first + (i + r.count - 1) % r.count;
                const int m = r.first + i;
                const int b = r.first + (i + 1) % r.count;
                const double before = glm::length(at(a, vert, false) - 2.f * at(m, vert, false) + at(b, vert, false));
                const double after = glm::length(at(a, vert, true) - 2.f * at(m, vert, true) + at(b, vert, true));
                sumBefore += before;
                sumAfter += after;
                c.worstJitterBefore = za::max(c.worstJitterBefore, before);
                c.worstJitterAfter = za::max(c.worstJitterAfter, after);
                points++;
            }
        }
    }
    c.jitterBefore = points ? sumBefore / static_cast<double>(points) : 0.0;
    c.jitterAfter = points ? sumAfter / static_cast<double>(points) : 0.0;
    return c;
}

constexpr const char* defaultCheckModels[] = {"progs/soldier.mdl", "progs/knight.mdl", "progs/ogre.mdl",
    "progs/shambler.mdl", "progs/player.mdl", "progs/v_shot.mdl", "progs/v_axe.mdl", "progs/v_nail.mdl", "progs/dog.mdl",
    "progs/demon.mdl", "progs/zombie.mdl", "progs/flame2.mdl"};

void checkOne(const char* name)
{
    qmodel_t* model = Mod_ForName(name, false);
    if(!model || model->type != mod_alias)
    {
        Con_Printf("mdldequant: check %s: not an alias model\n", name);
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        Con_Printf("mdldequant: check %s: not a .mdl\n", name);
        return;
    }
    const Check c = checkModel(hdr);
    const double less = c.jitterBefore > 0.0 ? 100.0 * (1.0 - c.jitterAfter / c.jitterBefore) : 0.0;
    Con_Printf("mdldequant: check %s: %d verts, %d poses, %d runs (%d loops); max offset %.3f steps (%s); jitter %.4f -> "
               "%.4f units (-%.0f%%), worst %.3f -> %.3f; %.2f ms; %d bytes; stored %s\n",
        name, hdr->numverts, hdr->numposes, c.runs, c.loops, c.maxOffset, c.maxOffset <= halfStep ? "ok" : "OVER",
        c.jitterBefore, c.jitterAfter, less, c.worstJitterBefore, c.worstJitterAfter, c.ms,
        hdr->numposes * hdr->numverts * 4, c.same ? "same" : "DIFFERENT");
}

// vr_mdl_dequant_check [model ...]: the numbers above, for the named models (default: a few monsters and weapons).
void check_f()
{
    if(Cmd_Argc() > 1)
    {
        for(int i = 1; i < Cmd_Argc(); i++)
        {
            checkOne(Cmd_Argv(i));
        }
    }
    else
    {
        for(const char* name : defaultCheckModels)
        {
            checkOne(name);
        }
    }
    Con_Printf("mdldequant: loads: %d models dequantised in %.1f ms, %.1f KB of offsets (vr_mdl_dequant %s)\n",
        loadTotals.models, loadTotals.seconds * 1000.0, static_cast<double>(loadTotals.bytes) / 1024.0,
        vr_mdl_dequant.string);
}

void onChanged(cvar_t*)
{
    Mod_ReuploadAliasModels();
}

} // namespace

extern "C" void VR_MdlDequantCompute(const void* aliashdr, unsigned* out)
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const double t0 = Sys_DoubleTime();
    compute(hdr, out);
    loadTotals.models++;
    loadTotals.seconds += Sys_DoubleTime() - t0;
    loadTotals.bytes += static_cast<size_t>(hdr->numposes) * hdr->numverts * sizeof(unsigned);
}

extern "C" int VR_MdlDequantOn(void)
{
    return vr_mdl_dequant.value != 0.f;
}

namespace qvr::mdldequant
{

void init()
{
    Cvar_SetCallback(&vr_mdl_dequant, onChanged);
}

void registerCommands()
{
    Cmd_AddCommand("vr_mdl_dequant_check", check_f);
}

glm::vec3 poseVertex(const void* aliashdr, int pose, int vertex)
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const size_t k = static_cast<size_t>(pose) * hdr->numverts + vertex;
    const trivertx_t& t = posesOf(hdr)[k];
    glm::vec3 p{t.v[0], t.v[1], t.v[2]};
    if(hdr->dequant && vr_mdl_dequant.value != 0.f)
    {
        const unsigned packed = reinterpret_cast<const unsigned*>(reinterpret_cast<const byte*>(hdr) + hdr->dequant)[k];
        p += glm::vec3{decode(packed, 0), decode(packed, 1), decode(packed, 2)};
    }
    return p;
}

} // namespace qvr::mdldequant
