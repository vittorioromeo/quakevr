#ifdef QVR_TESTS
// vr_arena_perks_test.qc -- the Survival Arena's perk tests (vr_arena_perks.qc). `vr_arena_perks_test N`
// (Debug > Automation & Self-Tests; VR_ARENA_PERKS_Test_Frame, world.qc's StartFrame), the arena running
// (`vr_arena 1` before the map, or the map "vrarena") and the player kept alive (`god`); each ends on
// `arena_perks: N passed, N failed` (run.sh -QuitOn "arena_perks: [0-9]+ passed").
// - 1 the pool and its knobs: seven distinct perks, each named, described and wearing its own model; the label
//   line; the taken perks kept on the director, never twice; how many are left to offer; the offer count clamped.
// - 2 the offer: three pedestals after a boss wave, each another perk, each with its model and a trigger's touch
//   box; a new offer replaces the ones standing; a taken perk is never offered again; the knobs; walking over one
//   pedestal takes that perk and clears the other two, while a foe walking over one means nothing.

// ---------------------------------------------------------------------------
// The test's own state
// ---------------------------------------------------------------------------

float ark_t_case;        // 0 idle; 1, 2, ... running
float ark_t_pass;
float ark_t_fail;
float ark_t_step;
float ark_t_until;
float ark_t_quad_at;     // test 3: when the quad a hand kill gave is over

// Test 3 borrows a live player, and puts him back as it found him.
entity ark_t_p;
float ark_t_health0; float ark_t_max0; float ark_t_armor0; float ark_t_items0;
float ark_t_shells0; float ark_t_quad0; float ark_t_saved;
void() VR_APK3_Restore;
float ark_t_keep[5];     // the cvars the tests set, put back at the end
float ark_t_taken0;      // the perks the run had already taken
string ark_t_prefix;     // its verdict line ("arena_perks"; the full-map check prints `arena_full: ...`)

string(float i) VR_APK_CvarName =
{
    if(i == 0) { return "vr_arena_perks"; }
    if(i == 1) { return "vr_arena_perk_count"; }
    if(i == 2) { return "vr_arena_max_alive"; }
    if(i == 3) { return "vr_arena_wave_delay"; }
    if(i == 4) { return "vr_arena_skill_impact"; }
    return "";
}
float VR_APK_NCVARS = 5;

void() VR_APK_Save =
{
    float i;
    for(i = 0; i < VR_APK_NCVARS; i++) { ark_t_keep[i] = cvar(VR_APK_CvarName(i)); }
}

void() VR_APK_Restore =
{
    float i;
    for(i = 0; i < VR_APK_NCVARS; i++) { cvar_set(VR_APK_CvarName(i), ftos(ark_t_keep[i])); }
}

// The tests' settings: the perk file's own defaults, so a check says what the perk says and not what a config said.
void() VR_APK_SetDefaults =
{
    cvar_set("vr_arena_perks", "1");
    cvar_set("vr_arena_perk_count", "3");
    cvar_set("vr_arena_skill_impact", "0"); // (the arena these tests drive is its knobs themselves: no difficulty in the way)
}

void(float ok, string what) VR_APK_Check =
{
    if(ok) { ark_t_pass = ark_t_pass + 1; return; }
    ark_t_fail = ark_t_fail + 1;
    dprint(sprintf("arena_perks: fail %s\n", what));
}

void() VR_APK_End =
{
    dprint(sprintf("%s: %g passed, %g failed\n", ark_t_prefix, ark_t_pass, ark_t_fail));
    VR_APK3_Restore(); // (test 3 borrows a live player's health, armour, guns and quad)
    VR_APK_Restore();
    if(ar_mgr) { ar_mgr.ark_taken = ark_t_taken0; } // (the run's perks are the test's, not the player's)
    cvar_set("vr_arena_perks_test", "0");
    ark_t_case = 0;
}

// ---------------------------------------------------------------------------
// Test 2: the offer (vrtesthall with vr_arena 1, or vrarena; god)
// ---------------------------------------------------------------------------

// The pedestals standing, collected into one bit mask (0: none).
float() VR_APK2_PedBits =
{
    float bits = 0;
    entity ped;
    for(ped = find(world, classname, ARK_PED_NAME); ped; ped = find(ped, classname, ARK_PED_NAME))
    {
        bits = bits | ped.ark_perk;
    }
    return bits;
}

