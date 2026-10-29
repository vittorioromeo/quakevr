#ifdef QVR_TESTS
// vr_arena_perks_test.qc -- the Survival Arena's perk tests (vr_arena_perks.qc). `vr_arena_perks_test N`
// (Debug > Automation & Self-Tests; VR_ARENA_PERKS_Test_Frame, world.qc's StartFrame), the arena running
// (`vr_arena 1` before the map, or the map "vrarena") and the player kept alive (`god`); each ends on
// `arena_perks: N passed, N failed` (run.sh -QuitOn "arena_perks: [0-9]+ passed"; 4 `arena_full: ...`, 5
// `arena_cards: ...`).
// - 1 the pool and its knobs: eight distinct perks, each named, described, with its own model and card colour; the
//   taken perks kept on the director, never twice; how many are left to offer; the offer count clamped.
// - 2 the offer: three cards after a boss wave, each another perk; a new offer replaces the open one; a taken perk
//   is never offered again; the knobs; picking a card takes that perk and closes the offer, a pick past the cards or
//   a second pick takes nothing.
// - 3 each perk's numbers, and the offer putting nothing in the field. 4 a real run to its first boss wave: the
//   arena held while the cards are up, the pick, the run going on.
// - 5 the engine's card panel end to end (vr_arenacards.cpp): the arena stands still while it shows the cards, the
//   mock laser and trigger pick one (and a screenshot of the panel), the countdown and the next wave follow.

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
float ark_t_keep[6];     // the cvars the tests set, put back at the end
float ark_t_taken0;      // the perks the run had already taken
string ark_t_prefix;     // its verdict line ("arena_perks"; the full-map check prints `arena_full: ...`)

string(float i) VR_APK_CvarName =
{
    if(i == 0) { return "vr_arena_perks"; }
    if(i == 1) { return "vr_arena_perk_count"; }
    if(i == 2) { return "vr_arena_max_alive"; }
    if(i == 3) { return "vr_arena_wave_rest"; }
    if(i == 4) { return "vr_arena_skill_impact"; }
    if(i == 5) { return "vr_arena_countdown"; }
    return "";
}
float VR_APK_NCVARS = 6;

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
    if(ar_mgr) { VR_ARK_CloseOffer(); }               // (a test's cards never left holding the arena)
    if(ark_t_case == 5) { localcmd("vr_mock_button main trigger 0\nvr_arena_cards_mock_laser off\n"); }
    cvar_set("vr_arena_perks_test", "0");
    ark_t_case = 0;
}

// ---------------------------------------------------------------------------
// Test 2: the offer (vrtesthall with vr_arena 1, or vrarena; god)
// ---------------------------------------------------------------------------

// The open offer's cards, collected into one bit mask (0: none open).
float() VR_APK2_CardBits =
{
    float bits = 0;
    float i;
    for(i = 0; i < VR_ARK_CardCount(); i++) { bits = bits | VR_ARK_CardBit(i); }
    return bits;
}

// The open offer's cards that are not proper: no perk, one the run took already, no picture, or a perk twice.
float() VR_APK_BadCard =
{
    float bad = 0;
    float bits = 0;
    float i;
    for(i = 0; i < VR_ARK_CardCount(); i++)
    {
        float bit = VR_ARK_CardBit(i);
        if(!VR_ARK_CanOffer(bit)) { bad = bad + 1; }
        if(VR_ARK_Model(bit) == "") { bad = bad + 1; }
        if((bits & bit) != 0) { bad = bad + 1; }
        bits = bits | bit;
    }
    return bad;
}

