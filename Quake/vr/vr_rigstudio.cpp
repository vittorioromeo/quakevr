// vr_rigstudio.cpp -- the Rig Studio: see vr_rigstudio.hpp (its mode, its scene, its camera's hook, the edits, undo and
// redo, saving, the commands). The viewport is vr_rigstudio_view.cpp's, the windows vr_rigstudio_ui.cpp's.

#include "vr_rigstudio_internal.hpp"
#include "vr_aggrip.hpp"
#include "vr_aglegs.hpp"
#include "vr_agpose.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_imgui.hpp"
#include "vr_progs.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigdata.hpp"
#include "vr_rigpose.hpp"
#include "vr_rigseq.hpp"
#include "vr_toolgun.hpp"
#include "vr_view.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"

#include <SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <string.h>
#include <time.h>

extern "C" int VR_IsActive(void);
extern "C" {
extern cvar_t scr_viewsize;
extern cvar_t r_drawviewmodel;
extern cvar_t r_lerpmodels;
}

using namespace qvr;
using namespace qvr::rigstudio::detail;

namespace qvr::rigstudio::detail
{
Studio st;
} // namespace qvr::rigstudio::detail

namespace
{

constexpr float deg = 3.14159265f / 180.f;
constexpr float stageAhead = 160.f; // units: the subject this far ahead of the start

// The model's frame groups (made with its geometry).
struct Groups
{
    rigdata::AnimGroup g[maxGroups];
    int n{0};
    const qmodel_t* model{nullptr};
};
Groups groups;

struct SvVm
{
    qcvm_t* old{nullptr};
    SvVm() { PR_PushQCVM(&sv.qcvm, &old); }
    ~SvVm() { PR_PopQCVM(old); }
};

[[nodiscard]] bool inGame()
{
    return sv.active && svs.maxclients >= 1 && cls.state == ca_connected && cls.signon == SIGNONS;
}

[[nodiscard]] edict_t* subjectEdict()
{
    if(st.subject <= 0 || st.subject >= sv.qcvm.num_edicts)
    {
        return nullptr;
    }
    edict_t* e = EDICT_NUM(st.subject);
    return e->free ? nullptr : e;
}

void keepSettings()
{
    if(st.keptSettings)
    {
        return;
    }
    st.keepFreeze = vr_agrunt_freeze.value;
    st.keepViewsize = scr_viewsize.value;
    st.keepCrosshair = crosshair.value;
    st.keepViewmodel = r_drawviewmodel.value;
    st.keptSettings = true;
    Cvar_SetValueQuick(&vr_agrunt_freeze, 1.f);
    Cvar_SetValue("viewsize", 120.f);
    Cvar_SetValue("crosshair", 0.f);
    Cvar_SetValue("r_drawviewmodel", 0.f);
}

void restoreSettings()
{
    if(!st.keptSettings)
    {
        return;
    }
    Cvar_SetValueQuick(&vr_agrunt_freeze, st.keepFreeze);
    Cvar_SetValue("viewsize", st.keepViewsize);
    Cvar_SetValue("crosshair", st.keepCrosshair);
    Cvar_SetValue("r_drawviewmodel", st.keepViewmodel);
    st.keptSettings = false;
    if(st.keptLayers)
    {
        Cvar_SetValueQuick(&vr_agrunt_ik, st.keepIk);
        Cvar_SetValueQuick(&vr_aglegs, st.keepLegs);
        Cvar_SetValueQuick(&vr_agrunt_upper, st.keepUpper);
        Cvar_SetValue("r_lerpmodels", st.keepLerp);
        Cvar_SetValueQuick(&vr_hit_react, st.keepKicks);
        st.keptLayers = false;
    }
}

// The stage: ahead of the player's start, on the floor.
void findStage()
{
    edict_t* p = EDICT_NUM(1);
    st.home = glm::vec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]};
    const float yaw = p->v.angles[1];
    const glm::vec3 fwd{za::cos(yaw * deg), za::sin(yaw * deg), 0.f};
    vec3_t start{st.home.x, st.home.y, st.home.z}, zero{0.f, 0.f, 0.f};
    const glm::vec3 ahead = st.home + fwd * stageAhead;
    vec3_t end{ahead.x, ahead.y, ahead.z};
    trace_t tr = SV_Move(start, zero, zero, end, MOVE_NOMONSTERS, p);
    const glm::vec3 at = tr.fraction < 1.f ? st.home + fwd * za::max(stageAhead * tr.fraction - 40.f, 48.f) : ahead;
    vec3_t from{at.x, at.y, at.z}, down{at.x, at.y, at.z - 256.f};
    tr = SV_Move(from, zero, zero, down, MOVE_NOMONSTERS, p);
    st.stage = glm::vec3{at.x, at.y, tr.fraction < 1.f ? tr.endpos[2] : at.z + p->v.mins[2]};
    st.stageYaw = yaw + 180.f;
    st.staged = true;
    st.cam.focus = st.stage + glm::vec3{0.f, 0.f, 24.f};
    st.cam.yaw = yaw + 20.f;
    st.cam.pitch = 10.f;
    st.cam.dist = 90.f;
    if(!st.aim.placed)
    {
        st.aim.at = aimDefault(); // (ahead of the subject, level; kept across its changes)
        st.aim.placed = true;
    }
}

// The subject's aim target as QC reads it (vr_rigseq.qc .vr_rs_aim, .vr_rs_aimon), the fields found once a progs.
struct AimFields
{
    const void* progs{nullptr};
    int at{-1}, on{-1};
};
AimFields aimFields;

void tellAim(edict_t* e)
{
    if(aimFields.progs != sv.qcvm.progs)
    {
        aimFields.progs = sv.qcvm.progs;
        aimFields.at = ED_FindFieldOffset("vr_rs_aim");
        aimFields.on = ED_FindFieldOffset("vr_rs_aimon");
    }
    progs::setFieldVec(e, aimFields.at, st.aim.at);
    progs::setFieldFloat(e, aimFields.on, st.aim.on ? 1.f : 0.f);
}

[[nodiscard]] bool armedClass(const char* spawn)
{
    return !strncmp(spawn, "monster_vr_", 11);
}

// The subject gone with everything it owns (its weapons and their records, a worn tank: QC VR_RigStudio_Remove), and
// the studio's own hold on it dropped (its edits kept: they are the model's, not the entity's).
void removeSubject()
{
    if(!sv.active)
    {
        st.subject = 0;
        return;
    }
    const SvVm vm;
    edict_t* e = subjectEdict();
    const func_t fn = progs::findFunction("VR_RigStudio_Remove");
    if(e && fn && st.subject > svs.maxclients)
    {
        pr_global_struct->time = qcvm->time;
        pr_global_struct->self = EDICT_TO_PROG(EDICT_NUM(1));
        G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
        PR_ExecuteProgram(fn);
        Con_Printf("rigstudio: subject %d removed\n", st.subject);
    }
    st.subject = 0;
    st.note = posestudio::Note{};
    st.noteFrame = -1;
}

void makeSubject()
{
    st.spawnAt = -1.0;
    removeSubject(); // (exactly one: a respawn asked twice before its spawn came)
    const glm::vec3 at = st.stage + glm::vec3{0.f, 0.f, 25.f};
    const glm::vec3 angles{0.f, st.stageYaw, 0.f};
    edict_t* e = armedClass(st.choice.spawn)
                     ? box3d::spawnClass(st.choice.spawn, at, angles, nullptr, "weapon", static_cast<float>(st.choice.weapon),
                           false, 0.f, "weapon2", static_cast<float>(st.choice.weapon2))
                     : box3d::spawnClass(st.choice.spawn, at, angles);
    if(!e)
    {
        say(va("%s could not be spawned here", st.choice.spawn));
        return;
    }
    st.subject = NUM_FOR_EDICT(e);
    st.made = st.choice;
    e->v.takedamage = 0.f;
    Con_Printf("rigstudio: %s spawned (entity %d)\n", st.choice.spawn, st.subject);
}

// The subject held still where it stands (its AI frozen: vr_agrunt_freeze; any other monster's thinking stopped).
void holdSubject(edict_t* e)
{
    e->v.velocity[0] = e->v.velocity[1] = e->v.velocity[2] = 0.f;
    e->v.angles[1] = st.stageYaw;
    if(!armedClass(st.made.spawn))
    {
        e->v.nextthink = static_cast<float>(sv.qcvm.time) + 1.f; // (its own think never comes)
        e->v.movetype = MOVETYPE_NONE;
    }
    e->v.takedamage = 0.f;
    tellAim(e);
}

// The model to edit: the subject's (its data from the files, or empty).
void takeModel(const qmodel_t* model)
{
    if(model == st.model)
    {
        return;
    }
    st.model = model;
    st.rig = model ? ragdoll::rigFor(const_cast<qmodel_t*>(model)) : nullptr;
    st.numBones = st.rig ? st.rig->numBones : 0;
    st.doc = rigdata::ModelData{};
    if(st.rig)
    {
        if(const rigdata::ModelData* m = rigdata::of(model))
        {
            st.doc = *m;
        }
        else
        {
            char base[64];
            rigdata::baseName(model->name, base, sizeof(base));
            st.doc.model = model->name;
            st.doc.base = base;
        }
        for(int b = 0; b < st.numBones; b++)
        {
            st.doc.boneName[b] = st.rig->bones[b].name;
        }
    }
    st.dirtyRig = false;
    st.dirtySeq.clear();
    st.dirtySeq.resize(st.doc.seqs.size(), 0);
    st.undo.clear();
    st.redo.clear();
    st.selBones = 0;
    st.active = -1;
    st.hidden = 0;
    st.solo = -1;
    st.selKeys.clear();
    st.seq = st.doc.seqs.empty() ? -1 : 0;
    st.source = Source::Rest;
    st.frame = 0.f;
    st.geomFor = false;
    st.weightsGen = -1;
    st.docGen++;
    clearLive();
    groups.n = 0;
    groups.model = model;
    if(const aliashdr_t* hdr = hdrOf())
    {
        groups.n = rigdata::animGroups(hdr, groups.g, maxGroups);
    }
    if(st.rig)
    {
        Con_Printf("rigstudio: editing %s: %d bones, %d vertices, %d animations, %d sequences\n", model->name, st.numBones,
            st.rig->numVerts, groups.n, static_cast<int>(st.doc.seqs.size()));
    }
}

[[nodiscard]] rigpose::Rigid keyOf(int pose, int b)
{
    return rigpose::key(*st.rig, pose, b);
}

// ---- The commands ----

void open_f()
{
    if(VR_IsActive())
    {
        Con_Printf("The Rig Studio is flat-screen only: it does not open in a headset (start the game with -flatscreenmode).\n");
        return;
    }
    if(Cmd_Argc() > 1 && strcmp(Cmd_Argv(1), "here") != 0)
    {
        q_strlcpy(st.choice.spawn, Cmd_Argv(1), sizeof(st.choice.spawn));
    }
    if(Cmd_Argc() > 2)
    {
        st.choice.weapon = Q_atoi(Cmd_Argv(2));
    }
    if(Cmd_Argc() > 3)
    {
        st.choice.weapon2 = Q_atoi(Cmd_Argv(3));
    }
    if(!imgui::ensure())
    {
        Con_Printf("rigstudio: ImGui could not start (no window?)\n");
        return;
    }
    const bool here = Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "here");
    if(inGame())
    {
        removeSubject(); // (opened again while open: the last subject never left standing beside the new one)
    }
    st.open = true;
    st.staged = false;
    st.subject = 0;
    st.spawnAt = -1.0;
    st.model = nullptr;
    st.aim.placed = false; // (ahead of the subject where it stands)
    keepSettings();
    imgui::setTheme(static_cast<int>(vr_rigstudio_theme.value));
    if(here && inGame())
    {
        st.wantMap = false;
    }
    else
    {
        st.wantMap = true;
        Cbuf_InsertText(va("map %s\n", vr_rigstudio_map.string));
    }
    key_dest = key_game;
    Con_Printf("rigstudio: open (%s, weapons %d %d)\n", st.choice.spawn, st.choice.weapon, st.choice.weapon2);
}

void close_f()
{
    close();
}

void select_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_rigstudio_select <bone> [add]\n");
        return;
    }
    const int b = boneByName(Cmd_Argv(1));
    if(b < 0)
    {
        Con_Printf("rigstudio: no bone %s\n", Cmd_Argv(1));
        return;
    }
    selectBone(b, Cmd_Argc() > 2);
    Con_Printf("rigstudio: selected %s\n", st.rig->bones[b].name);
}

