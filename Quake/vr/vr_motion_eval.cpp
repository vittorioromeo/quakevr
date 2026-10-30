// vr_motion_eval.cpp -- the motion recorder's evaluation (vr_motion.hpp, docs/vr-port/MOTIONS.md), moved out of
// vr_motion_play.cpp (the playback; what the two share is vr_motion_play_internal.hpp).
//
// vr_motion_eval plays many takes, each in the map loaded afresh (the same start every time: nothing
// left from the one before), and checks each one's events against quakevr/motions/expect.cfg.

#include "vr_strings.hpp"
#include "vr_geom.hpp"
#include "vr_anchor.hpp"
#include "vr_angvel.hpp"
#include "vr_avatar.hpp"
#include "vr_motion.hpp"
#include "vr_motion_take.hpp"
#include "vr_motion_play_internal.hpp"
#include "vr_motion_review.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_progs.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Replace.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/ReverseIterator.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "vr_zancle.hpp"
#include "vr_srvrandom.hpp"
#include "vr_weapon_ids.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
namespace qvr::motion
{

za::String weaponSettingsLine(); // vr_motion.cpp: the header's "weapon settings" line as it would be written now

using namespace detail;

// ----------------------------------------------------------------------------
// Evaluation
// ----------------------------------------------------------------------------

namespace
{

// An expectation (expect.cfg): see the file's comments and docs/vr-port/MOTIONS.md.
struct Expectation
{
    za::Vector<za::String> required;  // kind[/sub][@point|point] (any one of them)
    za::Vector<za::String> forbidden; // kind[/sub]
    za::Vector<za::String> allowed;   // +kind: hits of these kinds don't fail it (not required)
    bool none{false};
    za::Vector<za::String> poses;     // parry, guard: held for half the take
    za::Vector<za::String> notPoses;  // parry, guard: never, in the take
    za::Vector<za::String> weapons;   // the weapon classes it applies to (empty: any)
    bool skip{false};                   // "-": reported only
};

ankerl::unordered_dense::map<za::String, Expectation> expectations;
za::String expectPath;

[[nodiscard]] za::String underscores(za::String s)
{
    za::replace(s.begin(), s.end(), ' ', '_');
    return s;
}

void loadExpectations()
{
    expectations.clear();
    expectPath = motionsDir() + "/expect.cfg";
    za::String text;
    if(!files::readText(expectPath.cStr(), text))
    {
        Con_Printf("vr_motion_eval: no %s: every take is reported without a verdict\n", expectPath.cStr());
        return;
    }
    za::Vector<za::StringView> lines;
    files::forLines(text, [&](za::StringView l) { lines.pushBack(l); });
    for(za::StringView line : lines)
    {
        const size_t hash = line.find('#');
        if(hash != za::StringView::nPos)
        {
            line = line.substrByPosLen(0, hash);
        }
        files::Words words{line};
        za::String label, w;
        if(!(words >> label))
        {
            continue;
        }
        Expectation e;
        while(words >> w)
        {
            if(w == "-")
            {
                e.skip = true;
            }
            else if(w == "none")
            {
                e.none = true;
            }
            else if(w.rfind("!pose:", 0) == 0)
            {
                e.notPoses.emplaceBack(w.substrByPosLen(6));
            }
            else if(w[0] == '+')
            {
                e.allowed.emplaceBack(w.substrByPosLen(1));
            }
            else if(w[0] == '!')
            {
                e.forbidden.emplaceBack(w.substrByPosLen(1));
            }
            else if(w.rfind("pose:", 0) == 0)
            {
                e.poses.emplaceBack(w.substrByPosLen(5));
            }
            else if(w.rfind("weapon:", 0) == 0)
            {
                for(const za::String& c : split(w.substrByPosLen(7), '|'))
                {
                    e.weapons.pushBack(c);
                }
            }
            else
            {
                e.required.pushBack(w);
            }
        }
        expectations[label] = ZA_MOVE(e);
    }
}

// A weapon id's class, as expect.cfg names them.
[[nodiscard]] za::String weaponClass(int wid)
{
    switch(wid)
    {
        case WID_FIST: return "fist";
        case WID_GRAPPLE: return "grapple";
        case WID_AXE: return "axe";
        case WID_MJOLNIR: return "mjolnir";
        case WID_SWORD: return "sword";
        case WID_CHAINSAW: return "chainsaw"; // a melee weapon (QC VR_MTHING_SAW)
        case WID_GRUNTGUN:                    // a grunt's shotgun, an enforcer's laser rifle (QC vr_enemyguns.qc)
        case WID_ENFRIFLE: return "gun";
        case WID_CROWBAR: return "crowbar"; // a melee weapon (QC VR_MTHING_CROWBAR)
        case WID_SUPERAXE: return "superaxe"; // Dawn of the Machine's Super Axe (QC vr_mg3_weapons.qc; VR_MTHING_AXE)
        default: // (AD's soldiers' guns, the crossbow knight's; the toolgun, nail pistol, flare gun, revolver: "other", as before)
            return (wid >= WID_SHOTGUN && wid <= WID_LASER_CANNON) || (wid >= WID_ADROCKET && wid <= WID_ADCROSSBOW) ? "gun" : "other";
    }
}

[[nodiscard]] bool matchesItem(const Event& e, const za::String& item)
{
    za::String kind = item;
    za::String points;
    if(const size_t at = kind.find('@'); at != za::StringView::nPos)
    {
        points = kind.substrByPosLen(at + 1);
        kind = kind.substrByPosLen(0, at);
    }
    za::String sub;
    if(const size_t slash = kind.find('/'); slash != za::StringView::nPos)
    {
        sub = kind.substrByPosLen(slash + 1);
        kind = kind.substrByPosLen(0, slash);
    }
    if(e.kind != kind || (!sub.empty() && e.sub != sub))
    {
        return false;
    }
    if(!points.empty())
    {
        const za::Vector<za::String> list = split(points, '|');
        return za::find(list.begin(), list.end(), underscores(e.detail)) != list.end();
    }
    return true;
}

struct Result
{
    za::String path; // the take's file
    za::String file, label, weapons, expectation, verdict, reason, replayed, recorded, same;
    int frames{0};
    double err{0.0};
};

za::Vector<Result> results;

[[nodiscard]] za::String expectationText(const Expectation& e)
{
    za::String s;
    const auto add = [&](const za::String& w) { s += (s.empty() ? "" : " ") + w; };
    if(e.skip)
    {
        add("-");
    }
    if(e.none)
    {
        add("none");
    }
    for(const auto& r : e.required)
    {
        add(r);
    }
    for(const auto& f : e.forbidden)
    {
        add("!" + f);
    }
    for(const auto& a : e.allowed)
    {
        add("+" + a);
    }
    for(const auto& p : e.poses)
    {
        add("pose:" + p);
    }
    for(const auto& p : e.notPoses)
    {
        add("!pose:" + p);
    }
    if(!e.weapons.empty())
    {
        za::String w;
        for(const auto& c : e.weapons)
        {
            w += (w.empty() ? "" : "|") + c;
        }
        add("weapon:" + w);
    }
    return s;
}

// The verdict on a replay: PASS, FAIL, N/A (not for this weapon), or "-" (no expectation).
void judge(const Report& r, const Expectation* e, Result& out)
{
    const za::String main = weaponClass(r.wid[HAND_MAIN]);
    const za::String off = weaponClass(r.wid[HAND_OFF]);
    out.weapons = main + (r.wid[HAND_OFF] ? "+" + off : "");
    if(!r.ok)
    {
        out.verdict = "ERROR";
        out.reason = "the playback stopped";
        return;
    }
    if(!e || e->skip)
    {
        out.verdict = "-";
        out.reason = e ? "reported only" : "no expectation for the label";
        return;
    }
    out.expectation = expectationText(*e);
    if(!e->weapons.empty() && za::find(e->weapons.begin(), e->weapons.end(), main) == e->weapons.end() &&
       za::find(e->weapons.begin(), e->weapons.end(), off) == e->weapons.end())
    {
        out.verdict = "N/A";
        out.reason = "not for a " + main + " (expected with " + [&] {
            za::String w;
            for(const auto& c : e->weapons)
            {
                w += (w.empty() ? "" : " or ") + c;
            }
            return w;
        }() + ")";
        return;
    }

    za::Vector<za::String> fails;
    // Required (any one): an event of one of them, a hit against the target.
    if(!e->required.empty())
    {
        bool any = false;
        for(const Event& ev : r.replayed)
        {
            for(const za::String& item : e->required)
            {
                // (A hit names the classname of what it struck: the training dummy as its enemy, vr_dummy_type.)
                if(matchesItem(ev, item) && (!isHit(ev) || ev.target == r.targetClass ||
                                                    (!r.targetName.empty() && ev.target == r.targetName)))
                {
                    any = true;
                }
            }
        }
        if(!any)
        {
            fails.pushBack("no " + [&] {
                za::String w;
                for(const auto& c : e->required)
                {
                    w += (w.empty() ? "" : " or ") + c;
                }
                return w;
            }());
        }
    }
    // Forbidden: listed ones, any hit whose kind isn't required, and with none every melee event.
    for(const Event& ev : r.replayed)
    {
        bool bad = false;
        for(const za::String& item : e->forbidden)
        {
            bad = bad || matchesItem(ev, item);
        }
        if(e->none && (isHit(ev) || ev.kind == "stroke" || ev.kind == "push" || ev.kind == "deflect"))
        {
            bad = true;
        }
        if(isHit(ev) && !e->none)
        {
            bool expected = false;
            for(const za::String& item : e->required)
            {
                const za::StringView kind = item.substrByPosLen(0, item.findFirstOf("/@"));
                expected = expected || kind == ev.kind;
            }
            expected = expected || za::find(e->allowed.begin(), e->allowed.end(), ev.kind) != e->allowed.end();
            bad = bad || !expected;
        }
        if(bad)
        {
            fails.pushBack("unexpected " + eventText(ev));
        }
    }
    // Poses: held for at least half the take.
    for(const za::String& p : e->poses)
    {
        const int held = p == "parry" ? r.parryAnyFrames
                         : p == "guard" ? r.guardFrames
                                        : 0;
        if(r.recFrames == 0 || held * 2 < r.recFrames)
        {
            fails.pushBack(va("%s pose held %d of %d frames", p.cStr(), held, r.recFrames));
        }
    }
    // Poses that must never be: not a frame of the take in them.
    for(const za::String& p : e->notPoses)
    {
        const int held = p == "parry" ? r.parryAnyFrames : p == "guard" ? r.guardFrames : 0;
        if(held > 0)
        {
            fails.pushBack(va("%s pose in %d of %d frames", p.cStr(), held, r.recFrames));
        }
    }
    out.verdict = fails.empty() ? "PASS" : "FAIL";
    for(const za::String& f : fails)
    {
        out.reason += (out.reason.empty() ? "" : "; ") + f;
    }
}

enum class Eval
{
    Idle,
    Loading, // the map (re)loading
    Playing,
};

Eval evalState = Eval::Idle;
za::Vector<za::String> evalFiles;
za::Vector<za::String> evalFileOpts; // a list line's test options after its take ("<take> | at <x y z yaw> weapon <id>")
size_t evalIndex = 0;
za::String evalMap = "vrmelee-recording";
za::String evalOut;
Options evalOpts;
int evalWait = 0;
bool evalQuit = false; // quit the game when done (scripts)
double evalStart = 0.0;
za::String evalProgress; // a file told how far it is (the review's re-evaluation, vr_motion_review.cpp)

void writeProgress(const za::String& text)
{
    if(evalProgress.empty())
    {
        return;
    }
    const za::String p = files::isRelative(evalProgress) ? files::join(motionsDir(), evalProgress) : evalProgress;
    if(FILE* f = fopen(p.cStr(), "wb"))
    {
        fputs(text.cStr(), f);
        fclose(f);
    }
}

// developer 0 while a map loads (the user's value kept, and put back).
bool quiet = false;
za::String userDeveloper;
void quietLoad(bool on)
{
    cvar_t* developer = Cvar_FindVar("developer");
    if(!developer || on == quiet)
    {
        return;
    }
    quiet = on;
    if(on)
    {
        userDeveloper = developer->string;
        Cvar_SetQuick(developer, "0");
    }
    else
    {
        Cvar_SetQuick(developer, userDeveloper.cStr());
    }
}

// host_maxfps raised while evaluating: the frames' game time is the takes' own (hostFrameTime), so only the
// wall clock between them changes (the server stays at 72 Hz: host_maxfps is above 72 either way).
za::String userMaxfps;
void fastFrames(bool on)
{
    cvar_t* maxfps = Cvar_FindVar("host_maxfps");
    if(!maxfps)
    {
        return;
    }
    if(on && userMaxfps.empty())
    {
        userMaxfps = maxfps->string;
        if(maxfps->value > 72.f && maxfps->value < 1000.f)
        {
            Cvar_SetQuick(maxfps, "1000");
        }
    }
    else if(!on && !userMaxfps.empty())
    {
        Cvar_SetQuick(maxfps, userMaxfps.cStr());
        userMaxfps.clear();
    }
}

void evalDone(const Report& r)
{
    Result res;
    res.path = evalIndex < evalFiles.size() ? evalFiles[evalIndex] : r.file;
    res.file = r.file;
    if(evalIndex < evalFileOpts.size() && !evalFileOpts[evalIndex].empty())
    {
        res.file += " | " + evalFileOpts[evalIndex]; // (the same take at several spots: told apart)
    }
    za::String label = take.label;
    res.label = label;
    const Expectation* e = nullptr;
    if(const auto it = expectations.find(label); it != expectations.end())
    {
        e = &it->second;
    }
    else if(const auto jt = expectations.find(categoryOf(label)); jt != expectations.end())
    {
        e = &jt->second;
    }
    judge(r, e, res);
    res.replayed = eventsText(r.replayed, r.replayedT, true);
    res.recorded = eventsText(r.recorded, r.recordedT, true);
    const Match m = matchHits(r);
    const auto source = take.header.find("source");
    const bool synthetic = source != take.header.end() && source->second.rfind("synthetic", 0) == 0;
    res.same = synthetic ? "-" : (m.matched == m.recordedHits && m.matched == m.replayedHits ? "yes" : "no");
    res.frames = r.frames;
    res.err = r.errCount ? za::max(r.errMax[HAND_MAIN], r.errMax[HAND_OFF]) : -1.0;
    if(!r.warnings.empty())
    {
        res.reason += (res.reason.empty() ? "" : "; ") + r.warnings;
    }
    Con_Printf("%-5s %s: %s%s%s%s\n", res.verdict.cStr(), res.file.cStr(), res.replayed.cStr(),
        res.reason.empty() ? "" : " -- ", res.reason.cStr(),
        res.same == "no" ? va(" [live: %s]", res.recorded.cStr()) : "");
    results.pushBack(ZA_MOVE(res));
    evalIndex++;
    writeProgress(va("%d/%d %s", static_cast<int>(evalIndex), static_cast<int>(evalFiles.size()), r.file.cStr()));
    evalState = Eval::Loading;
    evalWait = -1;
}

void writeResults()
{
    fixedLoading = false;
    quietLoad(false);
    fastFrames(false);
    za::String path = evalOut;
    if(path.empty())
    {
        const time_t now = time(nullptr);
        char stamp[64];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
        path = motionsDir() + "/eval_" + stamp + ".csv";
    }
    else if(files::isRelative(path))
    {
        path = motionsDir() + "/" + path;
    }
    FILE* f = fopen(path.cStr(), "wb");
    int pass = 0, fail = 0, na = 0, none = 0;
    ankerl::unordered_dense::map<za::String, za::Pair<int, int>> perLabel; // passes, judged
    for(const Result& r : results)
    {
        pass += r.verdict == "PASS";
        fail += r.verdict == "FAIL" || r.verdict == "ERROR";
        na += r.verdict == "N/A";
        none += r.verdict == "-";
        if(r.verdict == "PASS" || r.verdict == "FAIL")
        {
            auto& c = perLabel[categoryOf(r.label).empty() ? r.label : categoryOf(r.label)];
            c.first += r.verdict == "PASS";
            c.second++;
        }
    }
    if(f)
    {
        fprintf(f, "# vr_motion_eval: %d takes, %d pass, %d fail, %d n/a, %d without an expectation (%s, %s)\n",
            static_cast<int>(results.size()), pass, fail, na, none, evalMap.cStr(), expectPath.cStr());
        fprintf(f, "file,label,weapons,expected,verdict,reason,events,recorded_events,same_hits_as_recorded,frames,hand_error_u\n");
        const auto cell = [](za::String s) {
            za::replace(s.begin(), s.end(), ',', ' ');
            return s;
        };
        for(const Result& r : results)
        {
            fprintf(f, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%s\n", cell(r.file).cStr(), cell(r.label).cStr(), r.weapons.cStr(),
                cell(r.expectation).cStr(), r.verdict.cStr(), cell(r.reason).cStr(), cell(r.replayed).cStr(),
                cell(r.recorded).cStr(), r.same.cStr(), r.frames, r.err >= 0.0 ? va("%.3f", r.err) : "");
        }
        fclose(f);
    }
    Con_Printf("vr_motion_eval: %d takes: %d pass, %d fail, %d n/a, %d without an expectation (%.0f s)\n",
        static_cast<int>(results.size()), pass, fail, na, none, Sys_DoubleTime() - evalStart);
    int same = 0, compared = 0;
    for(const Result& r : results)
    {
        compared += r.same == "yes" || r.same == "no";
        same += r.same == "yes";
    }
    if(compared)
    {
        Con_Printf("vr_motion_eval: %d of %d replays hit as their takes did live (the same hits: kind, sub, hand, target, point)\n",
            same, compared);
    }
    for(const auto* entry : qza::sortedByKey(perLabel)) // (in the labels' order, as a std::map had them)
    {
        const auto& [label, c] = *entry;
        Con_Printf("  %-24s %d/%d\n", label.cStr(), c.first, c.second);
    }
    Con_Printf("vr_motion_eval: the table is %s\n", f ? path.cStr() : "(could not be written)");

    // Each take's verdict next to it (eval_status.csv: what Review Takes lists), from an evaluation as the takes are
    // judged: the current melee settings, the takes' own rate, the melee recording room.
    bool lineOpts = false;
    for(const za::String& s : evalFileOpts)
    {
        lineOpts = lineOpts || !s.empty();
    }
    if(evalOpts.rate > 0.f || evalOpts.recorded || evalOpts.atSet || evalOpts.weapon >= 0 || lineOpts || q_strcasecmp(evalMap.cStr(), "vrmelee-recording") != 0)
    {
        Con_Printf("vr_motion_eval: (not the takes' verdicts: rate, recorded, another map, at or weapon; eval_status.csv "
                   "unchanged)\n");
    }
    else
    {
        za::Vector<review::Verdict> verdicts;
        for(const Result& r : results)
        {
            verdicts.pushBack({r.path, r.label, r.weapons, r.expectation, r.verdict, r.reason, r.replayed, r.recorded,
                r.same, r.frames, r.err});
        }
        review::recordEval(verdicts);
    }
    writeProgress(va("done %d", static_cast<int>(results.size())));
}

} // namespace

namespace detail
{

void evalFrame()
{
    if(evalState == Eval::Idle || state != State::Idle)
    {
        return;
    }
    if(evalState == Eval::Loading)
    {
        if(evalWait < 0)
        {
            if(evalIndex >= evalFiles.size())
            {
                evalState = Eval::Idle;
                writeResults();
                if(evalQuit)
                {
                    // (A few frames after the disconnect: quit in a game asks first.)
                    Cbuf_AddText("disconnect\nwait\nwait\nwait\nwait\nwait\nquit\n");
                }
                return;
            }
            // Each take in the map loaded afresh: the same start every time. Quietly (developer 0: a load's
            // thousands of "can't find" lines for textures cost seconds); the take plays at the user's.
            quietLoad(true);
            fixedLoading = true;
            srand(1);
            qvr::srvrandom::seedNextMap(1u); // (the map's QC random(): its spawns' draws)
            Cbuf_AddText(va("map %s\n", evalMap.cStr()));
            evalWait = 0;
            return;
        }
        if(!sv.active || cls.signon != SIGNONS || !hands::current().valid)
        {
            evalWait = 0;
            return;
        }
        if(++evalWait < 20)
        {
            return;
        }
        evalState = Eval::Playing;
        quietLoad(false);
        fixedLoading = false;
        onDone = evalDone;
        Options o = evalOpts;
        if(evalIndex < evalFileOpts.size())
        {
            applyLineOptions(evalFileOpts[evalIndex].cStr(), o);
        }
        if(!startPlayback(evalFiles[evalIndex], o))
        {
            Result res;
            res.path = evalFiles[evalIndex];
            res.file = za::String{files::fileName(evalFiles[evalIndex])};
            res.verdict = "ERROR";
            res.reason = "could not play";
            results.pushBack(res);
            evalIndex++;
            evalState = Eval::Loading;
            evalWait = -1;
        }
    }
}

} // namespace detail

namespace
{

// The takes a vr_motion_eval argument names: a folder (its .csv takes), a pattern (in motions/, or
// in its folder), or a take; by default every take in motions/.
[[nodiscard]] za::Vector<za::String> collectTakes(const za::String& arg)
{
    za::Vector<za::String> out;
    const auto isTake = [](za::StringView name) {
        const za::StringView dot = name.substrByPosLen(za::min(name.rfind('.'), name.size()));
        return dot == ".csv" && name.rfind('.') != 0 && !name.startsWith("eval_");
    };
    const auto folder = [&](const za::String& dir, const za::StringView* pattern) {
        files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
            if(!isDirectory && isTake(name) && (!pattern || files::globMatch(*pattern, name)))
            {
                out.pushBack(files::join(dir, name));
            }
        });
    };
    if(arg.empty())
    {
        folder(motionsDir(), nullptr);
    }
    else
    {
        const za::String bases[] = {arg, files::join(motionsDir(), arg), files::join(com_gamedir, arg)};
        bool found = false;
        for(const za::String& b : bases)
        {
            if(files::isDirectory(b.cStr()))
            {
                folder(b, nullptr);
                found = true;
                break;
            }
            if(files::isFile(b.cStr()))
            {
                out.pushBack(b);
                found = true;
                break;
            }
        }
        if(!found)
        {
            const za::String dir{files::parentPath(arg)};
            const za::StringView pattern = files::fileName(arg); // (a glob: * and ?, either case)
            for(const za::String& d : {dir, files::join(motionsDir(), dir), files::join(com_gamedir, dir)})
            {
                if(!d.empty() && files::isDirectory(d.cStr()))
                {
                    folder(d, &pattern);
                    if(!out.empty())
                    {
                        break;
                    }
                }
            }
        }
    }
    za::quickSort(out.begin(), out.end());
    return out;
}

} // namespace

