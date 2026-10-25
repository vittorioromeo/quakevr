#ifdef QVR_TESTS
// vr_armedrepl_test.qc -- armed humanoid replacements' tests (vr_armedrepl.qc; ROUND22.md, "Armed humanoid
// replacements"). `vr_agrunt_test N` (Debug > Automation & Self-Tests), the player kept alive (VR_AGTest_Frame); each
// ends on `agtest: test N over: P passed, F failed` (run.sh -QuitOn "agtest: test [0-9]+ over").
// - 70 (any open map: vrtesthall): the rolls' statistics over seeds per option and skill, the eligibility gates (no
//   super shotgun before a player owns one, no rare weapon before the episode's vr_agrunt_repl_rare_map), the caps,
//   dual and gun-and-axe chances, determinism (the same seed twice; real spawns at the same spots twice).
// - 71 (vrtesthall): the balance: a vanilla grunt, then a replaced one with its own rifle, then the enforcers, each
//   alone against you for 150 s (`agrtest: balance ...`: shots, hits, accuracy, damage a second), then 40 of each
//   killed (their drops: guns, backpacks and their ammo, extra weapons).
// - 72 (vrtesthall): a gun and an axe: the gun at range, the axe up close, the gun again backed off; a spent shotgun
//   with no reserve: the axe; a gun lying near: taken up.
// - 73 (any map): the census: the map's monsters, the armed ones and their loadouts, all killed (`agrtest: map ...`).

float vr_agrt_step;
float vr_agrt_until;
float vr_agrt_case;
entity vr_agrt_g;
vector vr_agrt_home;      // the player's spot at the test's start
float vr_agrt_keep[16];   // the settings put back at the end
float vr_agrt_skill;

string(float i) VR_AGRT_CvarName =
{
    if(i == 0) { return "vr_agrunt_repl"; }
    if(i == 1) { return "vr_agrunt_repl_common"; }
    if(i == 2) { return "vr_agrunt_repl_uncommon"; }
    if(i == 3) { return "vr_agrunt_repl_rare"; }
    if(i == 4) { return "vr_agrunt_repl_rare_map"; }
    if(i == 5) { return "vr_agrunt_repl_dual"; }
    if(i == 6) { return "vr_agrunt_repl_gunaxe"; }
    if(i == 7) { return "vr_agrunt_repl_seed"; }
    if(i == 8) { return "vr_agrunt_repl_id"; }
    return "";
};
float VR_AGRT_NCVARS = 9;

void() VR_AGRT_Save =
{
    float i;
    for(i = 0; i < VR_AGRT_NCVARS; i++)
    {
        vr_agrt_keep[i] = cvar(VR_AGRT_CvarName(i));
    }
    vr_agrt_skill = skill;
};

void() VR_AGRT_Restore =
{
    float i;
    for(i = 0; i < VR_AGRT_NCVARS; i++)
    {
        cvar_set(VR_AGRT_CvarName(i), ftos(vr_agrt_keep[i]));
    }
    skill = vr_agrt_skill;
};

// The options for a case: the replacements on, the chances, the rare weapons' map (others 0).
void(float common, float uncommon, float rare, float dual, float gunaxe) VR_AGRT_Set =
{
    cvar_set("vr_agrunt_repl", "1");
    cvar_set("vr_agrunt_repl_id", "1");
    cvar_set("vr_agrunt_repl_common", ftos(common));
    cvar_set("vr_agrunt_repl_uncommon", ftos(uncommon));
    cvar_set("vr_agrunt_repl_rare", ftos(rare));
    cvar_set("vr_agrunt_repl_dual", ftos(dual));
    cvar_set("vr_agrunt_repl_gunaxe", ftos(gunaxe));
    cvar_set("vr_agrunt_repl_rare_map", "4");
    cvar_set("vr_agrunt_repl_seed", "0");
};

// VR_AGRT_Roll's counts.
float vr_agrt_n;
float vr_agrt_native;
float vr_agrt_t1;
float vr_agrt_t2;
float vr_agrt_t3;
float vr_agrt_dual;
float vr_agrt_gunaxe;
float vr_agrt_ssg;
float vr_agrt_hash; // the loadouts' sum (determinism)

