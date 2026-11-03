// vr_blowline.cpp -- a monster's melee blow as a line, read from its model's own baked frames (ROUND23.md, "Directional
// parry: the only parry"). The monsters without the armed humanoids' IK have no weapon entity: their weapon or claw is
// vertices in their frames. Nothing per monster is kept by hand, so a replaced model or a mod's monster gets its own.
//
// - Its animations: what its QuakeC plays, its frame functions' chain (void() knight_atk6 = [$attackb6, knight_atk7]: an
//   OP_STATE): the consecutive frames around the blow's: read when it first strikes in them. The models' own names won't
//   do (this game's knight's frames are "1", "2", ..., its shambler's all "From ASE", its dog's empty) nor the jumps
//   between frames (a fiend's claw comes down in one). A monster without a chain now: the dequantiser's runs.
// - Its strikers, per run: the vertex that travels furthest over the run, and the rigid part it belongs to: the vertices
//   whose distance to it stays the same over the whole run (a sword, a chainsaw, a forearm and its claw; a Quake .mdl has
//   no bones, but what a bone carries keeps its distances). The part's two furthest vertices are the line. A second
//   striker: another limb that travels at least `secondShare` as far (a fiend's other claw, a shambler's other arm),
//   sharing no vertex with the first.
// - Its blow, per pose: the striker that travels furthest around that pose (`windowBack`..`windowAhead` poses: a blow is
//   dealt as a frame starts, while the one before it is still drawn, and some blows come after their stroke). It counts
//   only if the line is long enough, gets ahead of the monster and swings: its far end moves `minSweep` square to it and
//   it turns `minTurn`. A dog's lunge (its whole body, pitching a little), a fish's bite (its tail is what moves) or a body
//   in the air (the spawn) doesn't: no line; the QC reads those by the guard (vr_dirparry.qc).
// - As drawn: Quake's clients blend from the last frame to the frame over the think's interval, so the line at a time is
//   the two ends blended the same way, whatever the move's arc in between. tau walks along the run; the run's ends hold.
//
// vr_blowline_dump prints all of it, and QC's Show Disarm Volumes draws the line on the monster.

#include "vr_blowline.hpp"

#include "vr_engine.hpp"
#include "vr_mdldequant.hpp"
#include "vr_mem.hpp"
#include "vr_progs.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <cstring>

using namespace qvr;