namespace detail
{

// vr_motion_eval [<folder, pattern or take>] [list <file>] [map <name>] [out <file>] [rate <hz> [jitter <fraction>]] [save] [recorded] [verbose]
// [watch] [progress <file>] [at <x> <y> <z> <yaw>] [weapon <id>] [quit];
// vr_motion_eval stop
void eval_f()
{
    if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "stop"))
    {
        if(evalState != Eval::Idle)
        {
            evalState = Eval::Idle;
            stopPlayback("vr_motion_eval stop");
            writeResults();
        }
        return;
    }
    if(evalState != Eval::Idle || state != State::Idle)
    {
        Con_Printf("vr_motion_eval: busy (vr_motion_eval stop)\n");
        return;
    }
    const Backend* be = backend();
    if((!be || strcmp(be->name(), "mock") != 0) && backendRestartPending())
    {
        // Straight from a start-up script ("vr_backend mock;vr_motion_eval ..."): the backend starts with the next
        // frame (VR_BeginFrame). The command again then, first in the buffer (before quake.rc's vr_startgame).
        Cbuf_InsertText(va("wait\n%s %s\n", Cmd_Argv(0), Cmd_Args() ? Cmd_Args() : ""));
        return;
    }
    if(!be || strcmp(be->name(), "mock") != 0)
    {
        Con_Printf("vr_motion_eval: plays in the mock headset only (vr_backend mock)\n");
        return;
    }
    za::String arg, list;
    evalOpts = Options{};
    evalProgress.clear();
    evalOpts.quiet = true;
    // (vr_motion_eval_rate, _jitter, _stutter: the defaults of its rate, jitter and stutter options: an eval script's
    // takes replayed at another headset's rate, eval.sh's EVAL_PRE.)
    evalOpts.rate = vr_motion_eval_rate.value;
    evalOpts.jitter = vr_motion_eval_jitter.value;
    evalOpts.stutter = static_cast<int>(vr_motion_eval_stutter.value);
    evalMap = "vrmelee-recording";
    evalOut.clear();
    evalQuit = false;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        const char* a = Cmd_Argv(i);
        if(!q_strcasecmp(a, "map") && i + 1 < Cmd_Argc())
        {
            evalMap = Cmd_Argv(++i);
        }
        else if(!q_strcasecmp(a, "out") && i + 1 < Cmd_Argc())
        {
            evalOut = Cmd_Argv(++i);
        }
        else if(!q_strcasecmp(a, "rate") && i + 1 < Cmd_Argc())
        {
            evalOpts.rate = Q_atof(Cmd_Argv(++i));
        }
        else if(!q_strcasecmp(a, "jitter") && i + 1 < Cmd_Argc())
        {
            evalOpts.jitter = Q_atof(Cmd_Argv(++i));
        }
        else if(!q_strcasecmp(a, "stutter") && i + 1 < Cmd_Argc())
        {
            evalOpts.stutter = Q_atoi(Cmd_Argv(++i));
        }
        else if(!q_strcasecmp(a, "save"))
        {
            evalOpts.save = true;
        }
        else if(!q_strcasecmp(a, "recorded"))
        {
            evalOpts.recorded = true;
        }
        else if(!q_strcasecmp(a, "watch"))
        {
            evalOpts.watch = true;
        }
        else if(!q_strcasecmp(a, "verbose"))
        {
            evalOpts.quiet = false;
        }
        else if(!q_strcasecmp(a, "quit"))
        {
            evalQuit = true;
        }
        else if(!q_strcasecmp(a, "list") && i + 1 < Cmd_Argc())
        {
            list = Cmd_Argv(++i);
        }
        else if(!q_strcasecmp(a, "progress") && i + 1 < Cmd_Argc())
        {
            evalProgress = Cmd_Argv(++i);
        }
        else if(parseTestOption(i, evalOpts))
        {
        }
        else if(arg.empty())
        {
            arg = a;
        }
        else
        {
            Con_Printf("vr_motion_eval: unknown option \"%s\"\n", a);
            return;
        }
    }
    if(!list.empty())
    {
        // A file of takes, a path a line (relative: to motions/): the review's Re-evaluate.
        const za::String lp = files::isRelative(list) ? files::join(motionsDir(), list) : list;
        za::String text;
        if(!files::readText(lp.cStr(), text, files::Mode::Binary))
        {
            Con_Printf("vr_motion_eval: can't read %s\n", lp.cStr());
            return;
        }
        evalFiles.clear();
        evalFileOpts.clear();
        files::forLines(text, [](za::StringView line) {
            while(line.endsWith('\r') || line.endsWith(' '))
            {
                line.removeSuffix(1);
            }
            // "<take> | <test options>" (Options' at, weapon): that take with them.
            za::StringView lineOpts;
            if(const size_t bar = line.find('|'); bar != za::StringView::nPos)
            {
                lineOpts = line.substrByPosLen(bar + 1);
                line = line.substrByPosLen(0, bar);
                while(line.endsWith(' '))
                {
                    line.removeSuffix(1);
                }
                while(lineOpts.startsWith(' '))
                {
                    lineOpts.removePrefix(1);
                }
            }
            if(line.empty())
            {
                return;
            }
            const za::String p = files::isRelative(line) ? files::join(motionsDir(), line) : za::String{line};
            if(files::isFile(p.cStr()))
            {
                evalFiles.pushBack(p);
                evalFileOpts.pushBack(za::String{lineOpts});
            }
            else
            {
                Con_Printf("vr_motion_eval: no take %s (left out)\n", p.cStr());
            }
        });
    }
    else
    {
        evalFiles = collectTakes(arg);
        evalFileOpts.clear();
    }
    if(evalFiles.empty())
    {
        Con_Printf("vr_motion_eval: no takes in \"%s\"\n", arg.empty() ? motionsDir().cStr() : arg.cStr());
        return;
    }
    loadExpectations();
    results.clear();
    evalIndex = 0;
    evalState = Eval::Loading;
    evalWait = -1;
    evalStart = Sys_DoubleTime();
    if(!evalOpts.watch)
    {
        fastFrames(true);
    }
    Con_Printf("vr_motion_eval: %d takes, each in %s\n", static_cast<int>(evalFiles.size()), evalMap.cStr());
}

