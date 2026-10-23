//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_play_test.qc -- the armed grunts' gameplay tests (vr_armedgrunt_play.qc; ROUND22.md, "Armed grunts:
// gameplay"). `vr_agrunt_test N` (20..39), in an open map (vrtesthall), the player kept alive (VR_AGTest_Frame):
// - 20: ragdolls: a grunt and an armed grunt side by side, killed (corpses), then knocked down alive (active ragdolls):
//   each shot at its torso with pellets: `agplay: rag <who> <state>: traced <classname>, health <h0> -> <h1>, gib <g>`
//   and PASS when the armed grunt takes the shots as the grunt does.
// ----------------------------------------------------------------------------

entity vr_agpt_a;  // the plain grunt
entity vr_agpt_b;  // the armed grunt
float vr_agpt_h0a;
float vr_agpt_h0b;

void(string s) VR_AGPT_Print = { bprint(sprintf("agplay: %s\n", s)); };

// Every grunt (armed or not) and every weapon lying about goes (each test's start).
void() VR_AGPT_Clear =
{
    VR_AGT_ClearWeapons();
    entity e = find(world, classname, "monster_army");
    while(e)
    {
        entity n = find(e, classname, "monster_army");
        VR_AGT_Remove(e);
        e = n;
    }
};

// A plain grunt `dist` ahead, `side` units to the right, facing the player.
entity(float dist, float side) VR_AGPT_Grunt =
{
    entity p = vr_agt_player;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    vector rt = v_right;
    rt_z = 0;
    vector at = p.origin + fw * dist + normalize(rt) * side;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.classname = "monster_army";
    monster_army();
    entity g = self;
    self = oldself;
    return g;
};

// Pellets from the player's eye at e's torso (`n` of them): what the line meets first.
string(entity e, float n) VR_AGPT_Shoot =
{
    entity p = vr_agt_player;
    vector at = isragdoll(e) ? limbplace(e, 1, 3) : e.origin + '0 0 10';
    if(at == '0 0 0') { at = e.origin + '0 0 4'; }
    vector from = p.origin + p.view_ofs;
    vector dir = normalize(at - from);
    traceline(from, at + dir * 32, MOVE_HITGIBS | MOVE_HITMODEL, p);
    string what = trace_ent.classname;
    entity oldself = self;
    self = p;
    VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
    vr_tracer_hand = cVR_MainHand;
    vr_tracer_from = from;
    FireBulletsImpl(n, dir, '0 0 0', from, 2048, FALSE);
    VR_ClearHitKind();
    self = oldself;
    return what;
};

void() VR_AGPT_Ragdoll =
{
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        p.flags = p.flags | FL_NOTARGET;
        vr_agpt_a = VR_AGPT_Grunt(160, -50);
        makevectors(p.angles);
        p.angles_y = p.angles_y - 17;
        vr_agpt_b = VR_AGT_Spawn(WID_SHOTGUN, 168);
        p.angles_y = p.angles_y + 17;
        vr_agt_step = 1;
        vr_agt_until = time + 0.5;
        return;
    }
    if(time < vr_agt_until) { return; }
    entity a = vr_agpt_a;
    entity b = vr_agpt_b;
    if(vr_agt_step == 1 || vr_agt_step == 4) // the alive ones knocked down (4: their corpses next)
    {
        if(vr_agt_step == 1)
        {
            VR_Knockdown_Start(a);
            VR_Knockdown_Start(b);
            VR_AGPT_Print(sprintf("rag knockdown: grunt %g (rag %g), armed %g (rag %g)", a.vr_knockdown, isragdoll(a),
                b.vr_knockdown, isragdoll(b)));
        }
        else
        {
            T_Damage(a, p, p, a.health + 1);
            T_Damage(b, p, p, b.health + 1);
        }
        vr_agt_step = vr_agt_step + 1;
        vr_agt_until = time + (vr_agt_step == 2 ? 0.4 : 2.5);
        return;
    }
    if(vr_agt_step == 2 || vr_agt_step == 5)
    {
        vr_agpt_h0a = vr_agt_step == 2 ? a.health : a.vr_corpse_hp;
        vr_agpt_h0b = vr_agt_step == 2 ? b.health : b.vr_corpse_hp;
        string ta = VR_AGPT_Shoot(a, 2);
        string tb = VR_AGPT_Shoot(b, 2);
        VR_AGPT_Print(sprintf("rag shots (%s): grunt traced %s, armed traced %s", vr_agt_step == 2 ? "alive" : "corpse", ta, tb));
        vr_agt_step = vr_agt_step + 1;
        vr_agt_until = time + 0.3;
        return;
    }
    if(vr_agt_step == 3 || vr_agt_step == 6)
    {
        string st = vr_agt_step == 3 ? "alive" : "corpse";
        float da = vr_agpt_h0a - a.health;
        float db = vr_agpt_h0b - b.health;
        if(vr_agt_step == 6)
        {
            da = vr_agpt_h0a - a.vr_corpse_hp;
            db = vr_agpt_h0b - b.vr_corpse_hp;
        }
        VR_AGPT_Print(sprintf("rag %s: grunt health %g -> %g rag %g, armed %g -> %g rag %g", st, vr_agpt_h0a, a.health,
            isragdoll(a), vr_agpt_h0b, b.health, isragdoll(b)));
        VR_AGT_Check(sprintf("rag %s: the armed grunt takes the shots as the grunt (%g, %g):", st, da, db), db > 0 && da > 0);
        if(vr_agt_step == 3)
        {
            vr_agt_step = 4;
            vr_agt_until = time + 0.1;
            return;
        }
        // Gibbed: a big blast on each corpse.
        T_Damage(a, p, p, 200);
        T_Damage(b, p, p, 200);
        float ga = a.model != "progs/soldier.mdl";
        float gb = b.model != "progs/vrgrunt.mdl";
        VR_AGT_Check(sprintf("rag gib: grunt %s (%s), armed %s (%s):", ga ? "gibbed" : "whole", a.model,
            gb ? "gibbed" : "whole", b.model), ga && gb);
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        cvar_set("vr_agrunt_test", "0");
    }
};