void() VR_APK2_OfferChecks =
{
    VR_APK_Check(VR_ARK_CountPeds() == 3, "a boss wave's clear offers three pedestals");

    entity ped;
    float bits = 0;
    float apart = TRUE;
    for(ped = find(world, classname, ARK_PED_NAME); ped; ped = find(ped, classname, ARK_PED_NAME))
    {
        if((bits & ped.ark_perk) != 0) { apart = FALSE; }
        bits = bits | ped.ark_perk;
        if(!VR_ARK_CanOffer(ped.ark_perk)) { apart = FALSE; }
        if(ped.modelindex <= 0) { apart = FALSE; }         // (its perk's shipped model really loaded)
        if(ped.solid != SOLID_TRIGGER) { apart = FALSE; }  // (a touch box: nobody walks into a pedestal)
    }
    VR_APK_Check(apart, "the three stand apart: another perk each, a model each, a trigger each");
    VR_APK_Check((bits != 0) && VR_ARK_CountTaken() == 0, "an offer standing takes nothing by itself");
    VR_APK_Check(VR_ARK_OfferTurn(5) != VR_ARK_OfferTurn(10), "a later boss turns to another three");

    // The first offer's pedestals are the next boss wave's to replace, and a perk already taken is not offered.
    VR_ARK_Take(ARK_QUAD);
    VR_ARK_Offer(10);
    float later = VR_APK2_PedBits();
    VR_APK_Check(VR_ARK_CountPeds() == 3, "a new offer replaces the pedestals standing, it does not add to them");
    VR_APK_Check((later & ARK_QUAD) == 0, "a perk this run took is not offered again");
    VR_APK_Check(later != 0, "and it still offers three");

    // A knob off, and a pool used up: an offer stands nothing.
    cvar_set("vr_arena_perks", "0");
    VR_ARK_Offer(15);
    VR_APK_Check(VR_ARK_CountPeds() == 0, "vr_arena_perks 0: a boss wave's clear offers nothing");
    cvar_set("vr_arena_perks", "1");

    cvar_set("vr_arena_perk_count", "2");
    VR_ARK_Offer(20);
    VR_APK_Check(VR_ARK_CountPeds() == 2, "vr_arena_perk_count says how many pedestals stand");
    cvar_set("vr_arena_perk_count", "3");

    float i;
    for(i = 0; i < ARK_POOL_N; i++) { VR_ARK_Take(VR_ARK_PoolBit(i)); }
    VR_ARK_Offer(25);
    VR_APK_Check(VR_ARK_CountPeds() == 0, "a run that took every perk is offered nothing");
    ar_mgr.ark_taken = 0;
    VR_ARK_ClearPeds();
}

void() VR_APK2_TouchChecks =
{
    entity p = VR_ARENA_FindPlayer();
    if(p == world)
    {
        VR_APK_Check(FALSE, "no player is up to choose (god, and the map's own player)");
        return;
    }

    VR_ARK_Offer(5);
    entity ped = find(world, classname, ARK_PED_NAME);
    if(!ped)
    {
        VR_APK_Check(FALSE, "the offer's pedestals are not in the field");
        return;
    }
    float chosen = ped.ark_perk;

    entity keep = self;
    self = ped;
    other = p;
    VR_ARK_PedTouch();
    self = keep;

    VR_APK_Check(VR_ARK_Has(chosen), "walking over a pedestal takes its perk");
    VR_APK_Check(VR_ARK_CountTaken() == 1, "and exactly one perk: the choice is one of three");
    VR_APK_Check(ar_mgr.ark_taken == chosen, "the perks not chosen stay untaken");
    VR_APK_Check(VR_ARK_CountPeds() == 0, "the other two pedestals go away with it");

    // A monster walking over a pedestal means nothing.
    VR_ARK_Offer(5);
    ped = find(world, classname, ARK_PED_NAME);
    entity foe = find(world, classname, "monster_enforcer");
    if(!foe) { foe = find(world, classname, "monster_grunt"); }
    if(foe)
    {
        entity keep2 = self;
        entity keep3 = other;
        self = ped;
        other = foe;
        VR_ARK_PedTouch();
        self = keep2;
        other = keep3;
        VR_APK_Check((VR_ARK_CountTaken() == 1) && (VR_ARK_CountPeds() == 3), "a foe walking over a pedestal changes nothing");
    }
    else
    {
        VR_APK_Check(TRUE, "no foe in this run to walk over a pedestal (nothing to check)");
    }
    VR_ARK_ClearPeds();
}

void() VR_APK2_Begin =
{
    ark_t_step = 1;

    if(!ar_mgr)
    {
        VR_APK_Check(FALSE, "the arena's director is not in this map (vr_arena 1 before the map, or vrarena)");
        VR_APK_End();
        return;
    }
    if(VR_ARK_CountPeds()) { VR_ARK_ClearPeds(); }
    ar_mgr.ark_taken = 0;
    VR_ARK_Offer(5); // (the fifth boss cleared: the offer, on the map's item points, or the ring if it has none)
}