// `n` seeds' loadouts for the row `kind` (seeds `base`..).
void(float kind, float n, float base) VR_AGRT_Roll =
{
    vr_agrt_n = n;
    vr_agrt_native = 0;
    vr_agrt_t1 = 0;
    vr_agrt_t2 = 0;
    vr_agrt_t3 = 0;
    vr_agrt_dual = 0;
    vr_agrt_gunaxe = 0;
    vr_agrt_ssg = 0;
    vr_agrt_hash = 0;
    float i;
    for(i = 0; i < n; i++)
    {
        vector s;
        s_x = VR_AGR_Mod((base + i) * 37 + 11, 30011);
        s_y = VR_AGR_Mod((base + i) * 101 + 5, 30011);
        s_z = kind + 1;
        VR_AGR_Pick(kind, s);
        if(vr_agr_tier == 0) { vr_agrt_native = vr_agrt_native + 1; }
        if(vr_agr_tier == 1) { vr_agrt_t1 = vr_agrt_t1 + 1; }
        if(vr_agr_tier == 2) { vr_agrt_t2 = vr_agrt_t2 + 1; }
        if(vr_agr_tier == 3) { vr_agrt_t3 = vr_agrt_t3 + 1; }
        if(vr_agr_code2 > 0 && !vr_agr_gunaxe) { vr_agrt_dual = vr_agrt_dual + 1; }
        if(vr_agr_gunaxe) { vr_agrt_gunaxe = vr_agrt_gunaxe + 1; }
        if(vr_agr_code == WID_SUPER_SHOTGUN || vr_agr_code2 == WID_SUPER_SHOTGUN) { vr_agrt_ssg = vr_agrt_ssg + 1; }
        vr_agrt_hash = VR_AGR_Mod(vr_agrt_hash * 7 + vr_agr_code * 3 + vr_agr_code2 + vr_agr_gunaxe, 30011);
    }
};

// A share of the last roll within lo..hi.
void(string what, float count, float lo, float hi) VR_AGRT_Share =
{
    float f = count / max(1, vr_agrt_n);
    VR_AGT_Check(sprintf("stats %s: %g of %g (%.3f, want %.2f..%.2f)", what, count, vr_agrt_n, f, lo, hi), f >= lo && f <= hi);
};

void(float wid, float has) VR_AGRT_Own =
{
    if(has)
    {
        VR_WeaponUtil_EntAddWeapon(vr_agt_player, wid);
    }
    else
    {
        VR_WeaponUtil_EntDelWeapon(vr_agt_player, wid);
    }
};

// A grunt (`kind` 0) or an enforcer (1) as a map's, `at`, facing the player; `repl` the replacements on for it.
entity(float kind, vector at, float repl) VR_AGRT_Make =
{
    cvar_set("vr_agrunt_repl", repl ? "1" : "0");
    entity oldself = self;
    self = spawn();
    self.classname = kind == VR_AM_ENFORCER ? "monster_enforcer" : "monster_army";
    setorigin(self, at);
    self.angles_y = vectoyaw(vr_agt_player.origin - at);
    if(kind == VR_AM_ENFORCER) { monster_enforcer(); } else { monster_army(); }
    entity g = self;
    self = oldself;
    return g;
};

// A spot `fw` ahead of the player and `side` to his left, on his floor.
vector(float fw, float side) VR_AGRT_Ahead =
{
    makevectors(vr_agt_player.angles);
    vector f = v_forward;
    f_z = 0;
    f = normalize(f);
    vector l = '0 0 0';
    l_x = -f_y;
    l_y = f_x;
    return vr_agrt_home + f * fw + l * side;
};

void(entity g) VR_AGRT_Remove =
{
    if(g.vr_ag)
    {
        VR_AGT_Remove(g);
        return;
    }
    remove(g);
};

// ----------------------------------------------------------------------------
// Test 70: the rolls
// ----------------------------------------------------------------------------

