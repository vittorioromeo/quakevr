// vr_view_diag.cpp -- the view's debug and test commands: model reload, grasp dump, bench and sweep, the ragdoll hand
// probe, hotspot fit and checks, anchorWhere, the posing checks, dumpView.
// Split from vr_view.cpp (a pure move); what they share is in vr_view_internal.hpp.

#include "vr_view_internal.hpp"
#include "vr_admelee.hpp"

namespace qvr::view
{
void setGhost(int hand, qmodel_t* model, const glm::vec3& pos, const glm::vec3& rot, float alpha)
{
    if(hand < 0 || hand > 1)
    {
        return;
    }
    ghostRequests[hand] = {model, pos, rot, alpha, host_framecount};
}

float fingerCurl(int hand, int finger)
{
    if(hand < 0 || hand > 1 || finger < 0 || finger > 4)
    {
        return 0.f;
    }
    // As drawn without a grasp: the curl (frames 0..5) with a held weapon's finger tweak.
    return rigCurl(hand, FingerThumb + finger) / 5.f;
}

void resetCaches()
{
    za::fill(viewModels, ViewModelEntry{});
    clipSizes.clear();
    for(RigHand& rh : rigHands)
    {
        rh.drawn = false;
        rh.held = Held{};
        rh.grasp = Grasp{};
    }
    handrig::reset();
    grasp::reset();
}

namespace
{
// A model name as the engine knows it: progs/<name>.mdl (a body's .md5mesh is its .mdl's enhanced replacement).
[[nodiscard]] za::String modelPath(const char* arg)
{
    za::String name = arg;
    za::replace(name.begin(), name.end(), '\\', '/');
    if(name.find('/') == za::StringView::nPos)
    {
        name = "progs/" + name;
    }
    const za::SizeT dot = name.rfind('.');
    if(dot == za::StringView::nPos || dot < name.rfind('/'))
    {
        name += ".mdl";
    }
    else if(!q_strcasecmp(name.cStr() + dot, ".md5mesh") || !q_strcasecmp(name.cStr() + dot, ".md5anim"))
    {
        name = name.substrByPosLen(0, dot) + ".mdl";
    }
    return name;
}

struct ReloadMatch
{
    za::Vector<za::String> names; // empty: the editable ones
    za::Vector<za::String> done;
};

qboolean reloadMatch(const char* name, void* ctx)
{
    auto& m = *static_cast<ReloadMatch*>(ctx);
    bool yes = false;
    if(m.names.empty())
    {
        const char* base = strrchr(name, '/');
        base = base ? base + 1 : name;
        // The models the Blender add-on edits (docs/vr-port/MODELS_IN_BLENDER.md).
        static constexpr const char* prefixes[] = {"v_", "vrbody", "vrgadget", "vrflashlight", "vrpauldron", "legholster",
            "vr_shell", "wpnbutton", "hand_base", "finger_"};
        yes = !q_strncasecmp(name, "progs/", 6) && za::anyOf(prefixes, prefixes + za::getArraySize(prefixes), [&](const char* p) {
            return !q_strncasecmp(base, p, strlen(p));
        });
    }
    else
    {
        yes = za::anyOf(m.names.begin(), m.names.end(), [&](const za::String& n) { return !q_strcasecmp(n.cStr(), name); });
    }
    if(yes)
    {
        m.done.emplaceBack(name);
    }
    return yes;
}

} // namespace

void modelReload_f()
{
    ReloadMatch m;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        m.names.pushBack(modelPath(Cmd_Argv(i)));
    }
    Mod_ReloadAliasModels(reloadMatch, &m);
    for(const za::String& n : m.names)
    {
        if(za::find(m.done.begin(), m.done.end(), n) == m.done.end())
        {
            Con_Printf("vr_model_reload: %s isn't loaded (it is read when it's first drawn)\n", n.cStr());
        }
    }
    if(m.done.empty())
    {
        return;
    }
    // What was worked out from the old files: forgotten, worked out again from the new ones as they are drawn.
    anchor::onGameDirChanged(); // the anchors' bindings (vr_anchor.cpp): by model, and a reloaded model keeps its slot
    grasp::reset();
    modelcollide::reset();
    selfcollide::reset();
    weapons::resetCaches();
    avatar::reset();
    flashlight::onModelsReloaded(za::find(m.done.begin(), m.done.end(), "progs/vrflashlight.mdl") != m.done.end());
    mem::on(mem::ModelReload); // the registered caches of models' data (vr_mem.hpp)
    // Each model named, or (all of them) one line, and the body's check.
    const bool each = !m.names.empty();
    bool body = false;
    za::String list;
    for(const za::String& n : m.done)
    {
        qmodel_t* model = Mod_ForName(n.cStr(), false);
        if(!model || model->type != mod_alias)
        {
            continue;
        }
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
        const bool isBody = strstr(n.cStr(), "vrbody") != nullptr;
        body = body || isBody;
        list += (list.empty() ? "" : ", ") + n.substrByPosLen(n.rfind('/') + 1);
        if(hdr->poseverttype == aliashdr_t::PV_QUAKE1)
        {
            if(each)
            {
                Con_Printf("vr_model_reload: %s: %d vertices, %d triangles, %d frames\n", n.cStr(), hdr->numverts,
                    hdr->numtris, hdr->numframes);
            }
        }
        else
        {
            const bool usable = !isBody || avatar::usable(model);
            if(each || !usable)
            {
                Con_Printf("vr_model_reload: %s: its enhanced replacement (%d vertices, %d bones)%s\n", n.cStr(),
                    hdr->numverts_vbo, hdr->numbones, usable ? "" : ": NOT usable as the body (see above)");
            }
        }
    }
    if(!each)
    {
        Con_Printf("vr_model_reload: %d models read again: %s\n", static_cast<int>(m.done.size()), list.cStr());
    }
    if(body)
    {
        bodyblood::clear(); // the wounds painted on the old mesh
    }
}

void woundTargets(entity_t* out[3])
{
    out[0] = entities.body.visible && entities.body.ent.model ? &entities.body.ent : nullptr;
    for(int hand = 0; hand < 2; hand++)
    {
        out[1 + hand] = rigHands[hand].drawn && entities.hand[hand][FingerBase].ent.model ? &entities.hand[hand][FingerBase].ent : nullptr;
    }
}

int handBonePoses(const entity_t* e, const float** matrices)
{
    for(int hand = 0; hand < 2; hand++)
    {
        if(rigHands[hand].drawn && (e == &entities.hand[hand][FingerBase].ent || e == &entities.ghostHand[hand].ent))
        {
            if(matrices)
            {
                *matrices = rigHands[hand].skin.data();
            }
            return handrig::data::numJoints;
        }
    }
    return 0;
}

