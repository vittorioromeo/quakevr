// vr_arenadirector.cpp -- see vr_arenadirector.hpp (docs/vr-port/ARENA_DIRECTOR.md). The planner is the one
// Misc/quakevr/arena_director_sim.py simulates; keep the two in step.

#include "vr_arenadirector.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::arenadirector
{
namespace
{

constexpr const char* dataPath = "data/arena_director.txt";
constexpr int gunRows = 34;    // (QC AR_GUN_N: the weapon ladder's rows)
constexpr int maxPlan = 40;    // (QC AR_PLAN_MAX)
constexpr int maxBlocks = 192; // the data file's blocks at most
constexpr int maxKeys = 20;    // a block's keys at most
constexpr int textLen = 96;
constexpr int maxItems = 64;   // a kind's blocks at most (bodies, weapons, ...)
constexpr int maxWaves = 400;  // waves the run remembers its themes for

// QC's VR_WAVE_KINDS rows, by their names (the data file's `kind`).
constexpr const char* const kindNames[] = {"grunt", "enforcer", "zombie", "dog", "knight", "ogre", "demon",
                                           "hellknight", "shambler", "shalrath", "wizard", "armed_knight",
                                           "armed_deathknight", "plain_grunt", "plain_enforcer"};
constexpr int kindCount = static_cast<int>(sizeof(kindNames) / sizeof(kindNames[0]));

// ---------------------------------------------------------------------------------------------------------------
// The data file: `<kind> <name> { key "value" ... }`
// ---------------------------------------------------------------------------------------------------------------
struct Block
{
    char kind[24]{};
    char name[40]{};
    int n{0};
    char keys[maxKeys][24]{};
    char vals[maxKeys][textLen]{};

    [[nodiscard]] const char* str(const char* k, const char* d = "") const
    {
        for(int i = 0; i < n; i++)
        {
            if(!strcmp(keys[i], k))
            {
                return vals[i];
            }
        }
        return d;
    }
    [[nodiscard]] float num(const char* k, float d = 0.f) const
    {
        const char* s = str(k, nullptr);
        return s ? static_cast<float>(atof(s)) : d;
    }
    [[nodiscard]] bool has(const char* k) const { return str(k, nullptr) != nullptr; }
    // A word of a space-separated value (tags "humanoid melee").
    [[nodiscard]] bool word(const char* k, const char* w) const
    {
        const char* s = str(k);
        const size_t len = strlen(w);
        while(*s)
        {
            while(*s == ' ')
            {
                s++;
            }
            const char* e = s;
            while(*e && *e != ' ')
            {
                e++;
            }
            if(static_cast<size_t>(e - s) == len && !strncmp(s, w, len))
            {
                return true;
            }
            s = e;
        }
        return false;
    }
};

struct Data
{
    bool loaded{false};
    int n{0};
    Block blocks[maxBlocks];
    // Each kind's blocks (indices into blocks).
    int bodies[maxItems]{}, nBodies{0};
    int weapons[maxItems]{}, nWeapons{0};
    int bosses[maxItems]{}, nBosses{0};
    int themes[maxItems]{}, nThemes{0};
    int supplies[maxItems]{}, nSupplies{0};
    const Block* threat{nullptr};
    const Block* roles{nullptr};
    const Block* novelty{nullptr};
    const Block* progress{nullptr};
    const Block* supply{nullptr};
    const Block* cadence{nullptr};
    const Block* green{nullptr};
    const Block* yellow{nullptr};
    const Block* red{nullptr};
    const Block* gun2{nullptr};
    const Block* infected{nullptr};
    const Block* bossMark{nullptr};
    // Parsed lists.
    int splitN{0};
    int splitWave[16]{};
    float split[16][4]{};
    int sawN{0};
    float saw[8]{};
    int recentN{0};
    float recent[8]{};
};
Data data;

const Block empty{};

[[nodiscard]] const Block& blk(int i) { return i >= 0 ? data.blocks[i] : empty; }
[[nodiscard]] const Block& body(int i) { return blk(data.bodies[i]); }
[[nodiscard]] const Block& weapon(int i) { return blk(data.weapons[i]); }

[[nodiscard]] const Block* find(const char* kind, const char* name)
{
    for(int i = 0; i < data.n; i++)
    {
        if(!strcmp(data.blocks[i].kind, kind) && !strcmp(data.blocks[i].name, name))
        {
            return &data.blocks[i];
        }
    }
    return nullptr;
}

int floats(const char* s, float* out, int max)
{
    int n = 0;
    while(*s && n < max)
    {
        char* e = nullptr;
        const float v = strtof(s, &e);
        if(e == s)
        {
            s++;
            continue;
        }
        out[n++] = v;
        s = e;
    }
    return n;
}

bool load()
{
    data = Data{};
    byte* file = COM_LoadMallocFile(dataPath, nullptr);
    if(!file)
    {
        Con_Printf("\x02arena director: no %s\n", dataPath);
        return false;
    }
    const char* p = reinterpret_cast<const char*>(file);
    while((p = COM_Parse(p)) != nullptr)
    {
        if(data.n >= maxBlocks)
        {
            Con_Printf("\x02%s: more than %d blocks\n", dataPath, maxBlocks);
            break;
        }
        Block& b = data.blocks[data.n];
        b = Block{};
        q_strlcpy(b.kind, com_token, sizeof(b.kind));
        if(!(p = COM_Parse(p)))
        {
            break;
        }
        q_strlcpy(b.name, com_token, sizeof(b.name));
        if(!(p = COM_Parse(p)) || strcmp(com_token, "{") != 0)
        {
            Con_Printf("\x02%s: \"%s %s\": no {\n", dataPath, b.kind, b.name);
            break;
        }
        while((p = COM_Parse(p)) != nullptr && strcmp(com_token, "}") != 0)
        {
            char key[24];
            q_strlcpy(key, com_token, sizeof(key));
            if(!(p = COM_Parse(p)))
            {
                break;
            }
            if(b.n < maxKeys)
            {
                q_strlcpy(b.keys[b.n], key, sizeof(b.keys[0]));
                q_strlcpy(b.vals[b.n], com_token, sizeof(b.vals[0]));
                b.n++;
            }
            else
            {
                Con_Printf("\x02%s: \"%s %s\": more than %d keys\n", dataPath, b.kind, b.name, maxKeys);
            }
        }
        const int idx = data.n++;
        auto add = [&](int* list, int& count)
        {
            if(count < maxItems)
            {
                list[count++] = idx;
            }
        };
        if(!strcmp(b.kind, "body")) { add(data.bodies, data.nBodies); }
        else if(!strcmp(b.kind, "weapon")) { add(data.weapons, data.nWeapons); }
        else if(!strcmp(b.kind, "boss")) { add(data.bosses, data.nBosses); }
        else if(!strcmp(b.kind, "theme")) { add(data.themes, data.nThemes); }
        else if(!strcmp(b.kind, "supply")) { add(data.supplies, data.nSupplies); }
        else if(strcmp(b.kind, "curve") != 0 && strcmp(b.kind, "extra") != 0)
        {
            Con_Printf("\x02%s: \"%s %s\": unknown block\n", dataPath, b.kind, b.name);
        }
    }
    free(file);

    data.threat = find("curve", "threat");
    data.roles = find("curve", "roles");
    data.novelty = find("curve", "novelty");
    data.progress = find("curve", "progress");
    data.supply = find("curve", "supply");
    data.cadence = find("curve", "cadence");
    data.green = find("extra", "armour_green");
    data.yellow = find("extra", "armour_yellow");
    data.red = find("extra", "armour_red");
    data.gun2 = find("extra", "second_weapon");
    data.infected = find("extra", "infected");
    data.bossMark = find("extra", "boss");
    if(!data.threat || !data.roles || !data.novelty || !data.progress || !data.supply || !data.cadence || !data.green ||
       !data.yellow || !data.red || !data.gun2 || !data.infected || !data.bossMark || !data.nBodies || !data.nWeapons ||
       !data.nThemes)
    {
        Con_Printf("\x02%s: a curve, an extra, or every body, weapon or theme is missing\n", dataPath);
        return false;
    }
    // roles split: "w:f,m,h,x w:..."
    const char* s = data.roles->str("split");
    while(*s && data.splitN < 16)
    {
        char* e = nullptr;
        const long w = strtol(s, &e, 10);
        if(e == s || *e != ':')
        {
            s++;
            continue;
        }
        s = e + 1;
        float v[4]{};
        for(int i = 0; i < 4; i++)
        {
            v[i] = strtof(s, &e);
            s = (*e == ',') ? e + 1 : e;
        }
        data.splitWave[data.splitN] = static_cast<int>(w);
        memcpy(data.split[data.splitN], v, sizeof(v));
        data.splitN++;
    }
    data.sawN = floats(data.threat->str("saw", "1"), data.saw, 8);
    data.recentN = floats(data.novelty->str("recent"), data.recent, 8);
    data.loaded = data.splitN > 0 && data.sawN > 0;
    Con_DPrintf("arena director: %s, %d bodies, %d weapons, %d bosses, %d themes, %d supplies\n", dataPath, data.nBodies,
                data.nWeapons, data.nBosses, data.nThemes, data.nSupplies);
    return data.loaded;
}

// ---------------------------------------------------------------------------------------------------------------
// The run
// ---------------------------------------------------------------------------------------------------------------
struct Rng
{
    uint32_t s{1};

    [[nodiscard]] float next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return static_cast<float>(s >> 8) * (1.f / 16777216.f);
    }
    [[nodiscard]] int below(int n) { return n <= 1 ? 0 : za::min(n - 1, static_cast<int>(next() * static_cast<float>(n))); }
};

[[nodiscard]] uint32_t mix(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h ? h : 1u;
}

struct Foe
{
    int body{0};
    int weapon{-1}; // (data.weapons index; -1 none)
    int weapon2{-1};
    int armour{0};
    int plates{2};
    bool infected{false};
    int boss{0};
    int surge{0};
    float cost{0.f};
};

struct Combo
{
    int body;
    int weapon;
    float cost;
};

struct Run
{
    bool on{false};
    int skill{1};
    uint32_t seed{1};
    bool rowOk[gunRows]{};
    int planned{0}; // the last wave planned (0: none)
    // The memory (a pure function of the seed and the waves planned).
    int seenBody[maxItems]{};   // the wave it was last in (0: never)
    int seenWeapon[maxItems]{};
    int seenBoss[maxItems]{};
    bool offered[maxItems]{};   // a weapon some earlier wave's foe carried (the player had the chance of it)
    int lastNew{0};
    int themeOf[maxWaves]{};
    float carry{0.f};
    // The last wave's plan.
    Foe foes[maxPlan];
    int nFoes{0};
    int theme{0};
    int family{-1}; // (One Family's: a weapon index whose family it is)
    float budget{0.f};
    float spent{0.f};
    int surges{0};
    bool forced{false};
    int cap{0};
    // The supplies (they lean on the player, so they are not replayed).
    float bank{0.f};
    int lastPower{-99};
    int supply[48]{};
    int nSupply{0};
    // Scratch: the wave's per-item spend and count.
    float spentBody[maxItems]{};
    float spentWeapon[maxItems]{};
    int countBody[maxItems]{};
    int countWeapon[maxItems]{};
};
Run run;

Combo combos[maxItems * maxItems];
int nCombos{0};
char nameText[2][96]{}; // arenadirname's answers

// The difficulty (QC VR_ARENA_Skill, VR_ARENA_WaveAdj).
[[nodiscard]] float factor()
{
    const float m[4] = {0.75f, 1.f, 1.25f, 1.5f};
    return za::max(0.f, 1.f + vr_arena_skill_impact.value * (m[run.skill] - 1.f));
}
[[nodiscard]] float waveAdj(float w)
{
    const float d[4] = {3.f, 0.f, -2.f, -4.f};
    return za::max(2.f, w + d[run.skill] * vr_arena_skill_impact.value);
}
[[nodiscard]] float waveSize() { return za::clamp(vr_arena_wave_size.value, 0.25f, 4.f); }

[[nodiscard]] int bossEvery()
{
    int every = static_cast<int>(za::clamp(vr_arena_boss_every.value, 0.f, 20.f));
    if(every > 0 && run.skill < 1)
    {
        every += static_cast<int>(vr_arena_skill_impact.value);
    }
    return every;
}
[[nodiscard]] bool bossWave(int w)
{
    const int e = bossEvery();
    return e > 0 && w % e == 0;
}

// An item's `from`..`until` (the first rungs, from wave 3 or sooner, never moved by the difficulty).
[[nodiscard]] bool unlocked(const Block& b, int w)
{
    const float f = b.num("from");
    if(f <= 0.f)
    {
        return false;
    }
    const float u = b.num("until");
    return static_cast<float>(w) >= (f <= 3.f ? f : waveAdj(f)) && (u <= 0.f || static_cast<float>(w) <= waveAdj(u));
}

[[nodiscard]] bool weaponOk(int i, int w)
{
    const Block& g = weapon(i);
    const int row = static_cast<int>(g.num("row", -1));
    return unlocked(g, w) && (row < 0 || (row < gunRows && run.rowOk[row]));
}

[[nodiscard]] int armourBest(int w)
{
    return unlocked(*data.red, w) ? 3 : unlocked(*data.yellow, w) ? 2 : unlocked(*data.green, w) ? 1 : 0;
}
[[nodiscard]] int armourPlates(int w)
{
    const float past = za::max(0.f, static_cast<float>(w) - waveAdj(data.green->num("from", 4)));
    const float every = za::max(1.f, data.green->num("plate_every", 3));
    return za::clamp(2 + static_cast<int>(floorf(past * factor() / every)), 2, 10);
}
[[nodiscard]] float armourCost(int type, int plates)
{
    if(type <= 0)
    {
        return 0.f;
    }
    const Block& e = type == 1 ? *data.green : type == 2 ? *data.yellow : *data.red;
    return e.num("cost") + e.num("plate") * static_cast<float>(za::max(0, plates - 2));
}

[[nodiscard]] float foeCost(const Foe& f)
{
    float c = body(f.body).num("cost") + (f.weapon >= 0 ? weapon(f.weapon).num("cost") : 0.f);
    if(f.weapon2 >= 0)
    {
        c += weapon(f.weapon2).num("cost") * data.gun2->num("mult", 0.6f);
    }
    if(f.boss)
    {
        c *= data.bossMark->num("mult", 2.5f) * (f.boss == 1 ? 1.f : data.bossMark->num("second", 0.6f));
    }
    c += armourCost(f.armour, f.plates);
    if(f.infected)
    {
        c += data.infected->num("cost", 4);
    }
    return c;
}

[[nodiscard]] bool humanoid(int b) { return strcmp(body(b).str("arms", "none"), "none") != 0; }
[[nodiscard]] bool isBlade(int wi) { return !strcmp(weapon(wi).str("family"), "blades"); }

// The threat curve with its sawtooth, by the skill and the wave size.
[[nodiscard]] float threat(int w)
{
    const Block& t = *data.threat;
    const float b = t.num("base") + t.num("slope") * powf(static_cast<float>(za::max(0, w - 1)), t.num("power", 1));
    const int every = za::max(1, bossEvery() > 0 ? bossEvery() : 5);
    const int pos = (w - 1) % every;
    const int k = static_cast<int>(roundf(static_cast<float>(pos * (data.sawN - 1)) / static_cast<float>(za::max(1, every - 1))));
    return b * data.saw[za::clamp(k, 0, data.sawN - 1)] * factor() * waveSize();
}

void splitAt(int w, const Block& theme, float out[4])
{
    const float* s = data.split[data.splitN - 1];
    float tmp[4];
    if(w <= data.splitWave[0])
    {
        s = data.split[0];
    }
    for(int i = 0; i + 1 < data.splitN; i++)
    {
        const int w0 = data.splitWave[i];
        const int w1 = data.splitWave[i + 1];
        if(w0 <= w && w <= w1)
        {
            const float k = static_cast<float>(w - w0) / static_cast<float>(za::max(1, w1 - w0));
            for(int j = 0; j < 4; j++)
            {
                tmp[j] = data.split[i][j] + (data.split[i + 1][j] - data.split[i][j]) * k;
            }
            s = tmp;
            break;
        }
    }
    float add[4]{};
    if(theme.has("split") && w >= 4)
    {
        floats(theme.str("split"), add, 4);
    }
    float sum = 0.f;
    for(int j = 0; j < 4; j++)
    {
        out[j] = za::max(0.f, s[j] + add[j]);
        sum += out[j];
    }
    for(int j = 0; j < 4; j++)
    {
        out[j] = sum > 0.f ? out[j] / sum : (j == 0 ? 1.f : 0.f);
    }
}

[[nodiscard]] float novelty(int seenAt, int w)
{
    const float cap = data.novelty->num("cap", 1.6f);
    if(seenAt <= 0)
    {
        return cap;
    }
    const int d = w - seenAt;
    if(d <= 0)
    {
        return 1.f;
    }
    if(d <= data.recentN)
    {
        return data.recent[d - 1];
    }
    return za::min(cap, 1.f + data.novelty->num("bonus", 0.1f) * static_cast<float>(d - data.recentN - 1));
}

[[nodiscard]] float age(const Block& b, int w)
{
    const Block& n = *data.novelty;
    const float a = static_cast<float>(w) - waveAdj(b.num("from")) - n.num("fade_after", 10);
    return za::max(n.num("floor", 0.35f), 1.f - n.num("fade", 0.05f) * za::max(0.f, a));
}

// The theme's bias on a foe's tags ("tag:mult ...", each tag once).
[[nodiscard]] float bias(const Block& theme, const Block& b, const Block* g)
{
    float k = 1.f;
    const char* s = theme.str("bias");
    while(*s)
    {
        while(*s == ' ')
        {
            s++;
        }
        const char* colon = strchr(s, ':');
        if(!colon)
        {
            break;
        }
        char tag[24];
        q_strlcpy(tag, s, za::min(sizeof(tag), static_cast<size_t>(colon - s + 1)));
        char* e = nullptr;
        const float m = strtof(colon + 1, &e);
        if(b.word("tags", tag) || (g && g->word("tags", tag)))
        {
            k *= m;
        }
        s = e;
    }
    return k;
}

void buildCombos(int w)
{
    nCombos = 0;
    for(int bi = 0; bi < data.nBodies; bi++)
    {
        const Block& b = body(bi);
        if(!unlocked(b, w) || b.num("share", 1) <= 0.f)
        {
            continue;
        }
        const char* arms = b.str("arms", "none");
        if(!strcmp(arms, "none"))
        {
            combos[nCombos++] = {bi, -1, b.num("cost")};
            continue;
        }
        for(int wi = 0; wi < data.nWeapons; wi++)
        {
            if(!weaponOk(wi, w))
            {
                continue;
            }
            const Block& g = weapon(wi);
            const bool fists = g.num("row", 0) < 0;
            if(fists && strcmp(b.name, "grunt") != 0)
            {
                continue;
            }
            if(!strcmp(arms, "melee") && !isBlade(wi))
            {
                continue;
            }
            if(!strcmp(arms, "guns") && (isBlade(wi) || fists))
            {
                continue;
            }
            combos[nCombos++] = {bi, wi, b.num("cost") + g.num("cost")};
        }
    }
}

[[nodiscard]] float comboWeight(const Combo& c, int w, const Block& theme)
{
    const Block& b = body(c.body);
    float k = b.num("weight", 1) * sqrtf(novelty(run.seenBody[c.body], w));
    k *= powf(data.novelty->num("again", 0.75f), 0.5f * static_cast<float>(run.countBody[c.body]));
    const Block* g = nullptr;
    if(c.weapon >= 0)
    {
        g = &weapon(c.weapon);
        float again = theme.num("again", data.novelty->num("again", 0.75f));
        k *= g->num("weight", 1) * novelty(run.seenWeapon[c.weapon], w) * age(*g, w);
        if(run.family >= 0 && !strcmp(g->str("family"), weapon(run.family).str("family")))
        {
            k *= 5.f;
            again = 1.f;
        }
        k *= powf(again, static_cast<float>(run.countWeapon[c.weapon]));
    }
    return k * bias(theme, b, g);
}

[[nodiscard]] bool fits(const Combo& c, float total)
{
    const Block& b = body(c.body);
    if(run.spentBody[c.body] > 0.f && run.spentBody[c.body] + b.num("cost") > b.num("share", 1) * total + 1e-3f)
    {
        return false;
    }
    if(c.weapon >= 0)
    {
        const Block& g = weapon(c.weapon);
        if(run.spentWeapon[c.weapon] > 0.f && run.spentWeapon[c.weapon] + g.num("cost") > g.num("share", 1) * total + 1e-3f)
        {
            return false;
        }
    }
    return true;
}

Foe& add(const Combo& c, int surge)
{
    Foe& f = run.foes[run.nFoes++];
    f = Foe{};
    f.body = c.body;
    f.weapon = c.weapon;
    f.surge = surge;
    run.spentBody[c.body] += body(c.body).num("cost");
    run.countBody[c.body]++;
    if(c.weapon >= 0)
    {
        run.spentWeapon[c.weapon] += weapon(c.weapon).num("cost");
        run.countWeapon[c.weapon]++;
    }
    return f;
}

// Buy foes in a cost band [lo, hi] with `budget`; what is left comes back.
float spend(Rng& rng, float budget, float lo, float hi, int w, const Block& theme, float total, int surge, float over)
{
    int cand[maxItems * maxItems];
    float ws[maxItems * maxItems];
    const float quota = theme.num("humanoid", data.roles->num("humanoid", 0.7f));
    while(run.nFoes < run.cap)
    {
        int n = 0;
        for(int i = 0; i < nCombos; i++)
        {
            const Combo& c = combos[i];
            if(c.cost >= lo && c.cost <= hi && c.cost <= budget + over && fits(c, total))
            {
                cand[n++] = i;
            }
        }
        if(!n) // (a share cap may be all that stops it: a thin wave's grunt backbone)
        {
            for(int i = 0; i < nCombos; i++)
            {
                const Combo& c = combos[i];
                if(c.cost >= lo && c.cost <= hi && c.cost <= budget + over && !strcmp(body(c.body).str("kind"), "grunt"))
                {
                    cand[n++] = i;
                }
            }
            if(!n)
            {
                break;
            }
        }
        int nh = 0;
        for(int i = 0; i < n; i++)
        {
            nh += combos[cand[i]].weapon >= 0 ? 1 : 0;
        }
        const bool humanOnly = nh > 0 && rng.next() < quota;
        float sum = 0.f;
        for(int i = 0; i < n; i++)
        {
            const Combo& c = combos[cand[i]];
            ws[i] = (humanOnly && c.weapon < 0) ? 0.f : comboWeight(c, w, theme);
            sum += ws[i];
        }
        float r = rng.next() * sum;
        int pick = cand[n - 1];
        for(int i = 0; i < n; i++)
        {
            if(ws[i] > 0.f && r < ws[i])
            {
                pick = cand[i];
                break;
            }
            r -= ws[i];
        }
        add(combos[pick], surge);
        budget -= combos[pick].cost;
    }
    return budget;
}

// The boss (one; two at Nightmare): the candidates within its share, by weight and novelty, else the cheapest.
float buyBoss(Rng& rng, int w, float total)
{
    struct Cand
    {
        int boss;
        int body;
        int weapon;
        int armour;
        float cost;
    };
    Cand cands[maxItems];
    int n = 0;
    const int best = armourBest(w);
    for(int i = 0; i < data.nBosses && n < maxItems; i++)
    {
        const Block& b = blk(data.bosses[i]);
        if(!unlocked(b, w))
        {
            continue;
        }
        int bi = -1;
        for(int k = 0; k < data.nBodies; k++)
        {
            if(!strcmp(body(k).name, b.str("body")))
            {
                bi = k;
            }
        }
        if(bi < 0)
        {
            continue;
        }
        int wi = -1;
        if(!strcmp(b.str("weapon"), "tierpick"))
        {
            const char* arms = body(bi).str("arms", "none");
            int top = -1;
            int pool[maxItems];
            int np = 0;
            for(int k = 0; k < data.nWeapons; k++)
            {
                const bool fists = weapon(k).num("row", 0) < 0;
                if(!weaponOk(k, w) || fists || (!strcmp(arms, "melee") && !isBlade(k)) ||
                   (!strcmp(arms, "guns") && isBlade(k)))
                {
                    continue;
                }
                const int t = static_cast<int>(weapon(k).num("tier"));
                if(t > top)
                {
                    top = t;
                    np = 0;
                }
                if(t == top)
                {
                    pool[np++] = k;
                }
            }
            if(!np)
            {
                continue;
            }
            wi = pool[rng.below(np)];
        }
        Foe f;
        f.body = bi;
        f.weapon = wi;
        f.boss = 1;
        if(humanoid(bi) && best > 0)
        {
            f.armour = b.has("armour") ? static_cast<int>(b.num("armour")) : za::min(3, best + 1);
            f.plates = za::min(10, armourPlates(w) + 4);
        }
        float cost = foeCost(f);
        if(run.skill >= 3 && vr_arena_skill_impact.value > 0.f)
        {
            f.boss = 2;
            cost += foeCost(f);
        }
        cands[n++] = {i, bi, wi, f.armour, cost};
    }
    if(!n)
    {
        return 0.f;
    }
    const float share = data.threat->num("boss_share", 0.55f) * total;
    float ws[maxItems];
    float sum = 0.f;
    int cheapest = 0;
    for(int i = 0; i < n; i++)
    {
        cheapest = cands[i].cost < cands[cheapest].cost ? i : cheapest;
        ws[i] = cands[i].cost <= share ? blk(data.bosses[cands[i].boss]).num("weight", 1) * novelty(run.seenBoss[cands[i].boss], w) : 0.f;
        sum += ws[i];
    }
    int pick = cheapest;
    if(sum > 0.f)
    {
        float r = rng.next() * sum;
        for(int i = 0; i < n; i++)
        {
            if(ws[i] > 0.f && r < ws[i])
            {
                pick = i;
                break;
            }
            r -= ws[i];
        }
    }
    const Cand& c = cands[pick];
    run.seenBoss[c.boss] = w;
    const int bosses = (run.skill >= 3 && vr_arena_skill_impact.value > 0.f) ? 2 : 1;
    for(int i = 0; i < bosses; i++)
    {
        Foe& f = add({c.body, c.weapon, 0.f}, 0);
        f.boss = 1 + i;
        f.armour = c.armour;
        f.plates = c.armour ? za::min(10, armourPlates(w) + 4) : 2;
    }
    return c.cost;
}

// The progression guarantee: a gun no earlier wave offered, on a grunt (the lowest tier, the cheapest; or one up to
// `peek` waves before its own wave when every unlocked one was offered).
float guarantee(Rng& rng, int w, float budget)
{
    run.forced = false;
    const Block& pr = *data.progress;
    if(static_cast<float>(w) < waveAdj(pr.num("from", 3)) || w - run.lastNew < static_cast<int>(pr.num("every", 3)))
    {
        return budget;
    }
    for(int i = 0; i < run.nFoes; i++)
    {
        if(run.foes[i].weapon >= 0 && !run.offered[run.foes[i].weapon])
        {
            return budget;
        }
    }
    int grunt = -1;
    for(int k = 0; k < data.nBodies; k++)
    {
        grunt = !strcmp(body(k).str("kind"), "grunt") ? k : grunt;
    }
    if(grunt < 0)
    {
        return budget;
    }
    for(int pass = 0; pass < 2; pass++)
    {
        int best = -1;
        float bestTier = 99.f;
        float bestCost = 1e9f;
        for(int k = 0; k < data.nWeapons; k++)
        {
            const Block& g = weapon(k);
            const int row = static_cast<int>(g.num("row", -1));
            if(row < 0 || run.offered[k] || !run.rowOk[row])
            {
                continue;
            }
            const bool ok = pass == 0 ? weaponOk(k, w)
                                      : g.num("from") > 0.f && waveAdj(g.num("from")) <= static_cast<float>(w) + pr.num("peek", 3);
            if(!ok)
            {
                continue;
            }
            const float t = g.num("tier");
            const float c = g.num("cost") + rng.next() * 0.01f;
            if(t < bestTier || (t == bestTier && c < bestCost))
            {
                best = k;
                bestTier = t;
                bestCost = c;
            }
        }
        if(best >= 0)
        {
            const Combo c{grunt, best, body(grunt).num("cost") + weapon(best).num("cost")};
            add(c, 0);
            run.forced = true;
            return budget - c.cost;
        }
    }
    return budget;
}

// Upgrades on the wave's humanoids (the priciest first): armour, a second weapon, an infection, each within its share.
float upgrades(Rng& rng, float budget, int w, float total)
{
    const int best = armourBest(w);
    int order[maxPlan];
    int n = 0;
    for(int i = 0; i < run.nFoes; i++)
    {
        if(humanoid(run.foes[i].body) && !run.foes[i].boss)
        {
            order[n++] = i;
        }
    }
    for(int i = 0; i < n; i++) // (priciest first; a stable order for equal costs)
    {
        for(int j = i + 1; j < n; j++)
        {
            if(foeCost(run.foes[order[j]]) > foeCost(run.foes[order[i]]))
            {
                const int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    }
    float armourSpent = 0.f;
    float gun2Spent = 0.f;
    float infSpent = 0.f;
    for(int i = 0; i < run.nFoes; i++)
    {
        const Foe& f = run.foes[i];
        if(!f.boss)
        {
            armourSpent += armourCost(f.armour, f.plates);
            gun2Spent += f.weapon2 >= 0 ? weapon(f.weapon2).num("cost") * data.gun2->num("mult", 0.6f) : 0.f;
            infSpent += f.infected ? data.infected->num("cost", 4) : 0.f;
        }
    }
    const bool foeArmour = vr_arena_foe_armor.value != 0.f;
    for(int k = 0; k < n; k++)
    {
        Foe& f = run.foes[order[k]];
        const char* opts[3];
        float ow[3];
        int no = 0;
        if(foeArmour && best > 0 && !f.armour)
        {
            opts[no] = "armour";
            ow[no++] = 0.75f;
        }
        if(unlocked(*data.gun2, w) && f.weapon >= 0 && weapon(f.weapon).num("row", -1) >= 0 && f.weapon2 < 0)
        {
            opts[no] = "gun2";
            ow[no++] = 0.15f;
        }
        if(unlocked(*data.infected, w) && data.infected->num("share") > 0.f && !f.infected)
        {
            opts[no] = "infected";
            ow[no++] = 0.10f;
        }
        if(!no)
        {
            continue;
        }
        float sum = 0.f;
        for(int i = 0; i < no; i++)
        {
            sum += ow[i];
        }
        float r = rng.next() * sum;
        int o = no - 1;
        for(int i = 0; i < no; i++)
        {
            if(r < ow[i])
            {
                o = i;
                break;
            }
            r -= ow[i];
        }
        if(!strcmp(opts[o], "armour"))
        {
            const int type = 1 + rng.below(best);
            const float c = armourCost(type, armourPlates(w));
            if(c <= budget && armourSpent + c <= data.green->num("share", 0.5f) * total)
            {
                f.armour = type;
                f.plates = armourPlates(w);
                budget -= c;
                armourSpent += c;
            }
        }
        else if(!strcmp(opts[o], "gun2"))
        {
            const float c = weapon(f.weapon).num("cost") * data.gun2->num("mult", 0.6f);
            if(c <= budget && gun2Spent + c <= data.gun2->num("share", 0.15f) * total)
            {
                f.weapon2 = f.weapon;
                budget -= c;
                gun2Spent += c;
            }
        }
        else
        {
            const float c = data.infected->num("cost", 4);
            if(c <= budget && infSpent + c <= data.infected->num("share", 0.1f) * total)
            {
                f.infected = true;
                budget -= c;
                infSpent += c;
            }
        }
    }
    return budget;
}

[[nodiscard]] const Block& themeBlock(int t) { return blk(data.themes[za::clamp(t, 0, data.nThemes - 1)]); }

// A theme value, or the cadence curve's.
[[nodiscard]] float cad(const Block& theme, const char* key, float d)
{
    return theme.num(key, data.cadence->num(key, d));
}

int rollTheme(Rng& rng, int w)
{
    if(w <= 3 || vr_arena_themes.value == 0.f)
    {
        return 0; // (the first: Mixed)
    }
    const int last = w > 1 ? run.themeOf[za::min(w - 1, maxWaves - 1)] : -1;
    float ws[maxItems];
    float sum = 0.f;
    for(int t = 0; t < data.nThemes; t++)
    {
        const Block& b = themeBlock(t);
        ws[t] = 0.f;
        if(!unlocked(b, w) || (t == last && t != 0))
        {
            continue;
        }
        // Gated on the bodies it leans on: every body it names in `needs` unlocked.
        bool ok = true;
        const char* need = b.str("needs");
        char name[40];
        while(*need && ok)
        {
            while(*need == ' ')
            {
                need++;
            }
            int len = 0;
            while(need[len] && need[len] != ' ' && len < 39)
            {
                name[len] = need[len];
                len++;
            }
            name[len] = 0;
            need += len;
            if(len)
            {
                bool found = false;
                for(int k = 0; k < data.nBodies; k++)
                {
                    found = found || (!strcmp(body(k).name, name) && unlocked(body(k), w));
                }
                ok = found;
            }
        }
        if(!ok)
        {
            continue;
        }
        bool recent = false;
        for(int k = za::max(1, w - 3); k < w; k++)
        {
            recent = recent || (run.themeOf[za::min(k, maxWaves - 1)] == t);
        }
        ws[t] = b.num("weight", 1) * (recent && t != 0 ? 0.5f : 1.f);
        sum += ws[t];
    }
    float r = rng.next() * sum;
    for(int t = 0; t < data.nThemes; t++)
    {
        if(ws[t] > 0.f && r < ws[t])
        {
            return t;
        }
        r -= ws[t];
    }
    return 0;
}

void resetRun()
{
    const bool on = run.on;
    const int skill = run.skill;
    const uint32_t seed = run.seed;
    bool rows[gunRows];
    memcpy(rows, run.rowOk, sizeof(rows));
    run = Run{};
    run.on = on;
    run.skill = skill;
    run.seed = seed;
    memcpy(run.rowOk, rows, sizeof(rows));
}

void planOne(int w)
{
    Rng rng{mix(run.seed, static_cast<uint32_t>(w) * 7919u + 1u)};
    const float total = threat(w) + run.carry;
    run.nFoes = 0;
    memset(run.spentBody, 0, sizeof(run.spentBody));
    memset(run.spentWeapon, 0, sizeof(run.spentWeapon));
    memset(run.countBody, 0, sizeof(run.countBody));
    memset(run.countWeapon, 0, sizeof(run.countWeapon));
    run.theme = rollTheme(rng, w);
    run.themeOf[za::min(w, maxWaves - 1)] = run.theme;
    const Block& theme = themeBlock(run.theme);
    buildCombos(w);
    run.family = -1;
    if(theme.num("family") != 0.f)
    {
        int fam[maxItems];
        int nf = 0;
        for(int i = 0; i < nCombos; i++) // (one weapon of each family the wave could field)
        {
            const int wi = combos[i].weapon;
            if(wi < 0 || !strcmp(weapon(wi).str("family", "none"), "none"))
            {
                continue;
            }
            bool dup = false;
            for(int k = 0; k < nf; k++)
            {
                dup = dup || !strcmp(weapon(fam[k]).str("family"), weapon(wi).str("family"));
            }
            if(!dup)
            {
                fam[nf++] = wi;
            }
        }
        run.family = nf ? fam[rng.below(nf)] : -1;
    }
    float a[2] = {3.f, 0.5f};
    floats(data.threat->str("count", "3 0.5"), a, 2);
    run.cap = za::clamp(static_cast<int>((a[0] + a[1] * static_cast<float>(w)) * waveSize() * factor() + 0.5f), 1, maxPlan);
    run.budget = total;

    float budget = total;
    if(bossWave(w))
    {
        budget -= buyBoss(rng, w, total);
    }
    budget = guarantee(rng, w, budget);

    // The surges: the held-back share, spent as groups that come in at once.
    const bool surgesOn = vr_arena_reinforce.value != 0.f && static_cast<float>(w) >= waveAdj(data.threat->num("reserve_from", 6));
    float sv[3] = {1.f, 0.05f, 3.f};
    floats(theme.str("surges", data.cadence->str("surges", "1 0.05 3")), sv, 3);
    run.surges = surgesOn ? za::clamp(static_cast<int>(sv[0] + sv[1] * static_cast<float>(w)), 0, static_cast<int>(sv[2])) : 0;
    const float reserve = run.surges > 0 ? cad(theme, "reserve", data.threat->num("reserve", 0.2f)) * za::max(0.f, budget) : 0.f;
    const float main = za::max(0.f, budget - reserve);

    float s[4];
    splitAt(w, theme, s);
    const float fm = data.roles->num("fodder_max", 12);
    const float mm = data.roles->num("mid_max", 24);
    const float lo[3] = {0.f, fm + 1e-3f, mm + 1e-3f};
    const float hi[3] = {fm, mm, 1e9f};
    float left = 0.f;
    for(int role = 2; role >= 0; role--)
    {
        left = spend(rng, main * s[role] + left, lo[role], hi[role], w, theme, total, 0, 0.f);
    }
    left = upgrades(rng, left + main * s[3], w, total);
    left = spend(rng, left, lo[0], hi[0], w, theme, total, 0, run.surges ? 0.f : data.threat->num("overshoot", 2));
    const int nMain = run.nFoes;
    for(int k = 1; k <= run.surges; k++)
    {
        left = spend(rng, reserve / static_cast<float>(run.surges) + (k == 1 ? left : 0.f), 0.f, mm, w, theme, total, k,
                     k == run.surges ? data.threat->num("overshoot", 2) : 0.f);
    }
    if(left > 0.f)
    {
        left = upgrades(rng, left, w, total); // (a full wave: its budget left turned into upgrades)
    }
    run.carry = za::min(left, data.threat->num("carry_max", 0.25f) * threat(w + 1)); // (negative: the overshoot's debt)

    // The order QC brings them in: the wave's own first with its bosses at their middle, then each surge.
    Foe ordered[maxPlan];
    int no = 0;
    int nb = 0;
    for(int i = 0; i < nMain; i++)
    {
        nb += run.foes[i].boss ? 1 : 0;
    }
    const int mainsOnly = nMain - nb;
    const int mid = mainsOnly / 2;
    int seenMain = 0;
    for(int i = 0; i <= nMain; i++)
    {
        if(seenMain == mid)
        {
            for(int j = 0; j < nMain; j++)
            {
                if(run.foes[j].boss)
                {
                    ordered[no++] = run.foes[j];
                }
            }
            seenMain = -1;
        }
        if(i < nMain && !run.foes[i].boss)
        {
            ordered[no++] = run.foes[i];
            seenMain = seenMain >= 0 ? seenMain + 1 : seenMain;
        }
    }
    // Each surge after its share of the wave's own (surge k of S after k / (S + 1) of them): QC reads them in order.
    Foe withSurges[maxPlan];
    int ns = 0;
    int k = 1;
    for(int i = 0; i <= no; i++)
    {
        while(k <= run.surges && i >= za::min(no, za::max(1, (k * no) / (run.surges + 1))))
        {
            for(int j = nMain; j < run.nFoes; j++)
            {
                if(run.foes[j].surge == k)
                {
                    withSurges[ns++] = run.foes[j];
                }
            }
            k++;
        }
        if(i < no)
        {
            withSurges[ns++] = ordered[i];
        }
    }
    memcpy(run.foes, withSurges, sizeof(Foe) * static_cast<size_t>(ns));
    run.nFoes = ns;

    run.spent = 0.f;
    for(int i = 0; i < run.nFoes; i++)
    {
        Foe& f = run.foes[i];
        f.cost = foeCost(f);
        run.spent += f.cost;
        run.seenBody[f.body] = w;
        if(f.weapon >= 0)
        {
            run.seenWeapon[f.weapon] = w;
            if(!run.offered[f.weapon] && weapon(f.weapon).num("row", -1) >= 0)
            {
                run.lastNew = w;
            }
            run.offered[f.weapon] = true;
        }
    }
    run.planned = w;
}

void logPlan(int w)
{
    Con_Printf("arena director: wave %d theme %s budget %.0f spent %.0f foes %d cap %d surges %d carry %.1f%s\n", w,
               themeBlock(run.theme).str("name", themeBlock(run.theme).name), run.budget, run.spent, run.nFoes, run.cap,
               run.surges, run.carry, run.forced ? " (new gun forced)" : "");
    for(int i = 0; i < run.nFoes; i++)
    {
        const Foe& f = run.foes[i];
        Con_Printf("  %s%s%s%s%s%s%s cost %.1f%s\n", body(f.body).name, f.weapon >= 0 ? "+" : "",
                   f.weapon >= 0 ? weapon(f.weapon).name : "", f.weapon2 >= 0 ? " dual" : "",
                   f.armour ? (f.armour == 1 ? " green" : f.armour == 2 ? " yellow" : " red") : "",
                   f.infected ? " infected" : "", f.boss ? " BOSS" : "", f.cost,
                   f.surge ? va(" surge %d", f.surge) : "");
    }
}

bool plan(int w)
{
    if(!run.on || !data.loaded || w < 1)
    {
        return false;
    }
    if(w == run.planned)
    {
        return true;
    }
    if(w < run.planned)
    {
        resetRun();
    }
    for(int k = run.planned + 1; k <= w; k++)
    {
        planOne(k);
    }
    if(vr_arena_director_log.value != 0.f)
    {
        logPlan(w);
    }
    return true;
}

bool start(int skill, uint32_t seed, const char* rows)
{
    if(!load())
    {
        run.on = false;
        return false;
    }
    run.skill = za::clamp(skill, 0, 3);
    run.seed = seed ? seed : 1u;
    for(int i = 0; i < gunRows; i++)
    {
        run.rowOk[i] = rows && static_cast<int>(strlen(rows)) > i ? rows[i] == '1' : true;
    }
    run.on = true;
    resetRun();
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// The supplies
// ---------------------------------------------------------------------------------------------------------------
float supplyPlan(int w, float threatBeaten, float hp, float dom, float spent[4])
{
    const Block& s = *data.supply;
    float pts = za::min(s.num("max", 60), (s.num("base", 8) + s.num("per_threat", 0.06f) * threatBeaten) / za::max(0.1f, factor()));
    pts *= za::clamp(vr_arena_supply_scale.value, 0.f, 4.f);
    if(vr_arena_supply_reactive.value == 0.f || s.num("reactive", 1) == 0.f)
    {
        hp = 0.5f;
        dom = 0.5f;
    }
    const char* kinds[4] = {"power", "armour", "heal", "ammo"};
    float sh[4] = {s.num("power", 0.07f) + 0.25f * za::max(0.f, dom - 0.6f), s.num("armour", 0.3f),
                   s.num("heal", 0.55f) + 0.5f * (0.5f - hp) - 0.2f * (dom - 0.5f), s.num("ammo", 0.08f)};
    float tot = 0.f;
    for(float& v : sh)
    {
        v = za::max(0.f, v);
        tot += v;
    }
    run.nSupply = 0;
    float left = 0.f;
    for(int k = 0; k < 4; k++)
    {
        spent[k] = 0.f;
        float b = pts * sh[k] / za::max(1e-3f, tot) + left;
        left = 0.f;
        if(k == 0)
        {
            b += run.bank;
            run.bank = 0.f;
        }
        // The kind's items, priciest first.
        int items[maxItems];
        int n = 0;
        for(int i = 0; i < data.nSupplies; i++)
        {
            const Block& it = blk(data.supplies[i]);
            if(!strcmp(it.str("kind"), kinds[k]) && unlocked(it, w))
            {
                items[n++] = i;
            }
        }
        for(int i = 0; i < n; i++)
        {
            for(int j = i + 1; j < n; j++)
            {
                if(blk(data.supplies[items[j]]).num("cost") > blk(data.supplies[items[i]]).num("cost"))
                {
                    const int t = items[i];
                    items[i] = items[j];
                    items[j] = t;
                }
            }
        }
        bool bought = false;
        for(int i = 0; i < n && !(k == 0 && bought); i++)
        {
            const Block& it = blk(data.supplies[items[i]]);
            if(k == 0 && w - run.lastPower < static_cast<int>(it.num("gap", 3)))
            {
                continue;
            }
            int got = 0;
            while(it.num("cost") <= b && got < static_cast<int>(it.num("max", 99)) && run.nSupply < 48)
            {
                if(k == 2 && it.num("heal") >= 100.f && hp > 0.35f)
                {
                    break; // (a megahealth only for one who needs it)
                }
                run.supply[run.nSupply++] = static_cast<int>(it.num("code"));
                b -= it.num("cost");
                spent[k] += it.num("cost");
                got++;
                if(k == 0)
                {
                    run.lastPower = w;
                    bought = true;
                    break;
                }
            }
        }
        if(k == 0 && !bought)
        {
            run.bank = b; // (saved for a later clear)
            b = 0.f;
        }
        left = b;
    }
    for(int i = 0; i < static_cast<int>(left) && run.nSupply < 48; i++)
    {
        run.supply[run.nSupply++] = SupplyShard;
        spent[1] += 1.f;
    }
    return pts;
}

// ---------------------------------------------------------------------------------------------------------------
// The commands: the plans printed, the self-test, the data reloaded
// ---------------------------------------------------------------------------------------------------------------
const char allRows[gunRows + 1] = "1111111111111111111111111111111111";

void print_f()
{
    const uint32_t seed = Cmd_Argc() > 1 ? static_cast<uint32_t>(atoi(Cmd_Argv(1))) : 1u;
    const int skill = Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : 1;
    const int waves = Cmd_Argc() > 3 ? atoi(Cmd_Argv(3)) : 10;
    if(!start(skill, seed, allRows))
    {
        return;
    }
    for(int w = 1; w <= waves; w++)
    {
        planOne(w);
        logPlan(w);
    }
    run.on = false;
}

void reload_f()
{
    Con_Printf("arena director: %s %s\n", dataPath, load() ? "loaded" : "FAILED");
}

[[nodiscard]] uint32_t planHash()
{
    uint32_t h = 7;
    for(int i = 0; i < run.nFoes; i++)
    {
        const Foe& f = run.foes[i];
        h = mix(h, static_cast<uint32_t>(f.body * 131 + (f.weapon + 1) * 7 + f.armour * 3 + f.boss + f.surge * 977 +
                                          (f.weapon2 + 1) * 1009 + (f.infected ? 5003 : 0)));
    }
    return mix(h, static_cast<uint32_t>(run.theme));
}

// The director's own checks (vr_arena_director_test): plans for many seeds at every skill, waves 1-30.
void test_f()
{
    testkit::Tally t;
    const char* name = "arena director test";
    char what[256];
    int knightsGun = 0;
    int dkGun = 0;
    int dkBoss = 0;
    int knightfall = 0;
    int themeRepeat = 0;
    int themeEarly = 0;
    int overBudget = 0;
    int overCap = 0;
    int drought = 0;
    int themes = 0;
    float surgeFoes = 0.f;
    int supplyBad = 0;
    int powerGap = 0;
    for(int skill = 0; skill < 4; skill++)
    {
        for(uint32_t seed = 1; seed <= 40; seed++)
        {
            uint32_t hashes[31]{};
            if(!t.check(start(skill, seed, allRows), name, "the data file loads"))
            {
                testkit::verdict(name, t.passed, t.failed);
                return;
            }
            int lastPowerSeen = -99;
            for(int w = 1; w <= 30; w++)
            {
                planOne(w);
                hashes[w] = planHash();
                const Block& th = themeBlock(run.theme);
                themes |= 1 << run.theme;
                if(w > 1 && run.theme != 0 && run.theme == run.themeOf[w - 1])
                {
                    themeRepeat++;
                }
                if(!unlocked(th, w))
                {
                    themeEarly++;
                }
                if(!strcmp(th.name, "knightfall"))
                {
                    knightfall++;
                }
                // The budget: never past what it had, but for the overshoot and a boss that fits no share.
                float bossCost = 0.f;
                for(int i = 0; i < run.nFoes; i++)
                {
                    const Foe& f = run.foes[i];
                    bossCost += f.boss ? f.cost : 0.f;
                    surgeFoes += f.surge ? 1.f : 0.f;
                    const char* k = body(f.body).name;
                    if(f.weapon >= 0 && !strcmp(k, "gun_knight")) { knightsGun++; }
                    if(f.weapon >= 0 && !strcmp(k, "gun_dk") && !f.boss) { dkGun++; }
                    if(f.boss && !strcmp(k, "gun_dk") && f.armour == 3) { dkBoss++; }
                }
                const float allowance = data.threat->num("overshoot", 2) + 0.01f +
                                        za::max(0.f, bossCost - data.threat->num("boss_share", 0.55f) * run.budget);
                if(run.spent > run.budget + allowance)
                {
                    overBudget++;
                    if(overBudget <= 3)
                    {
                        snprintf(what, sizeof(what), "skill %d seed %u wave %d spent %.1f of %.1f", skill, seed, w, run.spent, run.budget);
                        t.check(false, name, what);
                    }
                }
                if(run.nFoes > run.cap + (bossWave(w) ? 2 : 0) + 1)
                {
                    overCap++;
                }
                if(w >= 6 && w - run.lastNew > 3)
                {
                    // A drought is fine only when no gun is left to offer within `peek`.
                    bool any = false;
                    for(int k = 0; k < data.nWeapons; k++)
                    {
                        const Block& g = weapon(k);
                        any = any || (g.num("row", -1) >= 0 && !run.offered[k] && g.num("from") > 0.f &&
                                      waveAdj(g.num("from")) <= static_cast<float>(w) + data.progress->num("peek", 3));
                    }
                    drought += any ? 1 : 0;
                }
                float spent[4];
                const float pts = supplyPlan(w, run.spent, 0.2f + 0.025f * static_cast<float>(w % 30), static_cast<float>(seed % 10) / 10.f, spent);
                if(spent[0] + spent[1] + spent[2] + spent[3] > pts + 60.f + 1.f || pts <= 0.f)
                {
                    supplyBad++;
                }
                for(int i = 0; i < run.nSupply; i++)
                {
                    const int c = run.supply[i];
                    if(c == SupplyQuad || c == SupplyRing || c == SupplyPent)
                    {
                        powerGap += (w - lastPowerSeen < 3) ? 1 : 0;
                        lastPowerSeen = w;
                    }
                }
            }
            // The same seed again: the same plans (and a wave planned out of order: replayed, the same).
            start(skill, seed, allRows);
            bool same = true;
            for(int w = 1; w <= 30; w++)
            {
                planOne(w);
                same = same && planHash() == hashes[w];
            }
            run.planned = 30;
            plan(12);
            same = same && planHash() == hashes[12];
            snprintf(what, sizeof(what), "skill %d seed %u: the same seed plans the same waves (replayed too)", skill, seed);
            t.check(same, name, what);
        }
    }
    snprintf(what, sizeof(what), "%d waves over budget", overBudget);
    t.check(overBudget == 0, name, what);
    snprintf(what, sizeof(what), "%d waves over the foe cap", overCap);
    t.check(overCap == 0, name, what);
    snprintf(what, sizeof(what), "%d waves with 3+ waves and no new gun though one was there", drought);
    t.check(drought == 0, name, what);
    int seenThemes = 0;
    for(int k = 0; k < data.nThemes; k++)
    {
        seenThemes += (themes >> k) & 1;
    }
    snprintf(what, sizeof(what), "themes: %d repeats, %d before their wave, %d of %d themes seen", themeRepeat, themeEarly,
             seenThemes, data.nThemes);
    t.check(themeRepeat == 0 && themeEarly == 0 && seenThemes == data.nThemes, name, what);
    snprintf(what, sizeof(what), "knights with guns %d, death knights with guns %d, death knight bosses in red %d, Knightfall %d",
             knightsGun, dkGun, dkBoss, knightfall);
    t.check(knightsGun > 0 && dkGun > 0 && dkBoss > 0 && knightfall > 0, name, what);
    snprintf(what, sizeof(what), "surge foes %.0f", surgeFoes);
    t.check(surgeFoes > 0.f, name, what);
    snprintf(what, sizeof(what), "supplies: %d bad, %d power-ups closer than 3 waves", supplyBad, powerGap);
    t.check(supplyBad == 0 && powerGap == 0, name, what);

    // A row the game lacks never comes (rowsOk), and a plan with director data off is refused.
    char rows[gunRows + 1];
    memcpy(rows, allRows, sizeof(rows));
    rows[14] = '0'; // (the rocket launcher)
    start(3, 5, rows);
    int rl = 0;
    for(int w = 1; w <= 30; w++)
    {
        planOne(w);
        for(int i = 0; i < run.nFoes; i++)
        {
            rl += run.foes[i].weapon >= 0 && weapon(run.foes[i].weapon).num("row", -1) == 14.f ? 1 : 0;
        }
    }
    snprintf(what, sizeof(what), "a missing row: %d rocket launchers planned", rl);
    t.check(rl == 0, name, what);
    Con_Printf("arena director test: knights with guns %d, death knights with guns %d, red death knight bosses %d, "
               "Knightfall waves %d, surge foes %.0f\n", knightsGun, dkGun, dkBoss, knightfall, surgeFoes);
    run.on = false;
    testkit::verdict(name, t.passed, t.failed);
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_arena_director_print", print_f);   // <seed> <skill> <waves>: the plans, as the log prints them
    Cmd_AddCommand("vr_arena_director_reload", reload_f); // data/arena_director.txt read again
    Cmd_AddCommand("vr_arena_director_test", test_f);     // the planner's own checks: "arena director test: N passed, M failed"
}

void pfStart()
{
    const int skill = static_cast<int>(G_FLOAT(OFS_PARM0));
    const uint32_t seed = static_cast<uint32_t>(za::max(1.f, G_FLOAT(OFS_PARM1)));
    // The rows this game has: two 17-bit masks (rows 0..16, 17..33), a float holding 24 bits exactly.
    const int lo = static_cast<int>(G_FLOAT(OFS_PARM2));
    const int hi = static_cast<int>(G_FLOAT(OFS_PARM3));
    char rows[gunRows + 1];
    for(int i = 0; i < gunRows; i++)
    {
        rows[i] = ((i < 17 ? lo >> i : hi >> (i - 17)) & 1) ? '1' : '0';
    }
    rows[gunRows] = 0;
    G_FLOAT(OFS_RETURN) = start(skill, seed, rows) ? 1.f : 0.f;
}

void pfPlan()
{
    const int w = static_cast<int>(G_FLOAT(OFS_PARM0));
    G_FLOAT(OFS_RETURN) = plan(w) ? static_cast<float>(run.nFoes) : 0.f;
}

void pfGet()
{
    const int what = static_cast<int>(G_FLOAT(OFS_PARM0));
    const int i = static_cast<int>(G_FLOAT(OFS_PARM1));
    const int field = static_cast<int>(G_FLOAT(OFS_PARM2));
    float v = 0.f;
    if(what == WhatFoe && i >= 0 && i < run.nFoes)
    {
        const Foe& f = run.foes[i];
        int kind = 0;
        for(int k = 0; k < kindCount; k++)
        {
            kind = !strcmp(kindNames[k], body(f.body).str("kind")) ? k : kind;
        }
        switch(field)
        {
        case FoeKind: v = static_cast<float>(kind); break;
        case FoeRow: v = f.weapon >= 0 ? weapon(f.weapon).num("row", -1) : -2.f; break;
        case FoeRow2: v = f.weapon2 >= 0 ? weapon(f.weapon2).num("row", -1) : -2.f; break;
        case FoeArmour: v = static_cast<float>(f.armour); break;
        case FoePlates: v = static_cast<float>(f.plates); break;
        case FoeInfected: v = f.infected ? 1.f : 0.f; break;
        case FoeBoss: v = static_cast<float>(f.boss); break;
        case FoeSurge: v = static_cast<float>(f.surge); break;
        case FoeCost: v = f.cost; break;
        default: break;
        }
    }
    else if(what == WhatWave && data.loaded)
    {
        const Block& th = themeBlock(run.theme);
        float g[2] = {1.f, 1.f};
        floats(th.str("group", data.cadence->str("group", "1 2")), g, 2);
        switch(field)
        {
        case WaveTheme: v = static_cast<float>(run.theme); break;
        case WaveBudget: v = run.budget; break;
        case WaveSpent: v = run.spent; break;
        case WaveSurges: v = static_cast<float>(run.surges); break;
        case WaveGap: v = cad(th, "gap", 3.f); break;
        case WaveGroupMin: v = g[0]; break;
        case WaveGroupMax: v = za::max(g[0], g[1]); break;
        case WaveStretch: v = cad(th, "stretch", 1.5f); break;
        case WaveSoft: v = cad(th, "soft", 0.6f); break;
        case WaveSpread: v = cad(th, "spread", 0.f); break;
        case WaveForced: v = run.forced ? 1.f : 0.f; break;
        case WaveQuickTime: v = data.cadence->num("quick_time", 4.f); break;
        case WaveQuickPay: v = data.cadence->num("quick_pay", 0.25f); break;
        case WaveSurgeWarn: v = data.cadence->num("surge_warn", 2.f); break;
        case WaveAmbush: v = cad(th, "ambush", 0.f); break;
        case WaveGapRef: v = za::max(1.f, cad(th, "gap_ref", 10.f)); break;
        default: break;
        }
    }
    else if(what == WhatSupply && i >= 0 && i < run.nSupply)
    {
        v = static_cast<float>(run.supply[i]);
    }
    G_FLOAT(OFS_RETURN) = v;
}

void pfName()
{
    const int what = static_cast<int>(G_FLOAT(OFS_PARM0));
    char* t = nameText[what == 1 ? 1 : 0];
    t[0] = 0;
    if(data.loaded && what == 0)
    {
        q_strlcpy(t, themeBlock(run.theme).str("name", themeBlock(run.theme).name), sizeof(nameText[0]));
    }
    else if(data.loaded && what == 1)
    {
        for(int i = 0; i < run.nFoes; i++)
        {
            const Foe& f = run.foes[i];
            if(f.boss == 1)
            {
                const char* bn = body(f.body).str("title", body(f.body).name);
                if(f.weapon >= 0 && weapon(f.weapon).num("row", -1) >= 0)
                {
                    q_snprintf(t, sizeof(nameText[0]), "%s with a %s", bn, weapon(f.weapon).str("title", weapon(f.weapon).name));
                }
                else
                {
                    q_strlcpy(t, bn, sizeof(nameText[0]));
                }
                break;
            }
        }
    }
    G_INT(OFS_RETURN) = PR_SetEngineString(t);
}

void pfSupply()
{
    const int w = static_cast<int>(G_FLOAT(OFS_PARM0));
    float spent[4];
    if(!data.loaded)
    {
        G_FLOAT(OFS_RETURN) = 0.f;
        return;
    }
    supplyPlan(w, G_FLOAT(OFS_PARM1), za::clamp(G_FLOAT(OFS_PARM2), 0.f, 1.f), za::clamp(G_FLOAT(OFS_PARM3), 0.f, 1.f), spent);
    G_FLOAT(OFS_RETURN) = static_cast<float>(run.nSupply);
}

} // namespace qvr::arenadirector
