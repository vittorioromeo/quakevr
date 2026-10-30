// vr_foegrab_diag.cpp -- holding enemies' diagnostics and tests: the neck snap's and the two-hand moves' measures,
// panels and drawings, the test commands (and their registration).
// Split from vr_foegrab.cpp (a pure move); what they share is in vr_foegrab_internal.hpp.

#include "vr_foegrab_internal.hpp"
#include "vr_progs.hpp"

namespace foegrab::detail
{
namespace
{
[[nodiscard]] bool neckDiagOn()
{
    return vr_foegrab_necksnap_debug.value != 0.f;
}
} // namespace

// Both hands of the first player at `m`'s head this frame, measured (`t`): the diagnostic's (with it on, or the log's).
void neckDiagMeasured(int c, edict_t* m, bool ragdoll, const HeadZone& z, const glm::vec3* point, const Zone* where,
    const NeckTwist& t)
{
    if(c != 0)
    {
        return;
    }
    NeckDiag& d = neckDiag;
    const double time = qcvm->time;
    const int num = NUM_FOR_EDICT(m);
    if(d.ent != num || d.since < 0.0 || time - d.measuredAt > 0.1)
    {
        d.since = time;
        d.best = 0.f;
    }
    d.ent = num;
    d.ragdoll = ragdoll;
    d.measured = true;
    d.measuredAt = time;
    d.zone = z;
    for(int h = 0; h < 2; h++)
    {
        d.point[h] = point[h];
        d.where[h] = where[h];
        d.holds[h] = true;
    }
    d.twist = t;
    d.best = za::max(d.best, t.turned);
}

// The first player's hands left `m`'s head (or snapped it): its largest turn printed (the diagnostic on, or
// vr_foegrab_debug), kept for the panel.
void neckDiagEnd(bool snapped)
{
    NeckDiag& d = neckDiag;
    if(d.since < 0.0)
    {
        return;
    }
    if(neckDiagOn() || debug())
    {
        edict_t* m = EDICT_NUM(d.ent);
        Con_Printf("necksnap diag: %s (%d) %s, Hand Line: both hands at its head %.2f s, its largest turn %.0f deg within "
                   "%.2f s (snaps at %.0f): %s\n",
            m->free ? "?" : PR_GetString(m->v.classname), d.ent, d.ragdoll ? "a ragdoll" : "standing",
            qcvm->time - d.since, d.best, neckWindow(), za::max(vr_foegrab_necksnap_angle.value, 1.f),
            snapped ? "snapped" : "no snap");
    }
    d.outcomeAt = qcvm->time;
    d.snapped = snapped;
    d.outcomeTurned = d.best;
    d.since = -1.0;
}

namespace
{
[[nodiscard]] bool liftDiagOn()
{
    return vr_foegrab_lift_debug.value != 0.f;
}

[[nodiscard]] bool throwDiagOn()
{
    return vr_foegrab_throw_debug.value != 0.f;
}
} // namespace

[[nodiscard]] bool legDiagOn()
{
    return vr_foegrab_leggrab_debug.value != 0.f;
}

namespace
{
// `m`'s place and yours, as the diagnostics show them.
void grabDiagPlace(GrabDiag& g, edict_t* player, edict_t* m)
{
    g.origin = vec(m->v.origin);
    g.bottom = m->v.absmin[2];
    g.top = m->v.absmax[2];
    g.you = vec(player->v.origin);
    g.off = glm::length(glm::vec2{g.origin - g.you});
    g.humanoid = liftable(m);
}
} // namespace

// Player `c`'s lift (throw) stopped now by `why` (the first player's: the diagnostic's).
void liftNote(int c, const char* why)
{
    if(c == 0)
    {
        grabDiag.liftNow = why;
    }
}
void throwNote(int c, const char* why)
{
    if(c == 0)
    {
        grabDiag.throwNow = why;
    }
}

// Both hands of the first player measured on `t.m` this frame (twoHandCheck), `way` the turn's smoothed way.
void grabDiagMeasured(int c, const TwoHands& t, const glm::vec3& way)
{
    if(c != 0)
    {
        return;
    }
    GrabDiag& g = grabDiag;
    const double time = qcvm->time;
    const int num = NUM_FOR_EDICT(t.m);
    if(g.ent != num || g.since < 0.0 || time - g.measuredAt > 0.1)
    {
        g.since = time;
        g.liftBest = g.throwBest = g.throwBestSpeed = 0.f;
        g.liftTried = g.throwTried = false;
        g.legPullBest = g.legUpBest = 0.f;
        g.legTried = false;
    }
    g.ent = num;
    g.measured = true;
    g.measuredAt = time;
    g.holds[0] = g.holds[1] = true;
    g.two = t;
    g.two.way = way;
    grabDiagPlace(g, t.player, t.m);
    g.liftBest = za::max(g.liftBest, t.vm.z);
    if(t.twist > g.throwBest)
    {
        g.throwBest = t.twist;
        g.throwBestSpeed = t.speed;
    }
    g.liftNow = g.throwNow = g.legNow = "";
}

// The first player's lift tried on `m`: `ok` lifted, `last` the panel's result; printed with the diagnostic on.
void liftTry(int c, edict_t* m, bool ok, const char* last, float up, float off)
{
    if(c != 0)
    {
        return;
    }
    GrabDiag& g = grabDiag;
    g.liftTried = true;
    g.tryEnt = NUM_FOR_EDICT(m);
    g.liftAt = qcvm->time;
    g.lifted = ok;
    q_strlcpy(g.liftLast, last, sizeof(g.liftLast));
    if(liftDiagOn())
    {
        Con_Printf("lift diag: %s (%d): pulled up %.2f m/s (lifts at %.2f, within 45 deg), %.0f units from you (reach "
                   "%.0f): %s\n",
            PR_GetString(m->v.classname), NUM_FOR_EDICT(m), up, liftSpeedNeed(), off, liftReach(), last);
    }
}

// The first player's judo throw tried on `m`: `ok` thrown, `last` the panel's result; printed with the diagnostic on.
void throwTry(int c, edict_t* m, bool ok, const char* last, float twist, float speed, const char* wayName)
{
    if(c != 0)
    {
        return;
    }
    GrabDiag& g = grabDiag;
    g.throwTried = true;
    g.tryEnt = NUM_FOR_EDICT(m);
    g.throwAt = qcvm->time;
    g.thrown = ok;
    q_strlcpy(g.throwLast, last, sizeof(g.throwLast));
    if(throwDiagOn())
    {
        Con_Printf("throw diag: %s (%d): turned %.0f deg/s (throws at %.0f) at %.2f m/s (at %.2f), %s: %s\n",
            PR_GetString(m->v.classname), NUM_FOR_EDICT(m), twist, vr_foegrab_throw_twist.value, speed,
            vr_foegrab_throw_speed.value, wayName, last);
    }
}

namespace
{
// The first player's two-hand hold measured until the last frame ended: each move not tried, its best printed.
void grabDiagEnd()
{
    GrabDiag& g = grabDiag;
    if(g.since < 0.0)
    {
        return;
    }
    edict_t* m = EDICT_NUM(g.ent);
    const char* name = m->free ? "?" : PR_GetString(m->v.classname);
    const double held = qcvm->time - g.since;
    if(liftDiagOn() && !g.liftTried)
    {
        Con_Printf("lift diag: %s (%d): both hands on it %.2f s, its fastest pull up %.2f m/s (lifts at %.2f, within 45 "
                   "deg): no lift\n",
            name, g.ent, held, g.liftBest, liftSpeedNeed());
    }
    if(throwDiagOn() && !g.throwTried)
    {
        Con_Printf("throw diag: %s (%d): both hands on it %.2f s, its hardest turn %.0f deg/s at %.2f m/s (throws at %.0f "
                   "deg/s, %.2f m/s): no throw\n",
            name, g.ent, held, g.throwBest, g.throwBestSpeed, vr_foegrab_throw_twist.value, vr_foegrab_throw_speed.value);
    }
    if(legDiagOn() && !g.legTried)
    {
        Con_Printf("leg diag: %s (%d): both hands on it %.2f s (%s, %s; grip %.2f), its fastest pull towards you %.2f m/s "
                   "(pulls at %.2f), up %.2f m/s (lifts at %.2f): no leg pull or lift\n",
            name, g.ent, held, g.legBone[0][0] ? g.legBone[0] : "-", g.legBone[1][0] ? g.legBone[1] : "-", g.legWeight,
            g.legPullBest, g.legPullNeed, g.legUpBest, g.legLiftNeed);
    }
    g.since = -1.0;
}
} // namespace

// The walk test's step (before the holds slow it): the monster walked straight away from the first player.
void walkTestStep(double dt)
{
    WalkTest& w = walkTest;
    if(w.ent <= 0 || w.ent >= qcvm->num_edicts)
    {
        return;
    }
    edict_t* m = EDICT_NUM(w.ent);
    if(m->free || qcvm->time >= w.until)
    {
        if(!w.reported)
        {
            w.reported = true;
            const glm::vec3 d = m->free ? glm::vec3{0.f} : vec(m->v.origin) - w.from;
            Con_Printf("foegrab walk test: %s (%d) moved %.1f units flat (asked %.1f), hold %.2f\n",
                m->free ? "?" : PR_GetString(m->v.classname), w.ent, glm::length(glm::vec2{d}), w.speed * w.seconds,
                m->free ? 0.f : fieldFloatOr(m, fields().vr_foegrab_hold, 0.f));
            w.ent = 0;
        }
        return;
    }
    edict_t* player = EDICT_NUM(1);
    glm::vec3 away = vec(m->v.origin) - vec(player->v.origin);
    away.z = 0.f;
    if(glm::length(away) < 1e-3f)
    {
        return;
    }
    const glm::vec3 from = vec(m->v.origin);
    const glm::vec3 to = from + glm::normalize(away) * (w.speed * static_cast<float>(dt));
    (void)moveFlat(m, to);
}

namespace
{
// The live monster nearest the first player (null: none).
[[nodiscard]] edict_t* nearestMonster()
{
    edict_t* player = EDICT_NUM(1);
    return progs::nearestBy(svs.maxclients + 1, 1e30f, [&](const edict_t* m) {
        const bool live = (static_cast<int>(m->v.flags) & FL_MONSTER) && m->v.health > 0.f;
        return live ? glm::distance(vec(m->v.origin), vec(player->v.origin)) : 1e30f;
    });
}

// vr_foegrab_necksnap_test: the live monster nearest the first player has its neck snapped as both his hands would
// (neckSnap: a humanoid's; standing, knocked down first; its ragdoll's neck loose, vr_foegrab_necksnap_loose), with no
// hands holding it. vr_foegrab_neck_angle <edict>: its ragdoll's head against its chest now (NECKANGLE: swing and twist,
// degrees; no edict: the last one vr_foegrab_necksnap_test snapped). For tests (Debug > Tests > Holding Enemies).
int neckTestNum = 0; // the edict vr_foegrab_necksnap_test snapped last
void neckSnapTest_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("vr_foegrab_necksnap_test: in a game\n");
        return;
    }
    const progs::ServerVm vm;
    edict_t* m = nearestMonster();
    if(!m || m->v.health <= 0.f)
    {
        Con_Printf("vr_foegrab_necksnap_test: no live monster\n");
        return;
    }
    NeckTwist t;
    t.turned = za::max(vr_foegrab_necksnap_angle.value, 1.f);
    t.line = t.turned;
    const int num = NUM_FOR_EDICT(m);
    const bool ragdoll = box3d::isRagdoll(num);
    neckTestNum = num;
    neckSnap(0, m, t, ragdoll);
    Con_Printf("vr_foegrab_necksnap_test: %s (%d) %s, health %.0f, ragdoll %d\n", PR_GetString(m->v.classname), num,
        ragdoll ? "a ragdoll" : "standing", m->v.health, box3d::isRagdoll(num) ? 1 : 0);
}

