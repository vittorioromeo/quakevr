// canonical: no NEW copies of the helper families the refactor made canonical.
//
// docs/vr-port/CODE_STYLE.md ("Canonical helpers") and docs/vr-port/ARCHITECTURE.md ("Where to put new code") name one
// implementation per helper. This check counts, per file, the code that copies or bypasses one (a local `vec(const
// float*)` or `wrap180`, a new `rnd` wrapper, a direct `->haptic(` call, `PR_PushQCVM` outside `progs::ServerVm`, a
// QC `find(world, classname, "player")`, ...) and fails when a file has more than Misc/quakevr/canonical_allowlist.txt
// allows. The allowlist holds today's sites: the canonical implementations themselves and the ones the refactor left
// on purpose (ROUND23, "left" lines). Comments are ignored; string literals are kept.
//
//   qvr_checks canonical              check (the kit's build.sh runs it; exit 1 on a new copy)
//   qvr_checks canonical --list FAM   every current site of one family (file:line: text)
//   qvr_checks canonical --tighten    lower the allowlist to today's counts (after migrating sites)
//
// A new site that really is meant (a different rule, a test of the engine itself): add or raise its allowlist line by
// hand, with the reason after `#`. Never raise a count just to pass the build.
#include "common.hpp"
#include "maps.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/Pair.hpp"

#include <regex>

