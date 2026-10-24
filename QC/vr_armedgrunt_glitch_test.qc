// Test 46: the armed humanoids' glitch finder (vr_agglitch.cpp; ROUND22.md, "Armed humanoids in motion: the glitch
// finder and a robust solve"; Debug > Tests > Armed Humanoids: Glitch Finder). You unseen; each subject in turn (each
// kind with each weapon class, VR_AGG_Set: fists, a sword, an axe, the guns, a chainsaw, two shotguns, two axes) plays
// its real gameplay loops against a target moving all round it (its own AI: it aims, fires, reloads, runs, strikes,
// takes pain, picks its weapon up again), VR_AGG_LEN seconds each:
// - 0..7 the target circling 220 units off, rising and falling (every yaw, pitch up and down): aim, fire, reload;
// - 7..9.5 the target far off (a run);
// - 9.5..13 the target circling close (its strikes; a gun's fire up close);
// - 13..15 pain (a little damage three times, its health kept);
// - 15..19 its weapon knocked out of its hand (it takes it up again).
// The engine measures every frame of it as drawn (vr_agglitch_ent, _subject, _state) and vr_agglitch_report prints and
// writes the worst moments and each metric's worst and 95th percentile. scratch2 / scratch3: the first and last subject
// (0..VR_AGG_COUNT - 1; both 0: all).
float VR_AGG_SETS = 12;
float VR_AGG_COUNT = 24;  // (both kinds)
float VR_AGG_LEN = 19;
float vr_agg_i;         // the subject now
float vr_agg_last;
float vr_agg_at;        // its start
float vr_agg_pain;      // its pain's label held till then
float vr_agg_hits;      // the damage dealt so far
float vr_agg_knocked;
vector vr_agg_centre;   // the target's circles' middle
vector vr_agg_far;      // ... its far spot
entity vr_agg_tgt;

float(float s) VR_AGG_Set =
{
    vr_agt_weapon2 = 0;
    if(s == 0) { return -1; }
    if(s == 1) { return WID_SWORD; }
    if(s == 2) { return WID_AXE; }
    if(s == 3) { return WID_SHOTGUN; }
    if(s == 4) { return WID_SUPER_SHOTGUN; }
    if(s == 5) { return WID_NAILGUN; }
    if(s == 6) { return WID_ROCKET_LAUNCHER; }
    if(s == 7) { return WID_LIGHTNING; }
    if(s == 8) { return WID_CHAINSAW; }
    if(s == 9) { vr_agt_weapon2 = WID_SHOTGUN; return WID_SHOTGUN; }
    if(s == 10) { vr_agt_weapon2 = WID_AXE; return WID_AXE; }
    return WID_MJOLNIR;
};

void(entity attacker, float damage) VR_AGG_TargetPain = { self.health = 100000; };
void() VR_AGG_TargetDie = { self.health = 100000; };

entity() VR_AGG_Target =
{
    if(!vr_agg_tgt)
    {
        vr_agg_tgt = spawn();
        vr_agg_tgt.classname = "vr_agg_target";
        vr_agg_tgt.netname = "the glitch finder's target";
        vr_agg_tgt.solid = SOLID_BBOX;
        vr_agg_tgt.movetype = MOVETYPE_NONE;
        vr_agg_tgt.takedamage = DAMAGE_AIM;
        vr_agg_tgt.th_pain = VR_AGG_TargetPain;
        vr_agg_tgt.th_die = VR_AGG_TargetDie;
        vr_agg_tgt.view_ofs = '0 0 22';
        setsize(vr_agg_tgt, '-16 -16 -24', '16 16 32');
    }
    vr_agg_tgt.health = 100000;
    return vr_agg_tgt;
};

string(entity g) VR_AGG_State =
{
    if(g.vr_ag_act == VR_AGP_PUNCH) { return sprintf("punch %g", g.vr_ag_act_kind); }
    if(g.vr_ag_act == VR_AGP_SWING) { return sprintf("swing %g", g.vr_ag_act_kind); }
    if(g.vr_ag_act == VR_AGP_SAW) { return "saw thrust"; }
    if(time < vr_agg_pain) { return "pain"; }
    VR_AM_Row(g.vr_ag_kind);
    if((vr_agg_knocked && !g.vr_ag_prop) || (g.frame >= vr_am_f_take && g.frame < vr_am_f_take + vr_am_n_take * 2)) { return "pickup"; }
    float rlt = max(0.1, cvar_hget(cvarh_vr_agrunt_reload_time));
    if(g.vr_agp_rl_at > 0 && time - g.vr_agp_rl_at <= rlt) { return "reload"; }
    if(g.vr_ag_kick_at > 0 && time - g.vr_ag_kick_at < 0.25) { return "fire"; }
    VR_AM_Row(g.vr_ag_kind);
    if(g.frame >= vr_am_f_run && g.frame < vr_am_f_run + 8) { return "run"; }
    if(g.vr_ag_aim != '0 0 0') { return "aim"; }
    return "carry";
};

