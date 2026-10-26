#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_ammo_test.qc -- armed humanoids' ammo, loot, damage and sounds (vr_armedgrunt.qc; ROUND22.md, "Armed
// humanoids' ammo, loot, damage and sounds"): `vr_agrunt_test 47` in vrtesthall, the player kept alive (no armour).
// Each case: an armed monster with its weapon ahead of the player (a melee one at arm's length, an explosive one far):
// - `agammo: <case> spawn: clip <c>/<size>, reserve <r> (<m> magazines) ...: PASS|FAIL` (a full magazine and at least
//   two more; none for a weapon with no magazine), then its clip set to 1 (a reload to come, its sound).
// - It hunts the player until it has hit him 3 times and reloaded (a gun with a reserve), or 15 s pass (a safety net):
//   `agammo: <case> damage: <h> hits, <raw> a hit before the armed scale, <dmg> after, <s> shots, <r> reloads`.
// - Killed (gibbed): `agammo: <case> loot: backpack <shells> <nails> <rockets> <cells> (+<rogue's>), expected ...:
//   PASS|FAIL`: the ammo left of its weapons' (their reserves; a weapon knocked out of its hand: its reserve kept),
//   or, with none of a real ammo kind, its monster's own (a grunt's 5 shells, an enforcer's 5 cells).
// `agammo: case <i> <name>` opens each case and `agammo: case end` closes it: with vr_debug_sound_log 2 the sounds
// between them (`sndlog: ...`) are its own (Misc/quakevr/agammo_table.py makes the tables). Last: `agammo: done:
// <p> passed, <f> failed`, then `agtest: test 47 over`.
// ----------------------------------------------------------------------------

float vr_agam_i;        // the case
float vr_agam_step;     // 0 spawn, 1 settle, 2 the fight, 3 next
float vr_agam_until;
float vr_agam_hits0;    // vr_agr_pl_hits at the fight's start
float vr_agam_raw0;     // ... vr_agr_pl_raw
float vr_agam_dmg0;     // ... vr_agr_pl_dmg
float vr_agam_reload;   // a reload expected
float vr_agam_disarmed; // the disarm case: its reserve when knocked out of its hand (its ammo kind's)
float vr_agam_disarm_aid;
float vr_agam_passed;
float vr_agam_failed;
string vr_agam_name;
entity vr_agam_g;
float VR_AGAM_COUNT = 36;
float vr_agam_last;     // the cases end before this one
vector vr_agam_org;     // the player's start (held there)
float vr_agam_org_set;

void(string s) VR_AGAM_Print = { bprint(sprintf("agammo: %s\n", s)); };

void(string what, float ok) VR_AGAM_Check =
{
    VR_AGAM_Print(sprintf("%s %s", what, ok ? "PASS" : "FAIL"));
    if(ok) { vr_agam_passed = vr_agam_passed + 1; } else { vr_agam_failed = vr_agam_failed + 1; }
};

