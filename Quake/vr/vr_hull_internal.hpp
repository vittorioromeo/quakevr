#pragma once

// vr_hull_internal.hpp -- what vr_hull.cpp (see vr_hull.hpp) shares with its diagnostics, vr_hull_diag.cpp.
// Internal: included by those two alone. What was the file's anonymous namespace and both use is namespace
// hull::detail: its types and variables here (each variable one object: inline), its functions declared here
// and defined in the file that owns them.

#include "vr_hull.hpp"
#include "vr_alloccount.hpp"
#include "vr_brushes.hpp"
#include "vr_diskcache.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_files.hpp"
#include "vr_jobs.hpp"
#include "vr_mem.hpp"
#include "vr_progs.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/StablePartition.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"
#include "vr_srvrandom.hpp"

#include <atomic>
#include <string.h>

namespace qvr::hull::detail
{

// A table's planes by planeKey(dist), each key's in the order they were added.
using PlaneIndex = ankerl::unordered_dense::map<long long, za::Vector<int>>;
inline constexpr float minWidth = 8.f, maxWidth = 32.f;

// A brush's face, outward: inside is dot(normal, p) < dist + grows * the box's reach along normal. A face of hull 0's
// brushes grows with the box (grows 1); a recovered clip brush's face (see recoverClips) is already grown by Quake's
// 32 box and shrinks back with a narrower one if it faces the open (grows 1, dist less Quake's box), else stays but for
// its height (growsZ -1, dist plus Quake's box's: a shorter box's floor under it rises toward it as much; recoverClips).
struct Plane
{
    glm::vec3 normal;
    float dist;
    float grows;
    float growsZ = 0.f; // ... plus growsZ * the box's reach along normal's z alone
};

struct Brush
{
    za::U32 first;
    za::U32 count;
    glm::vec3 mins, maxs; // hull 0's: its bounds; a clip brush's: the bounds of the box centres it holds (32 box)
    bool clip;
};

// A brush model's brushes: one of the world's submodels (its doors, lifts, func_walls), or an external .bsp's model
// (maps/b_*.bsp, vrfiringrange's panels and tables), whose hull 0 has its own nodes: its leaves' entries in leafBrush
// start at `base`.
struct SubModel
{
    const mclipnode_t* clipnodes; // its hull 0's
    int head;                     // its head node in them
    za::U32 base;           // its hull 0's leaves in leafBrush: [base + node * 2 + side]
    za::U32 firstBrush, numBrushes;
};

// The server map's solid space as brushes: pointers into the hunk (the world's hull 0), released at every map change.
struct Brushes
{
    const mclipnode_t* clipnodes = nullptr; // the hull 0 they were built from (the world's and its brush models')
    za::Vector<Plane> planes;
    za::Vector<Brush> brushes;
    za::Vector<int> leafBrush; // [base + node * 2 + side]: the brush of that child when it is a solid leaf, else -1
    za::Vector<SubModel> subs; // the world's submodels (0: the world), then external models as they are met
    za::Vector<int> modelSub;  // [modelindex]: its entry in subs; -1 none (not a brush model), -2 not looked up
    za::Vector<int> clips;     // the world's recovered clip brushes
    za::Vector<int> leafClipStart; // [node * 2 + side]: where the clip brushes whose box centres reach into that
    za::Vector<int> leafClipList;  // leaf of the world's hull 0 (empty or solid) start in leafClipList (and end: +1)
    mutable za::Vector<za::SizeT> clipStamp; // a clip brush's last test (one per sweep: it is in many leaves)
    mutable za::SizeT stamp = 0;
    za::Vector<int> hull1Clip; // [hull 1 node * 2 + side]: a solid leaf's clip brush, -1 not one, -2 not looked at
    double ms = 0.0;            // the build's time
    double clipMs = 0.0;        // of which the clip brushes'
    int bevels = 0;             // planes added as Quake 2's bevels (axial, edge)
    int dropped = 0;            // solid leaves with no volume left (slivers under the build's epsilon)
    int hull1Leaves = 0;        // the world's hull 1 solid leaves looked at for clip brushes
    int numnodes = 0;
    za::U64 key = 0;            // the world's content (keyOf): what they are kept under for the same map's next load
    // The world's own (build): external models' brushes, planes and leaves follow (subOf), let go when kept.
    za::SizeT worldSubs = 0, worldPlanes = 0, worldBrushes = 0, worldLeafBrush = 0;
    int worldBevels = 0, worldDropped = 0;
    auto members()
    {
        return qvr::mem::list(clipnodes, planes, brushes, leafBrush, subs, modelSub, clips, leafClipStart, leafClipList, clipStamp, stamp, hull1Clip, ms,
            clipMs, bevels, dropped, hull1Leaves, numnodes, key, worldSubs, worldPlanes, worldBrushes, worldLeafBrush, worldBevels,
            worldDropped);
    }
};

inline mem::Cache<Brushes> built{"hull brushes", mem::MapChange};

struct Result
{
    trace_t trace;
    int brushTests;
};

struct Tree
{
    za::Vector<mclipnode_t> nodes;
    za::Vector<mplane_t> planes;
    za::Vector<int> heads;                    // [sub]: its tree's root in nodes; -1 not built yet
    // Its planes' index (TreeBuilder), kept from one model's build to the next: the first `indexed` planes are in it. Made
    // again for each model's build it was most of a big map's allocations (ad_grendel: 5.2 million of 8.5, its 440 brush
    // models times two trees, each indexing all the tree's planes again).
    PlaneIndex index;
    za::SizeT indexed = 0;
    // What a load of the same map gets back (keepForReload): the tree as the load left it (its world's tree, and the
    // models prepared with the map: checkpoint), the models' builds after it let go.
    za::SizeT keptNodes = 0, keptPlanes = 0;
    int keptSolid = 0, keptEmpty = 0;
    za::Vector<int> keptHeads; // (empty: nothing to keep)
    const mclipnode_t* forClipnodes = nullptr; // the Brushes it was built from (their world's hull 0)
    glm::vec3 ext{0.f};                        // the half size of the box it was built for
    int redone = 0;                            // pieces of its builds on the pool done again on one thread (buildTree)
    double ms = 0.0;                           // the builds so far
    // (developer 1) the world's build on the pool: its brushes grown, its units built, merged; its units' count and the
    // slowest's build and pieces (the critical path: buildTree)
    double growMs = 0.0, unitsMs = 0.0, mergeMs = 0.0, slowestMs = 0.0;
    za::SizeT slowestPieces = 0, units = 0;
    int solidLeaves = 0, emptyLeaves = 0;
    bool fromDisk = false;                     // the world's tree read from the disk cache (vr_hull_cache)
    int cutBack = 0;                           // the world's compile (or file): its pieces cut back (reported by settle)
    auto members()
    {
        return qvr::mem::list(nodes, planes, heads, index, indexed, keptNodes, keptPlanes, keptSolid, keptEmpty, keptHeads,
            forClipnodes, ext, ms, solidLeaves, emptyLeaves, redone, fromDisk, cutBack, growMs, unitsMs, mergeMs, slowestMs,
            slowestPieces, units);
    }
};

inline mem::Cache<Tree> tree{"hull tree", mem::MapChange};

// Monsters' trees (vr_mhull): one per box size their widths ask for (a few: the widths come from the classes'
// settings); the player's size shares the player's tree.
struct MonsterTrees
{
    za::Vector<Tree> trees;
    auto members() { return qvr::mem::list(trees); }
};

inline mem::Cache<MonsterTrees> monsterTrees{"hull monster trees", mem::MapChange};

inline constexpr za::SizeT maxMonsterTrees = 128; // reserved before asynchronous builds: their Tree pointers remain stable

struct HullAudit
{
    za::U64 slots = 0, clears = 0, runtimeBuilds = 0;
};

inline HullAudit hullAudit;

struct KeptMap
{
    za::U64 key = 0;
    Brushes brushes;
    Tree player;                // (its heads empty: none)
    za::Vector<Tree> monsters;
};

struct KeptMaps
{
    za::Vector<KeptMap> maps; // the most recent last
    za::U64 hits = 0, misses = 0; // loads that got theirs back, loads that built them
    auto members() { return qvr::mem::list(maps, hits, misses); }
};

// Kept across game folder changes too (a map package mounted and let go): the key is the map's content, and nothing
// in them points anywhere while kept. Only counted (vr_memstats); vr_hull_keep bounds it.
inline mem::Cache<KeptMaps> kept{"hull kept maps", mem::Never};

struct MonsterClass
{
    const char* classname;
    const char* name;
    cvar_t* width;
};

inline const MonsterClass monsterClasses[] = {
    {"monster_army", "Grunt", &vr_mhull_army},
    {"monster_dog", "Rottweiler", &vr_mhull_dog},
    {"monster_ogre", "Ogre", &vr_mhull_ogre},
    {"monster_knight", "Knight", &vr_mhull_knight},
    {"monster_hell_knight", "Death Knight", &vr_mhull_hknight},
    {"monster_ranged_knight", "Ranged Knight", &vr_mhull_hknight}, // (Dawn of the Machine's: the death knight's width)
    {"monster_zombie", "Zombie", &vr_mhull_zombie},
    {"monster_wizard", "Scrag", &vr_mhull_wizard},
    {"monster_demon1", "Fiend", &vr_mhull_demon},
    {"monster_shambler", "Shambler", &vr_mhull_shambler},
    {"monster_super_shambler", "Super Shambler", &vr_mhull_shambler}, // (Dawn of the Machine's: the shambler's width)
    {"monster_shalrath", "Vore", &vr_mhull_shalrath},
    {"monster_enforcer", "Enforcer", &vr_mhull_enforcer},
    {"monster_fish", "Rotfish", &vr_mhull_fish},
    {"monster_tarbaby", "Spawn", &vr_mhull_tarbaby},
};

inline constexpr int monsterClassCount = static_cast<int>(sizeof(monsterClasses) / sizeof(monsterClasses[0]));

// The map load's builds on the pool (beforeLoad, spawned): the map as brushes, then the player's and the monsters'
// trees, made while the server spawns the map; settle() waits for them (every use of the brushes or the trees first).
// While they run, the main thread touches neither `built` nor the trees being made.
struct Pending
{
    jobs::Future<void> brushes;         // build(): the map as brushes
    za::Vector<Tree*> trees;           // being compiled, each by its job below (after the brushes)
    za::Vector<jobs::Future<void>> run;
    glm::vec3 playerExt{0.f};           // the player's tree's box, if it is one of them
    double posted = 0.0;                // when the builds were handed out (the load's report)
    bool kept = false;                  // the brushes (and the trees found) the last load's of the same map
};

inline Pending pending;

// The disk cache's counts this session (vr_hull_stats), added to by the trees' jobs.
struct DiskCounts
{
    std::atomic<int> read{0}, written{0}, missed{0}, rejected{0}, same{0}, differed{0};
    std::atomic<long long> readUs{0};
};

inline DiskCounts diskCounts;

inline za::SizeT heldBytes(const Brushes& b)
{
    return mem::heldBytes(b.planes) + mem::heldBytes(b.brushes) + mem::heldBytes(b.leafBrush) + mem::heldBytes(b.subs) +
           mem::heldBytes(b.modelSub) + mem::heldBytes(b.clips) + mem::heldBytes(b.leafClipStart) +
           mem::heldBytes(b.leafClipList) + mem::heldBytes(b.clipStamp) + mem::heldBytes(b.hull1Clip);
}

inline za::SizeT heldBytes(const Tree& t)
{
    return mem::heldBytes(t.nodes) + mem::heldBytes(t.planes) + mem::heldBytes(t.heads) + mem::heldBytes(t.index) +
           mem::heldBytes(t.keptHeads);
}

inline za::SizeT heldBytes(const KeptMap& k)
{
    return heldBytes(k.brushes) + heldBytes(k.player) + mem::heldBytes(k.monsters);
}

float widthSetting();
void settle();
const Brushes* worldBrushes(qmodel_t* world);
bool wanted();
void quakeHull(float size, float& width, float& height);
int monsterClassOf(const edict_t* ent);
za::U64 keyOf(const qmodel_t* world);
za::SizeT keepLimit();
float hitWidthSetting();
void collidingSubs(za::Vector<za::SizeT>& subs);
float classWidth(int cls, float size, float quake);
int buildTree(Tree& t, const Brushes& b, za::SizeT sub, const glm::dvec3* watch = nullptr, bool report = true);
bool boxInSolid(const Brushes& b, const hull_t& hull, int head, const glm::dvec3& p, const glm::dvec3& ext,
    bool clips = true, za::SizeT base = 0);
const Tree& treeFor(const Brushes& b, za::SizeT sub, const glm::vec3& ext, const char* target = nullptr);
trace_t treeTrace(const Tree& t, int head, const glm::vec3& start, const glm::vec3& end);
void build(Brushes& b, qmodel_t* world, za::U64 key);
void cutBack(Brushes& b);
void cutBack(Tree& t);
Result boxTrace(const Brushes& b, const hull_t& hull0, int head, const glm::vec3& start, const glm::vec3& mins,
    const glm::vec3& maxs, const glm::vec3& end, za::SizeT base = 0, const float* halfspace = nullptr);
Tree* findSlot(const mclipnode_t* clipnodes, const glm::vec3& ext);
bool boxInBrush(const Brushes& b, const Brush& br, const glm::dvec3& p, const glm::dvec3& ext);
double support(const Plane& p, const glm::dvec3& ext);
int clientNum(const edict_t* ent);
float entWidthSetting();

} // namespace qvr::hull::detail