// vr_rigstudio_rotate <x> <y> <z> [world]: the selected bones turned by the rotation vector (degrees; about their own
// axes, or the model's with "world"), as the rotate handle turns them.
void rotate_f()
{
    if(Cmd_Argc() < 4 || !st.rig || !st.selBones)
    {
        Con_Printf("usage: vr_rigstudio_rotate <x> <y> <z> [world] (degrees; a bone selected)\n");
        return;
    }
    pushUndo("rotate");
    const glm::vec3 rv{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    const bool world = Cmd_Argc() > 4 && !strcmp(Cmd_Argv(4), "world");
    for(int b = 0; b < st.numBones; b++)
    {
        const int p = st.rig->bones[b].parent;
        if(!(st.selBones & (1u << b)) || (p >= 0 && (st.selBones & (1u << p))))
        {
            continue;
        }
        evaluate(st.frame, st.F, st.P, true);
        const glm::quat q = world ? rigdata::toQuat(rv) : st.F[b].rot * rigdata::toQuat(rv) * glm::inverse(st.F[b].rot);
        const glm::vec3 c = rigpose::apply(st.F[b], st.pivots[b]);
        rigpose::Rigid w;
        w.rot = q;
        w.pos = c - q * c;
        moveBone(b, w);
    }
    if(st.autoKey && st.source != Source::Rest)
    {
        keySelected();
    }
    publish();
    Con_Printf("rigstudio: rotated by %g %g %g\n", rv.x, rv.y, rv.z);
}

void move_f()
{
    if(Cmd_Argc() < 4 || !st.rig || !st.selBones)
    {
        Con_Printf("usage: vr_rigstudio_move <x> <y> <z> (units, the model's axes; a bone selected)\n");
        return;
    }
    pushUndo("move");
    rigpose::Rigid w;
    w.pos = glm::vec3{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    for(int b = 0; b < st.numBones; b++)
    {
        const int p = st.rig->bones[b].parent;
        if((st.selBones & (1u << b)) && !(p >= 0 && (st.selBones & (1u << p))))
        {
            moveBone(b, w);
        }
    }
    if(st.autoKey && st.source != Source::Rest)
    {
        keySelected();
    }
    publish();
}

void key_f()
{
    pushUndo("key");
    keySelected();
    publish();
}

void frame_f()
{
    if(Cmd_Argc() > 1)
    {
        setFrame(Q_atof(Cmd_Argv(1)));
    }
    Con_Printf("rigstudio: frame %g of %g\n", st.frame, timelineLength());
}

// vr_rigstudio_source rest | anim <group> | seq <name>
void source_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_rigstudio_source rest | anim <name> | seq <name>\n");
        return;
    }
    const char* what = Cmd_Argv(1);
    if(!strcmp(what, "rest"))
    {
        st.source = Source::Rest;
    }
    else if(!strcmp(what, "anim") && Cmd_Argc() > 2)
    {
        for(int i = 0; i < groups.n; i++)
        {
            if(!q_strcasecmp(groups.g[i].name, Cmd_Argv(2)))
            {
                st.source = Source::Anim;
                st.group = i;
            }
        }
    }
    else if(!strcmp(what, "seq") && Cmd_Argc() > 2)
    {
        for(za::SizeT i = 0; i < st.doc.seqs.size(); i++)
        {
            if(!q_strcasecmp(st.doc.seqs[i].name.cStr(), Cmd_Argv(2)))
            {
                st.source = Source::Seq;
                st.seq = static_cast<int>(i);
            }
        }
    }
    clearLive();
    setFrame(0.f);
    Con_Printf("rigstudio: source %s\n", st.source == Source::Rest ? "rest" : st.source == Source::Anim ? groups.g[st.group].name : curSeq() ? curSeq()->name.cStr() : "?");
}

void seqNew_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_rigstudio_seq_new <name> [frames] [fps] [loop]\n");
        return;
    }
    // (a name that is there: that one dropped first, made afresh)
    for(za::SizeT i = 0; i < st.doc.seqs.size(); i++)
    {
        if(!q_strcasecmp(st.doc.seqs[i].name.cStr(), Cmd_Argv(1)))
        {
            pushUndo("replace sequence");
            st.doc.seqs.erase(st.doc.seqs.begin() + static_cast<za::PtrDiffT>(i));
            if(i < st.dirtySeq.size())
            {
                st.dirtySeq.erase(st.dirtySeq.begin() + static_cast<za::PtrDiffT>(i));
            }
            break;
        }
    }
    newSequence(Cmd_Argv(1), Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 10, Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 10.f,
        Cmd_Argc() > 4 && Q_atoi(Cmd_Argv(4)) != 0);
}

void seqEvent_f()
{
    rigdata::Sequence* s = curSeq();
    if(Cmd_Argc() < 3 || !s || st.source != Source::Seq)
    {
        Con_Printf("usage: vr_rigstudio_seq_event <frame> <name> (a sequence being edited)\n");
        return;
    }
    pushUndo("event");
    rigdata::Event e;
    e.frame = Q_atoi(Cmd_Argv(1));
    e.name = Cmd_Argv(2);
    za::SizeT at = 0;
    while(at < s->events.size() && s->events[at].frame <= e.frame)
    {
        at++;
    }
    s->events.insert(s->events.begin() + static_cast<za::PtrDiffT>(at), e);
    touchedSeq(st.seq);
}

void seqSet_f()
{
    // vr_rigstudio_seq_set <field> <value>: fps, length, loop, blendin, blendout, arms, hold, base
    rigdata::Sequence* s = curSeq();
    if(Cmd_Argc() < 3 || !s)
    {
        Con_Printf("usage: vr_rigstudio_seq_set fps|length|loop|blendin|blendout|arms|hold|base <value>\n");
        return;
    }
    pushUndo("sequence");
    const char* f = Cmd_Argv(1);
    const char* v = Cmd_Argv(2);
    if(!strcmp(f, "fps")) { s->fps = za::clamp(static_cast<float>(Q_atof(v)), 0.1f, 240.f); }
    else if(!strcmp(f, "length")) { s->length = za::max(Q_atoi(v), 1); }
    else if(!strcmp(f, "loop")) { s->loop = Q_atoi(v) != 0; }
    else if(!strcmp(f, "blendin")) { s->blendIn = za::max(static_cast<float>(Q_atof(v)), 0.f); }
    else if(!strcmp(f, "blendout")) { s->blendOut = za::max(static_cast<float>(Q_atof(v)), 0.f); }
    else if(!strcmp(f, "arms")) { s->arms = Q_atoi(v) != 0; }
    else if(!strcmp(f, "hold")) { s->hold = Q_atoi(v) != 0; }
    else if(!strcmp(f, "base")) { s->base = v; }
    touchedSeq(st.seq);
}

void save_f()
{
    save();
}

void revert_f()
{
    revert();
}

void undo_f()
{
    undo();
}

void redo_f()
{
    redo();
}

// vr_rigstudio_weight <vertex> <bone> <weight>: the vertex's weight on the bone set (the others scaled to make 1).
void weight_f()
{
    if(Cmd_Argc() < 4 || !st.rig)
    {
        Con_Printf("usage: vr_rigstudio_weight <vertex> <bone> <weight>\n");
        return;
    }
    const int v = Q_atoi(Cmd_Argv(1));
    const int b = boneByName(Cmd_Argv(2));
    if(v < 0 || v >= st.rig->numVerts || b < 0)
    {
        Con_Printf("rigstudio: no such vertex or bone\n");
        return;
    }
    pushUndo("weight");
    setVertWeight(v, b, static_cast<float>(Q_atof(Cmd_Argv(3))));
    touchedRig();
}

// vr_rigstudio_vertex <vertex>: its weights and its place as posed now (model units).
void vertex_f()
{
    if(Cmd_Argc() < 2 || !st.rig)
    {
        Con_Printf("usage: vr_rigstudio_vertex <vertex>\n");
        return;
    }
    int v = Q_atoi(Cmd_Argv(1));
    if(!strcmp(Cmd_Argv(1), "first") && Cmd_Argc() > 2)
    {
        // (the first vertex the bone owns)
        const int b = boneByName(Cmd_Argv(2));
        v = -1;
        for(int i = 0; b >= 0 && i < st.rig->numVerts && v < 0; i++)
        {
            v = st.rig->vertBone[static_cast<za::SizeT>(i)] == b ? i : -1;
        }
    }
    ensureWeights();
    evaluate(st.frame, st.F, st.P, true);
    poseGeometry();
    if(v < 0 || v >= static_cast<int>(st.posedVerts.size()))
    {
        Con_Printf("rigstudio: no vertex %d\n", v);
        return;
    }
    const glm::vec3& p = st.posedVerts[static_cast<za::SizeT>(v)];
    const glm::vec3& r = st.restVerts[static_cast<za::SizeT>(v)];
    za::String w;
    const rigdata::VertWeights& vw = st.weights[static_cast<za::SizeT>(v)];
    for(int i = 0; i < vw.n; i++)
    {
        w += va(" %s %.3f", st.rig->bones[vw.bone[i]].name, vw.w[i]);
    }
    Con_Printf("rigstudio: vertex %d rest %.3f %.3f %.3f posed %.3f %.3f %.3f weights%s\n", v, r.x, r.y, r.z, p.x, p.y, p.z, w.cStr());
}

// vr_rigstudio_rest <bone> pivot|end <x> <y> <z> | cone|twist|lower|upper <deg> | reset
void rest_f()
{
    if(Cmd_Argc() < 3 || !st.rig)
    {
        Con_Printf("usage: vr_rigstudio_rest <bone> pivot|end <x> <y> <z> | cone|twist|lower|upper <degrees> | reset\n");
        return;
    }
    const int b = boneByName(Cmd_Argv(1));
    if(b < 0)
    {
        return;
    }
    pushUndo("rest");
    rigdata::BoneEdit& e = st.doc.bones[b];
    const char* f = Cmd_Argv(2);
    if(!strcmp(f, "reset"))
    {
        e = rigdata::BoneEdit{};
        touchedRig();
        return;
    }
    if(!e.on)
    {
        const ragdoll::Bone& rb = st.rig->bones[b];
        e.on = true;
        e.pivot = rb.pivot;
        e.end = rb.end;
        e.cone = rb.cone / deg;
        e.twist = rb.twist / deg;
        e.lower = rb.lower / deg;
        e.upper = rb.upper / deg;
        e.hinge = rb.hinge;
    }
    const glm::vec3 v{Q_atof(Cmd_Argv(3)), Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : 0.f, Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 0.f};
    if(!strcmp(f, "pivot")) { e.pivot = v; }
    else if(!strcmp(f, "end")) { e.end = v; }
    else if(!strcmp(f, "cone")) { e.cone = v.x; }
    else if(!strcmp(f, "twist")) { e.twist = v.x; }
    else if(!strcmp(f, "lower")) { e.lower = v.x; }
    else if(!strcmp(f, "upper")) { e.upper = v.x; }
    touchedRig();
}

// vr_rigstudio_panel <name> <0|1>: a window shown or hidden (outliner, properties, timeline, tools, posedata, help,
// prefs, sequence), for the screenshots; vr_rigstudio_view <mesh game|shaded|wire|xray> | <colors none|bone|heat> |
// <bones octa|stick|off> | <tool select|rotate|move|ik|rest|weights|paint> | <snap 1|3|7 [flip]> | frame
void panel_f()
{
    if(Cmd_Argc() < 3)
    {
        return;
    }
    const char* n = Cmd_Argv(1);
    const bool on = Q_atoi(Cmd_Argv(2)) != 0;
    bool* flags[] = {&st.showOutliner, &st.showProps, &st.showTimeline, &st.showTools, &st.showPoseData, &st.showHelp,
        &st.showPrefs, &st.showSequence};
    const char* names[] = {"outliner", "properties", "timeline", "tools", "posedata", "help", "prefs", "sequence"};
    for(int i = 0; i < 8; i++)
    {
        if(!strcmp(n, names[i]))
        {
            *flags[i] = on;
        }
    }
}

void view_f()
{
    if(Cmd_Argc() < 2)
    {
        return;
    }
    const char* w = Cmd_Argv(1);
    const char* v = Cmd_Argc() > 2 ? Cmd_Argv(2) : "";
    if(!strcmp(w, "mesh"))
    {
        st.mesh = !strcmp(v, "shaded") ? MeshMode::Shaded : !strcmp(v, "wire") ? MeshMode::Wire : !strcmp(v, "xray") ? MeshMode::Xray : MeshMode::Game;
    }
    else if(!strcmp(w, "colors"))
    {
        st.colors = !strcmp(v, "bone") ? Colors::ByBone : !strcmp(v, "heat") ? Colors::Heat : Colors::None;
    }
    else if(!strcmp(w, "bones"))
    {
        st.bones = !strcmp(v, "stick") ? BoneLook::Stick : !strcmp(v, "off") ? BoneLook::Off : BoneLook::Octahedral;
    }
    else if(!strcmp(w, "tool"))
    {
        const char* tools[toolCount] = {"select", "rotate", "move", "ik", "rest", "weights", "paint", "posekeys"};
        for(int i = 0; i < toolCount; i++)
        {
            if(!strcmp(v, tools[i]))
            {
                st.tool = static_cast<Tool>(i);
            }
        }
    }
    else if(!strcmp(w, "snap"))
    {
        snapView(Q_atoi(v), Cmd_Argc() > 3);
    }
    else if(!strcmp(w, "frame"))
    {
        frameSelection();
    }
    else if(!strcmp(w, "camera"))
    {
        // vr_rigstudio_view camera <distance> [yaw from its front] [pitch]: about the subject's middle
        st.cam.dist = za::clamp(static_cast<float>(Q_atof(v)), 8.f, 4000.f);
        st.cam.focus = st.stage + glm::vec3{0.f, 0.f, 24.f};
        if(Cmd_Argc() > 3)
        {
            st.cam.yaw = st.stageYaw + 180.f + static_cast<float>(Q_atof(Cmd_Argv(3)));
        }
        if(Cmd_Argc() > 4)
        {
            st.cam.pitch = static_cast<float>(Q_atof(Cmd_Argv(4)));
        }
    }
    else if(!strcmp(w, "onion"))
    {
        st.onion = Q_atoi(v) != 0;
    }
    else if(!strcmp(w, "proxies"))
    {
        st.proxies = Q_atoi(v) != 0;
    }
}

