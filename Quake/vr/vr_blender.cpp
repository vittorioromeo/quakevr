// vr_blender.cpp -- the game's side of the Blender workflow (docs/vr-port/BLENDER_WORKFLOW.md).
//
// vr_edit_in_blender [model | hand | look] [-n]: opens the model's source .blend in Blender (vr_blender_path; empty: the
// newest "Program Files/Blender Foundation/Blender X.Y/blender.exe"). The model: the one named (v_shot, progs/v_shot.mdl),
// else the weapon in the main hand, else the one looked at (the drawn alias model nearest the view's line). Its source:
// Misc/quakevr/generated.json's "source" for quakevr/progs/<model>.mdl (a part or a derived model gives the .blend that
// writes it: vr_pump_on_v_shot gives v_shot.blend, vrgrunt soldier.blend), else src_models/blend/<model>/<model>.blend;
// a model with neither (the jointed hand, the body) is imported into a new Blender scene by the add-on
// (quakevr_models.edit_from_game): saved inside the repository, that .blend exports on save. The repository is the game
// folder's own (quakevr/ resolved through links: its parent). -n prints what it would run.
//
// vr_edit_skin [model | hand | look] [skin] [-n]: as vr_edit_in_blender, then the add-on's Edit Skin Externally (the
// skin's PNG in your paint program, watched: saved there, the .blend saves and exports, the game reads it again).
//
// vr_model_show [model]: the model drawn in front of you (where you look, at arm's length past its size), turning, its
// file read again first; no model: hidden. Not the body or the hands (they are drawn on you).
//
// vr_blender_link 1 (the default): the add-on's Show in Game writes quakevr/blender_request.txt; the game reads it every
// half second of real time (one file time asked: nothing else) and runs its lines, only these two: `show <model>`
// (vr_model_show) and `reload <model>...` (vr_model_reload). Anything else is printed and ignored; a model name is
// letters, digits and _ . / - only. A request already there when the game starts is not run.

