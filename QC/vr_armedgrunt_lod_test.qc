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

// vr_agrunt_test 228, on vrarena: a far, unseen armed grunt (400 units behind him; vr_agrunt_pose_lod_near 128 for the
// test) wakes and fights as before. Four trials: a shotgun's brought into his view 400 units ahead (it sees him), and
// shot where it stands behind him (1 damage); an unarmed one's (its fists: the poser's LOD) woken by his shot's sound
// (VR_Stealth_Noise) and shot. Each: 64 frames unseen first (posed once in vr_agrunt_pose_lod_frames), then posed the
// very frame it is seen or hit, and every frame after (its pose's eases where they would be: as posed each frame),
// alert to him (its enemy; the sound: alert at least) and attacking within 4 s. `agtest: aglod wake <trial> trace
// <hash>`: its place, frame, turn, stealth state, weapon's place and his health each frame for 3 s from the wake;
// `aglod react <trial> enemy <frame> pose <frames after>`: the frame it took him for its enemy and how many after that
// it was posed. aglod_test.sh runs it with vr_agrunt_pose_lod_frames 8, 2 (as before 1.1.1) and vr_agrunt_pose_lod 0,
// and the traces and reactions must agree.
//   sv_random_seed 7;skill 1;map vrarena;wait30;god;vr_agrunt_test 228     -QuitOn "test 228 over"
float AGLOD_WAKE_DIST = 400;
float AGLOD_TRIALS = 4;
float aglod_trial, aglod_t0, aglod_poses, aglod_lastpose, aglod_wake, aglod_attacked, aglod_every, aglod_keepnear;
float aglod_hash, aglod_game, aglod_react_e, aglod_react_p, aglod_react_t;
vector aglod_fw;