void ragdollHandProbe_f()
{
    const float cm = 100.f / units::metresToUnits();
    bool any = false;
    for(int hand = 1; hand >= 0; hand--)
    {
        LimbHold limb;
        if(!limbHold(hand, limb))
        {
            continue;
        }
        any = true;
        const RigHand& rh = rigHands[hand];
        const char* name = hand == HAND_MAIN ? "main" : "off";
        float lag = -1.f;
        {
            qcvm_t* oldvm = nullptr;
            PR_PushQCVM(&sv.qcvm, &oldvm);
            lag = box3d::ragdollHandReach(EDICT_NUM(cl.viewentity), hand);
            PR_PopQCVM(oldvm);
        }
        const LimbEase& e = limbEase[hand];
        Con_Printf("ragdoll hand %s: edict %d part %d; the limb's held point %.1f cm from the hand (the spring's lag and sag); "
                   "the hand drawn %.1f cm and %.0f degrees (of %.0f) off its controller (eased %.2f)\n",
            name, limb.num, limb.part, lag * cm, glm::length(e.offset * e.blend) * cm,
            glm::degrees(glm::angle(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, e.turn, e.blend))), e.unclamped, e.blend);
        Con_Printf("ragdoll hand %s rest: the palm %.1f cm from the limb's mesh as held, %.1f cm rested on it (slid %.1f cm; "
                   "negative: in it; vr_ragdoll_hand_rest)\n",
            name, e.gapBefore * cm, e.gapAfter * cm, glm::length(e.rest) * cm);
        if(!rh.drawn || !limb.shape)
        {
            Con_Printf("  (the hand isn't drawn, or the limb has no shape)\n");
            continue;
        }
        {
            glm::vec3 mid{0.f};
            for(const grasp::Triangle& t : limb.shape->tris)
            {
                mid += (t.p[0] + t.p[1] + t.p[2]) / 3.f;
            }
            mid /= static_cast<float>(za::max<za::SizeT>(limb.shape->tris.size(), 1));
            glm::vec3 at;
            bool in = false;
            const float d = grasp::surfaceDistance(*limb.shape, limb.toWorld, limb.pos, 64.f, at, in);
            Con_Printf("  the limb's mesh: %d triangles, its middle %.1f cm from where the hand holds it, %.1f cm from its "
                       "surface%s\n",
                static_cast<int>(limb.shape->tris.size()), glm::distance(glm::vec3{limb.toWorld * glm::vec4{mid, 1.f}}, limb.pos) * cm,
                d * cm, in ? " (in it)" : "");
        }
        // The drawn hand's palm and fingertips to the limb's mesh (cm; negative: in it), and the fingers' closure.
        const auto toLimb = [&](const glm::vec3& rigPoint) {
            const glm::vec3 p{rh.rigToWorld * glm::vec4{drawnInRig(rh, rigPoint), 1.f}};
            glm::vec3 at;
            bool in = false;
            const float d = grasp::surfaceDistance(*limb.shape, limb.toWorld, p, 64.f, at, in);
            return d < 0.f ? 999.f : (in ? -d : d) * cm;
        };
        glm::vec3 tips[handrig::FingerCount];
        grasp::fingertips(rh.pose, tips);
        const Grasp& g = rh.grasp;
        int met = 0;
        float curl = 0.f;
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            met += g.valid && (g.solution.finger[f].met || g.solution.finger[f].fromClosed) ? 1 : 0;
            for(int j = 0; j < handrig::jointsPerFinger; j++)
            {
                curl += rh.joints[f][j];
            }
        }
        Con_Printf("  grasp %s (%s, part %d), %d of 5 fingers met it; drawn curl %.2f (0 open .. 4 fist); palm %.1f cm from "
                   "the limb's surface; fingertips %.1f %.1f %.1f %.1f %.1f cm\n",
            g.valid ? "solved" : "none", rh.held.shape ? "its limb" : "not the limb", rh.held.frame, met,
            curl / static_cast<float>(handrig::FingerCount * handrig::jointsPerFinger), toLimb(grasp::palmCentre()), toLimb(tips[0]),
            toLimb(tips[1]), toLimb(tips[2]), toLimb(tips[3]), toLimb(tips[4]));
    }
    if(!any)
    {
        Con_Printf("vr_ragdoll_hand_probe: no hand holds a ragdoll's limb\n");
    }
}

// vr_grasp_dump <main|off> <file>: the hand as drawn (its triangles, in its model space: hand_base.mdl's) and what it
// holds, in the same space, as an .obj ("o hand", "o held") in the game folder, to look at from any side
// (Misc/quakevr tools, round 21's composites).
void graspBench_f()
{
    const int runs = Cmd_Argc() > 1 ? CLAMP(1, Q_atoi(Cmd_Argv(1)), 100000) : 1000;
    for(int hand = 0; hand < 2; hand++)
    {
        const RigHand& rh = rigHands[hand];
        if(!rh.drawn || !rh.held.ent || !rh.held.ent->model)
        {
            continue;
        }
        const int frame = rh.held.frame >= 0 ? rh.held.frame : rh.held.ent->frame;
        const grasp::Shape* shape = heldShape(rh.held, frame);
        if(!shape)
        {
            continue;
        }
        const glm::mat4 inRig = glm::inverse(rh.solveRig) * heldToWorld(rh.held);
        const grasp::Settings settings = graspSettings(rh.held, rh.rigUnit);
        za::Vector<double> us;
        grasp::Solution s;
        for(int i = 0; i < runs; i++)
        {
            const auto t0 = za::Clock::nowNanoseconds();
            grasp::solve(rh.pose, *shape, inRig, settings, rh.grasp.valid ? &rh.grasp.solution : nullptr, s);
            us.pushBack(za::nanosecondsToMicroseconds(za::Clock::nowNanoseconds() - t0));
        }
        za::quickSort(us.begin(), us.end());
        // Solved afresh (as when first taken) and again with the solve before (as each frame it moves in the hand).
        grasp::Solution first;
        grasp::forgetSolves(); // a real solve, not a remembered one
        const auto t0 = za::Clock::nowNanoseconds();
        grasp::solve(rh.pose, *shape, inRig, settings, nullptr, first);
        const double firstUs = za::nanosecondsToMicroseconds(za::Clock::nowNanoseconds() - t0);

        Con_Printf("vr_grasp_bench: %s hand, %s (%d triangles): afresh %.1f us (%d probes, %d places); again, %d times: min "
                   "%.1f us, median %.1f us, max %.1f us (%d probes)\n",
            hand == HAND_MAIN ? "main" : "off", rh.held.ent->model->name, s.triangles, firstUs, first.probes, first.places, runs,
            us.front(), us[us.size() / 2], us.back(), s.probes);
        // Where the fingers stopped (afresh | again), to compare solvers.
        za::String stops;
        for(const grasp::Solution* sol : {&first, &s})
        {
            for(int f = 0; f < handrig::FingerCount; f++)
            {
                char b[64];
                const grasp::FingerStop& st = sol->finger[f];
                q_snprintf(b, sizeof(b), " %.2f/%.2f/%.2f%s%s%s", st.stop[0], st.stop[1], st.stop[2], st.met ? "m" : "",
                    st.startsInside ? "i" : "", st.fromClosed ? "c" : "");
                stops += b;
                q_snprintf(b, sizeof(b), "(%d)", sol->fingerProbes[f]);
                stops += b;
            }
            stops += sol == &first ? " |" : "";
        }
        Con_Printf("vr_grasp_bench: stops%s\n", stops.cStr());
    }
}