#include "vr_blender.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_strings.hpp"
#include "vr_testkit.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace qvr::blender
{

namespace
{

constexpr double pollSeconds = 0.5;               // the request file's time asked this often (real time)
constexpr const char* requestName = "blender_request.txt"; // in the game folder (the add-on's REQUEST_FILE)
constexpr float showSpin = 40.f;                  // vr_model_show's model turns this many degrees a second
constexpr float lookReach = 2000.f;               // vr_edit_in_blender look: models this far at most

struct Link
{
    bool started{false};
    double nextPoll{0.0};
    za::I64 seen{0}; // the request file's time last run (or found at the start)
};

struct Preview
{
    entity_t ent{};
    bool on{false};
    qmodel_t* world{nullptr}; // the map it was shown in: another map hides it
    float yaw{0.f};
    za::String name;
};

Link link;
Preview preview;

// What vr_edit_in_blender runs: the program and its arguments, and what it opens.
struct Edit
{
    za::String model;   // progs/<model>.mdl's <model>
    za::String opens;   // the .blend, or the file the add-on imports
    bool imports{false};
    za::String why;     // an error when `opens` is empty
};

[[nodiscard]] bool validName(za::StringView s)
{
    if(s.empty() || s.size() > 63)
    {
        return false;
    }
    for(const char c : s)
    {
        if(!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '.' && c != '/' && c != '-')
        {
            return false;
        }
    }
    return s.find("..") == za::StringView::nPos;
}

// "progs/v_shot.mdl", "v_shot.mdl", "V_SHOT": "v_shot".
[[nodiscard]] za::String modelName(za::StringView s)
{
    za::StringView v = files::fileName(s);
    if(v.size() > 4 && q_strcasecmp(za::String{v.substrByPosLen(v.size() - 4)}.cStr(), ".mdl") == 0)
    {
        v.removeSuffix(4);
    }
    za::String out{v};
    for(char& c : out)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// The game folder as on disk (links followed), and the repository holding it ("" when it isn't in one).
[[nodiscard]] za::String gameFolder()
{
    return files::resolved(com_gamedir);
}

[[nodiscard]] za::String repoRoot()
{
    const za::String game = gameFolder();
    const za::String root{files::parentPath(game)};
    return files::isFile(files::join(root, "Misc/quakevr/blend_export.py").cStr()) ? root : za::String{};
}

// generated.json's "source" of quakevr/progs/<model>.mdl ("" when it has none).
[[nodiscard]] za::String recordedSource(const za::String& root, const za::String& model)
{
    za::String text;
    if(!files::readText(files::join(root, "Misc/quakevr/generated.json").cStr(), text))
    {
        return {};
    }
    const za::StringView all{text};
    const za::String key = "\"quakevr/progs/" + model + ".mdl\":";
    const za::SizeT at = all.find(za::StringView{key});
    if(at == za::StringView::nPos)
    {
        return {};
    }
    const za::SizeT end = all.find('}', at);
    const za::SizeT src = all.find(za::StringView{"\"source\": \""}, at);
    if(src == za::StringView::nPos || (end != za::StringView::nPos && src > end))
    {
        return {};
    }
    const za::SizeT from = src + 11;
    const za::SizeT to = all.find('"', from);
    return to == za::StringView::nPos ? za::String{} : files::join(root, all.substrByPosLen(from, to - from));
}

[[nodiscard]] Edit resolve(const za::String& model)
{
    Edit e;
    e.model = model;
    const za::String root = repoRoot();
    if(root.empty())
    {
        e.why = "the game folder (" + gameFolder() + ") isn't a checkout's quakevr: no sources to open";
        return e;
    }
    za::String blend = recordedSource(root, model);
    if(blend.empty() || !files::isFile(blend.cStr()))
    {
        blend = files::join(root, "Misc/quakevr/src_models/blend/" + model + "/" + model + ".blend");
    }
    if(files::isFile(blend.cStr()))
    {
        e.opens = blend;
        return e;
    }
    // No source .blend yet: the game's own file, imported by the add-on.
    for(const char* ext : {".md5mesh", ".mdl"})
    {
        const za::String f = files::join(root, "quakevr/progs/" + model + ext);
        if(files::isFile(f.cStr()))
        {
            e.opens = f;
            e.imports = true;
            return e;
        }
    }
    e.why = model + ": no source .blend and no quakevr/progs/" + model + ".mdl in " + root;
    return e;
}

// vr_blender_path, else the newest Blender in Program Files ("" when none).
[[nodiscard]] za::String blenderPath()
{
    if(vr_blender_path.string && vr_blender_path.string[0])
    {
        return za::String{vr_blender_path.string};
    }
#ifdef _WIN32
    const char* pf = getenv("ProgramFiles");
    const za::String dir = files::join(pf && pf[0] ? pf : "C:/Program Files", "Blender Foundation");
    za::String best;
    int bestVersion = -1;
    files::forEachEntry(dir.cStr(), [&](const char* name, const bool isDir) {
        int major = 0, minor = 0;
        if(!isDir || sscanf(name, "Blender %d.%d", &major, &minor) < 1)
        {
            return;
        }
        const za::String exe = files::join(files::join(dir, name), "blender.exe");
        if(major * 1000 + minor > bestVersion && files::isFile(exe.cStr()))
        {
            bestVersion = major * 1000 + minor;
            best = exe;
        }
    });
    return files::generic(best);
#else
    return za::String{"blender"};
#endif
}

[[nodiscard]] za::String commandLine(const za::String& blender, const Edit& e)
{
    if(!e.imports)
    {
        return "\"" + blender + "\" \"" + e.opens + "\"";
    }
    return "\"" + blender + "\" --python-expr \"import quakevr_models as q; q.edit_from_game(r'" + e.opens + "')\"";
}

// vr_edit_skin's: the source .blend opened, then the add-on's Edit Skin Externally on skin `skin` (-1: the panel's).
[[nodiscard]] za::String skinCommandLine(const za::String& blender, const Edit& e, const int skin)
{
    char expr[96];
    snprintf(expr, sizeof(expr), "import quakevr_models as q; q.edit_skin_from_game(%d)", skin);
    return "\"" + blender + "\" \"" + e.opens + "\" --python-expr \"" + expr + "\"";
}

[[nodiscard]] bool launch(const za::String& cmd, const za::String& dir)
{
#ifdef _WIN32
    za::Vector<wchar_t> wcmd(cmd.size() * 2 + 2, 0);
    za::Vector<wchar_t> wdir(dir.size() * 2 + 2, 0);
    if(!MultiByteToWideChar(CP_UTF8, 0, cmd.cStr(), -1, wcmd.data(), static_cast<int>(wcmd.size())) ||
        !MultiByteToWideChar(CP_UTF8, 0, dir.cStr(), -1, wdir.data(), static_cast<int>(wdir.size())))
    {
        return false;
    }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if(!CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, wdir.data(), &si, &pi))
    {
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    (void)dir;
    return system((cmd + " &").cStr()) == 0;
#endif
}

[[nodiscard]] float modelRadius(const qmodel_t* m)
{
    return 0.5f * glm::distance(glm::vec3{m->mins[0], m->mins[1], m->mins[2]},
                      glm::vec3{m->maxs[0], m->maxs[1], m->maxs[2]});
}

// The drawn alias model nearest the view's line (not the hands, the body, a held weapon or the shown model).
[[nodiscard]] const entity_t* lookedAt()
{
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const glm::vec3 fwd{vpn[0], vpn[1], vpn[2]};
    const entity_t* best = nullptr;
    float bestScore = 1e9f;
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        bool weapon = false;
        if(!e || !e->model || e->model->type != mod_alias || e == &preview.ent || e == &cl.viewent ||
            VR_IsViewEntity(e) || view::handOf(e, weapon) >= 0)
        {
            continue;
        }
        const glm::vec3 to = glm::vec3{e->origin[0], e->origin[1], e->origin[2]} - eye;
        const float along = glm::dot(to, fwd);
        if(along <= 0.f || along > lookReach)
        {
            continue;
        }
        const float off = glm::length(to - fwd * along);
        const float size = za::max(modelRadius(e->model), 8.f);
        if(off < size * 1.5f && off / size < bestScore)
        {
            bestScore = off / size;
            best = e;
        }
    }
    return best;
}

// The model vr_edit_in_blender works on: `what` named, "hand" (the main hand's weapon), "look", or "" (the hand's, else
// the one looked at). "" when there is none. skin (optional): the skin the looked-at entity wears (else left as is).
[[nodiscard]] za::String pickModel(const char* what, int* skin = nullptr)
{
    const bool hand = !what[0] || q_strcasecmp(what, "hand") == 0;
    const bool look = !what[0] || q_strcasecmp(what, "look") == 0;
    if(!hand && !look)
    {
        return modelName(what);
    }
    if(hand)
    {
        if(const view::ViewEntity* w = view::heldWeapon(HAND_MAIN); w && w->ent.model)
        {
            return modelName(w->ent.model->name);
        }
    }
    if(look)
    {
        if(const entity_t* e = lookedAt())
        {
            if(skin)
            {
                *skin = e->skinnum;
            }
            return modelName(e->model->name);
        }
    }
    return {};
}

void editInBlender_f()
{
    bool dry = false;
    const char* what = "";
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        if(strcmp(Cmd_Argv(i), "-n") == 0)
        {
            dry = true;
        }
        else
        {
            what = Cmd_Argv(i);
        }
    }
    const za::String model = pickModel(what);
    if(model.empty())
    {
        Con_Printf("vr_edit_in_blender: no model %s (vr_edit_in_blender <model> | hand | look)\n",
            what[0] ? "there" : "in your main hand or where you look");
        return;
    }
    const Edit e = resolve(model);
    if(e.opens.empty())
    {
        Con_Printf("vr_edit_in_blender: %s\n", e.why.cStr());
        return;
    }
    const za::String blender = blenderPath();
    if(blender.empty())
    {
        Con_Printf("vr_edit_in_blender: no Blender found in Program Files/Blender Foundation: set vr_blender_path to "
                   "its blender.exe\n");
        return;
    }
    const za::String cmd = commandLine(blender, e);
    if(dry)
    {
        Con_Printf("vr_edit_in_blender: %s: would %s %s\n  %s\n", model.cStr(), e.imports ? "import" : "open",
            e.opens.cStr(), cmd.cStr());
        return;
    }
    if(!launch(cmd, repoRoot()))
    {
        Con_Printf("vr_edit_in_blender: couldn't start %s (vr_blender_path)\n", blender.cStr());
        return;
    }
    Con_Printf("vr_edit_in_blender: %s: %s %s in Blender%s\n", model.cStr(), e.imports ? "importing" : "opening",
        e.opens.cStr(), e.imports ? " (save it inside the repository: Ctrl+S then exports it)" : "");
}

// vr_edit_skin [model | hand | look] [skin] [-n]: the model's source .blend opened in Blender, its skin's PNG in your
// paint program (the add-on's Edit Skin Externally): saved there, Blender saves the .blend, which exports, and the game
// reads the model again (vr_model_watch). The skin: the number given, else the looked-at entity's, else the panel's.
void editSkin_f()
{
    bool dry = false;
    const char* what = "";
    int skin = -1;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        const char* a = Cmd_Argv(i);
        if(strcmp(a, "-n") == 0)
        {
            dry = true;
        }
        else if(isdigit(static_cast<unsigned char>(a[0])))
        {
            skin = atoi(a);
        }
        else
        {
            what = a;
        }
    }
    int lookedSkin = -1;
    const za::String model = pickModel(what, &lookedSkin);
    if(model.empty())
    {
        Con_Printf("vr_edit_skin: no model %s (vr_edit_skin <model> | hand | look [skin])\n",
            what[0] ? "there" : "in your main hand or where you look");
        return;
    }
    if(skin < 0)
    {
        skin = lookedSkin;
    }
    const Edit e = resolve(model);
    if(e.opens.empty() || e.imports)
    {
        Con_Printf("vr_edit_skin: %s\n", e.opens.empty() ? e.why.cStr() :
            (model + ": no source .blend (vr_edit_in_blender imports it: save it inside the repository, then Edit "
                     "Skin Externally in the Quake VR panel)").cStr());
        return;
    }
    const za::String blender = blenderPath();
    if(blender.empty())
    {
        Con_Printf("vr_edit_skin: no Blender found in Program Files/Blender Foundation: set vr_blender_path to its "
                   "blender.exe\n");
        return;
    }
    const za::String cmd = skinCommandLine(blender, e, skin);
    if(dry)
    {
        Con_Printf("vr_edit_skin: %s skin %d: would open %s\n  %s\n", model.cStr(), skin, e.opens.cStr(), cmd.cStr());
        return;
    }
    if(!launch(cmd, repoRoot()))
    {
        Con_Printf("vr_edit_skin: couldn't start %s (vr_blender_path)\n", blender.cStr());
        return;
    }
    Con_Printf("vr_edit_skin: %s: opening %s in Blender, then skin %d in your paint program (-1: the panel's); save it "
               "there and the game reads the model again\n", model.cStr(), e.opens.cStr(), skin);
}