void() VR_AGLOD_WakeFrame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(aglod_step == 0)
    {
        if(aglod_trial >= AGLOD_TRIALS)
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
            aglod_trial = AGLOD_TRIALS;
            return;
        }
        vr_agt_grunt = g = VR_AGT_Spawn(aglod_trial < 2 ? WID_SHOTGUN : -1, 64); // (2, 3: its fists)
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
        if(!g.takedamage || !(aglod_trial < 2 ? g.vr_ag_prop : g.vr_ag_poser) || time < aglod_t0 + 0.6)
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
        aglod_every = floor(cvar_hget(cvarh_vr_agrunt_pose_lod_frames)); // (0 or 1: every frame, no LOD)
        aglod_every = (cvar_hget(cvarh_vr_agrunt_pose_lod) && aglod_every >= 2) ? aglod_every : 1;
        // (the whole think's LOD, vr_agrunt_think_lod: once in vr_agrunt_think_lod_frames at least; aglod_test.sh runs
        // it off too)
        float thinkEvery = floor(cvar_hget(cvarh_vr_agrunt_think_lod_frames));
        if(cvar_hget(cvarh_vr_agrunt_think_lod) && thinkEvery > aglod_every && g.vr_ag_prop) // (armed: its weapon's think)
        {
            aglod_every = thinkEvery;
        }
        float want = floor(64 / aglod_every);
        VR_AGT_Check(sprintf("trial %g: unseen, posed %g times in 64 frames (%g)", aglod_trial, aglod_poses, want),
            fabs(aglod_poses - want) <= 1 && g.enemy == world);
        // The wake.
        if(aglod_trial == 0)
        {
            setorigin(g, p.origin + aglod_fw * AGLOD_WAKE_DIST);
            g.angles_y = vectoyaw(p.origin - g.origin);
            p.flags = p.flags - (p.flags & FL_NOTARGET);
        }
        else if(aglod_trial == 2)
        {
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            VR_Stealth_Noise(p.origin, 1000, p, STL_N_GUN); // (his shot's sound, heard behind him)
        }
        else
        {
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            T_Damage(g, p, p, 1);
        }
        aglod_wake = time;
        aglod_attacked = 0;
        aglod_hash = 0;
        aglod_game = 0;
        aglod_poses = 0;
        aglod_react_e = -1;
        aglod_react_p = -1;
        aglod_step = 3;
        return;
    }
    // Its reaction: the first frame its enemy is seen here (taken in the frame before: this runs before its thinks), and
    // the first after that it had been posed since.
    float kf = floor((time - aglod_wake) * 72 + 0.5);
    if(aglod_react_e < 0 && g.enemy)
    {
        aglod_react_e = kf;
        aglod_react_t = time - frametime;
    }
    if(aglod_react_e >= 0 && aglod_react_p < 0 && g.vr_agp_lod_pose >= aglod_react_t - 0.0001)
    {
        aglod_react_p = kf - aglod_react_e;
    }
    // Woken: posed the frame it was seen or hit (its think ran then), and every frame after while it is in view or alert.
    if(aglod_step == 3)
    {
        if(aglod_trial != 2) // (the sound: heard in a later frame, its reaction's line)
        {
            VR_AGT_Check(sprintf("trial %g: posed the frame it was %s", aglod_trial, aglod_trial == 0 ? "seen" : "hit"),
                g.vr_agp_lod_pose == aglod_wake);
        }
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
        // (the game alone: its place, frame, turn, state, his health; not its weapon's place, which a LOD's skipped pose
        // leaves where its move carried it: vr_agrunt_think_lod's on and off runs must agree on this one)
        aglod_game = aglod_game + (g.origin * '1 3 7' + g.frame * 11 + p.health * 19 + g.angles_y * 23 + g.stl_state * 29 +
            g.vr_ag_shots * 31) * (1 + (k & 7));
        aglod_hash = aglod_hash + (g.origin * '1 3 7' + g.frame * 11 + g.vr_ag_prop.origin * '5 13 17' + p.health * 19 +
            g.angles_y * 23 + g.stl_state * 29) * (1 + (k & 7));
    }
    if(s < 4)
    {
        return;
    }
    VR_AGT_Check(sprintf("trial %g: alert to him (enemy %s, stealth state %g, shots %g, act %g)", aglod_trial, g.enemy.classname,
        g.stl_state, g.vr_ag_shots, g.vr_ag_act), g.enemy == p || (aglod_trial == 2 && g.stl_state == STL_ALERT));
    VR_AGT_Print(sprintf("aglod react %g enemy %g pose %g", aglod_trial, aglod_react_e, aglod_react_p));
    if(aglod_trial == 1)
    {
        VR_AGT_Check("trial 1: attacking within 4 s", aglod_attacked); // (seen 400 units ahead it closes in first: its trace, the same in every run, says it fights as before)
    }
    VR_AGT_Check(sprintf("trial %g: posed every frame since (%g of %g)", aglod_trial, aglod_poses, floor(4 * 72 + 0.5)),
        aglod_trial != 0 || aglod_poses >= floor(4 * 72 + 0.5) - 1); // (shot or heard: behind him, alert, posed as before: every other frame till it aims)
    VR_AGT_Print(sprintf("aglod wake %g trace %.3f", aglod_trial, aglod_hash));
    VR_AGT_Print(sprintf("aglod wake %g game %.3f", aglod_trial, aglod_game));
    aglod_trial = aglod_trial + 1;
    aglod_step = 0;
};

// vr_agrunt_test 229, on vrarena: a worn pack's LOD (vr_worn_backpack_lod_near, the run sets 128) as its wearer walks
// into his view. A grunt with a shotgun (it wears a pack) 400 units off to his side, out of his view, brought round him
// at a walk (3.5 units a frame) into his view: its pack never placed while it was unseen, and placed from the first
// frame it is seen (on its body as his client draws it, the steps eased: within 64 units of its origin).
// The pack's own place is unseenfar's second entity (e2): one whose wearer leaves his PVS while the pack is still in it
// is placed (vr_wornpack.qc).
//   sv_random_seed 7;skill 1;map vrarena;wait30;god;vr_worn_backpack_lod_near 128;vr_agrunt_test 229     -QuitOn "test 229 over"
float AGLOD_PK_DIST = 400;
float aglod_pk_ang, aglod_pk_unseen, aglod_pk_moved;
vector aglod_pk_last;