void() VR_APK2_OfferChecks =
{
    VR_APK_Check(VR_ARK_CardCount() == 3, "a boss wave's clear offers three cards");
    VR_APK_Check(VR_ARK_CardSerial() > 0, "and the offer is open for the panel to show");
    VR_APK_Check(VR_APK_BadCard() == 0, "the three are apart: another perk each, a picture each, none already taken");
    VR_APK_Check((VR_APK2_CardBits() != 0) && VR_ARK_CountTaken() == 0, "an offer standing takes nothing by itself");
    VR_APK_Check(VR_ARK_OfferTurn(5) != VR_ARK_OfferTurn(10), "a later boss turns to another three");
    VR_APK_Check(VR_ARK_CardBit(-1) == 0 && VR_ARK_CardBit(3) == 0, "there is no card past the offer's");

    // A new offer replaces the open one, and a perk already taken is not offered.
    float serial = VR_ARK_CardSerial();
    VR_ARK_Take(ARK_QUAD);
    VR_ARK_Offer(10);
    float later = VR_APK2_CardBits();
    VR_APK_Check(VR_ARK_CardCount() == 3, "a new offer replaces the cards open, it does not add to them");
    VR_APK_Check(VR_ARK_CardSerial() > serial, "and it is a new offer for the panel (placed in front again)");
    VR_APK_Check((later & ARK_QUAD) == 0, "a perk this run took is not offered again");
    VR_APK_Check(later != 0, "and it still offers three");

    // A knob off, and a pool used up: an offer opens nothing.
    cvar_set("vr_arena_perks", "0");
    VR_ARK_Offer(15);
    VR_APK_Check(VR_ARK_CardCount() == 0, "vr_arena_perks 0: a boss wave's clear offers nothing");
    cvar_set("vr_arena_perks", "1");

    cvar_set("vr_arena_perk_count", "2");
    VR_ARK_Offer(20);
    VR_APK_Check(VR_ARK_CardCount() == 2, "vr_arena_perk_count says how many cards are offered");
    cvar_set("vr_arena_perk_count", "3");

    float i;
    for(i = 0; i < ARK_POOL_N; i++) { VR_ARK_Take(VR_ARK_PoolBit(i)); }
    VR_ARK_Offer(25);
    VR_APK_Check(VR_ARK_CardCount() == 0, "a run that took every perk is offered nothing");
    ar_mgr.ark_taken = 0;
    VR_ARK_CloseOffer();
}

void() VR_APK2_PickChecks =
{
    VR_ARK_Offer(5);
    float n = VR_ARK_CardCount();
    float chosen = VR_ARK_CardBit(1);

    VR_APK_Check(!VR_ARK_CardPick(n), "a pick past the last card takes nothing");
    VR_APK_Check(VR_ARK_CountTaken() == 0 && VR_ARK_CardCount() == n, "and leaves the offer open");

    VR_APK_Check(VR_ARK_CardPick(1), "picking a card takes");
    VR_APK_Check(VR_ARK_Has(chosen), "picking a card takes its perk");
    VR_APK_Check(VR_ARK_CountTaken() == 1, "and exactly one perk: the choice is one of three");
    VR_APK_Check(ar_mgr.ark_taken == chosen, "the perks not chosen stay untaken");
    VR_APK_Check(VR_ARK_CardCount() == 0 && VR_ARK_CardSerial() == 0, "the cards go with it");
    VR_APK_Check(!VR_ARK_CardPick(0), "and a second pick of the same offer takes nothing");
    VR_APK_Check(VR_ARK_CountTaken() == 1, "one perk still");
    VR_ARK_CloseOffer();
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
    VR_ARK_CloseOffer();
    ar_mgr.ark_taken = 0;
    VR_ARK_Offer(5); // (the fifth boss cleared)
}

void() VR_APK2_Frame =
{
    if(ark_t_step == 1) { ark_t_step = 2; return; } // (a frame, as a real clear's offer has)
    if(ark_t_step == 2) { VR_APK2_OfferChecks(); ark_t_step = 3; return; }
    if(ark_t_step == 3) { VR_APK2_PickChecks(); VR_APK_End(); return; }
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

    // Named, described, and each card shows a model of its own.
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

    // Each card wears a colour of its own, and its text fits a card (the panel wraps it on about five short lines).
    float coloured = TRUE;
    for(i = 0; i < ARK_POOL_N; i++)
    {
        bit = VR_ARK_PoolBit(i);
        if(VR_ARK_Colour(bit) == '0 0 0') { coloured = FALSE; }
        if(strlen(VR_ARK_Name(bit)) > 14 || strlen(VR_ARK_What(bit)) > 64) { coloured = FALSE; }
        float k;
        for(k = i + 1; k < ARK_POOL_N; k++)
        {
            if(VR_ARK_Colour(bit) == VR_ARK_Colour(VR_ARK_PoolBit(k))) { coloured = FALSE; }
        }
    }
    VR_APK_Check(coloured, "every perk's card has a colour no other card has, and text that fits a card");

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

    VR_APK_Check(!VR_ARK_Kept(), "with no perk taken the arena's gun cap is as set");
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

    VR_ARK_WaveDone(3); // (an ordinary wave: no boss, so no cards either)
    VR_APK_Check(VR_AmmoUtil_GetAmmoCountByAmmoId(ark_t_p, AID_SHELLS) == 25, "Quartermaster: a wave's clear is what hands it out");
    VR_APK_Check(VR_ARK_CardCount() == 0, "and an ordinary wave clear still offers nothing");
}