[[nodiscard]] bool drawnOnYou(const za::String& model)
{
    return strings::startsWith(model.cStr(), "hand_rig") || strings::startsWith(model.cStr(), "vrbody");
}

void hidePreview()
{
    preview.on = false;
    preview.ent.model = nullptr;
}

// Shows `name` (a model name; empty: hides). False: not shown (the reason printed).
bool showModel(const za::String& name)
{
    if(name.empty())
    {
        hidePreview();
        Con_Printf("vr_model_show: hidden\n");
        return true;
    }
    if(drawnOnYou(name))
    {
        hidePreview();
        Con_Printf("vr_model_show: %s is drawn on you (it is read again when its files change)\n", name.cStr());
        return false;
    }
    if(!cl.worldmodel || cls.state != ca_connected)
    {
        Con_Printf("vr_model_show: not in a map\n");
        return false;
    }
    const za::String path = "progs/" + name + ".mdl";
    if(!COM_FileExists(path.cStr(), nullptr))
    {
        Con_Printf("vr_model_show: no %s\n", path.cStr());
        return false;
    }
    za::Vector<za::String> names;
    names.pushBack(path);
    view::reloadModels(names); // (the file as last exported, if it was loaded before)
    qmodel_t* m = Mod_ForName(path.cStr(), false);
    if(!m || m->type != mod_alias)
    {
        Con_Printf("vr_model_show: %s isn't a model it can draw\n", path.cStr());
        return false;
    }
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    glm::vec3 fwd{vpn[0], vpn[1], 0.f};
    fwd = glm::length(fwd) > 0.01f ? glm::normalize(fwd) : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 centre = eye + fwd * za::max(40.f, modelRadius(m) * 1.6f + 16.f);
    const glm::vec3 mid{(m->mins[0] + m->maxs[0]) * 0.5f, (m->mins[1] + m->maxs[1]) * 0.5f,
        (m->mins[2] + m->maxs[2]) * 0.5f};
    preview.ent = entity_t{};
    preview.ent.model = m;
    preview.ent.scale = ENTSCALE_DEFAULT;
    preview.ent.alpha = ENTALPHA_DEFAULT;
    preview.ent.colormap = vid.colormap;
    preview.ent.lerpflags = LERP_RESETANIM;
    for(int i = 0; i < 3; i++)
    {
        preview.ent.origin[i] = centre[i] - (i == 2 ? mid[2] : 0.f); // (its middle at eye height)
    }
    preview.yaw = atan2f(-fwd.y, -fwd.x) * 180.f / static_cast<float>(M_PI) -
                  showSpin * static_cast<float>(cl.time); // (facing you now)
    preview.world = cl.worldmodel;
    preview.name = name;
    preview.on = true;
    Con_Printf("vr_model_show: %s in front of you, turning (vr_model_show: hide)\n", path.cStr());
    return true;
}

