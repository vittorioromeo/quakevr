// vr_arenawaves.cpp -- the Survival Arena's wave presets: see vr_arenawaves.hpp.

#include "vr_arenawaves.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::arenawaves
{
namespace
{

constexpr int fileVersion = 1;
constexpr const char* const assignFile = "assign.cfg";
constexpr const char* const assignDefault = "default"; // (assign.cfg: this map and skill plays Default)
constexpr int fieldTexts = 16; // (what one QC entry look-up holds at once: VR_ARENA_PresetEntry's)

// arenawavesfield's fields (QC AR_F_*).
enum Field
{
    FieldKind,
    FieldMain,
    FieldOff,
    FieldArmour,
    FieldPieces,
    FieldCount,
    FieldDelay,
    FieldInfected,
    FieldBoss,
    FieldBad
};

struct RunState
{
    bool running{false};
    Preset preset;      // the preset QC's director reads (arenawavesload made it)
    za::String name;

    bool playOn{false}; // the editor's Play
    char playMap[64]{};
    za::String playName;
    Preset playPreset;

    za::String report;  // QC's lines this map
    char text[fieldTexts][64]{}; // arenawavesfield's answers (a ring: PR_SetEngineString keeps the pointers)
    int nextText{0};
};
RunState run;

void copyText(char* dst, size_t size, const char* src)
{
    q_strlcpy(dst, src, size);
}

template <size_t N>
void setText(char (&dst)[N], const char* src)
{
    copyText(dst, N, src);
}

[[nodiscard]] za::String folder()
{
    return za::String{com_gamedir} + "/" + (vr_arena_waves_dir.string[0] ? vr_arena_waves_dir.string : "arena_waves");
}

// Whose files where (vr_arenawaves.hpp, "Shipped and the player's"): a release build reads the shipped folder and writes
// only <folder>_user; a development build (his, the agents') has no second folder, it writes the shipped one.
[[nodiscard]] bool keepsPlayersApart()
{
#ifdef QVR_RELEASE
    return true;
#else
    return vr_arena_waves_as_release.value != 0.f;
#endif
}

[[nodiscard]] za::String playerFolder()
{
    return folder() + "_user";
}

[[nodiscard]] za::String writeFolder()
{
    return keepsPlayersApart() ? playerFolder() : folder();
}

[[nodiscard]] bool fileThere(const char* path)
{
    FILE* f = fopen(path, "rb");
    if(f)
    {
        fclose(f);
    }
    return f != nullptr;
}

[[nodiscard]] za::String presetPath(const char* name)
{
    return folder() + "/" + name + ".txt";
}

[[nodiscard]] za::String writePath(const char* name)
{
    return writeFolder() + "/" + name + ".txt";
}

// Where a preset is read from: the player's copy, else the shipped one.
[[nodiscard]] za::String readPath(const char* name)
{
    const za::String own = writePath(name);
    return fileThere(own.cStr()) ? own : presetPath(name);
}

[[nodiscard]] bool readAll(const char* path, za::String& out)
{
    FILE* f = fopen(path, "rb");
    if(!f)
    {
        return false;
    }
    char buf[4096];
    out = za::String{};
    size_t n;
    while((n = fread(buf, 1, sizeof(buf) - 1, f)) > 0)
    {
        buf[n] = '\0';
        out += buf;
    }
    fclose(f);
    return true;
}

bool writeAll(const char* path, const za::String& text)
{
    char dir[MAX_OSPATH];
    copyText(dir, sizeof(dir), path);
    if(char* slash = strrchr(dir, '/'))
    {
        *slash = '\0';
        Sys_mkdir(dir);
    }
    FILE* f = fopen(path, "wb");
    if(!f)
    {
        return false;
    }
    const size_t n = strlen(text.cStr());
    const bool ok = fwrite(text.cStr(), 1, n, f) == n;
    fclose(f);
    return ok;
}

// One line's tokens: `key=value` or a bare word; a value in double quotes may hold spaces.
struct Token
{
    char key[64]{};
    char value[64]{};
};
constexpr int maxTokens = 16;

int tokenize(const char* s, Token* tokens)
{
    int n = 0;
    while(*s && n < maxTokens)
    {
        while(*s == ' ' || *s == '\t')
        {
            s++;
        }
        if(!*s || (s[0] == '/' && s[1] == '/') || s[0] == '#')
        {
            break;
        }
        Token& t = tokens[n++];
        size_t k = 0;
        while(*s && *s != '=' && *s != ' ' && *s != '\t')
        {
            if(k + 1 < sizeof(t.key))
            {
                t.key[k++] = *s;
            }
            s++;
        }
        t.key[k] = '\0';
        size_t v = 0;
        t.value[0] = '\0';
        if(*s == '=')
        {
            s++;
            const bool quoted = *s == '"';
            if(quoted)
            {
                s++;
            }
            while(*s && (quoted ? *s != '"' : (*s != ' ' && *s != '\t')))
            {
                if(v + 1 < sizeof(t.value))
                {
                    t.value[v++] = *s;
                }
                s++;
            }
            if(quoted && *s == '"')
            {
                s++;
            }
            t.value[v] = '\0';
        }
    }
    return n;
}

void say(za::String& report, int line, const char* what, const char* detail)
{
    report += va("arena preset: line %d: %s%s%s\n", line, what, detail[0] ? ": " : "", detail);
}

[[nodiscard]] const char* afterWord(After a)
{
    return a == After::Last ? "last" : a == After::Loop ? "loop" : "end";
}

// A value quoted when it has to be.
void putValue(za::String& out, const char* key, const char* value)
{
    out += " ";
    out += key;
    out += "=";
    if(strchr(value, ' ') || !value[0])
    {
        out += "\"";
        out += value;
        out += "\"";
    }
    else
    {
        out += value;
    }
}

[[nodiscard]] int armourCode(const char* s)
{
    constexpr const char* const words[] = {"none", "green", "yellow", "red", "random"};
    for(int i = 0; i < 5; i++)
    {
        if(!strcmp(s, words[i]))
        {
            return i;
        }
    }
    return -1;
}

// The plates' mask (vr_agarmor.qc's bits: 0 the cuirass, 1 the helmet, then pairs), -1 random, -2 not understood.
[[nodiscard]] int piecesMask(const char* s)
{
    if(!strcmp(s, "full"))
    {
        return 1023;
    }
    if(!strcmp(s, "random"))
    {
        return -1;
    }
    struct Piece
    {
        const char* word;
        int bits;
    };
    constexpr Piece pieces[] = {{"chest", 1}, {"helmet", 2}, {"shoulders", 12}, {"forearms", 48}, {"thighs", 192}, {"shins", 768}};
    int mask = 0;
    while(*s)
    {
        const char* end = strchr(s, '+');
        const size_t len = end ? static_cast<size_t>(end - s) : strlen(s);
        bool known = false;
        for(const Piece& p : pieces)
        {
            if(strlen(p.word) == len && !strncmp(p.word, s, len))
            {
                mask |= p.bits;
                known = true;
            }
        }
        if(!known)
        {
            return -2;
        }
        s += len + (end ? 1 : 0);
    }
    return mask > 0 ? mask : -2;
}

// Assign lines.
struct AssignLine
{
    char map[64]{};
    int skill{0};
    char name[64]{};
};

[[nodiscard]] za::Vector<AssignLine> readAssignFile(const za::String& dir)
{
    za::Vector<AssignLine> lines;
    za::String text;
    if(!readAll((dir + "/" + assignFile).cStr(), text))
    {
        return lines;
    }
    const char* s = text.cStr();
    while(*s)
    {
        const char* end = strchr(s, '\n');
        const size_t len = end ? static_cast<size_t>(end - s) : strlen(s);
        char line[192];
        copyText(line, za::min(sizeof(line), len + 1), s);
        s += len + (end ? 1 : 0);
        Token t[maxTokens];
        const int n = tokenize(line, t);
        if(n != 2)
        {
            continue;
        }
        char* head = t[0].key;
        char* dot = strrchr(head, '.');
        if(!dot || dot[1] < '0' || dot[1] > '3' || dot[2])
        {
            continue;
        }
        AssignLine a;
        a.skill = dot[1] - '0';
        *dot = '\0';
        setText(a.map, head);
        setText(a.name, t[1].key);
        lines.pushBack(a);
    }
    return lines;
}

// The assignments in force: the shipped ones, the player's over them line by line (a `default` line of the player's: a
// shipped one undone).
[[nodiscard]] za::Vector<AssignLine> readAssign()
{
    za::Vector<AssignLine> lines = readAssignFile(folder());
    if(!keepsPlayersApart())
    {
        return lines;
    }
    for(const AssignLine& own : readAssignFile(playerFolder()))
    {
        za::SizeT i = 0;
        while(i < lines.size() && !(lines[i].skill == own.skill && !strcmp(lines[i].map, own.map)))
        {
            i++;
        }
        if(i < lines.size())
        {
            lines.erase(lines.begin() + i);
        }
        if(strcmp(own.name, assignDefault) != 0)
        {
            lines.insert(lines.begin() + i, own);
        }
    }
    return lines;
}

// Which preset QC's arena plays here.
[[nodiscard]] bool startPreset(int skill)
{
    run.running = false;
    run.report = za::String{};
    if(run.playOn && strcmp(run.playMap, sv.name) != 0)
    {
        run.playOn = false; // (another map: the editor's Play is over)
    }
    if(run.playOn)
    {
        run.preset = run.playPreset;
        run.name = run.playName;
        run.running = !run.preset.waves.empty();
    }
    else
    {
        const za::String name = assigned(sv.name, skill);
        if(!name.empty())
        {
            za::String why;
            Preset p;
            if(load(name.cStr(), p, why))
            {
                run.preset = static_cast<Preset&&>(p);
                run.name = name;
                run.running = !run.preset.waves.empty();
            }
            run.report += why;
            Con_Printf("%s", why.cStr());
            if(!run.running)
            {
                Con_Printf("arena preset: %s for %s.%d can't play: Default\n", name.cStr(), sv.name, skill);
            }
        }
    }
    if(run.running)
    {
        Con_Printf("arena preset: %s plays in %s (skill %d)\n", run.name.cStr(), sv.name, skill);
    }
    return run.running;
}

[[nodiscard]] const char* fieldText(const char* s)
{
    char* t = run.text[run.nextText];
    run.nextText = (run.nextText + 1) % fieldTexts;
    copyText(t, sizeof(run.text[0]), s);
    return t;
}

[[nodiscard]] const char* fieldOf(int wave, int entry, int field)
{
    if(wave == -2)
    {
        return mapListed(sv.name) ? "1" : "0";
    }
    if(wave == -1)
    {
        return va("%d", static_cast<int>(run.preset.after));
    }
    if(!run.running || wave < 0 || wave >= static_cast<int>(run.preset.waves.size()))
    {
        return "";
    }
    const Wave& w = run.preset.waves[static_cast<za::SizeT>(wave)];
    if(entry < 0)
    {
        return va("%d", field == 1 ? w.pool : static_cast<int>(w.entries.size()));
    }
    if(entry >= static_cast<int>(w.entries.size()))
    {
        return "";
    }
    const Entry& e = w.entries[static_cast<za::SizeT>(entry)];
    switch(field)
    {
    case FieldKind: return e.kind;
    case FieldMain: return e.mainHand;
    case FieldOff: return e.off;
    case FieldArmour: return va("%d", armourCode(e.armour));
    case FieldPieces: return va("%d", piecesMask(e.pieces));
    case FieldCount: return va("%d", e.count);
    case FieldDelay: return va("%g", e.delay);
    case FieldInfected: return e.infected;
    case FieldBoss: return va("%d", e.boss);
    case FieldBad: return badReason(e);
    default: return "";
    }
}

// ---- Commands ------------------------------------------------------------------------------------------------------

// vr_arena_play <preset | default> [wave] [map]: the preset (a saved one) plays from that wave in this arena map, or in
// `map` (the first arena map when this one isn't an arena): Debug > Tools > Arena Wave Editor's Play, from the console.
void play_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_arena_play <preset | default> [wave] [map]\n");
        return;
    }
    if(!strcmp(Cmd_Argv(1), "default"))
    {
        playOff();
        Con_Printf("arena preset: back to what's assigned\n");
        return;
    }
    Preset p;
    za::String why;
    const bool ok = load(Cmd_Argv(1), p, why);
    Con_Printf("%s", why.cStr());
    if(!ok)
    {
        return;
    }
    const char* map = Cmd_Argc() > 3 ? Cmd_Argv(3) : (sv.active && (vr_arena.value != 0.f || mapListed(sv.name) || isArenaMap(sv.name))) ? sv.name : defaultArenaMap();
    play(map, Cmd_Argv(1), p, Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 0);
}