void selverts_f()
{
    // vr_rigstudio_selverts bone <name> | grow | shrink | none | <v>...
    if(Cmd_Argc() < 2 || !st.rig)
    {
        return;
    }
    const char* w = Cmd_Argv(1);
    ensureWeights();
    if(!strcmp(w, "bone") && Cmd_Argc() > 2)
    {
        selectByBone(boneByName(Cmd_Argv(2)), false);
    }
    else if(!strcmp(w, "grow") || !strcmp(w, "shrink"))
    {
        growSelection(!strcmp(w, "grow"));
    }
    else if(!strcmp(w, "none"))
    {
        za::fill(st.selVerts, static_cast<uint8_t>(0));
    }
    else
    {
        for(int i = 1; i < Cmd_Argc(); i++)
        {
            const int v = Q_atoi(Cmd_Argv(i));
            if(v >= 0 && v < static_cast<int>(st.selVerts.size()))
            {
                st.selVerts[static_cast<za::SizeT>(v)] = 1;
            }
        }
    }
    int n = 0;
    for(const uint8_t s : st.selVerts)
    {
        n += s;
    }
    Con_Printf("rigstudio: %d vertices selected\n", n);
}

void assign_f()
{
    if(Cmd_Argc() < 2)
    {
        return;
    }
    pushUndo("assign");
    assignSelected(boneByName(Cmd_Argv(1)));
}

void pose_f()
{
    // vr_rigstudio_pose save <name> | load <name> [selected] | mirror | copy | paste
    if(Cmd_Argc() < 2 || !st.rig)
    {
        return;
    }
    const char* w = Cmd_Argv(1);
    if(!strcmp(w, "save") && Cmd_Argc() > 2)
    {
        pushUndo("save pose");
        rigdata::NamedPose np;
        np.name = Cmd_Argv(2);
        np.pose = currentPose();
        st.doc.poses.pushBack(np);
        touchedRig();
    }
    else if(!strcmp(w, "load") && Cmd_Argc() > 2)
    {
        for(const rigdata::NamedPose& np : st.doc.poses)
        {
            if(!q_strcasecmp(np.name.cStr(), Cmd_Argv(2)))
            {
                pushUndo("load pose");
                applyPose(np.pose, Cmd_Argc() > 3 ? st.selBones : 0xffffffffu);
                publish();
            }
        }
    }
    else if(!strcmp(w, "mirror"))
    {
        pushUndo("mirror pose");
        mirrorPose();
        publish();
    }
}

void status_f()
{
    Con_Printf("rigstudio: %s, subject %d (%s), model %s, source %s, frame %.2f/%g, %d bones selected, undo %d redo %d, %s\n",
        st.open ? "open" : "closed", st.subject, st.made.spawn, st.model ? st.model->name : "none",
        st.source == Source::Rest ? "rest" : st.source == Source::Anim ? "animation" : "sequence", st.frame, timelineLength(),
        __builtin_popcount(st.selBones), static_cast<int>(st.undo.size()), static_cast<int>(st.redo.size()),
        st.dirtyRig || za::anyOf(st.dirtySeq.begin(), st.dirtySeq.end(), [](uint8_t d) { return d != 0; }) ? "unsaved changes" : "saved");
}

// vr_rigstudio_count: the server's entities in use, and of them the monsters, the held weapons, the weapons' records and
// the worn tanks (the subject's changes leave exactly one subject and its own weapons: the tests).
// How far off the aim target the subject's right weapon points (degrees): as drawn (the arms' solve, relinkSkinned's
// note: its frame's forward against the way from it to the target) and as the server placed it (its entity's angles).
struct AimReading
{
    bool drawnOk{false}, serverOk{false};
    float drawn{0.f}, server{0.f};
    float gunPitch{0.f}, wantPitch{0.f}; // the drawn gun's pitch and the target's from it (degrees, up)
    glm::vec3 gun{0.f};
    bool barrelOk{false};
    float barrel{0.f}, barrelPitch{0.f}; // its barrel's line (butt to muzzle) against the way from the muzzle to the target
};

[[nodiscard]] AimReading readAim()
{
    AimReading r;
    if(st.noteFrame >= 0 && host_framecount - st.noteFrame <= 5 && st.note.ent == st.subject && st.note.placed.on[0])
    {
        const posestudio::Note& n = st.note;
        const glm::quat yq = glm::angleAxis(n.space.yawDeg * deg, glm::vec3{0.f, 0.f, 1.f});
        r.gun = n.space.origin + yq * (n.placed.weapon[0].pos * n.space.scale);
        const glm::vec3 fwd = glm::normalize(yq * (n.placed.weapon[0].rot * glm::vec3{1.f, 0.f, 0.f}));
        const glm::vec3 want = glm::normalize(st.aim.at - r.gun);
        r.drawn = za::acos(za::clamp(glm::dot(fwd, want), -1.f, 1.f)) / deg;
        r.gunPitch = za::asin(za::clamp(fwd.z, -1.f, 1.f)) / deg;
        r.wantPitch = za::asin(za::clamp(want.z, -1.f, 1.f)) / deg;
        r.drawnOk = true;
        // Its barrel: the model's butt-to-muzzle line, against the way from its muzzle to the target
        glm::vec3 muzzle, butt, second;
        int secondType = 0;
        if(n.prop > 0 && n.prop < cl.num_entities && cl_entities[n.prop].model &&
            view::weaponLocalGeometry(cl_entities[n.prop].model, muzzle, butt, second, secondType) && glm::length(muzzle - butt) > 1.f)
        {
            const auto w = [&](const glm::vec3& l) { return r.gun + yq * (n.placed.weapon[0].rot * (l * n.space.scale)); };
            const glm::vec3 m = w(muzzle);
            const glm::vec3 line = glm::normalize(m - w(butt));
            const glm::vec3 toT = glm::normalize(st.aim.at - m);
            r.barrel = za::acos(za::clamp(glm::dot(line, toT), -1.f, 1.f)) / deg;
            r.barrelPitch = za::asin(za::clamp(line.z, -1.f, 1.f)) / deg;
            r.barrelOk = true;
        }
    }
    if(sv.active && st.note.prop > 0 && st.note.prop < sv.qcvm.num_edicts)
    {
        const SvVm vm;
        const edict_t* p = EDICT_NUM(st.note.prop);
        if(!p->free)
        {
            const float pp = p->v.angles[0] * deg, py = p->v.angles[1] * deg; // (a model's pitch: up)
            const glm::vec3 fwd{za::cos(pp) * za::cos(py), za::cos(pp) * za::sin(py), za::sin(pp)};
            const glm::vec3 want = glm::normalize(st.aim.at - glm::vec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]});
            r.server = za::acos(za::clamp(glm::dot(fwd, want), -1.f, 1.f)) / deg;
            r.serverOk = true;
        }
    }
    return r;
}

// vr_rigstudio_aim [on|show|camera 0|1 | reset | at <x y z> | rel <forward left up> | select]: the aim target (as its
// Tools section does; rel: from the subject's origin in its facing); no arguments: where it is, and how far off it
// the subject's weapon points (drawn, and as the server placed it).
void aim_f()
{
    const char* w = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    const float v = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 1.f;
    if(!strcmp(w, "on")) { st.aim.on = v != 0.f; }
    else if(!strcmp(w, "show")) { st.aim.show = v != 0.f; }
    else if(!strcmp(w, "camera")) { pushAimUndo(); st.aim.atCamera = v != 0.f; }
    else if(!strcmp(w, "select")) { st.aim.selected = v != 0.f; }
    else if(!strcmp(w, "reset")) { resetAim(); }
    else if((!strcmp(w, "at") || !strcmp(w, "rel")) && Cmd_Argc() > 4)
    {
        const glm::vec3 a{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
        pushAimUndo();
        st.aim.atCamera = false;
        st.aim.placed = true;
        if(!strcmp(w, "at"))
        {
            st.aim.at = a;
        }
        else
        {
            const float y = st.stageYaw * deg;
            const glm::vec3 org = st.stage + glm::vec3{0.f, 0.f, 25.f};
            st.aim.at = org + glm::vec3{za::cos(y), za::sin(y), 0.f} * a.x + glm::vec3{-za::sin(y), za::cos(y), 0.f} * a.y +
                        glm::vec3{0.f, 0.f, a.z};
        }
    }
    const AimReading r = readAim();
    Con_Printf("rigstudio aim: %s, at %.1f %.1f %.1f%s; its weapon off it: drawn %s%.2f deg (gun pitch %.1f, to the target "
               "%.1f), server %s%.2f deg\n",
        st.aim.on ? "on" : "off", st.aim.at.x, st.aim.at.y, st.aim.at.z, st.aim.atCamera ? " (the camera)" : "",
        r.drawnOk ? "" : "(none) ", r.drawn, r.gunPitch, r.wantPitch, r.serverOk ? "" : "(none) ", r.server);
}

struct Census
{
    int used{0}, monsters{0}, weapons{0}, records{0}, tanks{0};
    int owned{0};   // the subject's: what it owns, and what those own
    int mineW{0};   // ... of them its held weapons
    int mineT{0};   // ... its worn tanks
    int mineR{0};   // the records its weapons hold
};

[[nodiscard]] Census census()
{
    Census c;
    const SvVm vm;
    const int instOfs = ED_FindFieldOffset("weaponinst");
    const edict_t* subj = subjectEdict();
    const int sp = subj ? EDICT_TO_PROG(subj) : -1;
    for(int i = 1; i < sv.qcvm.num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free)
        {
            continue;
        }
        c.used++;
        const char* cn = PR_GetString(e->v.classname);
        const bool weapon = !strcmp(cn, "vr_ag_weapon"), tank = !strcmp(cn, "vr_fueltank_worn");
        c.monsters += (static_cast<int>(e->v.flags) & FL_MONSTER) != 0;
        c.weapons += weapon;
        c.records += !strcmp(cn, "weapon_inst");
        c.tanks += tank;
        if(sp < 0 || e == subj)
        {
            continue;
        }
        const edict_t* o = PROG_TO_EDICT(e->v.owner);
        const bool mine = e->v.owner == sp || (e->v.owner != 0 && o->v.owner == sp && o != subj);
        c.owned += mine;
        c.mineW += mine && weapon;
        c.mineT += mine && tank;
        c.mineR += mine && weapon && instOfs >= 0 && progs::fieldInt(e, instOfs) != 0;
    }
    return c;
}

void count_f()
{
    if(!sv.active)
    {
        Con_Printf("rigstudio count: no server\n");
        return;
    }
    const Census c = census();
    Con_Printf("rigstudio count: edicts %d, monsters %d, weapons %d, records %d, tanks %d, subject %d (%s), owned by it %d "
               "(weapons %d, tanks %d, records %d)\n",
        c.used, c.monsters, c.weapons, c.records, c.tanks, st.subject, st.made.spawn, c.owned, c.mineW, c.mineT, c.mineR);
}

// vr_rigstudio_test_subjects: the subject changed through every kind of change (its weapon, its left weapon, its kind,
// a rigged monster's), the map loaded again, the studio closed and opened again here, twice: after each, exactly one
// subject and its own weapons (its records, its tank), and the entities besides it the same as after the first. Each
// step waits for its subject to stand (a few frames for its weapons and tank to be made); the verdict line ends it.
struct SubjectStep
{
    const char* spawn;  // nullptr: an action
    int weapon, weapon2;
    int expectW;        // its held weapons
    int action;         // 1 the map again, 2 close (no subject after), 3 open here
};
constexpr SubjectStep subjectSteps[] = {
    {"monster_vr_grunt", 4, 0, 1, 0},     // the shotgun
    {"monster_vr_grunt", 2, 0, 1, 0},     // the axe
    {"monster_vr_grunt", 4, 4, 2, 0},     // two shotguns
    {"monster_vr_grunt", 26, 0, 1, 0},    // the flamethrower (its tank on its back)
    {"monster_vr_enforcer", 16, 0, 1, 0}, // the enforcer's laser rifle
    {"monster_vr_enforcer", 4, 29, 2, 0}, // a shotgun and a nail pistol
    {"monster_army", 0, 0, 0, 0},         // a rigged monster, unarmed
    {"monster_vr_grunt", -1, 0, 0, 0},    // fists
    {"monster_ogre", 0, 0, 0, 0},
    {"monster_vr_grunt", 4, 0, 1, 0},
    {nullptr, 0, 0, 1, 1},                // the map loaded again (the subject staged again: the shotgun)
    {nullptr, 0, 0, 0, 2},                // closed
    {nullptr, 0, 0, 1, 3},                // opened again, here
    {nullptr, 0, 0, 1, 3},                // ... and again
};
constexpr int subjectStepCount = static_cast<int>(sizeof(subjectSteps) / sizeof(subjectSteps[0]));

struct SubjectTest
{
    int step{-1};       // -1 not running
    int waitFrames{0};  // frames since its subject stood
    int passed{0}, failed{0};
    int baseOther{-1};  // the entities besides the subject and its own (the first step's)
    int baseMonsters{-1};
    bool started{false};
};
SubjectTest stest;

void subjectStepStart()
{
    const SubjectStep& s = subjectSteps[stest.step];
    stest.waitFrames = 0;
    if(s.action == 1)
    {
        Cbuf_AddText(va("map %s\n", sv.name));
    }
    else if(s.action == 2)
    {
        close();
    }
    else if(s.action == 3)
    {
        Cbuf_AddText("vr_rigstudio here\n");
    }
    else
    {
        q_strlcpy(st.choice.spawn, s.spawn, sizeof(st.choice.spawn));
        st.choice.weapon = s.weapon;
        st.choice.weapon2 = s.weapon2;
        respawn();
    }
}