void modelShow_f()
{
    if(Cmd_Argc() > 1 && !validName(Cmd_Argv(1)))
    {
        Con_Printf("vr_model_show: not a model name: %s\n", Cmd_Argv(1));
        return;
    }
    (void)showModel(Cmd_Argc() > 1 ? modelName(Cmd_Argv(1)) : za::String{});
}

// A request line: true when it was run.
bool runRequest(za::StringView line)
{
    za::Vector<za::String> words;
    files::forPieces(line, ' ', [&](za::StringView w) {
        if(!w.empty())
        {
            words.pushBack(za::String{w});
        }
    });
    if(words.empty())
    {
        return false;
    }
    bool ok = words.size() >= 2;
    for(za::SizeT i = 1; i < words.size(); i++)
    {
        ok = ok && validName(words[i]);
    }
    if(ok && words[0] == "show" && words.size() == 2)
    {
        Con_Printf("vr_blender_link: show %s\n", words[1].cStr());
        (void)showModel(modelName(words[1]));
        return true;
    }
    if(ok && words[0] == "reload")
    {
        za::Vector<za::String> names;
        for(za::SizeT i = 1; i < words.size(); i++)
        {
            names.pushBack("progs/" + modelName(words[i]) + ".mdl");
        }
        Con_Printf("vr_blender_link: reload\n");
        view::reloadModels(names);
        return true;
    }
    Con_Printf("vr_blender_link: ignored \"%s\" (only show <model> and reload <model>...)\n", za::String{line}.cStr());
    return false;
}

