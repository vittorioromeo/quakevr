// vr_armedgrunt_test.qc -- the armed grunts' tests (vr_armedgrunt.qc; ROUND22.md, "Armed grunts"). `vr_agrunt_test N`
// (Debug > Tests > Armed Grunts), in an open map (vrtesthall), the player kept alive (his health topped up):
// - 1: every weapon in turn: an armed grunt with it ahead (a melee one at arm's length, an explosive one far), the
//   player its enemy, for a few seconds: `agtest: <weapon>: <shots> shots, <dmg> damage to you, clip <c>, reserve <r>,
//   <n> reloads: PASS|FAIL` (PASS: it hurt the player), then `agtest: every weapon: <p> passed, <f> failed`.
// - 2: a shotgun grunt with half a reserve: it fires, reloads (`reload PASS`), is struck on its gun hand (`disarm PASS`:
//   its gun lies about, its clip with it), goes back for it (`rearm PASS`), and is killed (`drop PASS`: the gun lies
//   where it died, its rounds in it).
// - 3: the yank: a shotgun grunt standing ahead (the player unseen), the player's main hand (a script's vr_mock_hand)
//   emptied, the grunt moved to it, the hand to its gun's height (vr_mock_hand_lift: each kind's own) and closed (the
//   test's +grabmain): `yank PASS` when the shotgun is in that hand and the grunt holds nothing.
// - 4: armed grunts' state printed (`agtest: state: ...`: run it again after a save and a load).

float vr_agt_step;      // the test's step
float vr_agt_index;     // test 1: the weapon being tried
float vr_agt_until;     // the step's end
float vr_agt_passed;
float vr_agt_failed;
float vr_agt_dmg0;      // vr_ag_player_dmg at the try's start
float vr_agt_clip;      // test 2: its clip before something
entity vr_agt_grunt;
entity vr_agt_player;
float VR_AGT_COUNT = 17;

// The armed monster the tests spawn: vr_agrunt_test_kind (vr_armedmon.qc VR_AM_*: 0 the grunt, 1 the enforcer).
float() VR_AGT_Kind = { return min(VR_AM_COUNT - 1, max(0, rint(cvar("vr_agrunt_test_kind")))); };
// ... its classname (its monster's).
string() VR_AGT_Classname = { VR_AM_Row(VR_AGT_Kind()); return vr_am_classname; };

float(float i) VR_AGT_Wid =
{
    if(i == 0) { return -1; }
    if(i == 1) { return WID_AXE; }
    if(i == 2) { return WID_SWORD; }
    if(i == 3) { return WID_CROWBAR; }
    if(i == 4) { return WID_CHAINSAW; }
    if(i == 5) { return WID_MJOLNIR; }
    if(i == 6) { return WID_SHOTGUN; }
    if(i == 7) { return WID_SUPER_SHOTGUN; }
    if(i == 8) { return WID_NAILGUN; }
    if(i == 9) { return WID_SUPER_NAILGUN; }
    if(i == 10) { return WID_GRENADE_LAUNCHER; }
    if(i == 11) { return WID_ROCKET_LAUNCHER; }
    if(i == 12) { return WID_LIGHTNING; }
    if(i == 13) { return WID_LASER_CANNON; }
    if(i == 14) { return WID_PROXIMITY_GUN; }
    if(i == 15) { return WID_GRUNTGUN; }
    if(i == 16) { return WID_ENFRIFLE; }
    // Test 12: Arcane Dimensions' (with its data): the soldiers' guns, the upgrades, the melee weapons.
    if(i == 17) { return WID_ADROCKET; }
    if(i == 18) { return WID_ADGRENADE; }
    if(i == 19) { return WID_ADPLASMA; }
    if(i == 20) { return WID_ADDEFENDER; }
    if(i == 21) { return WID_ADCENTURION; }
    if(i == 22) { return WID_ADPYRO; }
    if(i == 23) { return WID_ADFUMIGATOR; }
    if(i == 24) { return WID_ADCROSSBOW; }
    if(i == 25) { return VR_AG_CODE_SHADOWAXE; }
    if(i == 26) { return VR_AG_CODE_WIDOWMAKER; }
    if(i == 27) { return VR_AG_CODE_PLASMAGUN; }
    return VR_AG_CODE_ADMELEE + (i - 27); // (28..37: kinds 1..10)
};
float VR_AGT_ALL = 38; // test 12 runs 17..37

// The class of a "weapon" key's weapon (VR_AG_Decode).
float(float code) VR_AGT_Class =
{
    if(code < 0) { return VR_AG_CLASS_FISTS; }
    VR_AG_Decode(code);
    return VR_AG_Class(vr_ag_dec_wid);
};

string(float wid) VR_AGT_Name = { return wid < 0 ? "fists" : WeaponNameOf(wid, 0); };

// An armed grunt with `wid` (-1 its fists), `dist` ahead of the player on the floor, facing him.
float vr_agt_weapon2; // VR_AGT_Spawn's left hand's weapon ("weapon2"; 0 none)

entity(float wid, float dist) VR_AGT_Spawn =
{
    entity p = vr_agt_player;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    traceline(p.origin, p.origin + fw * dist, TRUE, p);
    vector at = trace_endpos - fw * 24;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.weapon = wid;
    self.weapon2 = vr_agt_weapon2; // (the dual wielding test's left hand)
    VR_AM_Spawn(VR_AGT_Kind());
    entity g = self;
    self = oldself;
    return g;
};

// It sees the player (FoundTarget).
void(entity g) VR_AGT_Hunt =
{
    entity oldself = self;
    self = g;
    self.enemy = vr_agt_player;
    FoundTarget();
    self = oldself;
};

void(entity g) VR_AGT_Remove =
{
    if(g.vr_ag_prop)
    {
        g.vr_ag_prop.weaponinst = world;
        remove(g.vr_ag_prop);
    }
    g.vr_ag_prop = world;
    if(g.vr_ag_prop2)
    {
        g.vr_ag_prop2.weaponinst = world;
        remove(g.vr_ag_prop2);
    }
    g.vr_ag_prop2 = world;
    remove(g);
};

// Every weapon lying about goes (between test 1's tries).
void() VR_AGT_ClearWeapons =
{
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        entity n = find(e, classname, "thrown_weapon");
        remove(e);
        e = n;
    }
};

void(string s) VR_AGT_Print = { bprint(sprintf("agtest: %s\n", s)); };

void(string what, float ok) VR_AGT_Check =
{
    VR_AGT_Print(sprintf("%s %s", what, ok ? "PASS" : "FAIL"));
    if(ok) { vr_agt_passed = vr_agt_passed + 1; } else { vr_agt_failed = vr_agt_failed + 1; }
};

// The weapon lying nearest `at` (world: none within 96).
entity(vector at) VR_AGT_LyingNear =
{
    entity best = world;
    float bestDist = 96;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(vlen(e.origin - at) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - at);
        }
        e = find(e, classname, "thrown_weapon");
    }
    return best;
};

void() VR_AGTest_Weapons =
{
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_agt_index >= (cvar_hget(cvarh_vr_agrunt_test) == 1 ? VR_AGT_COUNT : VR_AGT_ALL))
        {
            VR_AGT_Print(sprintf("every weapon: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        VR_AGT_ClearWeapons();
        float wid = VR_AGT_Wid(vr_agt_index);
        float c = VR_AGT_Class(wid);
        float dist = (c == VR_AG_CLASS_MELEE || c == VR_AG_CLASS_FISTS) ? 70 : c == VR_AG_CLASS_EXPLOSIVE ? 360 : 240;
        vr_agt_grunt = VR_AGT_Spawn(wid, dist);
        vr_agt_step = 1;
        vr_agt_until = time + 0.6;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until) { return; }
        VR_AGT_Hunt(g);
        vr_agt_dmg0 = vr_ag_player_dmg;
        vr_agt_step = 2;
        vr_agt_until = time + 5;
        return;
    }
    if(time < vr_agt_until && g.health > 0)
    {
        return;
    }
    float dmg = vr_ag_player_dmg - vr_agt_dmg0;
    string nm = g.vr_ag_prop ? g.vr_ag_prop.netname : sprintf("%s (code %g)", VR_AGT_Name(g.vr_ag_lastwid), g.vr_ag_code);
    VR_AGT_Check(sprintf("%s: %g shots, %g blows, %g damage to you, clip %g, reserve %g, %g reloads, %g disarms:",
        nm, g.vr_ag_total_shots, g.vr_ag_strikes, dmg, g.vr_ag_prop.weaponinst.wi_clip,
        g.vr_ag_reserve, g.vr_ag_reloads, g.vr_ag_disarms), dmg > 0);
    VR_AGT_Remove(g);
    vr_agt_index = vr_agt_index + 1;
    vr_agt_step = 0;
    if(cvar_hget(cvarh_vr_agrunt_test) >= 100)
    {
        cvar_set("vr_agrunt_test", "0"); // (one weapon's)
    }
};