void neckAngle_f()
{
    float swing = 0.f, twist = 0.f;
    const int num = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : neckTestNum;
    if(!sv.active || !box3d::ragdollNeckAngle(num, swing, twist))
    {
        Con_Printf("NECKANGLE %d none\n", num);
        return;
    }
    Con_Printf("NECKANGLE %d swing %.1f twist %.1f\n", num, swing, twist);
}

// vr_foegrab_hurt <share>: the enemy the first player's hands hold (else the live monster nearest him) is left with that
// share of its full health (its .max_health): for trying the two-hand throw on a hurt one (vr_foegrab_throw_hurt). The
// training dummy's health is vr_dummy_health's. For tests (Debug > Tests > Holding Enemies).
void hurt_f()
{
    if(!sv.active || svs.maxclients < 1 || Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_foegrab_hurt <share of its full health, 0.01..1>\n");
        return;
    }
    const progs::ServerVm vm;
    edict_t* m = nullptr;
    for(int h = 0; h < 2 && !m; h++)
    {
        const Hold& g = holders[0].holds[h];
        m = g.active && g.ent < qcvm->num_edicts && !EDICT_NUM(g.ent)->free ? EDICT_NUM(g.ent) : nullptr;
    }
    m = m ? m : nearestMonster();
    if(!m || isTrainingDummy(m))
    {
        Con_Printf("vr_foegrab_hurt: %s\n", m ? "the training dummy's health is vr_dummy_health's" : "no live monster");
        return;
    }
    const float share = za::clamp(static_cast<float>(Q_atof(Cmd_Argv(1))), 0.01f, 1.f);
    const float full = m->v.max_health > 0.f ? m->v.max_health : m->v.health;
    m->v.health = za::max(1.f, za::round(full * share));
    Con_Printf("vr_foegrab_hurt: %s (%d) at %.0f of %.0f health\n", PR_GetString(m->v.classname), NUM_FOR_EDICT(m),
        m->v.health, full);
}