void() VR_APK2_Frame =
{
    if(ark_t_step == 1) { ark_t_step = 2; return; } // (a frame, so the pedestals are properly in the field)
    if(ark_t_step == 2) { VR_APK2_OfferChecks(); ark_t_step = 3; return; }
    if(ark_t_step == 3) { VR_APK2_TouchChecks(); VR_APK_End(); return; }
}

// ---------------------------------------------------------------------------
// Test 1: the pool, its labels and its knobs (nothing in the field needed)
// ---------------------------------------------------------------------------

void() VR_APK1_Pool =
{
    float i;
    float bit;
    float seen = 0;
    float distinct = TRUE;

    for(i = 0; i < ARK_POOL_N; i++)
    {
        bit = VR_ARK_PoolBit(i);
        if(bit == 0) { distinct = FALSE; }
        if((seen & bit) != 0) { distinct = FALSE; }
        seen = seen | bit;
    }
    VR_APK_Check(distinct, "the pool holds seven distinct perks");
    VR_APK_Check(VR_ARK_PoolBit(ARK_POOL_N) == 0, "the pool has nothing past its last rung");

    // Named, described, and each pedestal wears a model of its own.
    float named = TRUE;
    for(i = 0; i < ARK_POOL_N; i++)
    {
        bit = VR_ARK_PoolBit(i);
        if(VR_ARK_Name(bit) == "") { named = FALSE; }
        if(VR_ARK_What(bit) == "") { named = FALSE; }
        if(VR_ARK_Model(bit) == "") { named = FALSE; }
        float j;
        for(j = i + 1; j < ARK_POOL_N; j++)
        {
            if(VR_ARK_Model(bit) == VR_ARK_Model(VR_ARK_PoolBit(j))) { named = FALSE; }
        }
    }
    VR_APK_Check(named, "every perk is named, described, and wears a model no other perk wears");

    // The line a pedestal is named by: the perk's name, then what it does.
    float labelled = TRUE;
    for(i = 0; i < ARK_POOL_N; i++)
    {
        bit = VR_ARK_PoolBit(i);
        string lab = VR_ARK_Label(bit);
        string nm = VR_ARK_Name(bit);
        if(strlen(lab) <= strlen(nm)) { labelled = FALSE; }
        if(substring(lab, 0, strlen(nm)) != nm) { labelled = FALSE; }
    }
    VR_APK_Check(labelled, "a pedestal's line reads its name and then what it does");

    // The knobs.
    cvar_set("vr_arena_perk_count", "9");
    VR_APK_Check(VR_ARK_Opt_Count() == ARK_POOL_N, "the offer count is clamped to the pool's size");
    cvar_set("vr_arena_perk_count", "0");
    VR_APK_Check(VR_ARK_Opt_Count() == 1, "at least one perk is offered");
    cvar_set("vr_arena_perk_count", "3");
    VR_APK_Check(VR_ARK_Opt_Count() == 3, "three perks are offered by default");

    cvar_set("vr_arena_perks", "0");
    VR_APK_Check(!VR_ARK_Opt_On(), "vr_arena_perks 0 turns the offer off");
    cvar_set("vr_arena_perks", "1");
    VR_APK_Check(VR_ARK_Opt_On(), "vr_arena_perks 1 (the default) leaves the offer on");
}

void() VR_APK1_State =
{
    float i;

    ar_mgr.ark_taken = 0;
    VR_APK_Check(!VR_ARK_Has(ARK_QUAD), "a fresh run has taken no perk");
    VR_APK_Check(VR_ARK_CountTaken() == 0, "a fresh run counts no perk taken");
    VR_APK_Check(VR_ARK_CountLeft() == ARK_POOL_N, "a fresh run may offer every perk in the pool");

    VR_APK_Check(VR_ARK_Take(ARK_QUAD), "a perk can be taken");
    VR_APK_Check(VR_ARK_Has(ARK_QUAD), "the director keeps the perk the player took");
    VR_APK_Check(!VR_ARK_Take(ARK_QUAD), "the same perk is not taken twice");
    VR_APK_Check(VR_ARK_CountTaken() == 1, "and the run holds exactly one");
    VR_APK_Check(!VR_ARK_CanOffer(ARK_QUAD), "a perk already taken is not offered again");
    VR_APK_Check(VR_ARK_CanOffer(ARK_FEED), "a perk not taken still may be offered");

    for(i = 0; i < ARK_POOL_N; i++) { VR_ARK_Take(VR_ARK_PoolBit(i)); }
    VR_APK_Check(VR_ARK_CountLeft() == 0, "a run that took every perk has nothing left to offer");
}