namespace qvrc
{
namespace
{

constexpr const char* allowlistPath = "Misc/quakevr/canonical_allowlist.txt";

// One pattern of a family. ECMAScript; `lineStart`: Python's `(?m)^` before it (tried at each line's start only);
// `first`: the characters a match can start with (the others are skipped; empty: any).
struct Pattern
{
    za::String re;
    bool lineStart = false;
    za::String first;
};

// A cheap test before the family's patterns: a file without any of these has no site. A literal, optionally a whole
// word at its start, and optionally followed by spaces and `then`.
struct Needle
{
    za::Vector<za::String> lits;
    bool wordStartOnly = false;
    bool wordEndOnly = false;
    char then = 0;
};

struct Family
{
    za::String id;
    za::String lang;
    za::Vector<Pattern> patterns;
    Needle needle;
    za::String use;
};

za::String alt(const za::Vector<za::String>& names)
{
    za::String s;
    for(const auto& n : names)
    {
        if(!s.empty())
        {
            s += '|';
        }
        s += n;
    }
    return s;
}

// A C++ definition (or declaration) of NAME: a return type then the name and `(`, not a call after return/else/...;
// and the same as a lambda: `auto NAME = [...](`.
za::Vector<Pattern> cppDef(const za::Vector<za::String>& names, const za::String& params = "")
{
    const za::String a = "(?:" + alt(names) + ")";
    return {
        {R"([ \t]*(?:\[\[\w+\]\][ \t]*)*(?:(?:static|inline|constexpr|const|extern|friend)\s+)*)"
         R"((?!return\b|else\b|case\b|new\b|delete\b|throw\b|co_return\b|goto\b)\w[\w:<>]*[ \t*&]+(?:)" +
             a + R"()\s*\()" + params,
         true, ""},
        {R"(\bauto\s+(?:)" + a + R"()\s*=\s*\[[^\]\n]*\]\s*\()" + params, false, "a"},
    };
}

za::Vector<Family> families()
{
    const za::Vector<za::String> geomMath = {"safeNormalize", "wrap180", "wrapYaw", "wrapDegrees", "wrapAngle",
                                             "smooth01", "smoothstep", "smoothStep", "approachExp", "yawDelta",
                                             "yawOf", "closestOnSegment", "segmentDistance", "closestOnTriangle",
                                             "nearestOnTri", "quakeFromTracking", "angleBetween"};
    const za::Vector<za::String> rnd = {"rnd", "rnd01", "random01", "randomFloat", "frand", "randf", "rand01"};
    const za::Vector<za::String> strs = {"startsWith", "endsWith", "lower", "toLower", "trim", "appendf",
                                         "nearestNormal", "quantile", "percentile", "quantileSorted"};
    const za::Vector<za::String> mathCopy = {"Cross", "Wrap180", "Wrap", "AngleNorm", "SegDist", "SegmentDist",
                                             "Lerp", "Nlerp", "Clamp", "Smooth", "Ease", "clamp"};
    return {
        {"geom-vec", "cpp", cppDef({"vec", "toGlm", "toVec"}, R"(\s*const\s+(?:float\s*\*|vec3_t))"),
         {{"vec", "toGlm", "toVec"}, true, true}, "geom::vec / geom::store (vr_geom.hpp)"},
        {"geom-math", "cpp", cppDef(geomMath), {geomMath, true, true}, "geom:: (vr_geom.hpp)"},
        {"rnd-wrapper", "cpp", cppDef(rnd), {rnd, true, true},
         "vrand::range / vrand::xorshift* / vrand::lcg* (vr_random.hpp)"},
        {"strings-copy", "cpp", cppDef(strs), {strs, true, true},
         "strings:: (vr_strings.hpp), mdlbuild::nearestNormal, vrstats::quantileSorted"},
        {"haptic-direct", "cpp", {{R"((?:->|\.)\s*haptic\s*\()", false, "-."}}, {{"haptic"}},
         "haptics::pulse(hand, seconds, amplitude, hz) (vr_backend.hpp)"},
        {"qcvm-push", "cpp", {{R"(\bPR_PushQCVM\s*\()", false, "P"}}, {{"PR_PushQCVM"}},
         "progs::ServerVm vm; (vr_progs.hpp)"},
        {"player-edict", "cpp",
         {{R"(\bEDICT_NUM\s*\(\s*1\s*\))", false, "E"}, {R"(\bsvs\.clients\s*\[\s*0\s*\]\s*\.edict\b)", false, "s"}},
         {{"EDICT_NUM", "svs.clients"}}, "progs::localPlayer() / progs::playerEdict() (vr_progs.hpp)"},
        {"cvar-by-name", "cpp", {{R"(\bCvar_VariableValue\s*\(\s*"vr_)", false, "C"}}, {{"Cvar_VariableValue"}},
         "the cvar_t directly: vr_x.value"},
        // The QC reads a cvar or tunable by handle (QC/vr_cvars.qc; qvr_checks cvars_qc makes every used cvarh_<name>),
        // not by cvar("<name>"): a name lookup each call, and a misspelt or retired name nothing checks (QC/ad aside).
        {"qc-cvar-by-name", "qc", {{R"(\bcvar\s*\()", false, "c"}}, {{"cvar"}, true, false, '('},
         "cvar_hget(cvarh_<name>) (QC/vr_cvars.qc; run build/qvr_checks/qvr_checks.exe cvars_qc)"},
        {"field-by-name", "cpp", {{R"(\bED_FindFieldOffset\s*\(\s*")", false, "E"}}, {{"ED_FindFieldOffset"}},
         "progs::fields().x (add the field to vr_fields.inc)"},
        {"qc-find-player", "qc", {{R"(\bfind\s*\(\s*world\s*,\s*classname\s*,\s*"player"\s*\))", false, "f"}},
         {{"\"player\""}}, "VR_Player() / VR_NextPlayer(p) (QC/vr_util.qc)"},
        {"qc-math-copy", "qc",
         {{R"([ \t]*\w+\s*\([^)]*\)\s*(?:\w+_(?:Cross|Wrap180|Wrap|AngleNorm|SegDist|SegmentDist|)"
           R"(Lerp|Nlerp|Clamp|Smooth|Ease)|clamp)\s*=)",
           true, ""}},
         {mathCopy, false, false, '='},
         "VR_Cross, VR_Wrap180, clamp, VR_Lerp, VR_Smooth, VR_SegmentDistance(Eps) (QC/vr_util.qc)"},
        {"qc-test-check", "qc",
         {{R"([ \t]*void\s*\(\s*(?:float\s+\w+\s*,\s*string\s+\w+|string\s+\w+\s*,\s*float\s+\w+))"
           R"(\s*\)\s*\w+_Check\s*=)",
           true, ""}},
         {{"_Check"}, false, false, '='}, "VR_Test_Begin/Check/Verdict (QC/vr_testutil.qc) or the VR_AGT_* harness"},
        // The tests' one harness (docs/vr-port/TESTING.md, "Writing a test"; design review 7): no new hand-made
        // tallies, verdict lines or library copies.
        {"qc-test-tally", "qc", {{R"([ \t]*float\s+\w*_(?:pass|passed|fail|failed|fails)\s*;)", true, ""}},
         {{"_pass", "_fail"}}, "VR_Test_Begin(Flags)/Check/Verdict's tally (QC/vr_testutil.qc)"},
        {"qc-test-verdict", "qc", {{R"("[^"\n]*%g passed,? %g failed)", false, "\""}}, {{"%g passed"}},
         "VR_Test_Verdict (QC/vr_testutil.qc)"},
        {"cpp-test-verdict", "cpp", {{R"("[^"\n]*passed,? %(?:d|i|u|zu) failed)", false, "\""}}, {{"passed"}},
         "testkit::Tally / testkit::verdict (vr_testkit.hpp)"},
        {"py-test-copy", "py", {{R"(def verdict\()", true, ""}, {R"("C:/OHWorkspace/qvr-kit")", false, "\""}},
         {{"def verdict(", "qvr-kit"}}, "qvrtest.Tally / qvrtest.KIT (Misc/quakevr/qvrtest.py)"},
        {"sh-test-copy", "sh", {{R"([ \t]*check\s*\(\s*\)\s*\{)", true, ""}}, {{"check"}},
         "source qvrtest.sh: check / verdict (Misc/quakevr/qvrtest.sh)"},
        // The impact table (design review 4; ROUND23, "One impact-effect table"): a hit's blood, sparks or splinters
        // and what the thing is made of come from one row, not a new hand-built ladder.
        {"qc-hit-fx", "qc", {{R"(\bparticle2\s*\([^;\n]*QVR_PARTICLE_PRESET_(?:BLOOD|SPARKS|WOODDUST)\b)", false, "p"}},
         {{"particle2"}}, "VR_ImpactFx / SpawnBlood / VR_HitBlood (QC/vr_crates.qc; the rows: Quake/vr/vr_impacts.cpp)"},
        {"qc-material-ladder", "qc", {{R"(\b(?:VR_Hit_Metal|VR_Crate_Wooden|VR_Hit_Armoured)\s*\()", false, "V"}},
         {{"VR_Hit_Metal", "VR_Crate_Wooden", "VR_Hit_Armoured"}},
         "VR_MaterialOf (QC/vr_crates.qc) and the impact table's row"},
        {"qc-test-spawn", "qc", {{R"([ \t]*func_enemy_dispenser_use\s*\(\s*\)\s*;)", true, ""}},
         {{"func_enemy_dispenser_use"}}, "VR_Test_Spawn (QC/vr_testutil.qc): the tests' one spawner, impulse 241's"},
        // One "is this hand free to grab?" (ROUND23, "One busy-hand check"): a grab's start asks VR_Hand_Free with its
        // exceptions (VR_HANDFREE_*), not its own mix of the force grab's lock, the slot's busy kinds or the
        // flashlight's.
        {"qc-busy-hand", "qc",
         {{R"(\bVR_Forcegrab_IsHandBusy\s*\()", false, "V"},
          {R"(\bVR_Hand_Kind\s*\([^()]*\)\s*[!=]=\s*VR_HOLD_(?:NONE|FLASHLIGHT)\b)", false, "V"},
          {R"(\bVR_Hand_IsBusyKind\s*\()", false, "V"},
          {R"(\bVR_Hand_CanTake(?:From)?\s*\()", false, "V"}},
         {{"VR_Forcegrab_IsHandBusy", "VR_Hand_Kind", "VR_Hand_IsBusyKind", "VR_Hand_CanTake"}},
         "VR_Hand_Free(player, hand, VR_HANDFREE_* exceptions) (QC/vr_wpnforcegrab.qc)"},
        // QC/C++ mirrors from one source (design review 10): a QC constant's value is not copied into the engine as a
        // literal next to its QC name (a table row "VR_X", {1.f, 2.f, 3.f} or "VR_X", 0.5f), it is a #define both
        // include.
        {"cpp-qc-constant", "cpp",
         {{R"re("(?:VR|QVR)_[A-Z][A-Z0-9_]*"\s*,\s*(?:glm::vec3\s*)?[{(]?\s*-?\.?[0-9])re", false, "\""}},
         {{"\"VR_", "\"QVR_"}}, "a #define in a Quake/vr/*_shared.h that the QC #includes too (vr_agpose_shared.h VR_AGPS_*)"},
        // Rendering stays multiview-ready (CODE_STYLE.md, "Rendering stays multiview-ready"; STEREO.md): a GL program
        // made outside gl_shaders.c's GL_CreateProgram has no multiview twin, and vr_multiview_check cannot see it.
        {"gl-program-no-twin", "cpp", {{R"(\b(?:glProgram|linkProgram|buildProgram|fsrProgram)\s*\()", false, "glbf"}},
         {{"Program"}, false, false, '('},
         "a program in gl_shaders.c (GL_CreateProgram: its twin is made and checked by vr_multiview_check); a compute "
         "or per-eye-for-good program: its allowlist line, with why"},
    };
}

bool needleHit(const Needle& nd, za::StringView text)
{
    for(const auto& lit : nd.lits)
    {
        for(za::SizeT i = 0; (i = text.find(lit, i)) != npos; ++i)
        {
            if(nd.wordStartOnly && !wordStart(text, i))
            {
                continue;
            }
            if(nd.wordEndOnly && !wordEnd(text, i + lit.size()))
            {
                continue;
            }
            if(nd.then)
            {
                const za::SizeT p = skipSpace(text, i + lit.size());
                if(p >= text.size() || text[p] != nd.then)
                {
                    continue;
                }
            }
            return true;
        }
    }
    return false;
}

// Comments become spaces (newlines kept, so line numbers hold); string and char literals stay.
za::String stripComments(za::StringView t)
{
    za::String o{t};
    za::SizeT i = 0;
    const za::SizeT n = t.size();
    while(i < n)
    {
        const char c = t[i];
        if(c == '"' || c == '\'')
        {
            za::SizeT j = i + 1;
            bool closed = false;
            while(j < n && t[j] != '\n')
            {
                if(t[j] == '\\')
                {
                    if(j + 1 >= n)
                    {
                        break;
                    }
                    j += 2;
                    continue;
                }
                if(t[j] == c)
                {
                    closed = true;
                    break;
                }
                ++j;
            }
            i = closed ? j + 1 : i + 1;
        }
        else if(c == '/' && i + 1 < n && t[i + 1] == '/')
        {
            while(i < n && t[i] != '\n')
            {
                o[i++] = ' ';
            }
        }
        else if(c == '/' && i + 1 < n && t[i + 1] == '*')
        {
            const za::SizeT e = t.find("*/", i + 2);
            if(e == npos)
            {
                ++i;
                continue;
            }
            for(za::SizeT k = i; k < e + 2; ++k)
            {
                if(o[k] != '\n')
                {
                    o[k] = ' ';
                }
            }
            i = e + 2;
        }
        else
        {
            ++i;
        }
    }
    return o;
}

// The start of every match of a family's patterns, in order and once each (Python's finditer per pattern, the starts
// merged).
za::Vector<za::SizeT> matches(const za::Vector<std::regex>& res, const za::Vector<Pattern>& pats, za::StringView text)
{
    za::Vector<za::SizeT> hits;
    for(za::SizeT p = 0; p < pats.size(); ++p)
    {
        const auto& pat = pats[p];
        za::SizeT next = 0; // finditer: the next match starts at or after the last one's end
        auto tryAt = [&](za::SizeT i) {
            std::cmatch m;
            auto flags = std::regex_constants::match_continuous;
            if(i > 0)
            {
                flags |= std::regex_constants::match_prev_avail;
            }
            if(std::regex_search(text.data() + i, text.data() + text.size(), m, res[p], flags))
            {
                hits.pushBack(i);
                next = i + za::max<za::SizeT>(1, static_cast<za::SizeT>(m.length(0)));
                return true;
            }
            return false;
        };
        if(pat.lineStart)
        {
            for(za::SizeT i = 0; i < text.size();)
            {
                if(i >= next)
                {
                    tryAt(i);
                }
                const za::SizeT nl = text.find('\n', i);
                if(nl == npos)
                {
                    break;
                }
                i = nl + 1;
            }
        }
        else
        {
            for(za::SizeT i = 0; i < text.size();)
            {
                if(!pat.first.empty())
                {
                    i = text.findFirstOf(pat.first, i);
                    if(i == npos)
                    {
                        break;
                    }
                }
                if(i >= next)
                {
                    tryAt(i);
                }
                ++i;
            }
        }
    }
    za::quickSort(hits.begin(), hits.end(), [](za::SizeT a, za::SizeT b) { return a < b; });
    za::Vector<za::SizeT> unique;
    for(const za::SizeT h : hits)
    {
        if(unique.empty() || unique.back() != h)
        {
            unique.pushBack(h);
        }
    }
    return unique;
}

struct Site
{
    int line;
    za::String text;
};

using Sites = Map<za::String, za::Vector<Site>>; // path -> sites
using Found = Map<za::String, Sites>;            // family -> its sites

za::Vector<za::String> globPosix(const Ctx& ctx, const char* dir, const za::Vector<za::StringView>& exts, bool recursive)
{
    za::Vector<za::String> out;
    for(const auto& p : listFiles(ctx.tree / dir, exts, recursive))
    {
        out.pushBack(relPosix(p, ctx.tree));
    }
    return out;
}

Found scan(const Ctx& ctx, const za::Vector<Family>& fams)
{
    Map<za::String, za::Vector<za::String>> files;
    {
        za::Vector<za::String> cpp;
        for(auto& p : globPosix(ctx, "Quake/vr", {".cpp", ".hpp", ".h", ".inc", ".c"}, true))
        {
            if(p.find("/external/") == npos)
            {
                cpp.pushBack(ZA_MOVE(p));
            }
        }
        sortStrings(cpp);
        za::Vector<za::String> qc;
        for(auto& p : globPosix(ctx, "QC", {".qc"}, false))
        {
            if(p != "QC/vr_cvars_gen.qc") // (generated: qvr_checks cvars_qc)
            {
                qc.pushBack(ZA_MOVE(p));
            }
        }
        sortStrings(qc);
        auto misc = [&](const char* ext) {
            za::Vector<za::String> v;
            for(auto& p : globPosix(ctx, "Misc/quakevr", {ext}, true))
            {
                if(!endsWith(p, "/qvrtest.py") && !endsWith(p, "/qvrtest.sh"))
                {
                    v.pushBack(ZA_MOVE(p));
                }
            }
            sortStrings(v);
            return v;
        };
        files["cpp"] = ZA_MOVE(cpp);
        files["qc"] = ZA_MOVE(qc);
        files["py"] = misc(".py");
        files["sh"] = misc(".sh");
    }
    // Every file's text and comment-stripped text, once (read in parallel by the families).
    Map<za::String, za::Pair<za::String, za::String>> texts;
    for(const auto& [lang, list] : files)
    {
        for(const auto& p : list)
        {
            (void)texts[p];
        }
    }
    {
        // (the map is complete: its values stay where they are while the threads fill them)
        za::Vector<za::Pair<const za::String*, za::Pair<za::String, za::String>*>> jobs;
        for(auto& [p, t] : texts)
        {
            jobs.pushBack({&p, &t});
        }
        const unsigned nThreads = za::max(1u, za::min(16u, za::Thread::hardwareConcurrency()));
        za::Vector<za::Thread> pool;
        for(unsigned w = 0; w < nThreads; ++w)
        {
            pool.emplaceBack([&, w] {
                for(za::SizeT k = w; k < jobs.size(); k += nThreads)
                {
                    jobs[k].second->first = readText(ctx.tree / toPath(*jobs[k].first));
                    jobs[k].second->second = stripComments(jobs[k].second->first);
                }
            });
        }
        for(auto& t : pool)
        {
            t.join();
        }
    }
    Found found;
    za::Vector<Sites> perFamily;
    perFamily.resize(fams.size());
    za::Vector<za::Thread> pool;
    for(za::SizeT f = 0; f < fams.size(); ++f)
    {
        pool.emplaceBack([&, f] {
            const auto& fam = fams[f];
            za::Vector<std::regex> res;
            for(const auto& p : fam.patterns)
            {
                res.emplaceBack(p.re.data(), p.re.size(), std::regex::ECMAScript | std::regex::optimize);
            }
            for(const auto& path : files.at(fam.lang))
            {
                const auto& [raw, text] = texts.at(path);
                if(!needleHit(fam.needle, raw))
                {
                    continue;
                }
                for(const za::SizeT pos : matches(res, fam.patterns, text))
                {
                    const za::SizeT before = pos == 0 ? npos : text.rfind('\n', pos - 1);
                    const za::SizeT ls = before == npos ? 0 : before + 1;
                    const za::SizeT le = text.find('\n', pos);
                    const auto lineText = text.substrByPosLen(ls, le == npos ? npos : le - ls);
                    perFamily[f][path].pushBack({lineAt(text, pos), za::String{trim(lineText)}});
                }
            }
        });
    }
    for(auto& t : pool)
    {
        t.join();
    }
    for(za::SizeT f = 0; f < fams.size(); ++f)
    {
        found[fams[f].id] = ZA_MOVE(perFamily[f]);
    }
    return found;
}

// An allowlist line's key: the family and the path (neither has a space).
za::String allowKey(za::StringView fam, za::StringView path)
{
    za::String k{fam};
    k += ' ';
    k += path;
    return k;
}

} // namespace

int checkCanonical(const Ctx& ctx, const Args& args, Out& out)
{
    const auto fams = families();
    const Found found = scan(ctx, fams);
    Map<za::String, int> allowed; // allowKey -> count
    za::Vector<za::String> lines;
    bool have = false;
    const za::String allow = readText(ctx.tree / allowlistPath, &have);
    if(have)
    {
        for(auto raw : splitLines(allow))
        {
            lines.emplaceBack(raw);
            const auto cut = raw.find('#');
            const auto body = splitWords(cut == npos ? raw : raw.substrByPosLen(0, cut));
            if(body.size() == 3 && allDigits(body[2]))
            {
                allowed[allowKey(body[0], body[1])] = static_cast<int>(toInt(body[2]));
            }
        }
    }
    const auto count = [&](const za::String& fam, const za::String& path) -> za::SizeT {
        const auto f = found.find(fam);
        if(f == found.end())
        {
            return 0;
        }
        const auto p = f->second.find(path);
        return p == f->second.end() ? 0 : p->second.size();
    };
    if(args.size() >= 2 && args[0] == "--list")
    {
        const auto f = found.find(args[1]);
        if(f == found.end())
        {
            za::String names;
            for(const auto& fam : fams)
            {
                if(!names.empty())
                {
                    names += ", ";
                }
                names += fam.id;
            }
            out.line("check_canonical: no family {} ({})", args[1], names);
            return 1;
        }
        for(const auto* e : qza::sortedByKey(f->second))
        {
            for(const auto& h : e->second)
            {
                out.line("{}:{}: {}", e->first, h.line, h.text.substrByPosLen(0, 140));
            }
        }
        return 0;
    }
    if(!args.empty() && args[0] == "--tighten")
    {
        za::String o;
        int changed = 0;
        for(auto raw : lines)
        {
            const auto cut = raw.find('#');
            const auto body = splitWords(cut == npos ? za::StringView(raw) : raw.substrByPosLen(0, cut));
            if(body.size() == 3 && allDigits(body[2]))
            {
                const za::String fam{body[0]};
                const za::String path{body[1]};
                const int n = static_cast<int>(toInt(body[2]));
                const int now = static_cast<int>(count(fam, path));
                if(now < n)
                {
                    ++changed;
                    if(now == 0)
                    {
                        continue;
                    }
                    const za::String from = fmt("{} {} {}", fam, path, n);
                    const auto at = raw.find(from);
                    if(at != npos)
                    {
                        raw.replace(at, from.size(), fmt("{} {} {}", fam, path, now));
                    }
                }
            }
            o += raw;
            o += '\n';
        }
        writeRaw(ctx.tree / allowlistPath, o);
        out.line("check_canonical: {} allowlist line(s) lowered or dropped", changed);
        return 0;
    }
    za::Vector<za::String> problems;
    za::SizeT total = 0;
    for(const auto& fam : fams)
    {
        for(const auto* e : qza::sortedByKey(found.at(fam.id)))
        {
            const auto& [path, hits] = *e;
            total += hits.size();
            const auto a = allowed.find(allowKey(fam.id, path));
            const int n = a == allowed.end() ? 0 : a->second;
            if(static_cast<int>(hits.size()) > n)
            {
                za::String where;
                for(const auto& h : hits)
                {
                    if(!where.empty())
                    {
                        where += ", ";
                    }
                    where += za::toString(h.line);
                }
                problems.pushBack(fmt("{}: {} {} site(s), {} allowed (lines {}): use {}", path, hits.size(), fam.id, n,
                                      where, fam.use));
            }
        }
    }
    int stale = 0;
    for(const auto& [key, n] : allowed)
    {
        const za::SizeT space = key.find(' ');
        if(static_cast<int>(count(za::String{key.substrByPosLen(0, space)}, za::String{key.substrByPosLen(space + 1)})) <
           n)
        {
            ++stale;
        }
    }
    if(!problems.empty())
    {
        for(const auto& p : problems)
        {
            out.line("check_canonical: {}", p);
        }
        out.line("check_canonical: {} new copy/copies of a canonical helper (docs/vr-port/CODE_STYLE.md, "
                 "'Canonical helpers'; a meant one: a line in {} with its reason)",
                 problems.size(), allowlistPath);
        return 1;
    }
    const za::String note = stale ? fmt("; {} allowlist line(s) above today's count: run --tighten", stale) : za::String();
    out.line("check_canonical: {} families, {} allowed sites, no new copies{}", fams.size(), total, note);
    return 0;
}

} // namespace qvrc
