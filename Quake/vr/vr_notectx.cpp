#include "vr_alloccount.h"
// vr_notectx.cpp -- see vr_notectx.hpp.

#include "vr_notectx.hpp"

#include "vr_box3d.hpp"
#include "vr_checklist.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_foegrab.hpp"
#include "vr_motion.hpp"
#include "vr_progs.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "vr_zancle.hpp"

#include <stdio.h>

namespace qvr::notectx
{
namespace
{

// The QC's weapon ids (QC vr_defs.qc WID_*), named.
constexpr const char* weaponNames[] = {"fists", "grapple", "axe", "Mjolnir", "shotgun", "super shotgun", "nailgun",
    "super nailgun", "grenade launcher", "proximity gun", "rocket launcher", "lightning gun", "laser cannon", "sword",
    "chainsaw", "grunt's shotgun", "enforcer's rifle", "crowbar", "super axe", "toolgun", "AD rocket launcher",
    "AD grenade launcher", "AD plasma rifle", "AD defender's shotgun", "AD eliminator", "AD centurion's plasma gun",
    "AD flamethrower", "AD fumigator", "AD crossbow"};

[[nodiscard]] const char* weaponName(int wid)
{
    return wid >= 0 && wid < static_cast<int>(za::getArraySize(weaponNames)) ? weaponNames[wid] : "?";
}

constexpr float nearbyMetres = 5.f;

// The console's lines (Con_Printf's chunks, each with its time): a ring of the last ones.
constexpr int consoleCap = 2048;
ConsoleLine consoleRing[consoleCap];
int consoleNext = 0;
int consoleCount = 0;

// The host frames' times: a ring (30 s at 144 Hz and more).
constexpr int frameCap = 8192;
FrameTime frameRing[frameCap];
int frameNext = 0;
int frameCount = 0;
int framesSeen = 0; // since the start: the settings are watched from the third (after the config's exec)

// The settings changed this session (archived cvars): the value before the first change, the latest, how many, when.
struct Change
{
    const cvar_s* var{nullptr};
    za::String first;
    za::String latest;
    int count{0};
    double at{0.0};
};
za::Vector<Change> changes;

// The checklist's last Test.
za::String lastTest;
double lastTestAt = -1.0;

[[nodiscard]] edict_t* entityOf(int ofsValue)
{
    if(ofsValue <= 0 || ofsValue % qcvm->edict_size != 0 || ofsValue / qcvm->edict_size >= qcvm->num_edicts)
    {
        return nullptr;
    }
    edict_t* e = PROG_TO_EDICT(ofsValue);
    return e->free ? nullptr : e;
}

[[nodiscard]] const char* modelOf(const edict_t* e)
{
    const int index = static_cast<int>(e->v.modelindex);
    return index > 0 && index < MAX_MODELS && sv.models[index] ? sv.models[index]->name : "-";
}

[[nodiscard]] float fieldByName(edict_t* e, const char* name, float fallback)
{
    const int ofs = ED_FindFieldOffset(name);
    return ofs >= 0 ? progs::fieldFloat(e, ofs) : fallback;
}

// A hand's lines: "main hand: ...", "main held: ...", ...
void describeHand(za::String& out, edict_t* player, int hand)
{
    const auto& f = progs::fields();
    const bool main = hand == HAND_MAIN;
    const char* h = main ? "main" : "off";
    const int wid = static_cast<int>(main ? player->v.weapon : progs::fieldFloatOr(player, f.weapon2, 0.f));
    const int flags = static_cast<int>(progs::fieldFloatOr(player, main ? f.weaponflags : f.weaponflags2, 0.f));
    const float clipSize = progs::fieldFloatOr(player, main ? f.weaponclipsize : f.weaponclipsize2, 0.f);
    const float ammo = main ? player->v.currentammo : progs::fieldFloatOr(player, f.currentammo2, 0.f);
    const float counter = progs::fieldFloatOr(player, main ? f.ammocounter : f.ammocounter2, 0.f);
    const int instOfs = main ? f.weaponinst : f.weaponinst2;
    edict_t* inst = instOfs >= 0 ? entityOf(progs::fieldInt(player, instOfs)) : nullptr;
    const qmodel_t* model = weapons::heldModel(hand);
    out += va("%s hand: weapon %d %s, model %s, offsets slot %d, flags %d", h, wid, weaponName(wid),
        model ? model->name : "-", weapons::heldSlot(hand), flags);
    if(inst)
    {
        out += va(", clip %.0f/%.0f", progs::fieldFloatOr(inst, f.wi_clip, 0.f), clipSize);
        if(progs::fieldFloatOr(inst, f.vr_depleted, 0.f) != 0.f)
        {
            out += ", spent (depleted)";
        }
    }
    out += va(", ammo %.0f (counter %.0f)\n", ammo, counter);

    if(edict_t* held = entityOf(progs::fieldInt(player, progs::handFields(main).held)))
    {
        out += va("%s held: %s (%s) #%d\n", h, modelOf(held), PR_GetString(held->v.classname), NUM_FOR_EDICT(held));
    }
    if(const za::String enemy = foegrab::noteHand(hand); !enemy.empty())
    {
        out += za::String{h} + " enemy: " + enemy + "\n";
    }
    if(const za::String limb = box3d::noteRagdollHold(NUM_FOR_EDICT(player), hand); !limb.empty())
    {
        out += za::String{h} + " ragdoll: " + limb + "\n";
    }
    const int fgOfs = main ? f.mainhand_fgtarget : f.offhand_fgtarget;
    if(edict_t* fg = fgOfs >= 0 ? entityOf(progs::fieldInt(player, fgOfs)) : nullptr)
    {
        const float locked = progs::fieldFloatOr(player, main ? f.mainhand_fglocked : f.offhand_fglocked, 0.f);
        const float pulled = progs::fieldFloatOr(player, main ? f.mainhand_fgpulled : f.offhand_fgpulled, 0.f);
        out += va("%s force grab: %s (%s) #%d%s%s\n", h, modelOf(fg), PR_GetString(fg->v.classname), NUM_FOR_EDICT(fg),
            locked != 0.f ? ", locked" : "", pulled != 0.f ? ", pulled" : "");
    }
}

// The monsters within nearbyMetres, the nearest first: "nearby: ..." lines.
void describeNearby(za::String& out, edict_t* player)
{
    const auto& f = progs::fields();
    const float reach = nearbyMetres * units::metresToUnits();
    const glm::vec3 from{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    za::Vector<za::Pair<float, int>> closeBy;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER))
        {
            continue;
        }
        const glm::vec3 mid{e->v.origin[0] + (e->v.mins[0] + e->v.maxs[0]) * 0.5f,
            e->v.origin[1] + (e->v.mins[1] + e->v.maxs[1]) * 0.5f, e->v.origin[2] + (e->v.mins[2] + e->v.maxs[2]) * 0.5f};
        if(const float d = glm::distance(mid, from); d <= reach)
        {
            closeBy.pushBack({d, i});
        }
    }
    za::quickSort(closeBy.begin(), closeBy.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const int agOfs = progs::fields().vr_ag;
    for(const auto& [d, i] : closeBy)
    {
        edict_t* e = EDICT_NUM(i);
        const int think = static_cast<int>(e->v.think);
        const char* thinkName =
            think > 0 && think < qcvm->progs->numfunctions ? PR_GetString(qcvm->functions[think].s_name) : "-";
        out += va("nearby: %s #%d at %.1f m, health %.0f, think %s, frame %.0f", PR_GetString(e->v.classname), i,
            d / units::metresToUnits(), e->v.health, thinkName, e->v.frame);
        if(edict_t* enemy = entityOf(e->v.enemy))
        {
            out += va(", enemy %s #%d", PR_GetString(enemy->v.classname), NUM_FOR_EDICT(enemy));
        }
        if(agOfs >= 0 && progs::fieldFloat(e, agOfs) != 0.f)
        {
            const int wid = static_cast<int>(fieldByName(e, "vr_ag_wid", -1.f));
            out += wid < 0 ? za::String{", armed: fists (disarmed)"}
                           : za::String{va(", armed: %s (%d), reserve %.0f", weaponName(wid), wid,
                                 fieldByName(e, "vr_ag_reserve", 0.f))};
        }
        if(const float k = progs::fieldFloatOr(e, f.vr_knockdown, 0.f); k != 0.f)
        {
            out += va(", knocked down (%g)", k);
        }
        if(const float c = progs::fieldFloatOr(e, f.vr_corpse, 0.f); c != 0.f)
        {
            out += c >= 2.f ? ", a corpse" : ", dying";
        }
        if(const float hold = progs::fieldFloatOr(e, f.vr_foegrab_hold, 0.f); hold > 0.f)
        {
            out += va(", held %.2f", hold);
        }
        if(progs::fieldFloatOr(e, f.vr_headless, 0.f) != 0.f)
        {
            out += ", headless";
        }
        if(const za::String rag = box3d::noteRagdoll(i); !rag.empty())
        {
            out += ", " + rag;
        }
        out += "\n";
    }
    if(closeBy.empty())
    {
        out += va("nearby: no monster within %.0f m\n", nearbyMetres);
    }
}

} // namespace