void() VR_AGTest_Sequence =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        cvar_set("vr_agrunt_reserve", "1");
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 200);
        vr_agt_step = 1;
        vr_agt_until = time + 0.6;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until) { return; }
        VR_AGT_Hunt(g);
        vr_agt_step = 2;
        vr_agt_until = time + 25;
        return;
    }
    if(vr_agt_step == 2) // fires its clip out, reloads
    {
        if(g.vr_ag_reloads < 1 && time < vr_agt_until) { return; }
        VR_AGT_Check(sprintf("reload: %g shots, %g reloads, clip %g, reserve %g:", vr_ag_shots_fired, g.vr_ag_reloads,
            g.vr_ag_prop.weaponinst.wi_clip, g.vr_ag_reserve), g.vr_ag_reloads >= 1 && g.vr_ag_prop.weaponinst.wi_clip > 0);
        // Struck on its gun hand.
        vr_agt_clip = g.vr_ag_prop.weaponinst.wi_clip;
        vr_hitpos = g.vr_ag_prop.origin;
        T_Damage(g, p, p, 12);
        vr_hitpos = '0 0 0';
        entity w = VR_AGT_LyingNear(g.origin);
        VR_AGT_Check(sprintf("disarm: held %s, lying %s with %g:", g.vr_ag_prop ? "something" : "nothing",
            w ? w.netname : "nothing", w.weaponinst.wi_clip), !g.vr_ag_prop && w && w.weaponinst.wi_clip == vr_agt_clip);
        vr_agt_step = 3;
        vr_agt_until = time + 12;
        return;
    }
    if(vr_agt_step == 3) // goes back for it
    {
        // (Taken up: counted when its hand closes on it, VR_AG_TakeGrab, a few frames after VR_AG_TakeBegin gave it the
        // prop: waiting on the prop alone checked before the count.)
        if((!g.vr_ag_prop || g.vr_ag_pickups < 1) && time < vr_agt_until && g.health > 0) { return; }
        entity lying = VR_AGT_LyingNear(g.origin);
        VR_AGT_Print(sprintf("rearm: frame %g, enemy %s, health %g, a weapon %.0f away (speed %.0f, depleted %g)", g.frame,
            g.enemy.classname, g.health, lying ? vlen(lying.origin - g.origin) : -1, vlen(lying.velocity), lying.vr_depleted));
        VR_AGT_Print(sprintf("rearm: next think in %g, knockdown %g, movetype %g, seek %s, seek next in %g", g.nextthink - time,
            g.vr_knockdown, g.movetype, g.vr_ag_seek.classname, g.vr_ag_seek_next - time));
        VR_AGT_Check(sprintf("rearm: %g pickups, holds %s:", g.vr_ag_pickups, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"),
            g.vr_ag_pickups >= 1 && g.vr_ag_prop != world);
        vr_agt_clip = g.vr_ag_prop.weaponinst.wi_clip;
        vector at = g.origin;
        T_Damage(g, p, p, 200);
        entity w2 = VR_AGT_LyingNear(at);
        VR_AGT_Check(sprintf("drop: lying %s with %g (held %g):", w2 ? w2.netname : "nothing", w2.weaponinst.wi_clip, vr_agt_clip),
            w2 && w2.weapon == WID_SHOTGUN && w2.weaponinst.wi_clip == vr_agt_clip);
        VR_AGT_Print(sprintf("sequence: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
        cvar_set("vr_agrunt_reserve", "2");
        cvar_set("vr_agrunt_test", "0");
    }
};

// The mock hand (a script's vr_mock_hand) brought up or down to height `z` (each kind's own weapon height, not the
// grunt's the scripts were written for): vr_mock_hand_lift (a cvar: at once, not after the script's waits as a command
// would be), taken up on the next frames; the tests set it back to 0.
void(entity p, float z) VR_AGT_HandToHeight =
{
    float dz = z - p.handpos_z;
    if(fabs(dz) > 1)
    {
        cvar_set("vr_mock_hand_lift", ftos(cvar("vr_mock_hand_lift") + dz / VRMetersToUnits()));
    }
};

void(entity g, entity p) VR_AGPT_ToHand; // (vr_armedgrunt_play_test.qc)

void() VR_AGTest_Yank =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        p.flags = p.flags | FL_NOTARGET;
        p.weapon = WID_FIST; // (the main hand empty)
        p.weaponinst = world;
        p.weaponflags = 0;
        cvar_set("vr_agrunt_tug", "0"); // (the grab at once: the tug-of-war is vr_agrunt_test 21..23)
        cvar_set("vr_mock_hand_lift", "0");
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 100); // (out of the hand's reach until it is brought there)
        vr_agt_step = 1;
        vr_agt_until = time + 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until) { return; }
        // The grunt moved so that its gun is at your main hand (the script holds it out: vr_mock_hand, and holds
        // +grabmain) and the hand to its gun's height (each kind's own); then the hand closes on it (its grab's time now:
        // as the button's press).
        VR_AGPT_ToHand(g, p);
        VR_AGT_HandToHeight(p, g.vr_ag_prop.origin_z); // (its kind's gun height: an enforcer's is higher)
        vr_agt_step = 2;
        vr_agt_until = time + 0.3;
        return;
    }
    if(vr_agt_step == 2)
    {
        if(time < vr_agt_until) { return; }
        VR_AGPT_ToHand(g, p);
        VR_AGT_Print(sprintf("yank: the hand at %v, the gun at %v", p.handpos, g.vr_ag_prop.origin));
        p.mainhand_lastgrabtime = time;
        vr_agt_step = 3;
        vr_agt_until = time + 3;
        return;
    }
    if(time < vr_agt_until && p.weapon != WID_SHOTGUN)
    {
        return;
    }
    cvar_set("vr_mock_hand_lift", "0");
    VR_AGT_Check(sprintf("yank:your main hand %s, the grunt holds %s:", VR_AGT_Name(p.weapon),
        g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"), p.weapon == WID_SHOTGUN && !g.vr_ag_prop);
    cvar_set("vr_agrunt_tug", "1");
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};

void() VR_AGTest_State =
{
    float n = 0;
    entity e = find(world, classname, VR_AGT_Classname());
    while(e)
    {
        if(e.vr_ag)
        {
            n = n + 1;
            entity q = e.vr_ag_prop;
            VR_AGT_Print(sprintf("state: %s health %g, holds %s (clip %g, reserve %g), its weapon %.1f from its hand",
                VR_AGT_Name(e.vr_ag_wid), e.health, q ? q.netname : "nothing", q.weaponinst.wi_clip,
                e.vr_ag_reserve, q ? vlen(q.origin - VR_AM_Hand(e)) : -1));
            if(q)
            {
                VR_AGT_Print(sprintf("state:   %s at %v (the grunt's origin + %v), angles %v, muzzle %v", q.model, q.origin,
                    q.origin - e.origin, q.angles, weaponmuzzle(q)));
            }
        }
        e = find(e, classname, VR_AGT_Classname());
    }
    VR_AGT_Print(sprintf("state: %g armed grunts", n));
    cvar_set("vr_agrunt_test", "0");
};

// Test 11: launcher grunts near walls (vr_agrunt_blast_safe 0 / 1: before / after). VR_AGT_BLAST engagements, each a
// grenade launcher's, a rocket launcher's or a proximity gun's grunt (in turn) spawned up to 120..420 units ahead of
// you on a random heading (a wall nearer: 24 units from it, its back to it), hunting you for 8 s or till it dies. Then
// `agtest: blast: <n> engagements, <k> self-kills (<g> gl, <r> rl, <p> prox), <d> self damage, <s> shots, <h> held,
// <y> damage to you`.
float VR_AGT_BLAST = 60;
float vr_agt_kills0;
float vr_agt_kills_by[3];
float vr_agt_yaw0;
void() VR_AGTest_Blast =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        if(vr_agt_index == 0)
        {
            vr_ag_self_kills = 0;
            vr_ag_self_dmg = 0;
            vr_ag_held_fire = 0;
            vr_ag_shots_fired = 0;
            vr_agt_dmg0 = vr_ag_player_dmg;
            vr_agt_yaw0 = p.angles_y;
            vr_agt_kills_by[0] = 0;
            vr_agt_kills_by[1] = 0;
            vr_agt_kills_by[2] = 0;
        }
        if(vr_agt_index >= VR_AGT_BLAST)
        {
            p.angles_y = vr_agt_yaw0;
            // (a builtin takes 8 arguments at most: two lines)
            VR_AGT_Print(sprintf("blast: %g engagements, %g self-kills (%g gl, %g rl, %g prox), %.0f self damage",
                VR_AGT_BLAST, vr_ag_self_kills, vr_agt_kills_by[0], vr_agt_kills_by[1], vr_agt_kills_by[2], vr_ag_self_dmg));
            VR_AGT_Print(sprintf("blast: %g shots, %g held, %.0f damage to you", vr_ag_shots_fired, vr_ag_held_fire,
                vr_ag_player_dmg - vr_agt_dmg0));
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        VR_AGT_ClearWeapons();
        entity e = find(world, classname, "proximity_grenade");
        while(e)
        {
            entity n = find(e, classname, "proximity_grenade");
            remove(e);
            e = n;
        }
        NumProximityGrenades = 0;
        float k = vr_agt_index - 3 * floor(vr_agt_index / 3);
        float wid = k == 0 ? WID_GRENADE_LAUNCHER : k == 1 ? WID_ROCKET_LAUNCHER : WID_PROXIMITY_GUN;
        p.angles_y = vr_agt_yaw0 + vr_agt_index * 137.5; // (headings spread round: the golden angle)
        vr_agt_grunt = VR_AGT_Spawn(wid, 120 + 300 * random());
        vr_agt_kills0 = vr_ag_self_kills;
        vr_agt_step = 1;
        vr_agt_until = time + 0.4;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until) { return; }
        VR_AGT_Hunt(g);
        vr_agt_step = 2;
        vr_agt_until = time + 7;
        return;
    }
    if(time < vr_agt_until && g.health > 0)
    {
        return;
    }
    if(vr_ag_self_kills > vr_agt_kills0)
    {
        float kk = vr_agt_index - 3 * floor(vr_agt_index / 3);
        vr_agt_kills_by[kk] = vr_agt_kills_by[kk] + 1;
    }
    VR_AGT_Print(sprintf("blast: #%g %s: %g shots, %g held, %s, %.0f from you", vr_agt_index,
        VR_AGT_Name(g.vr_ag_lastwid), g.vr_ag_total_shots, g.vr_ag_held,
        vr_ag_self_kills > vr_agt_kills0 ? "killed itself" : g.health > 0 ? "alive" : "dead", vlen(g.origin - p.origin)));
    if(g.health > 0)
    {
        VR_AGT_Remove(g);
    }
    vr_agt_index = vr_agt_index + 1;
    vr_agt_step = 0;
};

