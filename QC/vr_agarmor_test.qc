// vr_agarmor_test.qc -- armour for armed humanoids' test (vr_agarmor.qc; Debug > Tests; ROUND23.md, "Armour for armed
// humanoids, P1"). `vr_agarmor_test 1`: two armed grunts ahead of the player (one armoured, one not) hit through T_Damage
// at their plates' places: the absorb math by both rules, a headshot with and without the helmet (and its pop), a
// break (the bit off, the debris a prop you can't force-grab, gone after its fade), a shotgun's pellets over two
// pieces, a cut stopped by a plate then let through once it broke, the unarmoured grunt's damage untouched, a gibbing
// shedding every plate, and the skill rolls over 200 spawns each. Ends on "agarmor test: N passed, M failed".
// `vr_agarmor_test 2`: three armed grunts ahead in full green, yellow and red sets (the eyeshots).
// P2: an armed enforcer too (its pieces at their places, its power pack no plate, its debris, the skill's roll dressing
// it, its wear's word reaching the client), the damage marks' thresholds; `vr_agarmor_test 3` three enforcers in full
// sets, `4` three grunts in yellow, whole, at two thirds and at a third (the eyeshots).
// `vr_agarmor_test 6`: a grunt and an enforcer close ahead in full yellow (the helmets' eyeshots: vr_agarmor_headcam).
// `vr_agarmor_test 7` (agent/dknight): the armed knight (kind 2) and death knight (kind 3), vr_agarmor_rule 1: each one's
// pieces at their places on its own rig, the cuirass absorbing, a pauldron broken into its own model's debris, a blade's
// blow on a plate sparking with no blood, the skill's roll dressing it, its wear reaching the client; the debris fading.
// `vr_agarmor_test 8`: a knight and a death knight close ahead in full yellow (the eyeshots).

entity vr_art_a, vr_art_b; // the armoured grunt, the bare one
float vr_art_state;       // 0 idle, 1 the checks, 2 waiting for the debris to fade
float vr_art_until;       // state 2's give-up time
float vr_art_rule, vr_art_helm, vr_art_cuts, vr_art_dtime; // the settings put back after

void(float ok, string what) VR_ART_Check =
{
    if(ok)
    {
        VR_Test_Count(TRUE);
        dprint(sprintf("agarmor test: PASS %s\n", what));
    }
    else
    {
        VR_Test_Count(FALSE);
        dprint(sprintf("agarmor test: FAIL %s\n", what));
    }
};

entity vr_art_c;          // P2: the armoured enforcer
float vr_art_cwear;       // its wear the server set (the client's word awaited in state 2)

// An armed humanoid of `kind` (0 the grunt, 1 the enforcer) ahead of the player, `side` to the right, `ahead` out.
entity(entity p, float side, float kind, float ahead) VR_ART_SpawnKind =
{
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    entity oldself = self;
    self = spawn();
    setorigin(self, p.origin + fw * ahead + v_right * side);
    self.angles_y = vectoyaw(p.origin - self.origin);
    self.weapon = -1; // (its fists)
    VR_AM_Spawn(kind);
    entity g = self;
    self = oldself;
    g.vr_ar_type = 0;
    g.vr_ar_mask = 0;
    g.health = 1000;
    return g;
};

entity(entity p, float side) VR_ART_Spawn = { return VR_ART_SpawnKind(p, side, 0, 128); };

// A hit of `d` at `p` (PositionalDamage's `part`): the health it took.
float(entity g, vector p, float part, float d) VR_ART_Hit =
{
    float h0 = g.health;
    VR_Hit_SetZone(g, part, p);
    T_Damage(g, world, world, d);
    return h0 - g.health;
};

float(float m) VR_ART_Count =
{
    float n = 0;
    float i;
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        n = n + ((m & VR_AgArmor_Bit(i)) != 0);
    }
    return n;
};