// vr_foegrab_walk_test <speed> <seconds>: the live monster nearest the first player walks straight away from him at
// `speed` units/s for `seconds` (moved each server frame, before the holds slow it): how far it got is printed (held or
// not). For tests (Debug > Tests > Holding Enemies).
void walkTest_f()
{
    if(!sv.active || svs.maxclients < 1 || Cmd_Argc() < 3)
    {
        Con_Printf("usage: vr_foegrab_walk_test <speed units/s> <seconds>\n");
        return;
    }
    const progs::ServerVm vm;
    edict_t* best = nearestMonster();
    if(!best)
    {
        Con_Printf("vr_foegrab_walk_test: no live monster\n");
        return;
    }
    const float seconds = static_cast<float>(Q_atof(Cmd_Argv(2)));
    walkTest = WalkTest{NUM_FOR_EDICT(best), static_cast<float>(Q_atof(Cmd_Argv(1))), seconds, qcvm->time + seconds,
        vec(best->v.origin), false};
    Con_Printf("foegrab walk test: %s (%d) walks away at %.0f units/s for %.2f s\n", PR_GetString(best->v.classname),
        walkTest.ent, walkTest.speed, seconds);
}

// vr_foegrab_throw_test [way] [twist] [edict]: the live monster nearest the first player thrown as both his hands' turn would
// (its kind and health decide; no hands needed): way 0 to his left, 1 to his right, 2 towards him, 3 away; `twist` the
// hands' turn of it about the vertical (degrees/s, + to the left). Its fall is traced (ThrowTrace). For tests (Debug >
// Tests > Holding Enemies).
void throwTest_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_foegrab_throw_test [way: 0 left, 1 right, 2 towards you, 3 away] [twist deg/s] (in a game)\n");
        return;
    }
    const progs::ServerVm vm;
    edict_t* player = EDICT_NUM(1);
    // (A third argument: that edict instead of the nearest, if it is a live monster: vr_ragdoll_act's.)
    const int only = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : 0;
    edict_t* m = only > svs.maxclients && only < qcvm->num_edicts ? EDICT_NUM(only) : nearestMonster();
    if(m && (m->free || !(static_cast<int>(m->v.flags) & FL_MONSTER) || m->v.health <= 0.f))
    {
        m = nullptr;
    }
    if(!m)
    {
        Con_Printf("vr_foegrab_throw_test: no live monster\n");
        return;
    }
    glm::vec3 away = vec(m->v.origin) - vec(player->v.origin);
    away.z = 0.f;
    away = glm::length(away) > 1e-3f ? glm::normalize(away) : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 left = glm::cross(glm::vec3{0.f, 0.f, 1.f}, away);
    const int way = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 0;
    const glm::vec3 dir = way == 1 ? -left : way == 2 ? -away : way == 3 ? away : left;
    const float twist = glm::radians(Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 0.f);
    const float result = throwDown(player, m, dir, twist, true);
    Con_Printf("vr_foegrab_throw_test: %s (%d): %s\n", PR_GetString(m->v.classname), NUM_FOR_EDICT(m), throwResult(result));
}

// vr_foegrab_status: each player's holds.
void status_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_foegrab_status: no server\n");
        return;
    }
    const progs::ServerVm vm;
    int count = 0;
    for(int c = 0; c < za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)); c++)
    {
        for(int h = 0; h < 2; h++)
        {
            const Hold& g = holders[c].holds[h];
            if(!g.active)
            {
                continue;
            }
            edict_t* m = EDICT_NUM(g.ent);
            const char* where = "";
            const bool drags = oneHandDrags(c, h, m, &where);
            Con_Printf("foegrab status: player %d %s hand holds %s (%d) at (%.1f %.1f %.1f): hand %.2f, held %.2f, stretch "
                       "%.1f cm, %.2f s; by %s: %s\n",
                c + 1, handName(h), PR_GetString(m->v.classname), g.ent, m->v.origin[0], m->v.origin[1], m->v.origin[2],
                g.strength, fieldFloatOr(m, fields().vr_foegrab_hold, 0.f), g.stretch / cm(), qcvm->time - g.since, where,
                drags ? "drags" : "doesn't drag");
            count++;
        }
    }
    if(count == 0)
    {
        Con_Printf("foegrab status: no hand holds an enemy\n");
    }
    for(int c = 0; c < za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)); c++)
    {
        if(holders[c].twoHanded)
        {
            Con_Printf("foegrab status: player %d holds it with both hands: its hardest turn %.0f deg/s, the hands at %.2f "
                       "m/s (throws at %.0f deg/s, %.2f m/s)\n",
                c + 1, holders[c].peakTwist, holders[c].peakSpeed, vr_foegrab_throw_twist.value,
                vr_foegrab_throw_speed.value);
        }
        if(holders[c].neckEnt != 0)
        {
            Con_Printf("foegrab status: player %d neck snap (hand line): the line's last turn %.0f deg, the largest "
                       "within %.2f s %.0f deg (snaps at %.0f)\n",
                c + 1, holders[c].neckWinLast, neckWindow(), holders[c].neckWinPeak, vr_foegrab_necksnap_angle.value);
        }
    }
    // An enemy held on as a ragdoll (handOver): its pelvis from the player (ahead, to his left, up).
    for(int c = 0; c < za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)); c++)
    {
        const Holder& hd = holders[c];
        Con_Printf("foegrab status: player %d at %.1f %.1f %.1f\n", c + 1, EDICT_NUM(c + 1)->v.origin[0],
            EDICT_NUM(c + 1)->v.origin[1], EDICT_NUM(c + 1)->v.origin[2]);
        edict_t* m = hd.carried > 0 && hd.carried < qcvm->num_edicts ? EDICT_NUM(hd.carried) : nullptr;
        glm::vec3 pelvis, head, feet;
        if(!m || m->free || !box3d::ragdollStance(hd.carried, pelvis, head, feet))
        {
            continue;
        }
        edict_t* player = EDICT_NUM(c + 1);
        const glm::vec3 ahead = forwardFlat(player);
        const glm::vec3 d = pelvis - vec(player->v.origin);
        Con_Printf("foegrab status: player %d holds %s (%d) on as a ragdoll (%s) with %d hand(s), %.2f s: its pelvis %.1f "
                   "ahead, %.1f to his left, %.1f up; the held limbs %.1f and %.1f units from the off and main hands\n",
            c + 1, PR_GetString(m->v.classname), hd.carried, hd.lifted ? "lifted" : hd.snapped ? "snapped" : "thrown", carryingHands(c, m),
            qcvm->time - hd.carriedSince, glm::dot(d, ahead), glm::dot(d, glm::cross(glm::vec3{0.f, 0.f, 1.f}, ahead)), d.z,
            box3d::ragdollHandReach(player, 0), box3d::ragdollHandReach(player, 1));
    }
    // The client's drawn hands (the stats as the view last put them on their spots).
    for(int h = 0; h < 2; h++)
    {
        if(pins[h].weight > 0.f)
        {
            Con_Printf("foegrab status: drawn %s hand %.2f on its spot (%.1f %.1f %.1f)\n", handName(h), pins[h].weight,
                pins[h].at.x, pins[h].at.y, pins[h].at.z);
        }
    }
}