void() VR_APK1_Run =
{
    if(!ar_mgr)
    {
        VR_APK_Check(FALSE, "the arena's director is not in this map (vr_arena 1 before the map, or vrarena)");
        return;
    }
    VR_APK1_Pool();
    VR_APK1_State();
}

// ---------------------------------------------------------------------------
// Test 3: what each perk promises, in numbers (vrtesthall with vr_arena 1, or vrarena; god)
// ---------------------------------------------------------------------------

// The player the perks work on, and what of his the test touched, put back at the end (the state is at the top).
void() VR_APK3_Save =
{
    ark_t_p = VR_ARENA_FindPlayer();
    if(!ark_t_p) { return; }
    ark_t_saved = 1;
    ark_t_health0 = ark_t_p.health;
    ark_t_max0 = ark_t_p.max_health;
    ark_t_armor0 = ark_t_p.armorvalue;
    ark_t_items0 = ark_t_p.items;
    ark_t_shells0 = VR_AmmoUtil_GetAmmoCountByAmmoId(ark_t_p, AID_SHELLS);
    ark_t_quad0 = ark_t_p.super_damage_finished;
}

void() VR_APK3_Restore =
{
    if(!ark_t_saved || !ark_t_p) { return; }
    ark_t_p.armorvalue = ark_t_armor0;
    ark_t_p.items = ark_t_items0;
    ark_t_p.max_health = ark_t_max0;
    ark_t_p.health = ark_t_health0;
    if(ark_t_p.max_health > 0 && ark_t_p.health > ark_t_p.max_health) { ark_t_p.health = ark_t_p.max_health; }
    VR_AmmoUtil_SetAmmoCountByAmmoId(ark_t_p, AID_SHELLS, ark_t_shells0);
    ark_t_p.super_damage_finished = ark_t_quad0;
    ark_t_saved = 0;
}

// Numbers that come out of `time + x` are only ever equal to the tenth.
float(float xA, float xB) VR_APK_Near =
{
    return (xA > xB - 0.01) && (xA < xB + 0.01);
}

// `e` fell to `by`, worked as a swing of a hand (the quad perk reads vr_hitkind, as a real kill does).
void(entity e, entity by, float xKind) VR_APK3_Kill =
{
    vr_hitkind = xKind;
    VR_ARK_Kill(e, by);
}

void() VR_APK3_Off =
{
    ar_mgr.ark_taken = 0;

    ark_t_p.armorvalue = 0;
    VR_ARK_ArmorBonus(ark_t_p, 100);
    VR_APK_Check(ark_t_p.armorvalue == 0, "with no perk taken picked up armour is worth what it always was");

    ark_t_p.health = 50;
    VR_ARK_Decap(world, ark_t_p, 1);
    VR_APK_Check(ark_t_p.health == 50, "with no perk taken a beheading heals nobody");

    ark_t_p.super_damage_finished = 0;
    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_MELEE);
    VR_APK_Check(ark_t_p.health == 50, "with no perk taken a hand kill heals nobody");
    VR_APK_Check(ark_t_p.super_damage_finished == 0, "with no perk taken a hand kill gives no quad");

    VR_APK_Check(!VR_ARK_Kept(), "with no perk taken the drop cap works as shipped");
    VR_APK_Check(ark_t_p.max_health == VR_MaxHealth(ark_t_p), "with no perk taken health tops at what it always did");
}

void() VR_APK3_Quad =
{
    ar_mgr.ark_taken = ARK_QUAD;
    ark_t_p.super_damage_finished = 0;

    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_MELEE);
    VR_APK_Check(ark_t_p.super_damage_finished > time, "Quad Strike: a foe killed with the hands brings the quad");
    VR_APK_Check(VR_APK_Near(ark_t_p.super_damage_finished - time, ARK_QUAD_SECS), "and it lasts 2 seconds, no longer");
    VR_APK_Check(ark_t_p.super_time == 1, "and it is the shipped quad the arena borrows");

    ark_t_p.super_damage_finished = 0;
    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_SHOT);
    VR_APK_Check(ark_t_p.super_damage_finished == 0, "Quad Strike: a kill that was not a swing of a hand gives nothing");

    ark_t_p.super_damage_finished = 0;
    VR_APK3_Kill(world, world, QVR_HITKIND_MELEE);
    VR_APK_Check(ark_t_p.super_damage_finished == 0, "Quad Strike: a foe another foe killed is nobody's perk");

    ark_t_p.super_damage_finished = time + 25; // (a picked-up Quad Damage with 25 s left)
    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_MELEE);
    VR_APK_Check(VR_APK_Near(ark_t_p.super_damage_finished - time, 25), "Quad Strike never cuts a longer quad short");
    ark_t_p.super_damage_finished = 0;

    ark_t_quad_at = time + ARK_QUAD_SECS + 0.5; // (measured again once its 2 seconds are past)
}