void() VR_ART_Run =
{
    entity a = vr_art_a;
    float i, d, hp, n;
    float f0 = a.frame; // (its pain frames move its arms: the places found in this one, so put back before each hit)
    vector pc = limbpiece(a, 0, 10);
    vector ph = limbpiece(a, 1, 10);
    vector pp = limbpiece(a, 2, 10);
    float c0 = ragdollbone(a, pc, QVR_ARMOR_PIECE_AT);
    float c1 = ragdollbone(a, ph, QVR_ARMOR_PIECE_AT);
    float c2 = ragdollbone(a, pp, QVR_ARMOR_PIECE_AT);
    VR_ART_Check(c0 == 0 && c1 == 1 && c2 == 2, sprintf("the pieces found at their places (cuirass %g, helmet %g, pauldron %g)", c0, c1, c2));

    // The player's rule: green saves ceil(0.3 x 10) = 3 from the cuirass's pool.
    cvar_set("vr_agarmor_rule", "0");
    VR_AgArmor_Give(a, 1, 1023);
    hp = a.vr_ar_hp[0];
    d = VR_ART_Hit(a, pc, 0, 10);
    VR_ART_Check(d == 7 && a.vr_ar_hp[0] == hp - 3, sprintf("rule 0: 10 on green cuirass takes %g (7), pool %g -> %g", d, hp, a.vr_ar_hp[0]));
    VR_AgArmor_Give(a, 3, 1023);
    d = VR_ART_Hit(a, pc, 0, 10);
    VR_ART_Check(d == 2, sprintf("rule 0: 10 on red takes %g (2)", d));

    // A plate blocks all: nothing through, then a hit past its pool breaks it.
    cvar_set("vr_agarmor_rule", "1");
    VR_AgArmor_Give(a, 1, 1023);
    hp = a.vr_ar_hp[0];
    d = VR_ART_Hit(a, pc, 0, 10);
    VR_ART_Check(d == 0 && a.vr_ar_hp[0] == hp - 10, sprintf("rule 1: 10 on the cuirass takes %g (0), pool %g", d, a.vr_ar_hp[0]));
    float made = vr_ar_debris_made;
    hp = a.vr_ar_hp[0];
    d = VR_ART_Hit(a, pc, 0, 60);
    VR_ART_Check(d == 60 - hp && !VR_AgArmor_On(a, 0) && a.vr_ar_mask == 1022,
        sprintf("rule 1: 60 past a pool of %g takes %g, the cuirass broken (mask %g)", hp, d, a.vr_ar_mask));
    entity deb = find(world, classname, "vr_ar_debris");
    VR_ART_Check(vr_ar_debris_made == made + 1 && deb != world && deb.model == "progs/vrgrunt.mdl#plate0" && deb.vr_rigid == 1 &&
        deb.movetype == MOVETYPE_BOUNCE && !(deb.flags & FL_FORCEGRABBABLE),
        sprintf("the broken cuirass falls as a physics prop (%s, rigid %g), not force-grabbable", deb.model, deb.vr_rigid));
    cvar_set("vr_agarmor_rule", "0");

    // Headshots: the helmet halves the multiplier's extra and stops the pop; without it, all of it and the pop stands.
    float m = cvar_hget(cvarh_vr_headshot_mult);
    float halve = cvar_hget(cvarh_vr_positional_damage) && m > 1;
    float dm = halve ? 10 * (1 + (m - 1) * 0.5) : 10 * m;
    VR_AgArmor_Give(a, 1, 1023);
    vr_decap_ent = a;
    vr_decap_time = time;
    vr_decap_limb = -1;
    d = VR_ART_Hit(a, ph, 1, 10 * m);
    VR_ART_Check(d == ceil(dm - ceil(0.3 * dm)) && vr_decap_ent == world,
        sprintf("headshot of %g on a helmet takes %g (%g), the pop stopped", 10 * m, d, ceil(dm - ceil(0.3 * dm))));
    VR_AgArmor_Give(a, 1, 1023 - 2);
    vr_decap_ent = a;
    vr_decap_time = time;
    vr_decap_limb = -1;
    d = VR_ART_Hit(a, ph, 1, 10 * m);
    VR_ART_Check(d == ceil(10 * m) && vr_decap_ent == a, sprintf("headshot of %g with no helmet takes %g, the pop stands", 10 * m, d));
    vr_decap_ent = world;

    // A shotgun's pellets: three on the cuirass, three on the left pauldron (4 each): both pools take ceil(0.3 x 12).
    VR_AgArmor_Give(a, 1, 1023);
    float h0 = a.vr_ar_hp[0];
    float h2 = a.vr_ar_hp[2];
    a.frame = f0;
    ClearMultiDamage();
    for(i = 0; i < 6; i++)
    {
        VR_Hit_SetZone(a, 0, (i < 3) ? pc : pp);
        AddMultiDamage(a, 4);
    }
    float hb = a.health;
    ApplyMultiDamage();
    VR_ART_Check(a.vr_ar_hp[0] == h0 - 4 && a.vr_ar_hp[2] == h2 - 4 && hb - a.health == 16,
        sprintf("pellets: cuirass %g -> %g, pauldron %g -> %g, health took %g (16)", h0, a.vr_ar_hp[0], h2, a.vr_ar_hp[2], hb - a.health));

    // A blade's cut under the pauldron: stopped; once it broke, let through.
    VR_AgArmor_Give(a, 1, 1023);
    vr_decap_ent = a;
    vr_decap_time = time;
    vr_decap_limb = 0;
    vr_decap_at = pp;
    a.frame = f0;
    VR_ART_Hit(a, pp, 2, 1);
    VR_ART_Check(vr_decap_ent == world, "a cut under the unbroken pauldron stopped");
    a.vr_ar_hp[2] = 1;
    a.frame = f0;
    VR_ART_Hit(a, pp, 2, 10);
    vr_decap_ent = a;
    vr_decap_time = time;
    vr_decap_limb = 0;
    vr_decap_at = pp;
    a.frame = f0;
    VR_ART_Hit(a, pp, 2, 1);
    VR_ART_Check(!VR_AgArmor_On(a, 2) && vr_decap_ent == a, "a cut once the pauldron broke let through");
    vr_decap_ent = world;

    // The bare grunt: the whole of it.
    d = VR_ART_Hit(vr_art_b, limbpiece(a, 0, 10) - a.origin + vr_art_b.origin, 0, 10);
    float d2 = VR_ART_Hit(vr_art_b, vr_art_b.origin, 1, 13);
    VR_ART_Check(d == 10 && d2 == 13 && vr_art_b.vr_ar_type == 0, sprintf("an unarmoured grunt takes all of it (%g of 10, %g of 13)", d, d2));

    // A gibbing sheds every plate left.
    VR_AgArmor_Give(a, 2, 1023 - 1 - 16);
    made = vr_ar_debris_made;
    VR_AgArmor_Shed(a, -40);
    VR_ART_Check(vr_ar_debris_made == made + 8 && a.vr_ar_mask == 0, sprintf("gibbed: %g plates shed (8)", vr_ar_debris_made - made));

    // The skill rolls: 200 spawns each.
    float sk;
    for(sk = 0; sk < 4; sk++)
    {
        string base = strcat("vr_agarmor_skill", ftos(sk));
        float chance = cvar(base);
        float best = cvar(strcat(base, "_type"));
        float want = cvar(strcat(base, "_pieces"));
        float got = 0;
        float bad = 0;
        for(i = 0; i < 200; i++)
        {
            VR_AgArmor_Roll(sk, a.origin + [i * 37, i * 11, 0]);
            if(vr_ar_roll_type > 0)
            {
                got = got + 1;
                n = VR_ART_Count(vr_ar_roll_mask);
                bad = bad + (vr_ar_roll_type > best || n != want || !(vr_ar_roll_mask & 1));
            }
        }
        VR_ART_Check(fabs(got / 200 - chance) <= 0.05 && bad == 0,
            sprintf("skill %g: %g of 200 armoured (chance %g), %g of them off the cap or the count", sk, got, chance, bad));
    }

    // P2: damage marks. Green's cuirass pool 31: 11 leaves 20 (0.65: dented), 10 more leaves 10 (0.32: battered).
    cvar_set("vr_agarmor_rule", "1");
    VR_AgArmor_Give(a, 1, 1023);
    a.frame = f0;
    VR_ART_Hit(a, pc, 0, 11);
    float w1 = a.vr_ar_wear;
    a.frame = f0;
    VR_ART_Hit(a, pc, 0, 10);
    VR_ART_Check(w1 == 1 && a.vr_ar_wear == 1 + 1024 && a.vr_ar_hp[0] == 10,
        sprintf("wear: the cuirass at 20 of 31 dented (wear %g), at 10 battered (%g)", w1, a.vr_ar_wear));
    cvar_set("vr_agarmor_rule", "0");
};