// vr_arena_assign <map> <skill 0..3> <preset | default>: which preset plays there (the editor's row, from the console).
void assign_f()
{
    if(Cmd_Argc() < 4)
    {
        Con_Printf("vr_arena_assign <map> <skill 0..3> <preset | default>\n");
        return;
    }
    setAssigned(Cmd_Argv(1), za::clamp(Q_atoi(Cmd_Argv(2)), 0, skillCount - 1), !strcmp(Cmd_Argv(3), "default") ? "" : Cmd_Argv(3));
}

// vr_arena_preset_list: the saved presets and what's assigned where.
void list_f()
{
    for(const za::String& n : list())
    {
        Con_Printf("arena preset: file %s\n", n.cStr());
    }
    for(const AssignLine& a : readAssign())
    {
        Con_Printf("arena preset: %s.%d %s\n", a.map, a.skill, a.name);
    }
    Con_Printf("arena preset: running %s\n", run.running ? run.name.cStr() : "Default");
}

// vr_arena_waves_fixture: the tests' presets, written into the folder (vr_arena_waves_dir arena_waves_test first).
constexpr const char* const fixtureGood =
    "arena_waves 1\n"
    "after end\n"
    "wave pool=0\n"
    "  entry kind=grunt main=Nailgun off=none armour=green pieces=chest+helmet infected=none count=2 delay=0 boss=0\n"
    "  entry kind=zombie main=none off=none armour=none pieces=full infected=none count=1 delay=1 boss=0\n"
    "wave pool=3\n"
    "  entry kind=enforcer main=none off=none armour=red pieces=full infected=shambler count=1 delay=0 boss=0\n"
    "  entry kind=plain_grunt main=none off=none armour=none pieces=full infected=none count=2 delay=0 boss=0\n"
    "  entry kind=armed_knight main=Axe off=Axe armour=yellow pieces=full infected=none count=1 delay=0 boss=1\n"
    "wave pool=0\n"
    "  entry kind=dog main=none off=none armour=none pieces=full infected=none count=1 delay=0 boss=0\n";

