// vr_walltorch.cpp -- wall torches taken off their walls: the engine's side (see vr_walltorch.hpp; QC vr_walltorch.qc;
// docs/vr-port/ROUND21.md, "Wall torches you can take").

#include "vr_walltorch.hpp"

#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/String/String.hpp"

#include <string.h>

using namespace qvr;

namespace
{

constexpr const char* stickModelName = "progs/vrtorch.mdl";    // a taken torch (make_walltorch.py)
constexpr const char* wallModelName = "progs/flame.mdl";       // a torch on its wall (id's)
constexpr const char* fireModelName = "progs/vrtorch_fire.mdl"; // id's torch's flame alone, made from its file
constexpr const char* crackleName = "ambience/fire1.wav";      // the torches' crackle (misc.qc FireAmbient; QC plays it
constexpr float crackleVolume = 0.5f;                          // on a taken torch at this volume: vr_walltorch.qc)
constexpr int fireLevels = 16;                                  // the stick's frames 1..16: its fire in sixteenths
constexpr int wallFrame = 17;                                   // the stick's frame on its wall (a full fire)

// The stick's skin is laid out as the wall torch's Quake VR loads (quakevr/progs/flame.mdl, make_walltorch.py): its skin
// is copied in as the stick loads, but for our corner (the pit's embers, empty in it), and charred down to this row for
// the burnt-out skin (the head). Only a flame.mdl of that torch's shape (its vertices and triangles) gives its skin.
constexpr int ownS0 = 129, ownT0 = 2, ownS1 = 149, ownT1 = 30;
constexpr int charredTo = 72;
constexpr int wallVerts = 134, wallTris = 132;

// The stick (make_walltorch.py): along its +x, its pit (where the fire sits) at x 0.9..1.8.
constexpr glm::vec3 stickHead{1.2f, 0.f, 0.f};
// Id's torch (flame.mdl, upright): its flame rises from z 1.28 (the cup), and its light is at z 20 (vr_emissive.cpp).
constexpr float fireBase = 1.28f;
constexpr float fireLight = 20.f;

// The flame leans away from the way the head moves: this much (tangent) per unit/s, at most 40 degrees, the head's
// speed eased over 0.08 s.
constexpr float leanPerSpeed = 1.f / 500.f;
constexpr float leanMost = 0.84f;
constexpr float leanEase = 0.08f;

struct Taken
{
    glm::vec3 wall{0.f};  // where it hung (its crackle is there): last seen on its wall, else its baseline
    bool wallKnown = false;
    glm::vec3 head{0.f};  // the stick's head, last frame
    glm::vec3 vel{0.f};   // its velocity, eased
    double time = -1.0;   // when the head was last placed (-1: not lit)
    glm::vec3 fire{0.f};  // its light's place (the flame's middle)
    float level = 0.f;    // its fire, 0..1
    unsigned stamp = 0;   // the frame it was last seen taken
};

ankerl::unordered_dense::map<int, Taken> taken;
const qmodel_t* torchWorld = nullptr;
int torchGeneration = -1;
unsigned frameStamp = 0;
const qmodel_t* stickModel = nullptr;
const qmodel_t* wallModel = nullptr;
qmodel_t* fireModel = nullptr;
bool fireTried = false;

void forget()
{
    taken.clear();
    stickModel = nullptr;
    wallModel = nullptr;
    fireModel = nullptr;
    fireTried = false;
    torchWorld = nullptr;
    torchGeneration = -1;
}

// The models by name among the client's precached ones (once a map).
void findModels()
{
    stickModel = nullptr;
    wallModel = nullptr;
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        const qmodel_t* m = cl.model_precache[i];
        if(!strcmp(m->name, stickModelName))
        {
            stickModel = m;
        }
        else if(!strcmp(m->name, wallModelName))
        {
            wallModel = m;
        }
    }
}

[[nodiscard]] qmodel_t* fire()
{
    if(!fireTried)
    {
        fireTried = true;
        fireModel = Mod_ForName(fireModelName, false);
        if(!fireModel)
        {
            Con_DPrintf("wall torch: no flame to draw (%s)\n", fireModelName);
        }
    }
    return fireModel;
}