void subjectTestTick()
{
    if(stest.step < 0 || !inGame())
    {
        return;
    }
    if(!stest.started)
    {
        if(st.open && st.staged && subjectEdict())
        {
            stest.started = true;
            subjectStepStart();
        }
        return;
    }
    const SubjectStep& s = subjectSteps[stest.step];
    const bool closedStep = s.action == 2;
    const bool ready = closedStep ? !st.open : (st.open && st.staged && st.spawnAt < 0.0 && subjectEdict() != nullptr);
    if(!ready || ++stest.waitFrames < 8) // (8 frames: its weapons' first thinks, its tank made on its back)
    {
        return;
    }
    const Census c = census();
    const bool subj = subjectEdict() != nullptr;
    const int other = c.used - (subj ? 1 + c.owned + c.mineR : 0);
    const int otherMonsters = c.monsters - (subj ? 1 : 0);
    if(stest.baseOther < 0 || s.action == 1) // (a map loaded again: its own count, the next steps' base)
    {
        stest.baseOther = other;
        stest.baseMonsters = otherMonsters;
    }
    const bool tankOk = c.mineT == ((s.weapon == 26 || s.weapon == 27) && !s.action ? 1 : 0) || s.action;
    const bool ok = (closedStep ? !subj : subj) && c.mineW == (closedStep ? 0 : s.expectW) && c.mineR == c.mineW &&
                    c.weapons == c.mineW && c.tanks == c.mineT && tankOk && other == stest.baseOther &&
                    otherMonsters == stest.baseMonsters;
    (ok ? stest.passed : stest.failed)++;
    Con_Printf("rigstudio subjects %d: %s %s: edicts %d (besides it %d, first %d), monsters %d, its weapons %d of %d, "
               "records %d, tanks %d of %d: %s\n",
        stest.step + 1, s.action == 1 ? "map again" : s.action == 2 ? "closed" : s.action == 3 ? "opened here" : s.spawn,
        s.action ? "" : va("%d %d", s.weapon, s.weapon2), c.used, other, stest.baseOther, c.monsters, c.mineW, c.weapons,
        c.mineR, c.mineT, c.tanks, ok ? "PASS" : "FAIL");
    stest.step++;
    if(stest.step >= subjectStepCount)
    {
        Con_Printf("rigstudio subjects: %d passed, %d failed\n", stest.passed, stest.failed);
        stest.step = -1;
        return;
    }
    subjectStepStart();
}

// vr_rigstudio_posekey [kind <k> | class <c> | pose <p> | phase <n> | follow 0|1 | part <n>]...: the Pose Studio Data
// window's key (as its combos do; the Pose Keys tool's part); then its values printed.
void posekey_f()
{
    for(int i = 1; i + 1 < Cmd_Argc(); i += 2)
    {
        const char* w = Cmd_Argv(i);
        const int v = Q_atoi(Cmd_Argv(i + 1));
        if(!strcmp(w, "kind")) { st.pdKind = za::clamp(v, 0, agpose::numKinds - 1); }
        else if(!strcmp(w, "class")) { st.pdClass = za::clamp(v, 0, agpose::NumClasses - 1); }
        else if(!strcmp(w, "pose")) { st.pdPose = za::clamp(v, 0, agpose::NumPoses - 1); }
        else if(!strcmp(w, "phase")) { st.pdPhase = za::clamp(v, 0, agpose::numPhases - 1); }
        else if(!strcmp(w, "follow")) { st.pdFollow = v != 0; }
        else if(!strcmp(w, "part")) { st.pdPart = v; }
    }
    const int code = agpose::code(st.pdKind, st.pdClass, st.pdPose, agpose::isStrike(st.pdPose) ? st.pdPhase : 0);
    const agpose::Key k = agpose::value(code);
    Con_Printf("rigstudio posekey %d (%s, %s, %s): R %.2f %.2f %.2f turn %.2f %.2f %.2f, L %.2f %.2f %.2f turn %.2f %.2f %.2f, "
               "elbows %.3f %.3f %.3f / %.3f %.3f %.3f, torso %.2f %.2f %.2f; subject %d %d\n",
        code, st.pdKind ? "enforcer" : "grunt", agpose::className(st.pdClass), agpose::poseName(st.pdClass, st.pdPose),
        k.v[0].x, k.v[0].y, k.v[0].z, k.v[1].x, k.v[1].y, k.v[1].z, k.v[2].x, k.v[2].y, k.v[2].z, k.v[3].x, k.v[3].y, k.v[3].z,
        k.v[4].x, k.v[4].y, k.v[4].z, k.v[5].x, k.v[5].y, k.v[5].z, k.v[6].x, k.v[6].y, k.v[6].z, st.subjKind, st.subjClass);
    aglegs::Placed L;
    const bool legs = st.subject > 0 && aglegs::placed(st.subject, L);
    Con_Printf("rigstudio posekey: the subject's legs %s (kneel %.2f, weight %.2f), legs layer %s\n", legs ? "placed" : "not placed",
        legs ? L.on : 0.f, legs ? L.w : 0.f, aglegs::enabled() ? "on" : "off");
}

// ---- Settling: the subject's drawn parts still for a few frames (the game's arms eased onto their targets) ----

struct Settle
{
    rigpose::Rigid last[4];
    bool have{false};
    int still{0};
    int frames{0};
};

constexpr int settleParts[4] = {0, 4, 6, 7}; // the right weapon, the right elbow, the torso, the pelvis

void settleReset(Settle& s)
{
    s = Settle{};
}

[[nodiscard]] bool noteNow()
{
    return st.subject > 0 && st.noteFrame >= 0 && host_framecount - st.noteFrame <= 5 && st.note.ent == st.subject;
}

// True once every shown part moved under 0.01 units and 0.05 degrees a frame for 8 frames (6 at least after the reset).
[[nodiscard]] bool settled(Settle& s)
{
    s.frames++;
    if(!noteNow())
    {
        s.have = false;
        s.still = 0;
        return false;
    }
    bool still = s.have;
    for(int i = 0; i < 4; i++)
    {
        rigpose::Rigid f;
        if(!posePart(settleParts[i], f))
        {
            f = rigpose::Rigid{};
        }
        if(s.have)
        {
            const float d = glm::length(f.pos - s.last[i].pos);
            const float a = 2.f * za::acos(za::min(1.f, za::abs(glm::dot(glm::normalize(f.rot), glm::normalize(s.last[i].rot))))) / deg;
            still = still && d < 0.01f && a < 0.05f;
        }
        s.last[i] = f;
    }
    s.have = true;
    s.still = still ? s.still + 1 : 0;
    return s.frames >= 6 && s.still >= 8;
}

// vr_rigstudio_test_aim: the subject (the shotgun grunt) in its shoot animation, aiming off, then at a target level
// ahead (the default), high to its left and low to its right; each settled, its weapon's line against the way to the
// target measured (its barrel's line from butt to muzzle against the way from its muzzle to the target: within 8
// degrees; its entity's forward, drawn and as the server placed it, printed), a screenshot taken. The verdict ends it.
struct AimCase
{
    const char* name;
    bool on;
    glm::vec3 rel; // forward, left, up from the subject's origin (all 0: Reset)
};
constexpr AimCase aimCases[] = {
    {"aim off (carried, as before)", false, {0.f, 0.f, 0.f}},
    {"level ahead (the default)", true, {0.f, 0.f, 0.f}},
    {"high left", true, {200.f, 70.f, 110.f}},
    {"low right", true, {200.f, -70.f, -45.f}},
};
constexpr int aimCaseCount = static_cast<int>(sizeof(aimCases) / sizeof(aimCases[0]));

struct AimTest
{
    int step{-1};
    bool ready{false}; // the subject standing, its animation picked, the camera placed
    char anim[32]{"shoot"};
    float frame{1.f};
    int phase{0};
    int passed{0}, failed{0};
    Settle settle;
    int shotWait{0};
};
AimTest atest;

void aimCaseStart()
{
    const AimCase& c = aimCases[atest.step];
    st.aim.on = c.on;
    if(c.rel == glm::vec3{0.f})
    {
        resetAim();
    }
    else
    {
        Cbuf_AddText(va("vr_rigstudio_aim rel %g %g %g\n", c.rel.x, c.rel.y, c.rel.z));
    }
    settleReset(atest.settle);
    atest.phase = 0;
}

void aimTestTick()
{
    if(atest.step < 0 || !st.open)
    {
        return;
    }
    if(!atest.ready)
    {
        if(!st.model || !noteNow())
        {
            return; // (the room and its subject still coming)
        }
        for(int i = 0; i < groups.n; i++)
        {
            if(!q_strcasecmp(groups.g[i].name, atest.anim))
            {
                st.source = Source::Anim;
                st.group = i;
            }
        }
        st.playing = false;
        setFrame(atest.frame);
        // (the camera at its left side, a little in front, looking across at it and the way it aims)
        const float y = st.stageYaw * deg;
        st.cam.focus = st.stage + glm::vec3{za::cos(y), za::sin(y), 0.f} * 80.f + glm::vec3{0.f, 0.f, 40.f};
        st.cam.yaw = st.stageYaw + 118.f;
        st.cam.pitch = 0.f;
        st.cam.dist = 210.f;
        st.showTools = st.showOutliner = st.showProps = st.showTimeline = st.showSequence = st.showPoseData = false;
        atest.ready = true;
        aimCaseStart();
        return;
    }
    if(atest.phase == 0)
    {
        if(!settled(atest.settle))
        {
            if(atest.settle.frames > 600)
            {
                Con_Printf("rigstudio aim %d: FAIL timed out (never settled)\n", atest.step + 1);
                atest.failed++;
                atest.phase = 2;
            }
            return;
        }
        const AimCase& c = aimCases[atest.step];
        const AimReading r = readAim();
        // (within 8 degrees: the game's own aim, the gun Aim key's turn and the arms' solve included; off: the carry)
        const bool ok = !c.on || (r.barrelOk && r.serverOk && r.barrel < 8.f);
        (ok ? atest.passed : atest.failed)++;
        Con_Printf("rigstudio aim %d: %s: target %.1f %.1f %.1f; its barrel off it %.2f deg (barrel pitch %.1f, the "
                   "target's %.1f); its entity's forward off it: drawn %.2f deg, server %.2f deg: %s\n",
            atest.step + 1, c.name, st.aim.at.x, st.aim.at.y, st.aim.at.z, r.barrel, r.barrelPitch, r.wantPitch, r.drawn,
            r.server, ok ? "PASS" : "FAIL");
        Cbuf_AddText("screenshot\n");
        atest.phase = 1;
        atest.shotWait = 0;
        return;
    }
    if(atest.phase == 1 && ++atest.shotWait < 2)
    {
        return; // (the screenshot taken first, of this case)
    }
    atest.step++;
    if(atest.step >= aimCaseCount)
    {
        Con_Printf("rigstudio aim: %d passed, %d failed\n", atest.passed, atest.failed);
        atest.step = -1;
        return;
    }
    aimCaseStart();
}

// vr_rigstudio_test_aim [animation] [frame] (default shoot 1): run once the studio is open (it waits for its subject).
void testAim_f()
{
    if(!st.open)
    {
        Con_Printf("rigstudio aim: the studio is not open\n");
        return;
    }
    atest = AimTest{};
    atest.step = 0;
    if(Cmd_Argc() > 1)
    {
        q_strlcpy(atest.anim, Cmd_Argv(1), sizeof(atest.anim));
    }
    atest.frame = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 1.f;
}

// vr_rigstudio_test_posekeys: the Pose Keys tool on the subject (the shotgun grunt, Aim): each part dragged by the mock
// mouse on its handle by a known amount (the right weapon along its key's x and round its z, the right elbow down, the
// torso round its z; in Pickup the pelvis down): the key's values changed by that (its space), the drawn part moved or
// turned by it, then undone: the key exactly as it was, the part back. Screenshots before and after the weapon's move.
struct PkStep
{
    const char* name;
    int part;     // posestudio's numbers
    int axis;     // 0..2 an arrow, 20..22 a ring
    float amount; // units or degrees
    int pose;     // agpose::Pose
};
constexpr PkStep pkSteps[] = {
    {"right weapon, forward 4", 0, 0, 4.f, agpose::Aim},
    {"right weapon, turned 20 deg left (z)", 0, 22, 20.f, agpose::Aim},
    {"right elbow, down 4", 4, 2, -4.f, agpose::Aim},
    {"torso, turned 15 deg left (z)", 6, 22, 15.f, agpose::Aim},
    {"pelvis, down 3 (Pickup)", 7, 2, -3.f, agpose::Pickup},
};
constexpr int pkStepCount = static_cast<int>(sizeof(pkSteps) / sizeof(pkSteps[0]));

struct PkTest
{
    int step{-1};
    int phase{0};
    int passed{0}, failed{0};
    Settle settle;
    int code{-1};
    agpose::Key key0;
    rigpose::Rigid part0;
    rigpose::Rigid placed0, target0; // the right weapon as drawn and its target (model space) before
    int wait{0};
};
PkTest ptest;

[[nodiscard]] float quatDeg(const glm::quat& a, const glm::quat& b)
{
    return 2.f * za::acos(za::min(1.f, za::abs(glm::dot(glm::normalize(a), glm::normalize(b))))) / deg;
}

[[nodiscard]] glm::quat rotOf(const glm::vec3& r)
{
    const float a = glm::length(r);
    return a < 1e-5f ? glm::quat{1.f, 0.f, 0.f, 0.f} : glm::angleAxis(a * deg, r / a);
}

void pkFail(const char* why)
{
    Con_Printf("rigstudio posekeys %d: %s: FAIL %s\n", ptest.step + 1, pkSteps[ptest.step].name, why);
    ptest.failed++;
    ptest.phase = 9;
}