constexpr const char* const fixtureBad =
    "// comments and blank lines aren't kept\n"
    "arena_waves 1\n"
    "\n"
    "wave\n"
    "  entry kind=gargantua count=2\n"
    "  entry kind=grunt main=\"Ray Gun\" count=1\n"
    "  entry kind=grunt main=Nailgun off=\"Ray Gun\" count=1\n"
    "  entry kind=grunt main=Nailgun armour=purple count=1\n"
    "  entry kind=grunt main=Nailgun pieces=wings count=1\n"
    "  entry kind=grunt main=Nailgun infected=gargantua count=1\n"
    "  entry kind=dog count=1\n"
    "  flurble 3\n";

void fixture_f()
{
    writeAll(presetPath("fixture_good").cStr(), za::String{fixtureGood});
    writeAll(presetPath("fixture_bad").cStr(), za::String{fixtureBad});
    Preset p;
    za::String why;
    if(parse(fixtureGood, p, why))
    {
        p.waves.resize(1);
        za::String text = write(p);
        writeAll(presetPath("fixture_othermap").cStr(), text);
    }
    Con_Printf("arena preset: fixtures written to %s\n", folder().cStr());
}

// vr_arena_waves_test: the format, in the engine alone. Ends on "arena waves test: N passed, M failed".
void test_f()
{
    int passed = 0, failed = 0;
    const auto verdict = [&](bool ok, const char* what) {
        Con_Printf("arena waves test: %s: %s\n", ok ? "PASS" : "FAIL", what);
        (ok ? passed : failed)++;
    };
    Preset p;
    za::String why;
    verdict(parse(fixtureGood, p, why) && why.empty(), "the good fixture parses with nothing to say");
    verdict(p.waves.size() == 3 && p.waves[0].entries.size() == 2 && p.waves[1].entries.size() == 3 && p.waves[1].pool == 3,
        "its waves, entries and pool");
    verdict(!strcmp(p.waves[1].entries[2].off, "Axe") && p.waves[1].entries[2].boss == 1 && p.waves[0].entries[1].delay == 1.f,
        "an entry's fields");
    verdict(!strcmp(write(p).cStr(), fixtureGood), "written again it is byte for byte the same");
    Sys_mkdir(folder().cStr());
    za::String saved, again;
    Preset q;
    const bool stored = save("fixture_roundtrip", p, why) && readAll(presetPath("fixture_roundtrip").cStr(), saved) &&
        load("fixture_roundtrip", q, why) && save("fixture_roundtrip", q, why) && readAll(presetPath("fixture_roundtrip").cStr(), again);
    verdict(stored && !strcmp(saved.cStr(), fixtureGood) && !strcmp(again.cStr(), fixtureGood), "saved, loaded, saved: the same bytes on disk");

    Preset b;
    za::String badSaid;
    verdict(parse(fixtureBad, b, badSaid) && b.waves.size() == 1 && b.waves[0].entries.size() == 7, "a file with odd entries still parses: they're the game's to judge");
    verdict(strstr(badSaid.cStr(), "flurble") != nullptr, "a line it doesn't understand is said");
    const za::String canon = write(b);
    Preset b2;
    za::String ignored;
    verdict(parse(canon.cStr(), b2, ignored) && !strcmp(write(b2).cStr(), canon.cStr()), "a hand-written file, once saved, is stable");
    verdict(badReason(b.waves[0].entries[3])[0] != 0 && badReason(b.waves[0].entries[4])[0] != 0 && !badReason(b.waves[0].entries[0])[0],
        "an unknown armour or pieces word is a reason; the kind is QC's");
    verdict(!parse("arena_waves 99\n", q, why), "a version from the future is refused");
    verdict(!parse("hello\n", q, why), "not a preset is refused");
    verdict(validName("My_waves-2") && !validName("../evil") && !validName("") && !validName("a b") && !validName("assign") && !validName("default"), "names are file names, never paths");
    verdict(piecesMask("chest+helmet") == 3 && piecesMask("full") == 1023 && piecesMask("random") == -1 && piecesMask("shins") == 768 && piecesMask("wings") == -2, "the pieces' words");

    // The assignments: keyed by map and skill, in any map's name; "" clears.
    setAssigned("vrarena", 1, "fixture_good");
    setAssigned("someothermap", 2, "fixture_othermap");
    setAssigned("vrarena", 3, "fixture_bad");
    verdict(!strcmp(assigned("vrarena", 1).cStr(), "fixture_good") && !strcmp(assigned("someothermap", 2).cStr(), "fixture_othermap") && assigned("vrarena", 0).empty(), "assign.cfg by map and skill");
    verdict(mapListed("someothermap") && !mapListed("e1m1"), "a map named there is listed");
    setAssigned("vrarena", 3, "");
    setAssigned("vrarena", 1, "");
    setAssigned("someothermap", 2, "");
    verdict(assigned("vrarena", 1).empty() && !mapListed("someothermap"), "cleared again");
    removeFile("fixture_roundtrip");

    // Shipped and the player's, as a release keeps them (vr_arena_waves_as_release): the shipped files are never written.
    Preset one;
    (void)parse(fixtureGood, one, why);
    one.waves.resize(1);
    (void)save("fixture_shipped", p, why); // (a development build: the shipped folder)
    setAssigned("vrarena", 1, "fixture_shipped");
    setAssigned("vrarena", 2, "fixture_shipped");
    za::String shippedBytes, shippedAssign, now;
    (void)readAll(presetPath("fixture_shipped").cStr(), shippedBytes);
    (void)readAll((folder() + "/" + assignFile).cStr(), shippedAssign);
    Cvar_SetValueQuick(&vr_arena_waves_as_release, 1.f);
    Preset read;
    verdict(load("fixture_shipped", read, why) && read.waves.size() == p.waves.size() && !removable("fixture_shipped"),
        "a player reads a shipped preset, and can't delete it");
    verdict(save("fixture_shipped", one, why) && fileThere(writePath("fixture_shipped").cStr()) && load("fixture_shipped", read, why) &&
            read.waves.size() == 1 && readAll(presetPath("fixture_shipped").cStr(), now) && !strcmp(now.cStr(), shippedBytes.cStr()),
        "saving over it keeps a copy of their own, which wins; the shipped file's bytes the same");
    int seen = 0;
    for(const za::String& n : list())
    {
        seen += !strcmp(n.cStr(), "fixture_shipped");
    }
    verdict(seen == 1, "the list has the name once");
    verdict(removable("fixture_shipped") && removeFile("fixture_shipped") && load("fixture_shipped", read, why) && read.waves.size() == p.waves.size(),
        "deleting their copy brings the shipped one back");
    setAssigned("vrarena", 1, "fixture_good");
    setAssigned("vrarena", 2, "");
    setAssigned("anothermap", 0, "fixture_good");
    verdict(!strcmp(assigned("vrarena", 1).cStr(), "fixture_good") && assigned("vrarena", 2).empty() && !strcmp(assigned("anothermap", 0).cStr(), "fixture_good"),
        "their assignments win over the shipped ones, a cleared one too");
    verdict(readAll((folder() + "/" + assignFile).cStr(), now) && !strcmp(now.cStr(), shippedAssign.cStr()), "the shipped assign.cfg's bytes the same");
    setAssigned("vrarena", 1, "");
    setAssigned("anothermap", 0, "");
    verdict(assigned("vrarena", 1).empty() && !mapListed("anothermap"), "cleared again");
    Cvar_SetValueQuick(&vr_arena_waves_as_release, 0.f);
    setAssigned("vrarena", 1, "");
    setAssigned("vrarena", 2, "");
    removeFile("fixture_shipped");
    qvr::testkit::verdict("arena waves test", passed, failed);
}

} // namespace