// Test 13: idle re-arm. You unseen (notarget), an unarmed armed grunt ahead and a shotgun lying 140 units to its side:
// `rearm idle PASS` when it holds it within 12 s, never having had an enemy.
void() VR_AGTest_IdleRearm =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        p.flags = p.flags | FL_NOTARGET;
        vr_agt_grunt = VR_AGT_Spawn(-1, 260);
        makevectors(p.angles);
        p.angles_y = p.angles_y + 30;
        entity src = VR_AGT_Spawn(WID_SHOTGUN, 200); // (a grunt dropping its shotgun: it lies there)
        p.angles_y = p.angles_y - 30;
        VR_AG_DropHeld(src, '0 0 0');
        VR_AGT_Remove(src);
        vr_agt_step = 1;
        vr_agt_until = time + 12;
        return;
    }
    if(!g.vr_ag_prop && time < vr_agt_until)
    {
        return;
    }
    VR_AGT_Check(sprintf("rearm idle: %g pickups, holds %s, enemy %s, after %.1f s:", g.vr_ag_pickups,
        g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", g.enemy ? g.enemy.classname : "none", 12 - (vr_agt_until - time)),
        g.vr_ag_prop != world && !g.enemy);
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};

void(float t) VR_AGPT_Frame; // (vr_armedgrunt_play_test.qc)
// Tests 14 and 15 (Debug > Tests > Armed Grunts: Attack Poses): four armed grunts in a row ahead (14: fists, axe, sword,
// crowbar; 15: chainsaw, Mjolnir, shotgun, super shotgun), turned 60 degrees, each held at an attack's phase every frame:
// scratch2 1 the wind-up's end, 2 the strike's middle, 3 its end (the chainsaw's cut), 4 the recovery's middle (0: as
// they are); scratch3 the variation (a punch's: 0 jab, 1 cross, 2 hook left, 3 hook right, 4 uppercut; a swing's: 0
// overhead, 1 diagonal, 2 horizontal, 3 backhand). No blow is struck. vr_agrunt_test 0 ends it.
entity vr_agt_row[4];
float vr_agt_row_made;

void(float t) VR_AGT_PoseRow =
{
    float k;
    if(vr_agt_row_made != t)
    {
        vr_agt_row_made = t;
        for(k = 0; k < 4; k++)
        {
            float wid = t == 14 ? VR_AGT_Wid(k == 0 ? 0 : k) : (k == 0 ? WID_CHAINSAW : k == 1 ? WID_MJOLNIR : k == 2 ? WID_SHOTGUN :
                WID_SUPER_SHOTGUN);
            makevectors(vr_agt_player.angles);
            vector side = v_right;
            side_z = 0;
            vector keep = vr_agt_player.origin;
            setorigin(vr_agt_player, keep + side * ((k - 1.5) * 40));
            vr_agt_weapon2 = (t == 14 && k > 0 && cvar("scratch4") > 0) ? WID_SWORD : 0; // (scratch4: a sword in each left hand)
            entity r = VR_AGT_Spawn(wid, cvar("scratch1") > 0 ? cvar("scratch1") : 120); // (scratch1: further off)
            vr_agt_weapon2 = 0;
            r.angles_y = vectoyaw(keep - r.origin) + 60;
            setorigin(vr_agt_player, keep);
            vr_agt_row[k] = r;
        }
    }
    float ph = cvar("scratch2");
    for(k = 0; k < 4; k++)
    {
        entity g = vr_agt_row[k];
        if(g && g.vr_ag && g.health > 0 && cvar("scratch5") > 0)
        {
            // (scratch5: each guards its head from you, as from a melee wind-up: its weapon carried by its hands)
            VR_Guard_Raise(g, vr_agt_player.origin + '0 0 22', normalize(g.origin - vr_agt_player.origin));
        }
        if(!g || !g.vr_ag || ph <= 0)
        {
            continue;
        }
        entity p = g.vr_ag_prop;
        g.vr_ag_act = !p ? VR_AGP_PUNCH : p.weapon == WID_CHAINSAW ? VR_AGP_SAW : VR_AG_Class(p.weapon) == VR_AG_CLASS_MELEE ?
            VR_AGP_SWING : 0;
        g.vr_ag_act_kind = cvar("scratch3");
        g.vr_ag_act_hand = 2; // (two held: both swing)
        if(g.vr_ag_act == VR_AGP_PUNCH)
        {
            g.vr_ag_act_kind = min(4, g.vr_ag_act_kind);
        }
        g.vr_ag_readyw = 0.7;
        VR_AGP_Times(g);
        float s = ph == 1 ? vr_agp_tw : ph == 2 ? vr_agp_tw + vr_agp_ts * 0.6 : ph == 3 ? vr_agp_tw + vr_agp_ts + vr_agp_th * 0.5 :
            vr_agp_tw + vr_agp_ts + vr_agp_th + vr_agp_tr * 0.5;
        g.vr_ag_act_at = time - s;
        g.vr_ag_act_tick = time;
        g.vr_ag_act_hit = 2; // (no blow, no cut)
        VR_AGP_Body(g);
        g.vr_ag_aim = (!g.vr_ag_act && ph >= 2) ? VR_AGP_ToWorld('1 0 0') : '0 0 0';
        g.vr_ag_kick_at = (!g.vr_ag_act && ph == 2) ? time - 0.03 : 0;
        g.nextthink = time + 1; // (held: its AI stands)
    }
};