void() VR_APK3_QuadLate =
{
    VR_APK_Check(ark_t_p.super_damage_finished <= time, "Quad Strike: and it is gone again 2 seconds later");
}

void() VR_APK3_Feed =
{
    ar_mgr.ark_taken = ARK_FEED;
    ar_mgr.ark_feed_at = 0;

    ark_t_p.health = 50;
    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_SHOT);
    VR_APK_Check(ark_t_p.health == 55, "Feeder: a kill restores 5 health");

    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_SHOT);
    VR_APK_Check(ark_t_p.health == 55, "Feeder: and no more than once a second");

    ar_mgr.ark_feed_at = time + 1;
    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_SHOT);
    VR_APK_Check(ark_t_p.health == 55, "Feeder: the next kill waits for its second");

    ar_mgr.ark_feed_at = time;
    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_SHOT);
    VR_APK_Check(ark_t_p.health == 60, "Feeder: once its second is past a kill restores 5 again");

    ark_t_p.health = ark_t_p.max_health - 2;
    ar_mgr.ark_feed_at = time; // (the last kill is the cap check, not the gap: let it feed)
    VR_APK3_Kill(world, ark_t_p, QVR_HITKIND_SHOT);
    VR_APK_Check(ark_t_p.health == ark_t_p.max_health, "Feeder: never past what health tops at");
}

void() VR_APK3_Snap =
{
    ar_mgr.ark_taken = ARK_SNAP;

    ark_t_p.health = 50;
    VR_ARK_Decap(world, ark_t_p, 1);
    VR_APK_Check(ark_t_p.health == 70, "Neck Snapper: a foe beheaded restores 20 health");

    ark_t_p.health = ark_t_p.max_health - 5;
    VR_ARK_Decap(world, ark_t_p, 1);
    VR_APK_Check(ark_t_p.health == ark_t_p.max_health, "Neck Snapper: never past what health tops at");

    ark_t_p.health = 50;
    VR_ARK_Decap(world, world, 1);
    VR_APK_Check(ark_t_p.health == 50, "Neck Snapper: a foe beheaded by someone who is not a player heals nobody");
}

void() VR_APK3_Iron =
{
    ar_mgr.ark_taken = ARK_IRON;

    ark_t_p.armorvalue = 0;
    VR_ARK_ArmorBonus(ark_t_p, 100);
    VR_APK_Check(ark_t_p.armorvalue == 125, "Iron Skin: green armour is worth 125, not 100");

    VR_ARK_ArmorBonus(ark_t_p, 150);
    VR_APK_Check(ark_t_p.armorvalue == 175, "Iron Skin: yellow armour is worth 175, not 150");

    VR_ARK_ArmorBonus(ark_t_p, 200);
    VR_APK_Check(ark_t_p.armorvalue == 225, "Iron Skin: red armour is worth 225, not 200");
    VR_APK_Check(ark_t_p.armorvalue - 200 == ARK_IRON_ARM, "Iron Skin: never more than 25 past what the armour is worth");
}

void() VR_APK3_Wind =
{
    ar_mgr.ark_taken = ARK_WIND;
    ark_t_p.max_health = VR_MaxHealth(ark_t_p);

    VR_ARK_SecondWind();
    VR_APK_Check(ark_t_p.max_health == VR_MaxHealth(ark_t_p) + ARK_WIND_HP, "Second Wind: health reaches 15 past its normal top");

    VR_ARK_SecondWind();
    VR_APK_Check(ark_t_p.max_health == VR_MaxHealth(ark_t_p) + ARK_WIND_HP, "Second Wind: and does not creep up when said again");

    ark_t_p.max_health = VR_MaxHealth(ark_t_p); // (as if a fresh level: back to the ordinary top, and full)
    ark_t_p.health = VR_MaxHealth(ark_t_p);
    VR_ARK_SecondWind();
    VR_APK_Check(ark_t_p.health == VR_MaxHealth(ark_t_p) + ARK_WIND_HP, "Second Wind: full at the old top comes up with it");
}