// ---- The text ------------------------------------------------------------------------------------------------------

bool parse(const char* text, Preset& out, za::String& report)
{
    out = Preset{};
    int lineNo = 0;
    bool header = false;
    const char* s = text;
    while(*s)
    {
        const char* end = strchr(s, '\n');
        const size_t len = end ? static_cast<size_t>(end - s) : strlen(s);
        char line[512];
        copyText(line, za::min(sizeof(line), len + 1), s);
        s += len + (end ? 1 : 0);
        lineNo++;
        const size_t l = strlen(line);
        if(l > 0 && line[l - 1] == '\r')
        {
            line[l - 1] = '\0';
        }
        Token t[maxTokens];
        const int n = tokenize(line, t);
        if(n == 0)
        {
            continue;
        }
        const char* word = t[0].key;
        if(!header)
        {
            if(strcmp(word, "arena_waves") != 0 || n < 2)
            {
                say(report, lineNo, "not an arena preset (it starts with arena_waves <version>)", "");
                return false;
            }
            if(Q_atoi(t[1].key) < 1 || Q_atoi(t[1].key) > fileVersion)
            {
                say(report, lineNo, "a version this game doesn't know", t[1].key);
                return false;
            }
            header = true;
        }
        else if(!strcmp(word, "after") && n >= 2)
        {
            if(!strcmp(t[1].key, "end")) { out.after = After::End; }
            else if(!strcmp(t[1].key, "last")) { out.after = After::Last; }
            else if(!strcmp(t[1].key, "loop")) { out.after = After::Loop; }
            else { say(report, lineNo, "an unknown after", t[1].key); }
        }
        else if(!strcmp(word, "wave"))
        {
            if(static_cast<int>(out.waves.size()) >= maxWaves)
            {
                say(report, lineNo, "too many waves", "");
                continue;
            }
            Wave w;
            for(int i = 1; i < n; i++)
            {
                if(!strcmp(t[i].key, "pool")) { w.pool = za::clamp(Q_atoi(t[i].value), 0, maxCount); }
                else { say(report, lineNo, "an unknown word on a wave", t[i].key); }
            }
            out.waves.pushBack(static_cast<Wave&&>(w));
        }
        else if(!strcmp(word, "entry"))
        {
            if(out.waves.empty())
            {
                say(report, lineNo, "an entry before any wave", "");
                continue;
            }
            Wave& w = out.waves.back();
            if(static_cast<int>(w.entries.size()) >= maxEntries)
            {
                say(report, lineNo, "too many entries in this wave", "");
                continue;
            }
            Entry e;
            for(int i = 1; i < n; i++)
            {
                const char* k = t[i].key;
                const char* v = t[i].value;
                if(!strcmp(k, "kind")) { setText(e.kind, v); }
                else if(!strcmp(k, "main")) { setText(e.mainHand, v); }
                else if(!strcmp(k, "off")) { setText(e.off, v); }
                else if(!strcmp(k, "armour")) { setText(e.armour, v); }
                else if(!strcmp(k, "pieces")) { setText(e.pieces, v); }
                else if(!strcmp(k, "infected")) { setText(e.infected, v); }
                else if(!strcmp(k, "count")) { e.count = za::clamp(Q_atoi(v), 0, maxCount); }
                else if(!strcmp(k, "delay")) { e.delay = za::clamp(Q_atof(v), 0.f, 240.f); }
                else if(!strcmp(k, "boss")) { e.boss = Q_atoi(v) != 0 ? 1 : 0; }
                else { say(report, lineNo, "an unknown word on an entry", k); }
            }
            w.entries.pushBack(e);
        }
        else
        {
            say(report, lineNo, "a line this game doesn't understand", word);
        }
    }
    if(!header)
    {
        say(report, lineNo, "not an arena preset (it's empty)", "");
        return false;
    }
    return true;
}