void() VR_APK3_Spoils =
{
    ar_mgr.ark_taken = ARK_SPOIL;
    float gun0 = cvar("vr_arena_gun_max");
    cvar_set("vr_arena_gun_max", "24");
    VR_APK_Check(VR_ARK_Kept(), "Spoils: taken");
    VR_APK_Check(VR_ARENA_GunMost() == 48, "Spoils: taken, twice vr_arena_gun_max of the foes' guns lie about");
    ar_mgr.ark_taken = 0;
    VR_APK_Check(!VR_ARK_Kept(), "Spoils: not taken");
    VR_APK_Check(VR_ARENA_GunMost() == 24, "Spoils: not taken, vr_arena_gun_max of them");
    cvar_set("vr_arena_gun_max", ftos(gun0));
    VR_APK_Check(VR_ARK_Running(), "Spoils: and the perks only ever work inside a running arena run");
}

// A boss wave's clear drops that wave's medkits and armour AND makes the offer: the offer's cards are the engine's
// panel's, so it puts nothing in the field and leaves the drops every item point they had.
float() VR_APK_CountEnts =
{
    float n = 0;
    entity e;
    for(e = nextent(world); e; e = nextent(e)) { n = n + 1; }
    return n;
}

void() VR_APK3_NoField =
{
    ar_mgr.ark_taken = 0;
    VR_ARK_CloseOffer();
    float ents0 = VR_APK_CountEnts();
    float walk0 = ar_item_next;
    VR_ARK_Offer(5); // (wave 5 is a boss wave: this is what a real boss clear does)

    VR_APK_Check(VR_ARK_CardCount() == VR_ARK_Opt_Count(), "a boss wave's clear offers all the cards it says");
    VR_APK_Check(VR_APK_BadCard() == 0, "every card shows a real item's model and offers its own perk");
    VR_APK_Check(VR_APK_CountEnts() == ents0, "the offer puts nothing in the field: its cards are the panel's");
    VR_APK_Check(ar_item_next == walk0, "and the wave's drops keep the item points they had");

    VR_ARK_CloseOffer();
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
    VR_ARK_CloseOffer();
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
    if(ark_t_step == 7) { VR_APK3_NoField(); ark_t_step = 8; return; }

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
    VR_ARK_CloseOffer();
    ar_mgr.ark_taken = 0;
    cvar_set("vr_arena_max_alive", "2");   // (small waves: the drive to the first boss wave has to fit in a test)
    cvar_set("vr_arena_wave_rest", "1");
    cvar_set("vr_arena_countdown", "0"); // (no countdown: the perk tests time their own waves)
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
    VR_APK_Check(VR_ARK_CardCount() == VR_ARK_Opt_Count(), "the boss wave's clear offered every card it says");
    VR_APK_Check(VR_APK_BadCard() == 0, "every card shows a shipped item's model and offers its own perk");
    VR_APK_Check(ar_mgr.think == VR_ARK_HoldThink && VR_ARENA_Between(), "the arena waits between the waves while the cards are up");
    VR_APK_Check(VR_ARENA_CountAlive() == 0, "with nothing of the wave left in the field");

    float chosen = VR_ARK_CardBit(0);
    VR_APK_Check(VR_ARK_CardPick(0), "picking a card is the choice");
    VR_APK_Check(VR_ARK_Has(chosen), "the perk taken stands on the director");
    VR_APK_Check(ar_mgr.ark_taken == chosen, "and it is exactly the one the player picked");
    VR_APK_Check(VR_ARK_CountLeft() == ARK_POOL_N - 1, "the rest of the pool is still there to offer");
    VR_APK_Check(VR_ARK_CardCount() == 0, "and the cards are gone");
    VR_APK_Check(ar_mgr.think != VR_ARK_HoldThink && VR_ARENA_Between(), "the arena goes on with its rest");

    VR_ARK_Offer(2 * boss); // (the next boss wave's offer, asked for here rather than waited for)
    VR_APK_Check(VR_ARK_CardCount() == VR_ARK_Opt_Count(), "a later boss wave offers again");
    VR_APK_Check((VR_APK2_CardBits() & chosen) == 0, "and never a perk this run already took");

    VR_ARK_CloseOffer();
    VR_APK3_Restore();
    VR_APK_End();
}

