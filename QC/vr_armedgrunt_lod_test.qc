// vr_armedgrunt_lod_test.qc -- the armed grunts' QC cost by state (DWELL.md 9.7, "d2m8's cost"; ROUND25.md "Armed
// grunts' cost on crowded maps"). Each frame every entity's think statements (the engine's count, thinkcost: as
// `profile` counts them, deterministic) go to the armed grunt they belong to (itself, its weapons, anything it owns),
// by the state it is in that frame:
//   idle-unseen  no enemy, farther than vr_agrunt_pose_lod_near from every player and outside each one's view (unseenfar)
//   idle-seen    no enemy, near or in a player's view
//   alert        an enemy, not in its sight
//   fighting     an enemy in its sight
//   dead         its health gone (a corpse, a ragdoll)
// vr_agrunt_test 226: the map as it stands (his start: idle), AGLOD_FRAMES frames. 227: every armed grunt woken at
// the player first (god: they fight him), then the same. Any map:
//   sv_random_seed 7;skill 1;map d2m8;wait120;god;vr_agrunt_test 226     -QuitOn "test 226 over"
// Lines: `agtest: aglod <state> grunts <average count> instr/grunt/frame <n> instr/frame <n>`, then `others` (every
// other think) and `all`. No PASS rule beyond the window counted (a measurement).

#ifdef QVR_TESTS

float AGLOD_FRAMES = 360;
float aglod_step, aglod_frames;
float aglod_cost[5];  // the window's statements by state
float aglod_count[5]; // its grunt-frames by state
float aglod_other;    // every other think's

string(float s) VR_AGLOD_Name =
{
    if(s == 0) { return "idle-unseen"; }
    if(s == 1) { return "idle-seen"; }
    if(s == 2) { return "alert"; }
    if(s == 3) { return "fighting"; }
    return "dead";
};

// The armed grunt `e` belongs to (itself, its weapon, a projectile or pack it owns), or world.
entity(entity e) VR_AGLOD_Grunt =
{
    if(e.vr_ag && (e.flags & FL_MONSTER))
    {
        return e;
    }
    entity o = e.owner;
    if(o && o.vr_ag && (o.flags & FL_MONSTER))
    {
        return o;
    }
    return world;
};

float(entity g) VR_AGLOD_State =
{
    if(g.health <= 0)
    {
        return 4;
    }
    if(g.enemy)
    {
        entity keep = self;
        self = g;
        float sees = visible(g.enemy);
        self = keep;
        return sees ? 3 : 2;
    }
    return unseenfar(g, cvar_hget(cvarh_vr_agrunt_pose_lod_near)) ? 0 : 1;
};

// Every entity's think statements since the last frame, to its grunt's state (or others).
void() VR_AGLOD_Tally =
{
    entity e = nextent(world);
    while(e)
    {
        float c = thinkcost(e, TRUE);
        entity g = VR_AGLOD_Grunt(e);
        if(g)
        {
            float s = VR_AGLOD_State(g);
            aglod_cost[s] = aglod_cost[s] + c;
            if(g == e)
            {
                aglod_count[s] = aglod_count[s] + 1;
            }
        }
        else
        {
            aglod_other = aglod_other + c;
        }
        e = nextent(e);
    }
};