void() VR_APK3_Quart =
{
    ar_mgr.ark_taken = ARK_QUART;
    ark_t_p.items = IT_SHOTGUN;
    VR_AmmoUtil_SetAmmoCountByAmmoId(ark_t_p, AID_SHELLS, 10);

    VR_APK_Check(VR_ARK_Quartermaster(ark_t_p) == ARK_QUART_AMMO, "Quartermaster: 15 rounds for a wave cleared");
    VR_APK_Check(VR_AmmoUtil_GetAmmoCountByAmmoId(ark_t_p, AID_SHELLS) == 25, "Quartermaster: and the rounds are in the player");

    VR_AmmoUtil_SetAmmoCountByAmmoId(ark_t_p, AID_SHELLS, VR_MaxAmmo(ark_t_p, AID_SHELLS));
    VR_APK_Check(VR_ARK_Quartermaster(ark_t_p) == 0, "Quartermaster: nothing for a player already carrying all he may");

    VR_AmmoUtil_SetAmmoCountByAmmoId(ark_t_p, AID_SHELLS, 10);
    ark_t_p.items = 0;
    VR_APK_Check(VR_ARK_Quartermaster(ark_t_p) == 0, "Quartermaster: nothing for ammunition no gun of his uses");
    ark_t_p.items = IT_SHOTGUN;
    VR_AmmoUtil_SetAmmoCountByAmmoId(ark_t_p, AID_SHELLS, 10);

    VR_ARK_WaveDone(3); // (an ordinary wave: no boss, so no pedestals either)
    VR_APK_Check(VR_AmmoUtil_GetAmmoCountByAmmoId(ark_t_p, AID_SHELLS) == 25, "Quartermaster: a wave's clear is what hands it out");
    VR_APK_Check(VR_ARK_CountPeds() == 0, "and an ordinary wave clear still offers nothing");
}

void() VR_APK3_Spoils =
{
    ar_mgr.ark_taken = ARK_SPOIL;
    VR_APK_Check(VR_ARK_Kept(), "Spoils: taken, the cap on dropped enemy weapons leaves them where they fell");
    ar_mgr.ark_taken = 0;
    VR_APK_Check(!VR_ARK_Kept(), "Spoils: not taken, the cap works as shipped");
    VR_APK_Check(VR_ARK_Running(), "Spoils: and the perks only ever work inside a running arena run");
}

// A boss wave's clear drops that wave's medkits and armour AND makes the offer, and both read the map's
// info_arena_item points. A pedestal keeps its own space (VR_ARENA_PedPlace pushes it out from the map's centre),
// so a grabbable item is never standing inside a pedestal's trigger box.
float ARK_PED_GAP = 64; // (in the map, ignoring height) nearer than this and they are standing on each other

float(vector xA, vector xB) VR_APK_APart =
{
    vector d = xA - xB;
    d.z = 0;
    return vlen(d) > ARK_PED_GAP;
}

// How many pairs of things the arena made stand on one another: a pedestal on an item point (where a wave clear's
// medkit or armour lands), or a pedestal on a pedestal.
float() VR_APK_Clash =
{
    float n = 0;
    entity ped;
    entity spot;
    for(ped = find(world, classname, ARK_PED_NAME); ped; ped = find(ped, classname, ARK_PED_NAME))
    {
        for(spot = find(world, classname, ARK_PED_NAME); spot; spot = find(spot, classname, ARK_PED_NAME))
        {
            if(spot == ped) { continue; }
            if(!VR_APK_APart(ped.origin, spot.origin)) { n = n + 1; }
        }
        for(spot = find(world, classname, "info_arena_item"); spot; spot = find(spot, classname, "info_arena_item"))
        {
            if(!VR_APK_APart(ped.origin, spot.origin)) { n = n + 1; }
        }
    }
    return n;
}

// Pedestals that are not proper: no model, not a trigger, or the same perk standing twice.
float() VR_APK_BadPed =
{
    float bad = 0;
    float bits = 0;
    entity p;
    for(p = find(world, classname, ARK_PED_NAME); p; p = find(p, classname, ARK_PED_NAME))
    {
        if(!p.modelindex) { bad = bad + 1; }
        if(p.solid != SOLID_TRIGGER) { bad = bad + 1; }
        if((bits & p.ark_perk) != 0) { bad = bad + 1; }
        bits = bits | p.ark_perk;
    }
    return bad;
}

// A pedestal standing that offers `xBit` (the pool never offers a perk a run already took).
float(float xBit) VR_APK_HasPedPerk =
{
    entity p;
    for(p = find(world, classname, ARK_PED_NAME); p; p = find(p, classname, ARK_PED_NAME))
    {
        if(p.ark_perk == xBit) { return TRUE; }
    }
    return FALSE;
}