[[nodiscard]] za::String requestPath()
{
    return files::join(com_gamedir, requestName);
}

// Runs the request file if it is new: the lines run.
int poll()
{
    const za::String path = requestPath();
    const za::I64 stamp = files::lastWriteTime(path.cStr());
    if(stamp == 0 || stamp == link.seen)
    {
        return 0;
    }
    link.seen = stamp;
    za::String text;
    if(!files::readText(path.cStr(), text))
    {
        return 0;
    }
    int ran = 0;
    files::forLines(za::StringView{text}, [&](za::StringView l) {
        while(!l.empty() && (l[l.size() - 1] == '\r' || l[l.size() - 1] == ' '))
        {
            l.removeSuffix(1);
        }
        ran += !l.empty() && runRequest(l) ? 1 : 0;
    });
    return ran;
}

// vr_blender_test: the model picking and sources (a weapon, a hand-made gun, a derived model, a part, a prop, the
// jointed hand), Blender found, and the request file (a show, a reload, a line refused), with a verdict line.
void test_f()
{
    constexpr const char* name = "vr_blender_test";
    testkit::Tally t;
    const za::String root = repoRoot();
    t.check(!root.empty(), name, "the game folder is a checkout's quakevr");
    Con_Printf("%s: repository %s, Blender %s\n", name, root.cStr(), blenderPath().cStr());
    struct Case
    {
        const char* model;
        const char* ends; // what it opens ends so
        bool imports;
    };
    for(const Case& c : {Case{"v_shot", "src_models/blend/v_shot/v_shot.blend", false},
            Case{"progs/V_FLAREGUN.mdl", "models_src/flaregun/flaregun.blend", false},
            Case{"vrgrunt", "src_models/blend/soldier/soldier.blend", false},
            Case{"vr_pump_on_v_shot", "src_models/blend/v_shot/v_shot.blend", false},
            Case{"vr_crate1", "src_models/blend/vr_crate1/vr_crate1.blend", false},
            Case{"hand_rig", "quakevr/progs/hand_rig.md5mesh", true}})
    {
        const Edit e = resolve(modelName(c.model));
        const bool ok = strings::endsWith(e.opens.cStr(), c.ends) && e.imports == c.imports &&
                        files::isFile(e.opens.cStr());
        Con_Printf("%s: %s %s -> %s\n", name, ok ? "PASS" : "FAIL", c.model, e.opens.empty() ? e.why.cStr() : e.opens.cStr());
        (void)t.check(ok);
    }
    const za::String held = pickModel("");
    Con_Printf("%s: %s the model in the main hand: %s (the view's gun %s, %d entities drawn)\n", name,
        held == "v_shot" ? "PASS" : "FAIL", held.cStr(), cl.viewent.model ? cl.viewent.model->name : "none",
        cl_numvisedicts);
    (void)t.check(held == "v_shot");
    const Edit hand = resolve("hand_rig");
    const za::String cmd = commandLine("blender.exe", hand);
    (void)t.check(strstr(cmd.cStr(), "edit_from_game(r'") && strstr(cmd.cStr(), "hand_rig.md5mesh')"),
        name, "the hand's command line imports it through the add-on");
    const za::String skinCmd = skinCommandLine("blender.exe", resolve("armor"), 2);
    (void)t.check(strstr(skinCmd.cStr(), "armor.blend\" --python-expr") && strstr(skinCmd.cStr(), "edit_skin_from_game(2)"),
        name, "vr_edit_skin's command line opens the source and edits skin 2 through the add-on");
#ifdef _WIN32
    if(!vr_blender_path.string[0])
    {
        (void)t.check(strings::endsWith(blenderPath().cStr(), "blender.exe"), name, "Blender found in Program Files");
    }
#endif

    // The request file, as the add-on's Show in Game writes it.
    hidePreview();
    const za::String path = requestPath();
    (void)t.check(files::writeText(path.cStr(), "show v_shot\r\nexec quit\nshow ../x\nreload v_shot vrgadget\n"), name,
        "the request file written");
    link.seen = 0;
    const int ran = poll();
    (void)t.check(ran == 2, name, "two lines run, two refused");
    (void)t.check(preview.on && preview.ent.model && strcmp(preview.ent.model->name, "progs/v_shot.mdl") == 0, name,
        "show v_shot: shown");
    (void)t.check(poll() == 0, name, "the same request isn't run twice");
    (void)t.check(!showModel("vrbody") && !preview.on, name, "the body isn't shown (it is drawn on you)");
    (void)t.check(showModel("vr_crate1") && preview.on, name, "a prop shown");
    (void)showModel({});
    (void)t.check(!preview.on, name, "hidden");
    (void)files::writeText(path.cStr(), "");
    link.seen = files::lastWriteTime(path.cStr());
    t.verdict(name);
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_edit_in_blender", editInBlender_f);
    Cmd_AddCommand("vr_edit_skin", editSkin_f);
    Cmd_AddCommand("vr_model_show", modelShow_f);
    Cmd_AddCommand("vr_blender_test", test_f);
}

void frame()
{
    if(vr_blender_link.value == 0.f)
    {
        return;
    }
    const double now = Sys_DoubleTime();
    if(!link.started)
    {
        link.started = true;
        link.seen = files::lastWriteTime(requestPath().cStr()); // (one left from before: not run)
        link.nextPoll = now + pollSeconds;
        return;
    }
    if(now < link.nextPoll)
    {
        return;
    }
    link.nextPoll = now + pollSeconds;
    (void)poll();
}

void addPreview()
{
    if(!preview.on)
    {
        return;
    }
    if(cl.worldmodel != preview.world || cls.state != ca_connected)
    {
        hidePreview();
        return;
    }
    preview.ent.angles[0] = 0.f;
    preview.ent.angles[1] = preview.yaw + showSpin * static_cast<float>(cl.time);
    preview.ent.angles[2] = 0.f;
    if(cl_numvisedicts < MAX_VISEDICTS)
    {
        cl_visedicts[cl_numvisedicts++] = &preview.ent;
    }
}

} // namespace qvr::blender