namespace
{

constexpr float minLength = 16.f;    // units: a shorter part is no line to meet (a dog's head)
constexpr float minSweep = 30.f;     // units its far end moves square to it around the blow
constexpr float minTurn = 25.f;      // degrees it turns around the blow (a dog's whole body pitching in a lunge: less)
constexpr float minReach = 20.f;     // units ahead of the model's origin it gets (a fish's tail, the fastest thing on it: never)
constexpr float secondShare = 0.6f;  // a second striker travels this much of the first's way over the run
constexpr float bothShare = 0.7f;    // ... and strikes with it at a pose where it goes this much of its way
constexpr int windowBack = 3;        // poses around the blow's: from this many back
constexpr int windowAhead = 2;       // ... to this many ahead
constexpr int maxTries = 24;         // vertices tried as a striker (a stray vertex, rigid with nothing, is passed over)
constexpr float frameInterval = 0.1f; // s: a monster's think
constexpr int maxRunFrames = 64;     // frames of a chain read

struct Striker
{
    int a{-1}, b{-1}; // the line's ends (vertices): a the nearer to the model's axis, b the further
    int tip{-1};      // the vertex that travels furthest
    float length{0.f};
    float reach{0.f}; // how far ahead (model +x) it gets over the run
};

struct PoseBlow
{
    int count{0};
    int striker[2]{0, 1};
    float sweep{0.f};       // the first's, around this pose (units)
    float turn{0.f};        // ... (degrees)
    glm::vec3 normal{0.f};  // the plane it sweeps at its fastest there (model axes): the ideal parry's direction
};

struct Run
{
    za::Vector<int> poses;
    bool analysed{false};
    bool chained{true}; // the progs' chain's; else the dequantiser's, kept for its one pose: a chain read later replaces it
    Striker striker[2];
    int strikers{0};
};

struct ModelBlows
{
    const qmodel_t* model{nullptr};
    int numVerts{0};
    int numPoses{0};
    za::Vector<short> next, prev; // per pose: the dequantiser's runs (a monster with no chain now)
    za::Vector<int> runOf;        // per pose: -1 until it's struck in
    za::Vector<int> placeIn;      // per pose: its place in its run
    za::Vector<Run> runs;
    za::Vector<PoseBlow> blows;   // per pose
};

// The server progs' frame functions: per function, the one before it in a consecutive chain (0: none). Made when first
// needed with that progs loaded.
struct ChainTable
{
    const void* progs{nullptr};
    za::Vector<int> before;
    auto members() { return mem::list(progs, before); }
};
mem::Cache<ChainTable> chains{"blow lines' frame chains", mem::GameDirChange};

// The models read so far.
struct KnownModels
{
    za::Vector<ModelBlows> models;
    auto members() { return mem::list(models); }
};
mem::Cache<KnownModels> known{"blow lines", mem::GameDirChange};

const aliashdr_t* headerOf(const qmodel_t* model)
{
    return static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
}

glm::vec3 vertexAt(const aliashdr_t* hdr, int pose, int vert)
{
    return mdldequant::modelVertex(hdr, pose, pose, 0.f, vert);
}

ModelBlows* modelFor(qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    const aliashdr_t* hdr = headerOf(model);
    if(!hdr || hdr->numposes < 1 || hdr->numverts < 2)
    {
        return nullptr;
    }
    for(ModelBlows& m : known.models)
    {
        if(m.model == model && m.numVerts == hdr->numverts && m.numPoses == hdr->numposes)
        {
            return &m;
        }
    }
    mdldequant::prepare(model);
    known.models.pushBack(ModelBlows{});
    ModelBlows* m = &known.models.back();
    m->model = model;
    m->numVerts = hdr->numverts;
    m->numPoses = hdr->numposes;
    const za::SizeT np = static_cast<za::SizeT>(hdr->numposes);
    za::Vector<unsigned char> progs;
    m->next.resize(np);
    m->prev.resize(np);
    m->runOf.resize(np);
    m->placeIn.resize(np);
    m->blows.resize(np);
    progs.resize(np);
    mdldequant::poseSuccessors(hdr, model->name, m->next.data(), progs.data());
    for(za::SizeT p = 0; p < np; p++)
    {
        m->prev[p] = -1;
        m->runOf[p] = -1;
    }
    for(za::SizeT p = 0; p < np; p++)
    {
        if(m->next[p] >= 0)
        {
            m->prev[static_cast<za::SizeT>(m->next[p])] = static_cast<short>(p);
        }
    }
    return m;
}

// The part of the model rigid with `tip` over the run, its line and length. False: under the tolerance's two vertices.
bool rigidLine(const aliashdr_t* hdr, const Run& run, int tip, float tolerance, za::Vector<int>& part, Striker& out)
{
    part.clear();
    for(int v = 0; v < hdr->numverts; v++)
    {
        float lo = 1e9f, hi = -1e9f;
        for(const int pose : run.poses)
        {
            const float d = glm::distance(vertexAt(hdr, pose, v), vertexAt(hdr, pose, tip));
            lo = za::min(lo, d);
            hi = za::max(hi, d);
        }
        if(hi - lo <= tolerance)
        {
            part.pushBack(v);
        }
    }
    const int rest = run.poses[0];
    out = Striker{};
    out.tip = tip;
    for(za::SizeT i = 0; i < part.size(); i++)
    {
        const glm::vec3 pi = vertexAt(hdr, rest, part[i]);
        for(za::SizeT j = i + 1; j < part.size(); j++)
        {
            const float d = glm::distance(pi, vertexAt(hdr, rest, part[j]));
            if(d > out.length)
            {
                out.length = d;
                out.a = part[i];
                out.b = part[j];
            }
        }
    }
    if(out.a < 0)
    {
        return false;
    }
    const glm::vec3 pa = vertexAt(hdr, rest, out.a);
    const glm::vec3 pb = vertexAt(hdr, rest, out.b);
    if(pa.x * pa.x + pa.y * pa.y > pb.x * pb.x + pb.y * pb.y)
    {
        const int was = out.a;
        out.a = out.b;
        out.b = was;
    }
    out.reach = -1e9f;
    for(const int pose : run.poses)
    {
        out.reach = za::max(out.reach, za::max(vertexAt(hdr, pose, out.a).x, vertexAt(hdr, pose, out.b).x));
    }
    return true;
}

// Run `r`'s strikers and each of its poses' blows (see the top).
void analyse(ModelBlows& m, Run& r)
{
    r.analysed = true;
    const aliashdr_t* hdr = headerOf(m.model);
    const int count = static_cast<int>(r.poses.size());
    if(count < 3)
    {
        return;
    }
    const float tolerance = za::max(2.5f, 3.f * za::max(hdr->scale[0], za::max(hdr->scale[1], hdr->scale[2])));
    za::Vector<float> path;
    path.resize(static_cast<za::SizeT>(hdr->numverts));
    for(int v = 0; v < hdr->numverts; v++)
    {
        float along = 0.f;
        for(int k = 0; k + 1 < count; k++)
        {
            along += glm::distance(vertexAt(hdr, r.poses[static_cast<za::SizeT>(k)], v),
                vertexAt(hdr, r.poses[static_cast<za::SizeT>(k + 1)], v));
        }
        path[static_cast<za::SizeT>(v)] = along;
    }
    za::Vector<unsigned char> taken;
    taken.resize(static_cast<za::SizeT>(hdr->numverts));
    for(za::SizeT v = 0; v < taken.size(); v++)
    {
        taken[v] = 0;
    }
    const auto furthest = [&]() {
        int best = -1;
        for(int v = 0; v < hdr->numverts; v++)
        {
            if(!taken[static_cast<za::SizeT>(v)] && (best < 0 || path[static_cast<za::SizeT>(v)] > path[static_cast<za::SizeT>(best)]))
            {
                best = v;
            }
        }
        return best;
    };
    za::Vector<int> part;
    int first = -1;
    for(int tries = 0; tries < maxTries && first < 0; tries++)
    {
        const int v = furthest();
        if(v < 0)
        {
            return;
        }
        taken[static_cast<za::SizeT>(v)] = 1;
        if(rigidLine(hdr, r, v, tolerance, part, r.striker[0]) && r.striker[0].length >= minLength)
        {
            first = v;
        }
    }
    if(first < 0)
    {
        return;
    }
    r.strikers = 1;
    za::Vector<unsigned char> inFirst;
    inFirst.resize(taken.size());
    for(za::SizeT v = 0; v < taken.size(); v++)
    {
        inFirst[v] = 0;
    }
    for(const int v : part)
    {
        taken[static_cast<za::SizeT>(v)] = 1;
        inFirst[static_cast<za::SizeT>(v)] = 1;
    }
    for(int tries = 0; tries < maxTries; tries++)
    {
        const int v = furthest();
        if(v < 0 || path[static_cast<za::SizeT>(v)] < secondShare * path[static_cast<za::SizeT>(first)])
        {
            break;
        }
        taken[static_cast<za::SizeT>(v)] = 1;
        Striker other;
        if(!rigidLine(hdr, r, v, tolerance, part, other) || other.length < minLength)
        {
            continue;
        }
        bool shares = false;
        for(const int w : part)
        {
            shares = shares || inFirst[static_cast<za::SizeT>(w)];
            taken[static_cast<za::SizeT>(w)] = 1;
        }
        if(!shares)
        {
            r.striker[1] = other;
            r.strikers = 2;
            break;
        }
    }
    for(int k = 0; k < count; k++)
    {
        PoseBlow& blow = m.blows[static_cast<za::SizeT>(r.poses[static_cast<za::SizeT>(k)])];
        const int from = za::max(0, k - windowBack);
        const int to = za::min(count - 1, k + windowAhead);
        float along[2]{0.f, 0.f};
        float sweep[2]{0.f, 0.f};
        float fastest[2]{0.f, 0.f};
        float turned[2]{0.f, 0.f};
        glm::vec3 plane[2]{glm::vec3{0.f}, glm::vec3{0.f}};
        for(int s = 0; s < r.strikers; s++)
        {
            const Striker& st = r.striker[s];
            for(int w = from; w < to; w++)
            {
                const int p0 = r.poses[static_cast<za::SizeT>(w)];
                const int p1 = r.poses[static_cast<za::SizeT>(w + 1)];
                const glm::vec3 t0 = vertexAt(hdr, p0, st.tip);
                const glm::vec3 t1 = vertexAt(hdr, p1, st.tip);
                const glm::vec3 axis = glm::normalize(vertexAt(hdr, p0, st.b) - vertexAt(hdr, p0, st.a));
                const glm::vec3 axis1 = glm::normalize(vertexAt(hdr, p1, st.b) - vertexAt(hdr, p1, st.a));
                turned[s] += std::acos(za::clamp(glm::dot(axis, axis1), -1.f, 1.f)) * 57.29578f;
                const glm::vec3 moved = t1 - t0;
                const glm::vec3 square = moved - axis * glm::dot(moved, axis);
                const float across = glm::length(square);
                along[s] += glm::length(moved);
                sweep[s] += across;
                if(across > fastest[s])
                {
                    fastest[s] = across;
                    plane[s] = glm::cross(axis, square);
                }
            }
        }
        const int main = (r.strikers > 1 && along[1] > along[0]) ? 1 : 0;
        const int other = 1 - main;
        const auto counts = [&](int s) {
            return r.striker[s].length >= minLength && r.striker[s].reach >= minReach && sweep[s] >= minSweep && turned[s] >= minTurn;
        };
        blow.turn = turned[main];
        if(!counts(main))
        {
            continue;
        }
        blow.count = 1;
        blow.striker[0] = main;
        blow.striker[1] = other;
        blow.sweep = sweep[main];
        blow.normal = glm::length(plane[main]) > 0.f ? glm::normalize(plane[main]) : glm::vec3{0.f};
        if(r.strikers > 1 && along[other] >= bothShare * along[main] && counts(other))
        {
            blow.count = 2;
        }
    }
}

// The chains' table for the progs loaded now.
const za::Vector<int>& chainTable()
{
    if(chains.progs != qcvm->progs || chains.before.size() != static_cast<za::SizeT>(qcvm->progs->numfunctions))
    {
        chains.progs = qcvm->progs;
        chains.before.resize(static_cast<za::SizeT>(qcvm->progs->numfunctions));
        for(za::SizeT f = 0; f < chains.before.size(); f++)
        {
            chains.before[f] = 0;
        }
        for(int f = 1; f < qcvm->progs->numfunctions; f++)
        {
            int frame = 0, next = 0, frameNext = 0, after = 0;
            if(mdldequant::frameFunction(f, frame, next) && mdldequant::frameFunction(next, frameNext, after) &&
                frameNext == frame + 1 && !chains.before[static_cast<za::SizeT>(next)])
            {
                chains.before[static_cast<za::SizeT>(next)] = f;
            }
        }
    }
    return chains.before;
}

// The run of the animation at `frame`, its frame function's next one `then` (the entity's think): the chain's consecutive
// frames around it (else the dequantiser's run); analysed. -1: no such frame's pose.
int runFor(ModelBlows& m, int frame, int then)
{
    const aliashdr_t* hdr = headerOf(m.model);
    const int pose = hdr->frames[frame].firstpose;
    if(pose < 0 || pose >= hdr->numposes)
    {
        return -1;
    }
    int thenFrame = -1, thenNext = 0;
    const bool chained = mdldequant::frameFunction(then, thenFrame, thenNext);
    const int had = m.runOf[static_cast<za::SizeT>(pose)];
    if(had >= 0 && (m.runs[static_cast<za::SizeT>(had)].chained || !chained))
    {
        return had;
    }
    za::Vector<int> ahead; // its frames after `frame`
    za::Vector<int> back;  // ... before it, nearest first
    const za::Vector<int>& before = chainTable();
    int fn = then;
    int f = thenFrame, nx = thenNext;
    int last = frame;
    while(fn > 0 && f == last + 1 && f < hdr->numframes && static_cast<int>(ahead.size()) < maxRunFrames)
    {
        ahead.pushBack(f);
        last = f;
        fn = nx;
        if(!mdldequant::frameFunction(fn, f, nx))
        {
            break;
        }
    }
    if(chained)
    {
        // Its own function: the one before `then`; at the animation's end, the one of this frame that leads to it.
        int cur = (thenFrame == frame + 1) ? before[static_cast<za::SizeT>(then)] : 0;
        for(int g = 1; !cur && g < qcvm->progs->numfunctions; g++)
        {
            int gf = 0, gn = 0;
            if(mdldequant::frameFunction(g, gf, gn) && gf == frame && gn == then)
            {
                cur = g;
            }
        }
        int at = frame;
        while(cur > 0 && at > 0 && static_cast<int>(back.size()) < maxRunFrames)
        {
            cur = before[static_cast<za::SizeT>(cur)];
            if(cur > 0)
            {
                at--;
                back.pushBack(at);
            }
        }
    }
    Run run;
    if(ahead.size() + back.size() >= 2)
    {
        for(za::SizeT k = back.size(); k > 0; k--)
        {
            run.poses.pushBack(hdr->frames[back[k - 1]].firstpose);
        }
        run.poses.pushBack(pose);
        for(const int a : ahead)
        {
            run.poses.pushBack(hdr->frames[a].firstpose);
        }
    }
    else
    {
        run.chained = false;
        int first = pose;
        for(int k = 0; k < maxRunFrames && m.prev[static_cast<za::SizeT>(first)] >= 0; k++)
        {
            first = m.prev[static_cast<za::SizeT>(first)];
        }
        int p = first;
        for(int k = 0; k < maxRunFrames && p >= 0; k++)
        {
            run.poses.pushBack(p);
            p = m.next[static_cast<za::SizeT>(p)];
            if(p == first)
            {
                break;
            }
        }
    }
    const int index = static_cast<int>(m.runs.size());
    for(za::SizeT k = 0; k < run.poses.size(); k++)
    {
        const za::SizeT q = static_cast<za::SizeT>(run.poses[k]);
        const bool free = m.runOf[q] < 0 || !m.runs[static_cast<za::SizeT>(m.runOf[q])].chained;
        // (a frame played in two animations: the one it's first struck in)
        if((run.chained && free) || static_cast<int>(q) == pose)
        {
            m.runOf[q] = index;
            m.placeIn[q] = static_cast<int>(k);
        }
    }
    m.runs.pushBack(static_cast<Run&&>(run));
    analyse(m, m.runs.back());
    return index;
}

// The pose a walk of `steps` along the run from `pose` comes to (the run's ends hold).
int walkFrom(const ModelBlows& m, const Run& run, int pose, int steps)
{
    const int place = za::clamp(m.placeIn[static_cast<za::SizeT>(pose)] + steps, 0, static_cast<int>(run.poses.size()) - 1);
    return run.poses[static_cast<za::SizeT>(place)];
}

// The name of the frame that starts at `pose` ("?": none).
const char* frameNameOf(const aliashdr_t* hdr, int pose)
{
    for(int f = 0; f < hdr->numframes; f++)
    {
        if(hdr->frames[f].firstpose == pose)
        {
            return hdr->frames[f].name;
        }
    }
    return "?";
}

void dump_f()
{
    if(Cmd_Argc() < 3 || !sv.active)
    {
        Con_Printf("vr_blowline_dump <model> <frame function>... (a map loaded): the lines a monster's blows have along that "
                   "animation, as vr_blowline_dump progs/knight.mdl knight_atk1\n");
        return;
    }
    PR_SwitchQCVM(&sv.qcvm);
    ModelBlows* m = modelFor(Mod_ForName(Cmd_Argv(1), false));
    if(!m)
    {
        Con_Printf("blowline: %s: not a .mdl\n", Cmd_Argv(1));
        PR_SwitchQCVM(nullptr);
        return;
    }
    const aliashdr_t* hdr = headerOf(m->model);
    for(int a = 2; a < Cmd_Argc(); a++)
    {
        dfunction_t* func = VR_ED_FindFunction(Cmd_Argv(a));
        int frame = 0, then = 0;
        if(!func || !mdldequant::frameFunction(static_cast<int>(func - qcvm->functions), frame, then) || frame >= hdr->numframes)
        {
            Con_Printf("blowline: %s: no frame function of it\n", Cmd_Argv(a));
            continue;
        }
        const int index = runFor(*m, frame, then);
        if(index < 0)
        {
            continue;
        }
        const Run& r = m->runs[static_cast<za::SizeT>(index)];
        Con_Printf("blowline: %s %s (%d poses): %d strikers", Cmd_Argv(1), Cmd_Argv(a), static_cast<int>(r.poses.size()), r.strikers);
        for(int t = 0; t < r.strikers; t++)
        {
            Con_Printf("; v%d-v%d %.1f long, reach %.1f", r.striker[t].a, r.striker[t].b, r.striker[t].length, r.striker[t].reach);
        }
        Con_Printf("\n");
        for(za::SizeT k = 0; k < r.poses.size(); k++)
        {
            const int pose = r.poses[k];
            const PoseBlow& b = m->blows[static_cast<za::SizeT>(pose)];
            Con_Printf("blowline:   +%-2d %-8s %d lines, sweep %5.1f, turn %3.0f, square %5.2f %5.2f %5.2f", static_cast<int>(k) + 1,
                frameNameOf(hdr, pose), b.count, b.sweep, b.turn, b.normal.x, b.normal.y, b.normal.z);
            for(int t = 0; t < b.count; t++)
            {
                const Striker& st = r.striker[b.striker[t]];
                const glm::vec3 p0 = vertexAt(hdr, pose, st.a);
                const glm::vec3 p1 = vertexAt(hdr, pose, st.b);
                Con_Printf(", line %.1f %.1f %.1f .. %.1f %.1f %.1f", p0.x, p0.y, p0.z, p1.x, p1.y, p1.z);
            }
            Con_Printf("\n");
        }
    }
    PR_SwitchQCVM(nullptr);
}

} // namespace

