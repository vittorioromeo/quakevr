// vr_arena_perks.qc -- Survival Arena: the perk a boss wave's clear offers (the director's own file is
// vr_arena.qc).
//
// A boss wave cleared offers the player his choice of three perks off the pool below. Three pedestals come up
// on the map's info_arena_item points, each wearing a shipped item's model and each named by a centerprint line
// saying what it does; the player walks over the one he wants, the other two go away, and what he took is kept
// on the arena's director (ar_mgr.ark_taken). A perk does nothing until it is taken: every hook below starts by
// asking VR_ARK_Has. The same perk is never offered twice in a run; when the pool is empty a boss wave offers
// nothing.
//
// world.qc never calls this file directly. vr_arena.qc asks for the offer on a boss wave's clear
// (VR_ARK_WaveDone) and reports every kill (VR_ARK_Kill); vr_decap.qc reports a beheading (VR_ARK_Decap),
// items.qc an armour picked up (VR_ARK_ArmorBonus), vr_enemyguns.qc the cap on dropped enemy weapons
// (VR_ARK_Kept). The tests are vr_arena_perks_test.qc (vr_arena_perks_test N).

// ---------------------------------------------------------------------------
// The perks: each one a bit of ar_mgr.ark_taken
// ---------------------------------------------------------------------------

float ARK_QUAD  = 1;   // Quad Strike: a kill with your hands gives 2 seconds of quad damage
float ARK_SNAP  = 2;   // Neck Snapper: beheading a foe restores 20 health
float ARK_IRON  = 4;   // Iron Skin: every armour you pick up is 25 heavier
float ARK_WIND  = 8;   // Second Wind: your health reaches 15 past its normal top
float ARK_FEED  = 16;  // Feeder: a kill heals 5, at most once a second
float ARK_QUART = 32;  // Quartermaster: a wave clear tops up the ammo of the guns you hold
float ARK_SPOIL = 64;  // Spoils: a fallen foe's gun stays where it fell

float ARK_POOL_N = 7;  // the pool: every one of these is offered before any is offered twice

// What each perk gives (the numbers the tests read).
float ARK_QUAD_SECS  = 2;
float ARK_SNAP_HP    = 20;
float ARK_IRON_ARM   = 25;
float ARK_WIND_HP    = 15;
float ARK_FEED_HP    = 5;
float ARK_FEED_GAP   = 1;   // s between two Feeder heals
float ARK_QUART_AMMO = 15;  // of each kind a Quartermaster wave clear gives

// What a pedestal is: its touch box, and how long its label stays on the screen.
float ARK_PED_LIFE   = 240; // s a standing offer is re-named for the player (0: never)
float ARK_PED_NOTE   = 20;  // s between those reminders

// The entity fields this file owns: the director's taken perks and the beats of the ones that repeat,
// and a pedestal's perk.
.float ark_taken;      // (ar_mgr) the perks this run has taken, as bits
.float ark_feed_at;    // (ar_mgr) the time Feeder last healed
.float ark_offer_wave; // (ar_mgr) the wave the standing offer was made on
.float ark_perk;       // (a pedestal) which perk it offers

// ---------------------------------------------------------------------------
// The pool, and what it is called
// ---------------------------------------------------------------------------

// The pool's order. An offer walks it from a turning point, so the choice at the fifth boss is not the choice
// at the first one, and a run whose boss waves land the same way offers the same perks.
float(float xIndex) VR_ARK_PoolBit =
{
    if(xIndex == 0) { return ARK_QUAD;  }
    if(xIndex == 1) { return ARK_SNAP;  }
    if(xIndex == 2) { return ARK_IRON;  }
    if(xIndex == 3) { return ARK_WIND;  }
    if(xIndex == 4) { return ARK_FEED;  }
    if(xIndex == 5) { return ARK_QUART; }
    if(xIndex == 6) { return ARK_SPOIL; }
    return 0;
}

string(float xBit) VR_ARK_Name =
{
    if(xBit == ARK_QUAD)  { return "Quad Strike"; }
    if(xBit == ARK_SNAP)  { return "Neck Snapper"; }
    if(xBit == ARK_IRON)  { return "Iron Skin"; }
    if(xBit == ARK_WIND)  { return "Second Wind"; }
    if(xBit == ARK_FEED)  { return "Feeder"; }
    if(xBit == ARK_QUART) { return "Quartermaster"; }
    if(xBit == ARK_SPOIL) { return "Spoils"; }
    return "";
}