void pkTestTick()
{
    if(ptest.step < 0 || !st.open)
    {
        return;
    }
    if(ptest.phase == -1)
    {
        if(!st.model || !noteNow())
        {
            return; // (the room and its subject still coming)
        }
        // (the camera before it and to its left, near: the parts' handles clear of the windows)
        const float y = st.stageYaw * deg;
        st.cam.focus = st.stage + glm::vec3{za::cos(y), za::sin(y), 0.f} * 4.f + glm::vec3{0.f, 0.f, 30.f};
        st.cam.yaw = st.stageYaw + 180.f - 35.f;
        st.cam.pitch = 12.f;
        st.cam.dist = 85.f;
        st.showTools = st.showOutliner = st.showProps = st.showTimeline = st.showSequence = st.showPoseData = false;
        ptest.phase = 0;
        return;
    }
    const PkStep& s = pkSteps[ptest.step];
    switch(ptest.phase)
    {
    case 0: // held in its pose, settled: the key and the part as they are
    {
        rigpose::Rigid shownNow;
        if(!posePart(s.part, shownNow))
        {
            // (its part not there yet: the pose's change still on its way, the Pickup's kneel coming)
            ptest.settle.still = 0;
            if(++ptest.settle.frames > 600)
            {
                pkFail("its part never shown");
            }
            return;
        }
        if(!settled(ptest.settle))
        {
            if(ptest.settle.frames > 600)
            {
                pkFail("timed out (never settled)");
            }
            return;
        }
        ptest.code = s.part == 7 ? agpose::legCode(st.pdKind, agpose::Pickup)
                                 : agpose::code(st.pdKind, st.pdClass, st.pdPose, agpose::isStrike(st.pdPose) ? st.pdPhase : 0);
        ptest.key0 = agpose::value(ptest.code);
        if(!posePart(s.part, ptest.part0))
        {
            pkFail("its part not shown");
            return;
        }
        ptest.placed0 = st.note.placed.weapon[0];
        ptest.target0 = st.note.t.weapon[0];
        if(ptest.step == 0)
        {
            Cbuf_AddText("screenshot\n"); // (before)
        }
        ptest.wait = 0;
        ptest.phase = 1;
        return;
    }
    case 1: // the drag queued (a frame after the screenshot)
        if(++ptest.wait < 2)
        {
            return;
        }
        if(!mockDrag(2, s.part, s.axis, s.amount))
        {
            pkFail("no such handle shown");
            return;
        }
        ptest.wait = 0;
        ptest.phase = 2;
        return;
    case 2: // the drag done: settled again
        if(mockBusy() || st.dragging)
        {
            if(++ptest.wait > 200)
            {
                pkFail("the drag never ended");
            }
            return;
        }
        settleReset(ptest.settle);
        ptest.phase = 3;
        return;
    case 3:
    {
        if(!settled(ptest.settle))
        {
            if(ptest.settle.frames > 600)
            {
                pkFail("timed out after the drag");
            }
            return;
        }
        const agpose::Key k = agpose::value(ptest.code);
        rigpose::Rigid now;
        (void)posePart(s.part, now);
        const glm::quat body = bodyTurn();
        glm::vec3 axisB{0.f};
        axisB[s.axis % 20 % 3] = 1.f;
        bool ok = true;
        char text[400];
        if(s.part == 0 && s.axis < 3)
        {
            // its place: the drag's move plus where it was drawn from its target (the mapping's: let go where it is)
            const glm::vec3 want = axisB * s.amount + (ptest.placed0.pos - ptest.target0.pos);
            const glm::vec3 got = k.v[agpose::RPos] - ptest.key0.v[agpose::RPos];
            const glm::vec3 moved = glm::inverse(body) * (now.pos - ptest.part0.pos);
            ok = glm::length(got - want) < 0.3f && za::abs(glm::dot(moved, axisB) - s.amount) < 1.f;
            q_snprintf(text, sizeof(text), "key RPos +%.2f %.2f %.2f (asked %.2f %.2f %.2f), drawn moved %.2f %.2f %.2f",
                got.x, got.y, got.z, want.x, want.y, want.z, moved.x, moved.y, moved.z);
        }
        else if(s.part == 0)
        {
            const glm::quat R = glm::angleAxis(s.amount * deg, axisB);
            const glm::quat want = R * ptest.placed0.rot * glm::inverse(ptest.target0.rot) * rotOf(ptest.key0.v[agpose::RTurn]);
            const float off = quatDeg(rotOf(k.v[agpose::RTurn]), want);
            const float turned = quatDeg(now.rot, ptest.part0.rot);
            ok = off < 0.5f && za::abs(turned - s.amount) < 3.f;
            q_snprintf(text, sizeof(text), "key RTurn %.2f %.2f %.2f (off the asked %.2f deg), drawn turned %.2f deg",
                k.v[agpose::RTurn].x, k.v[agpose::RTurn].y, k.v[agpose::RTurn].z, off, turned);
        }
        else if(s.part == 4)
        {
            const glm::vec3 got = k.v[agpose::ElbowR] - ptest.key0.v[agpose::ElbowR];
            const glm::vec3 moved = glm::inverse(body) * (now.pos - ptest.part0.pos);
            ok = glm::length(got) > 0.05f && glm::dot(moved, axisB) * s.amount > 0.5f;
            q_snprintf(text, sizeof(text), "key ElbowR +%.3f %.3f %.3f, drawn elbow moved %.2f %.2f %.2f", got.x, got.y, got.z,
                moved.x, moved.y, moved.z);
        }
        else if(s.part == 6)
        {
            const glm::vec3 got = k.v[agpose::Torso] - ptest.key0.v[agpose::Torso];
            const glm::vec3 f0 = ptest.part0.rot * glm::vec3{1.f, 0.f, 0.f}, f1 = now.rot * glm::vec3{1.f, 0.f, 0.f};
            const float yaw = (za::atan2(f1.y, f1.x) - za::atan2(f0.y, f0.x)) / deg;
            ok = za::abs(got.x - s.amount) < 0.5f && za::abs(got.y) < 0.5f && za::abs(got.z) < 0.5f && za::abs(yaw - s.amount) < 3.f;
            q_snprintf(text, sizeof(text), "key Torso +%.2f %.2f %.2f, drawn chest turned %.2f deg", got.x, got.y, got.z, yaw);
        }
        else
        {
            const glm::vec3 got = k.v[agpose::LegPelvis] - ptest.key0.v[agpose::LegPelvis];
            const glm::vec3 moved = glm::inverse(body) * (now.pos - ptest.part0.pos);
            ok = za::abs(got.z - s.amount) < 0.3f && za::abs(got.x) < 0.3f && za::abs(got.y) < 0.3f && za::abs(moved.z - s.amount) < 1.f;
            q_snprintf(text, sizeof(text), "key LegPelvis +%.2f %.2f %.2f, drawn pelvis moved %.2f %.2f %.2f", got.x, got.y, got.z,
                moved.x, moved.y, moved.z);
        }
        Con_Printf("rigstudio posekeys %d: %s: %s: %s\n", ptest.step + 1, s.name, text, ok ? "PASS" : "FAIL");
        (ok ? ptest.passed : ptest.failed)++;
        if(ptest.step == 0)
        {
            Cbuf_AddText("screenshot\n"); // (after)
        }
        ptest.wait = 0;
        ptest.phase = 4;
        return;
    }
    case 4: // undone (a frame after the screenshot): the key exactly as it was, the part back
        if(++ptest.wait < 2)
        {
            return;
        }
        undo();
        settleReset(ptest.settle);
        ptest.phase = 5;
        return;
    case 5:
    {
        if(!settled(ptest.settle))
        {
            if(ptest.settle.frames > 600)
            {
                pkFail("timed out after the undo");
            }
            return;
        }
        const agpose::Key k = agpose::value(ptest.code);
        rigpose::Rigid now;
        (void)posePart(s.part, now);
        const bool exact = !memcmp(&k, &ptest.key0, sizeof(k));
        const float back = glm::length(now.pos - ptest.part0.pos), turned = quatDeg(now.rot, ptest.part0.rot);
        const bool ok = exact && back < 0.2f && turned < 0.5f;
        Con_Printf("rigstudio posekeys %d: %s: undone: the key %s, the part %.3f units, %.2f deg from before: %s\n",
            ptest.step + 1, s.name, exact ? "exactly as it was" : "CHANGED", back, turned, ok ? "PASS" : "FAIL");
        (ok ? ptest.passed : ptest.failed)++;
        ptest.phase = 9;
        return;
    }
    default: break;
    }
    // The next step
    ptest.step++;
    if(ptest.step >= pkStepCount)
    {
        Con_Printf("rigstudio posekeys: %d passed, %d failed\n", ptest.passed, ptest.failed);
        ptest.step = -1;
        return;
    }
    st.pdPose = pkSteps[ptest.step].pose;
    settleReset(ptest.settle);
    ptest.phase = 0;
}

void testPoseKeys_f()
{
    if(!st.open)
    {
        Con_Printf("rigstudio posekeys: the studio is not open\n");
        return;
    }
    st.tool = Tool::PoseKeys;
    st.pdFollow = true;
    st.aim.selected = false;
    ptest = PkTest{};
    ptest.step = 0;
    ptest.phase = -1; // (waits for its subject)
    st.pdPose = pkSteps[0].pose;
    settleReset(ptest.settle);
}

void testSubjects_f()
{
    if(!st.open)
    {
        Con_Printf("rigstudio subjects: the studio is not open\n");
        return;
    }
    stest = SubjectTest{};
    stest.step = 0;
    stest.started = false; // (its first change once the first subject stands)
}

void print_f()
{
    printDefaults();
}

void play_f()
{
    st.playing = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) != 0 : !st.playing;
}

void respawn_f()
{
    if(Cmd_Argc() > 1)
    {
        q_strlcpy(st.choice.spawn, Cmd_Argv(1), sizeof(st.choice.spawn));
    }
    if(Cmd_Argc() > 2)
    {
        st.choice.weapon = Q_atoi(Cmd_Argv(2));
    }
    if(Cmd_Argc() > 3)
    {
        st.choice.weapon2 = Q_atoi(Cmd_Argv(3));
    }
    respawn();
}

} // namespace

namespace qvr::rigstudio::detail
{

const aliashdr_t* hdrOf()
{
    if(!st.model || st.model->type != mod_alias)
    {
        return nullptr;
    }
    return static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(st.model)));
}

void say(const char* text)
{
    q_strlcpy(st.status, text, sizeof(st.status));
    st.statusAt = realtime;
    Con_Printf("rigstudio: %s\n", text);
}

void pushUndo(const char* what)
{
    if(!st.rig)
    {
        return;
    }
    Snapshot s;
    s.m = st.doc;
    for(int b = 0; b < maxBones; b++)
    {
        s.live[b] = st.live[b];
    }
    s.seq = st.seq;
    s.what = what;
    st.undo.pushBack(ZA_MOVE(s));
    if(st.undo.size() > static_cast<za::SizeT>(maxUndo))
    {
        st.undo.erase(st.undo.begin());
    }
    st.redo.clear();
}

void restoreSnapshot(Snapshot& s)
{
    const za::SizeT oldSeqs = st.doc.seqs.size();
    st.doc = s.m;
    for(int b = 0; b < maxBones; b++)
    {
        st.live[b] = s.live[b];
    }
    st.seq = st.doc.seqs.empty() ? -1 : za::clamp(s.seq, 0, static_cast<int>(st.doc.seqs.size()) - 1);
    if(st.doc.seqs.size() != oldSeqs)
    {
        st.dirtySeq.resize(st.doc.seqs.size(), 1);
    }
    for(uint8_t& d : st.dirtySeq)
    {
        d = 1;
    }
    st.dirtyRig = true;
    st.selKeys.clear();
    st.docGen++;
    publish();
}

void keepPreEdit()
{
    st.preEdit.m = st.doc;
    for(int b = 0; b < maxBones; b++)
    {
        st.preEdit.live[b] = st.live[b];
    }
    st.preEdit.seq = st.seq;
}

void pushPreEditUndo(const char* what)
{
    if(!st.rig)
    {
        return;
    }
    Snapshot s = st.preEdit;
    s.what = what;
    st.undo.pushBack(ZA_MOVE(s));
    if(st.undo.size() > static_cast<za::SizeT>(maxUndo))
    {
        st.undo.erase(st.undo.begin());
    }
    st.redo.clear();
}

// A Pose Studio key's or grip's entry: its value put back, the one now kept in `back` (the other stack's).
bool swapExternal(Snapshot& s, Snapshot& back)
{
    if(s.aimEntry)
    {
        back.aimEntry = true;
        back.aimAt = st.aim.at;
        back.what = s.what;
        st.aim.at = s.aimAt;
        st.aim.placed = true;
        st.aim.atCamera = false; // (put back where it was: no longer on the camera)
        return true;
    }
    if(s.poseCode >= 0)
    {
        back.poseCode = s.poseCode;
        back.poseKey = agpose::value(s.poseCode);
        back.what = s.what;
        agpose::set(s.poseCode, s.poseKey);
        return true;
    }
    if(s.gripKind >= 0)
    {
        back.gripKind = s.gripKind;
        back.gripSlot = s.gripSlot;
        back.gripHand = s.gripHand;
        back.grip = s.gripSlot == aggrip::allWeapons ? aggrip::global(s.gripKind, s.gripHand) : aggrip::offset(s.gripKind, s.gripSlot, s.gripHand);
        back.what = s.what;
        aggrip::set(s.gripKind, s.gripSlot, s.gripHand, s.grip);
        return true;
    }
    return false;
}

void keepCurrent(Snapshot& back, const Snapshot& s)
{
    back.m = st.doc;
    for(int b = 0; b < maxBones; b++)
    {
        back.live[b] = st.live[b];
    }
    back.seq = st.seq;
    back.what = s.what;
}

