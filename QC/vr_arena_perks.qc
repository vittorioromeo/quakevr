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
.float ark_until;      // (a pedestal) how long its label keeps being said

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
// The offer: three pedestals on the places a wave clear's drops already use
// ---------------------------------------------------------------------------

string ARK_PED_NAME = "vr_arena_perk"; // the pedestals' classname (what VR_ARK_ClearPeds puts away)

// The field's fields are declared above: ark_perk (a pedestal's perk) and, on the director, ark_offer_wave.

float ark_offer_bit[8]; // the perks the standing offer shows, ark_offer_n of them
float ark_offer_n;

// The offer's turning point in the pool: the tenth boss shows a different three from the fifth's, and a run whose
// boss waves land the same way offers the same ones (the tests read this).
float(float xWave) VR_ARK_OfferTurn = { return VR_ARENA_Mod(xWave, ARK_POOL_N); }

// Which perks this offer shows: from its turning point, skipping what this run already took, so no perk is
// offered twice in a run.
void(float xWave) VR_ARK_Choose =
{
    float want = VR_ARK_Opt_Count();
    float left = VR_ARK_CountLeft();
    float turn = VR_ARK_OfferTurn(xWave);
    float n = 0;
    float i;

    if(want > left) { want = left; }
    for(i = 0; (i < ARK_POOL_N) && (n < want); i++)
    {
        float bit = VR_ARK_PoolBit(VR_ARENA_Mod(turn + i, ARK_POOL_N));
        if(!VR_ARK_CanOffer(bit)) { continue; }
        ark_offer_bit[n] = bit;
        n = n + 1;
    }
    ark_offer_n = n;
}

// The pedestals standing in the field (a new offer replaces them; they block nothing: a trigger's touch box).
float() VR_ARK_CountPeds =
{
    float n = 0;
    entity p;
    for(p = find(world, classname, ARK_PED_NAME); p; p = find(p, classname, ARK_PED_NAME)) { n = n + 1; }
    return n;
}

float() VR_ARK_ClearPeds =
{
    float n = 0;
    entity p;
    entity nx;
    for(p = find(world, classname, ARK_PED_NAME); p; p = nx)
    {
        nx = find(p, classname, ARK_PED_NAME);
        remove(p);
        n = n + 1;
    }
    return n;
}

// The line a pedestal stands under, said again every ARK_PED_NOTE s while the offer stands (ARK_PED_LIFE s).
void() VR_ARK_PedLabel =
{
    VR_ARENA_PrintAll(sprintf("%s\n%s\n", VR_ARK_Name(self.ark_perk), VR_ARK_What(self.ark_perk)));
    if(time + ARK_PED_NOTE <= self.ark_until) { self.nextthink = time + ARK_PED_NOTE; }
}

// One pedestal: its perk's model, on `xAt`, named `xOrder` of the way in so the three lines do not speak at once.
entity(float xPerk, vector xAt, float xOrder) VR_ARK_SpawnPed =
{
    entity p = spawn();

    p.classname = ARK_PED_NAME;
    p.ark_perk  = xPerk;
    p.ark_until = time + ARK_PED_LIFE;
    setmodel(p, VR_ARK_Model(xPerk));
    if(!p.modelindex)
    {
        remove(p);
        dprint(sprintf("arena_perks: no model for %s\n", VR_ARK_Name(xPerk)));
        return world;
    }
    setorigin(p, xAt + '0 0 8');
    p.movetype = MOVETYPE_NONE;
    p.solid    = SOLID_TRIGGER; // (walk over it: it stands in nobody's way, and a monster walking over means nothing)
    p.takedamage = DAMAGE_NO;
    p.size     = '32 32 48';
    p.touch    = VR_ARK_PedTouch;
    p.think    = VR_ARK_PedLabel;
    p.nextthink = time + 0.8 * (xOrder + 1);

    if(cvar("developer")) dprint(sprintf("arena_perks: pedestal for %s at %s\n", VR_ARK_Name(xPerk), vtos(xAt)));
    return p;
}

// Walking over a pedestal is the choice: that perk is this run's, and the other two go away.
void() VR_ARK_PedTouch =
{
    if(!ar_mgr) { return; }
    if(!(other.flags & FL_CLIENT)) { return; }

    float bit = self.ark_perk;
    string name = VR_ARK_Name(bit);
    string what = VR_ARK_What(bit);

    VR_ARK_Take(bit);
    VR_ARK_ClearPeds();

    centerprint(other, sprintf("%s\n%s\n", name, what));
    VR_ARENA_PrintAll(sprintf("ARENA - %s taken\n", name));
    stuffcmd(other, "bf\n"); // (the same little haptic the shipped pickups give)
    dprint(sprintf("arena_perks: %s chosen (%g perks, %g pedestals left)\n", name, VR_ARK_CountTaken(),
                   VR_ARK_CountPeds()));
}

// A boss wave cleared: the offer. The old pedestals go; the pool's last rung having been taken, it offers nothing.
void(float xWave) VR_ARK_Offer =
{
    float made = 0;
    float i;

    VR_ARK_ClearPeds();
    ark_offer_n = 0;
    ar_mgr.ark_offer_wave = xWave;

    if(!VR_ARK_Opt_On())
    {
        dprint("arena_perks: a boss wave offers nothing (vr_arena_perks 0)\n");
        return;
    }

    VR_ARK_Choose(xWave);
    if(ark_offer_n == 0)
    {
        VR_ARENA_PrintAll("ARENA - every perk is taken already\n");
        dprint("arena_perks: the pool is empty, nothing to offer\n");
        return;
    }

    for(i = 0; i < ark_offer_n; i++)
    {
        if(VR_ARK_SpawnPed(ark_offer_bit[i], VR_ARENA_DropPlace(i), i)) { made = made + 1; }
    }

    VR_ARENA_PrintAll(sprintf("ARENA - %g perks to choose: walk over one\n", made));
    dprint(sprintf("arena_perks: wave %g offers %g pedestals (%g standing, %g perks left)\n", xWave, made,
                   VR_ARK_CountPeds(), VR_ARK_CountLeft()));
}

// ---------------------------------------------------------------------------
// The hooks, all inert until the perk is taken (filled in as each perk is built)
// ---------------------------------------------------------------------------

// vr_arena.qc: a wave clear (a boss wave's clear makes the offer), and every kill it counted.
void(float xWave) VR_ARK_WaveDone =
{
    if(!VR_ARK_Running()) { return; }
    if(VR_ARENA_IsBossWave(xWave)) { VR_ARK_Offer(xWave); }
}

void(entity e) VR_ARK_Kill = { };

// vr_decap.qc: a foe beheaded by `by`.
void(entity e, entity by, float xHow) VR_ARK_Decap = { };

// items.qc: the armour `p` just picked up, already carrying `xValue`.
void(entity p, float xValue) VR_ARK_ArmorBonus = { };

// vr_enemyguns.qc: the cap on dropped enemy weapons leaves them alone with Spoils taken.
float() VR_ARK_Kept = { return FALSE; }