void frame()
{
    FrameTime& t = frameRing[frameNext];
    t.realtime = realtime;
    t.dt = static_cast<float>(host_rawframetime);
    frameNext = (frameNext + 1) % frameCap;
    frameCount = za::min(frameCount + 1, frameCap);
    framesSeen++;
}

void testRun(const char* scenario)
{
    lastTest = scenario ? scenario : "";
    lastTestAt = realtime;
}

void describe(za::String& out)
{
    // The checklist.
    za::String text, section;
    int item = -1;
    double when = 0.0;
    if(checklist::lastHovered(text, section, item, when))
    {
        const int mark = item < 0 ? checklist::Untested : checklist::mark(item);
        out += va("checklist: %s%s%s%s (%s%s%s), under the cursor %.0f s before the note\n", section.empty() ? "" : "[",
            section.cStr(), section.empty() ? "" : "] ", text.cStr(),
            item < 0                       ? "no longer listed"
            : mark == checklist::Passed ? "passed"
            : mark == checklist::Failed ? "failed"
                                        : "open (untested)",
            item >= 0 && *checklist::scenario(item) ? ", test scene " : "", item >= 0 ? checklist::scenario(item) : "",
            realtime - when);
    }
    else
    {
        out += "checklist: none under the cursor this session\n";
    }
    if(lastTestAt >= 0.0)
    {
        out += va("last test: %s (%.0f s before the note)\n", lastTest.cStr(), realtime - lastTestAt);
    }

    // The hands and the monsters near: the server's (a local game).
    if(sv.active && svs.maxclients >= 1 && svs.clients && svs.clients[0].active && sv.qcvm.progs &&
        sv.qcvm.num_edicts > 1)
    {
        qcvm_t* oldVm = nullptr;
        PR_PushQCVM(&sv.qcvm, &oldVm);
        edict_t* player = EDICT_NUM(1);
        describeHand(out, player, HAND_MAIN);
        describeHand(out, player, HAND_OFF);
        describeNearby(out, player);
        PR_PopQCVM(oldVm);
    }

    // The settings changed (the latest first).
    za::Vector<const Change*> order;
    for(const Change& c : changes)
    {
        order.pushBack(&c);
    }
    za::quickSort(order.begin(), order.end(), [](const Change* a, const Change* b) { return a->at > b->at; });
    for(const Change* c : order)
    {
        out += va("setting: %s %s -> %s (%d change%s, the last %.0f s before the note)%s\n", c->var->name,
            c->first.cStr(), c->latest.cStr(), c->count, c->count == 1 ? "" : "s", realtime - c->at,
            c->first == c->latest ? ", back as it was" : "");
    }
    if(changes.empty())
    {
        out += "setting: none changed this session\n";
    }
}