void undo()
{
    if(st.undo.empty())
    {
        say("nothing to undo");
        return;
    }
    Snapshot s = ZA_MOVE(st.undo.back());
    st.undo.popBack();
    Snapshot back;
    if(!swapExternal(s, back))
    {
        keepCurrent(back, s);
        restoreSnapshot(s);
    }
    st.redo.pushBack(ZA_MOVE(back));
    say(va("undone: %s", s.what.cStr()));
}

void redo()
{
    if(st.redo.empty())
    {
        say("nothing to redo");
        return;
    }
    Snapshot s = ZA_MOVE(st.redo.back());
    st.redo.popBack();
    Snapshot back;
    if(!swapExternal(s, back))
    {
        keepCurrent(back, s);
        restoreSnapshot(s);
    }
    st.undo.pushBack(ZA_MOVE(back));
    say(va("redone: %s", s.what.cStr()));
}

void pushPoseUndo(int code)
{
    Snapshot s;
    s.poseCode = code;
    s.poseKey = agpose::value(code);
    s.what = "pose key";
    st.undo.pushBack(ZA_MOVE(s));
    st.redo.clear();
}

void pushGripUndo(int kind, int slot, int hand)
{
    Snapshot s;
    s.gripKind = kind;
    s.gripSlot = slot;
    s.gripHand = hand;
    s.grip = slot == aggrip::allWeapons ? aggrip::global(kind, hand) : aggrip::offset(kind, slot, hand);
    s.what = "grip";
    st.undo.pushBack(ZA_MOVE(s));
    st.redo.clear();
}

void pushAimUndo()
{
    Snapshot s;
    s.aimEntry = true;
    s.aimAt = st.aim.at;
    s.what = "aim target";
    st.undo.pushBack(ZA_MOVE(s));
    if(st.undo.size() > static_cast<za::SizeT>(maxUndo))
    {
        st.undo.erase(st.undo.begin());
    }
    st.redo.clear();
}

glm::vec3 aimDefault()
{
    const float yaw = st.stageYaw * deg;
    return st.stage + glm::vec3{0.f, 0.f, 25.f + aimHeight} + glm::vec3{za::cos(yaw), za::sin(yaw), 0.f} * aimAhead;
}

void resetAim()
{
    pushAimUndo();
    st.aim.at = aimDefault();
    st.aim.placed = true;
    st.aim.atCamera = false;
}

void publish()
{
    if(st.model && st.rig)
    {
        rigdata::publish(st.model, st.doc);
    }
}

void touchedRig()
{
    st.dirtyRig = true;
    st.docGen++;
    publish();
}

void touchedSeq(int seq)
{
    if(seq >= 0 && seq < static_cast<int>(st.dirtySeq.size()))
    {
        st.dirtySeq[static_cast<za::SizeT>(seq)] = 1;
    }
    st.docGen++;
    publish();
}

void save()
{
    if(!st.rig || !st.model)
    {
        say("nothing to save");
        return;
    }
    char path[MAX_OSPATH], dir[MAX_OSPATH], root[MAX_OSPATH];
    rigdata::rootDir(root, sizeof(root));
    q_snprintf(dir, sizeof(dir), "%s/rigs", root);
    files::createDirectories(dir);
    rigdata::rigPath(st.doc.base.cStr(), path, sizeof(path));
    za::String text;
    rigdata::writeRig(st.doc, *st.rig, text);
    int written = 0;
    if(files::writeText(path, text.toStringView()))
    {
        written++;
    }
    else
    {
        say(va("could not write %s", path));
    }
    rigdata::animDir(st.doc.base.cStr(), dir, sizeof(dir));
    files::createDirectories(dir);
    for(const rigdata::Sequence& s : st.doc.seqs)
    {
        rigdata::animPath(st.doc.base.cStr(), s.name.cStr(), path, sizeof(path));
        rigdata::writeSequence(st.doc, s, *st.rig, text);
        if(files::writeText(path, text.toStringView()))
        {
            written++;
        }
        else
        {
            say(va("could not write %s", path));
        }
    }
    st.dirtyRig = false;
    for(uint8_t& d : st.dirtySeq)
    {
        d = 0;
    }
    publish();
    say(va("saved %d files (%s)", written, st.doc.base.cStr()));
}

void revert()
{
    if(!st.model)
    {
        return;
    }
    pushUndo("revert");
    rigdata::reload(st.model);
    const rigdata::ModelData* m = rigdata::of(st.model);
    const za::String model = st.doc.model, base = st.doc.base;
    st.doc = m ? *m : rigdata::ModelData{};
    st.doc.model = model;
    st.doc.base = base;
    for(int b = 0; b < st.numBones; b++)
    {
        st.doc.boneName[b] = st.rig->bones[b].name;
    }
    st.dirtyRig = false;
    st.dirtySeq.clear();
    st.dirtySeq.resize(st.doc.seqs.size(), 0);
    st.seq = st.doc.seqs.empty() ? -1 : za::clamp(st.seq, 0, static_cast<int>(st.doc.seqs.size()) - 1);
    if(st.source == Source::Seq && st.seq < 0)
    {
        st.source = Source::Rest;
    }
    st.selKeys.clear();
    st.docGen++;
    clearLive();
    publish();
    say("reverted to the files");
}

void printDefaults()
{
    if(!st.rig)
    {
        return;
    }
    za::String out;
    out += va("// Quake VR: the Rig Studio's overrides of %s as defaults (Print as Defaults).\n", st.doc.model.cStr());
    out += "// Bones: rows for its seed table (vr_ragdoll.cpp / vr_ad_rigs.inc: name, parent, joint, centre, pivot, end,\n";
    out += "// capsule, cone, twist, flex, hinge); the centre is the bone's middle, flex its upper limit plus its rest bend.\n";
    const char* joints[] = {"Root", "Ball", "Hinge", "Loose"};
    int rows = 0;
    for(int b = 0; b < st.numBones; b++)
    {
        const rigdata::BoneEdit& e = st.doc.bones[b];
        if(!e.on)
        {
            continue;
        }
        const ragdoll::Bone& rb = st.rig->bones[b];
        const glm::vec3 c = (e.pivot + e.end) * 0.5f;
        float bend = 0.f;
        if(rb.joint == ragdoll::Joint::Hinge && rb.parent >= 0)
        {
            const glm::vec3 pp = st.doc.bones[rb.parent].on ? st.doc.bones[rb.parent].pivot : st.rig->bones[rb.parent].pivot;
            const glm::vec3 u = glm::normalize(e.pivot - pp), l = glm::normalize(e.end - e.pivot);
            bend = glm::degrees(za::acos(za::clamp(glm::dot(u, l), -1.f, 1.f)));
        }
        out += va("    {\"%s\", %d, Joint::%s, {%.1ff, %.1ff, %.1ff}, {%.1ff, %.1ff, %.1ff}, {%.1ff, %.1ff, %.1ff}, %.1ff, %.1ff, %.1ff, %.1ff, {%.2ff, %.2ff, %.2ff}},\n",
            rb.name, rb.parent, joints[static_cast<int>(rb.joint)], c.x, c.y, c.z, e.pivot.x, e.pivot.y, e.pivot.z, e.end.x,
            e.end.y, e.end.z, rb.capsule, e.cone, e.twist, rb.joint == ragdoll::Joint::Hinge ? e.upper + bend : 0.f, e.hinge.x,
            e.hinge.y, e.hinge.z);
        rows++;
    }
    if(rows == 0)
    {
        out += "    // (no bone overrides)\n";
    }
    out += va("// Vertex weights (%d vertices; the engine has no table for them: they stay in the rig file, rigs/%s.rig.txt):\n",
        static_cast<int>(st.doc.weights.size()), st.doc.base.cStr());
    for(const rigdata::VertWeights& w : st.doc.weights)
    {
        out += va("//   {%d", w.v);
        for(int i = 0; i < w.n; i++)
        {
            out += va(", {\"%s\", %.3ff}", st.rig->bones[w.bone[i]].name, w.w[i]);
        }
        out += "},\n";
    }
    out += va("// Poses (%d; locals: rotation vector in degrees, offset in units):\n", static_cast<int>(st.doc.poses.size()));
    for(const rigdata::NamedPose& p : st.doc.poses)
    {
        out += va("//   pose %s:", p.name.cStr());
        for(int b = 0; b < st.numBones; b++)
        {
            if(p.pose.mask & (1u << b))
            {
                const rigdata::Local& l = p.pose.bone[b];
                out += va(" {\"%s\", {%.2f, %.2f, %.2f}, {%.2f, %.2f, %.2f}}", st.rig->bones[b].name, l.rot.x, l.rot.y, l.rot.z,
                    l.off.x, l.off.y, l.off.z);
            }
        }
        out += "\n";
    }
    Con_Printf("%s", out.cStr());
    const time_t now = time(nullptr);
    const za::String dir = za::String{com_gamedir} + "/proposed_defaults";
    files::createDirectories(dir.cStr());
    za::String path = dir + "/rig_" + st.doc.base + "_" + files::timestamp(now) + ".txt";
    if(files::writeText(path.cStr(), out.toStringView()))
    {
        say(va("printed as defaults: %s", path.cStr()));
    }
}

void respawn()
{
    if(!inGame() || !st.staged)
    {
        return;
    }
    removeSubject(); // (it and everything it owns: its weapons, their records, a worn tank)
    Cbuf_InsertText("vr_scene_clean all\n"); // (what it left about: gibs, shells, fires)
    st.spawnAt = sv.qcvm.time + 0.25;
}

void close()
{
    if(!st.open)
    {
        return;
    }
    st.open = false;
    st.playing = false;
    if(st.dragging && (st.dragKind == 11 || st.dragKind == 12))
    {
        posestudio::deskEnd(true); // (a Pose Keys drag under way: kept, its preview never left on)
    }
    st.dragging = false;
    st.dragKind = 0;
    imgui::want(false);
    rigseq::setStudio(0, nullptr, 0, false);
    restoreSettings();
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if(inGame())
    {
        removeSubject(); // (the studio's stand-in: never left in the map, unkillable, its AI let go)
        const SvVm vm;
        edict_t* p = EDICT_NUM(1);
        p->v.flags = static_cast<float>(static_cast<int>(p->v.flags) & ~(FL_GODMODE | FL_NOTARGET));
    }
    st.subject = 0;
    st.staged = false;
    IN_Activate();
    Con_Printf("rigstudio: closed%s\n", st.dirtyRig || za::anyOf(st.dirtySeq.begin(), st.dirtySeq.end(), [](uint8_t d) { return d != 0; }) ? " (unsaved changes kept in this session: vr_rigstudio to go on)" : "");
}

int groupCount()
{
    return groups.n;
}

const rigdata::AnimGroup& groupAt(int i)
{
    return groups.g[za::clamp(i, 0, za::max(groups.n - 1, 0))];
}

float timelineLength()
{
    switch(st.source)
    {
    case Source::Anim: return groups.n ? static_cast<float>(groupAt(st.group).count) : 1.f;
    case Source::Seq: return curSeq() ? static_cast<float>(curSeq()->length) : 1.f;
    default: return 1.f;
    }
}

rigdata::Sequence* curSeq()
{
    if(st.seq < 0 || st.seq >= static_cast<int>(st.doc.seqs.size()))
    {
        return nullptr;
    }
    return &st.doc.seqs[static_cast<za::SizeT>(st.seq)];
}

rigdata::AnimOverride* curOverride(bool make)
{
    if(st.source != Source::Anim || groups.n == 0)
    {
        return nullptr;
    }
    const char* name = groupAt(st.group).name;
    for(rigdata::AnimOverride& o : st.doc.overrides)
    {
        if(!q_strcasecmp(o.anim.cStr(), name))
        {
            return &o;
        }
    }
    if(!make)
    {
        return nullptr;
    }
    st.doc.overrides.pushBack(rigdata::AnimOverride{});
    st.doc.overrides.back().anim = name;
    return &st.doc.overrides.back();
}

za::Vector<rigdata::Track>* curTracks(bool make)
{
    if(st.source == Source::Seq)
    {
        rigdata::Sequence* s = curSeq();
        return s ? &s->tracks : nullptr;
    }
    rigdata::AnimOverride* o = curOverride(make);
    return o ? &o->tracks : nullptr;
}

rigdata::Track* trackOf(za::Vector<rigdata::Track>& tracks, int bone, bool make)
{
    for(rigdata::Track& t : tracks)
    {
        if(t.bone == bone)
        {
            return &t;
        }
    }
    if(!make)
    {
        return nullptr;
    }
    tracks.pushBack(rigdata::Track{});
    tracks.back().bone = bone;
    return &tracks.back();
}

void clearLive()
{
    for(int b = 0; b < maxBones; b++)
    {
        st.live[b] = rigdata::Local{};
    }
}

void setFrame(float f)
{
    const float len = timelineLength();
    if(st.source == Source::Rest)
    {
        st.frame = 0.f;
        return;
    }
    const bool loop = st.source == Source::Seq ? (curSeq() && curSeq()->loop) : st.loop;
    if(loop)
    {
        st.frame = f - za::floor(f / len) * len;
    }
    else
    {
        st.frame = za::clamp(f, 0.f, za::max(len - 1.f, 0.f));
    }
}