void() VR_AGG_Next =
{
    entity p = vr_agt_player;
    if(vr_agt_grunt && vr_agt_grunt.classname != "") { VR_AGT_Remove(vr_agt_grunt); }
    VR_AGT_ClearWeapons();
    float kind = floor(vr_agg_i / VR_AGG_SETS);
    float set = vr_agg_i - kind * VR_AGG_SETS;
    cvar_set("vr_agrunt_test_kind", ftos(kind));
    float w1 = VR_AGG_Set(set);
    entity g = VR_AGT_Spawn(w1, 300);
    vr_agt_weapon2 = 0;
    vr_agt_grunt = g;
    g.health = 1000;
    g.max_health = 1000;
    vr_agg_centre = g.origin;
    makevectors([0, p.angles_y, 0]);
    traceline(g.origin, g.origin + v_forward * 650, TRUE, g);
    vr_agg_far = trace_endpos - v_forward * 40;
    entity tg = VR_AGG_Target();
    setorigin(tg, vr_agg_centre + v_forward * 220);
    entity oldself = self;
    self = g;
    self.enemy = tg;
    FoundTarget();
    self = oldself;
    vr_agg_at = time;
    vr_agg_pain = 0;
    vr_agg_hits = 0;
    vr_agg_knocked = 0;
    string sub = sprintf("%s %s%s", VR_AM_UiKindName(g.vr_ag_kind), g.vr_ag_prop ? g.vr_ag_prop.netname : "fists",
        g.vr_ag_prop2 ? sprintf("+%s", g.vr_ag_prop2.netname) : "");
    cvar_set("vr_agglitch_subject", sub);
    vector num = weapondrawnpose(g, 0, 20); // (its entity number)
    cvar_set("vr_agglitch_ent", ftos(num_x));
    VR_AGT_Print(sprintf("agglitch: subject %g: %s", vr_agg_i, sub));
};

void() VR_AGTest_Glitch =
{
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        p.flags = p.flags | FL_NOTARGET;
        vr_agg_i = cvar("scratch2");
        vr_agg_last = cvar("scratch3") > 0 ? min(VR_AGG_COUNT - 1, cvar("scratch3")) : VR_AGG_COUNT - 1;
        VR_AGG_Next();
        vr_agt_step = 1;
        return;
    }
    entity g = vr_agt_grunt;
    entity tg = VR_AGG_Target();
    float s = time - vr_agg_at;
    if(vr_agt_step == 1)
    {
        if(s >= VR_AGG_LEN || !g || g.classname == "" || g.health <= 0)
        {
            vr_agg_i = vr_agg_i + 1;
            if(vr_agg_i > vr_agg_last)
            {
                vr_agt_step = 2;
                return;
            }
            VR_AGG_Next();
            return;
        }
        // The target's place.
        vector at;
        if(s < 7)
        {
            float a = s * (s < 3.5 ? 50 : -50) + (s < 3.5 ? 0 : 350);
            makevectors([0, a, 0]);
            at = vr_agg_centre + v_forward * 220 + '0 0 1' * (130 * sin(s * 0.9));
        }
        else if(s < 9.5)
        {
            at = vr_agg_far;
        }
        else if(s < 13)
        {
            makevectors([0, (s - 9.5) * 40, 0]);
            at = g.origin + v_forward * 44; // (within its reach: VR_AG_GRIP_REACH)
        }
        else
        {
            makevectors([0, g.angles_y, 0]);
            at = g.origin + v_forward * (s < 15 ? 160 : 300);
        }
        setorigin(tg, at);
        if(g.enemy != tg && g.health > 0)
        {
            entity oldself = self;
            self = g;
            self.enemy = tg;
            FoundTarget();
            self = oldself;
        }
        // Pain three times, its health kept; then its weapon knocked out.
        if(s >= 13.2 && vr_agg_hits < 1 + floor((s - 13.2) / 0.7) && vr_agg_hits < 3)
        {
            vr_agg_hits = vr_agg_hits + 1;
            T_Damage(g, tg, tg, 3);
            g.health = 1000;
            vr_agg_pain = time + 0.6;
        }
        if(s >= 15 && !vr_agg_knocked)
        {
            vr_agg_knocked = 1;
            makevectors([0, g.angles_y + 60, 0]);
            VR_AG_Disarm(g, v_forward, 120, "the glitch finder");
        }
        cvar_set("vr_agglitch_state", VR_AGG_State(g));
        return;
    }
    if(vr_agt_step == 2)
    {
        if(vr_agt_grunt && vr_agt_grunt.classname != "") { VR_AGT_Remove(vr_agt_grunt); }
        vr_agt_grunt = world;
        cvar_set("vr_agglitch_ent", "0");
        localcmd("vr_agglitch_report agglitch\n");
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        VR_AGT_Print("agglitch: done");
        cvar_set("vr_agrunt_test", "0");
    }
};