// Test 40: dual wielding (`vr_agrunt_test_kind` the kind). A sword and a crowbar ("weapon2"): both held, the crowbar in
// its left hand; hunting you, both hands strike within 8 s; its left hand's weapon knocked out, it keeps the sword (in
// its right hand) and, out of a fight, takes the crowbar up again in its left; dying, both fall.
float vr_agt_count;
float vr_agt_keep_disarm;
float vr_agt_keep_parry;
void() VR_AGTest_Dual =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        vr_agt_weapon2 = WID_CROWBAR;
        vr_agt_grunt = VR_AGT_Spawn(WID_SWORD, 60);
        vr_agt_weapon2 = 0;
        g = vr_agt_grunt;
        entity pr = g.vr_ag_prop;
        entity pl = g.vr_ag_prop2;
        VR_AGT_Check(sprintf("dual: holds %s (left %g) and %s (left %g):", pr ? pr.netname : "nothing", pr.vr_ag_left,
            pl ? pl.netname : "nothing", pl.vr_ag_left), pr && pl && !pr.vr_ag_left && pl.vr_ag_left && pl.weapon == WID_CROWBAR);
        VR_AGT_Hunt(g);
        vr_agt_keep_disarm = cvar("vr_agrunt_disarm");
        vr_agt_keep_parry = cvar("vr_agrunt_parry_disarm");
        cvar_set("vr_agrunt_disarm", "0"); // (the fight's own disarms off: the test's blow below)
        cvar_set("vr_agrunt_parry_disarm", "0");
        vr_agt_step = 1;
        vr_agt_until = time + 8;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until && (g.vr_ag_strikes_l < 1 || g.vr_ag_strikes - g.vr_ag_strikes_l < 1)) { return; }
        cvar_set("vr_agrunt_disarm", ftos(vr_agt_keep_disarm));
        cvar_set("vr_agrunt_parry_disarm", ftos(vr_agt_keep_parry));
        VR_AGT_Check(sprintf("dual: blows right %g, left %g:", g.vr_ag_strikes - g.vr_ag_strikes_l, g.vr_ag_strikes_l),
            g.vr_ag_strikes_l >= 1 && g.vr_ag_strikes - g.vr_ag_strikes_l >= 1);
        // Its left hand's weapon knocked out (the first slot's rules: swapped there first, as a blow on it does).
        if(g.vr_ag_prop2 && !g.vr_ag_prop.vr_ag_left)
        {
            VR_AG_Swap(g);
        }
        VR_AG_Disarm(g, '0 1 0', 60, "the test's blow on its left hand's weapon");
        VR_AGT_Check(sprintf("dual: left disarmed: holds %s (left %g), second %s:", g.vr_ag_prop.netname, g.vr_ag_prop.vr_ag_left,
            g.vr_ag_prop2 ? g.vr_ag_prop2.netname : "none"), g.vr_ag_prop.weapon == WID_SWORD && !g.vr_ag_prop.vr_ag_left &&
            !g.vr_ag_prop2);
        p.flags = p.flags | FL_NOTARGET; // (out of the fight: it goes for a second weapon)
        g.enemy = world;
        vr_agt_step = 2;
        vr_agt_until = time + 10;
        return;
    }
    if(vr_agt_step == 2)
    {
        if(time < vr_agt_until && !g.vr_ag_prop2) { return; }
        VR_AGT_Check(sprintf("dual: re-armed: %g pickups, second %s (left %g):", g.vr_ag_pickups,
            g.vr_ag_prop2 ? g.vr_ag_prop2.netname : "none", g.vr_ag_prop2.vr_ag_left), g.vr_ag_prop2 && g.vr_ag_prop2.vr_ag_left);
        VR_AGT_ClearWeapons();
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        g.health = -10; // (killed outright: a blow from you its blades would parry)
        Killed(g, p);
        VR_AGT_Print(sprintf("dual: killed: health %g, holds %g %g", g.health, g.vr_ag_prop != world, g.vr_ag_prop2 != world));
        vr_agt_step = 3;
        vr_agt_until = time + 1;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(time < vr_agt_until) { return; }
        vr_agt_count = 0;
        entity e = find(world, classname, "thrown_weapon");
        while(e)
        {
            vr_agt_count = vr_agt_count + 1;
            e = find(e, classname, "thrown_weapon");
        }
        // (one landing on you may be in your hand: its auto-pickup)
        if(p.weapon == WID_SWORD || p.weapon == WID_CROWBAR) { vr_agt_count = vr_agt_count + 1; }
        if(p.weapon2 == WID_SWORD || p.weapon2 == WID_CROWBAR) { vr_agt_count = vr_agt_count + 1; }
        VR_AGT_Check(sprintf("dual: died: %g weapons fell (your hands %s, %s):", vr_agt_count, VR_AGT_Name(p.weapon),
            VR_AGT_Name(p.weapon2)), vr_agt_count == 2);
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        VR_AGT_Print(sprintf("dual: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
        cvar_set("vr_agrunt_test", "0");
    }
};

// Test 41: the weapons on the hands (vr_agrunt_ik_err; `vr_agrunt_test_kind` the kind). You unseen; an armed monster with
// an axe in each hand 90 units ahead, turned side on, its AI held: A every frame of its model held 0.15 s (tag = the
// frame), its weapons in their ready poses; B every swing (4 kinds) by the right, the left and both hands (tags 200 +
// 8 x (kind + 4 x hand) + the swing's eighth); C a second one with one axe in both hands, every swing (tags 400 + 8 x
// kind + eighth). The engine counts each drawn frame's hands' distances to their weapons under the tag
// (vr_agrunt_ik_tag) and prints them at the end (agerr lines). scratch1 N: hold tag N (an eyeshot), no end.
float vr_agt_sweep_n;  // its frames: the kind's model's
entity vr_agt_sweep2;  // C's monster
void(entity g, float a, float base) VR_AGT_SweepSwing =
{
    g.vr_ag_act = VR_AGP_SWING;
    g.vr_ag_act_kind = a & 3;
    g.vr_ag_act_hand = floor(a / 4);
    if(!g.vr_ag_prop)
    {
        // (its fists, scratch2 1: every punch, a jab, a cross, the hooks, an uppercut, in turn)
        g.vr_ag_act = VR_AGP_PUNCH;
        g.vr_ag_act_kind = a - floor(a / 5) * 5;
        g.vr_ag_act_hand = 0;
    }
    g.vr_ag_act_hit = 2; // (no blow)
    g.vr_ag_act_hitl = 1;
    g.vr_ag_act_tick = time;
    g.vr_ag_readyw = 0.7;
    float len = VR_AGP_Length(g);
    float s = time - g.vr_ag_act_at;
    g.frame = VR_AM_StandFrame(g, floor(s * 10));
    cvar_set("vr_agrunt_ik_tag", ftos(base + a * 8 + min(7, floor(s / len * 8))));
};

void() VR_AGTest_Sweep =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    float hold = cvar("scratch1");
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        p.flags = p.flags | FL_NOTARGET;
        if(vr_agt_grunt && vr_agt_grunt.classname != "" && vr_agt_grunt.health > 0) { VR_AGT_Remove(vr_agt_grunt); } // (a rerun's)
        if(vr_agt_sweep2 && vr_agt_sweep2.classname != "" && vr_agt_sweep2.health > 0) { VR_AGT_Remove(vr_agt_sweep2); }
        float t43 = cvar_hget(cvarh_vr_agrunt_test) == 43; // (43: a shotgun and a nail gun, every frame only)
        // (eyeshots: scratch2 its right hand's weapon id + 1, scratch3 its left's + 1 (-1 none), scratch4 how far,
        // temp1 its turn from facing you (degrees); 0 each: an axe in each hand (43: a shotgun and a nail gun), 90 units,
        // side on)
        float w1 = cvar("scratch2") ? cvar("scratch2") - 1 : (t43 ? WID_SHOTGUN : WID_AXE);
        if(cvar("scratch2") < 0)
        {
            w1 = -1; // (scratch2 -1: its fists, every punch: VR_AGT_SweepSwing)
        }
        float w2 = cvar("scratch3") ? cvar("scratch3") - 1 : (t43 ? WID_NAILGUN : WID_AXE);
        vr_agt_weapon2 = w2 >= 0 ? w2 : 0;
        vr_agt_grunt = VR_AGT_Spawn(w1, cvar("scratch4") ? cvar("scratch4") : 90);
        vr_agt_weapon2 = 0;
        g = vr_agt_grunt;
        g.angles_y = vectoyaw(p.origin - g.origin) + (cvar("temp1") ? cvar("temp1") : 90); // (side on: its right hand towards you)
        vr_agt_sweep2 = VR_AGT_Spawn(WID_AXE, -200); // (behind you, not drawn, until its turn)
        vr_agt_sweep2.angles_y = g.angles_y;
        vr_agt_sweep_n = cvar("vr_agrunt_test_kind") == 1 ? 108 : 120;
        localcmd("vr_agrunt_ik_err\n"); // (counted afresh)
        VR_AGT_Print(sprintf("sweep: %s holds %s and %s; the second %s", g.classname, g.vr_ag_prop.netname,
            g.vr_ag_prop2 ? g.vr_ag_prop2.netname : "nothing", vr_agt_sweep2.vr_ag_prop.netname));
        vr_agt_step = 1;
        vr_agt_index = 0;
        vr_agt_until = time + 0.15;
        return;
    }
    entity h = vr_agt_sweep2;
    g.nextthink = time + 1; // (held: its AI stands)
    h.nextthink = time + 1;
    g.enemy = world;
    h.enemy = world;
    float tag = -1;
    if(hold >= 200 && vr_agt_step == 1)
    {
        vr_agt_step = 2; // (held further on: straight there)
    }
    if(hold >= 400 && vr_agt_step == 2)
    {
        vector at0 = g.origin;
        setorigin(g, h.origin);
        setorigin(h, at0);
        vr_agt_step = 3;
    }
    if(vr_agt_step == 1)
    {
        // A: every frame
        if(hold > 0 && hold < 200)
        {
            vr_agt_index = hold;
            if(time >= vr_agt_until)
            {
                VR_AGT_Print(sprintf("sweep: held: %s health %g frame %g model %s at %v", g.classname, g.health, g.frame,
                    g.model, g.origin));
            }
            vr_agt_until = time + 1;
        }
        g.vr_ag_act = 0;
        g.frame = vr_agt_index;
        tag = vr_agt_index;
        if(time >= vr_agt_until)
        {
            vr_agt_index = vr_agt_index + 1;
            vr_agt_until = time + 0.15;
            if(vr_agt_index >= vr_agt_sweep_n)
            {
                vr_agt_step = cvar_hget(cvarh_vr_agrunt_test) == 43 ? 4 : 2; // (43: the guns' frames alone)
                vr_agt_index = 0;
                g.vr_ag_act_at = time;
            }
        }
    }
    if(vr_agt_step == 2 || vr_agt_step == 3)
    {
        // B, C: every swing
        entity s = vr_agt_step == 2 ? g : h;
        float base = vr_agt_step == 2 ? 200 : 400;
        float count = vr_agt_step == 2 ? 12 : 4;
        if(hold >= base && hold < base + count * 8)
        {
            // (held at that eighth of that swing)
            vr_agt_index = floor((hold - base) / 8);
            s.vr_ag_act_kind = vr_agt_index & 3;
            s.vr_ag_act_hand = floor(vr_agt_index / 4);
            VR_AGT_SweepSwing(s, vr_agt_index, base); // (its attack's kind and length: a punch's with its fists)
            float len0 = VR_AGP_Length(s);
            s.vr_ag_act_at = time - len0 * ((hold - base - vr_agt_index * 8) + 0.5) / 8;
        }
        VR_AGT_SweepSwing(s, vr_agt_index, base);
        if(time - s.vr_ag_act_at >= VR_AGP_Length(s))
        {
            vr_agt_index = vr_agt_index + 1;
            s.vr_ag_act_at = time;
            if(vr_agt_index >= count)
            {
                s.vr_ag_act = 0;
                vr_agt_index = 0;
                vr_agt_step = vr_agt_step + 1;
                h.vr_ag_act_at = time;
                if(vr_agt_step == 3)
                {
                    vector at = g.origin; // (the two change places)
                    setorigin(g, h.origin);
                    setorigin(h, at);
                }
            }
        }
        return;
    }
    if(vr_agt_step == 4)
    {
        cvar_set("vr_agrunt_ik_tag", "-1");
        localcmd("vr_agrunt_ik_err\n");
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        VR_AGT_Print("sweep: done");
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    cvar_set("vr_agrunt_ik_tag", ftos(tag));
};