// vr_grasp_sweep [runs]: each hand's grasp of what it holds solved afresh at 7 places in the hand (as held; moved 0.3
// hand units along x, y, z; turned 4 degrees about x, z) and 8 kinds of solve (as held; again with the solve before;
// the place searched wider; the palm given; the thumb's style swapped; the thenar swapped and a deeper overlap; a prop's
// fit, turned to face and flush; cupped by the other hand, when drawn): one line each with the whole result, to the
// last bit ("gsweep ...": Misc/quakevr/grasp_compare.py compares two runs' lines), then the afresh solve as held timed
// `runs` times (default 20; its median, ms).
void graspSweep_f()
{
    const int runs = Cmd_Argc() > 1 ? CLAMP(1, Q_atoi(Cmd_Argv(1)), 10000) : 20;
    const auto text = [](int hand, const char* model, int variant, int place, const grasp::Solution& s) {
        za::String t = va("gsweep h%d %s v%d p%d palm %.9g %.9g %.9g turn %.9g %.9g %.9g %.9g thumb %d %.9g %.9g %.9g %.9g",
            hand, model, variant, place, s.palm.x, s.palm.y, s.palm.z, s.palmTurn.w, s.palmTurn.x, s.palmTurn.y,
            s.palmTurn.z, s.thumbChoice, s.thumbTurn.w, s.thumbTurn.x, s.thumbTurn.y, s.thumbTurn.z);
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            const grasp::FingerStop& st = s.finger[f];
            t += va(" f%d %.9g %.9g %.9g %d%d%d%d", f, st.stop[0], st.stop[1], st.stop[2], st.met ? 1 : 0, st.startsInside ? 1 : 0,
                st.fromClosed ? 1 : 0, st.leastInside ? 1 : 0);
        }
        t += va(" probes %d places %d\n", s.probes, s.places);
        return t;
    };
    for(int hand = 0; hand < 2; hand++)
    {
        const RigHand& rh = rigHands[hand];
        if(!rh.drawn || !rh.held.ent || !rh.held.ent->model)
        {
            continue;
        }
        const int frame = rh.held.frame >= 0 ? rh.held.frame : rh.held.ent->frame;
        const grasp::Shape* shape = heldShape(rh.held, frame);
        if(!shape)
        {
            continue;
        }
        const char* model = COM_SkipPath(rh.held.ent->model->name);
        // Where it rests in the hand (its place from the settings: the same every run), else where it is now.
        const glm::mat4 inRig = rh.held.canonical ? rh.held.canonicalInRig
                                                  : glm::inverse(rh.solveRig) * heldToWorld(rh.held);
        const grasp::Settings base = graspSettings(rh.held, rh.rigUnit);

        // The other hand as drawn, for the cup (as updateGrasp makes it).
        const RigHand& other = rigHands[1 - hand];
        grasp::Shape otherShape;
        if(other.drawn)
        {
            const glm::mat4 otherInRig = glm::inverse(rh.solveRig) * other.rigToWorld;
            za::Vector<glm::vec3> posed;
            handrig::vertices(other.posed, posed);
            za::Vector<grasp::Triangle> tris;
            const auto at = [&](const glm::vec3& p) { return glm::vec3{otherInRig * glm::vec4{drawnInRig(other, p), 1.f}}; };
            for(const auto& tri : handrig::rig().triangles)
            {
                tris.pushBack({{at(posed[tri[0]]), at(posed[tri[1]]), at(posed[tri[2]])}});
            }
            grasp::makeShape(tris, otherShape);
        }

        const glm::mat4 places[7] = {glm::mat4{1.f}, glm::translate(glm::mat4{1.f}, glm::vec3{0.3f, 0.f, 0.f}),
            glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.3f, 0.f}), glm::translate(glm::mat4{1.f}, glm::vec3{0.f, 0.f, 0.3f}),
            glm::rotate(glm::mat4{1.f}, glm::radians(4.f), glm::vec3{1.f, 0.f, 0.f}),
            glm::rotate(glm::mat4{1.f}, glm::radians(4.f), glm::vec3{0.f, 0.f, 1.f}),
            glm::rotate(glm::translate(glm::mat4{1.f}, glm::vec3{-0.3f, -0.2f, 0.2f}), glm::radians(-4.f), glm::vec3{0.f, 1.f, 0.f})};
        int solves = 0, identical = 0;
        float maxDiff = 0.f;
        for(int p = 0; p < 7; p++)
        {
            const glm::mat4 m = places[p] * inRig;
            grasp::Solution first;
            for(int v = 0; v < 8; v++)
            {
                grasp::Settings s = base;
                const grasp::Solution* previous = nullptr;
                grasp::Shape* extra = nullptr;
                switch(v)
                {
                case 1: previous = &first; break;
                case 2:
                    s.searchPlace = true;
                    s.palmTurnLimit = 0.f;
                    s.palmLimit = za::fmax(base.palmLimit, 1.f) * 1.5f;
                    break;
                case 3:
                    s.fixedPalm = true;
                    s.palmMove = first.palm;
                    s.palmTurnMove = first.palmTurn;
                    break;
                case 4: s.thumbTop = !base.thumbTop; break;
                case 5:
                    s.thenar = !base.thenar;
                    s.overlap = base.overlap * 1.5f + 0.05f;
                    break;
                case 6:
                    s.searchPlace = false;
                    s.fixedPalm = false;
                    s.palmTurnLimit = 20.f;
                    s.palmLimit = za::fmax(base.palmLimit, 1.f);
                    break;
                case 7:
                    if(!other.drawn)
                    {
                        continue;
                    }
                    extra = &otherShape;
                    break;
                default: break;
                }
                grasp::forgetSolves(); // a real solve, not a remembered one
                grasp::Solution out;
                grasp::solve(rh.pose, *shape, m, s, previous, out, extra, glm::mat4{1.f}, extra ? 0.1f : 0.f);
                if(v == 0)
                {
                    first = out;
                }
                const za::String printed = text(hand, model, v, p, out);
                Con_Printf("%s", printed.cStr());
                solves++;
                // Solved again on this thread alone (vr_jobs_parallel 0): the same, to the last bit?
                jobs::setParallel(!jobs::parallel());
                grasp::forgetSolves();
                grasp::Solution other;
                grasp::solve(rh.pose, *shape, m, s, previous, other, extra, glm::mat4{1.f}, extra ? 0.1f : 0.f);
                jobs::setParallel(!jobs::parallel());
                identical += text(hand, model, v, p, other) == printed ? 1 : 0;
                for(int f = 0; f < handrig::FingerCount; f++)
                {
                    for(int j = 0; j < handrig::jointsPerFinger; j++)
                    {
                        maxDiff = za::fmax(maxDiff, za::fabs(other.finger[f].stop[j] - out.finger[f].stop[j]));
                    }
                }
            }
        }
        // The afresh solve as held timed on the pool's threads and on this one alone.
        const bool was = jobs::parallel();
        double median[2]{}, least[2]{};
        const jobs::Stats before = jobs::stats();
        jobs::Stats after;
        for(int mode = 0; mode < 2; mode++)
        {
            if(mode == 1)
            {
                after = jobs::stats();
            }
            jobs::setParallel(mode == 0);
            za::Vector<double> ms;
            for(int i = 0; i < runs; i++)
            {
                grasp::forgetSolves();
                grasp::Solution out;
                const auto t0 = za::Clock::nowNanoseconds();
                grasp::solve(rh.pose, *shape, inRig, base, nullptr, out);
                ms.pushBack(za::nanosecondsToMilliseconds(za::Clock::nowNanoseconds() - t0));
            }
            za::quickSort(ms.begin(), ms.end());
            median[mode] = ms[ms.size() / 2];
            least[mode] = ms.front();
        }
        jobs::setParallel(was);
        Con_Printf("gsweep time h%d %s: %d solves (%d the same on one thread, largest curl difference %g); afresh as held, "
                   "%d times: pool (%d workers; the caller ran %zu chunks, helpers %zu) min %.3f median %.3f ms, one thread min %.3f median %.3f ms\n",
            hand, model, solves, identical, maxDiff, runs, jobs::workers(), after.chunksCaller - before.chunksCaller,
            after.chunksHelpers - before.chunksHelpers, least[0], median[0],
            least[1], median[1]);
    }
}

void graspDump_f()
{
    const int hand = Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "off") ? HAND_OFF : HAND_MAIN;
    const RigHand& rh = rigHands[hand];
    if(!rh.drawn)
    {
        Con_Printf("vr_grasp_dump: the %s hand's jointed hand is not drawn\n", hand == HAND_MAIN ? "main" : "off");
        return;
    }
    char path[MAX_OSPATH];
    q_snprintf(path, sizeof(path), "%s/%s", com_gamedir, Cmd_Argc() >= 3 ? Cmd_Argv(2) : "grasp_dump.obj");
    FILE* f = fopen(path, "w");
    if(!f)
    {
        Con_Printf("vr_grasp_dump: can't write %s\n", path);
        return;
    }
    // The hand's mesh (handrig::rig(): the file's, split at the skin's seams), posed.
    int base = 1;
    za::Vector<glm::vec3> posed;
    const auto mesh = [&](const RigHand& which, const glm::mat4& toThis, const char* name) {
        handrig::vertices(which.posed, posed);
        fprintf(f, "o %s\n", name);
        for(const glm::vec3& v : posed)
        {
            const glm::vec3 p{toThis * glm::vec4{drawnInRig(which, v), 1.f}};
            fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
        }
        for(const auto& tri : handrig::rig().triangles)
        {
            fprintf(f, "f %d %d %d\n", base + tri[0], base + tri[1], base + tri[2]);
        }
        base += static_cast<int>(posed.size());
    };
    mesh(rh, glm::mat4{1.f}, "hand");

    // The other hand, when this one cups it.
    const RigHand& otherHand = rigHands[1 - hand];
    if(rh.held.cup && otherHand.drawn)
    {
        mesh(otherHand, glm::inverse(rh.rigToWorld) * otherHand.rigToWorld, "other");
    }

    // The spheres the solver tests the hand as, each an octahedron.
    za::Vector<glm::vec4> spheres;
    grasp::posedSpheres(rh.pose, spheres);
    fprintf(f, "o spheres\n");
    for(const glm::vec4& s : spheres)
    {
        const glm::vec3 c{s};
        for(const glm::vec3 d : {glm::vec3{1, 0, 0}, glm::vec3{-1, 0, 0}, glm::vec3{0, 1, 0}, glm::vec3{0, -1, 0}, glm::vec3{0, 0, 1}, glm::vec3{0, 0, -1}})
        {
            const glm::vec3 p = drawnInRig(rh, c + d * s.w);
            fprintf(f, "v %f %f %f\n", p.x, p.y, p.z);
        }
        constexpr int faces[8][3] = {{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4}, {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}};
        for(const auto& t : faces)
        {
            fprintf(f, "f %d %d %d\n", base + t[0], base + t[1], base + t[2]);
        }
        base += 6;
    }
    za::Vector<grasp::Triangle> tris;
    if(rh.held.shape) // (a ragdoll's limb: its own shape)
    {
        const glm::mat4 m = rh.held.toWorld;
        for(const grasp::Triangle& t : rh.held.shape->tris)
        {
            tris.pushBack({{glm::vec3{m * glm::vec4{t.p[0], 1.f}}, glm::vec3{m * glm::vec4{t.p[1], 1.f}}, glm::vec3{m * glm::vec4{t.p[2], 1.f}}}});
        }
    }
    if(rh.held.ent && rh.held.ent->model && (!tris.empty() || grasp::worldTriangles(*rh.held.ent, rh.held.mirrored, rh.held.frame, tris)))
    {
        const glm::mat4 toRig = glm::inverse(rh.rigToWorld);
        fprintf(f, "o held\n");
        for(const grasp::Triangle& t : tris)
        {
            for(const glm::vec3& p : t.p)
            {
                const glm::vec3 q{toRig * glm::vec4{p, 1.f}};
                fprintf(f, "v %f %f %f\n", q.x, q.y, q.z);
            }
        }
        for(size_t i = 0; i < tris.size(); i++)
        {
            fprintf(f, "f %d %d %d\n", base + static_cast<int>(3 * i), base + static_cast<int>(3 * i + 1), base + static_cast<int>(3 * i + 2));
        }
    }
    fclose(f);
    Con_Printf("vr_grasp_dump: %s\n", path);
}