// The bone's key at the frame (its value with the live delta folded in), the delta cleared.
void keyBone(int b)
{
    za::Vector<rigdata::Track>* tracks = curTracks(true);
    if(!tracks || b < 0 || b >= st.numBones)
    {
        return;
    }
    rigdata::Track* t = trackOf(*tracks, b, true);
    const float frame = za::floor(st.frame + 0.5f);
    // Its value at the frame now (keys, or the frame's own for a sequence's new track), then the delta.
    rigpose::Rigid F[maxBones], P[maxBones];
    evaluate(frame, F, P, false);
    rigdata::Local base;
    rigdata::Local sampled;
    const bool keyed = rigdata::sample(*t, frame, st.source == Source::Seq && curSeq() && curSeq()->loop ? static_cast<float>(curSeq()->length) : 0.f, sampled);
    if(st.source == Source::Seq)
    {
        if(keyed)
        {
            base = sampled;
        }
        else
        {
            const int p = st.rig->bones[b].parent;
            const rigpose::Rigid L = (p >= 0 && p < b) ? rigdata::compose(rigdata::inverse(P[p]), P[b]) : P[b];
            base = rigdata::fromRigid(L, st.pivots[b]);
        }
    }
    else
    {
        base = keyed ? sampled : rigdata::Local{};
    }
    const rigpose::Rigid v = rigdata::compose(rigdata::toRigid(base, st.pivots[b]), rigdata::toRigid(st.live[b], st.pivots[b]));
    rigdata::Key k;
    k.frame = frame;
    k.value = rigdata::fromRigid(v, st.pivots[b]);
    bool replaced = false;
    for(rigdata::Key& old : t->keys)
    {
        if(old.frame == frame)
        {
            k.interp = old.interp;
            old = k;
            replaced = true;
        }
    }
    if(!replaced)
    {
        za::SizeT at = 0;
        while(at < t->keys.size() && t->keys[at].frame < frame)
        {
            at++;
        }
        t->keys.insert(t->keys.begin() + static_cast<za::PtrDiffT>(at), k);
    }
    st.live[b] = rigdata::Local{};
}

void keySelected()
{
    if(st.source == Source::Rest)
    {
        say("the rest pose has no keys: pick an animation or a sequence in the timeline");
        return;
    }
    int n = 0;
    for(int b = 0; b < st.numBones; b++)
    {
        if(st.selBones & (1u << b))
        {
            keyBone(b);
            n++;
        }
    }
    if(st.source == Source::Seq)
    {
        touchedSeq(st.seq);
    }
    else
    {
        touchedRig();
    }
    if(n == 0)
    {
        say("select the bones to key");
    }
}

void newSequence(const char* name, int frames, float fps, bool loop)
{
    if(!st.rig)
    {
        return;
    }
    pushUndo("new sequence");
    rigdata::Sequence s;
    s.name = name;
    s.length = za::max(frames, 1);
    s.fps = za::clamp(fps, 0.1f, 240.f);
    s.loop = loop;
    for(za::SizeT i = 0; i < st.doc.seqs.size(); i++)
    {
        if(!q_strcasecmp(st.doc.seqs[i].name.cStr(), name))
        {
            s.name = va("%s_%d", name, static_cast<int>(st.doc.seqs.size()) + 1);
        }
    }
    st.doc.seqs.pushBack(ZA_MOVE(s));
    st.dirtySeq.pushBack(1);
    st.seq = static_cast<int>(st.doc.seqs.size()) - 1;
    st.source = Source::Seq;
    st.frame = 0.f;
    clearLive();
    touchedSeq(st.seq);
    say(va("new sequence %s: %d frames at %g fps", st.doc.seqs.back().name.cStr(), st.doc.seqs.back().length, st.doc.seqs.back().fps));
}

void selectBone(int b, bool add)
{
    if(!add)
    {
        st.selBones = 0;
    }
    if(b >= 0 && b < st.numBones)
    {
        st.selBones |= 1u << b;
        st.active = b;
    }
    else if(!add)
    {
        st.active = -1;
    }
}

int boneByName(const char* name)
{
    for(int b = 0; st.rig && b < st.numBones; b++)
    {
        if(!q_strcasecmp(st.rig->bones[b].name, name))
        {
            return b;
        }
    }
    return -1;
}

int mirrorBone(int b)
{
    if(!st.rig || b < 0)
    {
        return b;
    }
    char name[16];
    q_strlcpy(name, st.rig->bones[b].name, sizeof(name));
    const size_t n = strlen(name);
    if(n > 2 && name[n - 2] == '_' && (name[n - 1] == 'l' || name[n - 1] == 'r'))
    {
        name[n - 1] = name[n - 1] == 'l' ? 'r' : 'l';
        const int m = boneByName(name);
        return m >= 0 ? m : b;
    }
    return b;
}

void evaluate(float frame, rigpose::Rigid* F, rigpose::Rigid* P, bool withLive)
{
    const aliashdr_t* hdr = hdrOf();
    if(!st.rig || !hdr)
    {
        return;
    }
    const ragdoll::Rig& rig = *st.rig;
    rigdata::restOf(rig, &st.doc, st.pivots, st.ends);
    int p1 = 0, p2 = 0;
    float bl = 0.f;
    if(st.source == Source::Anim && groups.n > 0)
    {
        const rigdata::AnimGroup& g = groupAt(st.group);
        const int i1 = za::clamp(static_cast<int>(za::floor(frame)), 0, g.count - 1);
        const int i2 = st.loop ? (i1 + 1) % g.count : za::min(i1 + 1, g.count - 1);
        bl = za::clamp(frame - static_cast<float>(i1), 0.f, 1.f);
        p1 = rigdata::poseOfFrame(hdr, g.first + i1);
        p2 = rigdata::poseOfFrame(hdr, g.first + i2);
    }
    else if(st.source == Source::Seq && curSeq() && !curSeq()->base.empty())
    {
        const int f = rigdata::frameOfName(hdr, curSeq()->base.cStr());
        p1 = p2 = f >= 0 ? rigdata::poseOfFrame(hdr, f) : 0;
    }
    for(int b = 0; b < rig.numBones; b++)
    {
        P[b] = rigdata::lerp(keyOf(p1, b), keyOf(p2, b), bl);
    }
    st.basePose1 = p1;
    st.basePose2 = p2;
    st.baseBlend = bl;
    const rigdata::Sequence* s = st.source == Source::Seq ? curSeq() : nullptr;
    const rigdata::AnimOverride* o = st.source == Source::Anim ? curOverride(false) : nullptr;
    for(int b = 0; b < rig.numBones; b++)
    {
        const int p = rig.bones[b].parent;
        rigpose::Rigid L = (p >= 0 && p < b) ? rigdata::compose(rigdata::inverse(P[p]), P[b]) : P[b];
        if(st.source == Source::Rest)
        {
            L = withLive ? rigdata::toRigid(st.live[b], st.pivots[b]) : rigpose::Rigid{};
        }
        else
        {
            const za::Vector<rigdata::Track>* tracks = s ? &s->tracks : o ? &o->tracks : nullptr;
            for(za::SizeT i = 0; tracks && i < tracks->size(); i++)
            {
                const rigdata::Track& t = (*tracks)[i];
                rigdata::Local v;
                if(t.bone != b || !rigdata::sample(t, frame, s && s->loop ? static_cast<float>(s->length) : 0.f, v))
                {
                    continue;
                }
                L = s ? rigdata::toRigid(v, st.pivots[b]) : rigdata::compose(L, rigdata::toRigid(v, st.pivots[b]));
            }
            if(withLive)
            {
                L = rigdata::compose(L, rigdata::toRigid(st.live[b], st.pivots[b]));
            }
        }
        F[b] = (p >= 0 && p < b) ? rigdata::compose(F[p], L) : L;
    }
}

rigdata::Pose currentPose()
{
    rigdata::Pose pose;
    if(!st.rig)
    {
        return pose;
    }
    rigpose::Rigid F[maxBones], P[maxBones];
    evaluate(st.frame, F, P, true);
    rigdata::localsOf(*st.rig, st.pivots, F, pose.bone);
    pose.mask = st.numBones >= 32 ? 0xffffffffu : (1u << st.numBones) - 1u;
    return pose;
}

void moveBone(int b, const rigpose::Rigid& world)
{
    if(!st.rig || b < 0 || b >= st.numBones)
    {
        return;
    }
    rigpose::Rigid F[maxBones], P[maxBones];
    evaluate(st.frame, F, P, true);
    // F_b = G D (D the live local or delta): D' = G^-1 W G D
    const rigpose::Rigid D = rigdata::toRigid(st.live[b], st.pivots[b]);
    const rigpose::Rigid G = rigdata::compose(F[b], rigdata::inverse(D));
    const rigpose::Rigid D2 = rigdata::compose(rigdata::inverse(G), rigdata::compose(world, F[b]));
    st.live[b] = rigdata::fromRigid(D2, st.pivots[b]);
}

void applyPose(const rigdata::Pose& p, uint32_t bones)
{
    if(!st.rig)
    {
        return;
    }
    // The pose's locals (absolute): Rest takes them as they are; Anim and Seq as deltas over the frame's.
    rigpose::Rigid F[maxBones], P[maxBones];
    evaluate(st.frame, F, P, false);
    rigdata::Local cur[maxBones];
    rigdata::localsOf(*st.rig, st.pivots, F, cur);
    for(int b = 0; b < st.numBones; b++)
    {
        if(!(bones & p.mask & (1u << b)))
        {
            continue;
        }
        if(st.source == Source::Rest)
        {
            st.live[b] = p.bone[b];
        }
        else
        {
            const rigpose::Rigid d = rigdata::compose(rigdata::inverse(rigdata::toRigid(cur[b], st.pivots[b])),
                rigdata::toRigid(p.bone[b], st.pivots[b]));
            st.live[b] = rigdata::fromRigid(d, st.pivots[b]);
        }
    }
    if(st.autoKey && st.source != Source::Rest)
    {
        const uint32_t keep = st.selBones;
        st.selBones = bones & p.mask;
        keySelected();
        st.selBones = keep;
    }
}

void mirrorPose()
{
    // Each bone's local mirrored across the model's y = 0 (left and right swapped): a rotation vector (x, y, z) becomes
    // (-x, y, -z), an offset (x, -y, z).
    rigdata::Pose cur = currentPose();
    rigdata::Pose m;
    m.mask = cur.mask;
    for(int b = 0; b < st.numBones; b++)
    {
        const int o = mirrorBone(b);
        const rigdata::Local& l = cur.bone[o];
        m.bone[b].rot = glm::vec3{-l.rot.x, l.rot.y, -l.rot.z};
        m.bone[b].off = glm::vec3{l.off.x, -l.off.y, l.off.z};
    }
    applyPose(m, 0xffffffffu);
}

// ---- Weights ----

void ensureWeights()
{
    if(!st.rig)
    {
        return;
    }
    const int nv = st.rig->numVerts;
    if(st.weightsGen == st.docGen && static_cast<int>(st.weights.size()) == nv)
    {
        return;
    }
    st.weights.resize(static_cast<za::SizeT>(nv));
    for(int v = 0; v < nv; v++)
    {
        rigdata::VertWeights& w = st.weights[static_cast<za::SizeT>(v)];
        w = rigdata::VertWeights{};
        w.v = v;
        w.n = 1;
        w.bone[0] = st.rig->vertBone[static_cast<za::SizeT>(v)];
        w.w[0] = 1.f;
    }
    for(const rigdata::VertWeights& o : st.doc.weights)
    {
        if(o.v >= 0 && o.v < nv)
        {
            st.weights[static_cast<za::SizeT>(o.v)] = o;
        }
    }
    if(static_cast<int>(st.selVerts.size()) != nv)
    {
        st.selVerts.clear();
        st.selVerts.resize(static_cast<za::SizeT>(nv), 0);
    }
    st.weightsGen = st.docGen;
}

rigdata::VertWeights& editWeights(int v)
{
    ensureWeights();
    za::SizeT at = 0;
    while(at < st.doc.weights.size() && st.doc.weights[at].v < v)
    {
        at++;
    }
    if(at == st.doc.weights.size() || st.doc.weights[at].v != v)
    {
        st.doc.weights.insert(st.doc.weights.begin() + static_cast<za::PtrDiffT>(at), st.weights[static_cast<za::SizeT>(v)]);
    }
    return st.doc.weights[at];
}

void normalizeVert(rigdata::VertWeights& w)
{
    // (the tiny dropped, the rest to make 1)
    int n = 0;
    for(int i = 0; i < w.n; i++)
    {
        if(w.w[i] > 0.001f)
        {
            w.bone[n] = w.bone[i];
            w.w[n] = w.w[i];
            n++;
        }
    }
    w.n = n;
    float sum = 0.f;
    for(int i = 0; i < w.n; i++)
    {
        sum += w.w[i];
    }
    if(sum <= 0.f)
    {
        w.n = 1;
        w.bone[0] = st.rig->vertBone[static_cast<za::SizeT>(w.v)];
        w.w[0] = 1.f;
        return;
    }
    for(int i = 0; i < w.n; i++)
    {
        w.w[i] /= sum;
    }
}

void setVertWeight(int v, int bone, float value)
{
    rigdata::VertWeights& w = editWeights(v);
    value = za::clamp(value, 0.f, 1.f);
    int at = -1;
    for(int i = 0; i < w.n; i++)
    {
        at = w.bone[i] == bone ? i : at;
    }
    if(at < 0)
    {
        if(w.n >= rigdata::maxWeights)
        {
            int least = 0;
            for(int i = 1; i < w.n; i++)
            {
                least = w.w[i] < w.w[least] ? i : least;
            }
            at = least;
        }
        else
        {
            at = w.n++;
        }
        w.bone[at] = bone;
        w.w[at] = 0.f;
    }
    // The others share what is left, as they were among themselves.
    float others = 0.f;
    for(int i = 0; i < w.n; i++)
    {
        others += i == at ? 0.f : w.w[i];
    }
    for(int i = 0; i < w.n; i++)
    {
        if(i == at)
        {
            w.w[i] = value;
        }
        else
        {
            w.w[i] = others > 1e-6f ? w.w[i] / others * (1.f - value) : 0.f;
        }
    }
    if(others <= 1e-6f && value < 1.f && w.n == 1)
    {
        w.w[at] = 1.f; // (its only bone: kept whole)
    }
    normalizeVert(w);
    st.docGen++;
}