za::String write(const Preset& p)
{
    za::String out;
    out += va("arena_waves %d\n", fileVersion);
    out += va("after %s\n", afterWord(p.after));
    for(const Wave& w : p.waves)
    {
        out += va("wave pool=%d\n", w.pool);
        for(const Entry& e : w.entries)
        {
            out += "  entry";
            putValue(out, "kind", e.kind);
            putValue(out, "main", e.mainHand);
            putValue(out, "off", e.off);
            putValue(out, "armour", e.armour);
            putValue(out, "pieces", e.pieces);
            putValue(out, "infected", e.infected);
            out += va(" count=%d delay=%g boss=%d\n", e.count, e.delay, e.boss);
        }
    }
    return out;
}

const char* badReason(const Entry& e)
{
    if(armourCode(e.armour) < 0)
    {
        return "an unknown armour";
    }
    if(piecesMask(e.pieces) == -2)
    {
        return "unknown armour pieces";
    }
    return "";
}

bool validName(const char* name)
{
    const size_t n = strlen(name);
    if(n == 0 || n > 48 || !strcmp(name, "assign") || !strcmp(name, assignDefault))
    {
        return false;
    }
    for(size_t i = 0; i < n; i++)
    {
        const char c = name[i];
        if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
        {
            return false;
        }
    }
    return true;
}