// Test 42: dual-wielded guns (`vr_agrunt_test_kind` the kind; ROUND22.md "Armed monsters: dual-wielded guns"). A the
// pairs: gun pairs held, the big weapons and blade + gun refused (the left hand empty); B the penalty: a single shotgun
// and two shotguns fire 40 shots each at you (its kick zeroed each: spread alone; vr_agrunt_aim_error 4 meanwhile), the
// pellets' and the aims' mean angles off their lines (ratio ~ 1 / vr_2h_spread_reduction); C the kick: two nail guns and
// one, six shots 0.1 s apart (the kick left at each shot), two super shotguns' one shot; D a fight: two nail guns
// hunting you (god), their clips short: both fire, a reload; killed, both fall; E the pickup option: a shotgun alone,
// you unseen, a nail gun lying near it: vr_agrunt_dual_pickup 0 it leaves it, 1 it takes it in its left hand.
float vr_agt_keep_err;
float vr_agt_keep_pickup;
entity vr_agt_g2;
float vr_agt_kick_sum;
float vr_agt_kick_sum1;
float vr_agt_keep_heft;

entity(float w1, float w2, float dist) VR_AGT_SpawnPair =
{
    vr_agt_weapon2 = w2;
    entity g = VR_AGT_Spawn(w1, dist);
    vr_agt_weapon2 = 0;
    return g;
};

void(entity g) VR_AGT_RemovePair =
{
    if(g.vr_ag_prop2)
    {
        g.vr_ag_prop2.weaponinst = world;
        remove(g.vr_ag_prop2);
        g.vr_ag_prop2 = world;
    }
    VR_AGT_Remove(g);
};

// A pair spawned: held both or not (`want`).
void(float w1, float w2, float want) VR_AGT_PairCheck =
{
    entity g = VR_AGT_SpawnPair(w1, w2, 400);
    float both = g.vr_ag_prop != world && g.vr_ag_prop2 != world;
    VR_AGT_Check(sprintf("guns: %s + %s: %s (left %s):", VR_AGT_Name(w1), VR_AGT_Name(w2), both ? "both held" : "one",
        g.vr_ag_prop2 ? g.vr_ag_prop2.netname : "none"), both == want);
    VR_AGT_RemovePair(g);
};

// `n` shots of `g`'s gun `pr` at you, the kick zeroed each; the means off (pellets, aims) in vr_agt_kick_sum, _sum1.
void(entity g, entity pr, float n) VR_AGT_Volley =
{
    entity oldself = self;
    self = g;
    g.enemy = vr_agt_player;
    vr_fire_dev_sum = 0;
    vr_fire_dev_n = 0;
    float aims = 0;
    float k;
    for(k = 0; k < n; k++)
    {
        pr.weaponinst.wi_clip = 8;
        pr.vr_ag_hk = 0;
        VR_AG_ShootProp(pr);
        aims = aims + vr_ag_last_aimdev;
    }
    self = oldself;
    vr_agt_kick_sum = vr_fire_dev_n > 0 ? vr_fire_dev_sum / vr_fire_dev_n : 0;
    vr_agt_kick_sum1 = aims / n;
};

