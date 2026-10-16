// vr_mdldequant.cpp -- .mdl vertices dequantised at load (vr_mdl_dequant; docs/vr-port/MDL_DEQUANT.md).
//
// A .mdl stores each pose's vertices as one byte per axis (times the model's scale plus its origin): every position is
// rounded to a 1/255th of the model's size, each frame on its own, so a vertex moving smoothly in the artist's animation
// jitters by up to half a step from frame to frame (the rippling, the wobble). At load, each vertex's path along each
// animation is replaced by the smoothest path (the least squared second differences, per axis) that stays within half a
// step of every stored value: no vertex is ever moved further than the data's own rounding error, and what was really
// there (a fast swing, a hold) stays.
//
// - Animations: the loops the server's QuakeC plays (QcLoop: knight_stand1 .. knight_stand9, then knight_stand1 again;
//   found from the progs' frame functions when the model is first dequantised) are runs of their own, smoothed round the
//   loop (cyclic: the last pose into the first, so the loop's seam is as smooth as its inside). The rest: a frame group
//   (several poses in one frame: a flame's flicker) is one looping run; single frames run on while their names' stems
//   (the name less its trailing digits: walk1..walk8) stay the same and their poses follow, split where the model jumps
//   much further between two frames than it moves along the rest of the run (an unnamed model's unrelated animations). A
//   run whose last pose comes back near its first (as near as the run moves from frame to frame) is a loop too. Runs of
//   one or two poses stay as stored.
// - The solver: projected accelerated gradient descent (FISTA with adaptive restart) per vertex and axis, a few dozen
//   sweeps; the result, as an offset from the stored byte in [-0.5, 0.5] steps, is kept per pose and vertex as three
//   signed bytes (1/254th of a step: 0.004 units on a 60-unit model) next to the model's poses (aliashdr_t's dequant:
//   the offsets, then the normals as the buffer holds them).
// - The renderer (gl_mesh.c): the offsets go into the model's buffer after its poses (zero while vr_mdl_dequant is 0: the
//   shader then draws exactly the stored bytes) and the alias shader adds them (SSBO 11, PoseFrac). Toggling the cvar
//   uploads every loaded model's buffer again. Nothing is done per frame on the CPU.
// - Normals: a pose's normal is one of 162 directions (up to 10.7 degrees from the true one), so a vertex that barely
//   moves flips between neighbouring directions from frame to frame and its shading flickers (an idle knight's: 7.5
//   degrees a frame). Along the same runs, each component takes the smoothest path within 0.1 of the stored direction's
//   (made unit again: at most about 10 degrees from it), kept next to the offsets and written into the buffer instead
//   of the table's while the cvar is on.
// - The CPU's copies: anchors (vr_anchor.cpp: your weapons' flashes, mounts and shells on their vertices) follow the
//   drawn positions (poseVertex; their binding to vertices keeps the stored rest pose). The rig fits (r_lerpmodels 3's
//   swings are added to the dequantised poses on the GPU), hit zones, collisions and AO bakes keep the stored bytes:
//   half a step is far below their tolerances.

#include "vr_mdldequant.hpp"
#include "vr_diskcache.hpp"
#include "vr_jobs.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_mem.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <string.h>

using namespace qvr;