bool hotspotAt(int hand, const glm::vec3& p, glm::vec3& out)
{
    const ViewEntity& ve = entities.weapon[hand];
    const int slot = weapons::slotForModel(ve.ent.model);
    if(!ve.ent.model || slot < 0 || slot == weapons::fistSlot())
    {
        return false;
    }
    out = glm::vec3{glm::inverse(hotspotFrame(ve.ent, ve.mirrored)) * glm::vec4{p, 1.f}};
    return true;
}

const DrawnWeapon& drawnWeapon(int hand)
{
    return drawnWeapons[hand == 0 ? 0 : 1];
}

bool drawnIndexTip(int hand, glm::vec3& local)
{
    if(hand != 0 && hand != 1)
    {
        return false;
    }
    const IndexTip& t = indexTips[hand];
    if(t.frame < 0 || host_framecount - t.frame > 2 || !rigHands[hand].drawn)
    {
        return false;
    }
    local = t.local;
    return true;
}

const char* handPartName(HandPart part)
{
    constexpr const char* names[] = {"hand", "palm", "back of the hand", "knuckles", "fingers", "fingertips", "thumb"};
    const int i = static_cast<int>(part);
    return i >= 0 && i < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[i] : "hand";
}

namespace
{
// A rest vertex's part of the jointed hand: by the joint it follows most (the thumb's, a finger's knuckle ring, its last
// segment: the fingertips, the rest of a finger) or, on the palm, by its side (the palm's spheres' side: the palm; the
// other: the back of the hand). `palmMid`, `palmWay`: the palm's vertices' middle and the way to its palm side.
[[nodiscard]] int mainJoint(const handrig::Vertex& v)
{
    int best = 0;
    for(int i = 1; i < v.count && i < 4; i++)
    {
        best = v.weight[i] > v.weight[best] ? i : best;
    }
    return v.joint[best];
}

[[nodiscard]] HandPart restPart(const handrig::Vertex& v, const glm::vec3& palmMid, const glm::vec3& palmWay)
{
    const int j = mainJoint(v);
    if(j > 0 && j < handrig::data::numJoints)
    {
        const handrig::data::Joint& joint = handrig::data::joints[j];
        if(joint.finger == 0)
        {
            return HandPart::Thumb;
        }
        if(joint.kind == handrig::data::PartJoint && joint.index == 0)
        {
            return HandPart::Knuckles;
        }
        return joint.kind == handrig::data::SegmentJoint && joint.index == 3 ? HandPart::Fingertips : HandPart::Fingers;
    }
    return glm::dot(v.pos - palmMid, palmWay) >= 0.f ? HandPart::Palm : HandPart::Back;
}

// drawnHandSurface's parts of the jointed hand's vertices (restPart: the rig's at rest), for the rig's generation.
struct HandPartsCache
{
    bool made = false;
    unsigned generation = 0;
    za::Vector<HandPart> parts;
    auto members() { return mem::list(made, generation, parts); }
};
mem::Cache<HandPartsCache> handPartsCache{"view hand parts", mem::GameDirChange | mem::ModelReload};

} // namespace

bool drawnHandSurface(int hand, za::Vector<glm::vec4>& out, za::Vector<HandPart>* parts)
{
    out.clear();
    if(parts)
    {
        parts->clear();
    }
    if(hand != 0 && hand != 1)
    {
        return false;
    }
    const RigHand& rh = rigHands[hand];
    if(rh.drawn)
    {
        // The mesh's vertices as drawn (as the index fingertip: view::drawnIndexTip).
        za::Vector<glm::vec3>& posed = scratch.surfaceVerts;
        handrig::vertices(rh.posed, posed);
        for(const glm::vec3& p : posed)
        {
            out.pushBack(glm::vec4{glm::vec3{rh.rigToWorld * glm::vec4{drawnInRig(rh, p), 1.f}}, 0.f});
        }
        if(parts)
        {
            // Each vertex's part is the rig's at rest: found once per rig (its generation), not every frame.
            HandPartsCache& c = handPartsCache;
            if(!c.made || c.generation != handrig::generation() || c.parts.size() != posed.size())
            {
                c.made = true;
                c.generation = handrig::generation();
                c.parts.clear();
                const handrig::Rig& rig = handrig::rig();
                glm::vec3 palmMid{0.f}, sphereMid{0.f};
                int n = 0;
                for(const handrig::Vertex& v : rig.vertices)
                {
                    if(mainJoint(v) == 0)
                    {
                        palmMid += v.pos;
                        n++;
                    }
                }
                palmMid /= static_cast<float>(za::max(n, 1));
                for(const handrig::Sphere& sp : rig.palmSpheres)
                {
                    sphereMid += sp.c;
                }
                sphereMid /= static_cast<float>(za::max(static_cast<int>(rig.palmSpheres.size()), 1));
                const glm::vec3 palmWay = sphereMid - palmMid;
                for(size_t i = 0; i < posed.size(); i++)
                {
                    c.parts.pushBack(i < rig.vertices.size() ? restPart(rig.vertices[i], palmMid, palmWay) : HandPart::Hand);
                }
            }
            for(const HandPart p : c.parts)
            {
                parts->pushBack(p);
            }
        }
        return !out.empty();
    }
    const view::ViewEntity& he = entities.hand[hand][FingerBase];
    const hands::State& s = hands::current();
    if(!he.visible || !he.ent.model || !s.valid)
    {
        return false;
    }
    // The old models: three spheres along the hand (as vr_body_collide's: selfCollideDrawn).
    const avatar::HandPose hp = drawnHand(s, hand);
    const float k = units::metresToUnits() * units::bodyScale();
    for(const float along : {0.035f, 0.065f, 0.095f})
    {
        out.pushBack(glm::vec4{hp.wrist + hp.forward * (along * k), 0.032f * k});
        if(parts)
        {
            parts->pushBack(HandPart::Hand);
        }
    }
    return true;
}

glm::vec3 handPress(int hand)
{
    return hand == 0 || hand == 1 ? weaponPressed[hand] : glm::vec3{0.f};
}

glm::vec3 heldOutOf(int hand, int ent)
{
    if((hand != 0 && hand != 1) || ent <= 0 || ent >= cl_max_edicts || !vr_hand_collide_props_keep_out.value ||
        held::heldAlone(1 - hand) != ent)
    {
        return glm::vec3{0.f};
    }
    const RigHand& rh = rigHands[hand];
    if(rh.propAgainst != &cl_entities[ent] || !rh.weaponTouching)
    {
        return glm::vec3{0.f};
    }
    return rh.pushed - weaponPressed[1 - hand]; // (the hand off the prop as drawn: each moved apart)
}

WeaponHotspot weaponHotspot(int hand, int index)
{
    if(hand < 0 || hand > 1 || index < 0 || index >= weapons::maxHotspots || !entities.weapon[hand].visible)
    {
        return {};
    }
    const WorldHotspot& w = worldHotspots[hand][index];
    const glm::vec3 pos = w.type == weapons::HotspotType::Blade ? glm::mix(w.pos, w.end, 0.5f) : w.pos;
    return {static_cast<int>(w.type), pos, w.bias, w.share};
}

// vr_admelee_spine: an AD sword in a hand (vr_admelee.cpp: a curved one's points, QC VR_Melee_At): its blade's spine as
// drawn, in the world, at shares 0.27, 0.57, 0.86 and 1 of the line from the hand to its tip ("admelee spine:" lines;
// QC's own points for the same shares: vr_ad_test 126).
void adMeleeSpine_f()
{
    const hands::State& s = hands::current();
    for(int hand = 0; hand < 2; hand++)
    {
        WeaponFrame wf;
        if(!s.valid || !weaponFrame(s, hand, wf) || !wf.model)
        {
            continue;
        }
        for(const float f : {0.27f, 0.57f, 0.86f, 1.f})
        {
            glm::vec3 p, l;
            if(!admelee::spineAt(wf.model->name, f, p, l))
            {
                Con_Printf("admelee spine: hand %d: %s is not an AD sword\n", hand, wf.model->name);
                break;
            }
            const glm::vec3 w{wf.modelToWorld * glm::vec4{p, 1.f}};
            const glm::vec3 off = w - glm::vec3{wf.modelToWorld * glm::vec4{l, 1.f}};
            Con_Printf("admelee spine: hand %d %.2f drawn at %.2f %.2f %.2f, off its line %.2f %.2f %.2f\n", hand, f, w.x, w.y,
                w.z, off.x, off.y, off.z);
        }
    }
}