void() VR_AGRT_Stats =
{
    float n = 500; // (each case a frame of its own: QuakeC's instruction budget)
    float st = vr_agrt_step;
    vr_agrt_step = st + 1;
    skill = 1;
    if(st == 0)
    {
    VR_AGRT_Save();
    vr_agrt_home = vr_agt_player.origin;
    VR_AGRT_Own(WID_SUPER_SHOTGUN, FALSE);
    VR_AGRT_Own(WID_NAILGUN, FALSE);
    VR_AGRT_Own(WID_SUPER_NAILGUN, FALSE);
    VR_AGRT_Own(WID_ROCKET_LAUNCHER, FALSE);
    VR_AGRT_Set(0, 0, 0, 0, 0);
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("off, grunt: its own gun", vr_agrt_native, 1, 1);
    VR_AGRT_Roll(1, n, 0);
    VR_AGRT_Share("off, enforcer: its own gun", vr_agrt_native, 1, 1);

    return;
    }
    if(st == 1)
    {
    VR_AGRT_Set(0.3, 0, 0, 0, 0);
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("common 0.3, normal", vr_agrt_t1, 0.24, 0.36);
    return;
    }
    if(st == 2)
    {
    skill = 0;
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("common 0.3, easy (x0.5)", vr_agrt_t1, 0.10, 0.20);
    return;
    }
    if(st == 3)
    {
    skill = 2;
    VR_AGRT_Roll(1, n, 0);
    VR_AGRT_Share("common 0.3, hard (x1.5), enforcer", vr_agrt_t1, 0.39, 0.51);
    return;
    }
    if(st == 4)
    {
    skill = 3;
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("common 0.3, nightmare (x2)", vr_agrt_t1, 0.53, 0.67);

    return;
    }
    if(st == 5)
    {
    VR_AGRT_Set(0, 0.5, 0, 0, 0);
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("uncommon 0.5, none owned", vr_agrt_t2, 0, 0);
    VR_AGRT_Share("uncommon 0.5, none owned: no super shotgun", vr_agrt_ssg, 0, 0);
    return;
    }
    if(st == 6)
    {
    VR_AGRT_Set(0, 0.5, 0, 0, 0);
    VR_AGRT_Own(WID_SUPER_SHOTGUN, TRUE);
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("uncommon 0.5, the super shotgun owned", vr_agrt_t2, 0.43, 0.57);
    VR_AGRT_Share("uncommon 0.5, the super shotgun owned: all super shotguns", vr_agrt_ssg, 0.43, 0.57);

    return;
    }
    if(st == 7)
    {
    VR_AGRT_Set(0, 0, 0.5, 0, 0);
    VR_AGRT_Own(WID_ROCKET_LAUNCHER, TRUE);
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share(sprintf("rare 0.5 on %s (map index %g, from 4)", mapname, VR_AGR_MapIndex()), vr_agrt_t3, 0, 0);
    return;
    }
    if(st == 8)
    {
    VR_AGRT_Set(0, 0, 0.5, 0, 0);
    cvar_set("vr_agrunt_repl_rare_map", "0");
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("rare 0.5, any map", vr_agrt_t3, 0.43, 0.57);

    return;
    }
    if(st == 9)
    {
    VR_AGRT_Set(1, 1, 0, 0, 0);
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("common 1 + uncommon 1 (capped): its own gun", vr_agrt_native, 0, 0);
    VR_AGRT_Share("common 1 + uncommon 1 (capped): common", vr_agrt_t1, 0.43, 0.57);

    return;
    }
    if(st == 10)
    {
    VR_AGRT_Set(1, 0, 0, 0.5, 0);
    VR_AGRT_Roll(0, n, 0);
    VR_AGRT_Share("dual 0.5 (common guns)", vr_agrt_dual, 0.43, 0.57);
    return;
    }
    if(st == 11)
    {
    VR_AGRT_Set(0, 0, 0, 0, 0.4);
    VR_AGRT_Roll(1, n, 0);
    VR_AGRT_Share("gun and axe 0.4, enforcer", vr_agrt_gunaxe, 0.33, 0.47);

    return;
    }
    if(st == 12)
    {
    VR_AGRT_Set(0.3, 0.3, 0, 0.2, 0.2);
    VR_AGRT_Roll(0, 300, 5);
    float h1 = vr_agrt_hash;
    VR_AGRT_Roll(0, 300, 5);
    VR_AGT_Check(sprintf("determinism: the same seeds twice: %g, %g", h1, vr_agrt_hash), h1 == vr_agrt_hash);
    cvar_set("vr_agrunt_repl_seed", "7");
    VR_AGRT_Roll(0, 300, 5);
    VR_AGT_Check(sprintf("vr_agrunt_repl_seed 7 shuffles them: %g", vr_agrt_hash), h1 != vr_agrt_hash);
    cvar_set("vr_agrunt_repl_seed", "0");

    return;
    }
    if(st == 13)
    {
    // Real spawns: 12 grunts and enforcers as a map's (a player in: rolled at once), twice at the same spots.
    VR_AGRT_Set(0.4, 0.3, 0, 0.2, 0.2);
    float k;
    float sum1 = 0;
    float sum2 = 0;
    float armed = 0;
    float native = 0;
    float plain = 0;
    float pass;
    for(pass = 0; pass < 2; pass++)
    {
        for(k = 0; k < 12; k++)
        {
            entity g = VR_AGRT_Make(k & 1, VR_AGRT_Ahead(200 + floor(k / 4) * 60, (k & 3) * 60 - 90), TRUE);
            float code = (g.vr_ag_prop ? g.vr_ag_prop.weapon : -1) * 31 + (g.vr_ag_prop2 ? g.vr_ag_prop2.weapon : -1);
            if(pass == 0)
            {
                sum1 = VR_AGR_Mod(sum1 * 7 + code, 30011);
                armed = armed + (g.vr_ag && g.vr_agr);
                native = native + (g.vr_ag_prop.vr_ag_native || g.vr_ag_prop2.vr_ag_native);
            }
            else
            {
                sum2 = VR_AGR_Mod(sum2 * 7 + code, 30011);
            }
            VR_AGRT_Remove(g);
        }
    }
    entity v = VR_AGRT_Make(0, VR_AGRT_Ahead(200, 0), FALSE);
    plain = (!v.vr_ag) * (v.classname == "monster_army");
    VR_AGRT_Remove(v);
    VR_AGT_Check(sprintf("spawns: %g of 12 armed replacements, %g with their own gun", armed, native), armed == 12 && native > 0 && native < 12);
    VR_AGT_Check(sprintf("spawns: the same spots twice, the same loadouts (%g, %g)", sum1, sum2), sum1 == sum2);
    VR_AGT_Check("spawns: off, a grunt as it was", plain);
    VR_AGRT_Restore();
    cvar_set("vr_agrunt_test", "0");
    }
};