// ---- The files -----------------------------------------------------------------------------------------------------

bool load(const char* name, Preset& out, za::String& report)
{
    if(!validName(name))
    {
        report += va("arena preset: \"%s\" is not a preset's name\n", name);
        return false;
    }
    za::String text;
    if(!readAll(readPath(name).cStr(), text))
    {
        report += va("arena preset: %s not found (%s)\n", name, readPath(name).cStr());
        return false;
    }
    return parse(text.cStr(), out, report);
}

bool save(const char* name, const Preset& p, za::String& report)
{
    if(!validName(name))
    {
        report += va("arena preset: \"%s\" is not a preset's name\n", name);
        return false;
    }
    if(!writeAll(writePath(name).cStr(), write(p)))
    {
        report += va("arena preset: can't write %s\n", writePath(name).cStr());
        return false;
    }
    return true;
}

bool removeFile(const char* name)
{
    return validName(name) && remove(writePath(name).cStr()) == 0;
}

bool removable(const char* name)
{
    return validName(name) && fileThere(writePath(name).cStr());
}

za::Vector<za::String> list()
{
    za::Vector<za::String> names;
    const auto addFrom = [&names](const za::String& dir) {
        for(findfile_t* find = Sys_FindFirst(dir.cStr(), "txt"); find; find = Sys_FindNext(find))
        {
            if(!(find->attribs & FA_DIRECTORY))
            {
                char base[MAX_QPATH];
                COM_StripExtension(find->name, base, sizeof(base));
                if(validName(base))
                {
                    // (kept sorted, a name once: a player's copy of a shipped one)
                    za::SizeT at = 0;
                    while(at < names.size() && q_strcasecmp(names[at].cStr(), base) < 0)
                    {
                        at++;
                    }
                    if(at == names.size() || q_strcasecmp(names[at].cStr(), base) != 0)
                    {
                        names.insert(names.begin() + at, za::String{base});
                    }
                }
            }
        }
    };
    addFrom(folder());
    if(keepsPlayersApart())
    {
        addFrom(playerFolder());
    }
    return names;
}