// Each server frame, after the checks: a measured hold's end (neckDiagEnd); with the diagnostic on and no hands measuring,
// its enemy (a hand's hold, the foegrab's or a ragdoll's; else the humanoid nearest the palms within neckDiagReach) and
// where each palm would hold it.
constexpr float neckDiagReach = 64.f; // units
} // namespace

void neckDiagFrame()
{
    NeckDiag& d = neckDiag;
    const double time = qcvm->time;
    d.measured = d.measuredAt == time;
    if(d.since >= 0.0 && !d.measured)
    {
        neckDiagEnd(false);
    }
    if(!neckDiagOn() || svs.maxclients < 1)
    {
        return;
    }
    edict_t* player = EDICT_NUM(1);
    const VrMove* move = server::clientMove(player);
    if(!move)
    {
        d.ent = 0;
        return;
    }
    for(int h = 0; h < 2; h++)
    {
        d.palm[h] = trackedPalmOf(player, *move, h);
    }
    if(d.measured)
    {
        return;
    }
    d.ent = 0;
    const Holder& hd = holders[0];
    int ent = 0;
    for(int h = 0; h < 2; h++)
    {
        d.holds[h] = false;
        ent = ent == 0 && hd.holds[h].active ? hd.holds[h].ent : ent;
    }
    const int ofs[2]{fields().offhand_held, fields().mainhand_held};
    for(int h = 0; h < 2 && ent == 0; h++)
    {
        const int held = ofs[h] >= 0 ? fieldInt(player, ofs[h]) : 0;
        edict_t* e = held != 0 ? PROG_TO_EDICT(held) : nullptr;
        ent = e && !e->free && (static_cast<int>(e->v.flags) & FL_MONSTER) && e->v.health > 0.f ? NUM_FOR_EDICT(e) : 0;
    }
    const glm::vec3 middle = 0.5f * (d.palm[0] + d.palm[1]);
    float nearest = neckDiagReach;
    for(int num = 1; num < qcvm->num_edicts && ent == 0; num++)
    {
        edict_t* e = EDICT_NUM(num);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER) || e->v.health <= 0.f)
        {
            continue;
        }
        if(!groupTakes(e, mgroups::Param::NeckSnap))
        {
            continue;
        }
        const glm::vec3 box = glm::clamp(middle, vec(e->v.absmin), vec(e->v.absmax));
        if(glm::distance(box, middle) < nearest)
        {
            nearest = glm::distance(box, middle);
            d.ent = num;
        }
    }
    ent = ent != 0 ? ent : d.ent;
    if(ent == 0)
    {
        return;
    }
    edict_t* m = EDICT_NUM(ent);
    glm::vec3 joint{0.f};
    d.ent = ent;
    d.ragdoll = box3d::isRagdoll(ent);
    d.zone = d.ragdoll ? ragdollZoneOf(m, joint) : headZoneOf(m);
    d.twist = NeckTwist{};
    for(int h = 0; h < 2; h++)
    {
        const Hold& g = hd.holds[h];
        d.holds[h] = (g.active && g.ent == ent) || (ofs[h] >= 0 && fieldInt(player, ofs[h]) == EDICT_TO_PROG(m));
        box3d::RagdollHold rh;
        const bool ragHeld = d.ragdoll && box3d::ragdollHold(1, h, rh) && rh.num == ent;
        d.point[h] = d.palm[h];
        if(ragHeld)
        {
            (void)box3d::ragdollHoldPoint(1, h, d.point[h]);
        }
        else if(!d.ragdoll)
        {
            d.point[h] = restOf(m, g.active && g.ent == ent ? spotOf(g) : d.palm[h]);
        }
        d.where[h] = d.zone.where(d.point[h], d.ragdoll && !ragHeld ? palmSlack : 0.f);
    }
}