// ----------------------------------------------------------------------------
// Test 71: the balance
// ----------------------------------------------------------------------------

float VR_AGRT_FIGHT = 150;     // s each fight
float VR_AGRT_KILLS = 40;      // kills each for the drops
float vr_agrt_res[32];         // per case (kind * 2 + replaced): shots, hits, dmg, guns, packs, ammo, extra, n
float vr_agrt_shots0;
float vr_agrt_hits0;
float vr_agrt_dmg0;
float vr_agrt_killed;

.float vr_agrt_mark; // test 71's kills

// Every monster the test made gone (its kills' corpses, any left alive).
void() VR_AGRT_Clear =
{
    entity e = nextent(world);
    while(e)
    {
        entity next = nextent(e);
        if(e.vr_agrt_mark || e == vr_agrt_g)
        {
            VR_AGRT_Remove(e);
        }
        e = next;
    }
    vr_agrt_g = world;
};

float() VR_AGRT_Shots = { return (vr_agrt_case >= 2) ? vr_agr_shots_enf : vr_agr_shots_grunt; };

// The drops lying about now: counted into case `c`'s (-1: none), then gone.
void(float c) VR_AGRT_CountDrops =
{
    float at = max(0, c) * 8;
    float on = c >= 0;
    entity e = nextent(world);
    while(e)
    {
        entity next = nextent(e);
        if(e.classname == "thrown_weapon")
        {
            if(e.weapon == WID_GRUNTGUN || e.weapon == WID_ENFRIFLE)
            {
                vr_agrt_res[at + 3] = vr_agrt_res[at + 3] + on;
            }
            else
            {
                vr_agrt_res[at + 6] = vr_agrt_res[at + 6] + on;
            }
            e.weaponinst = world;
            remove(e);
        }
        else if(e.model == "progs/backpack.mdl")
        {
            vr_agrt_res[at + 4] = vr_agrt_res[at + 4] + on;
            vr_agrt_res[at + 5] = vr_agrt_res[at + 5] + on * (e.ammo_shells + e.ammo_cells + e.ammo_nails + e.ammo_rockets);
            remove(e);
        }
        e = next;
    }
};