// Walking over the first pedestal standing: returns the perk it offered (0: none).
float() VR_APK_TouchOne =
{
    entity p = VR_ARENA_FindPlayer();
    if(p == world) { return 0; }
    entity ped = find(world, classname, ARK_PED_NAME);
    if(!ped) { return 0; }

    float bit = ped.ark_perk;
    entity save = self;
    self = ped;
    other = p;
    VR_ARK_PedTouch();
    self = save;
    return bit;
}

void() VR_APK3_Place =
{
    ar_mgr.ark_taken = 0;
    VR_ARK_ClearPeds();
    VR_ARK_Offer(5); // (wave 5 is a boss wave: this is what a real boss clear does)

    float want = VR_ARK_Opt_Count();
    VR_APK_Check(VR_ARK_CountPeds() == want, "a boss wave's clear puts up all the pedestals it offers");

    float spots = 0;
    entity spot;
    for(spot = find(world, classname, "info_arena_item"); spot; spot = find(spot, classname, "info_arena_item"))
    {
        spots = spots + 1;
    }
    VR_APK_Check(VR_APK_Clash() == 0, "no pedestal stands where a wave's drop lands, nor where another pedestal stands");
    VR_APK_Check(VR_APK_BadPed() == 0, "every pedestal wears a real item's model and offers its own perk");

    // and the wave's drops keep the points they had (the item cursor is put back where it was)
    float walk0 = ar_item_next;
    entity s0 = VR_ARENA_ItemSpot(walk0);
    entity s1 = VR_ARENA_ItemSpot(walk0 + 1);
    vector d0 = VR_ARENA_DropPlace(0);
    vector d1 = VR_ARENA_DropPlace(1);
    ar_item_next = walk0;
    if(s0 && s1)
    {
        VR_APK_Check(d0 == s0.origin, "a wave clear's drop still lands on the map's own item point");
        VR_APK_Check(d1 == s1.origin, "and the next drop on the next one, not on a pedestal");
    }
    VR_APK_Check(spots == 0 || spots == 3, "the map's item points are untouched by the offer");

    VR_ARK_ClearPeds();
}

void() VR_APK3_Begin =
{
    if(!ar_mgr)
    {
        VR_APK_Check(FALSE, "the arena's director is not in this map (vr_arena 1 before the map, or vrarena)");
        VR_APK_End();
        return;
    }
    VR_APK3_Save();
    if(!ark_t_p)
    {
        VR_APK_Check(FALSE, "no player to work on");
        VR_APK_End();
        return;
    }
    VR_ARK_ClearPeds();
    ar_mgr.ark_taken = 0;
    ar_mgr.ark_feed_at = 0;

    VR_APK3_Off();
    VR_APK3_Quad();
    ark_t_step = 1;
}

void() VR_APK3_Frame =
{
    if(ark_t_step == 1)
    {
        if(time < ark_t_quad_at) { return; }
        VR_APK3_QuadLate();
        ark_t_step = 2;
        return;
    }
    if(ark_t_step == 2) { VR_APK3_Feed(); ark_t_step = 3; return; }
    if(ark_t_step == 3) { VR_APK3_Snap(); ark_t_step = 4; return; }
    if(ark_t_step == 4) { VR_APK3_Iron(); ark_t_step = 5; return; }
    if(ark_t_step == 5) { VR_APK3_Wind(); ark_t_step = 6; return; }
    if(ark_t_step == 6) { VR_APK3_Quart(); ark_t_step = 7; return; }
    if(ark_t_step == 7) { VR_APK3_Place(); ark_t_step = 8; return; }

    VR_APK3_Spoils();
    VR_APK3_Restore();
    VR_APK_End();
}

// ---------------------------------------------------------------------------
// Test 4: the director and the perks together, in a real arena map (vrarena, or vrtesthall with vr_arena 1; god)
// ---------------------------------------------------------------------------

// One of the wave's foes, brought down the way the arena's own live test does -- through combat.qc, so the perks'
// hook fires the way it does in a real fight. 1 when a blow landed.
float() VR_APK4_KillOne =
{
    entity p = VR_ARENA_FindPlayer();
    if(p == world) { return 0; }

    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        entity next = findflags(e, ar_slot, ar_wave);
        if(e.health > 0 && e.takedamage) { T_Damage(e, p, p, e.health + 100); return 1; }
        e = next;
    }
    return 0;
}