namespace
{

constexpr float halfStep = 0.5f;     // the box round each stored value, in steps
constexpr int maxSweeps = 400;       // the solver's limit per vertex and axis
constexpr float tolerance = 1e-3f;   // ... it stops once no value moves more than this (steps)
constexpr float lipschitz = 16.f;    // the largest eigenvalue of D2'D2 (second differences)
constexpr float cutFactor = 3.f;     // a step this many times the median of the steps round it splits a run
constexpr float loopFactor = 1.5f;   // a run whose last pose is this near its first (times its median step) loops
// The box round each stored normal, per component: a .mdl normal is one of 162 directions, up to 10.7 degrees from the
// true one; within this box (made unit again) the smoothed one is at most about 10 degrees from the stored.
constexpr float normalBox = 0.1f;
constexpr float normalTolerance = 1e-3f; // (an eighth of the buffer's step: 1/127)

struct Run
{
    int first{0};
    int count{0};
    bool loop{false};
    bool qc{false}; // a loop of the progs' (QcLoop)
};

// The model's load totals (vr_mdl_dequant_check).
struct LoadTotals
{
    int models{0};
    double seconds{0.0};
    size_t bytes{0};
    int withoutProgs{0}; // (no server progs loaded: no progs loops)
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

// --- The progs' loops ---------------------------------------------------------------------------------------------
//
// Nothing in a .mdl says which of its animations loop, and QVR's own monsters' frames have no names (or numbers) to
// split them by: the QuakeC knows. Its frame functions (void() knight_stand9 = [$stand9, knight_stand1] {...}) compile to
// an OP_STATE (the frame, the next function), so the chain from a function that a model's own code names (monster_knight:
// self.th_stand = knight_stand1) comes round to a loop: frames 0..8. A model's code is the functions that name the model
// (setmodel (self, "progs/knight.mdl")). A loop that several models' functions name goes to the models that name it more
// than half as often as the most (a spawner naming several monsters' loops once: not to all of them; a monster's other
// models, mon_soldier.mdl for soldier.mdl, too); of a model's loops that overlap, the most named stays. Only loops of
// consecutive frames count (frames 0, 1, ..., 8, then 0).

struct QcLoop
{
    char model[MAX_QPATH]{};
    int firstFrame{0};
    int count{0};
    int named{0}; // the functions naming the model and a function of the loop (overlaps: the most named that fits, qcLoopsOf)
};

// The server progs' loops: made on the first model dequantised with that progs loaded (a few ms), kept for its key.
struct QcKey
{
    unsigned crc{0};
    int statements{-1};
    int functions{0};
    int globals{0};
    [[nodiscard]] bool operator==(const QcKey& o) const
    {
        return crc == o.crc && statements == o.statements && functions == o.functions && globals == o.globals;
    }
};
struct QcLoopCache
{
    QcKey key;
    double ms{0.0}; // (vr_mdl_dequant_check)
    za::Vector<QcLoop> loops;
    auto members() { return mem::list(key, ms, loops); }
};
mem::Cache<QcLoopCache> qcLoops{"mdl dequant loops", mem::GameDirChange};

// A frame function's frame and next function (its first statement an OP_STATE), or false.
[[nodiscard]] bool stateOf(const qcvm_t& vm, int func, int& frame, int& next)
{
    const dprograms_t* pr = vm.progs;
    if(func <= 0 || func >= pr->numfunctions)
    {
        return false;
    }
    const int first = vm.functions[func].first_statement;
    if(first <= 0 || first >= pr->numstatements || vm.statements[first].op != OP_STATE)
    {
        return false;
    }
    const int a = static_cast<unsigned short>(vm.statements[first].a);
    const int b = static_cast<unsigned short>(vm.statements[first].b);
    if(a >= pr->numglobals || b >= pr->numglobals)
    {
        return false;
    }
    const float f = vm.globals[a];
    if(!(f >= 0.f && f < 65536.f) || static_cast<float>(static_cast<int>(f)) != f)
    {
        return false;
    }
    frame = static_cast<int>(f);
    next = reinterpret_cast<const int*>(vm.globals)[b];
    return true;
}

// The loop a chain of frame functions comes round to (by its lowest function: the same loop from any of its functions),
// if its frames are consecutive (some rotation of first, first + 1, ...) and three or more.
struct Cycle
{
    int key{0};
    int firstFrame{0};
    int count{0};
};
[[nodiscard]] bool cycleOf(const qcvm_t& vm, int func, za::Vector<int>& chain, Cycle& out)
{
    chain.clear();
    for(int steps = 0; steps < 1024; steps++)
    {
        int frame = 0, next = 0;
        if(!stateOf(vm, func, frame, next))
        {
            return false;
        }
        for(size_t i = 0; i < chain.size(); i++)
        {
            if(chain[i] != func)
            {
                continue;
            }
            const int n = static_cast<int>(chain.size() - i);
            if(n < 3)
            {
                return false;
            }
            int lowFrame = 1 << 30, at = 0, key = chain[i];
            for(int k = 0; k < n; k++)
            {
                int fr = 0, nx = 0;
                (void)stateOf(vm, chain[i + static_cast<size_t>(k)], fr, nx);
                key = za::min(key, chain[i + static_cast<size_t>(k)]);
                if(fr < lowFrame)
                {
                    lowFrame = fr;
                    at = k;
                }
            }
            for(int k = 0; k < n; k++)
            {
                int fr = 0, nx = 0;
                (void)stateOf(vm, chain[i + static_cast<size_t>((at + k) % n)], fr, nx);
                if(fr != lowFrame + k)
                {
                    return false;
                }
            }
            out = Cycle{key, lowFrame, n};
            return true;
        }
        chain.pushBack(func);
        func = next;
    }
    return false;
}

// The loops of the server progs loaded now (none without one: a model loaded before any map keeps its own runs).
void buildQcLoops()
{
    const qcvm_t& vm = sv.qcvm;
    if(!vm.progs)
    {
        return;
    }
    const dprograms_t* pr = vm.progs;
    const QcKey key{vm.crc, pr->numstatements, pr->numfunctions, pr->numglobals};
    if(qcLoops.key == key)
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    qcLoops.key = key;
    qcLoops.loops.clear();
    const int* gi = reinterpret_cast<const int*>(vm.globals);

    // the globals that hold a model's name (the string immediates: "progs/knight.mdl"; not a variable, named, a
    // function's local or the calls' parameters and return, whatever it last held), and the frame functions
    za::Vector<char> local(static_cast<size_t>(pr->numglobals), 0);
    for(int g = 0; g < za::min(RESERVED_OFS, pr->numglobals); g++)
    {
        local[static_cast<size_t>(g)] = 1;
    }
    for(int f = 1; f < pr->numfunctions; f++)
    {
        const dfunction_t& fn = vm.functions[f];
        for(int g = za::max(fn.parm_start, 0); g < za::min(fn.parm_start + fn.locals, pr->numglobals); g++)
        {
            local[static_cast<size_t>(g)] = 1;
        }
    }
    for(int d = 0; d < pr->numglobaldefs; d++)
    {
        if(vm.globaldefs[d].ofs < pr->numglobals)
        {
            local[vm.globaldefs[d].ofs] = 1;
        }
    }
    za::Vector<int> modelOf(static_cast<size_t>(pr->numglobals), -1);
    za::Vector<const char*> names;
    for(int g = 0; g < pr->numglobals; g++)
    {
        const int v = gi[g];
        if(local[static_cast<size_t>(g)] || v <= 0 || v >= vm.stringssize || vm.strings[v - 1] != 0)
        {
            continue; // (a local, or not the start of a string)
        }
        const char* s = vm.strings + v;
        const size_t len = strnlen(s, za::min(static_cast<size_t>(vm.stringssize - v), static_cast<size_t>(MAX_QPATH)));
        if(len < 5 || len >= MAX_QPATH || q_strcasecmp(s + len - 4, ".mdl"))
        {
            continue;
        }
        int id = -1;
        for(size_t k = 0; k < names.size() && id < 0; k++)
        {
            id = q_strcasecmp(names[k], s) ? -1 : static_cast<int>(k);
        }
        if(id < 0)
        {
            id = static_cast<int>(names.size());
            names.pushBack(s);
        }
        modelOf[static_cast<size_t>(g)] = id;
    }
    za::Vector<int> stateFuncOf(static_cast<size_t>(pr->numglobals), -1);
    for(int d = 0; d < pr->numglobaldefs; d++)
    {
        const ddef_t& def = vm.globaldefs[d];
        int frame = 0, next = 0;
        if((def.type & ~DEF_SAVEGLOBAL) == ev_function && def.ofs < pr->numglobals && stateOf(vm, gi[def.ofs], frame, next))
        {
            stateFuncOf[def.ofs] = gi[def.ofs];
        }
    }

    // each function's models and frame functions named (its statements run on from its first to the next function's)
    za::Vector<int> owner(static_cast<size_t>(pr->numstatements), -1);
    for(int f = 1; f < pr->numfunctions; f++)
    {
        const int first = vm.functions[f].first_statement;
        if(first > 0 && first < pr->numstatements)
        {
            owner[static_cast<size_t>(first)] = f;
        }
    }
    struct Support
    {
        Cycle cycle;
        int model{0};
        int count{0}; // the functions naming the model and a function of the loop
    };
    za::Vector<Support> support;
    za::Vector<int> models, refs, counted, chain;
    const auto addUnique = [](za::Vector<int>& list, int value) {
        for(const int x : list)
        {
            if(x == value)
            {
                return;
            }
        }
        list.pushBack(value);
    };
    const auto flush = [&]() {
        counted.clear();
        for(const int ref : refs)
        {
            Cycle c;
            if(models.empty() || !cycleOf(vm, ref, chain, c))
            {
                continue;
            }
            bool seen = false;
            for(const int k : counted)
            {
                seen = seen || k == c.key;
            }
            if(seen)
            {
                continue;
            }
            counted.pushBack(c.key);
            for(const int m : models)
            {
                bool found = false;
                for(Support& s : support)
                {
                    if(s.cycle.key == c.key && s.model == m)
                    {
                        s.count++;
                        found = true;
                    }
                }
                if(!found)
                {
                    support.pushBack(Support{c, m, 1});
                }
            }
        }
        models.clear();
        refs.clear();
    };
    int cur = -1;
    for(int s = 1; s < pr->numstatements; s++)
    {
        if(owner[static_cast<size_t>(s)] >= 0 && owner[static_cast<size_t>(s)] != cur)
        {
            flush();
            cur = owner[static_cast<size_t>(s)];
        }
        const dstatement_t& st = vm.statements[s];
        for(const short operand : {st.a, st.b, st.c})
        {
            const int g = static_cast<unsigned short>(operand);
            if(g >= pr->numglobals)
            {
                continue;
            }
            if(modelOf[static_cast<size_t>(g)] >= 0)
            {                addUnique(models, modelOf[static_cast<size_t>(g)]);
            }
            if(stateFuncOf[static_cast<size_t>(g)] >= 0)
            {
                addUnique(refs, stateFuncOf[static_cast<size_t>(g)]);
            }
        }
    }
    flush();

    // each loop to the models naming it more than half as often as the most (a model's overlapping loops: chosen by its
    // frames, qcLoopsOf: a function naming two models, AD's Hunter Ogre's spawn precaching progs/ogre.mdl for its upgrade,
    // gives the original's model its own loops too)
    for(size_t i = 0; i < support.size(); i++)
    {
        const Support& s = support[i];
        bool keep = true;
        for(size_t j = 0; j < support.size() && keep; j++)
        {
            const Support& o = support[j];
            if(i != j && o.cycle.key == s.cycle.key && o.count >= 2 * s.count)
            {
                keep = false;
            }
        }
        if(keep)
        {
            QcLoop loop;
            q_strlcpy(loop.model, names[static_cast<size_t>(s.model)], sizeof(loop.model));
            loop.firstFrame = s.cycle.firstFrame;
            loop.count = s.cycle.count;
            loop.named = s.count;
            qcLoops.loops.pushBack(loop);
        }
    }
    qcLoops.ms = (Sys_DoubleTime() - t0) * 1000.0;
}

// A run of frames split at its jumps (unrelated animations: frames without names), each part's loop found.
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
    // a jump: a step much longer than the steps round it (two each side), or than a step
    const auto jump = [&](int i) {
        za::Vector<float> round;
        for(int k = i - 2; k <= i + 2; k++)
        {
            if(k != i && k >= 0 && k + 1 < run.count)
            {
                round.pushBack(steps[static_cast<size_t>(k)]);
            }
        }
        return steps[static_cast<size_t>(i)] > cutFactor * za::max(medianOf(round), 1.f);
    };
    int start = 0;
    for(int i = 0; i < run.count; i++)
    {
        const bool end = i + 1 == run.count || jump(i);
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

// A frame's name's trailing number (stand8: 8; none: -1).
[[nodiscard]] int numberOf(const char* name)
{
    const int n = static_cast<int>(strnlen(name, 16));
    int digits = n;
    while(digits > 0 && name[digits - 1] >= '0' && name[digits - 1] <= '9')
    {
        digits--;
    }
    if(digits == n || n - digits > 6)
    {
        return -1;
    }
    int v = 0;
    for(int i = digits; i < n; i++)
    {
        v = v * 10 + (name[i] - '0');
    }
    return v;
}

// Whether frame `a`'s name runs on into the next's: the same stem (not none: the knight's frames are numbers alone),
// numbered on (stand8, stand9).
[[nodiscard]] bool namedOn(const aliashdr_t* hdr, int a)
{
    if(a < 0 || a + 1 >= hdr->numframes)
    {
        return false;
    }
    char sa[17], sb[17];
    stemOf(hdr->frames[a].name, sa);
    stemOf(hdr->frames[a + 1].name, sb);
    const int na = numberOf(hdr->frames[a].name);
    return sa[0] && !strcmp(sa, sb) && na >= 0 && numberOf(hdr->frames[a + 1].name) == na + 1;
}

// The progs' loops of `model` that fit its frames (single poses one after another, the names not running on into the
// loop's first frame or out of its last: AD's Hunter Ogre's stand1..stand8 isn't the original ogre's, whose stand9 follows),
// overlapping ones: the most named.
[[nodiscard]] za::Vector<QcLoop> qcLoopsOf(const aliashdr_t* hdr, const char* model)
{
    za::Vector<QcLoop> fit, chosen;
    if(!model)
    {
        return chosen;
    }
    for(const QcLoop& loop : qcLoops.loops)
    {
        if(loop.firstFrame < 0 || loop.count < 1 || loop.firstFrame + loop.count > hdr->numframes || q_strcasecmp(loop.model, model))
        {
            continue;
        }
        bool fits = !namedOn(hdr, loop.firstFrame - 1) && !namedOn(hdr, loop.firstFrame + loop.count - 1);
        for(int k = 0; k < loop.count && fits; k++)
        {
            const maliasframedesc_t& f = hdr->frames[loop.firstFrame + k];
            fits = f.numposes == 1 && f.firstpose == hdr->frames[loop.firstFrame].firstpose + k;
        }
        if(fits)
        {
            fit.pushBack(loop);
        }
    }
    za::Vector<char> taken(fit.size(), 0);
    for(size_t round = 0; round < fit.size(); round++)
    {
        size_t best = fit.size();
        for(size_t i = 0; i < fit.size(); i++)
        {
            if(!taken[i] && (best == fit.size() || fit[i].named > fit[best].named))
            {
                best = i;
            }
        }
        taken[best] = 1;
        const QcLoop& b = fit[best];
        bool overlaps = false;
        for(const QcLoop& c : chosen)
        {
            overlaps = overlaps || (c.firstFrame < b.firstFrame + b.count && b.firstFrame < c.firstFrame + c.count);
        }
        if(!overlaps)
        {
            chosen.pushBack(b);
        }
    }
    return chosen;
}

// The loop of `loops` that starts at `frame`: its frames, or 0.
[[nodiscard]] int qcLoopAt(const za::Vector<QcLoop>& loops, int frame)
{
    for(const QcLoop& loop : loops)
    {
        if(loop.firstFrame == frame)
        {
            return loop.count;
        }
    }
    return 0;
}

// The model's animations as runs of poses: the progs' loops of `model` (none: null) as they are, the rest by names and jumps.
[[nodiscard]] za::Vector<Run> runsOf(const aliashdr_t* hdr, const char* model, int* qcUsed = nullptr)
{
    const trivertx_t* verts = posesOf(hdr);
    const za::Vector<QcLoop> loops = qcLoopsOf(hdr, model);
    za::Vector<Run> runs;
    Run cur;
    char curStem[17] = {0};
    for(int f = 0; f < hdr->numframes; f++)
    {
        const maliasframedesc_t& frame = hdr->frames[f];
        if(const int loop = qcLoopAt(loops, f); loop > 0)
        {
            if(cur.count > 0)
            {
                pushNamedRun(runs, verts, hdr->numverts, cur);
                cur.count = 0;
            }
            runs.pushBack(Run{frame.firstpose, loop, true, true});
            if(qcUsed)
            {
                ++*qcUsed;
            }
            f += loop - 1;
            continue;
        }
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
    za::Vector<float> n; // a vertex's normals along the run (x, y, z per pose)
};

// g = D2'D2 y (second differences; a loop's wrap round), d the second differences (n >= 3).
void gradient(const float* y, float* d, float* g, int n, bool loop)
{
    for(int i = 1; i < n - 1; i++)
    {
        d[i] = y[i - 1] - 2.f * y[i] + y[i + 1];
    }
    d[0] = loop ? y[n - 1] - 2.f * y[0] + y[1] : 0.f;
    d[n - 1] = loop ? y[n - 2] - 2.f * y[n - 1] + y[0] : 0.f;
    for(int i = 1; i < n - 1; i++)
    {
        g[i] = d[i - 1] - 2.f * d[i] + d[i + 1];
    }
    g[0] = (loop ? d[n - 1] : 0.f) - 2.f * d[0] + d[1];
    g[n - 1] = d[n - 2] - 2.f * d[n - 1] + (loop ? d[0] : 0.f);
}

// The smoothest values within half a step of s.v (n >= 3 of them) into s.x; the sweeps taken.
int solve(Solve& s, int n, bool loop, float box = halfStep, float tol = tolerance)
{
    const float* v = s.v.data();
    float* x = s.x.data();
    float* y = s.y.data();
    float* g = s.g.data();
    float* d = s.d.data();
    float* next = s.next.data();
    for(int i = 0; i < n; i++)
    {
        x[i] = v[i];
        y[i] = v[i];
    }
    float t = 1.f;
    int sweep = 0;
    for(; sweep < maxSweeps; sweep++)
    {
        gradient(y, d, g, n, loop);
        float moved = 0.f;
        float restart = 0.f;
        for(int i = 0; i < n; i++)
        {
            next[i] = za::clamp(y[i] - g[i] * (1.f / lipschitz), v[i] - box, v[i] + box);
            moved = za::max(moved, za::abs(next[i] - x[i]));
            restart += g[i] * (next[i] - x[i]);
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
            y[i] = next[i] + beta * (next[i] - x[i]);
            x[i] = next[i];
        }
        t = tn;
        if(moved < tol)
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

// A normal as the vertex buffer holds it (gl_mesh.c: 127 times each component, truncated), packed.
[[nodiscard]] unsigned packNormal(float x, float y, float z)
{
    const auto byteOf = [](float c) { return static_cast<unsigned>(static_cast<unsigned char>(static_cast<signed char>(127.f * c))); };
    return byteOf(x) | byteOf(y) << 8 | byteOf(z) << 16;
}

[[nodiscard]] glm::vec3 unpackNormal(unsigned packed)
{
    return glm::vec3{static_cast<float>(static_cast<signed char>(packed & 255u)),
               static_cast<float>(static_cast<signed char>((packed >> 8) & 255u)),
               static_cast<float>(static_cast<signed char>((packed >> 16) & 255u))} /
           127.f;
}

// Every pose's offsets (numposes x numverts packed; zero where a run is too short to smooth), then every pose's normals
// (numposes x numverts packed as the buffer holds them: the stored one where nothing is smoothed), along `model`'s runs
// (null: without the progs' loops).
void solveVertex(const aliashdr_t* hdr, const trivertx_t* verts, const za::Vector<Run>& runs, int vert, Solve& s,
    unsigned* out, unsigned* normals, long* sweeps, long* solves);

jobs::Site computeSite{"mdl dequant"}; // (its parallelFor: vr_jobs_sites)

void compute(const aliashdr_t* hdr, const char* model, unsigned* out, long* sweeps = nullptr, long* solves = nullptr)
// (sweeps, solves: positions' [0] and normals' [1] counted)
{
    const trivertx_t* verts = posesOf(hdr);
    const size_t total = static_cast<size_t>(hdr->numposes) * hdr->numverts;
    unsigned* normals = out + total;
    for(size_t i = 0; i < total; i++)
    {
        out[i] = 0u;
        const float* n = r_avertexnormals[verts[i].lightnormalindex % NUMVERTEXNORMALS];
        normals[i] = packNormal(n[0], n[1], n[2]);
    }
    buildQcLoops();
    const za::Vector<Run> runs = runsOf(hdr, model);
    int longest = 0;
    for(const Run& r : runs)
    {
        longest = za::max(longest, r.count);
    }
    // Each vertex is solved on its own (its runs in order, as before): the vertices split between the pool's threads,
    // each chunk with its own scratch. The same results; the counts (vr_mdl_dequant_check) on the caller alone.
    jobs::parallelFor(computeSite, static_cast<za::SizeT>(hdr->numverts), 32, [&](za::SizeT begin, za::SizeT end) {
        Solve s;
        for(za::Vector<float>* b : {&s.v, &s.x, &s.y, &s.g, &s.d, &s.next})
        {
            b->resize(static_cast<size_t>(longest));
        }
        s.n.resize(static_cast<size_t>(longest) * 3);
        for(int vert = static_cast<int>(begin); vert < static_cast<int>(end); vert++)
        {
            solveVertex(hdr, verts, runs, vert, s, out, normals, sweeps, solves);
        }
    }, sweeps == nullptr);
}

// compute's work for one vertex: its positions' and normals' paths along each run, in the runs' order.
void solveVertex(const aliashdr_t* hdr, const trivertx_t* verts, const za::Vector<Run>& runs, int vert, Solve& s,
    unsigned* out, unsigned* normals, long* sweeps, long* solves)
{
    for(const Run& r : runs)
    {
        if(r.count < 3)
        {
            continue;
        }
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
                const int taken = solve(s, r.count, r.loop);
                if(sweeps)
                {
                    sweeps[0] += taken + 1;
                    solves[0]++;
                }
                for(int i = 0; i < r.count; i++)
                {
                    const float offset = s.x[static_cast<size_t>(i)] - s.v[static_cast<size_t>(i)];
                    out[static_cast<size_t>(r.first + i) * hdr->numverts + vert] |=
                        static_cast<unsigned>(static_cast<unsigned char>(encode(offset))) << (8 * axis);
                }
            }
            // its normal: each component the smoothest within normalBox of the stored direction's, then made unit again
            bool turns = false;
            for(int i = 1; i < r.count && !turns; i++)
            {
                turns = verts[static_cast<size_t>(r.first + i) * hdr->numverts + vert].lightnormalindex !=
                        verts[static_cast<size_t>(r.first) * hdr->numverts + vert].lightnormalindex;
            }
            if(!turns)
            {
                continue; // (one direction all along: kept)
            }
            for(int axis = 0; axis < 3; axis++)
            {
                for(int i = 0; i < r.count; i++)
                {
                    const int index = verts[static_cast<size_t>(r.first + i) * hdr->numverts + vert].lightnormalindex;
                    s.v[static_cast<size_t>(i)] = r_avertexnormals[index % NUMVERTEXNORMALS][axis];
                }
                const int taken = solve(s, r.count, r.loop, normalBox, normalTolerance);
                if(sweeps)
                {
                    sweeps[1] += taken + 1;
                    solves[1]++;
                }
                for(int i = 0; i < r.count; i++)
                {
                    s.n[static_cast<size_t>(i) * 3 + static_cast<size_t>(axis)] = s.x[static_cast<size_t>(i)];
                }
            }
            for(int i = 0; i < r.count; i++)
            {
                const glm::vec3 n = glm::normalize(glm::vec3{s.n[static_cast<size_t>(i) * 3], s.n[static_cast<size_t>(i) * 3 + 1],
                    s.n[static_cast<size_t>(i) * 3 + 2]});
                normals[static_cast<size_t>(r.first + i) * hdr->numverts + vert] = packNormal(n.x, n.y, n.z);
            }
        }
    }
}

// The progs' loops' seams (QcLoop): the second differences' root mean square and largest (steps) on the axes a vertex
// moves 3 steps or less along over the loop, at its seam (its last pose and its first, played after it) and inside; for
// the stored bytes [0], the runs found without the progs (as before them) [1] and now [2].
struct Seams
{
    double seam[3]{};
    double inner[3]{};
    double seamMax[3]{};
    double innerMax[3]{};
    long seamPoints{0};
    long innerPoints{0};
    void finish()
    {
        for(int set = 0; set < 3; set++)
        {
            seam[set] = seamPoints ? za::sqrt(seam[set] / static_cast<double>(seamPoints)) : 0.0;
            inner[set] = innerPoints ? za::sqrt(inner[set] / static_cast<double>(innerPoints)) : 0.0;
        }
    }
};

void addSeams(const aliashdr_t* hdr, const Run& r, const unsigned* const sets[3], Seams& s)
{
    if(!r.qc)
    {
        return;
    }
    const trivertx_t* verts = posesOf(hdr);
    for(int vert = 0; vert < hdr->numverts; vert++)
    {
        for(int axis = 0; axis < 3; axis++)
        {
            int lo = 255, hi = 0;
            for(int i = 0; i < r.count; i++)
            {
                const int value = verts[static_cast<size_t>(r.first + i) * hdr->numverts + vert].v[axis];
                lo = za::min(lo, value);
                hi = za::max(hi, value);
            }
            if(hi == lo || hi - lo > 3)
            {
                continue;
            }
            for(int i = 0; i < r.count; i++)
            {
                const bool atSeam = i == 0 || i == r.count - 1;
                for(int set = 0; set < 3; set++)
                {
                    const auto value = [&](int k) {
                        const size_t at = static_cast<size_t>(r.first + (k + r.count) % r.count) * hdr->numverts + vert;
                        return verts[at].v[axis] + (sets[set] ? decode(sets[set][at], axis) : 0.f);
                    };
                    const double d2 = value(i - 1) - 2.f * value(i) + value(i + 1);
                    (atSeam ? s.seam : s.inner)[set] += d2 * d2;
                    double& top = (atSeam ? s.seamMax : s.innerMax)[set];
                    top = za::max(top, za::abs(d2));
                }
                (atSeam ? s.seamPoints : s.innerPoints)++;
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
    double slowBefore{0.0}; // roughness: the fourth differences' root mean square per axis (steps; rounding noise alone: 2.4)
    double slowAfter{0.0};
    long slowPoints{0};
    double stillBefore{0.0}; // the second differences' root mean square (steps) on the axes a vertex moves 3 steps or less along
    double stillAfter{0.0};
    long stillPoints{0};
    long sweeps[2]{}; // the solver's, all vertices and axes: positions, normals
    long solves[2]{};
    float maxOffset{0.f};
    bool same{true};
    double ms{0.0};
    int qcLoops{0};
    Seams seams;
    double turnBefore{0.0}; // the normals' turn from pose to pose (degrees, rms) on the vertices that move 3 steps or less
    double turnAfter{0.0};
    double turnMaxBefore{0.0};
    double turnMaxAfter{0.0};
    long turnPoints{0};
    double normalOff{0.0}; // the furthest a normal is from its stored direction (degrees)
};

[[nodiscard]] Check checkModel(const aliashdr_t* hdr, const char* model)
{
    Check c;
    const size_t total = static_cast<size_t>(hdr->numposes) * hdr->numverts;
    za::Vector<unsigned> fresh(2 * total);
    const double t0 = Sys_DoubleTime();
    compute(hdr, model, fresh.data(), c.sweeps, c.solves);
    c.ms = (Sys_DoubleTime() - t0) * 1000.0;
    za::Vector<unsigned> legacy(2 * total);
    compute(hdr, nullptr, legacy.data());
    if(hdr->dequant && hdr->dequantready)
    {
        const auto* stored = reinterpret_cast<const unsigned*>(reinterpret_cast<const byte*>(hdr) + hdr->dequant);
        c.same = !memcmp(stored, fresh.data(), 2 * total * sizeof(unsigned));
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
    const za::Vector<Run> runs = runsOf(hdr, model, &c.qcLoops);
    const unsigned* const sets[3] = {nullptr, legacy.data(), fresh.data()};
    for(const Run& r : runs)
    {
        addSeams(hdr, r, sets, c.seams);
    }
    c.seams.finish();
    // the normals: how far they turn from pose to pose on the vertices that move 3 steps or less (the shading's flicker),
    // stored and now, and how far one is from its stored direction
    const unsigned* normals = fresh.data() + total;
    const auto angle = [](const glm::vec3& a, const glm::vec3& b) {
        return glm::degrees(za::acos(za::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.f, 1.f)));
    };
    for(const Run& r : runs)
    {
        if(r.count < 3)
        {
            continue;
        }
        for(int vert = 0; vert < hdr->numverts; vert++)
        {
            bool still = true;
            for(int axis = 0; axis < 3 && still; axis++)
            {
                int lo = 255, hi = 0;
                for(int i = 0; i < r.count; i++)
                {
                    const int value = verts[static_cast<size_t>(r.first + i) * hdr->numverts + vert].v[axis];
                    lo = za::min(lo, value);
                    hi = za::max(hi, value);
                }
                still = hi - lo <= 3;
            }
            const auto storedAt = [&](int i) {
                const size_t k = static_cast<size_t>(r.first + (i % r.count)) * hdr->numverts + vert;
                return unpackNormal(packNormal(r_avertexnormals[verts[k].lightnormalindex % NUMVERTEXNORMALS][0],
                    r_avertexnormals[verts[k].lightnormalindex % NUMVERTEXNORMALS][1],
                    r_avertexnormals[verts[k].lightnormalindex % NUMVERTEXNORMALS][2]));
            };
            const auto nowAt = [&](int i) {
                return unpackNormal(normals[static_cast<size_t>(r.first + (i % r.count)) * hdr->numverts + vert]);
            };
            for(int i = 0; i < r.count; i++)
            {
                c.normalOff = za::max(c.normalOff, static_cast<double>(angle(storedAt(i), nowAt(i))));
                if(!still || (!r.loop && i + 1 == r.count))
                {
                    continue;
                }
                const double before = angle(storedAt(i), storedAt(i + 1));
                const double after = angle(nowAt(i), nowAt(i + 1));
                c.turnBefore += before * before;
                c.turnAfter += after * after;
                c.turnMaxBefore = za::max(c.turnMaxBefore, before);
                c.turnMaxAfter = za::max(c.turnMaxAfter, after);
                c.turnPoints++;
            }
        }
    }
    if(c.turnPoints)
    {
        c.turnBefore = za::sqrt(c.turnBefore / static_cast<double>(c.turnPoints));
        c.turnAfter = za::sqrt(c.turnAfter / static_cast<double>(c.turnPoints));
    }
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
            bool slow[3]; // the axes it moves along by 3 steps or less over the run: its wobble is the rounding's alone
            for(int axis = 0; axis < 3; axis++)
            {
                int lo = 255, hi = 0;
                for(int i = 0; i < r.count; i++)
                {
                    const int value = verts[static_cast<size_t>(r.first + i) * hdr->numverts + vert].v[axis];
                    lo = za::min(lo, value);
                    hi = za::max(hi, value);
                }
                slow[axis] = hi > lo && hi - lo <= 3;
            }
            for(int i = first; i < last; i++)
            {
                const int a = r.first + (i + r.count - 1) % r.count;
                const int m = r.first + i;
                const int b = r.first + (i + 1) % r.count;
                const glm::vec3 d2Before = at(a, vert, false) - 2.f * at(m, vert, false) + at(b, vert, false);
                const glm::vec3 d2After = at(a, vert, true) - 2.f * at(m, vert, true) + at(b, vert, true);
                sumBefore += glm::length(d2Before);
                sumAfter += glm::length(d2After);
                points++;
                for(int axis = 0; axis < 3; axis++)
                {
                    if(slow[axis])
                    {
                        const double before = d2Before[axis] / hdr->scale[axis];
                        const double after = d2After[axis] / hdr->scale[axis];
                        c.stillBefore += before * before;
                        c.stillAfter += after * after;
                        c.stillPoints++;
                    }
                }
                const bool inner = r.loop ? r.count >= 5 : i >= 2 && i + 2 < r.count;
                for(int axis = 0; inner && axis < 3; axis++) // roughness: the fourth difference (a cubic's residual), steps
                {
                    const auto fourth = [&](bool after) {
                        double sum = 0.0;
                        constexpr double weights[5] = {1.0, -4.0, 6.0, -4.0, 1.0};
                        for(int k = -2; k <= 2; k++)
                        {
                            const int pose = r.first + (i + k + r.count) % r.count;
                            sum += weights[k + 2] * at(pose, vert, after)[axis];
                        }
                        return sum / hdr->scale[axis];
                    };
                    const double before = fourth(false);
                    const double after = fourth(true);
                    c.slowBefore += before * before;
                    c.slowAfter += after * after;
                    c.slowPoints++;
                }
            }
        }
    }
    c.jitterBefore = points ? sumBefore / static_cast<double>(points) : 0.0;
    c.jitterAfter = points ? sumAfter / static_cast<double>(points) : 0.0;
    if(c.stillPoints)
    {
        c.stillBefore = za::sqrt(c.stillBefore / static_cast<double>(c.stillPoints));
        c.stillAfter = za::sqrt(c.stillAfter / static_cast<double>(c.stillPoints));
    }
    if(c.slowPoints)
    {
        c.slowBefore = za::sqrt(c.slowBefore / static_cast<double>(c.slowPoints));
        c.slowAfter = za::sqrt(c.slowAfter / static_cast<double>(c.slowPoints));
    }
    return c;
}

constexpr const char* defaultCheckModels[] = {"progs/soldier.mdl", "progs/knight.mdl", "progs/ogre.mdl",
    "progs/shambler.mdl", "progs/player.mdl", "progs/v_shot.mdl", "progs/v_axe.mdl", "progs/v_nail.mdl", "progs/dog.mdl",
    "progs/demon.mdl", "progs/zombie.mdl", "progs/flame2.mdl"};

void checkOne(const char* name, bool listRuns)
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
    const Check c = checkModel(hdr, model->name);
    const double less = c.jitterBefore > 0.0 ? 100.0 * (1.0 - c.jitterAfter / c.jitterBefore) : 0.0;
    const double slowLess = c.slowBefore > 0.0 ? 100.0 * (1.0 - c.slowAfter / c.slowBefore) : 0.0;
    const double stillLess = c.stillBefore > 0.0 ? 100.0 * (1.0 - c.stillAfter / c.stillBefore) : 0.0;
    Con_Printf("mdldequant: check %s: %d verts, %d poses, %d runs (%d loops); max offset %.3f steps (%s); jitter %.4f -> "
               "%.4f units (-%.0f%%), roughness %.3f -> %.3f steps (-%.0f%%), near-still wobble %.3f -> %.3f steps (-%.0f%%); %.2f ms, %.1f sweeps (normals %.1f); %d bytes; stored %s\n",
        name, hdr->numverts, hdr->numposes, c.runs, c.loops, c.maxOffset, c.maxOffset <= halfStep ? "ok" : "OVER",
        c.jitterBefore, c.jitterAfter, less, c.slowBefore, c.slowAfter, slowLess, c.stillBefore, c.stillAfter, stillLess, c.ms,
        c.solves[0] ? static_cast<double>(c.sweeps[0]) / static_cast<double>(c.solves[0]) : 0.0,
        c.solves[1] ? static_cast<double>(c.sweeps[1]) / static_cast<double>(c.solves[1]) : 0.0, hdr->numposes * hdr->numverts * 8,
        c.same ? "same" : "DIFFERENT");
    const auto printSeams = [&](const char* what, const Seams& s) {
        Con_Printf("mdldequant: check %s: %s: seam second difference rms %.3f stored, %.3f without the progs' loops -> "
                   "%.3f steps (max %.2f, %.2f -> %.2f), inside %.3f, %.3f -> %.3f (max %.2f, %.2f -> %.2f; %ld seam and "
                   "%ld inside points)\n",
            name, what, s.seam[0], s.seam[1], s.seam[2], s.seamMax[0], s.seamMax[1], s.seamMax[2], s.inner[0], s.inner[1],
            s.inner[2], s.innerMax[0], s.innerMax[1], s.innerMax[2], s.seamPoints, s.innerPoints);
    };
    Con_Printf("mdldequant: check %s: normals' turn from pose to pose (near-still vertices) %.2f -> %.2f degrees rms (max "
               "%.1f -> %.1f; %ld points), furthest from the stored direction %.1f degrees\n",
        name, c.turnBefore, c.turnAfter, c.turnMaxBefore, c.turnMaxAfter, c.turnPoints, c.normalOff);
    if(c.qcLoops > 0)
    {
        char what[32];
        q_snprintf(what, sizeof(what), "%d progs loops", c.qcLoops);
        printSeams(what, c.seams);
    }
    if(listRuns) // (one model named: its runs, by their first frame's name; each progs loop's seam)
    {
        const size_t total = static_cast<size_t>(hdr->numposes) * hdr->numverts;
        za::Vector<unsigned> fresh(2 * total), legacy(2 * total);
        compute(hdr, model->name, fresh.data());
        compute(hdr, nullptr, legacy.data());
        const unsigned* const sets[3] = {nullptr, legacy.data(), fresh.data()};
        for(const Run& r : runsOf(hdr, model->name))
        {
            const char* first = "";
            for(int f = 0; f < hdr->numframes; f++)
            {
                if(hdr->frames[f].firstpose == r.first)
                {
                    first = hdr->frames[f].name;
                    break;
                }
            }
            Con_Printf("mdldequant: run %s: poses %d..%d (%s)%s\n", name, r.first, r.first + r.count - 1, first,
                r.qc ? " loop (progs)" : r.loop ? " loop" : "");
            if(r.qc)
            {
                Seams s;
                addSeams(hdr, r, sets, s);
                s.finish();
                char what[32];
                q_snprintf(what, sizeof(what), "loop %d..%d", r.first, r.first + r.count - 1);
                printSeams(what, s);
            }
        }
    }
}

// vr_mdl_dequant_check [model ...]: the numbers above, for the named models (default: a few monsters and weapons).
void check_f()
{
    if(Cmd_Argc() > 1)
    {
        for(int i = 1; i < Cmd_Argc(); i++)
        {
            checkOne(Cmd_Argv(i), Cmd_Argc() == 2);
        }
    }
    else
    {
        for(const char* name : defaultCheckModels)
        {
            checkOne(name, false);
        }
    }
    Con_Printf("mdldequant: loads: %d models dequantised in %.1f ms, %.1f KB of offsets (vr_mdl_dequant %s); progs loops: "
               "%d (%d without a progs, built in %.1f ms)\n",
        loadTotals.models, loadTotals.seconds * 1000.0, static_cast<double>(loadTotals.bytes) / 1024.0,
        vr_mdl_dequant.string, static_cast<int>(qcLoops.loops.size()), loadTotals.withoutProgs, qcLoops.ms);
}

void onChanged(cvar_t*)
{
    Mod_ReuploadAliasModels();
}

} // namespace

namespace
{

// The disk cache's (vr_mdl_dequant_cache; vr_diskcache.hpp, cache/dequant): BUMP dequantVersion whenever a change here
// could give other offsets or normals for the same model and loops (the solver, its constants, the runs' rules, the
// normals' packing, Quake's normal table). vr_mdl_dequant_cache 2 computes anyway and compares.
constexpr unsigned dequantVersion = 1;
const za::String dequantBuild = [] {
    const unsigned parts[] = {dequantVersion, static_cast<unsigned>(sizeof(trivertx_t)), static_cast<unsigned>(NUMVERTEXNORMALS)};
    return qvr::diskcache::buildOf(parts, sizeof(parts) / sizeof(parts[0]));
}();
constexpr char dequantMagic[5] = "QVRD";

// All that compute reads: the poses, the frames (their poses and names: the runs) and the progs' loops it takes.
[[nodiscard]] unsigned long long dequantKey(const aliashdr_t* hdr, const char* model)
{
    using qvr::diskcache::fnv;
    buildQcLoops();
    const int counts[3] = {hdr->numposes, hdr->numverts, hdr->numframes};
    unsigned long long h = fnv(qvr::diskcache::fnvStart, counts, sizeof(counts));
    h = fnv(h, posesOf(hdr), static_cast<size_t>(hdr->numposes) * hdr->numverts * sizeof(trivertx_t));
    for(int f = 0; f < hdr->numframes; f++)
    {
        const maliasframedesc_t& d = hdr->frames[f];
        h = fnv(h, &d.firstpose, sizeof(d.firstpose));
        h = fnv(h, &d.numposes, sizeof(d.numposes));
        h = fnv(h, d.name, sizeof(d.name));
    }
    for(const QcLoop& loop : qcLoopsOf(hdr, model))
    {
        h = fnv(h, &loop.firstFrame, sizeof(loop.firstFrame));
        h = fnv(h, &loop.count, sizeof(loop.count));
    }
    return h;
}

// compute's result read from the disk, else computed (and written); vr_mdl_dequant_cache 2: computed and compared.
void computeCached(const aliashdr_t* hdr, const char* model, unsigned* out)
{
    using qvr::diskcache::Product;
    const int mode = qvr::diskcache::modeOf(Product::Dequant);
    const size_t bytes = static_cast<size_t>(hdr->numposes) * hdr->numverts * 2 * sizeof(unsigned);
    if(!mode)
    {
        compute(hdr, model, out);
        return;
    }
    const unsigned long long key = dequantKey(hdr, model);
    unsigned a = 0, b = 0;
    za::Vector<char> data;
    const bool read = qvr::diskcache::read(Product::Dequant, dequantBuild.cStr(), key, "dqt", dequantMagic, a, b, data, model) &&
                      a == static_cast<unsigned>(hdr->numposes) && b == static_cast<unsigned>(hdr->numverts) &&
                      data.size() == bytes;
    if(read && mode == 1)
    {
        memcpy(out, data.data(), bytes);
        return;
    }
    compute(hdr, model, out);
    if(read)
    {
        qvr::diskcache::noteChecked(Product::Dequant, memcmp(out, data.data(), bytes) == 0, model);
        return;
    }
    qvr::diskcache::write(Product::Dequant, dequantBuild.cStr(), key, "dqt", dequantMagic,
        static_cast<unsigned>(hdr->numposes), static_cast<unsigned>(hdr->numverts), out, bytes);
}

} // namespace

extern "C" void VR_MdlDequantCompute(const char* model, const void* aliashdr, unsigned* out)
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const double t0 = Sys_DoubleTime();
    computeCached(hdr, model, out);
    loadTotals.withoutProgs += sv.qcvm.progs ? 0 : 1;
    loadTotals.models++;
    loadTotals.seconds += Sys_DoubleTime() - t0;
    loadTotals.bytes += static_cast<size_t>(hdr->numposes) * hdr->numverts * 2 * sizeof(unsigned);
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

void prepare(const qmodel_s* model)
{
    if(!model || model->type != mod_alias || !VR_MdlDequantOn())
    {
        return;
    }
    auto* mainhdr = static_cast<aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(mainhdr->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        return;
    }
    // As GLMesh_LoadVertexBuffer does at the model's first upload (which then finds them ready).
    for(aliashdr_t* hdr = mainhdr; hdr; hdr = Mod_NextSurface(hdr))
    {
        if(hdr->dequant && !hdr->dequantready)
        {
            VR_MdlDequantCompute(model->name, hdr, reinterpret_cast<unsigned*>(reinterpret_cast<byte*>(hdr) + hdr->dequant));
            hdr->dequantready = 1;
        }
    }
}

glm::vec3 poseVertex(const void* aliashdr, int pose, int vertex)
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    const size_t k = static_cast<size_t>(pose) * hdr->numverts + vertex;
    const trivertx_t& t = posesOf(hdr)[k];
    glm::vec3 p{t.v[0], t.v[1], t.v[2]};
    if(hdr->dequant && hdr->dequantready && vr_mdl_dequant.value != 0.f)
    {
        const unsigned packed = reinterpret_cast<const unsigned*>(reinterpret_cast<const byte*>(hdr) + hdr->dequant)[k];
        p += glm::vec3{decode(packed, 0), decode(packed, 1), decode(packed, 2)};
    }
    return p;
}

void poseSuccessors(const void* aliashdr, const char* model, short* next, unsigned char* progs)
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    for(int p = 0; p < hdr->numposes; p++)
    {
        next[p] = -1;
        progs[p] = 0;
    }
    buildQcLoops();
    for(const Run& r : runsOf(hdr, model))
    {
        for(int k = 0; k < r.count && r.first + k < hdr->numposes; k++)
        {
            const int p = r.first + k;
            next[p] = static_cast<short>(k + 1 < r.count ? p + 1 : (r.loop && r.count > 1 ? r.first : -1));
            progs[p] = r.qc ? 1 : 0;
        }
    }
}

} // namespace qvr::mdldequant