// Where a hit on plate `piece` of `c` lands, as the game resolves it (ragdollbone's QVR_ARMOR_PIECE_AT). The cuirass and
// the helmet go by their bones (vr_agarmor.cpp pieceAt): their middle. A limb plate by a player's shot at its middle
// (limbpiece 10), from ahead of `c`, then its side, the other side, behind, traced against the drawn model
// (MOVE_HITMODEL, as bullets are), the piece struck where it lands. The plate's own piece if a shot finds it; else what
// the shot from ahead struck (-1 no plate, -3 it missed `c`). (A limb plate's middle itself is no probe: a big plate's
// sits inside the limb it wraps, nearer another bone's skin: the death knight's pauldrons, his shins under the skirt's
// hem, found no plate there, while shots at them strike them.)
float(entity c, float piece) VR_ART_PlateShot =
{
    vector mid = limbpiece(c, piece, 10);
    if(piece <= 1)
    {
        return ragdollbone(c, mid, QVR_ARMOR_PIECE_AT);
    }
    makevectors([0, c.angles_y, 0]);
    vector fwd = v_forward;
    vector side = v_right * (((mid - c.origin) * v_right) < 0 ? -1 : 1);
    entity pl = VR_Player();
    float k, got, first = -3;
    for(k = 0; k < 4; k++)
    {
        vector out = k == 0 ? fwd : k == 1 ? side : k == 2 ? -side : -fwd;
        traceline(mid + out * 64, mid - out * 24, MOVE_HITMODEL, pl);
        got = trace_ent == c ? ragdollbone(c, trace_endpos, QVR_ARMOR_PIECE_AT) : -3;
        if(got == piece)
        {
            return got;
        }
        first = k == 0 ? got : first;
    }
    return first;
};

