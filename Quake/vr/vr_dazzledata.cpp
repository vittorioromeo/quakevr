// vr_dazzledata.cpp -- the dazzle shield's per-kind tweaks (vr_dazzledata.hpp): the table, its file, QC's dazzletweak.
//
// The file (quakevr/data/dazzle.txt, under the last base folder that has a quakevr/ folder, as the armour's and the
// reloads'): one line a field that is not zero, `<kind> <single|dual> <field> <values>`, fields hand, hand_turn, elbow
// (three numbers), twist (one), gun (three). A field left out is zero; no file: all zero (the base pose exactly).

#include "vr_dazzledata.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_strings.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/String/String.hpp"

#include <stdio.h>
#include <string.h>

namespace qvr::dazzledata
{

namespace
{

constexpr const char* kindNames[kindCount] = {"grunt", "enforcer", "knight", "deathknight", "deathguard", "crossbowknight"};
constexpr const char* kindLabels[kindCount] = {"Grunt", "Enforcer", "Knight", "Death Knight", "AD Death Guard", "AD Crossbow Knight"};
constexpr const char* setNames[setCount] = {"single", "dual"};

struct Table
{
    Tweak tweaks[kindCount][setCount];
    bool loaded{false};
    int revision{0};
    int saved{0};
};
Table table;                // seat-shared: dazzle.txt's tweaks (the server's data, one file)
char pathText[MAX_OSPATH];  // seat-shared: dazzle.txt's path

[[nodiscard]] int indexOf(const za::StringView w, const char* const* names, int n)
{
    for(int i = 0; i < n; i++)
    {
        if(w == za::StringView{names[i]})
        {
            return i;
        }
    }
    return -1;
}

void parse(const za::StringView text)
{
    int lineNo = 0;
    files::forLines(text, [&](za::StringView line) {
        lineNo++;
        files::Words words{line};
        za::StringView kindW, setW, fieldW;
        if(!words.next(kindW) || kindW.size() == 0 || kindW[0] == '/')
        {
            return; // (blank, or a comment)
        }
        if(!words.next(setW) || !words.next(fieldW))
        {
            Con_Printf("\x02%s:%d: expected \"<kind> <single|dual> <field> <values>\"\n", pathText, lineNo);
            return;
        }
        const int kind = indexOf(kindW, kindNames, kindCount);
        const int set = indexOf(setW, setNames, setCount);
        if(kind < 0 || set < 0)
        {
            Con_Printf("\x02%s:%d: no kind or set \"%.*s %.*s\" (skipped)\n", pathText, lineNo, static_cast<int>(kindW.size()),
                kindW.data(), static_cast<int>(setW.size()), setW.data());
            return;
        }
        Tweak& t = table.tweaks[kind][set];
        glm::vec3* v = fieldW == za::StringView{"hand"} ? &t.hand
                       : fieldW == za::StringView{"hand_turn"} ? &t.handTurn
                       : fieldW == za::StringView{"elbow"} ? &t.elbow
                       : fieldW == za::StringView{"gun"} ? &t.gun
                                                          : nullptr;
        bool ok = true;
        if(v)
        {
            glm::vec3 r{0.f};
            ok = words.next(r.x) && words.next(r.y) && words.next(r.z);
            *v = ok ? r : *v;
        }
        else if(fieldW == za::StringView{"twist"})
        {
            float f = 0.f;
            ok = words.next(f);
            t.twist = ok ? f : t.twist;
        }
        else
        {
            ok = false;
        }
        if(!ok)
        {
            Con_Printf("\x02%s:%d: bad field \"%.*s\"\n", pathText, lineNo, static_cast<int>(fieldW.size()), fieldW.data());
        }
    });
}

[[nodiscard]] za::String fileText()
{
    za::String out{"// quakevr/data/dazzle.txt -- the flashlight dazzle's shield pose tweaked per armed humanoid kind (vr_dazzledata.hpp;\n"
                   "// written by the Dazzle Studio, vr_dazzlestudio). Laid on the base pose (QC VR_AGP_Shield); a field left out is zero,\n"
                   "// all zero is the base pose exactly. `<kind> <single|dual> <field> <values>`: kinds grunt enforcer knight deathknight\n"
                   "// deathguard crossbowknight; single: one weapon (or none), dual: two (vr_agrunt_dazzle_dual's shield). Fields:\n"
                   "// hand (the shielding hand moved: forward, left, up in its body, units), hand_turn (two weapons: the left one turned\n"
                   "// in that hand, pitch yaw roll degrees), elbow (added to its elbow's way '0.3 1 -0.45'), twist (degrees: the elbow\n"
                   "// turned about the shoulder-to-hand line), gun (the gun hand moved away while it shields: forward, left, up).\n"};
    for(int k = 0; k < kindCount; k++)
    {
        for(int s = 0; s < setCount; s++)
        {
            const Tweak& t = table.tweaks[k][s];
            const char* kn = kindNames[k];
            const char* sn = setNames[s];
            const auto vec = [&](const char* field, const glm::vec3& v) {
                if(v != glm::vec3{0.f})
                {
                    strings::appendf(out, "%s %s %s %g %g %g\n", kn, sn, field, v.x, v.y, v.z);
                }
            };
            vec("hand", t.hand);
            vec("hand_turn", t.handTurn);
            vec("elbow", t.elbow);
            if(t.twist != 0.f)
            {
                strings::appendf(out, "%s %s twist %g\n", kn, sn, t.twist);
            }
            vec("gun", t.gun);
        }
    }
    return out;
}

void load_f()
{
    Con_Printf("dazzledata: %s %s\n", filePath(), load() ? "read" : "not found: all zero (the base pose)");
}

void save_f()
{
    Con_Printf("dazzledata: %s %s\n", filePath(), save() ? "written" : "COULD NOT BE WRITTEN");
}

} // namespace

bool Tweak::zero() const
{
    return hand == glm::vec3{0.f} && handTurn == glm::vec3{0.f} && elbow == glm::vec3{0.f} && twist == 0.f && gun == glm::vec3{0.f};
}

const char* kindName(int kind)
{
    return kindNames[za::clamp(kind, 0, kindCount - 1)];
}

const char* kindLabel(int kind)
{
    return kindLabels[za::clamp(kind, 0, kindCount - 1)];
}

Tweak& at(int kind, int set)
{
    if(!table.loaded)
    {
        (void)load();
    }
    return table.tweaks[za::clamp(kind, 0, kindCount - 1)][za::clamp(set, 0, setCount - 1)];
}

void touched()
{
    table.revision++;
}

int revision()
{
    return table.revision;
}

int savedRevision()
{
    return table.saved;
}

const char* filePath()
{
    // (quakevr/ in the last base folder that has one, as the armour's and the reloads': the file the studio writes is the one read)
    const char* name = vr_dazzle_file.string && vr_dazzle_file.string[0] ? vr_dazzle_file.string : "data/dazzle.txt";
    for(int i = com_numbasedirs - 1; i >= 0; i--)
    {
        char dir[MAX_OSPATH];
        q_snprintf(dir, sizeof(dir), "%s/quakevr", com_basedirs[i]);
        if(files::isDirectory(dir) || i == 0)
        {
            q_snprintf(pathText, sizeof(pathText), "%s/%s", dir, name);
            return pathText;
        }
    }
    q_snprintf(pathText, sizeof(pathText), "quakevr/%s", name);
    return pathText;
}

bool load()
{
    const char* path = filePath();
    table.loaded = true;
    for(auto& row : table.tweaks)
    {
        for(Tweak& t : row)
        {
            t = Tweak{};
        }
    }
    za::String text;
    const bool found = files::readText(path, text);
    if(found)
    {
        parse(za::StringView{text.cStr()});
    }
    table.revision++;
    table.saved = table.revision;
    return found;
}

bool save()
{
    const char* path = filePath();
    char dir[MAX_OSPATH];
    q_strlcpy(dir, path, sizeof(dir));
    if(char* slash = strrchr(dir, '/'))
    {
        *slash = 0;
        (void)files::createDirectories(dir);
    }
    const bool ok = files::writeText(path, za::StringView{fileText().cStr()});
    if(ok)
    {
        table.saved = table.revision;
    }
    return ok;
}

void clear()
{
    table.loaded = true;
    for(auto& row : table.tweaks)
    {
        for(Tweak& t : row)
        {
            t = Tweak{};
        }
    }
    touched();
}

void init()
{
    Cmd_AddCommand("vr_dazzle_load", load_f);
    Cmd_AddCommand("vr_dazzle_save", save_f);
}

void PF_dazzletweak()
{
    const int kind = static_cast<int>(G_FLOAT(OFS_PARM0));
    const int set = G_FLOAT(OFS_PARM1) != 0.f ? 1 : 0;
    const int field = static_cast<int>(G_FLOAT(OFS_PARM2));
    float* out = G_VECTOR(OFS_RETURN);
    glm::vec3 v{0.f};
    if(kind >= 0 && kind < kindCount)
    {
        const Tweak& t = at(kind, set);
        v = field == 0 ? t.hand : field == 1 ? t.handTurn : field == 2 ? t.elbow : field == 3 ? glm::vec3{t.twist, 0.f, 0.f}
            : field == 4 ? t.gun : glm::vec3{0.f};
    }
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

} // namespace qvr::dazzledata