za::String assigned(const char* map, int skill)
{
    for(const AssignLine& a : readAssign())
    {
        if(a.skill == skill && !strcmp(a.map, map))
        {
            return za::String{a.name};
        }
    }
    return za::String{};
}

void setAssigned(const char* map, int skill, const char* nameOrNone)
{
    // Only the writer's own file changes: clearing what a shipped line says takes a line of its own (readAssign).
    const za::String dir = writeFolder();
    za::Vector<AssignLine> lines = readAssignFile(dir);
    const auto shipped = readAssignFile(folder());
    const bool shippedLine = keepsPlayersApart() &&
        za::anyOf(shipped.begin(), shipped.end(), [&](const AssignLine& a) { return a.skill == skill && !strcmp(a.map, map); });
    const char* name = !nameOrNone[0] && shippedLine ? assignDefault : nameOrNone;
    bool found = false;
    for(za::SizeT i = 0; i < lines.size();)
    {
        if(lines[i].skill == skill && !strcmp(lines[i].map, map))
        {
            if(name[0] && !found)
            {
                setText(lines[i].name, name);
                found = true;
                i++;
            }
            else
            {
                lines.erase(lines.begin() + i);
            }
        }
        else
        {
            i++;
        }
    }
    if(name[0] && !found)
    {
        AssignLine a;
        setText(a.map, map);
        a.skill = skill;
        setText(a.name, name);
        za::SizeT at = 0;
        while(at < lines.size() && (strcmp(lines[at].map, a.map) < 0 || (!strcmp(lines[at].map, a.map) && lines[at].skill < a.skill)))
        {
            at++;
        }
        lines.insert(lines.begin() + at, a);
    }
    za::String out{"// The arena's wave presets by map and difficulty: <map>.<skill> <preset> (0 Easy, 1 Normal, 2 Hard, 3 Nightmare).\n"};
    for(const AssignLine& a : lines)
    {
        out += va("%s.%d %s\n", a.map, a.skill, a.name);
    }
    writeAll((dir + "/" + assignFile).cStr(), out);
}