// Each server frame, after the checks: a measured two-hand hold's end (grabDiagEnd); with the lift's or the throw's
// diagnostic on and both hands not measuring, its enemy (a hand's hold; else the live monster nearest the palms within
// neckDiagReach, standing) as the hands would read on it now, and what stops each move.
void grabDiagFrame()
{
    GrabDiag& g = grabDiag;
    const double time = qcvm->time;
    g.measured = g.measuredAt == time;
    if(g.since >= 0.0 && !g.measured)
    {
        grabDiagEnd();
    }
    if((!liftDiagOn() && !throwDiagOn() && !legDiagOn()) || svs.maxclients < 1)
    {
        return;
    }
    edict_t* player = EDICT_NUM(1);
    const VrMove* move = server::clientMove(player);
    if(!move)
    {
        g.ent = 0;
        return;
    }
    for(int h = 0; h < 2; h++)
    {
        g.palm[h] = trackedPalmOf(player, *move, h);
    }
    if(g.measured)
    {
        return;
    }
    const Holder& hd = holders[0];
    int ent = 0;
    for(int h = 0; h < 2 && ent == 0; h++)
    {
        ent = hd.holds[h].active ? hd.holds[h].ent : 0;
    }
    const glm::vec3 middle = 0.5f * (g.palm[0] + g.palm[1]);
    float nearest = neckDiagReach;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts && ent == 0; num++)
    {
        edict_t* e = EDICT_NUM(num);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER) || e->v.health <= 0.f || box3d::isRagdoll(num))
        {
            continue;
        }
        const glm::vec3 box = glm::clamp(middle, vec(e->v.absmin), vec(e->v.absmax));
        if(glm::distance(box, middle) < nearest)
        {
            nearest = glm::distance(box, middle);
            g.ent = num;
        }
    }
    g.ent = ent != 0 ? ent : nearest < neckDiagReach ? g.ent : 0;
    // (None: the one last tried, a while (lifted or thrown, a ragdoll now): its result stays in view.)
    const bool recent = time - za::max(za::max(g.liftAt, g.throwAt), g.legAt) < 4.0;
    if(g.ent == 0 && recent && g.tryEnt > 0 && g.tryEnt < qcvm->num_edicts && !EDICT_NUM(g.tryEnt)->free)
    {
        g.ent = g.tryEnt;
    }
    if(g.ent == 0)
    {
        return;
    }
    edict_t* m = EDICT_NUM(g.ent);
    int hands = 0;
    for(int h = 0; h < 2; h++)
    {
        g.holds[h] = hd.holds[h].active && hd.holds[h].ent == g.ent;
        hands += g.holds[h] ? 1 : 0;
    }
    g.two = twoHandsOf(player, *move, m, false);
    grabDiagPlace(g, player, m);
    const char* why = hands == 2 ? "settling the grip" : hands == 1 ? "one hand: use both" : "not held: grab it";
    g.liftNow = vr_foegrab_lift.value == 0.f ? "Lift off" : why;
    g.throwNow = vr_foegrab_throw.value == 0.f ? "Two-Hand Throw off" : why;
    g.legNow = vr_foegrab_leggrab.value == 0.f ? "Leg Grabs off" : !g.humanoid ? "not a humanoid" : why;
    if(legDiagOn())
    {
        // Where each hand holds it, or its palm is (where it would).
        float weight = 1.f;
        for(int h = 0; h < 2; h++)
        {
            const LegPart p = legPartAt(m, g.holds[h] ? spotOf(hd.holds[h]) : g.palm[h], &g.legBone[h]);
            g.legPart[h] = static_cast<int>(p);
            weight = za::min(weight, legWeight(p));
        }
        g.legWeight = weight;
        g.legMass = qvr::foegrab::monsterMass(player, m);
        g.legFactor = legMassFactor(g.legMass);
        g.legPullNeed = za::max(vr_foegrab_legpull_speed.value, 0.1f) * g.legFactor / za::max(weight, 0.05f);
        g.legLiftNeed = za::max(vr_foegrab_leglift_speed.value, 0.1f) * g.legFactor / za::max(weight, 0.05f);
        glm::vec3 you = g.you - g.origin;
        you.z = 0.f;
        g.legYou = glm::length(you) > 1e-3f ? glm::normalize(you) : glm::vec3{1.f, 0.f, 0.f};
        g.legToward = glm::dot(glm::vec3{g.two.vm.x, g.two.vm.y, 0.f}, g.legYou);
        g.legDrawn = 0.f;
    }
}

namespace
{
// A circle round `axis` (unit) at `centre`.
void diagCircle(const glm::vec3& centre, const glm::vec3& axis, float radius, const glm::vec4& colour, float width)
{
    const glm::vec3 u = glm::normalize(glm::abs(axis.z) < 0.9f ? glm::cross(axis, glm::vec3{0.f, 0.f, 1.f}) :
                                                                 glm::cross(axis, glm::vec3{1.f, 0.f, 0.f}));
    const glm::vec3 v = glm::cross(axis, u);
    constexpr int sides = 24;
    for(int i = 0; i < sides; i++)
    {
        const float a0 = glm::two_pi<float>() * static_cast<float>(i) / sides;
        const float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) / sides;
        lines::line(centre + radius * (u * za::cos(a0) + v * za::sin(a0)),
            centre + radius * (u * za::cos(a1) + v * za::sin(a1)), width, colour, colour);
    }
}

// An arc round `axis` from `from` (unit, across the axis), `degrees` long.
void diagArc(const glm::vec3& centre, const glm::vec3& axis, const glm::vec3& from, float radius, float degrees,
    const glm::vec4& colour, float width)
{
    const glm::vec3 v = glm::cross(axis, from);
    const int steps = za::max(1, static_cast<int>(za::abs(degrees) / 10.f) + 1);
    for(int i = 0; i < steps; i++)
    {
        const float a0 = glm::radians(degrees) * static_cast<float>(i) / static_cast<float>(steps);
        const float a1 = glm::radians(degrees) * static_cast<float>(i + 1) / static_cast<float>(steps);
        lines::line(centre + radius * (from * za::cos(a0) + v * za::sin(a0)),
            centre + radius * (from * za::cos(a1) + v * za::sin(a1)), width, colour, colour);
    }
}

[[nodiscard]] glm::vec4 whereColour(Zone z)
{
    return atHead(z) ? glm::vec4{0.2f, 1.f, 0.3f, 1.f} : z == Zone::Shoulder ? glm::vec4{1.f, 0.6f, 0.1f, 1.f} :
                                                                            glm::vec4{1.f, 0.15f, 0.15f, 1.f};
}

// The diagnostics' panels (neckDebugDraw, grabDebugDraw): a value met (green) or not yet (orange); a bar's tick (its
// threshold, at the middle of its track).
[[nodiscard]] glm::vec4 diagColour(bool met)
{
    return met ? glm::vec4{0.2f, 1.f, 0.3f, 0.9f} : glm::vec4{1.f, 0.55f, 0.1f, 0.9f};
}
const glm::vec4 diagTick{1.f, 1.f, 1.f, 1.f};

// A diagnostic's panel: `text` beside `centre`, `side` units on the viewer's right, `up` higher (the hands and the enemy
// stay clear), turned to face the eye.
void diagPanel(const char* text, const glm::vec3& centre, float side, float up, const glm::vec3& eye,
    za::Span<const text3d::OverlayBar> bars)
{
    const glm::vec3 look = centre - eye;
    const glm::vec3 right = glm::length(glm::vec2{look}) > 0.1f ?
                                glm::normalize(glm::vec3{look.y, -look.x, 0.f}) : glm::vec3{0.f, -1.f, 0.f};
    const glm::vec3 at = centre + right * side + glm::vec3{0.f, 0.f, up};
    const glm::vec3 toPanel = at - eye;
    const float yaw = glm::degrees(za::atan2(toPanel.y, toPanel.x));
    text3d::queueOverlay(text, at, glm::vec3{0.f, yaw, 0.f}, 0.06f, bars);
}