void() VR_AGTest_DualGuns =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        // A: the pairs
        VR_AGT_PairCheck(WID_SHOTGUN, WID_SHOTGUN, TRUE);
        VR_AGT_PairCheck(WID_SHOTGUN, WID_NAILGUN, TRUE);
        VR_AGT_PairCheck(WID_SUPER_SHOTGUN, WID_GRENADE_LAUNCHER, TRUE);
        VR_AGT_PairCheck(WID_SUPER_NAILGUN, WID_LIGHTNING, TRUE);
        VR_AGT_PairCheck(WID_SHOTGUN, WID_ROCKET_LAUNCHER, FALSE);
        VR_AGT_PairCheck(WID_ROCKET_LAUNCHER, WID_SHOTGUN, FALSE);
        VR_AGT_PairCheck(WID_SHOTGUN, WID_CHAINSAW, FALSE);
        VR_AGT_PairCheck(WID_SHOTGUN, WID_AXE, FALSE);
        VR_AGT_PairCheck(WID_AXE, WID_SHOTGUN, FALSE);
        VR_AGT_PairCheck(WID_AXE, WID_SWORD, TRUE);
        // (the big weapons' flag itself: the packs' Mjolnir and Laser Cannon whether or not their data is here)
        VR_AGT_Check(sprintf("guns: the big ones pair: rocket launcher %g, Laser Cannon %g, chainsaw %g, Mjolnir %g, AD's "
            "rocket launcher %g:", VR_AG_Pairable(WID_ROCKET_LAUNCHER, 0), VR_AG_Pairable(WID_LASER_CANNON, 0),
            VR_AG_Pairable(WID_CHAINSAW, 0), VR_AG_Pairable(WID_MJOLNIR, 0), VR_AG_Pairable(WID_ADROCKET, 0)),
            !VR_AG_Pairable(WID_ROCKET_LAUNCHER, 0) && !VR_AG_Pairable(WID_LASER_CANNON, 0) &&
            !VR_AG_Pairable(WID_CHAINSAW, 0) && !VR_AG_Pairable(WID_MJOLNIR, 0) && !VR_AG_Pairable(WID_ADROCKET, 0));
        VR_AGT_Check(sprintf("guns: these pair: grenade launcher %g, thunderbolt %g, proximity gun %g, AD's plasma %g:",
            VR_AG_Pairable(WID_GRENADE_LAUNCHER, 0), VR_AG_Pairable(WID_LIGHTNING, 0), VR_AG_Pairable(WID_PROXIMITY_GUN, 0),
            VR_AG_Pairable(WID_ADPLASMA, 0)), VR_AG_Pairable(WID_GRENADE_LAUNCHER, 0) && VR_AG_Pairable(WID_LIGHTNING, 0));
        // B: the penalty
        float keepPen = cvar("vr_agrunt_dual_penalty");
        vr_agt_keep_err = cvar("vr_agrunt_aim_error");
        cvar_set("vr_agrunt_aim_error", "4");
        p.flags = p.flags | FL_GODMODE;
        // (Side by side, 15 degrees either way from you: on one spot each is in the other's line of fire, its shots held
        // for that friend, vr_agrunt_friendly_care.)
        p.angles_y = p.angles_y - 15;
        entity a = VR_AGT_Spawn(WID_SHOTGUN, 220);
        p.angles_y = p.angles_y + 30;
        entity b = VR_AGT_SpawnPair(WID_SHOTGUN, WID_SHOTGUN, 220);
        p.angles_y = p.angles_y - 15;
        float heldBefore = vr_ag_ff_held;
        a.takedamage = DAMAGE_NO; // (side by side: the other's pellets)
        b.takedamage = DAMAGE_NO;
        VR_AGT_Volley(a, a.vr_ag_prop, 40);
        float sp1 = vr_agt_kick_sum;
        float aim1 = vr_agt_kick_sum1;
        VR_AGT_Volley(b, b.vr_ag_prop, 20);
        float sp2 = vr_agt_kick_sum;
        float aim2 = vr_agt_kick_sum1;
        VR_AGT_Volley(b, b.vr_ag_prop2, 20);
        sp2 = (sp2 + vr_agt_kick_sum) * 0.5;
        aim2 = (aim2 + vr_agt_kick_sum1) * 0.5;
        float want = 1 + keepPen * (1 / max(0.05, cvar("vr_2h_spread_reduction")) - 1);
        VR_AGT_Check(sprintf("guns: penalty %g: pellets %.2f deg off (one gun) vs %.2f (two): x%.2f (want %.2f), %g shots "
            "held for a friend:", keepPen, sp1, sp2, sp2 / max(0.001, sp1), want, vr_ag_ff_held - heldBefore),
            fabs(sp2 / max(0.001, sp1) - want) < 0.15 * want && vr_ag_ff_held == heldBefore);
        VR_AGT_Check(sprintf("guns: aims %.2f deg off (one gun) vs %.2f (two): x%.2f (want %.2f):", aim1, aim2,
            aim2 / max(0.001, aim1), want), fabs(aim2 / max(0.001, aim1) - want) < 0.25 * want);
        cvar_set("vr_agrunt_dual_penalty", "0");
        VR_AGT_Volley(b, b.vr_ag_prop, 40);
        VR_AGT_Check(sprintf("guns: penalty 0: two shotguns' pellets %.2f deg off (one gun %.2f):", vr_agt_kick_sum, sp1),
            fabs(vr_agt_kick_sum / max(0.001, sp1) - 1) < 0.15);
        cvar_set("vr_agrunt_dual_penalty", ftos(keepPen));
        cvar_set("vr_agrunt_aim_error", ftos(vr_agt_keep_err));
        p.flags = p.flags - (p.flags & FL_GODMODE);
        VR_AGT_Remove(a);
        VR_AGT_RemovePair(b);
        VR_AGT_Print(sprintf("guns: kicks (deg/s a shot in one hand): shotgun %.0f, super shotgun %.0f, nailgun %.0f, super "
            "nailgun %.0f, grenade launcher %.0f, thunderbolt %.0f", weaponheft(WeaponIdToModel(WID_SHOTGUN, 0), 1),
            weaponheft(WeaponIdToModel(WID_SUPER_SHOTGUN, 0), 1), weaponheft(WeaponIdToModel(WID_NAILGUN, 0), 1),
            weaponheft(WeaponIdToModel(WID_SUPER_NAILGUN, 0), 1), weaponheft(WeaponIdToModel(WID_GRENADE_LAUNCHER, 0), 1),
            weaponheft(WeaponIdToModel(WID_LIGHTNING, 0), 1)));
        // C: the kick (the dual-wieldable guns are all Heft 0 but AD's Widowmaker: the super nail gun's made 0.5 here,
        // the super nail gun's slot 5's Weapon Weights setting)
        vr_agt_keep_heft = cvar("vr_wofs_w_heft_05");
        cvar_set("vr_wofs_w_heft_05", "0.5");
        vr_agt_grunt = VR_AGT_SpawnPair(WID_SUPER_NAILGUN, WID_SUPER_NAILGUN, 220);
        p.angles_y = p.angles_y + 25; // (side by side: neither in the other's line of fire)
        vr_agt_g2 = VR_AGT_Spawn(WID_SUPER_NAILGUN, 260);
        p.angles_y = p.angles_y - 25;
        vr_agt_grunt.takedamage = DAMAGE_NO;
        vr_agt_g2.takedamage = DAMAGE_NO;
        p.flags = p.flags | FL_GODMODE;
        vr_agt_kick_sum = 0;
        vr_agt_kick_sum1 = 0;
        vr_agt_index = 0;
        vr_agt_until = time;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        g.nextthink = time + 1;
        vr_agt_g2.nextthink = time + 1;
        if(time < vr_agt_until) { return; }
        vr_agt_until = time + 0.1;
        entity oldself = self;
        self = g;
        g.enemy = p;
        g.vr_ag_prop.weaponinst.wi_clip = 50;
        VR_AG_ShootProp(g.vr_ag_prop);
        float k2 = vr_ag_last_kick;
        self = vr_agt_g2;
        vr_agt_g2.enemy = p;
        vr_agt_g2.vr_ag_prop.weaponinst.wi_clip = 50;
        VR_AG_ShootProp(vr_agt_g2.vr_ag_prop);
        float k1 = vr_ag_last_kick;
        self = oldself;
        VR_AGT_Print(sprintf("guns: kick: shot %g: two super nailguns' aim up %.2f deg at it (%.2f after), one's %.2f",
            vr_agt_index + 1, k2, g.vr_ag_prop.vr_ag_hk, k1));
        vr_agt_kick_sum = vr_agt_kick_sum + k2;
        vr_agt_kick_sum1 = vr_agt_kick_sum1 + k1;
        vr_agt_index = vr_agt_index + 1;
        if(vr_agt_index < 6) { return; }
        float kickNow = weaponheft(WeaponIdToModel(WID_SUPER_NAILGUN, 0), 1);
        cvar_set("vr_wofs_w_heft_05", ftos(vr_agt_keep_heft));
        VR_AGT_Check(sprintf("guns: kick (%.0f deg/s at Heft 0.5): two super nailguns' aims up %.2f deg in all, one's %.2f:",
            kickNow, vr_agt_kick_sum, vr_agt_kick_sum1), vr_agt_kick_sum1 == 0 && vr_agt_kick_sum > 0);
        // ... recovered before an aimed shot a second later (the single-shot guns' pace)
        VR_AGT_RemovePair(g);
        VR_AGT_Remove(vr_agt_g2);
        // D: a fight
        vr_agt_grunt = VR_AGT_SpawnPair(WID_NAILGUN, WID_SHOTGUN, 200);
        g = vr_agt_grunt;
        g.vr_ag_prop.weaponinst.wi_clip = 6;
        g.vr_ag_prop2.weaponinst.wi_clip = 2;
        g.vr_ag_reserve2 = 8;
        VR_AGT_Hunt(g);
        vr_agt_step = 2;
        vr_agt_until = time + 12;
        return;
    }
    if(vr_agt_step == 2)
    {
        entity pa = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop2 : g.vr_ag_prop;
        entity pb = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : g.vr_ag_prop2;
        if(time < vr_agt_until && (g.vr_ag_reloads < 1 || pa.vr_ag_total_shots < 1 || pb.vr_ag_total_shots < 1)) { return; }
        VR_AGT_Check(sprintf("guns: fight: right %s %g shots, left %s %g shots, %g reloads, holds %g:", pa.netname,
            pa.vr_ag_total_shots, pb.netname, pb.vr_ag_total_shots, g.vr_ag_reloads, (g.vr_ag_prop != world) + (g.vr_ag_prop2 != world)),
            pa.vr_ag_total_shots >= 1 && pb.vr_ag_total_shots >= 1 && g.vr_ag_reloads >= 1 && g.vr_ag_prop2);
        VR_AGT_ClearWeapons();
        g.health = -10;
        Killed(g, p);
        vr_agt_step = 3;
        vr_agt_until = time + 1;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(time < vr_agt_until) { return; }
        vr_agt_count = 0;
        entity e = find(world, classname, "thrown_weapon");
        while(e)
        {
            vr_agt_count = vr_agt_count + 1;
            e = find(e, classname, "thrown_weapon");
        }
        VR_AGT_Check(sprintf("guns: died: %g weapons fell:", vr_agt_count), vr_agt_count == 2);
        p.flags = p.flags - (p.flags & FL_GODMODE);
        // E: the pickup option, off
        VR_AGT_ClearWeapons();
        p.flags = p.flags | FL_NOTARGET;
        vr_agt_keep_pickup = cvar("vr_agrunt_dual_pickup");
        cvar_set("vr_agrunt_dual_pickup", "0");
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 260);
        p.angles_y = p.angles_y + 25;
        entity src = VR_AGT_Spawn(WID_NAILGUN, 220); // (a grunt dropping its nail gun: it lies there)
        p.angles_y = p.angles_y - 25;
        VR_AG_DropHeld(src, '0 0 0');
        VR_AGT_Remove(src);
        vr_agt_step = 4;
        vr_agt_until = time + 6;
        return;
    }
    if(vr_agt_step == 4)
    {
        if(time < vr_agt_until && !g.vr_ag_prop2) { return; }
        VR_AGT_Check(sprintf("guns: pickup 0: left hand %s:", g.vr_ag_prop2 ? g.vr_ag_prop2.netname : "empty"), !g.vr_ag_prop2);
        cvar_set("vr_agrunt_dual_pickup", "1");
        vr_agt_step = 5;
        vr_agt_until = time + 12;
        return;
    }
    if(vr_agt_step == 5)
    {
        if(time < vr_agt_until && !(g.vr_ag_prop2 && g.vr_ag_took_at <= 0)) { return; }
        entity l = g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : g.vr_ag_prop2;
        VR_AGT_Check(sprintf("guns: pickup 1: %g pickups, left hand %s:", g.vr_ag_pickups, l ? l.netname : "empty"),
            l && l.weapon == WID_NAILGUN);
        cvar_set("vr_agrunt_dual_pickup", ftos(vr_agt_keep_pickup));
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        VR_AGT_Print(sprintf("guns: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
        cvar_set("vr_agrunt_test", "0");
    }
};

// Test 44: a row of dual-gun humanoids ahead, 40 units apart, 300 off (the checklist scene agrunt_dual_guns): grunts with
// two shotguns, two nail guns, a super shotgun and a grenade launcher, enforcers with two super nail guns and a shotgun
// and a thunderbolt.
// Test 45: the arms' contact sheet (vr_agarms; ROUND22.md, "Armed humanoids' arms: posed and skinned"). You unseen, one
// armed monster (`vr_agrunt_test_kind`) `scratch4` units ahead (110), its AI held, through weapon sets scratch2 ..
// scratch3 (VR_AGT_SheetSet: 0 fists .. 13 the chainsaw, 14 two swords, 15 two crowbars) and, for each, the poses
// (VR_AGT_SheetPose: idle, aiming ahead, up, down, left, right, running, taking a weapon up, a pain, a blow), each seen
// from its front and its right side:
// held temp1 seconds (0.7), then a screenshot (`agsheet:` its label), the engine's arm numbers counted under tag set * 20
// + pose * 2 + view meanwhile (vr_agrunt_ik_err prints them at the end). Eyeshots: the run's script sets the camera
// (scratch/ag_sheet.sh: vr_mock_camera, vr_eyeshot 1). `scratch1` N > 0: the strike sheet instead (ROUND22.md, "Armed
// humanoids: the body in their strikes"): each pose an eighth of attack kind N - 1 (a swing's, a punch's) at a target
// ahead, side on, without its body's turn (view 0: vr_agrunt_strike_torso and _shoulder 0) and with it (view 1: theirs).
float vr_agt_sheet_set;
float vr_agt_sheet_pose;
float vr_agt_sheet_view;
float vr_agt_sheet_at;
entity vr_agt_sheet_tgt;
float vr_agt_sheet_w2; // (VR_AGT_SheetSet's left hand's weapon: -1 none)
float vr_agt_sheet_torso;    // (the strike sheet: vr_agrunt_strike_torso and _shoulder as they were, put back after)
float vr_agt_sheet_shoulder;
float vr_agt_sheet_take; // (the take sheet's phase now: below 1 the share of the way down, 1 the grab, above it 1 + the lift's share)
float(float s) VR_AGT_SheetSet =
{
    vr_agt_sheet_w2 = -1;
    if(s == 0) { return -1; }
    if(s == 1) { return WID_SWORD; }
    if(s == 2) { return WID_AXE; }
    if(s == 3) { return WID_SHOTGUN; }
    if(s == 4) { return WID_SUPER_SHOTGUN; }
    if(s == 5) { return WID_NAILGUN; }
    if(s == 6) { return WID_SUPER_NAILGUN; }
    if(s == 7) { return WID_GRENADE_LAUNCHER; }
    if(s == 8) { return WID_ROCKET_LAUNCHER; }
    if(s == 9) { return WID_LIGHTNING; }
    if(s == 10) { return WID_ENFRIFLE; }
    if(s == 11) { vr_agt_sheet_w2 = WID_SHOTGUN; return WID_SHOTGUN; }
    if(s == 12) { vr_agt_sheet_w2 = WID_AXE; return WID_AXE; }
    if(s == 14) { vr_agt_sheet_w2 = WID_SWORD; return WID_SWORD; } // (the one-handed melee weapons left-handed too)
    if(s == 15) { vr_agt_sheet_w2 = WID_CROWBAR; return WID_CROWBAR; }
    return WID_CHAINSAW;
};
string(float p) VR_AGT_SheetPoseName =
{
    if(p == 0) { return "idle"; }
    if(p == 1) { return "aim ahead"; }
    if(p == 2) { return "aim up"; }
    if(p == 3) { return "aim down"; }
    if(p == 4) { return "aim left"; }
    if(p == 5) { return "aim right"; }
    if(p == 6) { return "run"; }
    if(p == 7) { return "take up"; }
    if(p == 8) { return "pain"; }
    return "blow";
};

// `g` held in pose `pose` this frame (its target, its frame, its attack's phase).
void(entity g, float pose) VR_AGT_SheetPose =
{
    VR_AM_Row(g.vr_ag_kind);
    entity p = g.vr_ag_prop;
    g.nextthink = time + 1;
    g.velocity = '0 0 0';
    g.vr_ag_act = 0;
    g.vr_ag_aim = '0 0 0';
    g.enemy = world;
    g.frame = VR_AM_StandFrame(g, 0);
    makevectors([0, g.angles_y, 0]);
    vector chest = g.origin + '0 0 12';
    vector t = chest + v_forward * 200;
    if(pose == 2) { t = chest + v_forward * 140 + '0 0 140'; }
    if(pose == 3) { t = chest + v_forward * 170 - '0 0 100'; }
    if(pose == 4) { t = chest + v_forward * 150 - v_right * 130; }
    if(pose == 5) { t = chest + v_forward * 150 + v_right * 130; }
    setorigin(vr_agt_sheet_tgt, t);
    if((pose >= 1 && pose <= 5) || pose == 9)
    {
        g.enemy = vr_agt_sheet_tgt;
        if(p && VR_AG_Class(p.weapon) != VR_AG_CLASS_MELEE)
        {
            g.vr_ag_aim = normalize(t - p.origin);
        }
    }
    float sheet = cvar("scratch1") <= 0; // (the strike sheet: its eighths all on its stand frame)
    if(pose == 6 && sheet)
    {
        g.frame = vr_am_f_run + (floor(time * 10) & 7);
    }
    if(pose == 7 && p && sheet)
    {
        // (its hand on a weapon lying before it, down on its knee: the take's lowest frame, about to close)
        g.frame = vr_am_f_take + max(1, vr_am_n_take) - 1;
        g.vr_ag_took_at = time - 5; // (its hand just closed on it: still where it lay)
        g.vr_ag_took_grab = time - 0.001;
        float takeSheet = cvar("scratch1") < 0;
        if(takeSheet && vr_agt_sheet_take < 1)
        {
            // (the take sheet: on its way down, this share of it; its frames' that far)
            float tk = max(1, vr_am_n_take) * VR_AG_TAKE_FRAME;
            g.frame = vr_am_f_take + min(max(1, vr_am_n_take) - 1, floor(vr_agt_sheet_take * max(1, vr_am_n_take)));
            g.vr_ag_took_at = time - vr_agt_sheet_take * tk;
            g.vr_ag_took_grab = g.vr_ag_took_at + tk;
            g.think = vr_ag_takestep; // (taking: its reach not cut short, VR_AG_PlaceProp; held, its nextthink ahead)
        }
        else if(takeSheet && vr_agt_sheet_take > 1)
        {
            g.vr_ag_took_grab = time - (vr_agt_sheet_take - 1) * VR_AG_LIFT_TIME; // (... on its way up)
        }
        g.vr_ag_took_org = g.origin + v_forward * 18 + v_right * 4 + '0 0 -22';
        g.vr_ag_took_fw = v_forward;
        g.vr_ag_took_up = '0 0 1';
    }
    else
    {
        g.vr_ag_took_at = 0;
    }
    if(pose == 8 && sheet)
    {
        g.frame = g.vr_ag_kind == VR_AM_ENFORCER ? 67 : 41;
    }
    float strike = cvar("scratch1"); // (the strike sheet: its kind + 1; each pose an eighth of it)
    if(strike > 0)
    {
        float cs = p ? VR_AG_Class(p.weapon) : VR_AG_CLASS_FISTS;
        if(cs == VR_AG_CLASS_MELEE || cs == VR_AG_CLASS_FISTS)
        {
            g.enemy = vr_agt_sheet_tgt;
            setorigin(vr_agt_sheet_tgt, chest + v_forward * 40);
            g.vr_ag_act = cs == VR_AG_CLASS_FISTS ? VR_AGP_PUNCH : p.weapon == WID_CHAINSAW ? VR_AGP_SAW : VR_AGP_SWING;
            g.vr_ag_act_kind = strike - 1;
            g.vr_ag_act_hand = 0;
            g.vr_ag_act_hit = 2;
            g.vr_ag_act_hitl = 1;
            g.vr_ag_act_tick = time;
            g.vr_ag_act_var = 0.5;
            g.vr_ag_readyw = 0.7;
            g.vr_ag_act_at = time - VR_AGP_Length(g) * (pose + 0.5) / 8;
        }
        g.vr_ag_took_at = 0;
        return;
    }
    if(pose == 9)
    {
        float c = p ? VR_AG_Class(p.weapon) : VR_AG_CLASS_FISTS;
        if(c == VR_AG_CLASS_MELEE || c == VR_AG_CLASS_FISTS)
        {
            // (held at its blow: a swing's strike, a cross's punch)
            g.vr_ag_act = c == VR_AG_CLASS_FISTS ? VR_AGP_PUNCH : p.weapon == WID_CHAINSAW ? VR_AGP_SAW : VR_AGP_SWING;
            g.vr_ag_act_kind = c == VR_AG_CLASS_FISTS ? 1 : 0;
            g.vr_ag_act_hand = 0;
            g.vr_ag_act_hit = 2;
            g.vr_ag_act_hitl = 1;
            g.vr_ag_act_tick = time;
            VR_AGP_Times(g);
            g.vr_ag_act_at = time - vr_agp_tw - (c == VR_AG_CLASS_FISTS ? 0.8 : 0.6) * vr_agp_ts;
        }
        else
        {
            // (a shot's kick, a moment after it)
            g.vr_ag_kick_at = time - 0.05;
            if(p) { p.vr_ag_kick_at = time - 0.05; }
            if(g.vr_ag_prop2) { g.vr_ag_prop2.vr_ag_kick_at = time - 0.05; }
        }
    }
};

void() VR_AGTest_Sheet =
{
    entity pl = vr_agt_player;
    float first = cvar("scratch2");
    float last = cvar("scratch3") >= first ? cvar("scratch3") : first;
    float hold = cvar("temp1") > 0 ? cvar("temp1") : 0.7;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        pl.flags = pl.flags | FL_NOTARGET;
        if(vr_agt_grunt && vr_agt_grunt.classname != "" && vr_agt_grunt.health > 0) { VR_AGT_Remove(vr_agt_grunt); }
        if(!vr_agt_sheet_tgt) { vr_agt_sheet_tgt = spawn(); vr_agt_sheet_tgt.classname = "vr_agt_sheet_target"; }
        vr_agt_sheet_set = first;
        vr_agt_sheet_pose = 0;
        vr_agt_sheet_view = 0;
        vr_agt_sheet_torso = cvar("vr_agrunt_strike_torso");
        vr_agt_sheet_shoulder = cvar("vr_agrunt_strike_shoulder");
        localcmd("vr_agrunt_ik_err\n"); // (counted afresh)
        vr_agt_step = 1;
    }
    if(vr_agt_step == 1)
    {
        // A set's monster.
        if(vr_agt_grunt && vr_agt_grunt.classname != "" && vr_agt_grunt.health > 0) { VR_AGT_Remove(vr_agt_grunt); }
        float w1 = VR_AGT_SheetSet(vr_agt_sheet_set);
        vr_agt_weapon2 = vr_agt_sheet_w2 >= 0 ? vr_agt_sheet_w2 : 0;
        vr_agt_grunt = VR_AGT_Spawn(w1, cvar("scratch4") ? cvar("scratch4") : 110);
        vr_agt_weapon2 = 0;
        vr_agt_sheet_at = time;
        vr_agt_step = 2;
    }
    entity g = vr_agt_grunt;
    float tag = vr_agt_sheet_set * 20 + vr_agt_sheet_pose * (cvar("scratch1") < 0 ? 3 : 2) + vr_agt_sheet_view; // (the take sheet: 3 views)
    float strike = cvar("scratch1") > 0; // (the strike sheet: side on, each eighth without (view 0) and with its body)
    // (the take sheet, scratch1 -1: its poses the take's phases (way down 0.45, 0.8, the grab, half up), each from its
    // front, its right side and three-quarters: vr_aglegs)
    float takes = cvar("scratch1") < 0;
    vr_agt_sheet_take = vr_agt_sheet_pose == 0 ? 0.45 : vr_agt_sheet_pose == 1 ? 0.8 : vr_agt_sheet_pose == 2 ? 1 : 1.5;
    g.angles_y = vectoyaw(pl.origin - g.origin) + ((vr_agt_sheet_view || strike) ? (vr_agt_sheet_view == 2 ? 45 : 90) : 0) + 0.01;
    if(strike)
    {
        cvar_set("vr_agrunt_strike_torso", vr_agt_sheet_view ? ftos(vr_agt_sheet_torso) : "0");
        cvar_set("vr_agrunt_strike_shoulder", vr_agt_sheet_view ? ftos(vr_agt_sheet_shoulder) : "0");
    }
    VR_AGT_SheetPose(g, takes ? 7 : vr_agt_sheet_pose);
    float s = time - vr_agt_sheet_at;
    cvar_set("vr_agrunt_ik_tag", s >= hold * 0.6 ? ftos(tag) : "-1");
    if(vr_agt_step == 2 && s >= hold)
    {
        // (the script's screenshot follows this line: `vr_waitprint 3000 (tag; screenshot`; held 0.2 s more)
        VR_AGT_Print(sprintf("sheet: %s %s, set %g %s: %s, %s (tag %g)", VR_AM_UiKindName(g.vr_ag_kind), g.vr_ag_prop ?
            g.vr_ag_prop.netname : "fists", vr_agt_sheet_set, g.vr_ag_prop2 ? g.vr_ag_prop2.netname : "-",
            strike ? sprintf("strike eighth %g", vr_agt_sheet_pose) : takes ? sprintf("take phase %g", vr_agt_sheet_take) :
            VR_AGT_SheetPoseName(vr_agt_sheet_pose), strike ? (vr_agt_sheet_view ? "body" : "arms only") :
            (vr_agt_sheet_view == 2 ? "three-quarter" : vr_agt_sheet_view ? "side" : "front"), tag));
        vr_agt_step = 3;
        vr_agt_sheet_at = time;
        return;
    }
    if(vr_agt_step == 3 && s >= 0.2)
    {
        vr_agt_step = 2;
        vr_agt_sheet_at = time;
        vr_agt_sheet_view = vr_agt_sheet_view + 1;
        if(vr_agt_sheet_view > (takes ? 2 : 1))
        {
            vr_agt_sheet_view = 0;
            vr_agt_sheet_pose = vr_agt_sheet_pose + 1;
            if(vr_agt_sheet_pose > (strike ? 7 : takes ? 3 : 9))
            {
                vr_agt_sheet_pose = 0;
                vr_agt_sheet_set = vr_agt_sheet_set + 1;
                vr_agt_step = vr_agt_sheet_set > last ? 4 : 1;
            }
        }
    }
    if(vr_agt_step == 4)
    {
        cvar_set("vr_agrunt_ik_tag", "-1");
        localcmd("vr_agrunt_ik_err\n");
        if(vr_agt_grunt && vr_agt_grunt.classname != "" && vr_agt_grunt.health > 0) { VR_AGT_Remove(vr_agt_grunt); }
        pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
        if(strike)
        {
            cvar_set("vr_agrunt_strike_torso", ftos(vr_agt_sheet_torso));
            cvar_set("vr_agrunt_strike_shoulder", ftos(vr_agt_sheet_shoulder));
        }
        VR_AGT_Print("sheet: done");
        cvar_set("vr_agrunt_test", "0");
    }
};