// The arena maps (the header's note).
static constexpr const char* arenaMapNames[] = {"vrarena"};

bool isArenaMap(const char* map)
{
    for(const char* name : arenaMapNames)
    {
        if(!strcmp(name, map))
        {
            return true;
        }
    }
    return false;
}

za::Span<const char* const> arenaMaps()
{
    return {arenaMapNames, sizeof(arenaMapNames) / sizeof(arenaMapNames[0])};
}

const char* defaultArenaMap()
{
    return arenaMapNames[0];
}

bool mapListed(const char* map)
{
    for(const AssignLine& a : readAssign())
    {
        if(!strcmp(a.map, map))
        {
            return true;
        }
    }
    return false;
}

za::Vector<za::String> listedMaps()
{
    za::Vector<za::String> maps;
    for(const AssignLine& a : readAssign())
    {
        bool seen = false;
        for(const za::String& m : maps)
        {
            seen = seen || !strcmp(m.cStr(), a.map);
        }
        if(!seen)
        {
            maps.pushBack(za::String{a.map});
        }
    }
    return maps;
}

// ---- Play ----------------------------------------------------------------------------------------------------------

void play(const char* map, const char* name, const Preset& p, int wave)
{
    char target[64];
    copyText(target, sizeof(target), map); // (`map` may be sv.name, `name` a command's argument)
    run.playOn = true;
    setText(run.playMap, target);
    run.playName = za::String{name};
    run.playPreset = p;
    if(wave >= 1)
    {
        Cvar_SetValueQuick(&vr_arena_goto, static_cast<float>(wave)); // (the arena's jump, its first frame)
    }
    Cbuf_InsertText(va("map %s\n", target)); // (next: what follows in a script runs in the map)
    Con_Printf("arena preset: %s plays in %s from wave %d\n", name, target, za::max(wave, 1));
}

void playOff()
{
    run.playOn = false;
}

bool playing()
{
    return run.playOn;
}

const za::String& lastReport()
{
    return run.report;
}

void registerCommands()
{
    Cmd_AddCommand("vr_arena_play", play_f);
    Cmd_AddCommand("vr_arena_assign", assign_f);
    Cmd_AddCommand("vr_arena_preset_list", list_f);
    Cmd_AddCommand("vr_arena_waves_fixture", fixture_f);
    Cmd_AddCommand("vr_arena_waves_test", test_f);
}

// ---- QC's ----------------------------------------------------------------------------------------------------------

void pfLoad()
{
    const int skill = za::clamp(static_cast<int>(G_FLOAT(OFS_PARM0)), 0, skillCount - 1);
    G_FLOAT(OFS_RETURN) = startPreset(skill) ? static_cast<float>(run.preset.waves.size()) : 0.f;
}

void pfIsArenaMap()
{
    G_FLOAT(OFS_RETURN) = isArenaMap(G_STRING(OFS_PARM0)) ? 1.f : 0.f;
}

void pfField()
{
    const int wave = static_cast<int>(G_FLOAT(OFS_PARM0));
    const int entry = static_cast<int>(G_FLOAT(OFS_PARM1));
    const int field = static_cast<int>(G_FLOAT(OFS_PARM2));
    G_INT(OFS_RETURN) = PR_SetEngineString(fieldText(fieldOf(wave, entry, field)));
}

void pfReport()
{
    const char* s = G_STRING(OFS_PARM0);
    Con_Printf("%s", s);
    run.report += s;
}

} // namespace qvr::arenawaves