// The leg grabs' diagnostic (vr_foegrab_leggrab_debug; Leg Grabs > Show Diagnostic): each palm (green below the knee,
// orange a thigh, red off its legs), the hands' pull towards you (a level arrow, 12 units a m/s: green at Pull Speed)
// and up (a vertical one: green at Leg Lift Speed), a panel over it: the parts, the grip's weight, its mass's factor,
// the pull and the lift against their needs, the stroke's length, what stops it now, the last try.
void legDebugDraw(const GrabDiag& g, const glm::vec3& eye)
{
    constexpr float width = 0.15f;
    constexpr float track = 24.f;
    constexpr float perMs = 12.f;
    const TwoHands& t = g.two;
    const double time = sv.qcvm.time;
    const glm::vec4 partColour[3]{glm::vec4{1.f, 0.15f, 0.15f, 1.f}, glm::vec4{1.f, 0.6f, 0.1f, 1.f},
        glm::vec4{0.2f, 1.f, 0.3f, 1.f}};
    for(int h = 0; h < 2; h++)
    {
        diagCircle(g.palm[h], glm::vec3{0.f, 0.f, 1.f}, g.holds[h] ? 2.5f : 1.5f, partColour[za::clamp(g.legPart[h], 0, 2)],
            width);
    }
    const glm::vec3 from = 0.5f * (g.palm[0] + g.palm[1]);
    const float up = t.vm.z, toward = g.legToward;
    const bool pullFast = toward >= g.legPullNeed, liftFast = up >= g.legLiftNeed;
    const glm::vec4 grey{0.7f, 0.7f, 0.7f, 0.6f};
    lines::line(from, from + g.legYou * (g.legPullNeed * perMs), width, grey, grey);
    lines::line(from, from + g.legYou * (za::max(toward, 0.f) * perMs), 0.3f, diagColour(pullFast), diagColour(pullFast));
    const glm::vec3 vert{0.f, 0.f, 1.f};
    lines::line(from, from + vert * (g.legLiftNeed * perMs), width, grey, grey);
    lines::line(from, from + vert * (za::max(up, 0.f) * perMs), 0.3f, diagColour(liftFast), diagColour(liftFast));
    char text[720], title[40], hand0[40], hand1[40], grip[48], pull[40], drawn[40], lift[40], mass[40], now[48], best[48],
        last[56] = "";
    q_snprintf(title, sizeof(title), "LEG GRAB");
    q_snprintf(hand0, sizeof(hand0), "off: %s%s%s", g.legBone[0], g.legBone[0][0] ? " " : "", legPartName(g.legPart[0]));
    q_snprintf(hand1, sizeof(hand1), "main: %s%s%s", g.legBone[1], g.legBone[1][0] ? " " : "", legPartName(g.legPart[1]));
    const bool same = g.legBone[0][0] && !q_strcasecmp(g.legBone[0], g.legBone[1]);
    q_snprintf(grip, sizeof(grip), "grip %.2f, %s", g.legWeight,
        g.legWeight <= 0.f ? "not a leg grip" : same ? "one leg" : "both legs");
    q_snprintf(pull, sizeof(pull), "pull %5.2f / %.2f m/s", toward, g.legPullNeed);
    q_snprintf(drawn, sizeof(drawn), "stroke %4.1f / %.0f cm", g.legDrawn, vr_foegrab_legpull_distance.value);
    q_snprintf(lift, sizeof(lift), "up %5.2f / %.2f m/s", up, g.legLiftNeed);
    q_snprintf(mass, sizeof(mass), "%.0f kg: x%.2f", g.legMass, g.legFactor);
    q_snprintf(now, sizeof(now), "now: %s", g.legNow[0] ? g.legNow : "-");
    q_snprintf(best, sizeof(best), "best pull %.2f, up %.2f m/s", g.legPullBest, g.legUpBest);
    if(g.legAt >= 0.0 && time - g.legAt < 4.0)
    {
        q_snprintf(last, sizeof(last), "last: %s", g.legLast);
    }
    q_snprintf(text, sizeof(text), "%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s",
        title, hand0, hand1, grip, pull, "", drawn, "", lift, "", mass, now, last[0] ? last : best);
    const float pullNeed = za::max(g.legPullNeed, 0.01f), liftNeed = za::max(g.legLiftNeed, 0.01f);
    const float distNeed = za::max(vr_foegrab_legpull_distance.value, 0.1f);
    const text3d::OverlayBar bars[6]{
        {5, 3.f, track * za::clamp(toward / (2.f * pullNeed), 0.f, 1.f), track, diagColour(pullFast)},
        {5, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
        {7, 3.f, track * za::clamp(g.legDrawn / (2.f * distNeed), 0.f, 1.f), track, diagColour(g.legDrawn >= distNeed)},
        {7, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
        {9, 3.f, track * za::clamp(up / (2.f * liftNeed), 0.f, 1.f), track, diagColour(liftFast)},
        {9, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
    };
    const glm::vec3 top{t.middle.x, t.middle.y, g.top};
    diagPanel(text, top, 0.f, 26.f, eye, za::Span<const text3d::OverlayBar>{bars, 6});
}

} // namespace

} // namespace foegrab::detail

za::String qvr::foegrab::noteHand(int hand)
{
    if(!sv.active || svs.maxclients < 1 || hand < 0 || hand > 1)
    {
        return {};
    }
    const progs::ServerVm vm;
    const Holder& holder = holders[0];
    const Hold& g = holder.holds[hand];
    za::String out;
    if(g.active && g.ent > 0 && g.ent < qcvm->num_edicts)
    {
        edict_t* m = EDICT_NUM(g.ent);
        const char* where = "";
        const bool drags = oneHandDrags(0, hand, m, &where);
        out = va("%s #%d by %s, hand's hold %.2f (held %.2f), stretch %.1f cm, %.1f s, health %.0f%s",
            PR_GetString(m->v.classname), g.ent, where && where[0] ? where : "?", g.strength,
            fieldFloatOr(m, fields().vr_foegrab_hold, 0.f), g.stretch / cm(), qcvm->time - g.since, m->v.health,
            drags ? ", drags it" : "");
    }
    // The two-hand moves' state (the same for both hands: on the main hand's line, or the off's when only it holds).
    if(hand == 1 || !holder.holds[1].active)
    {
        if(holder.twoHanded)
        {
            out += va("%sboth hands: hardest turn %.0f deg/s at %.2f m/s", out.empty() ? "" : "; ", holder.peakTwist,
                holder.peakSpeed);
        }
        if(holder.neckEnt != 0)
        {
            out += va("%sneck twist: last %.0f deg, largest %.0f deg", out.empty() ? "" : "; ", holder.neckWinLast,
                holder.neckWinPeak);
        }
        if(holder.carried != 0 && holder.carried < qcvm->num_edicts)
        {
            out += va("%scarrying %s #%d as a ragdoll for %.1f s%s%s%s", out.empty() ? "" : "; ",
                PR_GetString(EDICT_NUM(holder.carried)->v.classname), holder.carried, qcvm->time - holder.carriedSince,
                holder.lifted ? ", lifted" : "", holder.flailing ? ", flailing" : "", holder.snapped ? ", neck snapped" : "");
        }
    }
    return out;
}

void qvr::foegrab::init()
{
    Cmd_AddCommand("vr_foegrab_status", status_f);
    Cmd_AddCommand("vr_foegrab_walk_test", walkTest_f);
    Cmd_AddCommand("vr_foegrab_hurt", hurt_f);
    Cmd_AddCommand("vr_foegrab_throw_test", throwTest_f);
    Cmd_AddCommand("vr_foegrab_necksnap_test", neckSnapTest_f);
    Cmd_AddCommand("vr_foegrab_neck_angle", neckAngle_f);
    // vr_foegrab_throw_grunt_test: the throw's grunt as set, heard here (the menu's Hear Throw Grunt).
    Cmd_AddCommand("vr_foegrab_throw_grunt_test", [] { S_LocalSound(climb::gruntSample(vr_foegrab_throw_grunt_sound.value)); });
}

void qvr::foegrab::neckDebugDraw(const glm::vec3& eye)
{
    const NeckDiag& d = neckDiag;
    if(!sv.active || !neckDiagOn() || d.ent <= 0 || d.ent >= sv.qcvm.num_edicts || !d.zone.valid)
    {
        return;
    }
    const HeadZone& z = d.zone;
    const bool both = atHead(d.where[0]) && atHead(d.where[1]);
    constexpr float width = 0.15f;
    const glm::vec4 zoneColour{both ? glm::vec4{0.2f, 1.f, 0.3f, 0.8f} : glm::vec4{0.3f, 0.8f, 1.f, 0.7f}};
    // The zone: the head (circles where it is cut: the jaw, its middle, its widest across), the neck's column under it.
    const glm::vec3 n = z.axis;
    const float headAt = za::min(za::sqrt(za::max(z.sphere() * z.sphere() - z.jaw() * z.jaw(), 0.f)), z.headAcross());
    diagCircle(z.centre + n * z.jaw(), n, headAt, zoneColour, width);
    diagCircle(z.centre, n, za::min(z.sphere(), z.headAcross()), zoneColour, width);
    const float topAlong = za::sqrt(za::max(z.sphere() * z.sphere() - z.headAcross() * z.headAcross(), 0.f));
    diagCircle(z.centre + n * topAlong, n, z.headAcross(), zoneColour, width);
    diagCircle(z.centre + n * z.jaw(), n, z.neckAcross(), zoneColour, width);
    diagCircle(z.centre + n * z.bottom(), n, z.neckAcross(), zoneColour, width);
    const glm::vec3 u = glm::normalize(glm::abs(n.z) < 0.9f ? glm::cross(n, glm::vec3{0.f, 0.f, 1.f}) :
                                                             glm::cross(n, glm::vec3{1.f, 0.f, 0.f}));
    const glm::vec3 v = glm::cross(n, u);
    for(const glm::vec3& side : {u, v, -u, -v})
    {
        lines::line(z.centre + n * z.bottom() + side * z.neckAcross(), z.centre + n * z.jaw() + side * z.neckAcross(),
            width, zoneColour, zoneColour);
        lines::line(z.centre + n * z.jaw() + side * headAt, z.centre + n * topAlong + side * z.headAcross(), width,
            zoneColour, zoneColour);
    }
    // Its neck's axis (magenta), through the top.
    const glm::vec4 axisColour{1.f, 0.3f, 1.f, 0.9f};
    lines::line(z.centre + n * z.bottom(), z.centre + n * (z.sphere() + 4.f), width, axisColour, axisColour);
    // The hands: where each holds (or would), from its palm.
    for(int h = 0; h < 2; h++)
    {
        const glm::vec4 c = whereColour(d.where[h]);
        lines::point(d.point[h], d.holds[h] ? 1.6f : 1.f, c);
        lines::line(d.palm[h], d.point[h], 0.08f, glm::vec4{1.f, 1.f, 1.f, 0.5f}, c);
    }
    // The line between the palms (yellow), and the twist's arc round the neck: Twist Angle (grey), the turn now (orange,
    // green once it is enough), from the line's way across the neck.
    const glm::vec4 lineColour{1.f, 0.95f, 0.2f, 0.9f};
    lines::line(d.palm[0], d.palm[1], width, lineColour, lineColour);
    const float need = za::max(vr_foegrab_necksnap_angle.value, 1.f);
    glm::vec3 from = d.palm[1] - d.palm[0];
    from -= n * glm::dot(from, n);
    from = glm::length(from) > 0.01f ? glm::normalize(from) : u;
    const float arcR = z.sphere() + 3.f;
    diagArc(z.centre, n, from, arcR, za::min(need, 360.f), glm::vec4{0.7f, 0.7f, 0.7f, 0.6f}, width);
    if(d.measured && d.twist.turned > 0.f)
    {
        diagArc(z.centre, n, from, arcR + 0.6f, -za::min(d.twist.turned, 360.f),
            d.twist.turned >= need ? glm::vec4{0.2f, 1.f, 0.3f, 1.f} : glm::vec4{1.f, 0.55f, 0.1f, 1.f}, 0.3f);
    }

    // The panel: the hand line's numbers against Twist Angle.
    const NeckTwist& t = d.twist;
    const double time = sv.qcvm.time;
    char text[512];
    char last[40] = "";
    if(d.outcomeAt >= 0.0 && time - d.outcomeAt < 4.0)
    {
        q_snprintf(last, sizeof(last), d.snapped ? "last: SNAPPED %.0f deg" : "last: no snap, %.0f of %.0f",
            d.outcomeTurned, need);
    }
    char title[40], turn[40], parts[40], hands[40], best[40];
    q_snprintf(title, sizeof(title), "NECK SNAP: Hand Line %.2f s", neckWindow());
    q_snprintf(turn, sizeof(turn), "turn %4.0f / %.0f deg", t.turned, need);
    q_snprintf(parts, sizeof(parts), "line %4.0f wrists %4.0f", t.line, t.wrist);
    q_snprintf(hands, sizeof(hands), "off %-8s main %-8s", zoneName(d.where[0]), zoneName(d.where[1]));
    q_snprintf(best, sizeof(best), "best %3.0f deg", d.best);
    q_snprintf(text, sizeof(text), "%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s", title, turn, "", parts, hands, best,
        last);
    constexpr float track = 24.f;
    const text3d::OverlayBar bars[2]{
        {2, 3.f, track * za::min(t.turned / (2.f * need), 1.f), track, diagColour(t.turned >= need)},
        {2, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
    };
    diagPanel(text, z.centre, z.sphere() + 9.f, 6.f, eye, za::Span<const text3d::OverlayBar>{bars, 2});
}

void qvr::foegrab::grabDebugDraw(const glm::vec3& eye)
{
    const GrabDiag& g = grabDiag;
    const bool liftOn = liftDiagOn(), throwOn = throwDiagOn(), legOn = legDiagOn();
    if(!sv.active || (!liftOn && !throwOn && !legOn) || g.ent <= 0 || g.ent >= sv.qcvm.num_edicts)
    {
        return;
    }
    if(legOn)
    {
        legDebugDraw(g, eye);
    }
    constexpr float width = 0.15f;
    constexpr float track = 24.f;
    const glm::vec3 up{0.f, 0.f, 1.f};
    const glm::vec4 grey{0.7f, 0.7f, 0.7f, 0.6f};
    const TwoHands& t = g.two;
    const double time = sv.qcvm.time;
    const glm::vec3 top{t.middle.x, t.middle.y, g.top};
    // The palms: green on it, grey not.
    for(int h = 0; h < 2; h++)
    {
        lines::point(g.palm[h], g.holds[h] ? 1.6f : 1.f, g.holds[h] ? diagColour(true) : grey);
    }
    // (Both panels: the lift's on the viewer's right, the throw's on the left.)
    constexpr float side = 20.f;
    if(liftOn)
    {
        // The hands' pull from between them (12 units a m/s): green once at Lift Speed (the cone's top) and within 45
        // degrees of straight up (inside the cone).
        const float need = liftSpeedNeed();
        const float lift = t.vm.z, level = glm::length(glm::vec2{t.vm});
        // (Still hands: no way, shown as level.)
        const float angle = glm::length(t.vm) < 0.05f ? 90.f : glm::degrees(za::atan2(level, lift));
        const bool fast = lift >= need, steep = lift >= level;
        constexpr float perMs = 12.f;
        const glm::vec3 from = 0.5f * (g.palm[0] + g.palm[1]);
        const glm::vec3 coneTop = from + up * (need * perMs);
        diagCircle(coneTop, up, need * perMs, grey, width);
        for(const glm::vec3& s : {glm::vec3{1.f, 0.f, 0.f}, glm::vec3{-1.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f},
                glm::vec3{0.f, -1.f, 0.f}})
        {
            lines::line(from, coneTop + s * (need * perMs), width, grey, grey);
        }
        const glm::vec4 pull = diagColour(fast && steep);
        lines::line(from, from + t.vm * perMs, 0.3f, pull, pull);
        // Lift Reach round its feet: green with you inside.
        const float reach = liftReach();
        diagCircle(glm::vec3{g.origin.x, g.origin.y, g.bottom + 1.f}, up, za::max(reach, 1.f), diagColour(g.off <= reach),
            width);
        char text[640], title[40], speed[40], ang[40], dist[40], who[40], now[48], best[40], last[56] = "";
        q_snprintf(title, sizeof(title), "LIFT");
        q_snprintf(speed, sizeof(speed), "up %5.2f / %.2f m/s", lift, need);
        q_snprintf(ang, sizeof(ang), "angle %3.0f / 45 deg", angle);
        q_snprintf(dist, sizeof(dist), "reach %3.0f / %.0f units", g.off, reach);
        q_snprintf(who, sizeof(who), "hands %d, %s", (g.holds[0] ? 1 : 0) + (g.holds[1] ? 1 : 0),
            g.humanoid ? "a humanoid" : "not a humanoid");
        q_snprintf(now, sizeof(now), "now: %s", g.liftNow[0] ? g.liftNow : "-");
        q_snprintf(best, sizeof(best), "best up %.2f m/s", g.liftBest);
        if(g.liftAt >= 0.0 && time - g.liftAt < 4.0)
        {
            q_snprintf(last, sizeof(last), "last: %s", g.liftLast);
        }
        q_snprintf(text, sizeof(text), "%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s", title,
            speed, "", ang, "", dist, "", who, now, best, last);
        const text3d::OverlayBar bars[6]{
            {2, 3.f, track * za::clamp(lift / (2.f * need), 0.f, 1.f), track, diagColour(fast)},
            {2, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
            {4, 3.f, track * za::clamp(angle / 90.f, 0.f, 1.f), track, diagColour(steep)},
            {4, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
            {6, 3.f, track * za::clamp(g.off / (2.f * za::max(reach, 1.f)), 0.f, 1.f), track, diagColour(g.off <= reach)},
            {6, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
        };
        diagPanel(text, top, side, 0.f, eye, za::Span<const text3d::OverlayBar>{bars, 6});
    }
    if(throwOn)
    {
        // The turn's way from its top (0.08 units a deg/s: where its top would go), the circle Turn to Throw: green
        // once past it with the hands fast enough.
        const float need = vr_foegrab_throw_twist.value, speedNeed = vr_foegrab_throw_speed.value;
        constexpr float perDeg = 0.08f;
        const bool turned = t.twist >= need, fast = t.speed >= speedNeed;
        diagCircle(top, up, za::max(need, 1.f) * perDeg, grey, width);
        const glm::vec3 dir = glm::length(t.way) > 1e-4f ? glm::normalize(t.way) : glm::vec3{0.f};
        const glm::vec4 turn = diagColour(turned && fast);
        lines::line(top, top + dir * (t.twist * perDeg), 0.3f, turn, turn);
        const char* wayName = t.twist >= 10.f ? throwWayName(dir, t.middle, g.you, nullptr) : "-";
        char text[640], title[40], twist[40], speed[40], way[40], who[40], now[48], best[48], last[56] = "";
        q_snprintf(title, sizeof(title), "JUDO THROW");
        q_snprintf(twist, sizeof(twist), "turn %4.0f / %.0f deg/s", t.twist, need);
        q_snprintf(speed, sizeof(speed), "hands %4.2f / %.2f m/s", t.speed, speedNeed);
        q_snprintf(way, sizeof(way), "way: %s", wayName);
        q_snprintf(who, sizeof(who), "hands %d on it", (g.holds[0] ? 1 : 0) + (g.holds[1] ? 1 : 0));
        q_snprintf(now, sizeof(now), "now: %s", g.throwNow[0] ? g.throwNow : "-");
        q_snprintf(best, sizeof(best), "best %.0f deg/s at %.2f m/s", g.throwBest, g.throwBestSpeed);
        if(g.throwAt >= 0.0 && time - g.throwAt < 4.0)
        {
            q_snprintf(last, sizeof(last), "last: %s", g.throwLast);
        }
        q_snprintf(text, sizeof(text), "%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s", title, twist,
            "", speed, "", way, who, now, best, last);
        const text3d::OverlayBar bars[4]{
            {2, 3.f, track * za::clamp(t.twist / (2.f * za::max(need, 1.f)), 0.f, 1.f), track, diagColour(turned)},
            {2, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
            {4, 3.f, track * za::clamp(t.speed / (2.f * za::max(speedNeed, 0.01f)), 0.f, 1.f), track, diagColour(fast)},
            {4, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
        };
        diagPanel(text, top, liftOn ? -side : side, 0.f, eye, za::Span<const text3d::OverlayBar>{bars, 4});
    }
}