// The player's main hand emptied (the tug's and the yank's hand).
void(entity p) VR_AGPT_EmptyHand =
{
    p.weapon = WID_FIST;
    p.weaponinst = world;
    p.weaponflags = 0;
};

// The grunt moved so its weapon is at the player's main hand.
void(entity g, entity p) VR_AGPT_ToHand =
{
    vector off = p.handpos - g.vr_ag_prop.origin;
    off_z = 0;
    setorigin(g, g.origin + off);
    VR_AG_PlaceProp(g, g.vr_ag_prop);
};

// A weapon `wid` lying at `at` (stay: held there, not falling).
entity(float wid, vector at, float stay) VR_AGPT_Weapon =
{
    entity oldself = self;
    self = spawn();
    setorigin(self, vr_agt_player.origin + '0 0 8');
    self.weapon = wid;
    monster_vr_grunt();
    entity src = self;
    self = oldself;
    entity w = VR_AG_DropHeld(src, '0 0 0');
    VR_AGT_Remove(src);
    setorigin(w, at);
    w.velocity = '0 0 0';
    if(stay)
    {
        w.movetype = MOVETYPE_NONE;
    }
    return w;
};

// Tests 21, 22: the tug-of-war. 21: a calm shotgun grunt (not fighting back): your hand held still 2 s, it holds on;
// pulled 16 units back, yours after vr_agrunt_tug_time. The script holds +grabmain. 22: an axe grunt fighting back every 0.3 s, your hand still: it
// shakes you off (a shove or a headbutt) and keeps the axe.
void() VR_AGPT_Tug =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = cvar_hget(cvarh_vr_agrunt_test);
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        cvar_set("vr_agrunt_tug_fight", t == 21 ? "0" : "0.3");
        vr_agt_grunt = VR_AGT_Spawn(t == 21 ? WID_SHOTGUN : WID_AXE, 60);
        vr_agt_step = 1;
        vr_agt_until = time + 1;
        return;
    }
    if(time < vr_agt_until && vr_agt_step != 2 && vr_agt_step != 4) { return; }
    if(vr_agt_step == 1)
    {
        VR_AGT_Hunt(g);
        VR_AGPT_ToHand(g, p);
        VR_AG_GrabBy(g, p, cVR_MainHand); // (the hand closed on it: the script holds +grabmain)
        vr_agt_step = 2;
        vr_agt_until = time + 1;
        return;
    }
    if(vr_agt_step == 2) // the grab: a tug begun
    {
        if(!g.vr_ag_tug_pl && time < vr_agt_until) { return; }
        VR_AGT_Check(sprintf("tug begun: by %s, need %.2f s:", g.vr_ag_tug_pl.classname, VR_AG_TugNeed(g)),
            g.vr_ag_tug_pl == p && g.vr_ag_prop != world);
        vr_agt_step = 3;
        vr_agt_until = time + (t == 21 ? 2 : 3);
        return;
    }
    if(vr_agt_step == 3)
    {
        if(t == 22)
        {
            if(g.vr_ag_tug_pl && time < vr_agt_until) { return; }
            VR_AGT_Check(sprintf("tug fought off: %s, %g fights, the grunt holds %s, your hand %s:", vr_ag_tug_why,
                g.vr_ag_tug_fights, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", VR_AGT_Name(p.weapon)),
                !g.vr_ag_tug_pl && g.vr_ag_tug_losses >= 1 && g.vr_ag_prop != world && p.weapon == WID_FIST);
            cvar_set("vr_agrunt_tug_fight", "1");
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        VR_AGT_Check(sprintf("tug held still 2 s: it holds %s, tug on %g, pulled %.2f s:",
            g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", g.vr_ag_tug_pl != world, g.vr_ag_tug_prog),
            g.vr_ag_prop != world && g.vr_ag_tug_pl == p);
        // Your hand pulled back 16 units (the grunt stepped from it: the same stretch).
        setorigin(g, g.origin - g.vr_ag_tug_away * 16);
        vr_agt_clip = time;
        vr_agt_step = 4;
        vr_agt_until = time + 5;
        return;
    }
    if(g.vr_ag_tug_pl && time < vr_agt_until) { return; }
    VR_AGT_Print(sprintf("tug: stretch %.1f, pulled %.2f, need %.2f, grunt at %v", vr_ag_tug_stretch, g.vr_ag_tug_prog, VR_AG_TugNeed(g), g.origin));
    VR_AGT_Check(sprintf("tug pulled: won after %.2f s (%s), your hand %s, the grunt holds %s:", time - vr_agt_clip,
        vr_ag_tug_why, VR_AGT_Name(p.weapon), g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"),
        g.vr_ag_tug_wins == 1 && p.weapon == WID_SHOTGUN && !g.vr_ag_prop);
    cvar_set("vr_agrunt_tug_fight", "1");
    cvar_set("vr_agrunt_test", "0");
};

// Test 23: when a grab takes it at once: calm (no), badly hurt, staggering, knocked down, stunned by a parry; then a
// grab of the hurt one's shotgun: yours.
void() VR_AGPT_Easy =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 90);
        vr_agt_step = 1;
        vr_agt_until = time + 0.5;
        return;
    }
    if(time < vr_agt_until) { return; }
    if(vr_agt_step == 1)
    {
        VR_AGT_Hunt(g);
        string calm = VR_AG_TugEasy(g, p);
        g.vr_parry_stagger_until = time + 1;
        string stun = VR_AG_TugEasy(g, p);
        g.vr_parry_stagger_until = 0;
        float hp = g.health;
        g.health = 8;
        string hurt = VR_AG_TugEasy(g, p);
        g.health = hp;
        VR_AGT_Check(sprintf("easy: calm '%s', stunned '%s', hurt '%s':", calm, stun, hurt),
            calm == "" && stun != "" && hurt != "");
        makevectors('0 1 0' * g.angles_y);
        VR_Knockdown_Stagger(g, v_forward * -1, 0.5, 0.95);
        vr_agt_step = 2;
        vr_agt_until = time + 0.2;
        return;
    }
    if(vr_agt_step == 2)
    {
        string st = VR_AG_TugEasy(g, p);
        VR_AGT_Print(sprintf("easy: shoved (knockdown %g, stagger %g): '%s'", g.vr_knockdown, g.vr_kd_stagger, st));
        VR_Knockdown_Start(g);
        string kd = VR_AG_TugEasy(g, p);
        VR_AGT_Check(sprintf("easy: staggering '%s', knocked down '%s':", st, kd), st != "" && kd != "");
        vr_agt_step = 3;
        vr_agt_until = time + 0.05;
        return;
    }
    // A fresh calm grunt, badly hurt: the grab takes its gun at once (in your hand the next frame).
    if(vr_agt_step == 3)
    {
        VR_AGPT_Clear();
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 60);
        VR_AGT_Hunt(vr_agt_grunt);
        vr_agt_grunt.health = 8;
        VR_AG_GrabBy(vr_agt_grunt, p, cVR_MainHand);
        VR_AGT_Print(sprintf("easy: after the grab: your hand %g, lying %s", p.weapon, VR_AGT_LyingNear(vr_agt_grunt.origin).classname));
        vr_agt_step = 4;
        vr_agt_until = time + 0.3;
        return;
    }
    VR_AGT_Check(sprintf("easy: hurt grabbed (%s): your hand %s, the grunt holds %s:", vr_ag_tug_why, VR_AGT_Name(p.weapon),
        g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"), p.weapon == WID_SHOTGUN && !g.vr_ag_prop);
    cvar_set("vr_agrunt_test", "0");
};

// Test 24: blows and parries. A calm grunt's gun hand struck for 6 (under vr_agrunt_disarm_damage 10): it keeps it; the
// same, badly hurt: dropped (half). Then an axe grunt hunting you, every blow parried (a forced parry), the chance 1:
// disarmed.
void() VR_AGPT_Blows =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 200);
        vr_agt_step = 1;
        vr_agt_until = time + 0.5;
        return;
    }
    if(time < vr_agt_until && vr_agt_step != 3) { return; }
    if(vr_agt_step == 1)
    {
        VR_AGT_Hunt(g);
        vr_hitpos = g.vr_ag_prop.origin;
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, '0 0 0', '0 0 0');
        T_Damage(g, p, p, 6);
        float kept = g.vr_ag_prop != world;
        g.health = 8;
        vr_hitpos = g.vr_ag_prop.origin;
        T_Damage(g, p, p, 6);
        VR_ClearHitKind();
        vr_hitpos = '0 0 0';
        VR_AGT_Check(sprintf("blow 6 on the hand: calm %s, hurt %s:", kept ? "keeps it" : "dropped it",
            g.vr_ag_prop ? "keeps it" : "dropped it"), kept && !g.vr_ag_prop);
        VR_AGT_Remove(g);
        VR_AGPT_Clear();
        VRGiveEntWeapon(p, cVR_MainHand, WID_AXE, 0, 0);
        cvar_set("vr_agrunt_parry_disarm", "1");
        vr_agpt_h0a = cvar_hget(cvarh_vr_enemy_shove);
        cvar_set("vr_enemy_shove", "0"); // (its axe's blows parried, not its shoves)
        vr_parry_test_force = 2;
        vr_agt_grunt = VR_AGT_Spawn(WID_AXE, 70);
        vr_agt_step = 2;
        vr_agt_until = time + 0.3;
        return;
    }
    if(vr_agt_step == 2)
    {
        VR_AGT_Hunt(g);
        vr_agt_step = 3;
        vr_agt_until = time + 6;
        return;
    }
    if(g.vr_ag_prop && time < vr_agt_until) { return; }
    VR_AGT_Check(sprintf("parry: %g blows, the grunt holds %s, %g disarms:", g.vr_ag_strikes,
        g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", g.vr_ag_disarms), !g.vr_ag_prop && g.vr_ag_disarms == 1);
    vr_parry_test_force = 0;
    cvar_set("vr_enemy_shove", ftos(vr_agpt_h0a));
    cvar_set("vr_agrunt_parry_disarm", "0.35");
    cvar_set("vr_agrunt_test", "0");
};

// Test 25: shoves and falls. You unseen; a shotgun grunt ahead staggered by a shove that doesn't floor it: it keeps
// it; knocked down: it drops it beside it; getting up, it takes it back.
void() VR_AGPT_Fall =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        p.flags = p.flags | FL_NOTARGET;
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 140);
        vr_agt_step = 1;
        vr_agt_until = time + 0.5;
        return;
    }
    if(time < vr_agt_until && vr_agt_step != 4) { return; }
    if(vr_agt_step == 1)
    {
        makevectors('0 1 0' * g.angles_y);
        VR_Knockdown_Stagger(g, v_forward * -1, 0.4, 1);
        VR_AGT_Print(sprintf("fall: shoved: knockdown %g, stagger %g", g.vr_knockdown, g.vr_kd_stagger));
        vr_agt_step = 2;
        vr_agt_until = time + 2.5;
        return;
    }
    if(vr_agt_step == 2)
    {
        VR_AGT_Check(sprintf("fall: staggered, not floored (knockdown %g): it holds %s:", g.vr_knockdown,
            g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"), g.vr_ag_prop != world && g.vr_knockdown != 1);
        VR_Knockdown_Start(g);
        vr_agt_step = 3;
        vr_agt_until = time + 1.2;
        return;
    }
    if(vr_agt_step == 3)
    {
        entity w = VR_AGT_LyingNear(g.origin);
        VR_AGT_Check(sprintf("fall: floored (knockdown %g): it holds %s, its shotgun lying %.0f units from it:",
            g.vr_knockdown, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", w ? vlen(w.origin - g.origin) : -1),
            !g.vr_ag_prop && w && w.weapon == WID_SHOTGUN && vlen(w.origin - g.origin) < 72);
        vr_agt_step = 4;
        vr_agt_until = time + 15;
        return;
    }
    if(!g.vr_ag_prop && time < vr_agt_until) { return; }
    VR_AGT_Check(sprintf("fall: got up (knockdown %g) and took it back: %s, %g pickups:", g.vr_knockdown,
        g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", g.vr_ag_pickups), g.vr_ag_prop != world && g.vr_ag_pickups == 1);
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};

// Test 26: reach. You unseen at vrtesthall's start; an unarmed grunt on the floor by the pool, a shotgun floating in the
// pool and one in the air above the floor: it goes for neither (4 s); then one on the floor 120 units off: taken.
void() VR_AGPT_Reach =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        p.flags = p.flags | FL_NOTARGET;
        entity oldself = self;
        self = spawn();
        setorigin(self, '180 -256 24');
        self.angles_y = 0;
        self.weapon = -1;
        monster_vr_grunt();
        vr_agt_grunt = self;
        self = oldself;
        g = vr_agt_grunt;
        entity wet = VR_AGPT_Weapon(WID_SHOTGUN, '330 -256 -14', TRUE);
        entity high = VR_AGPT_Weapon(WID_SUPER_SHOTGUN, g.origin + '0 -110 60', TRUE);
        traceline(g.origin, g.origin - '0 0 64', TRUE, g);
        VR_AGT_Print(sprintf("reach: grunt at %v (floor %.0f), wet at %v (contents %g), high at %v", g.origin,
            trace_endpos_z, wet.origin, pointcontents(wet.origin), high.origin));
        VR_AGT_Print(sprintf("reach: wet reachable %g, high reachable %g", VR_AG_Reachable(g, wet), VR_AG_Reachable(g, high)));
        vr_agt_clip = g.origin_z;
        vr_agt_step = 1;
        vr_agt_until = time + 4;
        return;
    }
    if(time < vr_agt_until && vr_agt_step != 2) { return; }
    if(vr_agt_step == 1)
    {
        VR_AGT_Check(sprintf("reach: out of reach ignored: seeking %s, %g pickups, its height %.0f (was %.0f):",
            g.vr_ag_seek ? g.vr_ag_seek.netname : "nothing", g.vr_ag_pickups, g.origin_z, vr_agt_clip),
            !g.vr_ag_seek && g.vr_ag_pickups == 0 && fabs(g.origin_z - vr_agt_clip) < 2);
        entity dry = VR_AGPT_Weapon(WID_NAILGUN, g.origin + '0 120 -16', FALSE);
        VR_AGT_Print(sprintf("reach: dry at %v reachable %g (%s)", dry.origin, VR_AG_Reachable(g, dry), vr_ag_reach_why));
        vr_agt_step = 2;
        vr_agt_until = time + 10;
        return;
    }
    if(!g.vr_ag_prop && time < vr_agt_until) { return; }
    entity oldself = self;
    self = g;
    entity fw = VR_AG_FindWeapon();
    self = oldself;
    entity dw = find(world, classname, "thrown_weapon");
    VR_AGT_Print(sprintf("reach: frame %g, seek next %.1f, finds %s, a weapon at %v reachable %g (%s), shun %s", g.frame, g.vr_ag_seek_next - time, fw.netname, dw.origin, VR_AG_Reachable(g, dw), vr_ag_reach_why, g.vr_ag_shun.netname));
    VR_AGT_Check(sprintf("reach: the floor's taken: holds %s, %g pickups:", g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing",
        g.vr_ag_pickups), g.vr_ag_prop != world && g.vr_ag_prop.weapon == WID_NAILGUN);
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};

void(float t) VR_AGPT_Frame =
{
    if(t == 20) { VR_AGPT_Ragdoll(); }
    else if(t == 21 || t == 22) { VR_AGPT_Tug(); }
    else if(t == 23) { VR_AGPT_Easy(); }
    else if(t == 24) { VR_AGPT_Blows(); }
    else if(t == 25) { VR_AGPT_Fall(); }
    else if(t == 26) { VR_AGPT_Reach(); }
};