// Case `i`'s weapon ("weapon" key; -1 its fists; 0: skipped, Arcane Dimensions' without its data), its left hand's
// (vr_agt_weapon2) and name.
float(float i) VR_AGAM_Code =
{
    vr_agt_weapon2 = 0;
    float ad = VR_ADGun_Available();
    if(i == 0) { vr_agam_name = "fists"; return -1; }
    if(i == 1) { vr_agam_name = "axe"; return WID_AXE; }
    if(i == 2) { vr_agam_name = "sword"; return WID_SWORD; }
    if(i == 3) { vr_agam_name = "crowbar"; return WID_CROWBAR; }
    if(i == 4) { vr_agam_name = "chainsaw"; return WID_CHAINSAW; }
    if(i == 5) { vr_agam_name = "mjolnir"; return WID_MJOLNIR; }
    if(i == 6) { vr_agam_name = "shotgun"; return WID_SHOTGUN; }
    if(i == 7) { vr_agam_name = "super shotgun"; return WID_SUPER_SHOTGUN; }
    if(i == 8) { vr_agam_name = "nailgun"; return WID_NAILGUN; }
    if(i == 9) { vr_agam_name = "super nailgun"; return WID_SUPER_NAILGUN; }
    if(i == 10) { vr_agam_name = "grenade launcher"; return WID_GRENADE_LAUNCHER; }
    if(i == 11) { vr_agam_name = "rocket launcher"; return WID_ROCKET_LAUNCHER; }
    if(i == 12) { vr_agam_name = "thunderbolt"; return WID_LIGHTNING; }
    if(i == 13) { vr_agam_name = "laser cannon"; return WID_LASER_CANNON; }
    if(i == 14) { vr_agam_name = "proximity gun"; return WID_PROXIMITY_GUN; }
    if(i == 15) { vr_agam_name = "burst rifle"; return WID_GRUNTGUN; }
    if(i == 16) { vr_agam_name = "enforcer rifle"; return WID_ENFRIFLE; }
    if(i == 17) { vr_agam_name = "nail pistol"; return WID_NAILPISTOL; }
    if(i == 18) { vr_agam_name = "flare gun"; return WID_FLAREGUN; }
    if(i == 19) { vr_agam_name = "revolver"; return WID_REVOLVER; }
    if(i == 20) { vr_agam_name = "super axe"; return WID_SUPERAXE; }
    if(i == 21) { vr_agam_name = "AD rocket launcher"; return ad ? WID_ADROCKET : 0; }
    if(i == 22) { vr_agam_name = "AD grenade launcher"; return ad ? WID_ADGRENADE : 0; }
    if(i == 23) { vr_agam_name = "AD plasma rifle"; return ad ? WID_ADPLASMA : 0; }
    if(i == 24) { vr_agam_name = "AD defender shotgun"; return ad ? WID_ADDEFENDER : 0; }
    if(i == 25) { vr_agam_name = "AD centurion plasma gun"; return ad ? WID_ADCENTURION : 0; }
    if(i == 26) { vr_agam_name = "AD flamethrower"; return ad ? WID_ADPYRO : 0; }
    if(i == 27) { vr_agam_name = "AD poison sprayer"; return ad ? WID_ADFUMIGATOR : 0; }
    if(i == 28) { vr_agam_name = "AD crossbow"; return ad ? WID_ADCROSSBOW : 0; }
    if(i == 29) { vr_agam_name = "AD shadow axe"; return ad ? VR_AG_CODE_SHADOWAXE : 0; }
    if(i == 30) { vr_agam_name = "AD widowmaker"; return ad ? VR_AG_CODE_WIDOWMAKER : 0; }
    if(i == 31) { vr_agam_name = "AD plasma gun"; return ad ? VR_AG_CODE_PLASMAGUN : 0; }
    if(i == 32) { vr_agam_name = "dual revolver + nail pistol"; vr_agt_weapon2 = WID_NAILPISTOL; return WID_REVOLVER; }
    if(i == 33) { vr_agam_name = "dual shotgun + burst rifle"; vr_agt_weapon2 = WID_GRUNTGUN; return WID_SHOTGUN; }
    if(i == 34) { vr_agam_name = "disarmed shotgun"; return WID_SHOTGUN; }
    if(i == 35) { vr_agam_name = "disarmed burst rifle"; return WID_GRUNTGUN; }
    return 0;
};

// What `p` (a held weapon) has: its line's part, and whether its ammo is right (a full magazine, two more at least).
string vr_agam_line;
float(entity p, float reserve) VR_AGAM_SpawnOk =
{
    if(!p)
    {
        vr_agam_line = "no weapon";
        return TRUE;
    }
    float size = VR_AG_ClipOf(p.weapon, p.weaponinst);
    vr_agam_line = sprintf("%s clip %g/%g, reserve %g (%.1f magazines)", p.netname, p.weaponinst.wi_clip, size, reserve,
        size > 0 ? reserve / size : 0);
    if(size <= 0)
    {
        return reserve == 0;
    }
    // (The chainsaw's engine idles from its spawn: a little of its fuel gone.)
    float full = p.weapon == WID_CHAINSAW ? p.weaponinst.wi_clip >= size - 1 : p.weaponinst.wi_clip == size;
    return full && reserve >= 2 * size;
};

// The backpack's ammo that `g` should drop: its weapons' reserves left (and what it carries of a weapon no longer in
// its hand: its .ammo_* fields), of a real ammo kind; none: its monster's own.
float vr_agam_e[8]; // by AID_* (1..7)
void(entity g) VR_AGAM_Expected =
{
    float k;
    for(k = 0; k < 8; k++) { vr_agam_e[k] = 0; }
    float a1 = g.vr_ag_wid > 0 ? WeaponIdToAmmoId(g.vr_ag_wid, g.vr_ag_wflags) : AID_NONE;
    if(a1 != AID_NONE) { vr_agam_e[a1] = vr_agam_e[a1] + g.vr_ag_reserve; }
    float a2 = (g.vr_ag_prop2 && g.vr_ag_wid2 > 0) ? WeaponIdToAmmoId(g.vr_ag_wid2, g.vr_ag_wflags2) : AID_NONE;
    if(a2 != AID_NONE) { vr_agam_e[a2] = vr_agam_e[a2] + g.vr_ag_reserve2; }
    for(k = 1; k < 8; k++) { vr_agam_e[k] = vr_agam_e[k] + VR_AmmoUtil_GetAmmoCountByAmmoId(g, k); } // (its pockets)
    float total = 0;
    for(k = 1; k < 8; k++) { total = total + vr_agam_e[k]; }
    if(total <= 0)
    {
        VR_AM_Row(g.vr_ag_kind);
        vr_agam_e[vr_am_loot_aid] = vr_am_loot_n;
    }
};