namespace qvr::blowline
{

void registerCommands()
{
    Cmd_AddCommand("vr_blowline_dump", dump_f);
}

void PF_blowline()
{
    const edict_t* e = G_EDICT(OFS_PARM0);
    const float tau = G_FLOAT(OFS_PARM1);
    const int which = static_cast<int>(G_FLOAT(OFS_PARM2));
    G_FLOAT(OFS_RETURN) = 0.f;
    const int modelIndex = static_cast<int>(e->v.modelindex);
    if(modelIndex <= 0 || modelIndex >= MAX_MODELS)
    {
        return;
    }
    ModelBlows* m = modelFor(sv.models[modelIndex]);
    if(!m)
    {
        return;
    }
    const aliashdr_t* hdr = headerOf(m->model);
    const int frame = static_cast<int>(e->v.frame);
    if(frame < 0 || frame >= hdr->numframes)
    {
        return;
    }
    const int pose = hdr->frames[frame].firstpose;
    if(pose < 0 || pose >= hdr->numposes)
    {
        return;
    }
    const int index = runFor(*m, frame, static_cast<int>(e->v.think));
    if(index < 0)
    {
        return;
    }
    const Run& run = m->runs[static_cast<za::SizeT>(index)];
    const PoseBlow& blow = m->blows[static_cast<za::SizeT>(pose)];
    G_FLOAT(OFS_RETURN) = static_cast<float>(blow.count);
    if(blow.count <= 0 || which < 0 || which >= blow.count)
    {
        return;
    }
    const Striker& st = run.striker[blow.striker[which]];

    // Where its drawing is: 0 as the frame starts (the last frame drawn), 1 as it ends (this one).
    const float share = za::clamp(1.f - (e->v.nextthink - static_cast<float>(qcvm->time)) / frameInterval, 0.f, 1.f);
    const float at = share + tau / frameInterval;
    const int whole = static_cast<int>(std::floor(at));
    const int pose1 = walkFrom(*m, run, pose, whole - 1);
    const int pose2 = walkFrom(*m, run, pose, whole);
    const float blend = at - static_cast<float>(whole);

    float scale = 1.f;
    if(const int ofs = progs::fields().scale; ofs >= 0)
    {
        const float s = progs::fieldFloat(const_cast<edict_t*>(e), ofs);
        scale = s > 0.f ? s : 1.f;
    }
    vec3_t angles{-e->v.angles[0], e->v.angles[1], e->v.angles[2]}; // (a model's pitch: as it's drawn)
    vec3_t f, r, u;
    AngleVectors(angles, f, r, u);
    const glm::vec3 origin{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
    const glm::vec3 fw{f[0], f[1], f[2]};
    const glm::vec3 rt{r[0], r[1], r[2]};
    const glm::vec3 up{u[0], u[1], u[2]};
    const auto world = [&](int vert) {
        const glm::vec3 l = mdldequant::modelVertex(hdr, pose1, pose2, blend, vert) * scale;
        return origin + fw * l.x - rt * l.y + up * l.z;
    };
    const progs::Bindings& b = progs::bindings();
    const auto put = [](float* g, const glm::vec3& v) {
        if(g)
        {
            g[0] = v.x;
            g[1] = v.y;
            g[2] = v.z;
        }
    };
    put(b.blowLine[0], world(st.a));
    put(b.blowLine[1], world(st.b));
    put(b.blowLine[2], glm::vec3{st.reach * scale, st.length * scale, blow.sweep * scale});
}

} // namespace qvr::blowline