void hotspotFit_f()
{
    const hands::State& s = hands::current();
    WeaponFrame wf;
    if(!s.valid || !weaponFrame(s, HAND_MAIN, wf))
    {
        Con_Printf("vr_hotspot_fit: hold a weapon in the main hand\n");
        return;
    }
    const glm::mat4 inv = glm::inverse(wf.modelToWorld);
    const glm::vec3 hand{inv * glm::vec4{s.pos[HAND_MAIN], 1.f}};
    const glm::vec3 fist{inv * glm::vec4{wf.fist, 1.f}};
    const float m = units::metresToUnits();
    Con_Printf("vr_hotspot_fit: %s: the hand at %.2f %.2f %.2f, the drawn fist at %.2f %.2f %.2f (model)\n", wf.model->name,
        hand.x, hand.y, hand.z, fist.x, fist.y, fist.z);
    if(s.muzzleValid[HAND_MAIN])
    {
        const glm::vec3 tip{inv * glm::vec4{s.muzzle[HAND_MAIN], 1.f}};
        Con_Printf("vr_hotspot_fit: the tip at %.2f %.2f %.2f (model), %.1f cm from the hand, %.1f from the fist\n", tip.x,
            tip.y, tip.z, glm::distance(s.muzzle[HAND_MAIN], s.pos[HAND_MAIN]) / m * 100.f,
            glm::distance(s.muzzle[HAND_MAIN], wf.fist) / m * 100.f);
    }
    glm::vec3 fwd, right, up;
    // The tracking space's axes (the play space's yaw, not the view's): the mock's.
    hands::angleVectors(glm::vec3{0.f, hands::playSpaceYaw(), 0.f}, fwd, right, up);
    const auto report = [&](int i, const char* what, const glm::vec3& at) {
        const glm::vec3 d = (at - s.pos[HAND_OFF]) / m;
        const glm::vec3 mp{inv * glm::vec4{at, 1.f}};
        Con_Printf("vr_hotspot_fit: hotspot %d %s at %.2f %.2f %.2f (model); from the off hand: right %.3f up %.3f "
                   "back %.3f m\n",
            i + 1, what, mp.x, mp.y, mp.z, glm::dot(d, right), glm::dot(d, up), -glm::dot(d, fwd));
    };
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        const WorldHotspot& w = worldHotspots[HAND_MAIN][i];
        if(w.type == weapons::HotspotType::Blade)
        {
            report(i, "(blade) its near end", w.pos);
            report(i, "(blade) its middle", glm::mix(w.pos, w.end, 0.5f));
            report(i, "(blade) its far end", w.end);
        }
        else if(w.type != weapons::HotspotType::None)
        {
            report(i, w.type == weapons::HotspotType::Cup ? "(cup)" : "(grip)", w.pos);
        }
    }
}

void hotspotHere_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_weapon_hotspot_here <1..4> [<type 0..3>] [main|off]\n");
        return;
    }
    const int index = CLAMP(1, Q_atoi(Cmd_Argv(1)), weapons::maxHotspots) - 1;
    const int type = Cmd_Argc() >= 3 ? CLAMP(0, Q_atoi(Cmd_Argv(2)), 3) : 1;
    const int hand = Cmd_Argc() >= 4 && !q_strcasecmp(Cmd_Argv(3), "off") ? HAND_OFF : HAND_MAIN;
    const int slot = weapons::slotForModel(entities.weapon[hand].ent.model);
    glm::vec3 p;
    // A cup is where the other hand's palm is (round 21, third pass).
    const glm::vec3 at = type == static_cast<int>(weapons::HotspotType::Cup) ? hands::palmPoint(hands::current(), 1 - hand)
                                                                             : hands::current().pos[1 - hand];
    if(slot < 0 || !hotspotAt(hand, at, p))
    {
        Con_Printf("vr_weapon_hotspot_here: no weapon in the %s hand\n", hand == HAND_MAIN ? "main" : "off");
        return;
    }
    weapons::Hotspot h = weapons::hotspot(slot, index);
    h.type = static_cast<weapons::HotspotType>(type);
    h.pos = type ? p : glm::vec3{0.f};
    weapons::setHotspot(slot, index, h);
    Con_Printf("vr_weapon_hotspot_here: %s hotspot %d: type %d at %.2f %.2f %.2f\n", entities.weapon[hand].ent.model->name, index + 1,
        type, p.x, p.y, p.z);
}

// vr_hotspots_check: for every slot, the weapon drawn in either hand at a few poses: where its old two-handed grip
// (the retired foregrip keys, mirrored as the old code did) and its grip hotspot are (they should match), and how far
// the drawn hand moved (it was at the weapon's hand anchor, round 20; now where the controller is: the weapon's
// origin).
void hotspotsCheck_f()
{
    float worstGrip = 0.f, worstHand = 0.f, worstMuzzle = 0.f;
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        const cvar_t* id = weapons::cvar(slot, Key::ID);
        qmodel_t* model = id && id->string[0] ? Mod_ForName(id->string, false) : nullptr;
        if(!model || model->type != mod_alias || slot == weapons::fistSlot())
        {
            continue;
        }
        float grip = -1.f, hand = 0.f, muzzle = 0.f;
        for(int pose = 0; pose < 6; pose++)
        {
            const bool mirrored = pose & 1;
            ViewEntity ve;
            ve.ent.model = model;
            ve.ent.scale = ENTSCALE_DEFAULT;
            ve.mirrored = mirrored;
            const float a = static_cast<float>(pose);
            ve.ent.origin[0] = 100.f + 7.f * a;
            ve.ent.origin[1] = -40.f + 3.f * a;
            ve.ent.origin[2] = 20.f - 5.f * a;
            ve.ent.angles[0] = -30.f + 17.f * a;
            ve.ent.angles[1] = 40.f * a;
            ve.ent.angles[2] = 25.f - 11.f * a;
            const glm::vec3 origin{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
            // The old drawn hand: the hand anchor and offset.
            const glm::vec3 oldHand = anchorPosition(ve, weapons::anchor(slot, Key::HandAnchor),
                weapons::vec(slot, Key::HandOffsetX, Key::HandOffsetY, Key::HandOffsetZ));
            hand = za::max(hand, glm::distance(oldHand, origin));
            // The muzzle: round 20 moved the weapon by its GunOffset (world axes, y mirrored); retired, now not.
            const char* muzzleAnchor = weapons::anchor(slot, Key::MuzzleAnchor);
            const glm::vec3 muzzleOffset = weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ);
            glm::vec3 gunOffset = weapons::vec(slot, Key::GunOffsetX, Key::GunOffsetY, Key::GunOffsetZ) * weapons::offsetScale();
            if(mirrored)
            {
                gunOffset.y = -gunOffset.y;
            }
            ViewEntity shifted = ve;
            for(int i = 0; i < 3; i++)
            {
                shifted.ent.origin[i] += gunOffset[i];
            }
            muzzle = za::max(muzzle,
                glm::distance(anchorPosition(shifted, muzzleAnchor, muzzleOffset), anchorPosition(ve, muzzleAnchor, muzzleOffset)));
            if(weapons::value(slot, Key::TwoHDisplayMode) != 1.f)
            {
                continue;
            }
            ViewEntity helped = ve;
            helped.mirrored = !mirrored;
            const glm::vec3 old = anchorPosition(helped, weapons::anchor(slot, Key::TwoHHandAnchor),
                weapons::vec(slot, Key::TwoHFixedOffsetX, Key::TwoHFixedOffsetY, Key::TwoHFixedOffsetZ));
            const weapons::Hotspot h = weapons::hotspot(slot, 0);
            const glm::vec3 now{hotspotFrame(ve.ent, mirrored) * glm::vec4{h.pos, 1.f}};
            grip = za::max(grip, glm::distance(old, now));
        }
        worstGrip = za::max(worstGrip, grip);
        worstHand = za::max(worstHand, hand);
        worstMuzzle = za::max(worstMuzzle, muzzle);
        Con_Printf("slot %2d %-22s foregrip: %s   muzzle %.4f   the hand moved %.2f units (%.2f cm)\n", slot, id->string,
            grip < 0.f ? "none" : va("old and hotspot %.4f units apart", grip), muzzle, hand,
            hand * 100.f / units::metresToUnits());
    }
    Con_Printf("vr_hotspots_check: the most a foregrip moved %.4f units, a muzzle %.4f, the hand %.2f units\n", worstGrip,
        worstMuzzle, worstHand);
}