void() VR_AGRT_Balance =
{
    entity pl = vr_agt_player;
    float kind = floor(vr_agrt_case / 2);
    float repl = vr_agrt_case - 2 * floor(vr_agrt_case / 2);
    if(vr_agrt_step == 0)
    {
        if(vr_agrt_case >= 4)
        {
            float k;
            for(k = 0; k < 2; k++)
            {
                float a = k * 2;
                float b = a + 1;
                string nm = k ? "enforcer" : "grunt";
                float dpsA = vr_agrt_res[a * 8 + 2] / VR_AGRT_FIGHT;
                float dpsB = vr_agrt_res[b * 8 + 2] / VR_AGRT_FIGHT;
                float accA = vr_agrt_res[a * 8 + 2] / max(1, vr_agrt_res[a * 8]); // (damage a shot: its accuracy; a hit may deal in parts)
                float accB = vr_agrt_res[b * 8 + 2] / max(1, vr_agrt_res[b * 8]);
                float rateA = vr_agrt_res[a * 8] / VR_AGRT_FIGHT;
                float rateB = vr_agrt_res[b * 8] / VR_AGRT_FIGHT;
                VR_AGT_Print(sprintf("balance %s vanilla: %g shots (%.2f/s), %g damage events, %.2f damage a shot, %.2f damage/s",
                    nm, vr_agrt_res[a * 8], rateA, vr_agrt_res[a * 8 + 1], accA, dpsA));
                VR_AGT_Print(sprintf("balance %s replaced: %g shots (%.2f/s), %g damage events, %.2f damage a shot, %.2f damage/s",
                    nm, vr_agrt_res[b * 8], rateB, vr_agrt_res[b * 8 + 1], accB, dpsB));
                VR_AGT_Print(sprintf("drops %s vanilla (%g kills): guns %g, backpacks %g (ammo %g), extra weapons %g",
                    nm, VR_AGRT_KILLS, vr_agrt_res[a * 8 + 3], vr_agrt_res[a * 8 + 4], vr_agrt_res[a * 8 + 5], vr_agrt_res[a * 8 + 6]));
                VR_AGT_Print(sprintf("drops %s replaced (%g kills): guns %g, backpacks %g (ammo %g), extra weapons %g",
                    nm, VR_AGRT_KILLS, vr_agrt_res[b * 8 + 3], vr_agrt_res[b * 8 + 4], vr_agrt_res[b * 8 + 5], vr_agrt_res[b * 8 + 6]));
                VR_AGT_Check(sprintf("balance %s: damage a second within 25%%", nm), fabs(dpsB - dpsA) <= 0.25 * max(0.1, dpsA));
                VR_AGT_Check(sprintf("balance %s: shots a second within 25%%", nm), fabs(rateB - rateA) <= 0.25 * max(0.01, rateA));
                VR_AGT_Check(sprintf("balance %s: damage a shot (accuracy) within 20%%", nm), fabs(accB - accA) <= 0.2 * max(0.1, accA));
                VR_AGT_Check(sprintf("drops %s: a gun from each, the same backpacks", nm),
                    vr_agrt_res[a * 8 + 3] == VR_AGRT_KILLS && vr_agrt_res[b * 8 + 3] == VR_AGRT_KILLS &&
                    vr_agrt_res[a * 8 + 4] == vr_agrt_res[b * 8 + 4] && vr_agrt_res[a * 8 + 5] == vr_agrt_res[b * 8 + 5]);
                VR_AGT_Check(sprintf("drops %s: extra weapons within 12 of each other", nm),
                    fabs(vr_agrt_res[a * 8 + 6] - vr_agrt_res[b * 8 + 6]) <= 12);
            }
            VR_AGRT_Restore();
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        if(vr_agrt_case == 0)
        {
            VR_AGRT_Save();
            vr_agrt_home = pl.origin;
        }
        VR_AGRT_Set(0, 0, 0, 0, 0);
        vr_agrt_g = VR_AGRT_Make(kind, VR_AGRT_Ahead(320, 0), repl);
        vr_agrt_step = 1;
        vr_agrt_until = time + 0.6;
        return;
    }
    entity g = vr_agrt_g;
    if(vr_agrt_step == 1)
    {
        if(time < vr_agrt_until) { return; }
        VR_AGT_Hunt(g);
        vr_agrt_shots0 = VR_AGRT_Shots();
        vr_agrt_hits0 = vr_agr_pl_hits;
        vr_agrt_dmg0 = vr_agr_pl_dmg;
        vr_agrt_step = 2;
        vr_agrt_until = time + VR_AGRT_FIGHT;
        return;
    }
    if(vr_agrt_step == 2)
    {
        if(g.enemy != pl && g.health > 0)
        {
            VR_AGT_Hunt(g); // (always on the player)
        }
        if(time < vr_agrt_until) { return; }
        float c = vr_agrt_case;
        vr_agrt_res[c * 8] = VR_AGRT_Shots() - vr_agrt_shots0;
        vr_agrt_res[c * 8 + 1] = vr_agr_pl_hits - vr_agrt_hits0;
        vr_agrt_res[c * 8 + 2] = vr_agr_pl_dmg - vr_agrt_dmg0;
        VR_AGRT_Clear();
        VR_AGRT_CountDrops(-1); // (none counted: what lay about before)
        vr_agrt_killed = 0;
        vr_agrt_step = 3;
        return;
    }
    if(vr_agrt_step == 3)
    {
        if(vr_agrt_killed >= VR_AGRT_KILLS)
        {
            vr_agrt_case = vr_agrt_case + 1;
            vr_agrt_step = 0;
            return;
        }
        // Ten in a row ahead, up a second (their walkmonster_start_go: hurt only then), then killed (not gibbed: their
        // deaths' frames drop the loot), counted 1.5 s later.
        float j;
        for(j = 0; j < 10; j++)
        {
            entity m = VR_AGRT_Make(kind, VR_AGRT_Ahead(260, (j - 4.5) * 48), repl);
            m.vr_agrt_mark = 1;
        }
        vr_agrt_killed = vr_agrt_killed + 10;
        vr_agrt_step = 4;
        vr_agrt_until = time + 1;
        return;
    }
    if(time < vr_agrt_until) { return; }
    entity e = nextent(world);
    if(vr_agrt_step == 4)
    {
        while(e)
        {
            if(e.vr_agrt_mark && e.health > 0)
            {
                T_Damage(e, pl, pl, e.health + 10);
            }
            e = nextent(e);
        }
        vr_agrt_step = 5;
        vr_agrt_until = time + 1.5;
        return;
    }
    VR_AGRT_CountDrops(vr_agrt_case);
    VR_AGRT_Clear();
    vr_agrt_step = 3;
};

// ----------------------------------------------------------------------------
// Test 72: a gun and an axe
// ----------------------------------------------------------------------------

float VR_AGRT_GIVEUP = 20; // s a step may take (FAIL "timed out")

void(string what, float ok) VR_AGRT_StepCheck =
{
    VR_AGT_Check(what, ok);
    vr_agrt_step = vr_agrt_step + 1;
    vr_agrt_until = time + VR_AGRT_GIVEUP;
};

float() VR_AGRT_OnAxe = { return vr_agrt_g.vr_ag_prop && VR_AG_Class(vr_agrt_g.vr_ag_wid) == VR_AG_CLASS_MELEE; };

void() VR_AGRT_GunAxe =
{
    entity pl = vr_agt_player;
    entity g = vr_agrt_g;
    float timedOut = vr_agrt_step > 0 && time > vr_agrt_until;
    if(g && g.health > 0 && g.enemy != pl && vr_agrt_step > 0 && vr_agrt_step != 8)
    {
        VR_AGT_Hunt(g);
    }
    if(vr_agrt_step == 0)
    {
        VR_AGRT_Save();
        vr_agrt_home = pl.origin;
        VR_AGRT_Set(0, 0, 0, 0, 1);
        vr_agrt_g = VR_AGRT_Make(0, VR_AGRT_Ahead(320, 0), TRUE);
        g = vr_agrt_g;
        VR_AGT_Check(sprintf("gun and axe: spawned with %s and %s", g.vr_ag_prop.netname, g.vr_ag_prop2.netname),
            g.vr_ag_gunaxe && g.vr_ag_prop.vr_ag_native && g.vr_ag_prop2.weapon == WID_AXE && g.vr_ag_prop2.vr_ag_left);
        vr_agrt_step = 1;
        vr_agrt_until = time + VR_AGRT_GIVEUP;
        vr_agrt_shots0 = g.vr_ag_total_shots;
        return;
    }
    if(vr_agrt_step == 1) // at range: the gun fired
    {
        if(g.vr_ag_total_shots > vr_agrt_shots0 || timedOut)
        {
            // (its weapons where its hands are: the gun on its right, the axe on its left)
            makevectors(g.angles);
            float gunSide = (g.vr_ag_prop.origin - g.origin) * v_right;
            float axeSide = (g.vr_ag_prop2.origin - g.origin) * v_right;
            VR_AGT_Check(sprintf("gun and axe: held in their hands: the gun %.1f to its right, the axe %.1f", gunSide,
                axeSide), gunSide > 2 && axeSide < -2);
            VR_AGRT_StepCheck(sprintf("gun and axe: at range, the gun fires (%g shots, the axe %s)", g.vr_ag_total_shots,
                VR_AGRT_OnAxe() ? "in use" : "carried"), !VR_AGRT_OnAxe() && !timedOut);
            setorigin(g, VR_AGRT_Ahead(56, 0)); // (up close)
            vr_agrt_shots0 = g.vr_ag_strikes;
        }
        return;
    }
    if(vr_agrt_step == 2) // up close: the axe swung
    {
        if((VR_AGRT_OnAxe() && g.vr_ag_strikes > vr_agrt_shots0) || timedOut)
        {
            VR_AGRT_StepCheck(sprintf("gun and axe: up close, the axe swung (%g blows, %g switches)", g.vr_ag_strikes,
                g.vr_agr_switches), !timedOut);
            setorigin(g, VR_AGRT_Ahead(330, 0)); // (backed off)
            vr_agrt_shots0 = g.vr_ag_total_shots;
        }
        return;
    }
    if(vr_agrt_step == 3) // backed off: the gun again
    {
        if((!VR_AGRT_OnAxe() && g.vr_ag_total_shots > vr_agrt_shots0) || timedOut)
        {
            VR_AGRT_StepCheck(sprintf("gun and axe: backed off, the gun again (%g shots, %g switches)",
                g.vr_ag_total_shots, g.vr_agr_switches), !timedOut);
            VR_AGRT_Remove(g);
            // A shotgun and an axe, the shotgun spent with no reserve: the axe.
            VR_AGRT_Set(1, 0, 0, 0, 1);
            vr_agrt_g = VR_AGRT_Make(0, VR_AGRT_Ahead(320, 0), TRUE);
            g = vr_agrt_g;
            if(g.vr_ag_prop.weapon == WID_SHOTGUN || g.vr_ag_prop.weapon == WID_NAILPISTOL)
            {
                g.vr_ag_prop.weaponinst.wi_clip = 0;
                g.vr_ag_reserve = 0;
            }
        }
        return;
    }
    if(vr_agrt_step == 4)
    {
        if((VR_AGRT_OnAxe() && !g.vr_ag_prop2) || timedOut)
        {
            VR_AGRT_StepCheck(sprintf("gun and axe: its gun spent, none near: the axe (%s, %g away)",
                g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing", vlen(g.origin - pl.origin)), VR_AGRT_OnAxe() && !timedOut);
            VR_AGT_ClearWeapons(); // (its spent gun)
            // A full shotgun lying near it, the player far: taken up again.
            makevectors(g.angles);
            CreateThrownWeapon(g, WID_SHOTGUN, 0, WeaponInst_New(VR_AG_ClipSize(WID_SHOTGUN)), g.origin + v_right * 40 + '0 0 8',
                '0 0 0', '0 0 0', '0 0 0', FALSE, FALSE, 0);
            setorigin(pl, VR_AGRT_Ahead(-250, 0));
            vr_agrt_shots0 = g.vr_ag_pickups;
        }
        return;
    }
    if(vr_agrt_step == 5)
    {
        if(g.vr_ag_pickups > vr_agrt_shots0 || timedOut)
        {
            entity gunP = VR_AG_Class(g.vr_ag_wid) == VR_AG_CLASS_MELEE ? g.vr_ag_prop2 : g.vr_ag_prop;
            float gunR = g.vr_ag_prop2 && gunP.weapon == WID_SHOTGUN && !gunP.vr_ag_left; // (the gun in its right hand)
            VR_AGRT_StepCheck(sprintf("gun and axe: a gun lying near, taken up (%g pickups, %s and %s)", g.vr_ag_pickups,
                g.vr_ag_prop.netname, g.vr_ag_prop2 ? g.vr_ag_prop2.netname : "nothing"), gunR && !timedOut);
            VR_AGRT_Remove(g);
            setorigin(pl, vr_agrt_home);
            VR_AGRT_Restore();
            cvar_set("vr_agrunt_test", "0");
        }
        return;
    }
};

// ----------------------------------------------------------------------------
// Test 73: a map's census
// ----------------------------------------------------------------------------

void() VR_AGRT_Census =
{
    entity pl = vr_agt_player;
    if(vr_agrt_step == 0)
    {
        vr_agrt_until = time + 3; // (the map's monsters down on their floors, its replacements rolled)
        vr_agrt_step = 1;
        return;
    }
    if(time < vr_agrt_until) { return; }
    if(vr_agrt_step == 1)
    {
        float live = 0;
        float armed = 0;
        float native = 0;
        float varied = 0;
        float waiting = 0;
        float pending = 0;
        float humans = 0;
        entity e = nextent(world);
        while(e)
        {
            if(e.MG_deferred)
            {
                waiting = waiting + 1;
            }
            else if((e.flags & FL_MONSTER) && e.health > 0)
            {
                live = live + 1;
                humans = humans + (e.classname == "monster_army" || e.classname == "monster_enforcer");
                if(e.vr_agr)
                {
                    armed = armed + 1;
                    native = native + VR_AGR_NativeAlone(e);
                    varied = varied + !VR_AGR_NativeAlone(e);
                    pending = pending + e.vr_agr_pending;
                }
            }
            e = nextent(e);
        }
        VR_AGT_Print(sprintf("map %s: monsters %g, live %g (waiting %g), killed %g", mapname, total_monsters, live, waiting,
            killed_monsters));
        VR_AGT_Print(sprintf("map %s: grunts and enforcers %g, replaced %g: own gun %g, varied %g, unrolled %g", mapname, humans, armed,
            native, varied, pending));
        e = nextent(world);
        while(e)
        {
            if((e.flags & FL_MONSTER) && e.health > 0 && !e.MG_deferred)
            {
                T_Damage(e, pl, pl, e.health + 100);
            }
            e = nextent(e);
        }
        vr_agrt_until = time + 2;
        vr_agrt_step = 2;
        return;
    }
    VR_AGT_Print(sprintf("map %s: killed %g of %g", mapname, killed_monsters, total_monsters));
    cvar_set("vr_agrunt_test", "0");
};

void(float t) VR_AGRT_Frame =
{
    if(t == 70) { VR_AGRT_Stats(); }
    else if(t == 71) { VR_AGRT_Balance(); }
    else if(t == 72) { VR_AGRT_GunAxe(); }
    else if(t == 73) { VR_AGRT_Census(); }
};

// The test's state at its start (VR_AGTest_Frame, a new test).
void() VR_AGRT_Begin =
{
    vr_agrt_step = 0;
    vr_agrt_case = 0;
    vr_agrt_g = world;
    float i;
    for(i = 0; i < 32; i++)
    {
        vr_agrt_res[i] = 0;
    }
};
#endif