void() VR_AGTest_DualRow =
{
    entity p = vr_agt_player;
    float keepKind = cvar("vr_agrunt_test_kind");
    float w1[5];
    float w2[5];
    w1[0] = WID_SHOTGUN;        w2[0] = WID_SHOTGUN;
    w1[1] = WID_NAILGUN;        w2[1] = WID_NAILGUN;
    w1[2] = WID_SUPER_SHOTGUN;  w2[2] = WID_GRENADE_LAUNCHER;
    w1[3] = WID_SUPER_NAILGUN;  w2[3] = WID_SUPER_NAILGUN;
    w1[4] = WID_SHOTGUN;        w2[4] = WID_LIGHTNING;
    float k;
    for(k = 0; k < 5; k++)
    {
        cvar_set("vr_agrunt_test_kind", k >= 3 ? "1" : "0");
        makevectors(p.angles);
        vector side = v_right;
        side_z = 0;
        vector keep = p.origin;
        setorigin(p, keep + side * ((k - 2) * 40));
        entity r = VR_AGT_SpawnPair(w1[k], w2[k], 300);
        r.angles_y = vectoyaw(keep - r.origin);
        setorigin(p, keep);
    }
    cvar_set("vr_agrunt_test_kind", ftos(keepKind));
    cvar_set("vr_agrunt_test", "0");
};