// vr_anchor_where: each hand's weapon as drawn this frame, its anchors (the muzzle, the ammo button and screen with their
// offsets) and its magazine's and well's places (moved by their mount's anchor), in the world and in the gun's own frame
// (its drawn matrix undone: what the anchors alone decide, the gun's motion taken out). For comparing builds and checking
// a model edit in the game (Debug > Reports > Weapon Anchors).
void anchorWhere_f()
{
    for(int hand = 0; hand < 2; hand++)
    {
        const ViewEntity& ve = entities.weapon[hand];
        const int slot = ve.ent.model ? weapons::slotForModel(ve.ent.model) : -1;
        if(!ve.visible || slot < 0 || ve.ent.model->type != mod_alias)
        {
            Con_Printf("anchors hand %d: no weapon\n", hand);
            continue;
        }
        float m[16];
        render::anchorMatrix(ve, glm::vec3{0.f}, m);
        const glm::mat4 toLocal = glm::inverse(glm::mat4{glm::vec4{m[0], m[1], m[2], m[3]}, glm::vec4{m[4], m[5], m[6], m[7]},
            glm::vec4{m[8], m[9], m[10], m[11]}, glm::vec4{m[12], m[13], m[14], m[15]}});
        const auto print = [&](const char* what, const glm::vec3& w) {
            const glm::vec3 l{toLocal * glm::vec4{w, 1.f}};
            Con_Printf("anchors hand %d %s frame %d %s %.4f %.4f %.4f local %.4f %.4f %.4f\n", hand, ve.ent.model->name,
                ve.ent.frame, what, w.x, w.y, w.z, l.x, l.y, l.z);
        };
        print("muzzle", anchorPosition(ve, weapons::anchor(slot, Key::MuzzleAnchor),
            weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ)));
        print("button", anchorPosition(ve, weapons::anchor(slot, Key::WpnButtonAnchor),
            weapons::vec(slot, Key::WpnButtonX, Key::WpnButtonY, Key::WpnButtonZ)));
        print("text", anchorPosition(ve, weapons::anchor(slot, Key::WpnTextAnchor),
            weapons::vec(slot, Key::WpnTextX, Key::WpnTextY, Key::WpnTextZ)));
        for(const ViewEntity* part : {&entities.mag[hand], &entities.well[hand]})
        {
            if(part->visible && part->ent.model)
            {
                print(part->ent.model->name, glm::vec3{part->ent.origin[0], part->ent.origin[1], part->ent.origin[2]});
            }
        }
    }
}
// vr_dumpview: lists the VR view entities.
void holsterPoseCheck(weapons::HolsterKind kind, const qmodel_t* model, const glm::vec3& inPos, const glm::vec3& inFwd,
    const glm::vec3& inUp)
{
    if(posing::active())
    {
        Con_Printf("vr_pose_check: leave the posing mode first\n");
        return;
    }
    static constexpr const char* names[HolsterCount] = {"left hip", "right hip", "left upper", "right upper", "left shoulder",
        "right shoulder"};
    const int slot = weapons::slotForModel(model);
    const auto turnOf = [](const glm::vec3& f, const glm::vec3& u) {
        const glm::vec3 x = glm::normalize(f);
        const glm::vec3 z = glm::normalize(u - x * glm::dot(u, x));
        return glm::mat3{x, glm::cross(z, x), z};
    };
    int found = 0;
    for(int h = 0; h < HolsterCount; h++)
    {
        const HolsterSeen& seen = holsterSeen[h];
        if(!seen.valid || seen.floating || seen.kind != kind || seen.slot != slot || slot < 0)
        {
            continue;
        }
        const glm::vec3 pos = inHolsterFrame(seen.frame, seen.weaponPos - seen.pivot);
        const glm::mat3 now = turnOf(inHolsterFrame(seen.frame, seen.weaponTurn[0]), inHolsterFrame(seen.frame, seen.weaponTurn[2]));
        const glm::quat q = glm::quat_cast(glm::transpose(turnOf(inFwd, inUp)) * now);
        const float angle = glm::degrees(2.f * za::atan2(glm::length(glm::vec3{q.x, q.y, q.z}), za::fabs(q.w)));
        Con_Printf("vr_pose_check: %s holster: the weapon %.4f units and %.4f degrees from the pose set (at %.2f %.2f %.2f "
                   "in it; set at %.2f %.2f %.2f)\n",
            names[h], static_cast<double>(glm::distance(pos, inPos)), static_cast<double>(angle), static_cast<double>(pos.x),
            static_cast<double>(pos.y), static_cast<double>(pos.z), static_cast<double>(inPos.x), static_cast<double>(inPos.y),
            static_cast<double>(inPos.z));
        found++;
    }
    if(!found)
    {
        Con_Printf("vr_pose_check: no holster of the kind posed holds %s now: holster it there\n", model ? model->name : "it");
    }
}

void posingCheck(bool weaponTarget, int weaponHand, const glm::mat4& rigInWeapon, const glm::vec3* palmInWeapon,
    const glm::mat4& rigWorld)
{
    const int hand = weaponTarget ? weaponHand : 1 - weaponHand;
    const ViewEntity& weapon = entities.weapon[weaponHand];
    const RigHand& rh = rigHands[hand];
    if(!weapon.visible || !rh.drawn || posing::active())
    {
        Con_Printf("vr_pose_check: hold the weapon in the %s hand (out of the posing mode)\n", weaponHand == HAND_MAIN ? "main" : "off");
        return;
    }
    const glm::mat4 hs = hotspotFrame(weapon.ent, weapon.mirrored);
    const glm::mat4 predicted = hs * rigInWeapon;
    const glm::mat4& actual = rh.rigToWorld;
    const auto turnOf = [](const glm::mat4& m) {
        return glm::mat3{glm::normalize(glm::vec3{m[0]}), glm::normalize(glm::vec3{m[1]}), glm::normalize(glm::vec3{m[2]})};
    };
    const glm::quat q = glm::quat_cast(glm::transpose(turnOf(predicted)) * turnOf(actual));
    const float angle = glm::degrees(2.f * za::atan2(glm::length(glm::vec3{q.x, q.y, q.z}), za::fabs(q.w)));
    const float moved = glm::distance(glm::vec3{predicted[3]}, glm::vec3{actual[3]});
    const glm::vec3 palm{actual * glm::vec4{drawnInRig(rh, grasp::palmCentre()), 1.f}};
    const float palmMoved = palmInWeapon ? glm::distance(glm::vec3{hs * glm::vec4{*palmInWeapon, 1.f}}, palm) : -1.f;
    // The weapon where the pose puts it from the hand: its muzzle (its far end) against where it is.
    const hands::State& s = hands::current();
    float muzzleMoved = -1.f;
    if(s.muzzleValid[weaponHand])
    {
        const glm::mat4 weaponFromHand = actual * glm::inverse(rigInWeapon);
        const glm::vec3 muzzleInWeapon{glm::inverse(hs) * glm::vec4{s.muzzle[weaponHand], 1.f}};
        muzzleMoved = glm::distance(glm::vec3{weaponFromHand * glm::vec4{muzzleInWeapon, 1.f}}, s.muzzle[weaponHand]);
    }
    Con_Printf("pose check (%s, %s hand%s): hand %.4f units %.4f deg from the pose, muzzle %.4f units, drawn palm %s\n",
        weaponTarget ? "the weapon" : "a hotspot", hand == HAND_MAIN ? "main" : "off",
        weaponTarget || twohand::helping(hand) ? "" : ", NOT holding a hotspot", moved, angle, muzzleMoved,
        palmInWeapon ? va("%.4f units", palmMoved) : "not compared (the hand wasn't seen solved after the set)");
    // Where the weapon's hand is drawn in the world against where it was while posing (the same only with the controller,
    // and the player, where they were: the weapon's angle offsets are Euler angles added to the hand's, so the hand and
    // weapon together sit a little differently on a tilted controller).
    if(!weaponTarget)
    {
        return;
    }
    const glm::quat w = glm::quat_cast(glm::transpose(turnOf(rigWorld)) * turnOf(actual));
    Con_Printf("  in the world: %.4f units %.4f deg from where it was drawn while posing\n",
        glm::distance(glm::vec3{rigWorld[3]}, glm::vec3{actual[3]}),
        glm::degrees(2.f * za::atan2(glm::length(glm::vec3{w.x, w.y, w.z}), za::fabs(w.w))));
}