void() VR_APK4_Frame =
{
    if(ark_t_step == 1)
    {
        VR_APK4_KillOne();
        if(VR_ARK_CardCount() > 0) { ark_t_step = 2; }
        return;
    }
    VR_APK4_OfferChecks();
}

// ---------------------------------------------------------------------------
// Test 5: the card panel end to end (vrarena; god; the headless mock headset, so the engine's panel shows the cards).
// The run is jumped to its first boss wave and its foes are brought down; when the clear's cards are up and the panel
// shows them, the arena must stand still for ARK_T5_WAIT s (no wave, no countdown, no foe, the rest's time kept); the
// mock laser then lights card ARK_T5_CARD (a screenshot of the panel), the mock trigger picks it, and the perk must be
// taken and the arena go on: the countdown the rest's remainder after the pick, then the next wave.
// ---------------------------------------------------------------------------

float ARK_T5_WAIT = 3; // s the cards stay up untouched: longer than the rest and the countdown the test sets (1 + 1)
float ARK_T5_CARD = 2; // the card the mock laser lights and the trigger picks (1 the first)
float ARK_T5_SETTLE = 4; // frames for the laser's light to reach the panel's image (and the screenshot to be taken)

float ark_t5_wave;  // the wave the cards came up after
float ark_t5_left;  // the rest the director still had then
float ark_t5_said;  // the countdown's lines said by then
float ark_t5_t0;    // the time the panel first showed them
float ark_t5_bad;   // frames the arena moved while the cards were up
float ark_t5_bit;   // the perk on the card picked
float ark_t5_pick;  // the time it was picked
float ark_t5_frames;

void() VR_APK5_Begin =
{
    if(!ar_mgr)
    {
        VR_APK_Check(FALSE, "the arena's director is not in this map (vrarena)");
        VR_APK_End();
        return;
    }
    if(VR_ARENA_FindPlayer() == world)
    {
        VR_APK_Check(FALSE, "no player is up to choose (god, and the map's own player)");
        VR_APK_End();
        return;
    }
    VR_ARK_CloseOffer();
    ar_mgr.ark_taken = 0;
    cvar_set("vr_arena_max_alive", "2");
    cvar_set("vr_arena_wave_rest", "1");
    cvar_set("vr_arena_countdown", "1");
    VR_ARENA_FastForward(cvar("vr_arena_boss_every")); // (the first boss wave next: its clear makes the offer)
    ark_t5_bad = 0;
    ark_t_until = time + 120;
    ark_t_step = 1;
    dprint("arena_cards: jumping to the first boss wave\n");
}

// The arena standing still: the same wave, no countdown begun or said, nothing up in the field, the director held.
float() VR_APK5_Still =
{
    if(ar_wave != ark_t5_wave) { return FALSE; }
    if(ar_count_at > 0 || ar_count_said != ark_t5_said) { return FALSE; }
    if(VR_ARENA_CountAlive() > 0) { return FALSE; }
    if(ar_mgr.think != VR_ARK_HoldThink) { return FALSE; }
    return ar_mgr.ark_resume_left == ark_t5_left;
}