Logs takeLogs(double seconds)
{
    Logs l;
    l.noteAt = realtime;
    const double from = realtime - seconds;
    for(int k = 0; k < consoleCount; k++)
    {
        const ConsoleLine& c = consoleRing[(consoleNext - consoleCount + k + consoleCap) % consoleCap];
        if(c.realtime >= from)
        {
            l.console.pushBack(c);
        }
    }
    for(int k = 0; k < frameCount; k++)
    {
        const FrameTime& t = frameRing[(frameNext - frameCount + k + frameCap) % frameCap];
        if(t.realtime >= from)
        {
            l.frames.pushBack(t);
        }
    }
    return l;
}

namespace
{
// The median frame time, and the frames over twice it and over 20 ms (the hitches).
[[nodiscard]] float medianDt(const Logs& logs)
{
    za::Vector<float> dts;
    for(const FrameTime& t : logs.frames)
    {
        dts.pushBack(t.dt);
    }
    if(dts.empty())
    {
        return 0.f;
    }
    za::quickSort(dts.begin(), dts.end());
    return dts[dts.size() / 2];
}

[[nodiscard]] bool hitch(float dt, float median)
{
    return dt > 2.f * median && dt > 0.020f;
}
} // namespace

za::String frameSummary(const Logs& logs)
{
    const float median = medianDt(logs);
    int hitches = 0;
    const FrameTime* worst = nullptr;
    for(const FrameTime& t : logs.frames)
    {
        hitches += hitch(t.dt, median) ? 1 : 0;
        worst = !worst || t.dt > worst->dt ? &t : worst;
    }
    if(!worst)
    {
        return "no frames";
    }
    return va("%d frames, median %.1f ms, worst %.1f ms (%.1f s before the note), %d hitch%s (over twice the median and "
              "20 ms)",
        static_cast<int>(logs.frames.size()), median * 1000.f, worst->dt * 1000.f, logs.noteAt - worst->realtime, hitches,
        hitches == 1 ? "" : "es");
}