// vr_motion_header_check: the weapon anchors through a take's header and back (the "weapon settings" line as written now,
// read as a playback reads it: each the same text), and an old take's vertex index and a broken value (their defaults).
void headerCheck_f()
{
    za::Vector<za::Pair<za::String, za::String>> list;
    collectSettings(weaponSettingsLine(), false, list);
    int anchors = 0, wrong = 0;
    const cvar_t* some = nullptr;
    for(auto& [name, value] : list)
    {
        const cvar_t* var = anchorSetting(name) ? Cvar_FindVar(name.cStr()) : nullptr;
        if(!var)
        {
            continue;
        }
        anchors++;
        some = var;
        if(!decodeAnchor(*var, value) || strcmp(value.cStr(), var->string) != 0)
        {
            if(wrong++ == 0)
            {
                Con_Printf("vr_motion_header_check: %s \"%s\" read back as \"%s\"\n", name.cStr(), var->string, value.cStr());
            }
        }
    }
    int oldOk = 0;
    if(some)
    {
        for(const char* old : {"614", "30.3762", ""})
        {
            za::String value = old;
            oldOk += !decodeAnchor(*some, value) && value == some->default_string;
        }
    }
    Con_Printf("vr_motion_header_check: %d anchors, %d read back wrong; old or broken values to the default: %d of 3 (%s)\n",
        anchors, wrong, oldOk, anchors > 0 && wrong == 0 && oldOk == 3 ? "PASS" : "FAIL");
}

} // namespace detail

bool evaluating()
{
    return evalState != Eval::Idle;
}

} // namespace qvr::motion