void() VR_APK5_Frame =
{
    if(ark_t_step == 1) // the boss wave's foes brought down until its clear's cards are up
    {
        if(VR_ARK_CardCount() > 0) { ark_t_step = 2; return; }
        VR_APK4_KillOne();
        return;
    }
    if(ark_t_step == 2) // the panel showing them
    {
        if(ark_seen_at <= 0) { return; }
        ark_t5_wave = ar_wave;
        ark_t5_left = ar_mgr.ark_resume_left;
        ark_t5_said = ar_count_said;
        ark_t5_t0 = time;
        VR_APK_Check(VR_ARENA_IsBossWave(ar_wave), "the cards came up after a boss wave's clear");
        VR_APK_Check(VR_ARK_CardCount() == VR_ARK_Opt_Count(), "with every card it offers");
        VR_APK_Check(ark_t5_left > 0, "and the rest after the clear still to run");
        dprint(sprintf("arena_cards: %g cards shown after wave %g, %g s of rest held\n", VR_ARK_CardCount(), ar_wave,
                       ark_t5_left));
        ark_t_step = 3;
        return;
    }
    if(ark_t_step == 3) // ARK_T5_WAIT s untouched: nothing may move
    {
        if(!VR_APK5_Still()) { ark_t5_bad = ark_t5_bad + 1; }
        if(time < ark_t5_t0 + ARK_T5_WAIT) { return; }
        VR_APK_Check(ark_t5_bad == 0, sprintf("the arena stood still while the cards were up (%g frames moved)", ark_t5_bad));
        VR_APK_Check(ar_mgr.ark_resume_left == ark_t5_left, "the rest's time did not run down meanwhile");
        VR_APK_Check(VR_ARK_CountTaken() == 0, "and nothing was taken by itself");
        ark_t5_bit = VR_ARK_CardBit(ARK_T5_CARD - 1);
        localcmd(sprintf("vr_arena_cards_mock_laser %g\n", ARK_T5_CARD));
        ark_t5_frames = 0;
        ark_t_step = 4;
        return;
    }
    if(ark_t_step == 4) // the laser's light on the card: the screenshot
    {
        ark_t5_frames = ark_t5_frames + 1;
        if(ark_t5_frames < ARK_T5_SETTLE) { return; }
        localcmd("screenshot\n");
        ark_t5_frames = 0;
        ark_t_step = 5;
        return;
    }
    if(ark_t_step == 5) // the trigger pulled on it
    {
        ark_t5_frames = ark_t5_frames + 1;
        if(ark_t5_frames < ARK_T5_SETTLE) { return; }
        VR_APK_Check(VR_ARK_CardCount() > 0, "the cards were still up when the trigger was pulled");
        localcmd("vr_mock_button main trigger 1\n");
        ark_t_step = 6;
        return;
    }
    if(ark_t_step == 6) // the pick taken
    {
        if(VR_ARK_CardCount() > 0) { return; }
        ark_t5_pick = time;
        localcmd("vr_mock_button main trigger 0\nvr_arena_cards_mock_laser off\n");
        VR_APK_Check(VR_ARK_Has(ark_t5_bit), sprintf("the trigger on card %g took its perk (%s)", ARK_T5_CARD,
                                                     VR_ARK_Name(ark_t5_bit)));
        VR_APK_Check(ar_mgr.ark_taken == ark_t5_bit, "and that perk alone");
        VR_APK_Check(ar_mgr.think != VR_ARK_HoldThink, "the arena is let go");
        VR_APK_Check(fabs(ar_mgr.nextthink - time - ark_t5_left) < 0.05, "with the rest it had left when the cards came up");
        ark_t_step = 7;
        return;
    }
    if(ark_t_step == 7) // the countdown, the rest's remainder after the pick
    {
        if(ar_count_at <= 0) { return; }
        VR_APK_Check(fabs(ar_count_at - ark_t5_pick - ark_t5_left) < AR_TICK + 0.05,
                     sprintf("the countdown began %g s after the pick (the rest held: %g s)", ar_count_at - ark_t5_pick, ark_t5_left));
        ark_t_step = 8;
        return;
    }
    if(ar_wave <= ark_t5_wave) { return; } // the next wave
    VR_APK_Check(ar_wave == ark_t5_wave + 1, "and the next wave came after it");
    VR_APK_End();
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
        if(c == 5) { ark_t_prefix = "arena_cards"; } // (the panel's: `arena_cards: ...`)
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
        if(c == 5)
        {
            VR_APK5_Begin();
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
    if(ark_t_case == 5) { VR_APK5_Frame(); return; }
}
#endif