// P2: the enforcer: its pieces at their places, the power pack no plate, a break's debris its own, the skill's roll
// its too; its wear set for the client's word (state 2).
void() VR_ART_RunEnforcer =
{
    entity c = vr_art_c;
    float d, hp;
    float f0 = c.frame;
    vector pc = limbpiece(c, 0, 10);
    vector ph = limbpiece(c, 1, 10);
    vector pp = limbpiece(c, 3, 10);
    float i, found, bad = 0;
    string miss = "";
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        found = VR_ART_PlateShot(c, i);
        bad = bad + (found != i);
        miss = found != i ? strcat(miss, sprintf(" %g:%g", i, found)) : miss;
    }
    VR_ART_Check(c.model == "progs/vrenforcer.mdl" && bad == 0,
        sprintf("enforcer (%s): a shot at each plate strikes that plate (wrong: %g%s)", c.model, bad, miss));
    // Its power pack (behind the cuirass's cut: rest x -20): no plate there, the whole of a hit.
    makevectors(c.angles);
    vector pack = c.origin - v_forward * 20 + v_up * 15;
    float cp = ragdollbone(c, pack, QVR_ARMOR_PIECE_AT);
    cvar_set("vr_agarmor_rule", "1");
    VR_AgArmor_Give(c, 2, 1023);
    hp = c.vr_ar_hp[0];
    c.frame = f0;
    d = VR_ART_Hit(c, pack, 0, 10);
    VR_ART_Check(cp == -1 && d == 10 && c.vr_ar_hp[0] == hp, sprintf("enforcer: the power pack no plate (piece %g), 10 there takes %g", cp, d));
    c.frame = f0;
    d = VR_ART_Hit(c, pc, 0, 10);
    VR_ART_Check(d == 0 && c.vr_ar_hp[0] == hp - 10, sprintf("enforcer: rule 1: 10 on the cuirass takes %g (0), pool %g", d, c.vr_ar_hp[0]));
    float made = vr_ar_debris_made;
    c.frame = f0;
    d = VR_ART_Hit(c, pp, 2, 60);
    entity deb = find(world, classname, "vr_ar_debris");
    while(deb != world && deb.model != "progs/vrenforcer.mdl#plate3")
    {
        deb = find(deb, classname, "vr_ar_debris");
    }
    VR_ART_Check(!VR_AgArmor_On(c, 3) && vr_ar_debris_made == made + 1 && deb != world && deb.skin == 1 + 6,
        sprintf("enforcer: the right pauldron broken, its debris %s (skin %g: the battered yellow)", deb.model, deb.skin));
    cvar_set("vr_agarmor_rule", "0");
    // The skill's roll dresses enforcers too (this skill's chance at 1 for one spawn).
    string base = strcat("vr_agarmor_skill", ftos(max(0, min(3, floor(skill)))));
    float chance = cvar(base);
    cvar_set(base, "1");
    c.vr_ar_type = 0;
    c.vr_ar_mask = 0;
    VR_AgArmor_Spawn(c);
    float rolled = c.vr_ar_type;
    cvar_set(base, ftos(chance));
    VR_ART_Check(rolled > 0 && (c.vr_ar_mask & 1), sprintf("enforcer: the skill's roll dresses it (type %g, pieces %g)", rolled, c.vr_ar_mask));
    // Its wear for the client: yellow (cuirass pool 47, helmet 19): the cuirass to 15 (battered), the helmet to 12 (dented).
    VR_AgArmor_Give(c, 2, 1023);
    cvar_set("vr_agarmor_rule", "1");
    c.frame = f0;
    VR_ART_Hit(c, pc, 0, 32);
    c.frame = f0;
    VR_ART_Hit(c, ph, 0, 7); // (no headshot's multiplier: the piece is the point's)
    cvar_set("vr_agarmor_rule", "0");
    vr_art_cwear = c.vr_ar_wear;
    VR_ART_Check(vr_art_cwear == 1 + 2 + 1024, sprintf("enforcer: wear on the server %g (1027: the cuirass battered, the helmet dented)", vr_art_cwear));
};