string(float xBit) VR_ARK_What =
{
    if(xBit == ARK_QUAD)  { return "A kill with your hands gives 2 s of quad damage"; }
    if(xBit == ARK_SNAP)  { return "Beheading a foe restores 20 health"; }
    if(xBit == ARK_IRON)  { return "Every armour you pick up is 25 heavier"; }
    if(xBit == ARK_WIND)  { return "Your health reaches 15 past its normal top"; }
    if(xBit == ARK_FEED)  { return "A kill heals 5, at most once a second"; }
    if(xBit == ARK_QUART) { return "A wave clear tops up the ammo of the guns you hold"; }
    if(xBit == ARK_SPOIL) { return "A fallen foe's gun stays where it fell"; }
    return "";
}

// The model a pedestal wears: a shipped item's model, one per perk, no two the same.
string(float xBit) VR_ARK_Model =
{
    if(xBit == ARK_QUAD)  { return "progs/quaddama.mdl"; }
    if(xBit == ARK_SNAP)  { return "progs/gib1.mdl"; }
    if(xBit == ARK_IRON)  { return "progs/vrpauldron.mdl"; }
    if(xBit == ARK_WIND)  { return "progs/mervup.mdl"; }
    if(xBit == ARK_FEED)  { return "progs/gib2.mdl"; }
    if(xBit == ARK_QUART) { return "progs/vrpouch_ammo.mdl"; }
    if(xBit == ARK_SPOIL) { return "progs/v_shot.mdl"; }
    return "";
}

// The line a pedestal is named by: "<perk name> - <what it does>".
string(float xBit) VR_ARK_Label =
{
    return sprintf("%s - %s", VR_ARK_Name(xBit), VR_ARK_What(xBit));
}

// ---------------------------------------------------------------------------
// The knobs (vr_cvars.inc) and the run's state
// ---------------------------------------------------------------------------

float() VR_ARK_Opt_On    { return cvar("vr_arena_perks") != 0; }
float() VR_ARK_Opt_Count { return VR_ARENA_Clamp(cvar("vr_arena_perk_count"), 1, ARK_POOL_N); }

// The perks this file can hand out at all: an arena run, and a director to keep them on.
float() VR_ARK_Running =
{
    if(!ar_mgr) { return FALSE; }
    return VR_ARENA_On();
}

float(float xBit) VR_ARK_Has =
{
    if(!VR_ARK_Running()) { return FALSE; }
    return (ar_mgr.ark_taken & xBit) != 0;
}

// The perks this run holds, and the ones its offers may still use.
float() VR_ARK_CountTaken =
{
    float n = 0;
    float i;
    if(!VR_ARK_Running()) { return 0; }
    for(i = 0; i < ARK_POOL_N; i++)
    {
        if((ar_mgr.ark_taken & VR_ARK_PoolBit(i)) != 0) { n = n + 1; }
    }
    return n;
}

float() VR_ARK_CountLeft = { return ARK_POOL_N - VR_ARK_CountTaken(); }

// One perk, taken once: taking it again gives it back (a run never holds a perk twice).
float(float xBit) VR_ARK_Take =
{
    if(!VR_ARK_Running()) { return FALSE; }
    if(VR_ARK_Has(xBit)) { return FALSE; }
    ar_mgr.ark_taken = ar_mgr.ark_taken | xBit;
    if(cvar("developer")) dprint(sprintf("arena_perks: %s taken (%g of %g)\n", VR_ARK_Name(xBit),
                                         VR_ARK_CountTaken(), ARK_POOL_N));
    return TRUE;
}

float(float xBit) VR_ARK_Untaken =
{
    if(!VR_ARK_Running()) { return TRUE; }
    return (ar_mgr.ark_taken & xBit) == 0;
}

// A perk the pool may still offer.
float(float xBit) VR_ARK_CanOffer = { return (xBit != 0) && VR_ARK_Untaken(xBit); }

// The player a perk belongs to: the one the arena is running for (its own FindPlayer; the world when none is up).
entity() VR_ARK_Player = { return VR_ARENA_FindPlayer(); }

// ---------------------------------------------------------------------------
// The hooks, all inert until the perk is taken (filled in as each perk is built)
// ---------------------------------------------------------------------------

// vr_arena.qc: a wave clear, and every kill it counted.
void(float xWave) VR_ARK_WaveDone = { };
void(entity e) VR_ARK_Kill = { };

// vr_decap.qc: a foe beheaded by `by`.
void(entity e, entity by, float xHow) VR_ARK_Decap = { };

// items.qc: the armour `p` just picked up, already carrying `xValue`.
void(entity p, float xValue) VR_ARK_ArmorBonus = { };

// vr_enemyguns.qc: the cap on dropped enemy weapons leaves them alone with Spoils taken.
float() VR_ARK_Kept = { return FALSE; }