bool writeConsole(const za::String& path, const Logs& logs)
{
    FILE* f = fopen(path.cStr(), "wb");
    if(!f)
    {
        return false;
    }
    fputs("# Quake VR voice note: the console's last lines before the note, each with its time before it (seconds)\n", f);
    bool lineStart = true;
    for(const ConsoleLine& c : logs.console)
    {
        for(const char* p = c.text.cStr(); *p; p++)
        {
            if(lineStart)
            {
                fprintf(f, "[-%6.2f] ", logs.noteAt - c.realtime);
                lineStart = false;
            }
            // (The console's colour markers and the high half's characters, as they are: the reader strips them.)
            fputc(*p, f);
            lineStart = *p == '\n';
        }
    }
    if(!lineStart)
    {
        fputc('\n', f);
    }
    const bool ok = !ferror(f);
    return fclose(f) == 0 && ok;
}

bool writeFrames(const za::String& path, const Logs& logs)
{
    FILE* f = fopen(path.cStr(), "wb");
    if(!f)
    {
        return false;
    }
    const float median = medianDt(logs);
    fprintf(f, "# Quake VR voice note: the host frames before the note (t: seconds before it; dt: the frame time)\n# %s\n",
        frameSummary(logs).cStr());
    fputs("t,dt_ms,hitch\n", f);
    for(const FrameTime& t : logs.frames)
    {
        fprintf(f, "%.4f,%.2f,%d\n", logs.noteAt - t.realtime, t.dt * 1000.f, hitch(t.dt, median) ? 1 : 0);
    }
    const bool ok = !ferror(f);
    return fclose(f) == 0 && ok;
}

void stats(za::String& out)
{
    za::SizeT bytes = 0;
    int rows = 0, ents = 0;
    double seconds = 0.0;
    motion::replayStats(bytes, rows, ents, seconds);
    za::SizeT consoleBytes = sizeof(consoleRing);
    for(const ConsoleLine& c : consoleRing)
    {
        consoleBytes += c.text.capacity();
    }
    double rowMicros = 0.0, serverMicros = 0.0;
    motion::replayCost(rowMicros, serverMicros);
    out = va("cost: %.1f us a host frame, %.1f us a server frame; ", rowMicros, serverMicros);
    out += va("instant replay: %d rows (%.1f s), %d entity snapshots, %.2f MB; console ring %d lines %.2f MB; frame ring "
             "%d frames %.2f MB; %d settings changed",
        rows, seconds, ents, static_cast<double>(bytes) / (1024.0 * 1024.0), consoleCount,
        static_cast<double>(consoleBytes) / (1024.0 * 1024.0), frameCount,
        static_cast<double>(sizeof(frameRing)) / (1024.0 * 1024.0), static_cast<int>(changes.size()));
}

} // namespace qvr::notectx

extern "C" void VR_NoteConsolePrint(const char* text)
{
    using namespace qvr::notectx;
    if(!text || !text[0] || qvr::vr_notes.value == 0.f)
    {
        return;
    }
    ConsoleLine& c = consoleRing[consoleNext];
    c.realtime = realtime;
    c.text = text;
    consoleNext = (consoleNext + 1) % consoleCap;
    consoleCount = za::min(consoleCount + 1, consoleCap);
}

extern "C" void VR_NoteCvarChanging(cvar_s* var, const char* value)
{
    using namespace qvr::notectx;
    // Archived settings only (his), from the third frame (the config's exec before), not a take's playback's own.
    if(!var || !(var->flags & CVAR_ARCHIVE) || !var->string || framesSeen < 3 || qvr::motion::playing() ||
        qvr::motion::evaluating())
    {
        return;
    }
    for(Change& c : changes)
    {
        if(c.var == var)
        {
            c.latest = value;
            c.count++;
            c.at = realtime;
            return;
        }
    }
    Change c;
    c.var = var;
    c.first = var->string;
    c.latest = value;
    c.count = 1;
    c.at = realtime;
    changes.pushBack(ZA_MOVE(c));
}