// `vr_agarmor_test 7`: the knights' armour, `c` one of them (its model `mdl`, its name `who`), by vr_agarmor_rule 1 as
// the grunt's and the enforcer's: its pieces at their places (its rig's), the cuirass taking a hit, the right pauldron
// broken into its own debris, a blade's blow on the cuirass sparking (no blood), the skill's roll dressing it; then its
// wear set for the client's word (state 2). The wear: yellow, the cuirass to 15 (battered), the helmet to 12 (dented).
float vr_art_mode;      // 7: the knights' run (state 1 and 2 theirs)
entity vr_art_k, vr_art_dk; // the armed knight, the armed death knight
float vr_art_kwear, vr_art_dkwear;

float(entity c, string mdl, string who) VR_ART_RunKnight =
{
    float d, hp, s0, b0;
    float f0 = c.frame;
    vector pc = limbpiece(c, 0, 10);
    vector ph = limbpiece(c, 1, 10);
    vector pp = limbpiece(c, 3, 10);
    float i, found, bad = 0;
    string miss = "";
    for(i = 0; i < VR_AR_PIECES; i++)
    {
        found = VR_ART_PlateShot(c, i);
        bad = bad + (found != i);
        miss = found != i ? strcat(miss, sprintf(" %g:%g", i, found)) : miss;
    }
    VR_ART_Check(c.model == mdl && bad == 0,
        sprintf("%s (%s): a shot at each plate strikes that plate (wrong: %g%s)", who, c.model, bad, miss));
    cvar_set("vr_agarmor_rule", "1");
    VR_AgArmor_Give(c, 2, 1023);
    hp = c.vr_ar_hp[0];
    c.frame = f0;
    d = VR_ART_Hit(c, pc, 0, 10);
    VR_ART_Check(d == 0 && c.vr_ar_hp[0] == hp - 10, sprintf("%s: rule 1: 10 on the cuirass takes %g (0), pool %g", who, d, c.vr_ar_hp[0]));
    float made = vr_ar_debris_made;
    c.frame = f0;
    d = VR_ART_Hit(c, pp, 2, 60);
    string want = strcat(mdl, "#plate3");
    entity deb = find(world, classname, "vr_ar_debris");
    while(deb != world && deb.model != want)
    {
        deb = find(deb, classname, "vr_ar_debris");
    }
    VR_ART_Check(!VR_AgArmor_On(c, 3) && vr_ar_debris_made == made + 1 && deb != world && deb.skin == 1 + 6,
        sprintf("%s: the right pauldron broken, its debris %s (skin %g: the battered yellow)", who, deb.model, deb.skin));
    // A blade's blow on the cuirass (VR_HitBlood: the melee's blood asks the plate): sparks, no blood.
    VR_AgArmor_Give(c, 3, 1023);
    makevectors(c.angles);
    s0 = vr_ar_fx_sparks;
    b0 = vr_blood_sent;
    VR_HitBlood(c, pc, v_forward * 200, 4);
    VR_ART_Check(vr_ar_fx_sparks == s0 + 1 && vr_blood_sent == b0,
        sprintf("%s: a blow of 4 on a red cuirass: sparks (%g), no blood (%g)", who, vr_ar_fx_sparks - s0, vr_blood_sent - b0));
    cvar_set("vr_agarmor_rule", "0");
    // The skill's roll dresses it too (this skill's chance at 1 for one spawn).
    string base = strcat("vr_agarmor_skill", ftos(max(0, min(3, floor(skill)))));
    float chance = cvar(base);
    cvar_set(base, "1");
    c.vr_ar_type = 0;
    c.vr_ar_mask = 0;
    VR_AgArmor_Spawn(c);
    float rolled = c.vr_ar_type;
    float rmask = c.vr_ar_mask;
    cvar_set(base, ftos(chance));
    VR_ART_Check(rolled > 0 && (rmask & 1), sprintf("%s: the skill's roll dresses it (type %g, pieces %g)", who, rolled, rmask));
    VR_AgArmor_Give(c, 2, 1023);
    cvar_set("vr_agarmor_rule", "1");
    c.frame = f0;
    VR_ART_Hit(c, pc, 0, 32);
    c.frame = f0;
    VR_ART_Hit(c, ph, 0, 7);
    cvar_set("vr_agarmor_rule", "0");
    VR_ART_Check(c.vr_ar_wear == 1 + 2 + 1024, sprintf("%s: wear on the server %g (1027: the cuirass battered, the helmet dented)", who, c.vr_ar_wear));
    return c.vr_ar_wear;
};

