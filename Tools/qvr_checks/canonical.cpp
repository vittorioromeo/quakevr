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

#include <algorithm>
#include <map>
#include <regex>
#include <set>
#include <thread>

namespace qvrc
{
namespace
{

constexpr const char* allowlistPath = "Misc/quakevr/canonical_allowlist.txt";

// One pattern of a family. ECMAScript; `lineStart`: Python's `(?m)^` before it (tried at each line's start only);
// `first`: the characters a match can start with (the others are skipped; empty: any).
struct Pattern
{
    std::string re;
    bool lineStart = false;
    std::string first;
};

// A cheap test before the family's patterns: a file without any of these has no site. A literal, optionally a whole
// word at its start, and optionally followed by spaces and `then`.
struct Needle
{
    std::vector<std::string> lits;
    bool wordStartOnly = false;
    bool wordEndOnly = false;
    char then = 0;
};

struct Family
{
    std::string id;
    std::string lang;
    std::vector<Pattern> patterns;
    Needle needle;
    std::string use;
};

std::string alt(const std::vector<std::string>& names)
{
    std::string s;
    for(const auto& n : names)
    {
        s += (s.empty() ? "" : "|") + n;
    }
    return s;
}

// A C++ definition (or declaration) of NAME: a return type then the name and `(`, not a call after return/else/...;
// and the same as a lambda: `auto NAME = [...](`.
std::vector<Pattern> cppDef(const std::vector<std::string>& names, const std::string& params = "")
{
    const std::string a = "(?:" + alt(names) + ")";
    return {
        {R"([ \t]*(?:\[\[\w+\]\][ \t]*)*(?:(?:static|inline|constexpr|const|extern|friend)\s+)*)"
         R"((?!return\b|else\b|case\b|new\b|delete\b|throw\b|co_return\b|goto\b)\w[\w:<>]*[ \t*&]+(?:)" +
             a + R"()\s*\()" + params,
         true, ""},
        {R"(\bauto\s+(?:)" + a + R"()\s*=\s*\[[^\]\n]*\]\s*\()" + params, false, "a"},
    };
}

std::vector<Family> families()
{
    const std::vector<std::string> geomMath = {"safeNormalize", "wrap180", "wrapYaw", "wrapDegrees", "wrapAngle",
                                               "smooth01", "smoothstep", "smoothStep", "approachExp", "yawDelta",
                                               "yawOf", "closestOnSegment", "segmentDistance", "closestOnTriangle",
                                               "nearestOnTri", "quakeFromTracking", "angleBetween"};
    const std::vector<std::string> rnd = {"rnd", "rnd01", "random01", "randomFloat", "frand", "randf", "rand01"};
    const std::vector<std::string> strs = {"startsWith", "endsWith", "lower", "toLower", "trim", "appendf",
                                           "nearestNormal", "quantile", "percentile", "quantileSorted"};
    const std::vector<std::string> mathCopy = {"Cross", "Wrap180", "Wrap", "AngleNorm", "SegDist", "SegmentDist",
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

bool needleHit(const Needle& nd, std::string_view text)
{
    for(const auto& lit : nd.lits)
    {
        for(size_t i = 0; (i = text.find(lit, i)) != std::string_view::npos; ++i)
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
                const size_t p = skipSpace(text, i + lit.size());
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
std::string stripComments(std::string_view t)
{
    std::string o(t);
    size_t i = 0;
    const size_t n = t.size();
    while(i < n)
    {
        const char c = t[i];
        if(c == '"' || c == '\'')
        {
            size_t j = i + 1;
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
            const size_t e = t.find("*/", i + 2);
            if(e == std::string_view::npos)
            {
                ++i;
                continue;
            }
            for(size_t k = i; k < e + 2; ++k)
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

// The start of every match of a family's patterns (Python's finditer per pattern, the starts merged).
std::set<size_t> matches(const std::vector<std::regex>& res, const std::vector<Pattern>& pats, const std::string& text)
{
    std::set<size_t> hits;
    for(size_t p = 0; p < pats.size(); ++p)
    {
        const auto& pat = pats[p];
        size_t next = 0; // finditer: the next match starts at or after the last one's end
        auto tryAt = [&](size_t i) {
            std::smatch m;
            auto flags = std::regex_constants::match_continuous;
            if(i > 0)
            {
                flags |= std::regex_constants::match_prev_avail;
            }
            if(std::regex_search(text.begin() + static_cast<std::ptrdiff_t>(i), text.end(), m, res[p], flags))
            {
                hits.insert(i);
                next = i + std::max<size_t>(1, static_cast<size_t>(m.length(0)));
                return true;
            }
            return false;
        };
        if(pat.lineStart)
        {
            for(size_t i = 0; i < text.size();)
            {
                if(i >= next)
                {
                    tryAt(i);
                }
                const size_t nl = text.find('\n', i);
                if(nl == std::string::npos)
                {
                    break;
                }
                i = nl + 1;
            }
        }
        else
        {
            for(size_t i = 0; i < text.size();)
            {
                if(!pat.first.empty())
                {
                    i = text.find_first_of(pat.first, i);
                    if(i == std::string::npos)
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
    return hits;
}

struct Site
{
    int line;
    std::string text;
};

using Found = std::map<std::string, std::map<std::string, std::vector<Site>>>; // family -> path -> sites

std::vector<std::string> globPosix(const Ctx& ctx, const char* dir, const std::vector<std::string>& exts, bool recursive)
{
    std::vector<std::string> out;
    for(const auto& p : listFiles(ctx.tree / dir, exts, recursive))
    {
        out.push_back(relPosix(p, ctx.tree));
    }
    return out;
}

Found scan(const Ctx& ctx, const std::vector<Family>& fams)
{
    std::map<std::string, std::vector<std::string>> files;
    {
        std::vector<std::string> cpp;
        for(auto& p : globPosix(ctx, "Quake/vr", {".cpp", ".hpp", ".h", ".inc", ".c"}, true))
        {
            if(p.find("/external/") == std::string::npos)
            {
                cpp.push_back(std::move(p));
            }
        }
        sortStrings(cpp);
        std::vector<std::string> qc;
        for(auto& p : globPosix(ctx, "QC", {".qc"}, false))
        {
            if(p != "QC/vr_cvars_gen.qc") // (generated: qvr_checks cvars_qc)
            {
                qc.push_back(std::move(p));
            }
        }
        sortStrings(qc);
        auto misc = [&](const char* ext) {
            std::vector<std::string> v;
            for(auto& p : globPosix(ctx, "Misc/quakevr", {ext}, true))
            {
                if(!endsWith(p, "/qvrtest.py") && !endsWith(p, "/qvrtest.sh"))
                {
                    v.push_back(std::move(p));
                }
            }
            sortStrings(v);
            return v;
        };
        files["cpp"] = std::move(cpp);
        files["qc"] = std::move(qc);
        files["py"] = misc(".py");
        files["sh"] = misc(".sh");
    }
    // Every file's text and comment-stripped text, once (read in parallel by the families).
    std::map<std::string, std::pair<std::string, std::string>> texts;
    for(const auto& [lang, list] : files)
    {
        for(const auto& p : list)
        {
            texts[p];
        }
    }
    {
        std::vector<std::string*> raws;
        std::vector<std::pair<const std::string*, std::pair<std::string, std::string>*>> jobs;
        for(auto& [p, t] : texts)
        {
            jobs.emplace_back(&p, &t);
        }
        const unsigned nThreads = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
        std::vector<std::thread> pool;
        for(unsigned w = 0; w < nThreads; ++w)
        {
            pool.emplace_back([&, w] {
                for(size_t k = w; k < jobs.size(); k += nThreads)
                {
                    jobs[k].second->first = readText(ctx.tree / *jobs[k].first);
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
    std::vector<std::map<std::string, std::vector<Site>>> perFamily(fams.size());
    std::vector<std::thread> pool;
    for(size_t f = 0; f < fams.size(); ++f)
    {
        pool.emplace_back([&, f] {
            const auto& fam = fams[f];
            std::vector<std::regex> res;
            for(const auto& p : fam.patterns)
            {
                res.emplace_back(p.re, std::regex::ECMAScript | std::regex::optimize);
            }
            for(const auto& path : files.at(fam.lang))
            {
                const auto& [raw, text] = texts.at(path);
                if(!needleHit(fam.needle, raw))
                {
                    continue;
                }
                for(const size_t pos : matches(res, fam.patterns, text))
                {
                    const size_t ls = pos == 0 ? 0 : (text.rfind('\n', pos - 1) == std::string::npos
                                                          ? 0
                                                          : text.rfind('\n', pos - 1) + 1);
                    const size_t le = text.find('\n', pos);
                    const auto lineText = std::string_view(text).substr(
                        ls, le == std::string::npos ? std::string::npos : le - ls);
                    perFamily[f][path].push_back({lineAt(text, pos), std::string(trim(lineText))});
                }
            }
        });
    }
    for(auto& t : pool)
    {
        t.join();
    }
    for(size_t f = 0; f < fams.size(); ++f)
    {
        found[fams[f].id] = std::move(perFamily[f]);
    }
    return found;
}

} // namespace

int checkCanonical(const Ctx& ctx, const Args& args, Out& out)
{
    const auto fams = families();
    const Found found = scan(ctx, fams);
    std::map<std::pair<std::string, std::string>, int> allowed;
    std::vector<std::pair<std::pair<std::string, std::string>, int>> allowedOrder;
    std::vector<std::string> lines;
    bool have = false;
    const std::string allow = readText(ctx.tree / allowlistPath, &have);
    if(have)
    {
        for(auto raw : splitLines(allow))
        {
            lines.emplace_back(raw);
            const auto cut = raw.find('#');
            const auto body = splitWords(cut == std::string_view::npos ? raw : raw.substr(0, cut));
            if(body.size() == 3 && !body[2].empty() && std::all_of(body[2].begin(), body[2].end(), isDigit))
            {
                const auto key = std::pair{std::string(body[0]), std::string(body[1])};
                if(!allowed.contains(key))
                {
                    allowedOrder.emplace_back(key, 0);
                }
                allowed[key] = std::stoi(std::string(body[2]));
            }
        }
    }
    const auto count = [&](const std::string& fam, const std::string& path) -> size_t {
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
            std::string names;
            for(const auto& fam : fams)
            {
                names += (names.empty() ? "" : ", ") + fam.id;
            }
            out.line("check_canonical: no family {} ({})", args[1], names);
            return 1;
        }
        for(const auto& [path, hits] : f->second)
        {
            for(const auto& h : hits)
            {
                out.line("{}:{}: {}", path, h.line, h.text.substr(0, 140));
            }
        }
        return 0;
    }
    if(!args.empty() && args[0] == "--tighten")
    {
        std::string o;
        int changed = 0;
        for(auto raw : lines)
        {
            const auto cut = raw.find('#');
            const auto body = splitWords(cut == std::string::npos ? std::string_view(raw)
                                                                  : std::string_view(raw).substr(0, cut));
            if(body.size() == 3 && !body[2].empty() && std::all_of(body[2].begin(), body[2].end(), isDigit))
            {
                const std::string fam(body[0]);
                const std::string path(body[1]);
                const int n = std::stoi(std::string(body[2]));
                const int now = static_cast<int>(count(fam, path));
                if(now < n)
                {
                    ++changed;
                    if(now == 0)
                    {
                        continue;
                    }
                    const std::string from = std::format("{} {} {}", fam, path, n);
                    const auto at = raw.find(from);
                    if(at != std::string::npos)
                    {
                        raw.replace(at, from.size(), std::format("{} {} {}", fam, path, now));
                    }
                }
            }
            o += raw + "\n";
        }
        writeRaw(ctx.tree / allowlistPath, o);
        out.line("check_canonical: {} allowlist line(s) lowered or dropped", changed);
        return 0;
    }
    std::vector<std::string> problems;
    size_t total = 0;
    for(const auto& fam : fams)
    {
        for(const auto& [path, hits] : found.at(fam.id))
        {
            total += hits.size();
            const auto a = allowed.find({fam.id, path});
            const int n = a == allowed.end() ? 0 : a->second;
            if(static_cast<int>(hits.size()) > n)
            {
                std::string where;
                for(const auto& h : hits)
                {
                    where += (where.empty() ? "" : ", ") + std::to_string(h.line);
                }
                problems.push_back(std::format("{}: {} {} site(s), {} allowed (lines {}): use {}", path, hits.size(),
                                               fam.id, n, where, fam.use));
            }
        }
    }
    int stale = 0;
    for(const auto& [key, n] : allowed)
    {
        if(static_cast<int>(count(key.first, key.second)) < n)
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
    const std::string note =
        stale ? std::format("; {} allowlist line(s) above today's count: run --tighten", stale) : std::string();
    out.line("check_canonical: {} families, {} allowed sites, no new copies{}", fams.size(), total, note);
    return 0;
}

} // namespace qvrc
