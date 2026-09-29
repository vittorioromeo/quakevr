// vr_motion_review.cpp -- reviewing the motion takes: see vr_motion_review.hpp and docs/vr-port/MOTIONS.md
// ("Reviewing failing takes").

#include "vr_motion_review.hpp"
#include "vr_motion.hpp"
#include "vr_motion_take.hpp"

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace qvr::motion::child
{
bool start(const std::vector<std::string>& args, std::string& error); // vr_motion_child.cpp
bool running();
int lastExitCode();
void stop();
} // namespace qvr::motion::child

namespace qvr::motion::review
{

namespace
{

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

[[nodiscard]] std::vector<std::string> splitOn(const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::string cur;
    for(const char c : s)
    {
        if(c == sep)
        {
            out.push_back(cur);
            cur.clear();
        }
        else if(c != '\r')
        {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

[[nodiscard]] std::string trim(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if(a == std::string::npos)
    {
        return "";
    }
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

// A CSV cell: no commas or line breaks (the eval's tables are read by splitting on commas).
[[nodiscard]] std::string cell(std::string s)
{
    for(char& c : s)
    {
        if(c == ',' || c == '\n' || c == '\r')
        {
            c = ' ';
        }
    }
    return s;
}

[[nodiscard]] std::string now()
{
    const std::time_t t = std::time(nullptr);
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return text;
}

[[nodiscard]] bool globMatch(const char* p, const char* s)
{
    if(!*p)
    {
        return !*s;
    }
    if(*p == '*')
    {
        return globMatch(p + 1, s) || (*s && globMatch(p, s + 1));
    }
    return *s && (*p == '?' || *p == *s) && globMatch(p + 1, s + 1);
}

[[nodiscard]] std::string fileName(const fs::path& p)
{
    return p.filename().string();
}

// A take's file name: <label>_<YYYY-MM-DD_HH-MM-SS[-n]>.csv.
struct NameParts
{
    bool ok{false};
    std::string label, stamp;
};

[[nodiscard]] NameParts parseName(const std::string& name)
{
    static const std::regex pattern{R"(^(.+)_(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(-\d+)?)\.csv$)"};
    std::smatch m;
    if(!std::regex_match(name, m, pattern))
    {
        return {};
    }
    return {true, m[1].str(), m[2].str()};
}

[[nodiscard]] fs::path motionsPath()
{
    return fs::path{motionsDir()};
}

[[nodiscard]] fs::path reviewPath()
{
    const fs::path p = motionsPath() / "review";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

// Writes `text` into `path` whole, through a temporary file (a reader never sees half of it).
[[nodiscard]] bool writeWhole(const fs::path& path, const std::string& text)
{
    const fs::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if(!out)
        {
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if(!out)
        {
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if(ec)
    {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

[[nodiscard]] bool readWhole(const fs::path& path, std::string& text)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    text = ss.str();
    return true;
}

// A name in `dir` for `name` that no file has yet: name, else name-2, name-3 ... (before the extension).
[[nodiscard]] fs::path freeName(const fs::path& dir, const std::string& name)
{
    std::error_code ec;
    fs::path p = dir / name;
    const std::string stem = fs::path(name).stem().string();
    const std::string ext = fs::path(name).extension().string();
    for(int n = 2; fs::exists(p, ec) && n < 1000; n++)
    {
        p = dir / (stem + "-" + std::to_string(n) + ext);
    }
    return p;
}

// Moves a file; never over another one.
[[nodiscard]] bool moveFile(const fs::path& from, const fs::path& to, std::string& error)
{
    std::error_code ec;
    if(!fs::exists(from, ec))
    {
        error = fileName(from) + " is missing";
        return false;
    }
    if(fs::exists(to, ec))
    {
        error = fileName(to) + " is there already";
        return false;
    }
    fs::create_directories(to.parent_path(), ec);
    fs::rename(from, to, ec);
    if(ec)
    {
        error = "can't move " + fileName(from) + " (" + ec.message() + ")";
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// eval_status.csv
// ----------------------------------------------------------------------------

struct Status
{
    std::string file, label, verdict, reason, expected, events, recorded, same, weapons, evaluated;
};

const char* const statusColumns =
    "file,label,verdict,reason,expected,events,recorded_events,same_hits_as_recorded,weapons,evaluated";

[[nodiscard]] std::map<std::string, Status> readStatus(const fs::path& path)
{
    std::map<std::string, Status> out;
    std::ifstream in(path, std::ios::binary);
    std::string line;
    std::map<std::string, size_t> col;
    while(in && std::getline(in, line))
    {
        if(line.empty() || line[0] == '#')
        {
            continue;
        }
        const std::vector<std::string> cells = splitOn(line, ',');
        if(col.empty())
        {
            for(size_t i = 0; i < cells.size(); i++)
            {
                col[trim(cells[i])] = i;
            }
            continue;
        }
        const auto get = [&](const char* name) {
            const auto it = col.find(name);
            return it != col.end() && it->second < cells.size() ? cells[it->second] : std::string{};
        };
        Status s{get("file"), get("label"), get("verdict"), get("reason"), get("expected"), get("events"),
            get("recorded_events"), get("same_hits_as_recorded"), get("weapons"), get("evaluated")};
        if(!s.file.empty())
        {
            out[s.file] = std::move(s);
        }
    }
    return out;
}

[[nodiscard]] bool writeStatus(const fs::path& path, const std::map<std::string, Status>& entries)
{
    std::string text = "# eval_status.csv -- vr_motion_eval's verdict on each take of this folder, the latest evaluation "
                       "of each (docs/vr-port/MOTIONS.md, \"Reviewing failing takes\")\n";
    text += statusColumns;
    text += '\n';
    for(const auto& [name, s] : entries)
    {
        text += cell(s.file) + ',' + cell(s.label) + ',' + cell(s.verdict) + ',' + cell(s.reason) + ',' + cell(s.expected) +
                ',' + cell(s.events) + ',' + cell(s.recorded) + ',' + cell(s.same) + ',' + cell(s.weapons) + ',' +
                cell(s.evaluated) + '\n';
    }
    return writeWhole(path, text);
}

// ----------------------------------------------------------------------------
// The review's own files: suspects.cfg, review/reviewed.csv, review/undo.csv
// ----------------------------------------------------------------------------

struct Suspect
{
    std::string pattern, reason;
};

[[nodiscard]] std::vector<Suspect> readSuspects()
{
    std::vector<Suspect> out;
    std::ifstream in(motionsPath() / "suspects.cfg", std::ios::binary);
    std::string line;
    while(in && std::getline(in, line))
    {
        line = trim(line);
        if(line.empty() || line[0] == '#')
        {
            continue;
        }
        const size_t space = line.find_first_of(" \t");
        Suspect s;
        s.pattern = line.substr(0, space);
        s.reason = space == std::string::npos ? std::string{"suspect"} : trim(line.substr(space));
        if(s.pattern.size() < 4 || s.pattern.compare(s.pattern.size() - 4, 4, ".csv") != 0)
        {
            s.pattern += ".csv";
        }
        out.push_back(std::move(s));
    }
    return out;
}

struct Mark
{
    std::string decision; // keep, relabel
    std::string date;
};

std::map<std::string, Mark> marks; // by the take's file name

void readMarks()
{
    marks.clear();
    std::ifstream in(reviewPath() / "reviewed.csv", std::ios::binary);
    std::string line;
    while(in && std::getline(in, line))
    {
        if(line.empty() || line[0] == '#' || line.rfind("file,", 0) == 0)
        {
            continue;
        }
        const std::vector<std::string> c = splitOn(line, ',');
        if(c.size() >= 3 && !c[0].empty())
        {
            marks[c[0]] = {c[1], c[2]};
        }
    }
}

[[nodiscard]] bool writeMarks()
{
    std::string text = "# reviewed.csv -- the takes reviewed in the game (Review Takes): kept, or relabelled\nfile,decision,date\n";
    for(const auto& [name, m] : marks)
    {
        text += cell(name) + ',' + cell(m.decision) + ',' + cell(m.date) + '\n';
    }
    return writeWhole(reviewPath() / "reviewed.csv", text);
}

// Every change the review made, newest last: what Undo Last takes back.
//   keep    a: take, b/c: its mark before (decision, date)
//   discard a: take, b: its name in discarded/
//   restore a: its name in discarded/, b: the take
//   relabel a: the take, b: the relabelled take, c: the original's name in review/relabelled/, d/e: a's mark before
struct Change
{
    std::string date, action, a, b, c, d, e;
};

std::vector<Change> journal;

void readJournal()
{
    journal.clear();
    std::ifstream in(reviewPath() / "undo.csv", std::ios::binary);
    std::string line;
    while(in && std::getline(in, line))
    {
        if(line.empty() || line[0] == '#' || line.rfind("date,", 0) == 0)
        {
            continue;
        }
        std::vector<std::string> c = splitOn(line, ',');
        c.resize(7);
        journal.push_back({c[0], c[1], c[2], c[3], c[4], c[5], c[6]});
    }
}

[[nodiscard]] bool writeJournal()
{
    std::string text = "# undo.csv -- the review's changes, newest last (Undo Last takes back the last)\ndate,action,a,b,c,d,e\n";
    for(const Change& c : journal)
    {
        text += cell(c.date) + ',' + cell(c.action) + ',' + cell(c.a) + ',' + cell(c.b) + ',' + cell(c.c) + ',' +
                cell(c.d) + ',' + cell(c.e) + '\n';
    }
    return writeWhole(reviewPath() / "undo.csv", text);
}

// ----------------------------------------------------------------------------
// The takes, and the list
// ----------------------------------------------------------------------------

struct Take
{
    std::string name, label, category, detail, stamp;
    bool discarded{false};
    const Status* status{nullptr}; // its verdict (null: never evaluated)
    bool stale{false};             // the verdict is of the take under another label (relabelled since)
    std::string suspect;           // why it is suspect ("": it isn't)
    const Mark* mark{nullptr};     // reviewed
};

std::map<std::string, Status> statuses;
std::vector<Suspect> suspectList;
std::vector<Take> takes;
std::string lastEvaluated; // the newest verdict's date
bool dirty = true;
int gen = 0;
int shownFilter = -99, shownCategory = -99;
std::vector<int> shown; // indices into takes
std::vector<std::string> rowTexts, rowHelps;
bool multiDay = false;

std::string pickedName;       // the take the Take page shows
bool pickedDiscarded = false; // in discarded/
std::string actionText;       // the last change's outcome

enum Show : int
{
    ShowToReview,
    ShowFailing,
    ShowSuspect,
    ShowNotEvaluated,
    ShowReviewed,
    ShowAll,
    ShowDiscarded,
    ShowCount
};

[[nodiscard]] bool failing(const Take& t)
{
    return t.status && !t.stale && (t.status->verdict == "FAIL" || t.status->verdict == "ERROR");
}

[[nodiscard]] bool evaluated(const Take& t)
{
    return t.status && !t.stale;
}

[[nodiscard]] int categoryRank(const std::string& category)
{
    const auto& list = categories();
    const std::vector<int> order = categoryOrder();
    for(size_t i = 0; i < order.size(); i++)
    {
        if(category == list[order[i]].choice.name)
        {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(order.size());
}

void rebuild()
{
    dirty = false;
    statuses = readStatus(motionsPath() / "eval_status.csv");
    suspectList = readSuspects();
    readMarks();
    readJournal(); // (as on disk: every change is written as it is made)
    lastEvaluated.clear();
    std::map<std::string, const Status*> byStamp; // a relabelled take's verdict, under its old name
    for(const auto& [name, s] : statuses)
    {
        if(const NameParts p = parseName(name); p.ok)
        {
            byStamp[p.stamp] = &s;
        }
        lastEvaluated = std::max(lastEvaluated, s.evaluated);
    }

    takes.clear();
    std::error_code ec;
    const auto scan = [&](const fs::path& dir, bool discarded) {
        for(const auto& entry : fs::directory_iterator(dir, ec))
        {
            const std::string name = fileName(entry.path());
            const NameParts p = parseName(name);
            if(!p.ok || !entry.is_regular_file(ec))
            {
                continue;
            }
            Take t;
            t.name = name;
            t.label = p.label;
            t.stamp = p.stamp;
            t.category = categoryOf(p.label);
            t.detail = t.category.empty() || t.label == t.category ? "" : t.label.substr(t.category.size() + 1);
            t.discarded = discarded;
            if(const auto it = statuses.find(name); it != statuses.end())
            {
                t.status = &it->second;
            }
            else if(const auto jt = byStamp.find(p.stamp); jt != byStamp.end())
            {
                t.status = jt->second;
                t.stale = true;
            }
            for(const Suspect& s : suspectList)
            {
                if(globMatch(s.pattern.c_str(), name.c_str()))
                {
                    t.suspect = s.reason;
                    break;
                }
            }
            if(const auto mt = marks.find(name); mt != marks.end())
            {
                t.mark = &mt->second;
            }
            takes.push_back(std::move(t));
        }
    };
    scan(motionsPath(), false);
    if(fs::is_directory(motionsPath() / "discarded", ec))
    {
        scan(motionsPath() / "discarded", true);
    }
    std::sort(takes.begin(), takes.end(), [](const Take& a, const Take& b) {
        const int ra = categoryRank(a.category), rb = categoryRank(b.category);
        return ra != rb ? ra < rb : a.stamp != b.stamp ? a.stamp < b.stamp : a.name < b.name;
    });
    multiDay = false;
    for(const Take& t : takes)
    {
        multiDay = multiDay || t.stamp.compare(0, 10, takes.front().stamp, 0, 10) != 0;
    }
    shownFilter = -99; // filtered again
}

[[nodiscard]] bool passesFilter(const Take& t, int show, int category)
{
    if(category >= 0)
    {
        const auto& list = categories();
        if(category >= static_cast<int>(list.size()) || t.category != list[category].choice.name)
        {
            return false;
        }
    }
    if(show == ShowDiscarded)
    {
        return t.discarded;
    }
    if(t.discarded)
    {
        return false;
    }
    switch(show)
    {
        case ShowToReview: return (failing(t) || !t.suspect.empty()) && !t.mark;
        case ShowFailing: return failing(t);
        case ShowSuspect: return !t.suspect.empty();
        case ShowNotEvaluated: return !evaluated(t);
        case ShowReviewed: return t.mark != nullptr;
        default: return true;
    }
}

[[nodiscard]] std::string verdictCode(const Take& t)
{
    if(!t.status)
    {
        return "new";
    }
    if(t.stale)
    {
        return "old";
    }
    const std::string& v = t.status->verdict;
    return v == "ERROR" ? "ERR" : v == "-" ? "--" : v;
}

// "02:41:57", or with the day when the takes span several ("27 02:41").
[[nodiscard]] std::string shortTime(const Take& t)
{
    if(t.stamp.size() < 19)
    {
        return t.stamp;
    }
    std::string hms = t.stamp.substr(11, 8);
    std::replace(hms.begin(), hms.end(), '-', ':');
    return multiDay ? t.stamp.substr(8, 2) + " " + hms.substr(0, 5) : hms;
}

[[nodiscard]] std::string eventsOrNothing(const std::string& events)
{
    return events.empty() || events == "-" ? "nothing" : events;
}

// The row's help (four lines of 38 under the list): what matters first.
[[nodiscard]] std::string helpFor(const Take& t)
{
    std::string h;
    if(t.status && !t.stale)
    {
        h = t.status->verdict + (t.status->reason.empty() ? "" : ": " + t.status->reason) + ". Got " +
            eventsOrNothing(t.status->events) + ".";
    }
    else if(t.stale)
    {
        h = "Relabelled since its evaluation (" + t.status->label + ": " + t.status->verdict + "). Re-evaluate it.";
    }
    else
    {
        h = "Not evaluated yet (newer than the last evaluation).";
    }
    if(!t.suspect.empty())
    {
        h += " Suspect: " + t.suspect + ".";
    }
    if(t.status && !t.stale && !t.status->expected.empty())
    {
        h += " Expected " + t.status->expected + ".";
    }
    return h;
}

void refilter()
{
    shownFilter = CLAMP(0, static_cast<int>(vr_motion_review_show.value), ShowCount - 1);
    shownCategory = static_cast<int>(vr_motion_review_category.value);
    shown.clear();
    rowTexts.clear();
    rowHelps.clear();
    for(size_t i = 0; i < takes.size(); i++)
    {
        const Take& t = takes[i];
        if(!passesFilter(t, shownFilter, shownCategory))
        {
            continue;
        }
        shown.push_back(static_cast<int>(i));
        char text[64];
        q_snprintf(text, sizeof(text), "%-4.4s%c %-23.23s %-8.8s %c", verdictCode(t).c_str(), t.suspect.empty() ? ' ' : '*',
            t.label.c_str(), shortTime(t).c_str(), t.mark ? (t.mark->decision == "relabel" ? 'r' : 'k') : ' ');
        rowTexts.emplace_back(text);
        rowHelps.push_back(helpFor(t));
    }
    gen++;
}

void ensure()
{
    if(dirty)
    {
        rebuild();
    }
    if(shownFilter != static_cast<int>(vr_motion_review_show.value) ||
        shownCategory != static_cast<int>(vr_motion_review_category.value))
    {
        refilter();
    }
}

void changed()
{
    dirty = true;
    invalidateTakeCounts();
    ensure();
    gen++;
}

[[nodiscard]] const Take* find(const std::string& name, bool discarded)
{
    for(const Take& t : takes)
    {
        if(t.name == name && t.discarded == discarded)
        {
            return &t;
        }
    }
    return nullptr;
}

[[nodiscard]] const Take* picked()
{
    ensure();
    return pickedName.empty() ? nullptr : find(pickedName, pickedDiscarded);
}

[[nodiscard]] fs::path pathOf(const Take& t)
{
    return t.discarded ? motionsPath() / "discarded" / t.name : motionsPath() / t.name;
}

void say(const std::string& text, bool ok)
{
    actionText = text;
    Con_Printf("Review Takes: %s\n", text.c_str());
    S_LocalSound(ok ? "misc/menu3.wav" : "doors/basetry.wav");
}

// The decisions and the journal written (a change made, but not recorded, is said loudly).
void saveReview()
{
    if(!writeMarks() || !writeJournal())
    {
        Con_Warning("Review Takes: can't write motions/review/reviewed.csv or undo.csv\n");
        S_LocalSound("doors/basetry.wav");
    }
}

// ----------------------------------------------------------------------------
// Re-evaluate: a second copy of the game, in the mock headset
// ----------------------------------------------------------------------------

bool jobRunning = false;
int jobCount = 0;
double jobStart = 0.0;
double jobNextPoll = 0.0;
std::string jobProgress;

[[nodiscard]] fs::path jobFile(const char* name)
{
    return reviewPath() / name;
}

void startJob(const std::vector<std::string>& paths)
{
    if(COM_CheckParm("-evalcopy"))
    {
        say("this is the evaluation's copy of the game: it doesn't start another", false);
        return;
    }
    if(jobRunning)
    {
        say("a re-evaluation is running already (Stop Re-evaluation)", false);
        return;
    }
    if(paths.empty())
    {
        say("no takes to evaluate", false);
        return;
    }
    std::string list;
    for(const std::string& p : paths)
    {
        list += p + '\n';
    }
    // The copy's script: the map, a moment for the mock headset to start, the evaluation, and quit.
    std::string script = "sv_autosave 0\nmap vrfiringrange\n";
    for(int i = 0; i < 60; i++)
    {
        script += "wait\n";
    }
    script += "vr_motion_eval list \"review/eval_job.txt\" progress \"review/eval_progress.txt\" out \"review/eval_job_table.csv\" quit\n";
    std::error_code ec;
    fs::remove(jobFile("eval_progress.txt"), ec);
    if(!writeWhole(jobFile("eval_job.txt"), list) || !writeWhole(jobFile("eval_job.cfg"), script))
    {
        say("can't write motions/review/eval_job.*", false);
        return;
    }
    // Its settings are the config's: this game's now (the melee settings as they are set).
    Host_WriteConfiguration();

    // The script first: the engine keeps only the command line's first 256 characters for its + commands (a long
    // -basedir would cut it off). -noautoexec: the player's autoexec.cfg could start anything, a re-evaluation among
    // them; -evalcopy: this copy never starts copies of its own.
    std::vector<std::string> args{"+exec", "motions/review/eval_job.cfg", "-vrmock", "-noconfigwrite", "-noautoexec",
        "-evalcopy", "-nosound", "-window", "-width", "480", "-height", "270"};
    bool baseDir = false;
    for(int i = 1; i < com_argc; i++)
    {
        if((!q_strcasecmp(com_argv[i], "-game") || !q_strcasecmp(com_argv[i], "-basedir")) && i + 1 < com_argc)
        {
            baseDir = baseDir || !q_strcasecmp(com_argv[i], "-basedir");
            args.emplace_back(com_argv[i]);
            args.emplace_back(com_argv[i + 1]);
            i++;
        }
    }
    if(!baseDir)
    {
        args.emplace_back("-basedir");
        args.emplace_back(com_basedirs[com_numbasedirs - 1]); // (the game folder's)
    }
    std::string error;
    if(!child::start(args, error))
    {
        say("can't start the evaluation: " + error, false);
        return;
    }
    jobRunning = true;
    jobCount = static_cast<int>(paths.size());
    jobStart = realtime;
    jobNextPoll = realtime + 1.0;
    jobProgress = "starting";
    say(va("re-evaluating %d take%s in the background (about %d s)", jobCount, jobCount == 1 ? "" : "s",
            static_cast<int>(10 + 1.7 * jobCount)),
        true);
}

void pollJob()
{
    if(!jobRunning || realtime < jobNextPoll)
    {
        return;
    }
    jobNextPoll = realtime + 1.0;
    std::string text;
    if(readWhole(jobFile("eval_progress.txt"), text))
    {
        jobProgress = trim(text);
    }
    if(child::running())
    {
        return;
    }
    jobRunning = false;
    const int code = child::lastExitCode();
    std::string done;
    if(!readWhole(jobFile("eval_progress.txt"), done))
    {
        done.clear();
    }
    changed();
    if(code == 0 && trim(done).rfind("done", 0) == 0)
    {
        say(va("re-evaluated %d take%s (%.0f s)", jobCount, jobCount == 1 ? "" : "s", realtime - jobStart), true);
    }
    else
    {
        say(va("the re-evaluation stopped (exit code %d, %s): see motions/review/", code, jobProgress.c_str()), false);
    }
}

// ----------------------------------------------------------------------------
// The ghost
// ----------------------------------------------------------------------------

struct GhostEvent
{
    std::string text;
    bool hit{false};
    bool hasAt{false};
    glm::vec3 at{0.f}; // world, as recorded
};

struct GhostHand
{
    bool has{false};
    glm::vec3 pos{0.f}; // world, as recorded
    glm::vec3 rot{0.f}; // world angles
    int model{-1};      // into Ghost::models
    bool hasLine{false};
    glm::vec3 butt{0.f}, end{0.f};
    std::vector<glm::vec3> points;
};

struct GhostFrame
{
    double t{0.0};
    int phase{1}; // 0 pre, 1 rec, 2 tail
    glm::vec3 head{0.f};
    glm::vec3 view{0.f};
    GhostHand hands[2];
    std::vector<GhostEvent> events;
};

struct ReplayEvent
{
    double t;
    std::string text;
};

struct Marker
{
    std::string text;
    bool live{true};
    bool hit{false}; // a hit (red); a push, parry or batting (yellow)
    bool hasAt{false};
    glm::vec3 at{0.f};
    double until{0.0};
};

struct Ghost
{
    bool active{false};
    std::string name, label, verdict;
    std::vector<GhostFrame> frames;
    std::vector<std::string> modelNames;
    std::vector<qmodel_t*> models;
    std::vector<ReplayEvent> replayEvents;
    std::vector<Marker> markers;
    const qmodel_t* world{nullptr}; // the map it was started in
    bool hasMon{false};
    std::string monClass;
    glm::vec3 monW{0.f};  // the take's monster (the first frame's)
    glm::vec3 org0{0.f};  // the take's player at its start
    float yaw0{0.f};
    double t{0.0};        // the take's time shown
    double holdUntil{0.0}; // between loops (realtime)
    size_t nextEvent{0};  // the next frame whose events are shown
    size_t nextReplay{0};
    bool hasTarget{false};
    glm::vec3 target{0.f}; // the dummy now
    bool anchored{false};
    glm::vec3 anchorRec{0.f}, anchorNow{0.f};
    float dyaw{0.f};
};

Ghost ghost;

[[nodiscard]] std::string eventText(const std::vector<std::string>& f)
{
    // kind:sub:hand:value:x:y:z:target:detail
    std::string s = f[0];
    if(!f[1].empty())
    {
        s += "/" + f[1];
    }
    if(!f[2].empty())
    {
        s += " " + f[2];
    }
    const float value = std::strtof(f[3].c_str(), nullptr);
    if(value != 0.f)
    {
        s += va(" %.0f", value);
    }
    if(!f[8].empty())
    {
        s += " " + f[8];
    }
    return s;
}

[[nodiscard]] bool loadGhost(const fs::path& path, std::string& error)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        error = "can't open " + fileName(path);
        return false;
    }
    Ghost g;
    std::map<std::string, std::string> header;
    std::map<std::string, int> col;
    std::string line;
    bool yaw0Known = false;
    while(std::getline(in, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if(line.empty())
        {
            continue;
        }
        if(line[0] == '#')
        {
            const size_t colon = line.find(':');
            if(colon != std::string::npos)
            {
                header[trim(line.substr(1, colon - 1))] = trim(line.substr(colon + 1));
            }
            continue;
        }
        const std::vector<std::string> cells = splitOn(line, ',');
        if(col.empty())
        {
            for(size_t i = 0; i < cells.size(); i++)
            {
                col[trim(cells[i])] = static_cast<int>(i);
            }
            if(const auto it = header.find("yaw0"); it != header.end())
            {
                g.yaw0 = std::strtof(it->second.c_str(), nullptr);
                yaw0Known = true;
            }
            continue;
        }
        const auto str = [&](const std::string& name) -> const std::string& {
            static const std::string none;
            const auto it = col.find(name);
            return it != col.end() && it->second < static_cast<int>(cells.size()) ? cells[it->second] : none;
        };
        const auto has = [&](const std::string& name) { return !str(name).empty(); };
        const auto f = [&](const std::string& name) { return std::strtof(str(name).c_str(), nullptr); };
        const auto vec = [&](const std::string& a, const std::string& b, const std::string& c) {
            return glm::vec3{f(a), f(b), f(c)};
        };

        GhostFrame fr;
        fr.t = std::strtod(str("t").c_str(), nullptr);
        fr.phase = str("phase") == "pre" ? 0 : str("phase") == "tail" ? 2 : 1;
        const glm::vec3 org = vec("org_w_x", "org_w_y", "org_w_z");
        const auto world = [&](const glm::vec3& pf) { return org + hands::rotateYaw(pf, g.yaw0); };
        fr.head = has("head_w_x") ? vec("head_w_x", "head_w_y", "head_w_z") : world(vec("head_x_u", "head_y_u", "head_z_u"));
        fr.view = {f("view_pitch"), f("view_yaw") + g.yaw0, f("view_roll")};
        for(const int h : {HAND_MAIN, HAND_OFF})
        {
            const std::string p = h == HAND_MAIN ? "m_" : "o_";
            GhostHand& gh = fr.hands[h];
            gh.has = has(p + "pos_w_x");
            if(!gh.has)
            {
                continue;
            }
            gh.pos = vec(p + "pos_w_x", p + "pos_w_y", p + "pos_w_z");
            gh.rot = {f(p + "pitch"), f(p + "yaw") + g.yaw0, f(p + "roll")};
            const std::string& model = str(p + "model");
            if(!model.empty())
            {
                const auto it = std::find(g.modelNames.begin(), g.modelNames.end(), model);
                gh.model = static_cast<int>(it - g.modelNames.begin());
                if(it == g.modelNames.end())
                {
                    g.modelNames.push_back(model);
                }
            }
            gh.hasLine = has(p + "butt_x_u") && has(p + "end_x_u");
            if(gh.hasLine)
            {
                gh.butt = world(vec(p + "butt_x_u", p + "butt_y_u", p + "butt_z_u"));
                gh.end = world(vec(p + "end_x_u", p + "end_y_u", p + "end_z_u"));
            }
            for(int i = 0; i < 6; i++)
            {
                const std::string q = p + "pt" + std::to_string(i) + "_";
                if(has(q + "x_u"))
                {
                    gh.points.push_back(world(vec(q + "x_u", q + "y_u", q + "z_u")));
                }
            }
        }
        if(const std::string& events = str("events"); !events.empty())
        {
            for(const std::string& e : splitOn(events, ';'))
            {
                const std::vector<std::string> ef = splitOn(e, ':');
                if(ef.size() < 9 || ef[0] == "stroke")
                {
                    continue;
                }
                GhostEvent ge;
                ge.text = eventText(ef);
                ge.hit = ef[0] == "melee" || ef[0] == "bash" || ef[0] == "parrybash" || ef[0] == "shove" || ef[0] == "headbutt";
                ge.hasAt = !ef[4].empty();
                if(ge.hasAt)
                {
                    ge.at = world({std::strtof(ef[4].c_str(), nullptr), std::strtof(ef[5].c_str(), nullptr),
                        std::strtof(ef[6].c_str(), nullptr)});
                }
                fr.events.push_back(std::move(ge));
            }
        }
        if(!g.hasMon && has("mon_w_x"))
        {
            g.hasMon = true;
            g.monW = vec("mon_w_x", "mon_w_y", "mon_w_z");
            g.monClass = str("mon_class");
        }
        if(g.frames.empty())
        {
            g.org0 = org;
        }
        g.frames.push_back(std::move(fr));
    }
    if(g.frames.empty())
    {
        error = "no frames in " + fileName(path);
        return false;
    }
    if(!yaw0Known)
    {
        error = fileName(path) + " has no yaw0 (a synthetic take: play it with vr_motion_play)";
        return false;
    }
    for(const std::string& m : g.modelNames)
    {
        g.models.push_back(Mod_ForName(m.c_str(), false));
    }
    ghost = std::move(g);
    return true;
}

// The replay's events from the evaluation ("melee/stab main 12.0 the tip @0.345; ...").
[[nodiscard]] std::vector<ReplayEvent> replayEventsOf(const std::string& events)
{
    std::vector<ReplayEvent> out;
    if(events.empty() || events == "-")
    {
        return out;
    }
    size_t start = 0;
    while(start < events.size())
    {
        size_t end = events.find("; ", start);
        const std::string e = events.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const size_t at = e.rfind(" @");
        if(at != std::string::npos)
        {
            out.push_back({std::strtod(e.c_str() + at + 2, nullptr), e.substr(0, at)});
        }
        if(end == std::string::npos)
        {
            break;
        }
        start = end + 2;
    }
    std::sort(out.begin(), out.end(), [](const ReplayEvent& a, const ReplayEvent& b) { return a.t < b.t; });
    return out;
}

void restartGhost()
{
    ghost.t = ghost.frames.front().t;
    ghost.nextEvent = 0;
    ghost.nextReplay = 0;
    ghost.markers.clear();
}

[[nodiscard]] glm::vec3 placeGhost(const glm::vec3& recorded)
{
    return ghost.anchorNow + hands::rotateYaw(recorded - ghost.anchorRec, ghost.dyaw);
}

// Where the take goes in this world: as recorded relative to the dummy (moved with it, not turned, as playback puts
// the player), or without one around the player, turned to the player's heading.
void anchorGhost(const hands::State& s)
{
    if(ghost.hasMon && ghost.hasTarget)
    {
        ghost.anchorRec = ghost.monW;
        ghost.anchorNow = ghost.target;
        ghost.dyaw = 0.f;
        ghost.anchored = true;
        return;
    }
    if(!ghost.anchored)
    {
        ghost.anchorRec = ghost.org0;
        ghost.anchorNow = s.playerOrigin;
        ghost.dyaw = s.headAngles.y - ghost.yaw0;
        ghost.anchored = true;
    }
}

[[nodiscard]] float lerpAngle(float a, float b, float f)
{
    return a + std::remainder(b - a, 360.f) * f;
}

[[nodiscard]] glm::vec3 lerpAngles(const glm::vec3& a, const glm::vec3& b, float f)
{
    return {lerpAngle(a.x, b.x, f), lerpAngle(a.y, b.y, f), lerpAngle(a.z, b.z, f)};
}

void queueText(const std::string& text, const glm::vec3& pos, const hands::State& s)
{
    const glm::vec3 d = pos - s.head;
    const float yaw = glm::degrees(std::atan2(d.y, d.x));
    const float dist = std::max(glm::length(d), 16.f);
    text3d::queue(text, pos, glm::vec3{0.f, yaw, 0.f}, text3d::Align::Centre, 0.003f * dist);
}

void drawGhost()
{
    if(!ghost.active)
    {
        return;
    }
    if(cls.state != ca_connected || cl.worldmodel != ghost.world)
    {
        ghost.active = false; // a new map: its models are gone
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    anchorGhost(s);

    // The take's time: at the chosen pace, looping with a pause.
    const double speed = CLAMP(0.02, static_cast<double>(vr_motion_review_speed.value), 4.0);
    const double end = ghost.frames.back().t;
    if(ghost.holdUntil > 0.0)
    {
        if(realtime >= ghost.holdUntil) // the pause after the end: again from the start
        {
            restartGhost();
            ghost.holdUntil = 0.0;
        }
    }
    else
    {
        ghost.t += host_frametime * speed;
        if(ghost.t >= end)
        {
            ghost.t = end;
            ghost.holdUntil = realtime + 1.0;
        }
    }

    // The frames around it.
    size_t i = 0;
    while(i + 1 < ghost.frames.size() && ghost.frames[i + 1].t <= ghost.t)
    {
        i++;
    }
    const GhostFrame& a = ghost.frames[i];
    const GhostFrame& b = ghost.frames[std::min(i + 1, ghost.frames.size() - 1)];
    const float f = b.t > a.t ? static_cast<float>(CLAMP(0.0, (ghost.t - a.t) / (b.t - a.t), 1.0)) : 0.f;

    // Events: the take's own (live) where they happened, the evaluation's replay over the dummy.
    const double shownFor = 1.2 / std::max(speed, 0.25);
    while(ghost.nextEvent <= i && ghost.nextEvent < ghost.frames.size())
    {
        for(const GhostEvent& e : ghost.frames[ghost.nextEvent].events)
        {
            ghost.markers.push_back({"live: " + e.text, true, e.hit, e.hasAt, e.at, realtime + shownFor});
        }
        ghost.nextEvent++;
    }
    while(ghost.nextReplay < ghost.replayEvents.size() && ghost.replayEvents[ghost.nextReplay].t <= ghost.t)
    {
        ghost.markers.push_back({"replay: " + ghost.replayEvents[ghost.nextReplay].text, false, false, false, {}, realtime + shownFor});
        ghost.nextReplay++;
    }

    const float m2u = units::metresToUnits();
    const float rotDyaw = ghost.dyaw;
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        const GhostHand& ha = a.hands[h];
        const GhostHand& hb = b.hands[h].has ? b.hands[h] : ha;
        if(!ha.has)
        {
            continue;
        }
        const glm::vec3 pos = placeGhost(glm::mix(ha.pos, hb.pos, f));
        glm::vec3 rot = lerpAngles(ha.rot, hb.rot, f);
        rot.y += rotDyaw;
        qmodel_t* model = ha.model >= 0 && ha.model < static_cast<int>(ghost.models.size()) ? ghost.models[ha.model] : nullptr;
        view::setGhost(h, model, pos, rot, 0.7f);

        const glm::vec4 lineColour = h == HAND_MAIN ? glm::vec4{0.3f, 0.85f, 1.f, 0.9f} : glm::vec4{0.5f, 1.f, 0.6f, 0.9f};
        if(ha.hasLine)
        {
            const glm::vec3 butt = placeGhost(glm::mix(ha.butt, hb.hasLine ? hb.butt : ha.butt, f));
            const glm::vec3 tip = placeGhost(glm::mix(ha.end, hb.hasLine ? hb.end : ha.end, f));
            lines::glow(butt, tip, 0.012f * m2u, lineColour * 0.6f, lineColour);
            lines::glowPoint(tip, 0.03f * m2u, lineColour);
        }
        for(const glm::vec3& p : ha.points)
        {
            lines::point(placeGhost(p), 0.018f * m2u, glm::vec4{1.f, 0.85f, 0.3f, 0.9f});
        }
        // The far end's trail: the last 0.3 s of the take.
        glm::vec3 prev{0.f};
        bool havePrev = false;
        for(size_t k = i + 1; k-- > 0;)
        {
            const GhostHand& hk = ghost.frames[k].hands[h];
            const double age = ghost.t - ghost.frames[k].t;
            if(age > 0.3)
            {
                break;
            }
            const glm::vec3 p = placeGhost(hk.hasLine ? hk.end : hk.pos);
            if(havePrev)
            {
                const float fade = static_cast<float>(1.0 - age / 0.3);
                lines::glow(p, prev, 0.008f * m2u, lineColour * (0.5f * fade), lineColour * (0.5f * fade));
            }
            prev = p;
            havePrev = true;
        }
    }

    // The head, and where it looked.
    const glm::vec3 head = placeGhost(glm::mix(a.head, b.head, f));
    glm::vec3 view = lerpAngles(a.view, b.view, f);
    view.y += rotDyaw;
    lines::glowPoint(head, 0.07f * m2u, glm::vec4{0.6f, 0.75f, 1.f, 0.6f});
    lines::glow(head, head + hands::forward(view) * (0.25f * m2u), 0.006f * m2u, glm::vec4{0.6f, 0.75f, 1.f, 0.6f},
        glm::vec4{0.6f, 0.75f, 1.f, 0.f});

    // Its name, time and verdict over the dummy (or over the ghost), and the events shown.
    const glm::vec3 over = (ghost.hasMon && ghost.hasTarget ? ghost.target : head) + glm::vec3{0.f, 0.f, 1.3f * m2u};
    const char* phase = a.phase == 0 ? "lead-in" : a.phase == 2 ? "tail" : "take";
    queueText(va("GHOST %s  %.2f s %s  x%.2g\n%s", ghost.label.c_str(), ghost.t, phase, speed, ghost.verdict.c_str()), over, s);
    int replayRow = 0;
    for(auto it = ghost.markers.begin(); it != ghost.markers.end();)
    {
        if(realtime > it->until)
        {
            it = ghost.markers.erase(it);
            continue;
        }
        if(it->live && it->hasAt)
        {
            const glm::vec3 p = placeGhost(it->at);
            lines::glowPoint(p, 0.06f * m2u, it->hit ? glm::vec4{1.f, 0.3f, 0.2f, 0.9f} : glm::vec4{1.f, 0.85f, 0.2f, 0.9f});
            queueText(it->text, p + glm::vec3{0.f, 0.f, 0.12f * m2u}, s);
        }
        else
        {
            queueText(it->text, over - glm::vec3{0.f, 0.f, (0.12f + 0.07f * static_cast<float>(replayRow++)) * m2u}, s);
        }
        ++it;
    }
}

// ----------------------------------------------------------------------------
// The details
// ----------------------------------------------------------------------------

std::vector<std::string> details;
int detailsGen = -1;

void wrapInto(std::vector<std::string>& out, const std::string& text)
{
    constexpr size_t columns = 40;
    std::string rest = text;
    const std::string indent = "  ";
    bool first = true;
    while(!rest.empty())
    {
        const size_t width = first ? columns : columns - indent.size();
        size_t n = rest.size();
        if(n > width)
        {
            n = rest.rfind(' ', width);
            if(n == std::string::npos || n == 0)
            {
                n = width;
            }
        }
        out.push_back((first ? "" : indent) + rest.substr(0, n));
        rest = trim(rest.substr(n));
        first = false;
    }
}

void makeDetails()
{
    details.clear();
    const Take* t = picked();
    if(!t)
    {
        details.push_back(pickedName.empty() ? "No take picked: pick one in the list." : "The take is gone: " + pickedName);
        return;
    }
    std::string when = t->stamp;
    if(when.size() >= 19)
    {
        when = when.substr(0, 10) + " " + when.substr(11);
        std::replace(when.begin() + 11, when.end(), '-', ':');
    }
    wrapInto(details, t->label);
    wrapInto(details, "recorded " + when + (t->discarded ? "  DISCARDED" : ""));
    wrapInto(details, "category " + (t->category.empty() ? std::string{"-"} : t->category) +
                          (t->detail.empty() ? "" : ", detail " + t->detail));
    if(t->status && !t->stale)
    {
        const Status& s = *t->status;
        wrapInto(details, s.verdict + (s.reason.empty() ? "" : ": " + s.reason));
        wrapInto(details, "expected: " + (s.expected.empty() ? std::string{"-"} : s.expected));
        wrapInto(details, "replay: " + eventsOrNothing(s.events));
        wrapInto(details, "live: " + eventsOrNothing(s.recorded) +
                              (s.same == "yes" ? "  (same hits)" : s.same == "no" ? "  (other hits)" : ""));
        wrapInto(details, "weapons " + s.weapons + ", evaluated " + s.evaluated.substr(0, 16));
    }
    else if(t->stale)
    {
        wrapInto(details, "Relabelled since its evaluation: as " + t->status->label + " it was " + t->status->verdict +
                              (t->status->reason.empty() ? "" : " (" + t->status->reason + ")") + ". Re-evaluate it.");
    }
    else
    {
        wrapInto(details, "Not evaluated yet: Re-evaluate This Take.");
    }
    if(!t->suspect.empty())
    {
        wrapInto(details, "SUSPECT: " + t->suspect);
    }
    if(t->mark)
    {
        wrapInto(details, (t->mark->decision == "relabel" ? "reviewed: relabelled " : "reviewed: kept ") + t->mark->date);
    }
}

// vr_motion_review [list | pick <row or file> | show | play | stop | replay | keep | discard | restore | relabel <category> [detail] |
// undo | next | prev | reeval [take|shown] | stopeval | show]: the Review Takes page's actions, from the console.
void review_f()
{
    ensure();
    const std::string cmd = Cmd_Argc() > 1 ? Cmd_Argv(1) : "list";
    if(cmd == "list")
    {
        Con_Printf("%s\n%s\n%s\n%s\n", summary(), reviewedLine(), evalLine(), listTitle());
        for(int r = 0; r < rowCount(); r++)
        {
            Con_Printf("%3d %s\n", r, rowText(r));
        }
    }
    else if(cmd == "pick" && Cmd_Argc() > 2)
    {
        const char* arg = Cmd_Argv(2);
        bool number = *arg != 0;
        for(const char* p = arg; *p; p++)
        {
            number = number && *p >= '0' && *p <= '9';
        }
        if(number)
        {
            pick(Q_atoi(arg));
        }
        else
        {
            for(const Take& t : takes)
            {
                if(t.name == arg || t.name == std::string{arg} + ".csv")
                {
                    pickedName = t.name;
                    pickedDiscarded = t.discarded;
                    gen++;
                }
            }
        }
        Con_Printf("picked: %s\n", pickedName.empty() ? "-" : pickedName.c_str());
    }
    else if(cmd == "show")
    {
        for(int l = 0; l < detailLines && detailLine(l)[0]; l++)
        {
            Con_Printf("%s\n", detailLine(l));
        }
    }
    else if(cmd == "play") playGhost();
    else if(cmd == "replay") replayMock();
    else if(cmd == "stop") stopGhost();
    else if(cmd == "keep") keep();
    else if(cmd == "discard") discard();
    else if(cmd == "restore") restore();
    else if(cmd == "undo") undo();
    else if(cmd == "next") nextTake();
    else if(cmd == "prev") previousTake();
    else if(cmd == "stopeval") stopReevaluation();
    else if(cmd == "reeval")
    {
        if(Cmd_Argc() > 2 && !strcmp(Cmd_Argv(2), "take"))
        {
            reevaluateTake();
        }
        else
        {
            reevaluateShown();
        }
    }
    else if(cmd == "relabel" && Cmd_Argc() > 2)
    {
        const auto& list = categories();
        for(size_t c = 0; c < list.size(); c++)
        {
            if(!strcmp(list[c].choice.name, Cmd_Argv(2)))
            {
                Cvar_SetValueQuick(&vr_motion_relabel_category, static_cast<float>(c));
                int detail = 0;
                for(size_t d = 0; Cmd_Argc() > 3 && d < list[c].details.size(); d++)
                {
                    if(!strcmp(list[c].details[d].name, Cmd_Argv(3)))
                    {
                        detail = static_cast<int>(d);
                    }
                }
                Cvar_SetValueQuick(&vr_motion_relabel_detail, static_cast<float>(detail));
                relabel();
                return;
            }
        }
        Con_Printf("vr_motion_review relabel: no category \"%s\"\n", Cmd_Argv(2));
    }
    else
    {
        Con_Printf("vr_motion_review [list | pick <row or file> | show | play | stop | replay | keep | discard | restore | "
                   "relabel <category> [detail] | undo | next | prev | reeval [take|shown] | stopeval]\n");
    }
}

// The relabel's detail resets to none when its category changes (as the recorder's).
void relabelCategoryChanged(cvar_t* /* var */)
{
    if(vr_motion_relabel_detail.value != 0.f)
    {
        Cvar_SetValueQuick(&vr_motion_relabel_detail, 0.f);
    }
}

} // namespace

// ----------------------------------------------------------------------------
// The interface
// ----------------------------------------------------------------------------

void init()
{
    Cmd_AddCommand("vr_motion_review", review_f);
    Cvar_SetCallback(&vr_motion_relabel_category, relabelCategoryChanged);
}

void invalidate()
{
    dirty = true;
}

void frame()
{
    pollJob();
    drawGhost();
}

void serverFrame()
{
    if(!ghost.active || !sv.active || !svs.clients[0].active)
    {
        return;
    }
    edict_t* player = svs.clients[0].edict;
    const glm::vec3 from{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    const std::string wanted = ghost.monClass.empty() ? std::string{"vr_dummy"} : ghost.monClass;
    float best = 1e9f;
    bool found = false;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || wanted != PR_GetString(e->v.classname))
        {
            continue;
        }
        const glm::vec3 o{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
        if(const float d = glm::distance(o, from); d < best)
        {
            best = d;
            ghost.target = o;
            found = true;
        }
    }
    ghost.hasTarget = found;
}

void recordEval(const std::vector<Verdict>& verdicts)
{
    std::map<fs::path, std::vector<const Verdict*>> byFolder;
    for(const Verdict& v : verdicts)
    {
        byFolder[fs::path(v.path).parent_path()].push_back(&v);
    }
    const std::string when = now();
    for(const auto& [folder, list] : byFolder)
    {
        const fs::path file = folder / "eval_status.csv";
        std::map<std::string, Status> entries = readStatus(file);
        for(const Verdict* v : list)
        {
            const std::string name = fileName(v->path);
            entries[name] = {name, v->label, v->verdict, v->reason, v->expected, v->events, v->recorded, v->same, v->weapons, when};
        }
        if(writeStatus(file, entries))
        {
            Con_Printf("vr_motion_eval: the verdicts are in %s\n", file.string().c_str());
        }
        else
        {
            Con_Warning("vr_motion_eval: can't write %s\n", file.string().c_str());
        }
    }
    dirty = true;
}

int generation()
{
    ensure();
    return gen;
}

int rowCount()
{
    ensure();
    return static_cast<int>(shown.size());
}

const char* rowText(int row)
{
    return row >= 0 && row < static_cast<int>(rowTexts.size()) ? rowTexts[row].c_str() : "";
}

const char* rowHelp(int row)
{
    return row >= 0 && row < static_cast<int>(rowHelps.size()) ? rowHelps[row].c_str() : nullptr;
}

void pick(int row)
{
    ensure();
    if(row < 0 || row >= static_cast<int>(shown.size()))
    {
        return;
    }
    const Take& t = takes[shown[row]];
    pickedName = t.name;
    pickedDiscarded = t.discarded;
    // Relabel offers its own category first.
    const auto& list = categories();
    for(size_t c = 0; c < list.size(); c++)
    {
        if(t.category == list[c].choice.name)
        {
            Cvar_SetValueQuick(&vr_motion_relabel_category, static_cast<float>(c));
            for(size_t d = 0; d < list[c].details.size(); d++)
            {
                if(t.detail == list[c].details[d].name)
                {
                    Cvar_SetValueQuick(&vr_motion_relabel_detail, static_cast<float>(d));
                }
            }
        }
    }
    gen++;
}

namespace
{

// The menu's texts, valid until the same function's next call (the menu draws them at once).
struct ReviewReadouts
{
    std::string summary, reviewed, eval, listTitle;
    auto members() { return std::tie(summary, reviewed, eval, listTitle); }
};
mem::Scratch<ReviewReadouts> readouts{"review readouts"};

} // namespace

const char* summary()
{
    ensure();
    std::string& text = readouts.summary;
    int total = 0, fail = 0, suspect = 0, reviewed = 0;
    for(const Take& t : takes)
    {
        if(t.discarded)
        {
            continue;
        }
        total++;
        fail += failing(t);
        suspect += !t.suspect.empty();
        reviewed += t.mark != nullptr;
    }
    text = va("%d takes: %d failing, %d suspect", total, fail, suspect);
    return text.c_str();
}

const char* reviewedLine()
{
    ensure();
    std::string& text = readouts.reviewed;
    int kept = 0, relabelled = 0, discarded = 0;
    for(const Take& t : takes)
    {
        discarded += t.discarded;
        kept += !t.discarded && t.mark && t.mark->decision != "relabel";
        relabelled += !t.discarded && t.mark && t.mark->decision == "relabel";
    }
    text = va("%d reviewed (%d kept, %d relabelled), %d discarded", kept + relabelled, kept, relabelled, discarded);
    if(text.size() > 40)
    {
        text = va("%d reviewed, %d discarded", kept + relabelled, discarded);
    }
    return text.c_str();
}

const char* evalLine()
{
    ensure();
    std::string& text = readouts.eval;
    if(jobRunning)
    {
        text = va("evaluating: %s (%.0f s)", jobProgress.c_str(), realtime - jobStart);
        return text.c_str();
    }
    int fresh = 0;
    for(const Take& t : takes)
    {
        fresh += !t.discarded && !evaluated(t);
    }
    text = lastEvaluated.empty() ? std::string{"never evaluated: Re-evaluate"}
                                 : "evaluated " + lastEvaluated.substr(0, 16) + (fresh ? va(", %d not", fresh) : "");
    return text.c_str();
}

const char* listTitle()
{
    ensure();
    static const char* const names[ShowCount] = {"To Review", "Failing", "Suspect", "Not Evaluated", "Reviewed", "All", "Discarded"};
    std::string& text = readouts.listTitle;
    const auto& list = categories();
    const int c = shownCategory;
    text = va("%s%s%s: %d", names[shownFilter], c >= 0 && c < static_cast<int>(list.size()) ? ", " : "",
        c >= 0 && c < static_cast<int>(list.size()) ? list[c].choice.name : "", static_cast<int>(shown.size()));
    return text.c_str();
}

const char* lastAction()
{
    return actionText.c_str();
}

const char* detailLine(int line)
{
    if(detailsGen != generation() || details.empty())
    {
        makeDetails();
        detailsGen = gen;
    }
    return line >= 0 && line < static_cast<int>(details.size()) ? details[line].c_str() : "";
}

void playGhost()
{
    const Take* t = picked();
    if(!t)
    {
        say("no take picked", false);
        return;
    }
    if(cls.state != ca_connected || !sv.active)
    {
        say("the ghost plays in a map: go to the firing range (Play > Firing Range)", false);
        return;
    }
    std::string error;
    if(!loadGhost(pathOf(*t), error))
    {
        say(error, false);
        return;
    }
    ghost.name = t->name;
    ghost.label = t->label;
    ghost.verdict = t->status && !t->stale ? t->status->verdict + (t->status->reason.empty() ? "" : ": " + t->status->reason)
                                           : std::string{"not evaluated"};
    ghost.replayEvents = t->status && !t->stale ? replayEventsOf(t->status->events) : std::vector<ReplayEvent>{};
    ghost.world = cl.worldmodel;
    ghost.active = true;
    ghost.anchored = false;
    ghost.hasTarget = false;
    ghost.holdUntil = 0.0;
    restartGhost();
    say("ghost: " + t->name + (ghost.hasMon ? "" : " (no dummy in the take: shown around you)"), true);
}

void stopGhost()
{
    if(ghost.active)
    {
        ghost.active = false;
        say("ghost stopped", true);
    }
}

void replayMock()
{
    const Take* t = picked();
    if(!t)
    {
        say("no take picked", false);
        return;
    }
    const Backend* be = backend();
    if(!be || strcmp(be->name(), "mock") != 0)
    {
        say("Replay drives the tracking: the mock headset only (in the headset: Play Ghost)", false);
        return;
    }
    Cbuf_InsertText(va("vr_motion_play \"%s\" watch\n", pathOf(*t).generic_string().c_str())); // (next: before a script's rest)
    say("replaying " + t->name + " in the mock headset", true);
}

void keep()
{
    const Take* t = picked();
    if(!t || t->discarded)
    {
        say(t ? "a discarded take: Restore it first" : "no take picked", false);
        return;
    }
    const Mark before = t->mark ? *t->mark : Mark{};
    const std::string name = t->name;
    if(before.decision == "keep")
    {
        marks.erase(name);
        journal.push_back({now(), "keep", name, before.decision, before.date, "", ""});
        say("unmarked " + name, true);
    }
    else
    {
        marks[name] = {"keep", now()};
        journal.push_back({now(), "keep", name, before.decision, before.date, "", ""});
        say("kept " + name, true);
    }
    saveReview();
    changed();
}

void discard()
{
    const Take* t = picked();
    if(!t || t->discarded)
    {
        say(t ? "already discarded" : "no take picked", false);
        return;
    }
    const std::string name = t->name;
    const fs::path to = freeName(motionsPath() / "discarded", name);
    std::string error;
    if(!moveFile(motionsPath() / name, to, error))
    {
        say("can't discard: " + error, false);
        changed();
        return;
    }
    journal.push_back({now(), "discard", name, fileName(to), "", "", ""});
    saveReview();
    pickedName = fileName(to);
    pickedDiscarded = true;
    if(ghost.active && ghost.name == name)
    {
        ghost.active = false;
    }
    say("discarded " + name + " (into motions/discarded/; Undo Last or Restore brings it back)", true);
    changed();
}

void restore()
{
    const Take* t = picked();
    if(!t || !t->discarded)
    {
        say(t ? "not discarded" : "no take picked", false);
        return;
    }
    const std::string name = t->name;
    std::string error;
    if(!moveFile(motionsPath() / "discarded" / name, motionsPath() / name, error))
    {
        say("can't restore: " + error, false);
        changed();
        return;
    }
    journal.push_back({now(), "restore", name, name, "", "", ""});
    saveReview();
    pickedName = name;
    pickedDiscarded = false;
    say("restored " + name, true);
    changed();
}

void relabel()
{
    const Take* t = picked();
    if(!t || t->discarded)
    {
        say(t ? "a discarded take: Restore it first" : "no take picked", false);
        return;
    }
    const auto& list = categories();
    const Category& c = list[CLAMP(0, static_cast<int>(vr_motion_relabel_category.value), static_cast<int>(list.size()) - 1)];
    const Choice& d = c.details[CLAMP(0, static_cast<int>(vr_motion_relabel_detail.value), static_cast<int>(c.details.size()) - 1)];
    const std::string label = d.name[0] ? std::string{c.choice.name} + "_" + d.name : c.choice.name;
    if(label == t->label)
    {
        say("it is " + label + " already", false);
        return;
    }
    const std::string oldName = t->name;
    const Mark before = t->mark ? *t->mark : Mark{};
    const fs::path from = motionsPath() / oldName;

    // The take with its header's label, category and detail changed (and where it came from); the rest as it was.
    std::string text;
    if(!readWhole(from, text))
    {
        say("can't read " + oldName, false);
        return;
    }
    std::string out;
    out.reserve(text.size() + 128);
    size_t pos = 0;
    bool inHeader = true, noted = false;
    while(pos < text.size())
    {
        size_t eol = text.find('\n', pos);
        eol = eol == std::string::npos ? text.size() : eol + 1;
        const std::string line = text.substr(pos, eol - pos);
        pos = eol;
        if(!inHeader || line.empty() || line[0] != '#')
        {
            if(inHeader && !noted)
            {
                out += "# relabelled: from " + t->label + " on " + now() + " (Review Takes)\n";
                noted = true;
            }
            inHeader = false;
            out += line;
            continue;
        }
        const std::string key = trim(line.substr(1, line.find(':') == std::string::npos ? 0 : line.find(':') - 1));
        const std::string nl = line.size() >= 2 && line[line.size() - 2] == '\r' ? "\r\n" : "\n";
        if(key == "label")
        {
            out += "# label: " + label + nl;
        }
        else if(key == "category")
        {
            out += std::string{"# category: "} + c.choice.name + nl;
        }
        else if(key == "detail")
        {
            out += std::string{"# detail: "} + d.name + nl;
        }
        else if(key == "relabelled")
        {
            continue; // (the latest only)
        }
        else
        {
            out += line;
        }
    }
    const fs::path to = freeName(motionsPath(), safeLabel(label) + "_" + t->stamp + ".csv");
    if(!parseName(fileName(to)).ok)
    {
        say("can't name the relabelled take " + fileName(to), false);
        return;
    }
    std::error_code ec;
    {
        const fs::path tmp = to.string() + ".tmp";
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f.write(out.data(), static_cast<std::streamsize>(out.size()));
        f.close();
        if(!f || (fs::rename(tmp, to, ec), ec))
        {
            fs::remove(tmp, ec);
            say("can't write " + fileName(to), false);
            return;
        }
    }
    const fs::path backup = freeName(reviewPath() / "relabelled", oldName);
    std::string error;
    if(!moveFile(from, backup, error))
    {
        fs::remove(to, ec); // (the original is still where it was)
        say("can't relabel: " + error, false);
        changed();
        return;
    }
    journal.push_back({now(), "relabel", oldName, fileName(to), fileName(backup), before.decision, before.date});
    marks.erase(oldName);
    marks[fileName(to)] = {"relabel", now()};
    saveReview();
    pickedName = fileName(to);
    pickedDiscarded = false;
    say("relabelled " + oldName + " as " + label + " (" + fileName(to) + "; the original in motions/review/relabelled/)", true);
    changed();
}

void undo()
{
    ensure();
    if(journal.empty())
    {
        say("nothing to undo", false);
        return;
    }
    const Change c = journal.back();
    std::string error;
    bool ok = true;
    const auto setMark = [](const std::string& name, const std::string& decision, const std::string& date) {
        if(decision.empty())
        {
            marks.erase(name);
        }
        else
        {
            marks[name] = {decision, date};
        }
    };
    if(c.action == "keep")
    {
        setMark(c.a, c.b, c.c);
        pickedName = c.a;
        pickedDiscarded = false;
    }
    else if(c.action == "discard")
    {
        ok = moveFile(motionsPath() / "discarded" / c.b, motionsPath() / c.a, error);
        pickedName = c.a;
        pickedDiscarded = !ok;
    }
    else if(c.action == "restore")
    {
        ok = moveFile(motionsPath() / c.b, motionsPath() / "discarded" / c.a, error);
        pickedName = c.a;
        pickedDiscarded = ok;
    }
    else if(c.action == "relabel")
    {
        std::error_code ec;
        const fs::path relabelled = motionsPath() / c.b;
        ok = moveFile(reviewPath() / "relabelled" / c.c, motionsPath() / c.a, error);
        if(ok)
        {
            // The relabelled copy goes (the original, byte for byte, is back); a missing one is fine.
            fs::remove(relabelled, ec);
            marks.erase(c.b);
            setMark(c.a, c.d, c.e);
        }
        pickedName = c.a;
        pickedDiscarded = false;
    }
    if(!ok)
    {
        // (Dropped: it could never be undone now, and it would stand in front of the older ones. The files are as they
        // were: what is said names them.)
        journal.pop_back();
        saveReview();
        say("can't undo the " + c.action + " of " + c.a + ": " + error + "; dropped from the undo list (motions/review/undo.csv)",
            false);
        changed();
        return;
    }
    journal.pop_back();
    saveReview();
    say("undone: " + c.action + " " + c.a, true);
    changed();
}

namespace
{

// The row of the picked take, or where it would be (a take that left the list, discarded or kept): the row of the
// first take after it, in the list's order; `after` false: before it.
[[nodiscard]] int pickedRow(bool after)
{
    const Take* p = find(pickedName, pickedDiscarded);
    const auto key = [](const Take& t) { return std::make_tuple(categoryRank(t.category), t.stamp, t.name); };
    int row = after ? static_cast<int>(shown.size()) : -1;
    for(size_t r = 0; r < shown.size(); r++)
    {
        const Take& t = takes[shown[r]];
        if(p && &t == p)
        {
            return static_cast<int>(r);
        }
        if(p && after && key(t) > key(*p) && row == static_cast<int>(shown.size()))
        {
            row = static_cast<int>(r) - 1; // (the next is this one)
        }
        if(p && !after && key(t) < key(*p))
        {
            row = static_cast<int>(r) + 1; // (the previous is this one)
        }
    }
    return p ? row : -1;
}

void stepTake(int dir)
{
    ensure();
    if(shown.empty())
    {
        say("the list is empty", false);
        return;
    }
    const int row = pickedRow(dir > 0);
    pick(CLAMP(0, row + dir, static_cast<int>(shown.size()) - 1));
    if(ghost.active)
    {
        playGhost(); // the new one
    }
}

} // namespace

void nextTake()
{
    stepTake(1);
}

void previousTake()
{
    stepTake(-1);
}

void reevaluateTake()
{
    const Take* t = picked();
    if(!t || t->discarded)
    {
        say(t ? "a discarded take: Restore it first" : "no take picked", false);
        return;
    }
    startJob({fs::absolute(pathOf(*t)).generic_string()});
}

void reevaluateShown()
{
    ensure();
    std::vector<std::string> paths;
    for(const int i : shown)
    {
        if(!takes[i].discarded)
        {
            paths.push_back(fs::absolute(pathOf(takes[i])).generic_string());
        }
    }
    startJob(paths);
}

void stopReevaluation()
{
    if(!jobRunning)
    {
        say("no re-evaluation running", false);
        return;
    }
    child::stop();
    jobRunning = false;
    say("re-evaluation stopped (the verdicts as they were)", true);
}

} // namespace qvr::motion::review