void assignSelected(int bone)
{
    if(!st.rig || bone < 0)
    {
        return;
    }
    ensureWeights();
    int n = 0;
    for(int v = 0; v < st.rig->numVerts; v++)
    {
        if(!st.selVerts[static_cast<za::SizeT>(v)])
        {
            continue;
        }
        rigdata::VertWeights& w = editWeights(v);
        w.n = 1;
        w.bone[0] = bone;
        w.w[0] = 1.f;
        n++;
    }
    st.docGen++;
    touchedRig();
    say(va("%d vertices assigned to %s", n, st.rig->bones[bone].name));
}

void normalizeSelected()
{
    ensureWeights();
    for(int v = 0; st.rig && v < st.rig->numVerts; v++)
    {
        if(st.selVerts[static_cast<za::SizeT>(v)])
        {
            normalizeVert(editWeights(v));
        }
    }
    st.docGen++;
    touchedRig();
}

void selectByBone(int bone, bool add)
{
    ensureWeights();
    for(int v = 0; st.rig && v < st.rig->numVerts; v++)
    {
        const rigdata::VertWeights& w = st.weights[static_cast<za::SizeT>(v)];
        bool on = false;
        for(int i = 0; i < w.n; i++)
        {
            on = on || (w.bone[i] == bone && w.w[i] > 0.f);
        }
        uint8_t& s = st.selVerts[static_cast<za::SizeT>(v)];
        s = on ? 1 : (add ? s : 0);
    }
}

void growSelection(bool grow)
{
    ensureWeights();
    if(st.nbrs.empty())
    {
        return;
    }
    za::Vector<uint8_t> next = st.selVerts;
    for(za::SizeT v = 0; v < st.selVerts.size() && v < st.nbrs.size(); v++)
    {
        for(const int n : st.nbrs[v])
        {
            const uint8_t sn = st.selVerts[static_cast<za::SizeT>(n)];
            if(grow && sn)
            {
                next[v] = 1;
            }
            if(!grow && !sn)
            {
                next[v] = 0;
            }
        }
    }
    st.selVerts = next;
}

int mirrorVert(int v)
{
    if(st.restVerts.empty())
    {
        return -1;
    }
    const glm::vec3 p = st.restVerts[static_cast<za::SizeT>(v)];
    const glm::vec3 want{p.x, -p.y, p.z};
    int best = -1;
    float bestD = 0.75f; // units
    for(za::SizeT i = 0; i < st.restVerts.size(); i++)
    {
        const float d = glm::length(st.restVerts[i] - want);
        if(d < bestD)
        {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

} // namespace qvr::rigstudio::detail

namespace qvr::rigstudio
{

void init()
{
    Cmd_AddCommand("vr_rigstudio", open_f);
    Cmd_AddCommand("vr_rigstudio_close", close_f);
    Cmd_AddCommand("vr_rigstudio_select", select_f);
    Cmd_AddCommand("vr_rigstudio_rotate", rotate_f);
    Cmd_AddCommand("vr_rigstudio_move", move_f);
    Cmd_AddCommand("vr_rigstudio_key", key_f);
    Cmd_AddCommand("vr_rigstudio_frame", frame_f);
    Cmd_AddCommand("vr_rigstudio_source", source_f);
    Cmd_AddCommand("vr_rigstudio_seq_new", seqNew_f);
    Cmd_AddCommand("vr_rigstudio_seq_event", seqEvent_f);
    Cmd_AddCommand("vr_rigstudio_seq_set", seqSet_f);
    Cmd_AddCommand("vr_rigstudio_save", save_f);
    Cmd_AddCommand("vr_rigstudio_revert", revert_f);
    Cmd_AddCommand("vr_rigstudio_undo", undo_f);
    Cmd_AddCommand("vr_rigstudio_redo", redo_f);
    Cmd_AddCommand("vr_rigstudio_weight", weight_f);
    Cmd_AddCommand("vr_rigstudio_vertex", vertex_f);
    Cmd_AddCommand("vr_rigstudio_rest", rest_f);
    Cmd_AddCommand("vr_rigstudio_panel", panel_f);
    Cmd_AddCommand("vr_rigstudio_view", view_f);
    Cmd_AddCommand("vr_rigstudio_selverts", selverts_f);
    Cmd_AddCommand("vr_rigstudio_assign", assign_f);
    Cmd_AddCommand("vr_rigstudio_pose", pose_f);
    Cmd_AddCommand("vr_rigstudio_status", status_f);
    Cmd_AddCommand("vr_rigstudio_count", count_f);
    Cmd_AddCommand("vr_rigstudio_test_subjects", testSubjects_f);
    Cmd_AddCommand("vr_rigstudio_test_aim", testAim_f);
    Cmd_AddCommand("vr_rigstudio_test_posekeys", testPoseKeys_f);
    Cmd_AddCommand("vr_rigstudio_aim", aim_f);
    Cmd_AddCommand("vr_rigstudio_posekey", posekey_f);
    Cmd_AddCommand("vr_rigstudio_print", print_f);
    Cmd_AddCommand("vr_rigstudio_play", play_f);
    Cmd_AddCommand("vr_rigstudio_respawn", respawn_f);
    rigdata::registerCommands();
    rigseq::registerCommands();
}

void shutdown()
{
    close();
    imgui::shutdown();
}

bool active()
{
    return st.open;
}

void beforeLoad()
{
    // (a map loading while open: its edicts cleared, the subject with them; staged again in the new one)
    st.staged = false;
    st.subject = 0;
    st.spawnAt = -1.0;
    st.note = posestudio::Note{};
    st.noteFrame = -1;
    st.aim.placed = false; // (ahead of the new stage)
}

bool qcHold(int ent, int kind, int cls, glm::vec3& out)
{
    if(!st.open || ent != st.subject || st.subject <= 0)
    {
        return false;
    }
    st.subjKind = kind;
    st.subjClass = cls;
    if(st.tool == Tool::PoseKeys && st.pdKind == kind && st.pdClass == cls)
    {
        // (the Pose Keys tool: held in the window's pose, a strike at its phase; Aim at the aim target)
        out = glm::vec3{static_cast<float>(st.pdPose), static_cast<float>(agpose::isStrike(st.pdPose) ? st.pdPhase : 0), 0.f};
        return true;
    }
    out = glm::vec3{st.aim.on ? static_cast<float>(agpose::Aim) : -1.f, 0.f, 0.f};
    return true;
}

void serverFrame()
{
    subjectTestTick();
    if(!st.open || !inGame())
    {
        return;
    }
    if(st.wantMap && strcmp(sv.name, vr_rigstudio_map.string) != 0)
    {
        return; // (the room not loaded yet)
    }
    st.wantMap = false;
    const SvVm vm;
    edict_t* p = EDICT_NUM(1);
    p->v.flags = static_cast<float>(static_cast<int>(p->v.flags) | FL_GODMODE | FL_NOTARGET);
    if(!st.staged)
    {
        findStage();
        st.spawnAt = sv.qcvm.time + 0.1;
        return;
    }
    if(st.spawnAt >= 0.0 && sv.qcvm.time >= st.spawnAt)
    {
        makeSubject();
    }
    if(edict_t* e = subjectEdict())
    {
        holdSubject(e);
    }
}

void clientFrame()
{
    if(!st.open)
    {
        return;
    }
    if(VR_IsActive())
    {
        close(); // (a headset session began: the studio is flat-screen only)
        return;
    }
    imgui::want(true);
    mockFeed(); // (the tests' mouse, before ImGui's frame takes its events)
    if(!imgui::beginFrame())
    {
        return;
    }
    // The Pose Keys tool: the window's key the subject's (its kind, its weapon's class; entered: the pose it is in, Aim
    // with the aim target on), unless Follow the Subject is off
    if(st.tool == Tool::PoseKeys && st.pdFollow && st.subjKind >= 0 && st.subjClass >= 0)
    {
        const bool entered = st.lastTool != Tool::PoseKeys;
        st.pdKind = st.subjKind;
        st.pdClass = st.subjClass;
        int poses[agpose::NumPoses];
        const int np = agpose::posesOf(st.pdClass, poses, agpose::NumPoses);
        bool ok = false;
        for(int i = 0; i < np; i++)
        {
            ok = ok || poses[i] == st.pdPose;
        }
        if(entered || !ok)
        {
            st.pdPose = st.aim.on ? agpose::Aim : agpose::Carry;
        }
    }
    if(st.tool != Tool::PoseKeys && st.lastTool == Tool::PoseKeys && st.dragging && (st.dragKind == 11 || st.dragKind == 12))
    {
        posestudio::deskEnd(true);
        st.dragging = false;
        st.dragKind = 0;
    }
    st.lastTool = st.tool;
    // The mouse free and shown (the game's own look held: its input is the studio's)
    if(SDL_GetRelativeMouseMode())
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    if(SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
    {
        SDL_ShowCursor(SDL_ENABLE);
    }
    // The subject's model
    if(inGame() && st.subject > 0 && st.subject < cl_max_edicts && cl_entities)
    {
        const entity_t& e = cl_entities[st.subject];
        if(e.model && e.model->type == mod_alias)
        {
            takeModel(e.model);
        }
    }
    // The layers' cvars as the toggles have them
    if(!st.keptLayers)
    {
        st.keepIk = vr_agrunt_ik.value;
        st.keepLegs = vr_aglegs.value;
        st.keepUpper = vr_agrunt_upper.value;
        st.keepLerp = r_lerpmodels.value;
        st.keepKicks = vr_hit_react.value;
        st.layerArms = st.keepIk != 0.f;
        st.layerLegs = st.keepLegs != 0.f;
        st.layerUpper = st.keepUpper != 0.f;
        st.layerSmooth = st.keepLerp == 3.f;
        st.layerKicks = st.keepKicks != 0.f;
        st.keptLayers = true;
    }
    // Playback
    if(st.playing && st.source != Source::Rest)
    {
        const float fps = st.source == Source::Seq && curSeq() ? curSeq()->fps : 10.f;
        setFrame(st.frame + static_cast<float>(host_frametime) * fps * st.speed);
        if(st.source == Source::Seq && curSeq() && !curSeq()->loop && st.frame >= timelineLength() - 1.f && !st.loop)
        {
            st.playing = false;
        }
        if(st.source == Source::Anim && !st.loop && st.frame >= timelineLength() - 1.f)
        {
            st.playing = false;
        }
    }
    if(st.rig && hdrOf())
    {
        buildGeometry();
        ensureWeights();
        evaluate(st.frame, st.F, st.P, true);
        bool armsTouched = false;
        for(int b = 0; b < st.numBones; b++)
        {
            armsTouched = armsTouched || st.live[b].rot != glm::vec3{0.f} || st.live[b].off != glm::vec3{0.f};
        }
        if(const rigdata::AnimOverride* o = st.source == Source::Anim ? curOverride(false) : nullptr)
        {
            for(const rigdata::Track& t : o->tracks)
            {
                const char* bn = st.rig->bones[t.bone].name;
                armsTouched = armsTouched || (!t.keys.empty() && (!strncmp(bn, "upperarm", 8) || !strncmp(bn, "forearm", 7)));
            }
        }
        // (the game's arms on the pose's arm bones: the rest pose posed by hand, an animation's arm changes, a sequence's
        // Arms; else on the weapons as the game poses them)
        st.armsLayer = st.source == Source::Seq ? (curSeq() && curSeq()->arms) : (st.source == Source::Rest || armsTouched);
        poseGeometry();
        if(st.tool == Tool::PoseKeys)
        {
            rigseq::setStudio(0, nullptr, 0, false); // (Pose Keys: the game's own pose drawn, its frame and its arms)
        }
        else
        {
            rigseq::setStudio(st.subject, st.F, st.numBones, st.armsLayer);
        }
    }
    viewport();
    windows();
    aimTestTick(); // (the tests' steps: after the frame's note and drags)
    pkTestTick();
}

bool pinned(const entity_t* e, const void* aliashdr, short* pose1, short* pose2, float* blend)
{
    if(!st.open || st.subject <= 0 || !cl_entities || e != &cl_entities[st.subject] || !st.rig || st.tool == Tool::PoseKeys)
    {
        return false; // (Pose Keys: the frame the game holds it in: a run's stride, the pickup's kneel)
    }
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(st.basePose1 >= hdr->numposes || st.basePose2 >= hdr->numposes)
    {
        return false;
    }
    *pose1 = static_cast<short>(st.basePose1);
    *pose2 = static_cast<short>(st.basePose2);
    *blend = st.baseBlend;
    return true;
}

bool wantsNotes()
{
    return st.open;
}

void note(const posestudio::Note& n)
{
    if(st.open && n.ent == st.subject)
    {
        st.note = n;
        st.noteFrame = host_framecount;
    }
}

} // namespace qvr::rigstudio

// V_CalcRefdef: the studio's camera over the view.
extern "C" int VR_RigStudioView(float* origin, float* angles)
{
    if(!st.open || !inGame() || !st.staged)
    {
        return 0;
    }
    cameraView(origin, angles);
    return 1;
}

extern "C" int VR_RigStudioHolds(void)
{
    return 0; // (the scene runs on: the subject's arms are the server's poses each frame; its AI is frozen)
}