void(float t) VR_AGLOD_Frame =
{
    entity p = vr_agt_player;
    float s;
    if(aglod_step == 0)
    {
        if(t == 227)
        {
            // Every armed grunt after him (god: he lives through it).
            entity keep = self;
            entity g = nextent(world);
            while(g)
            {
                if(g.vr_ag && (g.flags & FL_MONSTER) && g.health > 0 && !g.enemy)
                {
                    self = g;
                    g.enemy = p;
                    FoundTarget();
                }
                g = nextent(g);
            }
            self = keep;
        }
        for(s = 0; s < 5; s++)
        {
            aglod_cost[s] = 0;
            aglod_count[s] = 0;
        }
        aglod_other = 0;
        aglod_frames = 0;
        VR_AGLOD_Tally(); // (zeroes the counts: the window starts now)
        for(s = 0; s < 5; s++)
        {
            aglod_cost[s] = 0;
            aglod_count[s] = 0;
        }
        aglod_other = 0;
        aglod_step = 1;
        return;
    }
    VR_AGLOD_Tally();
    aglod_frames = aglod_frames + 1;
    if(aglod_frames < AGLOD_FRAMES)
    {
        return;
    }
    float all = aglod_other;
    for(s = 0; s < 5; s++)
    {
        all = all + aglod_cost[s];
        VR_AGT_Print(sprintf("aglod %s grunts %.1f instr/grunt/frame %.0f instr/frame %.0f", VR_AGLOD_Name(s),
            aglod_count[s] / aglod_frames, aglod_count[s] ? aglod_cost[s] / aglod_count[s] : 0, aglod_cost[s] / aglod_frames));
    }
    VR_AGT_Print(sprintf("aglod others instr/frame %.0f", aglod_other / aglod_frames));
    VR_AGT_Print(sprintf("aglod all thinks instr/frame %.0f (%s, %g frames)", all / aglod_frames, mapname, aglod_frames));
    VR_AGT_Check("the window counted", aglod_frames == AGLOD_FRAMES);
    aglod_step = 0;
    cvar_set("vr_agrunt_test", "0");
};

// vr_agrunt_test 228, on vrarena: a far, unseen armed grunt (a shotgun, 400 units behind him; vr_agrunt_pose_lod_near
// 128 for the test) wakes and fights as before. Two trials: brought into his view 400 units ahead (it sees him), and
// shot where it stands behind him (1 damage). Each: 64 frames unseen first (posed once in vr_agrunt_pose_lod_frames),
// then posed the very frame it is seen or hit, and every frame after (its pose's eases where they would be: as posed
// each frame), alert to him (its enemy) and attacking within 4 s. `agtest: aglod wake <trial> trace <hash>`: its
// place, frame, weapon's place and his health each frame for 3 s from the wake; aglod_test.sh runs it with
// vr_agrunt_pose_lod_frames 8, 2 (as before 1.1.1) and vr_agrunt_pose_lod 0, and the traces must agree.
//   sv_random_seed 7;skill 1;map vrarena;wait30;god;vr_agrunt_test 228     -QuitOn "test 228 over"
float AGLOD_WAKE_DIST = 400;
float aglod_trial, aglod_t0, aglod_poses, aglod_lastpose, aglod_wake, aglod_attacked, aglod_every, aglod_keepnear;
float aglod_hash;
vector aglod_fw;