// Its crackle: the wall's static one silenced once the torch has left it; the torch's own follows it, as loud as its
// fire (its origin is where it started, else).
void crackles()
{
    for(int i = NUM_AMBIENTS; i < total_channels; i++)
    {
        channel_t& ch = snd_channels[i];
        if(!ch.sfx || strcmp(ch.sfx->name, crackleName) != 0)
        {
            continue;
        }
        if(i >= NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS)
        {
            for(const auto& [ent, t] : taken)
            {
                if(t.stamp == frameStamp && t.wallKnown && glm::distance(glm::vec3{ch.origin[0], ch.origin[1], ch.origin[2]}, t.wall) < 2.f)
                {
                    Con_DPrintf("wall torch: its wall's crackle at %.0f %.0f %.0f silenced\n", t.wall.x, t.wall.y, t.wall.z);
                    ch.sfx = nullptr; // (static sounds are made again at the next sign-on: a load, a new map)
                    break;
                }
            }
            continue;
        }
        const auto it = taken.find(ch.entnum);
        if(it == taken.end() || it->second.stamp != frameStamp)
        {
            continue;
        }
        const entity_t& e = cl_entities[ch.entnum];
        VectorCopy(e.origin, ch.origin);
        ch.master_vol = static_cast<int>(255.f * crackleVolume * za::max(0.2f, it->second.level));
    }
}

} // namespace