void dumpView_f()
{
    const hands::State& s = hands::current();
    Con_Printf("hands valid %d  player (%.1f %.1f %.1f)  main (%.1f %.1f %.1f)  off (%.1f %.1f %.1f)%s\n", s.valid,
        s.playerOrigin.x, s.playerOrigin.y, s.playerOrigin.z, s.pos[1].x, s.pos[1].y, s.pos[1].z, s.pos[0].x, s.pos[0].y,
        s.pos[0].z, twohand::helping(HAND_OFF) ? ", the off hand helping" : "");

    for(int h = 0; h < 2; h++)
    {
        Con_Printf("%s hand at (%.4f %.4f %.4f) angles (%.4f %.4f %.4f), controller at (%.4f %.4f %.4f) aim (%.4f %.4f %.4f)\n",
            h == HAND_MAIN ? "main" : "off", s.pos[h].x, s.pos[h].y, s.pos[h].z, s.rot[h].x, s.rot[h].y, s.rot[h].z,
            s.controllerPos[h].x, s.controllerPos[h].y, s.controllerPos[h].z, s.aimRot[h].x, s.aimRot[h].y, s.aimRot[h].z);
        Con_Printf("%s grip (Show Controller) at (%.4f %.4f %.4f) angles (%.4f %.4f %.4f), %.2f cm from the controller\n",
            h == HAND_MAIN ? "main" : "off", s.gripPos[h].x, s.gripPos[h].y, s.gripPos[h].z, s.gripRot[h].x, s.gripRot[h].y,
            s.gripRot[h].z, glm::distance(s.gripPos[h], s.controllerPos[h]) / (0.01f * units::metresToUnits()));
        if(rigHands[h].drawn)
        {
            const glm::vec3 palm{rigHands[h].rigToWorld * glm::vec4{drawnInRig(rigHands[h], grasp::palmCentre()), 1.f}};
            Con_Printf("%s drawn palm at (%.4f %.4f %.4f)\n", h == HAND_MAIN ? "main" : "off", palm.x, palm.y, palm.z);
        }
        // The grab test's fist, and the open hand, in the hand's frame (cm from its point: forward, left, up): how far
        // each reaches along each axis.
        za::Vector<glm::vec4> fist, open;
        if(emptyHandSpheres(s, h, true, fist) && emptyHandSpheres(s, h, false, open))
        {
            const float cm = 100.f / units::metresToUnits();
            for(const auto& [name, spheres] : {za::Pair{"fist", &fist}, za::Pair{"open hand", &open}})
            {
                glm::vec3 lo{1e9f}, hi{-1e9f};
                for(const glm::vec4& sp : *spheres)
                {
                    lo = glm::min(lo, glm::vec3{sp} - sp.w);
                    hi = glm::max(hi, glm::vec3{sp} + sp.w);
                }
                Con_Printf("%s %s: %d spheres, forward %.1f..%.1f cm, left %.1f..%.1f, up %.1f..%.1f of the hand's point\n",
                    h == HAND_MAIN ? "main" : "off", name, static_cast<int>(spheres->size()), lo.x * cm, hi.x * cm, lo.y * cm,
                    hi.y * cm, lo.z * cm, hi.z * cm);
            }
            const glm::vec3 palm{glm::transpose(held::axesFromAngles(&s.rot[h][0], true)) *
                                 (glm::vec3{rigPlacement(h, s.pos[h], basisAngles(anglesBasis(s.rot[h]) *
                                     anglesBasis(weaponAngleOffsets(weapons::fistSlot(), h == HAND_OFF))), h == HAND_OFF, nullptr) *
                                     glm::vec4{grasp::palmCentre(), 1.f}} - s.pos[h])};
            Con_Printf("%s palm's middle: forward %.1f cm, left %.1f, up %.1f of the hand's point\n", h == HAND_MAIN ? "main" : "off",
                palm.x * cm, palm.y * cm, palm.z * cm);
        }
        if(s.grip2HValid[h])
        {
            Con_Printf("%s weapon foregrip (%.1f %.1f %.1f)\n", h == HAND_MAIN ? "main" : "off", s.grip2H[h].x,
                s.grip2H[h].y, s.grip2H[h].z);
        }
        Con_Printf("%s hand: two-handed %.2f, virtual stock %.2f, helping %d, empty %d, weapon %d clip %d, flick reload %s\n",
            h == HAND_MAIN ? "main" : "off", twohand::transition(h), twohand::stock(h), twohand::helping(h) ? 1 : 0,
            held::handEmpty(h) ? 1 : 0, cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2],
            cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONCLIP : protocol::STAT_QVR_WEAPONCLIP2],
            twohand::flickAllowed(h) ? "allowed" : "not allowed");
        if(s.muzzleValid[h])
        {
            Con_Printf("%s weapon muzzle (%.4f %.4f %.4f), %.1f units from the hand%s\n", h == HAND_MAIN ? "main" : "off",
                s.muzzle[h].x, s.muzzle[h].y, s.muzzle[h].z, glm::distance(s.muzzle[h], s.pos[h]),
                twohand::bladeGrip(h) ? ", held two-handed by its blade" : "");
        }
        const HandInput& in = tracking().input.hands[h];
        Con_Printf("%s hand: trigger %.2f grip %.2f thumb %d, curls %.1f %.1f %.1f %.1f %.1f\n",
            h == HAND_MAIN ? "main" : "off", in.triggerValue, in.gripValue, in.thumbTouch, fingerFrames[h][FingerThumb],
            fingerFrames[h][FingerIndex], fingerFrames[h][FingerMiddle], fingerFrames[h][FingerRing],
            fingerFrames[h][FingerPinky]);
        Con_Printf("  finger tweaks %+.2f %+.2f %+.2f %+.2f %+.2f (frame/open:", fingerBias[h][FingerThumb],
            fingerBias[h][FingerIndex], fingerBias[h][FingerMiddle], fingerBias[h][FingerRing], fingerBias[h][FingerPinky]);
        for(int f = FingerThumb; f < FingerCount; f++)
        {
            const FingerPose p = fingerPose(h, f);
            Con_Printf(" %d/%.2f", p.frame, p.open);
        }
        Con_Printf(")\n");
    }

    if(body::pouchEnabled())
    {
        const glm::vec3 pouch = body::pouchPosition(s);
        Con_Printf("grenade pouch at (%.2f %.2f %.2f), reach %.1f: main hand %.1f units off (hotspot %d), off hand %.1f (%d)\n",
            pouch.x, pouch.y, pouch.z, body::pouchReach(), glm::distance(s.pos[HAND_MAIN], pouch), s.hotspot[HAND_MAIN],
            glm::distance(s.pos[HAND_OFF], pouch), s.hotspot[HAND_OFF]);
    }

    if(body::ammoPouchEnabled())
    {
        const glm::vec3 pouch = body::ammoPouchPosition(s);
        Con_Printf("ammo pouch at (%.2f %.2f %.2f), reach %.1f: main hand %.1f units off (hotspot %d), off hand %.1f (%d)\n",
            pouch.x, pouch.y, pouch.z, body::ammoPouchReach(), glm::distance(s.pos[HAND_MAIN], pouch),
            s.hotspot[HAND_MAIN], glm::distance(s.pos[HAND_OFF], pouch), s.hotspot[HAND_OFF]);
        // What it holds (vr_pouchitems.cpp): its leather's frame, the rounds placed in it, the first one's skin and model.
        const int rounds = ammoPouchItemCount();
        const entity_t& first = entities.ammoPouchItem[0].ent;
        const glm::vec3 lag = pouchitems::wobble();
        Con_Printf("ammo pouch: frame %d rounds %d skin %d %s wobble %.3f %.3f %.3f\n", entities.ammoPouch.ent.frame, rounds,
            rounds ? first.skinnum : 0, rounds && first.model ? first.model->name : "-", lag.x, lag.y, lag.z);
        // The nail magazines' top nails (vr_view.cpp magFrame: 0 two, 1 one, 2 none): the first and last in the pouch,
        // and the ones seated in the hands' guns.
        const entity_t& last = entities.ammoPouchItem[za::max(rounds - 1, 0)].ent;
        const view::ViewEntity* seated = entities.mag;
        Con_Printf("magazine frames: pouch first %d last %d, seated main %d (%s) off %d (%s)\n", rounds ? first.frame : -1,
            rounds ? last.frame : -1, seated[HAND_MAIN].visible ? seated[HAND_MAIN].ent.frame : -1,
            seated[HAND_MAIN].visible ? seated[HAND_MAIN].ent.model->name : "-",
            seated[HAND_OFF].visible ? seated[HAND_OFF].ent.frame : -1,
            seated[HAND_OFF].visible ? seated[HAND_OFF].ent.model->name : "-");
        glm::vec3 centre;
        if(ammoPouchCentre(centre))
        {
            Con_Printf("ammo pouch collection centre (%.3f %.3f %.3f), %.3f units from reach point\n",
                centre.x, centre.y, centre.z, glm::distance(centre, pouch));
        }
    }

    int i = 0;
    forEachEntity([&](ViewEntity& ve) {
        const entity_t& e = ve.ent;
        Con_Printf("%2d %-24s vis %d mir %d frame %d skin %d org (%.3f %.3f %.3f) ang (%.0f %.0f %.0f)\n", i++,
            e.model ? e.model->name : "-", ve.visible, ve.mirrored, e.frame, e.skinnum, e.origin[0], e.origin[1],
            e.origin[2], e.angles[0], e.angles[1], e.angles[2]);
    });
}