// The backpack dropped just now (world: none).
entity() VR_AGAM_Backpack =
{
    entity e = nextent(world);
    while(e)
    {
        if(e.model == "progs/backpack.mdl")
        {
            return e;
        }
        e = nextent(e);
    }
    return world;
};

// Backpacks lying about go (between cases).
void() VR_AGAM_ClearBackpacks =
{
    entity e = nextent(world);
    while(e)
    {
        entity n = nextent(e);
        if(e.model == "progs/backpack.mdl")
        {
            remove(e);
        }
        e = n;
    }
};

void() VR_AGAM_Begin =
{
    vr_agam_i = max(0, rint(cvar("scratch2")) - 1); // (scratch2 n: from case n-1; scratch3 k: k cases, 1 that one alone)
    vr_agam_last = cvar("scratch3") > 0 ? min(VR_AGAM_COUNT, vr_agam_i + rint(cvar("scratch3"))) : VR_AGAM_COUNT;
    vr_agam_step = 0;
    vr_agam_passed = 0;
    vr_agam_failed = 0;
    vr_agam_org_set = FALSE;
};

void() VR_AGAM_Frame =
{
    entity g = vr_agam_g;
    entity pl = vr_agt_player;
    pl.armorvalue = 0; // (its damage as dealt: no armour's share)
    pl.armortype = 0;
    if(vr_agam_org_set)
    {
        // (Held where it started: a blast's push sent the player onto the hall's buttons, a map restart.)
        setorigin(pl, vr_agam_org);
        pl.velocity = '0 0 0';
    }
    else
    {
        vr_agam_org = pl.origin;
        vr_agam_org_set = TRUE;
    }
    if(vr_agam_step == 0)
    {
        if(vr_agam_passed + vr_agam_failed == 0)
        {
            cvar_set("vr_debug_sound_log", "2");
            // (The hall's shootable buttons: a stray pellet on one restarted the map.)
            entity bt = find(world, classname, "func_button");
            while(bt)
            {
                bt.takedamage = DAMAGE_NO;
                bt = find(bt, classname, "func_button");
            }
            VR_AGAM_Print(sprintf("start: vr_agrunt_damage_scale %g, vr_agrunt_reserve %g, skill %g, AD %g",
                cvar_hget(cvarh_vr_agrunt_damage_scale), cvar_hget(cvarh_vr_agrunt_reserve), skill, VR_ADGun_Available()));
        }
        if(vr_agam_i >= vr_agam_last)
        {
            cvar_set("vr_debug_sound_log", "0");
            VR_AGAM_Print(sprintf("done: %g passed, %g failed", vr_agam_passed, vr_agam_failed));
            vr_agt_passed = vr_agam_passed;
            vr_agt_failed = vr_agam_failed;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        float code = VR_AGAM_Code(vr_agam_i);
        if(code == 0)
        {
            VR_AGAM_Print(sprintf("case %g %s: skipped (no Arcane Dimensions data)", vr_agam_i, vr_agam_name));
            vr_agam_i = vr_agam_i + 1;
            return;
        }
        VR_AGT_ClearWeapons();
        VR_AGAM_ClearBackpacks();
        float c = VR_AGT_Class(code);
        float dist = (c == VR_AG_CLASS_MELEE || c == VR_AG_CLASS_FISTS) ? 70 : c == VR_AG_CLASS_EXPLOSIVE ? 360 : 240;
        g = VR_AGT_Spawn(code, dist);
        vr_agt_weapon2 = 0;
        vr_agam_g = g;
        VR_AGAM_Print(sprintf("case %g %s", vr_agam_i, vr_agam_name));
        float ok = VR_AGAM_SpawnOk(g.vr_ag_prop, g.vr_ag_reserve);
        string line = vr_agam_line;
        if(g.vr_ag_prop2)
        {
            ok = (VR_AGAM_SpawnOk(g.vr_ag_prop2, g.vr_ag_reserve2) && ok);
            line = strcat(line, "; left: ", vr_agam_line);
        }
        VR_AGAM_Check(sprintf("%s spawn: %s:", vr_agam_name, line), ok);
        vr_agam_disarmed = 0;
        vr_agam_disarm_aid = AID_NONE;
        if(vr_agam_i == 34 || vr_agam_i == 35)
        {
            vr_agam_disarmed = g.vr_ag_reserve;
            vr_agam_disarm_aid = WeaponIdToAmmoId(g.vr_ag_wid, g.vr_ag_wflags);
            float dwid = g.vr_ag_wid;
            VR_AG_Disarm(g, '1 0 0', 150, "the test");
            // Its spare rounds in its pockets (its ammo kind's .ammo_* field; a single-use gun's own pocket).
            float kept = vr_agam_disarm_aid != AID_NONE ? VR_AmmoUtil_GetAmmoCountByAmmoId(g, vr_agam_disarm_aid) :
                         (g.vr_ag_pocket_wid == dwid ? g.vr_ag_pocket_n : 0);
            VR_AGAM_Check(sprintf("%s: knocked out of its hand, %g in reserve before, %g in its pockets:", vr_agam_name,
                vr_agam_disarmed, kept), kept == vr_agam_disarmed);
        }
        // A reload to come (its sound): a gun's clip down to 1.
        vr_agam_reload = FALSE;
        entity p = g.vr_ag_prop;
        if(p && VR_AG_Class(p.weapon) != VR_AG_CLASS_MELEE && VR_AG_ClipOf(p.weapon, p.weaponinst) > 1 && g.vr_ag_reserve > 0)
        {
            p.weaponinst.wi_clip = 1;
            vr_agam_reload = TRUE;
        }
        vr_agam_step = 1;
        vr_agam_until = time + 0.6; // (as test 1's: it lands and stands first)
        return;
    }
    if(vr_agam_step == 1)
    {
        if(time < vr_agam_until) { return; }
        VR_AGT_Hunt(g);
        vr_agam_hits0 = vr_agr_pl_hits;
        vr_agam_raw0 = vr_agr_pl_raw;
        vr_agam_dmg0 = vr_agr_pl_dmg;
        vr_agam_step = 2;
        vr_agam_until = time + 15; // (a safety net: the case ends on its hits and its reload)
        return;
    }
    if(vr_agam_step == 2)
    {
        float hits = vr_agr_pl_hits - vr_agam_hits0;
        float done = ((hits >= 3 || (vr_agam_i == 14 && g.vr_ag_total_shots >= 1)) &&
                      (!vr_agam_reload || g.vr_ag_reloads >= 1));
        if(!done && time < vr_agam_until && g.health > 0)
        {
            return;
        }
        float raw = vr_agr_pl_raw - vr_agam_raw0;
        float dmg = vr_agr_pl_dmg - vr_agam_dmg0;
        VR_AGAM_Check(strcat(sprintf("%s damage: %g hits, %.2f a hit before the armed scale, %.2f after, ", vr_agam_name,
            hits, hits > 0 ? raw / hits : 0, hits > 0 ? dmg / hits : 0), sprintf("%g shots, %g blows, %g reloads%s:",
            g.vr_ag_total_shots, g.vr_ag_strikes, g.vr_ag_reloads, done ? "" : " (timed out)")),
            vr_agam_i == 14 || (hits > 0 && (!vr_agam_reload || g.vr_ag_reloads >= 1))); // (the proximity gun: test 1's
            // known one, its mines laid short of a player standing still, often none: not checked)
        // Killed (its loot as it falls: its death frames' SoldierDrop, a gib's at once).
        VR_AGAM_Expected(g);
        VR_AGAM_ClearBackpacks(); // (the next one is its)
        T_Damage(g, pl, pl, 5000);
        VR_AGAM_Print(sprintf("%s killed: health %g, %s, model %s", vr_agam_name, g.health, g.classname, g.model));
        vr_agam_step = 3;
        vr_agam_until = time;
        return;
    }
    entity b = VR_AGAM_Backpack();
    if(!b && time < vr_agam_until + 3)
    {
        return; // (a safety net: it drops one within a few frames)
    }
    float k;
    float same = b != world;
    for(k = 1; k < 8; k++)
    {
        same = (same && (VR_AmmoUtil_GetAmmoCountByAmmoId(b, k) == vr_agam_e[k]));
    }
    // (Three sprintfs: a varargs builtin takes at most 8 parameters.)
    string got = b ? sprintf("backpack %g %g %g %g (+%g %g %g)", b.ammo_shells, b.ammo_nails, b.ammo_rockets,
        b.ammo_cells, b.ammo_lava_nails, b.ammo_multi_rockets, b.ammo_plasma) : "no backpack";
    VR_AGAM_Check(strcat(sprintf("%s loot: %s, ", vr_agam_name, got), sprintf("expected %g %g %g %g (+%g %g %g):",
        vr_agam_e[1], vr_agam_e[2], vr_agam_e[3], vr_agam_e[4], vr_agam_e[5], vr_agam_e[6], vr_agam_e[7])), same);
    VR_AGAM_Print("case end");
    vr_agam_i = vr_agam_i + 1;
    vr_agam_step = 0;
};
#endif