void() VR_AGLOD_WakeFrame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(aglod_step == 0)
    {
        if(aglod_trial >= 2)
        {
            VR_AGPT_Clear();
            cvar_set("vr_agrunt_pose_lod_near", ftos(aglod_keepnear));
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            aglod_trial = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        if(aglod_trial == 0)
        {
            aglod_keepnear = cvar_hget(cvarh_vr_agrunt_pose_lod_near);
            cvar_set("vr_agrunt_pose_lod_near", "128");
        }
        VR_AGPT_Clear();
        p.flags = p.flags | FL_NOTARGET; // (unheard, unseen behind him)
        makevectors(p.angles);
        aglod_fw = v_forward;
        aglod_fw_z = 0;
        aglod_fw = normalize(aglod_fw);
        traceline(p.origin, p.origin - aglod_fw * (AGLOD_WAKE_DIST + 40), TRUE, p);
        float room = trace_fraction * (AGLOD_WAKE_DIST + 40);
        traceline(p.origin, p.origin + aglod_fw * (AGLOD_WAKE_DIST + 40), TRUE, p);
        if(room < AGLOD_WAKE_DIST + 30 || trace_fraction < 1)
        {
            VR_AGT_Check(sprintf("room %g units ahead and behind him (a map with room: vrarena)", AGLOD_WAKE_DIST), FALSE);
            aglod_trial = 2;
            return;
        }
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 64);
        setorigin(g, p.origin - aglod_fw * AGLOD_WAKE_DIST);
        g.angles_y = vectoyaw(aglod_fw); // (its back to him: it faces the way he does)
        aglod_t0 = time;
        aglod_poses = 0;
        aglod_lastpose = -1;
        aglod_step = 1;
        return;
    }
    if(aglod_step == 1)
    {
        if(!g.takedamage || !g.vr_ag_prop || time < aglod_t0 + 0.6)
        {
            return; // (its spawn done)
        }
        aglod_t0 = time;
        aglod_poses = 0;
        aglod_lastpose = g.vr_agp_lod_pose;
        aglod_step = 2;
        return;
    }
    if(aglod_step == 2)
    {
        // Unseen: its poses counted over 64 frames.
        if(g.vr_agp_lod_pose != aglod_lastpose)
        {
            aglod_poses = aglod_poses + 1;
            aglod_lastpose = g.vr_agp_lod_pose;
        }
        if(time < aglod_t0 + 64 / 72 - 0.001)
        {
            return;
        }
        aglod_every = max(2, cvar_hget(cvarh_vr_agrunt_pose_lod_frames));
        float want = cvar_hget(cvarh_vr_agrunt_pose_lod) ? floor(64 / aglod_every) : 64;
        VR_AGT_Check(sprintf("trial %g: unseen, posed %g times in 64 frames (%g)", aglod_trial, aglod_poses, want),
            fabs(aglod_poses - want) <= 1 && g.enemy == world);
        // The wake.
        if(aglod_trial == 0)
        {
            setorigin(g, p.origin + aglod_fw * AGLOD_WAKE_DIST);
            g.angles_y = vectoyaw(p.origin - g.origin);
            p.flags = p.flags - (p.flags & FL_NOTARGET);
        }
        else
        {
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            T_Damage(g, p, p, 1);
        }
        aglod_wake = time;
        aglod_attacked = 0;
        aglod_hash = 0;
        aglod_poses = 0;
        aglod_step = 3;
        return;
    }
    // Woken: posed the frame it was seen or hit (its think ran then), and every frame after while it is in view or alert.
    if(aglod_step == 3)
    {
        VR_AGT_Check(sprintf("trial %g: posed the frame it was %s", aglod_trial, aglod_trial == 0 ? "seen" : "hit"),
            g.vr_agp_lod_pose == aglod_wake);
        aglod_step = 4;
    }
    if(g.vr_agp_lod_pose >= time - frametime - 0.0001)
    {
        aglod_poses = aglod_poses + 1;
    }
    if(g.vr_ag_act || g.vr_ag_shots > 0)
    {
        aglod_attacked = 1;
    }
    float s = time - aglod_wake;
    if(s <= 3)
    {
        float k = floor(s * 72 + 0.5);
        aglod_hash = aglod_hash + (g.origin * '1 3 7' + g.frame * 11 + g.vr_ag_prop.origin * '5 13 17' + p.health * 19) *
            (1 + (k & 7));
    }
    if(s < 4)
    {
        return;
    }
    VR_AGT_Check(sprintf("trial %g: alert to him (enemy %s, shots %g, act %g)", aglod_trial, g.enemy.classname, g.vr_ag_shots, g.vr_ag_act), g.enemy == p);
    if(aglod_trial == 1)
    {
        VR_AGT_Check("trial 1: attacking within 4 s", aglod_attacked); // (seen 400 units ahead it closes in first: its trace, the same in every run, says it fights as before)
    }
    VR_AGT_Check(sprintf("trial %g: posed every frame since (%g of %g)", aglod_trial, aglod_poses, floor(4 * 72 + 0.5)),
        aglod_trial == 1 || aglod_poses >= floor(4 * 72 + 0.5) - 1); // (shot: behind him, alert, posed as before: every other frame till it aims)
    VR_AGT_Print(sprintf("aglod wake %g trace %.3f", aglod_trial, aglod_hash));
    aglod_trial = aglod_trial + 1;
    aglod_step = 0;
};

#endif