// The client's word on `c` (type, pieces, wear) as the server's.
float(entity c, float wear) VR_ART_Heard =
{
    return ragdollbone(c, '2 0 0', QVR_ARMOR_CLIENT) == wear && ragdollbone(c, '0 0 0', QVR_ARMOR_CLIENT) == c.vr_ar_type &&
        ragdollbone(c, '1 0 0', QVR_ARMOR_CLIENT) == c.vr_ar_mask;
};

float vr_aft_busy; // `vr_agarmor_test 5` (vr_agarmor_fx_test.qc) running
void(float start) VR_AgArmor_FxTestFrame;

void() VR_AgArmor_TestFrame =
{
    if(vr_aft_busy)
    {
        VR_AgArmor_FxTestFrame(0);
        return;
    }
    if(vr_art_state == 0)
    {
        if(cvar("vr_agarmor_test") == 0)
        {
            return;
        }
        float request = cvar("vr_agarmor_test");
        cvar_set("vr_agarmor_test", "0");
        entity p = VR_Player();
        if(!p)
        {
            return;
        }
        if(request == 5)
        {
            VR_AgArmor_FxTestFrame(1); // (ragdolls, cuts, debris, sparks: vr_agarmor_fx_test.qc)
            return;
        }
        if(request == 2)
        {
            // (the eyeshots: green, yellow and red full sets side by side, facing the player)
            VR_AgArmor_Give(VR_ART_Spawn(p, -44), 1, 1023);
            VR_AgArmor_Give(VR_ART_Spawn(p, 0), 2, 1023);
            VR_AgArmor_Give(VR_ART_Spawn(p, 44), 3, 1023);
            return;
        }
        if(request == 3)
        {
            // (P2's eyeshots: enforcers in full green (facing), yellow (side on) and red (its back: the power pack) sets)
            entity e3 = VR_ART_SpawnKind(p, -44, 1, 128);
            VR_AgArmor_Give(e3, 1, 1023);
            e3 = VR_ART_SpawnKind(p, 0, 1, 128);
            e3.angles_y = e3.angles_y + 90;
            VR_AgArmor_Give(e3, 2, 1023);
            e3 = VR_ART_SpawnKind(p, 44, 1, 128);
            e3.angles_y = e3.angles_y + 180;
            VR_AgArmor_Give(e3, 3, 1023);
            return;
        }
        if(request == 6)
        {
            // (the helmets' eyeshots: a grunt and an enforcer close ahead in full yellow, facing the player;
            // vr_agarmor_headcam puts the mock eyes on their heads)
            VR_AgArmor_Give(VR_ART_SpawnKind(p, -40, 0, 80), 2, 1023);
            VR_AgArmor_Give(VR_ART_SpawnKind(p, 40, 1, 80), 2, 1023);
            return;
        }
        if(request == 8)
        {
            // (the knights' eyeshots: a knight and a death knight close ahead in full yellow, facing the player)
            VR_AgArmor_Give(VR_ART_SpawnKind(p, -44, 2, 110), 2, 1023);
            VR_AgArmor_Give(VR_ART_SpawnKind(p, 44, 3, 130), 2, 1023);
            return;
        }
        if(request == 7)
        {
            VR_Test_Begin("agarmor test");
            vr_art_rule = cvar("vr_agarmor_rule");
            vr_art_helm = cvar("vr_agarmor_helmet_head");
            vr_art_cuts = cvar("vr_agarmor_block_cuts");
            vr_art_dtime = cvar("vr_agarmor_debris_time");
            cvar_set("vr_agarmor_helmet_head", "1");
            cvar_set("vr_agarmor_block_cuts", "1");
            cvar_set("vr_agarmor_debris_time", "0.5");
            vr_art_k = VR_ART_SpawnKind(p, -48, 2, 128);
            vr_art_dk = VR_ART_SpawnKind(p, 48, 3, 160);
            vr_art_mode = 7;
            vr_art_state = 1;
            return; // (their poses first drawn)
        }
        if(request == 4)
        {
            // (P2's eyeshots: three grunts in full yellow, every piece whole, at two thirds, at a third of its pool)
            float k, i;
            for(k = 0; k < 3; k++)
            {
                entity g = VR_ART_SpawnKind(p, -36 + 36 * k, 0, 96);
                VR_AgArmor_Give(g, 2, 1023);
                for(i = 0; i < VR_AR_PIECES; i++)
                {
                    g.vr_ar_hp[i] = floor(VR_AgArmor_Full(2, i) * (1 - k / 3));
                    VR_AgArmor_Wear(g, i);
                }
            }
            return;
        }
        VR_Test_Begin("agarmor test");
        vr_art_rule = cvar("vr_agarmor_rule");
        vr_art_helm = cvar("vr_agarmor_helmet_head");
        vr_art_cuts = cvar("vr_agarmor_block_cuts");
        vr_art_dtime = cvar("vr_agarmor_debris_time");
        cvar_set("vr_agarmor_helmet_head", "1");
        cvar_set("vr_agarmor_block_cuts", "1");
        cvar_set("vr_agarmor_debris_time", "0.5");
        vr_art_a = VR_ART_Spawn(p, -40);
        vr_art_b = VR_ART_Spawn(p, 40);
        vr_art_c = VR_ART_SpawnKind(p, 0, 1, 200);
        vr_art_state = 1;
        return; // (their poses first drawn)
    }
    if(vr_art_state == 1 && vr_art_mode == 7)
    {
        vr_art_kwear = VR_ART_RunKnight(vr_art_k, "progs/vrknight.mdl", "knight");
        vr_art_dkwear = VR_ART_RunKnight(vr_art_dk, "progs/vrhknight.mdl", "death knight");
        vr_art_state = 2;
        vr_art_until = time + 5;
        return;
    }
    if(vr_art_mode == 7)
    {
        // (the debris fades out; the knights' wear reaches the client)
        float kh = VR_ART_Heard(vr_art_k, vr_art_kwear) && VR_ART_Heard(vr_art_dk, vr_art_dkwear);
        if((find(world, classname, "vr_ar_debris") != world || !kh) && time < vr_art_until)
        {
            return;
        }
        VR_ART_Check(kh, sprintf("knights: the client's word (wear %g, %g; the server's %g, %g)", ragdollbone(vr_art_k, '2 0 0', QVR_ARMOR_CLIENT),
            ragdollbone(vr_art_dk, '2 0 0', QVR_ARMOR_CLIENT), vr_art_kwear, vr_art_dkwear));
        VR_ART_Check(find(world, classname, "vr_ar_debris") == world, "the debris gone after its fade (else timed out)");
        remove(vr_art_k);
        remove(vr_art_dk);
        cvar_set("vr_agarmor_rule", ftos(vr_art_rule));
        cvar_set("vr_agarmor_helmet_head", ftos(vr_art_helm));
        cvar_set("vr_agarmor_block_cuts", ftos(vr_art_cuts));
        cvar_set("vr_agarmor_debris_time", ftos(vr_art_dtime));
        vr_art_mode = 0;
        vr_art_state = 0;
        VR_Test_Verdict();
        return;
    }
    if(vr_art_state == 1)
    {
        VR_ART_Run();
        VR_ART_RunEnforcer();
        vr_art_state = 2;
        vr_art_until = time + 5;
        return;
    }
    // The debris fades out (vr_agarmor_debris_time 0.5, then a second's fade); the enforcer's wear reaches the client
    // (QVR_SVC_ARMOR: the server's frame end, then the client's read).
    entity c = vr_art_c;
    float heard = ragdollbone(c, '2 0 0', QVR_ARMOR_CLIENT) == vr_art_cwear && ragdollbone(c, '0 0 0', QVR_ARMOR_CLIENT) == c.vr_ar_type &&
        ragdollbone(c, '1 0 0', QVR_ARMOR_CLIENT) == c.vr_ar_mask;
    if((find(world, classname, "vr_ar_debris") != world || !heard) && time < vr_art_until)
    {
        return;
    }
    VR_ART_Check(heard, sprintf("enforcer: the client's word: type %g, pieces %g, wear %g (the server's %g, %g, %g)",
        ragdollbone(c, '0 0 0', QVR_ARMOR_CLIENT), ragdollbone(c, '1 0 0', QVR_ARMOR_CLIENT), ragdollbone(c, '2 0 0', QVR_ARMOR_CLIENT),
        c.vr_ar_type, c.vr_ar_mask, vr_art_cwear));
    VR_ART_Check(find(world, classname, "vr_ar_debris") == world, "the debris gone after its fade (else timed out)");
    remove(vr_art_a);
    remove(vr_art_b);
    remove(vr_art_c);
    cvar_set("vr_agarmor_rule", ftos(vr_art_rule));
    cvar_set("vr_agarmor_helmet_head", ftos(vr_art_helm));
    cvar_set("vr_agarmor_block_cuts", ftos(vr_art_cuts));
    cvar_set("vr_agarmor_debris_time", ftos(vr_art_dtime));
    vr_art_state = 0;
    VR_Test_Verdict();
};