void() VR_AGLOD_PackEnd =
{
    vr_agt_player.flags = vr_agt_player.flags - (vr_agt_player.flags & FL_NOTARGET);
    VR_AGPT_Clear();
    aglod_step = 0;
    cvar_set("vr_agrunt_test", "0");
};

// Its place `ang` degrees round from where he looks (to his right), AGLOD_PK_DIST off, at its own height.
vector(float ang) VR_AGLOD_PackAt =
{
    makevectors('0 1 0' * (vectoyaw(aglod_fw) - ang));
    vector at = vr_agt_player.origin + v_forward * AGLOD_PK_DIST;
    at_z = vr_agt_grunt.origin_z;
    return at;
};

void() VR_AGLOD_PackFrame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    entity pk = g.vr_wp_pack;
    if(aglod_step == 0)
    {
        VR_AGPT_Clear();
        if(cvar_hget(cvarh_vr_worn_backpack_lod_near) > AGLOD_PK_DIST - 50)
        {
            VR_AGT_Check("run with vr_worn_backpack_lod_near 128 (aglod_test.sh)", FALSE);
            VR_AGLOD_PackEnd();
            return;
        }
        p.flags = p.flags | FL_NOTARGET; // (it stands still)
        makevectors(p.v_angle);
        aglod_fw = v_forward;
        aglod_fw_z = 0;
        aglod_fw = normalize(aglod_fw);
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 64);
        aglod_t0 = time;
        aglod_step = 1;
        return;
    }
    if(aglod_step == 1)
    {
        if(time > aglod_t0 + 5)
        {
            VR_AGT_Check("a shotgun grunt wearing a pack (timed out)", FALSE);
            VR_AGLOD_PackEnd();
            return;
        }
        if(!g.takedamage || !pk || pk.vr_wp_state != VR_WP_WORN || pk.model == "" || time < aglod_t0 + 0.6)
        {
            return; // (its spawn done, its pack on)
        }
        aglod_pk_ang = 110;
        vector at = VR_AGLOD_PackAt(aglod_pk_ang);
        traceline(p.origin, at, TRUE, p);
        if(trace_fraction < 1)
        {
            VR_AGT_Check(sprintf("room %g units to his right (a map with room: vrarena)", AGLOD_PK_DIST), FALSE);
            VR_AGLOD_PackEnd();
            return;
        }
        setorigin(g, at);
        g.angles_y = vectoyaw(p.origin - g.origin);
        aglod_frames = 0;
        aglod_pk_moved = 0;
        aglod_pk_unseen = 0;
        aglod_step = 2;
        return;
    }
    if(aglod_step == 2)
    {
        // Round him at a walk until it is seen (its pack placed after this frame).
        aglod_frames = aglod_frames + 1;
        if(aglod_frames > 200)
        {
            VR_AGT_Check("brought into his view (timed out)", FALSE);
            VR_AGLOD_PackEnd();
            return;
        }
        // (its pack's think saw it where it was put last frame: moved since only if it was seen there)
        if(aglod_frames > 1 && aglod_pk_unseen && vlen(pk.origin - aglod_pk_last) > 0.01)
        {
            aglod_pk_moved = aglod_pk_moved + 1;
        }
        aglod_pk_last = pk.origin;
        aglod_pk_ang = aglod_pk_ang - 0.5; // (3.5 units at 400)
        setorigin(g, VR_AGLOD_PackAt(aglod_pk_ang));
        aglod_pk_unseen = unseenfar(g, 128);
        if(aglod_pk_unseen)
        {
            return;
        }
        VR_AGT_Check(sprintf("out of his view: its pack not placed (moved %g of %g frames, %.1f deg when seen)",
            aglod_pk_moved, aglod_frames - 1, aglod_pk_ang), aglod_pk_moved == 0 && aglod_frames > 10);
        aglod_step = 3;
        return;
    }
    if(aglod_step == 3)
    {
        aglod_step = 4; // (its pack's think after this frame's: placed by the next)
        return;
    }
    // (on its body as his client draws it: a monster's steps are drawn eased, up to a tenth of a second's walk behind)
    VR_AGT_Check(sprintf("seen: its pack placed from the first frame (moved %g units, %g off its origin)",
        vlen(pk.origin - aglod_pk_last), vlen(pk.origin - g.origin)), vlen(pk.origin - aglod_pk_last) > 1 &&
        vlen(pk.origin - g.origin) < 64);
    VR_AGLOD_PackEnd();
};