bool previewGripMove(const hands::State& s, int hand, glm::vec3& worldMove)
{
    glm::vec3 cp, cd;
    float radius;
    if(!s.valid || !handrig::usable(viewModel(handrig::modelName)) || !rigHands[hand].drawn ||
        !grasp::gripChannel(rigHands[hand].pose, cp, cd, radius))
    {
        return false;
    }
    // The empty hand as it is drawn on the calibrated controller (with the fist's own Hand and Weapon Together offset),
    // whatever the hand holds now.
    const bool mirrored = hand == HAND_OFF;
    const int fist = weapons::fistSlot();
    glm::vec3 pos = s.calibratedPos[hand], rot = s.calibratedRot[hand];
    glm::mat3 turn{1.f};
    hands::wholeOffset(fist, hand, pos, rot, turn);
    const glm::vec3 handRot = basisAngles(anglesBasis(rot) * anglesBasis(weaponAngleOffsets(fist, mirrored)));
    const glm::vec3 channel{rigPlacement(hand, pos, handRot, mirrored, nullptr) * glm::vec4{cp, 1.f}};
    glm::vec3 c;
    glm::mat3 b;
    controllerPreviewPose(s, hand, c, b);
    worldMove = c - channel;
    return true;
}

// vr_reload_pouch_mags (Debug > Tests; pouch reloading's magazine guns, vr_reload_pouch_mag_*): each magazine the ammo
// pouch shows, its top as drawn (its mesh) against where the reload finds it (ammoPouchMagSlots: QC's way) and against the
// rounds' row's top middle (the old target, vr_reload_pouch_mag_slots 0), in cm out of the body, to the left and up; and
// each hand's gun's load point against the nearest one drawn (across it, over its top, the well's cosine along it).
void pouchMags_f()
{
    glm::vec3 drawn[pouchitems::magsMost], found[pouchitems::magsMost], box[4], up, foundUp;
    if(!ammoPouchRounds(box))
    {
        Con_Printf("pouchmags: no ammo pouch\n");
        return;
    }
    const int n = ammoPouchMags(drawn, up);
    const int m = ammoPouchMagSlots(found, foundUp);
    const float cm = 100.f / units::metresToUnits();
    glm::vec3 ax[3];
    for(int i = 0; i < 3; i++)
    {
        ax[i] = glm::length(box[i + 1]) > 1e-6f ? glm::normalize(box[i + 1]) : glm::vec3{0.f};
    }
    const auto parts = [&](const glm::vec3& d) {
        return glm::vec3{glm::dot(d, ax[0]), glm::dot(d, ax[1]), glm::dot(d, ax[2])} * cm;
    };
    Con_Printf("pouchmags: kind %d, %d left, frame %d: %d drawn, %d found\n", cl.stats[protocol::STAT_QVR_POUCHKIND],
        cl.stats[protocol::STAT_QVR_POUCHCOUNT], entities.ammoPouch.ent.frame, n, m);
    const glm::vec3 old = box[0] + box[3];
    for(int j = 0; j < n; j++)
    {
        const glm::vec3 a = j < m ? parts(found[j] - drawn[j]) : glm::vec3{0.f};
        const glm::vec3 b = parts(old - drawn[j]);
        Con_Printf("pouchmags: magazine %d at %.2f %.2f %.2f: found off it %.2f %.2f %.2f cm (%.2f); the row's top middle "
                   "%.2f %.2f %.2f cm (%.2f)\n",
            j, drawn[j].x, drawn[j].y, drawn[j].z, a.x, a.y, a.z, glm::length(a), b.x, b.y, b.z, glm::length(b));
    }
    const hands::State& s = hands::current();
    for(int h = 0; h < 2; h++)
    {
        if(!s.loadPortValid[h] || n == 0)
        {
            continue;
        }
        int best = 0;
        float bestD = 1e9f, across = 0.f, over = 0.f;
        for(int j = 0; j < n; j++)
        {
            const glm::vec3 d = s.loadPort[h] - drawn[j];
            const float hgt = glm::dot(d, up);
            const float acr = glm::length(d - up * hgt);
            if(za::max(acr, za::fabs(hgt)) < bestD)
            {
                bestD = za::max(acr, za::fabs(hgt));
                best = j;
                across = acr;
                over = hgt;
            }
        }
        Con_Printf("pouchmags: hand %d's load point at magazine %d: %.2f cm across it, %.2f cm over its top, cosine %.2f "
                   "along it\n",
            h, best, across * cm, over * cm, glm::dot(s.loadPortAxis[h], up));
    }
}
// vr_aim_check: per armed hand, where its shots go against its drawn barrel and the controller's calibrated aim (the
// Show Controller Laser's red, green and white, in degrees), with each one's pitch, and the wrist's bend (the drawn
// hand's forward against the forearm's, as last posed). The shots are the move's (weapons::shotAngles of the hand).
void aimCheck_f()
{
    const hands::State& s = hands::current();
    const auto deg = [](const glm::vec3& a, const glm::vec3& b) {
        return glm::degrees(za::acos(za::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.f, 1.f)));
    };
    const auto pitchOf = [](const glm::vec3& d) { return glm::degrees(za::asin(za::clamp(d.z, -1.f, 1.f))); };
    for(int h = 0; h < 2; h++)
    {
        const int slot = weapons::heldSlot(h);
        const char* name = h == HAND_MAIN ? "main" : "off";
        float bend = -1.f;
        avatar::ForearmFrame ff;
        if(avatar::forearmFrame(h, 1.f, ff))
        {
            bend = deg(ff.axes[0], ff.hand[0]);
        }
        if(slot < 0 || slot == weapons::fistSlot() || !s.muzzleValid[h])
        {
            Con_Printf("aim_check: %s hand unarmed, wrist bend %.1f\n", name, bend);
            continue;
        }
        const glm::vec3 shot = hands::forward(weapons::shotAngles(s.rot[h], slot, h == HAND_OFF));
        const glm::vec3 aim = hands::forward(s.aimRot[h]);
        const glm::vec3 barrel = glm::normalize(glm::vec3{hotspotFrame(entities.weapon[h].ent, h == HAND_OFF)[0]});
        Con_Printf("aim_check: %s hand slot %d: shot-barrel %.2f deg, shot-aim %.2f, barrel-aim %.2f; pitch shot %.2f "
                   "barrel %.2f aim %.2f; wrist bend %.1f\n",
            name, slot, deg(shot, barrel), deg(shot, aim), deg(barrel, aim), pitchOf(shot), pitchOf(barrel), pitchOf(aim),
            bend);
    }
}
} // namespace qvr::view

void qvr::view::registerDiagCommands()
{
    Cmd_AddCommand("vr_aim_check", aimCheck_f);
    Cmd_AddCommand("vr_dumpview", dumpView_f);
    Cmd_AddCommand("vr_grasp_dump", graspDump_f);
    Cmd_AddCommand("vr_grasp_bench", graspBench_f);
    Cmd_AddCommand("vr_ragdoll_hand_probe", ragdollHandProbe_f);
    Cmd_AddCommand("vr_grasp_sweep", graspSweep_f);
    Cmd_AddCommand("vr_model_reload", modelReload_f);
    Cmd_AddCommand("vr_hotspots_check", hotspotsCheck_f);
    Cmd_AddCommand("vr_anchor_where", anchorWhere_f);
    Cmd_AddCommand("vr_weapon_hotspot_here", hotspotHere_f);
    Cmd_AddCommand("vr_hotspot_fit", hotspotFit_f);
    Cmd_AddCommand("vr_admelee_spine", adMeleeSpine_f);
    Cmd_AddCommand("vr_reload_pouch_mags", pouchMags_f);
}