void() VR_AGPar_Test; // (vr_armedgrunt_parry_test.qc)
void() VR_AGTest_Glitch; // (vr_armedgrunt_glitch_test.qc)
void(float t) VR_AGBT_Frame; // (vr_armedgrunt_bugs_test.qc)
void(float t) VR_AGCT_Frame; // (vr_armedgrunt_close_test.qc)

float vr_agt_running; // the test running (vr_agrunt_test) last frame; 0 none

void() VR_AGTest_Frame =
{
    float t = cvar_hget(cvarh_vr_agrunt_test);
    if(!t)
    {
        if(vr_agt_running)
        {
            // (Every test's last line: the runs end on it, run.sh -QuitOn "agtest: test [0-9]+ over".)
            VR_AGT_Print(sprintf("test %g over: %g passed, %g failed", vr_agt_running, vr_agt_passed, vr_agt_failed));
            vr_agt_running = 0;
        }
        vr_agt_player = world;
        vr_agt_row_made = 0;
        return;
    }
    vr_agt_running = t;
    if(!vr_agt_player)
    {
        vr_agt_player = find(world, classname, "player");
        vr_agt_step = 0;
        vr_agt_index = 0;
        vr_agt_passed = 0;
        vr_agt_failed = 0;
        if(!vr_agt_player)
        {
            return;
        }
    }
    vr_agt_player.health = max(vr_agt_player.health, 500); // (kept alive)
    if(t == 1 || t == 12 || t >= 100)
    {
        if(t >= 100 && vr_agt_step == 0 && vr_agt_index == 0)
        {
            vr_agt_index = t - 100; // (`100+i`: weapon i alone)
        }
        if(t == 12 && vr_agt_step == 0 && vr_agt_index == 0)
        {
            vr_agt_index = VR_AGT_COUNT; // (Arcane Dimensions' weapons)
        }
        VR_AGTest_Weapons();
    }
    else if(t == 11) { VR_AGTest_Blast(); }
    else if(t == 13) { VR_AGTest_IdleRearm(); }
    else if(t == 2) { VR_AGTest_Sequence(); }
    else if(t == 3) { VR_AGTest_Yank(); }
    else if(t == 40) { VR_AGTest_Dual(); }
    else if(t == 41 || t == 43) { VR_AGTest_Sweep(); }
    else if(t == 42) { VR_AGTest_DualGuns(); }
    else if(t == 44) { VR_AGTest_DualRow(); }
    else if(t == 45) { VR_AGTest_Sheet(); } // (the arms' contact sheet: vr_agarms)
    else if(t == 46) { VR_AGTest_Glitch(); } // (the glitch finder: vr_armedgrunt_glitch_test.qc)
    else if(t == 4) { VR_AGTest_State(); }
    else if(t >= 20 && t <= 39) { VR_AGPT_Frame(t); } // (the gameplay tests: vr_armedgrunt_play_test.qc)
    else if(t >= 50 && t <= 59) { VR_AGS_TestFrame(t); } // (shooting the weapon out: vr_armedgrunt_shot_test.qc)
    else if(t == 60) { VR_AGPar_Test(); } // (they parry your blows: vr_armedgrunt_parry_test.qc)
    else if(t >= 61 && t <= 64) { VR_AGBT_Frame(t); } // (pickups drawn, drops up close, friendly fire: vr_armedgrunt_bugs_test.qc)
    else if(t == 65 || t == 66) { VR_AGCT_Frame(t); } // (grab anywhere, never a blow; a lying one reached for: vr_armedgrunt_close_test.qc)
    else if(t >= 6 && t <= 9) // a row of armed grunts ahead, 30 units apart (eyeshots): 6 guns, 7 melee, 8 more guns, 9 the rest
    {
        float base = (t - 6) * 4;
        float k;
        for(k = 0; k < 4; k++)
        {
            float i = base + k + 1;
            if(i <= VR_AGT_COUNT)
            {
                makevectors(vr_agt_player.angles);
                vector side = v_right;
                side_z = 0;
                vector keep = vr_agt_player.origin;
                setorigin(vr_agt_player, keep + side * ((k - 1.5) * 34));
                entity r = VR_AGT_Spawn(VR_AGT_Wid(i < VR_AGT_COUNT ? i : 0), cvar("scratch1") > 0 ? cvar("scratch1") : 110);
                r.angles_y = vectoyaw(keep - r.origin);
                setorigin(vr_agt_player, keep);
            }
        }
        cvar_set("vr_agrunt_test", "0");
    }
    else if(t == 14 || t == 15) // the attacks' eyeshots: a row turned side-on, held at an attack's phase (VR_AGT_PosePhase)
    {
        VR_AGT_PoseRow(t);
    }
    else if(t == 10) // every armed grunt hunts you now
    {
        entity h = find(world, classname, VR_AGT_Classname());
        while(h)
        {
            if(h.vr_ag && h.health > 0)
            {
                VR_AGT_Hunt(h);
            }
            h = find(h, classname, VR_AGT_Classname());
        }
        cvar_set("vr_agrunt_test", "0");
    }
    else if(t == 5) // a few armed grunts ahead, hunting you (eyeshots, save and load)
    {
        VR_AGT_Spawn(WID_SHOTGUN, 160);
        makevectors(vr_agt_player.angles);
        vr_agt_player.angles_y = vr_agt_player.angles_y + 25;
        VR_AGT_Spawn(WID_ROCKET_LAUNCHER, 220);
        vr_agt_player.angles_y = vr_agt_player.angles_y - 50;
        VR_AGT_Spawn(WID_AXE, 130);
        vr_agt_player.angles_y = vr_agt_player.angles_y + 25;
        cvar_set("vr_agrunt_test", "0");
    }
};