// vr_agrunt_test 235: a fight's census, for vr_agrunt_think_lod on against off (agent aglod; aglod_balance.py runs it on
// hip1m1's 69-monster fight, combat maps and the arena's waves, several seeds each). Over AGLODC_FRAMES frames
// (default 2160, 30 s) from its start: the players' damage taken (health set to 100000 first, no armour; god off),
// the frames he was hit in, the armed humanoids' shots (vr_ag_shots' rises), the monsters killed, the frames' think-LOD
// skips; then a hash of the game alone (every monster's place, health, frame, enemy; his health) and "test 235 over".
// Same seed, LOD on and off: the same hash means the same fight, frame for frame.
float AGLODC_FRAMES = 2160;
float aglodc_start, aglodc_hp, aglodc_hits, aglodc_shots, aglodc_kills, aglodc_skips0, aglodc_frames;
.float vr_aglodc_shots; // its vr_ag_shots last frame (the census)
.float vr_aglodc_dead;  // counted dead (the census)
void() VR_AGLOD_CensusFrame =
{
    entity p = VR_Player();
    entity e;
    if(!aglodc_start)
    {
        aglodc_start = time;
        aglodc_hits = 0;
        aglodc_shots = 0;
        aglodc_kills = 0;
        aglodc_frames = 0;
        aglodc_skips0 = vr_aglod_skips;
        p.flags = p.flags - (p.flags & FL_GODMODE);
        p.health = 100000;
        p.armorvalue = 0;
        aglodc_hp = p.health;
        return;
    }
    aglodc_frames = aglodc_frames + 1;
    if(p.health < aglodc_hp)
    {
        aglodc_hits = aglodc_hits + 1;
    }
    aglodc_hp = p.health;
    e = nextent(world);
    while(e)
    {
        if(e.flags & FL_MONSTER)
        {
            if(e.vr_ag_shots > e.vr_aglodc_shots)
            {
                aglodc_shots = aglodc_shots + (e.vr_ag_shots - e.vr_aglodc_shots);
            }
            e.vr_aglodc_shots = e.vr_ag_shots;
            if(e.health <= 0 && !e.vr_aglodc_dead)
            {
                e.vr_aglodc_dead = 1;
                aglodc_kills = aglodc_kills + 1;
            }
        }
        e = nextent(e);
    }
    float want = AGLODC_FRAMES;
    if(aglodc_frames < want)
    {
        return;
    }
    float h = p.health * 19;
    float n = 0;
    float hunting = 0;
    float dmin = 100000;
    e = nextent(world);
    while(e)
    {
        if(e.flags & FL_MONSTER)
        {
            n = n + 1;
            if(e.health > 0 && e.enemy == p)
            {
                hunting = hunting + 1;
                dmin = min(dmin, vlen(e.origin - p.origin));
            }
            h = h + (e.origin * '1 3 7' + e.health * 11 + e.frame * 13 + (e.enemy.origin * '1 1 1') * 17) * (1 + (n & 7));
        }
        e = nextent(e);
    }
    string head = sprintf("think_lod %g frames %g dmg %g hits %g shots %g", cvar_hget(cvarh_vr_agrunt_think_lod),
        aglodc_frames, 100000 - p.health, aglodc_hits, aglodc_shots);
    VR_AGT_Print(sprintf("aglod census: %s kills %g skips %g monsters %g game %.3f", head, aglodc_kills,
        vr_aglod_skips - aglodc_skips0, n, h));
    VR_AGT_Print(sprintf("aglod census hunting: %g of them hunting him, the nearest %.0f units; his flags %g", hunting, dmin,
        p.flags));
    aglodc_start = 0;
    cvar_set("vr_agrunt_test", "0");
};

#endif