void() VR_APK4_Begin =
{
    if(!ar_mgr)
    {
        VR_APK_Check(FALSE, "the arena's director is not in this map (vr_arena 1 before the map, or vrarena)");
        VR_APK_End();
        return;
    }
    VR_APK3_Save();
    VR_ARK_ClearPeds();
    ar_mgr.ark_taken = 0;
    cvar_set("vr_arena_max_alive", "2");   // (small waves: the drive to the first boss wave has to fit in a test)
    cvar_set("vr_arena_wave_delay", "1");
    ark_t_until = time + 90;
    ark_t_step = 1;
    dprint("arena_full: driving this map's arena to its first boss wave\n");
}

void() VR_APK4_OfferChecks =
{
    float boss = cvar("vr_arena_boss_every");

    VR_APK_Check(ar_wave >= boss, "the run reached its first boss wave");
    VR_APK_Check(VR_ARENA_IsBossWave(ar_mgr.ark_offer_wave), "the offer came from a boss wave's clear");
    VR_APK_Check(VR_ARK_CountTaken() == 0, "the offer itself takes no perk");
    VR_APK_Check(VR_ARK_CountPeds() == VR_ARK_Opt_Count(), "the boss wave's clear put up every pedestal it offers");
    VR_APK_Check(VR_APK_BadPed() == 0, "every pedestal wears a shipped item's model and offers its own perk");
    VR_APK_Check(VR_APK_Clash() == 0, "no pedestal stands where that wave's medkit or armour landed");

    float chosen = VR_APK_TouchOne();
    VR_APK_Check(chosen != 0, "walking over a pedestal is the choice");
    VR_APK_Check(VR_ARK_Has(chosen), "the perk taken stands on the director");
    VR_APK_Check(ar_mgr.ark_taken == chosen, "and it is exactly the one the player walked over");
    VR_APK_Check(VR_ARK_CountTaken() == 1, "one perk, no more");
    VR_APK_Check(VR_ARK_CountLeft() == ARK_POOL_N - 1, "the rest of the pool is still there to offer");
    VR_APK_Check(VR_ARK_CountPeds() == 0, "and the pedestals not walked over are gone");

    VR_ARK_Offer(2 * boss); // (the next boss wave's offer, asked for here rather than waited for)
    VR_APK_Check(VR_ARK_CountPeds() == VR_ARK_Opt_Count(), "a later boss wave offers again");
    VR_APK_Check(!VR_APK_HasPedPerk(chosen), "and never a perk this run already took");

    VR_ARK_ClearPeds();
    VR_APK3_Restore();
    VR_APK_End();
}

void() VR_APK4_Frame =
{
    if(ark_t_step == 1)
    {
        VR_APK4_KillOne();
        if(VR_ARK_CountPeds() > 0) { ark_t_step = 2; }
        return;
    }
    VR_APK4_OfferChecks();
}

// ---------------------------------------------------------------------------
// The frame (world.qc's StartFrame)
// ---------------------------------------------------------------------------

void() VR_ARENA_PERKS_Test_Frame =
{
    float c = cvar("vr_arena_perks_test");

    if(c == 0)
    {
        if(ark_t_case) { VR_APK_Restore(); ark_t_case = 0; dprint("arena_perks: test stopped\n"); }
        return;
    }

    if(ark_t_case == 0)
    {
        ark_t_case = c;
        ark_t_pass = 0;
        ark_t_fail = 0;
        ark_t_step = 0;
        ark_t_until = time + 30;
        ark_t_prefix = "arena_perks";
        if(c == 4) { ark_t_prefix = "arena_full"; } // (the full-map check: its verdict line is `arena_full: ...`)
        VR_APK_Save();
        ark_t_taken0 = 0;
        if(ar_mgr) { ark_t_taken0 = ar_mgr.ark_taken; }
        VR_APK_SetDefaults();
        dprint(sprintf("arena_perks: test %g begin\n", c));

        if(c == 1)
        {
            VR_APK1_Run();
            VR_APK_End();
            return;
        }
        if(c == 2)
        {
            VR_APK2_Begin();
            return;
        }
        if(c == 3)
        {
            VR_APK3_Begin();
            return;
        }
        if(c == 4)
        {
            VR_APK4_Begin();
            return;
        }

        VR_APK_Check(FALSE, "no such arena perk test");
        VR_APK_End();
        return;
    }

    if(ark_t_case != c) { return; } // (a different case asked for while one runs: the running one finishes)

    if(time > ark_t_until)
    {
        VR_APK_Check(FALSE, "the test timed out");
        VR_APK_End();
        return;
    }

    if(ark_t_case == 2) { VR_APK2_Frame(); return; }
    if(ark_t_case == 3) { VR_APK3_Frame(); return; }
    if(ark_t_case == 4) { VR_APK4_Frame(); return; }
}
#endif