namespace
{

// A palette index this much darker (0..1), charred (a little browner than grey), of the ordinary colours (not the
// fullbright ones).
[[nodiscard]] byte charred(byte c, float k)
{
    const auto rgb = [](int i) {
        const unsigned v = d_8to24table[i & 255];
        return glm::vec3{static_cast<float>(v & 255u), static_cast<float>((v >> 8) & 255u), static_cast<float>((v >> 16) & 255u)};
    };
    const glm::vec3 want = rgb(c) * k * glm::vec3{0.9f, 0.8f, 0.7f};
    int best = 0;
    float bestD = 1e9f;
    for(int i = 0; i < 224; i++)
    {
        const glm::vec3 e = rgb(i) - want;
        const float dr = e.x, dg = e.y, db = e.z;
        const float d = dr * dr + dg * dg + db * db;
        if(d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    return static_cast<byte>(best);
}

// Our stick with the wall torch's skin: its skins' pixels from the game's progs/flame.mdl (the same layout), our corner
// kept; the burnt-out skin's head charred. The stick as shipped (its own paint) if that torch isn't the one it was laid
// out for.
[[nodiscard]] byte* stickWithWallSkin(unsigned int* path_id)
{
    byte* own = COM_LoadMallocFile(stickModelName, path_id);
    if(!own)
    {
        return nullptr;
    }
    const size_t ownSize = static_cast<size_t>(com_filesize);
    unsigned int idPath = 0;
    byte* id = COM_LoadMallocFile(wallModelName, &idPath);
    const size_t idSize = id ? static_cast<size_t>(com_filesize) : 0;
    const auto rd = [](const byte* b, size_t at) -> int {
        int v;
        memcpy(&v, b + at, 4);
        return LittleLong(v);
    };
    constexpr size_t headerSize = 84;
    const auto keep = [&](const char* why) -> byte* {
        Con_DPrintf("wall torch: %s as shipped (%s)\n", stickModelName, why);
        free(id);
        return own;
    };
    if(!id || idSize < headerSize + 4 || ownSize < headerSize || rd(id, 0) != IDPOLYHEADER || rd(own, 0) != IDPOLYHEADER)
    {
        return keep("no wall torch to take its skin from");
    }
    const int w = rd(own, 52), h = rd(own, 56), skins = rd(own, 48);
    const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h);
    if(rd(id, 52) != w || rd(id, 56) != h || rd(id, 60) != wallVerts || rd(id, 64) != wallTris || rd(id, 48) < 1 ||
        rd(id, 84) != 0 || idSize < headerSize + 4 + bytes ||
        ownSize < headerSize + static_cast<size_t>(skins) * (4 + bytes))
    {
        return keep("another wall torch model, its skin laid out otherwise");
    }
    const byte* idSkin = id + headerSize + 4;
    for(int k = 0; k < skins; k++)
    {
        const size_t at = headerSize + static_cast<size_t>(k) * (4 + bytes);
        if(rd(own, at) != 0)
        {
            return keep("a skin group");
        }
        byte* skin = own + at + 4;
        for(int t = 0; t < h; t++)
        {
            for(int s = 0; s < w; s++)
            {
                if(s >= ownS0 && s < ownS1 && t >= ownT0 && t < ownT1)
                {
                    continue;
                }
                byte c = idSkin[t * w + s];
                if(k == 1 && t < charredTo)
                {
                    // Burnt out: the head charred, darkest at its top.
                    c = charred(c, 0.22f + 0.33f * static_cast<float>(t) / static_cast<float>(charredTo));
                }
                skin[t * w + s] = c;
            }
        }
    }
    free(id);
    Con_DPrintf("wall torch: %s: the wall torch's skin copied in (%d x %d)\n", stickModelName, w, h);
    return own;
}

} // namespace

// Mod_LoadModel: the file of a model made from another's, or NULL. progs/vrtorch.mdl: ours with the wall torch's skin
// (stickWithWallSkin). progs/vrtorch_fire.mdl: id's wall torch
// (progs/flame.mdl) without its stick: the triangles whose corners all move in its animation (its flame; the stick
// stands still). Nothing of id's is written anywhere: it is made as the model loads, from the game's own file.
extern "C" byte* VR_DerivedModelFile(const char* name, unsigned int* path_id)
{
    if(!strcmp(name, stickModelName))
    {
        return stickWithWallSkin(path_id);
    }
    if(strcmp(name, fireModelName) != 0)
    {
        return nullptr;
    }
    byte* src = COM_LoadMallocFile(wallModelName, path_id);
    if(!src)
    {
        return nullptr;
    }
    const size_t size = static_cast<size_t>(com_filesize);
    const auto rd = [&](size_t at) -> int {
        int v;
        memcpy(&v, src + at, 4);
        return LittleLong(v);
    };
    constexpr size_t headerSize = 84;
    const auto fail = [&](const char* why) -> byte* {
        Con_DPrintf("wall torch: %s: %s\n", wallModelName, why);
        free(src);
        return nullptr;
    };
    if(size < headerSize || rd(0) != IDPOLYHEADER)
    {
        return fail("not an alias model");
    }
    const int numSkins = rd(48), skinW = rd(52), skinH = rd(56), numVerts = rd(60), numTris = rd(64), numFrames = rd(68);
    if(numSkins < 0 || skinW <= 0 || skinH <= 0 || numVerts <= 0 || numTris <= 0 || numFrames <= 0)
    {
        return fail("bad header");
    }
    size_t at = headerSize;
    const size_t skinBytes = static_cast<size_t>(skinW) * static_cast<size_t>(skinH);
    for(int s = 0; s < numSkins; s++)
    {
        if(at + 4 > size)
        {
            return fail("short");
        }
        if(rd(at) == 0)
        {
            at += 4 + skinBytes;
        }
        else
        {
            const int n = at + 8 <= size ? rd(at + 4) : 0;
            at += 8 + static_cast<size_t>(za::max(n, 0)) * (4 + skinBytes);
        }
    }
    const size_t trisAt = at + static_cast<size_t>(numVerts) * 12;
    const size_t framesAt = trisAt + static_cast<size_t>(numTris) * 16;
    if(framesAt + 4 > size)
    {
        return fail("short");
    }
    // Which vertices move: the first frame's poses against each other (a group), or the first two frames.
    const size_t vertsBytes = static_cast<size_t>(numVerts) * 4;
    za::Vector<const byte*> poses;
    size_t f = framesAt;
    for(int k = 0; k < numFrames && poses.size() < 2; k++)
    {
        if(f + 4 > size)
        {
            break;
        }
        if(rd(f) == 0)
        {
            poses.pushBack(src + f + 4 + 24);
            f += 4 + 24 + vertsBytes;
        }
        else
        {
            const int n = f + 8 <= size ? rd(f + 4) : 0;
            const size_t first = f + 4 + 12 + static_cast<size_t>(n) * 4;
            for(int j = 0; j < n; j++)
            {
                poses.pushBack(src + first + static_cast<size_t>(j) * (24 + vertsBytes) + 24);
            }
            f = first + static_cast<size_t>(n) * (24 + vertsBytes);
        }
    }
    if(poses.size() < 2 || poses.back() + vertsBytes > src + size)
    {
        return fail("no animation to tell its flame by");
    }
    za::Vector<bool> moving(static_cast<size_t>(numVerts), false);
    for(size_t p = 1; p < poses.size(); p++)
    {
        for(int v = 0; v < numVerts; v++)
        {
            moving[static_cast<size_t>(v)] = moving[static_cast<size_t>(v)] || memcmp(poses[0] + v * 4, poses[p] + v * 4, 3) != 0;
        }
    }
    za::Vector<byte> tris;
    int kept = 0;
    for(int t = 0; t < numTris; t++)
    {
        const size_t o = trisAt + static_cast<size_t>(t) * 16;
        bool all = true;
        for(int c = 0; c < 3; c++)
        {
            const int v = rd(o + 4 + static_cast<size_t>(c) * 4);
            all = all && v >= 0 && v < numVerts && moving[static_cast<size_t>(v)];
        }
        if(all)
        {
            tris.emplaceBackRange(src + o, 16);
            kept++;
        }
    }
    if(kept == 0)
    {
        return fail("no triangle of its flame");
    }
    const size_t outSize = size - static_cast<size_t>(numTris - kept) * 16;
    byte* out = static_cast<byte*>(malloc(outSize + 1));
    memcpy(out, src, trisAt);
    const int keptLe = LittleLong(kept);
    memcpy(out + 64, &keptLe, 4);
    memcpy(out + trisAt, tris.data(), tris.size());
    memcpy(out + trisAt + tris.size(), src + framesAt, size - framesAt);
    out[outSize] = 0;
    free(src);
    Con_DPrintf("wall torch: %s: %d of %d triangles (its flame)\n", fireModelName, kept, numTris);
    return out;
}

// Mod_LoadAllSkins: the name a model's external skins are found by (progs/ogre.mdl_0.tga): the flame's are id's torch's.
extern "C" const char* VR_ModelSkinName(const char* name)
{
    return strcmp(name, fireModelName) == 0 ? wallModelName : name;
}

// Each client frame, after the temp entities (CL_ReadFromServer, before VR_TorchLights): the taken torches' flames,
// their crackles.
extern "C" void VR_WallTorchFlames(void)
{
    QVR_PROFILE("wall torches");
    if(cls.state != ca_connected || !cl.worldmodel)
    {
        return;
    }
    if(cl.worldmodel != torchWorld || worldGeneration() != torchGeneration)
    {
        taken.clear();
        torchWorld = cl.worldmodel;
        torchGeneration = worldGeneration();
        findModels();
    }
    if(!stickModel)
    {
        return; // (no torch can be taken on this map)
    }
    frameStamp++;
    bool any = false;
    const float size = za::max(0.f, vr_walltorch_flame.value);
    for(int i = 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(!e.model || e.msgtime != cl.mtime[0])
        {
            continue;
        }
        const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
        if(e.model == wallModel)
        {
            Taken& t = taken[i]; // (every torch on its wall: where it hangs)
            t.wall = origin;
            t.wallKnown = true;
            t.time = -1.0;
            continue;
        }
        if(e.model != stickModel)
        {
            continue;
        }
        Taken& t = taken[i];
        const bool hung = e.frame == wallFrame;
        if(hung)
        {
            // On its wall (drawn as our stick): where it hangs; its flame drawn, still (its light: vr_emissive.cpp,
            // walltorch::onWall).
            t.wall = origin;
            t.wallKnown = true;
            t.time = -1.0;
            t.vel = glm::vec3{0.f};
        }
        else
        {
            any = true;
            t.stamp = frameStamp;
            if(!t.wallKnown && e.baseline.modelindex > 0 && e.baseline.modelindex < MAX_MODELS &&
                (cl.model_precache[e.baseline.modelindex] == wallModel ||
                    (cl.model_precache[e.baseline.modelindex] == stickModel && e.baseline.frame == wallFrame)))
            {
                t.wall = glm::vec3{e.baseline.origin[0], e.baseline.origin[1], e.baseline.origin[2]};
                t.wallKnown = true;
            }
        }
        const int frame = hung ? fireLevels : za::clamp(static_cast<int>(e.frame), 0, fireLevels);
        t.level = static_cast<float>(frame) / static_cast<float>(fireLevels);
        if(frame == 0)
        {
            t.time = -1.0;
            continue;
        }

        const glm::mat3 axes = held::axesFromAngles(e.angles, false);
        const glm::vec3 head = origin + axes * stickHead;
        const double now = cl.time;
        if(t.time >= 0.0 && now > t.time)
        {
            const float dt = static_cast<float>(za::min(now - t.time, 0.1));
            glm::vec3 v = (head - t.head) / dt;
            if(glm::length(v) > 3000.f)
            {
                v = glm::vec3{0.f}; // a jump (taken into a hand, a teleport)
            }
            t.vel += (v - t.vel) * (1.f - za::exp(-dt / leanEase));
        }
        else if(t.time < 0.0)
        {
            t.vel = glm::vec3{0.f};
        }
        t.head = head;
        t.time = hung ? -1.0 : now;

        // Upright, leaning away from its motion; from past the head if the stick points down (not up through it).
        glm::vec3 lean = -t.vel * leanPerSpeed;
        lean.z = 0.f;
        if(const float l = glm::length(lean); l > leanMost)
        {
            lean *= leanMost / l;
        }
        const glm::vec3 up = glm::normalize(glm::vec3{0.f, 0.f, 1.f} + lean);
        const glm::vec3 along = axes[0];
        const glm::vec3 base = head + along * (za::max(0.f, -along.z) * 1.5f);
        const float s = hung ? 1.f : size * za::pow(t.level, 0.6f); // (it shrinks slower than its light dims at first; on its wall, id's)
        t.fire = base + glm::vec3{0.f, 0.f, (fireLight - fireBase) * s};

        qmodel_t* model = s > 0.03f ? fire() : nullptr;
        if(!model)
        {
            continue;
        }
        entity_t* ent = CL_NewTempEntity();
        if(!ent)
        {
            continue;
        }
        glm::vec3 x = along - up * glm::dot(along, up);
        if(glm::length(x) < 0.1f)
        {
            x = glm::cross(glm::vec3{0.f, 1.f, 0.f}, up);
        }
        x = glm::normalize(x);
        const glm::mat3 m{x, glm::cross(up, x), up};
        const glm::vec3 o = base - up * (fireBase * s);
        ent->origin[0] = o.x;
        ent->origin[1] = o.y;
        ent->origin[2] = o.z;
        held::anglesFromAxes(m, ent->angles, false);
        ent->model = model;
        ent->scale = ENTSCALE_ENCODE(s);
        ent->syncbase = za::fmod(static_cast<float>(i) * 0.618f, 1.f); // (the torches' flames not in step)
    }
    if(any)
    {
        crackles();
    }
}

bool walltorch::fire(int ent, glm::vec3& at, float& level)
{
    const auto it = taken.find(ent);
    if(it == taken.end() || it->second.stamp != frameStamp || it->second.time < 0.0 || it->second.level <= 0.f)
    {
        return false;
    }
    at = it->second.fire;
    level = it->second.level;
    return true;
}

bool walltorch::onWall(const entity_t& e)
{
    return e.model && e.model == stickModel && e.frame == wallFrame;
}

void walltorch::onGameDirChanged()
{
    forget();
}

void walltorch::restoreAfterLoad()
{
    if(vr_walltorch.value == 0.f || !sv.worldmodel || !sv.worldmodel->entities)
    {
        return;
    }
    const func_t spawn = progs::findFunction("light_torch_small_walltorch");
    if(!spawn)
    {
        return;
    }
    // The torches the save has: where they hung.
    za::Vector<glm::vec3> have;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || strcmp(PR_GetString(e->v.classname), "light_torch_small_walltorch") != 0)
        {
            continue;
        }
        const eval_t* home = GetEdictFieldValueByName(e, "wt_home");
        have.pushBack(home ? glm::vec3{home->vector[0], home->vector[1], home->vector[2]}
                            : glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]});
    }
    // The map's (its entities as they were spawned, the skill's and deathmatch's taken out as ED_LoadFromFile does).
    int restored = 0;
    const char* data = sv.worldmodel->entities;
    while((data = COM_Parse(data)) != nullptr && com_token[0] == '{')
    {
        const char* block = data;
        za::String classname;
        glm::vec3 origin{0.f};
        int spawnflags = 0;
        while((data = COM_Parse(data)) != nullptr && com_token[0] != '}')
        {
            const za::String key = com_token;
            data = COM_Parse(data);
            if(!data)
            {
                break;
            }
            if(key == "classname")
            {
                classname = com_token;
            }
            else if(key == "origin")
            {
                sscanf(com_token, "%f %f %f", &origin.x, &origin.y, &origin.z);
            }
            else if(key == "spawnflags")
            {
                spawnflags = atoi(com_token);
            }
        }
        if(!data)
        {
            break;
        }
        if(classname != "light_torch_small_walltorch")
        {
            continue;
        }
        if(deathmatch.value ? (spawnflags & SPAWNFLAG_NOT_DEATHMATCH)
                            : (current_skill == 0 && (spawnflags & SPAWNFLAG_NOT_EASY)) ||
                                  (current_skill == 1 && (spawnflags & SPAWNFLAG_NOT_MEDIUM)) ||
                                  (current_skill >= 2 && (spawnflags & SPAWNFLAG_NOT_HARD)))
        {
            continue;
        }
        const bool found = za::anyOf(have.begin(), have.end(), [&](const glm::vec3& h) { return glm::distance(h, origin) < 0.5f; });
        if(found)
        {
            continue;
        }
        edict_t* ent = ED_Alloc();
        ED_ParseEdict(block, ent);
        pr_global_struct->self = EDICT_TO_PROG(ent);
        pr_global_struct->time = qcvm->time;
        PR_ExecuteProgram(spawn);
        restored++;
    }
    if(restored > 0)
    {
        Con_DPrintf("wall torch: %d put back on their walls (a save without them)\n", restored);
    }
}
