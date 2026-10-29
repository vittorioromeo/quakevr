// vr_arena_perks.qc -- Survival Arena: the perk a boss wave's clear offers (the director's own file is
// vr_arena.qc).
//
// A boss wave cleared offers the player his choice of three perks off the pool below, as cards: the engine's panel
// (Quake/vr/vr_arenacards.cpp) floats them in front of him -- each its perk's name, colour, item picture and what it
// does -- and he points at one with either hand's laser and pulls the trigger (or presses its number key). While the
// cards are up the arena waits: its director is held between the two waves (VR_ARK_HoldThink: the rest after the
// clear, so the countdown and the next wave, keeps what it had left), and an offer only ever comes after a wave's
// last foe is down, so nothing is in the field meanwhile. What he took is kept on the arena's director
// (ar_mgr.ark_taken). A perk does nothing until it is taken: every hook below starts by asking VR_ARK_Has. The same
// perk is never offered twice in a run; when the pool is empty a boss wave offers nothing.
//
// world.qc never calls this file directly. vr_arena.qc asks for the offer on a boss wave's clear
// (VR_ARK_WaveDone) and reports its frame (VR_ARK_Frame); combat.qc's Killed reports a foe's fall with its killer
// at hand (VR_ARK_Kill); vr_decap.qc reports a beheading (VR_ARK_Decap), items.qc an armour picked up
// (VR_ARK_ArmorBonus), vr_arena.qc the cap on dropped guns (VR_ARK_Kept: Spoils doubles it). The engine's panel
// reads the cards and answers through the VR_ARK_Card* functions below. The tests are vr_arena_perks_test.qc
// (vr_arena_perks_test N; 5 the panel itself, picked by the mock laser and trigger).
//
// No perk promises moving faster: nothing already in the engine lets QC speed a player up without overwriting his
// own sv_maxspeed (vr_fatigue.qc's .vr_heavy_move only slows), so that one is left out rather than built fragile.

// ---------------------------------------------------------------------------
// The perks: each one a bit of ar_mgr.ark_taken
// ---------------------------------------------------------------------------

float ARK_QUAD  = 1;   // Quad Strike: a kill with your hands gives 2 seconds of quad damage
float ARK_SNAP  = 2;   // Neck Snapper: beheading a foe restores 20 health
float ARK_IRON  = 4;   // Iron Skin: every armour you pick up is 25 heavier
float ARK_WIND  = 8;   // Second Wind: your health reaches 15 past its normal top
float ARK_FEED  = 16;  // Feeder: a kill heals 5, at most once a second
float ARK_QUART = 32;  // Quartermaster: a wave clear tops up the ammo of the guns you hold
float ARK_SPOIL = 64;  // Spoils: twice as many of the guns the foes dropped lie about (vr_arena_gun_max)

float ARK_OPEN  = 128; // Can Opener: your hits on a foe's armour plates wear them down twice as fast
float ARK_POOL_N = 8;  // the pool: every one of these is offered before any is offered twice

// What each perk gives (the numbers the tests read).
float ARK_QUAD_SECS  = 2;
float ARK_SNAP_HP    = 20;
float ARK_IRON_ARM   = 25;
float ARK_WIND_HP    = 15;
float ARK_FEED_HP    = 5;
float ARK_FEED_GAP   = 1;   // s between two Feeder heals
float ARK_QUART_AMMO = 15;  // of each kind a Quartermaster wave clear gives
float ARK_OPEN_COST  = 2;   // Can Opener: a plate's pool pays this per point it saves from the player's hit

// An offer whose cards no panel has said it shows for this long is withdrawn (nothing taken), so a game without the
// engine's panel (no headset) never waits for a choice it cannot make. With a headset this never fires: the panel says
// it sees the cards at the start of every host frame (VR_BeginFrame, menu open or not), before that frame's server
// ticks, and one sighting is enough for good; a hitch runs at most host_fixedtick_max ticks (under 1 s) between two
// sightings, a closed-menu pause runs none, and a menu's live preview still polls.
float ARK_UNSEEN = 3;

// The entity fields this file owns: the director's taken perks, the beats of the ones that repeat, and what the
// director was doing when the cards held it.
.float ark_taken;        // (ar_mgr) the perks this run has taken, as bits
.float ark_feed_at;      // (ar_mgr) the time Feeder last healed
.float ark_offer_wave;   // (ar_mgr) the wave the last offer was made on
.void() ark_resume;      // (ar_mgr) its think while the cards hold it
.float ark_resume_left;  // (ar_mgr) and the seconds that think still had to wait

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
    if(xIndex == 7) { return ARK_OPEN;  }
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
    if(xBit == ARK_OPEN)  { return "Can Opener"; }
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
    if(xBit == ARK_SPOIL) { return "Twice as many fallen foes' guns stay lying about"; }
    if(xBit == ARK_OPEN)  { return "Your hits wear a foe's armour plates down twice as fast"; }
    return "";
}

// The picture on a perk's card (the panel draws the model's thumbnail): a shipped item's model, one per perk, no two
// the same.
string(float xBit) VR_ARK_Model =
{
    if(xBit == ARK_QUAD)  { return "progs/quaddama.mdl"; }
    if(xBit == ARK_SNAP)  { return "progs/gib1.mdl"; }
    if(xBit == ARK_IRON)  { return "progs/vrpauldron.mdl"; }
    if(xBit == ARK_WIND)  { return "progs/mervup.mdl"; }
    if(xBit == ARK_FEED)  { return "progs/gib2.mdl"; }
    if(xBit == ARK_QUART) { return "progs/vrpouch_ammo.mdl"; }
    if(xBit == ARK_SPOIL) { return "progs/v_shot.mdl"; }
    if(xBit == ARK_OPEN)  { return "progs/v_crowbar.mdl"; } // (world.qc precaches it always: VR_Crowbar_Precache)
    return "";
}

// A card's colour (its title band and edge): what the perk is about -- red the fight, green health, steel armour,
// gold supplies.
vector(float xBit) VR_ARK_Colour =
{
    if(xBit == ARK_QUAD)  { return '0.45 0.55 1'; }    // (the quad's own blue)
    if(xBit == ARK_SNAP)  { return '0.85 0.25 0.2'; }
    if(xBit == ARK_IRON)  { return '0.6 0.65 0.7'; }
    if(xBit == ARK_WIND)  { return '0.3 0.8 0.4'; }
    if(xBit == ARK_FEED)  { return '0.65 0.85 0.25'; }
    if(xBit == ARK_QUART) { return '0.9 0.7 0.25'; }
    if(xBit == ARK_SPOIL) { return '0.8 0.5 0.2'; }
    if(xBit == ARK_OPEN)  { return '0.75 0.35 0.75'; }
    return '0.5 0.5 0.5';
}

// ---------------------------------------------------------------------------
// The knobs (vr_cvars.inc) and the run's state
// ---------------------------------------------------------------------------

float() VR_ARK_Opt_On    { return cvar("vr_arena_perks") != 0; }

// How many cards a boss clear offers: vr_arena_perk_count (the Normal value) times the difficulty's factor --
// a Hard or Nightmare clear offers a fourth, an Easy one two -- still clamped 1 .. the pool's size (8), and
// VR_ARK_Choose still skips what this run already took, so nothing is ever offered twice.
float() VR_ARK_Opt_Count =
{
    return clamp(VR_ARENA_SkillWhole(cvar("vr_arena_perk_count") * VR_ARENA_Skill(0.75, 1.25, 1.5)), 1, ARK_POOL_N);
}

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
    if(cvar_hget(cvarh_developer)) dprint(sprintf("arena_perks: %s taken (%g of %g)\n", VR_ARK_Name(xBit),
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

// ---------------------------------------------------------------------------
// The offer: cards on the engine's panel, the arena held until one is picked
// ---------------------------------------------------------------------------

float ark_offer_bit[8]; // the perks the last offer showed, ark_offer_n of them
float ark_offer_n;

float ark_open;         // 1: an offer's cards are up and unanswered (the director held meanwhile)
float ark_open_serial;  // each offer its own number (the panel takes a new one for a new offer: placed in front again)
float ark_open_at;      // the time the cards went up
float ark_seen_at;      // the last time the engine's panel said it shows them (0: not yet)
float ark_hold_at;      // the time the director was held (0: it is not)

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

// The director let go: what it was doing goes on with the time it still had (the rest's own start moved on by the
// hold, so the rest it reports is the rest it had).
void() VR_ARK_Resume =
{
    if(!ar_mgr) { return; }
    if(ar_mgr.think != VR_ARK_HoldThink) { return; }
    float held = time - ark_hold_at;
    if(ar_rest_at > 0) { ar_rest_at = ar_rest_at + held; }
    ar_mgr.think = ar_mgr.ark_resume;
    ar_mgr.nextthink = time + ar_mgr.ark_resume_left;
    ark_hold_at = 0;
    dprint(sprintf("arena_perks: the arena goes on after %g s held (%g s of its rest left)\n", held,
                   ar_mgr.ark_resume_left));
}

// The cards taken down (picked, or withdrawn) and the director let go.
void() VR_ARK_CloseOffer =
{
    ark_open = 0;
    VR_ARK_Resume();
}

// The director held while the cards are up: only between two waves (a boss clear's rest), so no countdown, wave or
// spawn comes meanwhile; mid-wave (a test's own offer) it is left alone.
void() VR_ARK_Hold =
{
    if(!ar_mgr) { return; }
    if(ar_mgr.think == VR_ARK_HoldThink) { return; }
    if(!VR_ARENA_Between()) { return; }
    ar_mgr.ark_resume = ar_mgr.think;
    ar_mgr.ark_resume_left = max(0, ar_mgr.nextthink - time);
    ar_mgr.think = VR_ARK_HoldThink;
    ar_mgr.nextthink = time + AR_TICK;
    ark_hold_at = time;
    dprint(sprintf("arena_perks: the arena waits for the choice (%g s of its rest left)\n", ar_mgr.ark_resume_left));
}

// The held director's beat: it waits while the cards are up. Cards no panel has shown (no headset: nothing could pick
// them) are withdrawn after ARK_UNSEEN s, so such a game goes on without a perk rather than wait for ever.
void() VR_ARK_HoldThink =
{
    if(ark_open && (ark_seen_at <= 0) && (time - ark_open_at > ARK_UNSEEN))
    {
        VR_ARENA_PrintAll("ARENA - no perk panel to choose on: the offer is withdrawn\n");
        dprint("arena_perks: no panel showed the cards, the offer is withdrawn\n");
        ark_open = 0;
    }
    if(!ark_open)
    {
        VR_ARK_Resume();
        return;
    }
    self.nextthink = time + AR_TICK;
}

// A boss wave cleared: the offer. One still open is withdrawn first; the pool's last rung having been taken, it offers
// nothing.
void(float xWave) VR_ARK_Offer =
{
    VR_ARK_CloseOffer();
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

    ark_open = 1;
    ark_open_serial = ark_open_serial + 1;
    ark_open_at = time;
    ark_seen_at = 0;
    VR_ARK_Hold();
    dprint(sprintf("arena_perks: wave %g offers %g cards (%g perks left)\n", xWave, ark_offer_n, VR_ARK_CountLeft()));
}

// The engine's panel (vr_arenacards.cpp) asks, once a frame, through these: the open offer's number (0: none), its
// cards and their perks, its title, and says it shows them (VR_ARK_CardsSeen: the perks held, for its footer).
float() VR_ARK_CardSerial =
{
    if(!VR_ARK_Running() || !ark_open) { return 0; }
    return ark_open_serial;
}

float() VR_ARK_CardCount = { return VR_ARK_CardSerial() ? ark_offer_n : 0; }

float(float i) VR_ARK_CardBit =
{
    if(i < 0 || i >= ark_offer_n) { return 0; }
    return ark_offer_bit[i];
}

string() VR_ARK_CardTitle = { return sprintf("BOSS WAVE %g CLEARED", ar_mgr.ark_offer_wave); }

float() VR_ARK_PoolSize = { return ARK_POOL_N; }

float() VR_ARK_CardsSeen =
{
    if(ark_open && ark_seen_at <= 0) { dprint(sprintf("arena_perks: the panel shows the %g cards\n", ark_offer_n)); }
    ark_seen_at = time;
    return VR_ARK_CountTaken();
}

// Card `i` (0 first) picked: its perk is this run's, the cards go and the arena goes on. 1 when it took.
float(float i) VR_ARK_CardPick =
{
    if(!ark_open || !ar_mgr) { return 0; }
    float bit = VR_ARK_CardBit(i);
    if(bit == 0) { return 0; }

    string name = VR_ARK_Name(bit);
    VR_ARK_Take(bit);
    VR_ARK_CloseOffer();

    entity p = VR_PlayerAlive();
    if(p != world)
    {
        centerprint(p, sprintf("%s\n%s\n", name, VR_ARK_What(bit)));
        stuffcmd(p, "bf\n"); // (the same little haptic the shipped pickups give)
    }
    VR_ARENA_PrintAll(sprintf("ARENA - %s taken\n", name));
    dprint(sprintf("arena_perks: %s chosen (card %g of %g, %g perks)\n", name, i + 1, ark_offer_n, VR_ARK_CountTaken()));
    return 1;
}

// The engine's vr_arena_cards_offer (Debug > Automation & Self-Tests): the offer the next boss wave's clear would make,
// now -- only between two waves, where a real one comes.
void() VR_ARK_DebugOffer =
{
    if(!VR_ARK_Running()) { dprint("arena_perks: no Survival Arena run here (map vrarena)\n"); return; }
    if(!VR_ARENA_Between())
    {
        VR_ARENA_PrintAll("ARENA - perk cards come only between two waves\n");
        return;
    }
    float every = max(1, VR_ARENA_BossEvery());
    VR_ARK_Offer(max(every, ceil(max(ar_wave, 1) / every) * every));
}

// ---------------------------------------------------------------------------
// The perks at work: every hook says nothing unless its perk was taken, and nothing at all off the arena
// ---------------------------------------------------------------------------

// Heal `p` by xUp, never past what ordinary healing tops at (its .max_health: Second Wind included). A megahealth
// top is left as it is.
void(entity p, float xUp) VR_ARK_Heal =
{
    if(!(p.flags & FL_CLIENT)) { return; }
    if(p.health <= 0) { return; }
    if(p.max_health <= 0) { p.max_health = VR_MaxHealth(p); }
    if(p.health >= p.max_health) { return; }
    p.health = min(p.health + xUp, p.max_health);
}

// Quad Strike: the shipped powerup's own fields (items.qc's powerup_touch), only much shorter. A quad already running
// longer (a picked-up Quad Damage) is left as it is: the perk only ever adds quad time, never cuts it short.
void(entity p, float xSecs) VR_ARK_GiveQuad =
{
    if(p.super_damage_finished >= time + xSecs) { return; }
    p.super_time = 1;
    p.super_damage_finished = time + xSecs;
    VR_ItemUtil_EntAddItem(p, IID_QUAD);
    VR_QuadSound(p, "the arena's Quad Strike");
}

// Second Wind: what ordinary healing tops at, 15 past normal. Said from the director's frame: the arena strips its
// players once, client.qc puts .max_health back on a level change, and the perk must simply survive both. Only ever
// raises; a top already higher than that (a megahealth) is left alone.
void() VR_ARK_SecondWind =
{
    entity p;
    for(p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(!(p.flags & FL_CLIENT)) { continue; }
        float top = VR_MaxHealth(p) + ARK_WIND_HP;
        if(p.max_health < top)
        {
            float was = p.max_health;
            p.max_health = top;
            if((was > 0) && (p.health > 0) && (p.health >= was)) { p.health = top; } // (full at the old top: 15 more)
            dprint(sprintf("arena_perks: Second Wind: %s's health tops at %g\n", p.classname, top));
        }
    }
}

// Quartermaster: how much of one ammunition a wave clear gives, up to the ordinary cap (vr_ammoutil.qc), and how
// much it actually could.
float(entity p, float xAid) VR_ARK_TopUp =
{
    float have = VR_AmmoUtil_GetAmmoCountByAmmoId(p, xAid);
    float top = VR_MaxAmmo(p, xAid);
    if(have >= top) { return 0; }
    float gave = min(ARK_QUART_AMMO, top - have);
    VR_AmmoUtil_AddAmmoCountByAmmoId(p, xAid, gave);
    return gave;
}

// The guns this player holds, by their ammunition: nothing is handed out, no item is made — its own ammo is topped.
float(entity p) VR_ARK_Quartermaster =
{
    float gave = 0;
    if(((p.items & IT_SHOTGUN) != 0) || ((p.items & IT_SUPER_SHOTGUN) != 0)) { gave = gave + VR_ARK_TopUp(p, AID_SHELLS); }
    if(((p.items & IT_NAILGUN) != 0) || ((p.items & IT_SUPER_NAILGUN) != 0)) { gave = gave + VR_ARK_TopUp(p, AID_NAILS); }
    if(((p.items & IT_GRENADE_LAUNCHER) != 0) || ((p.items & IT_ROCKET_LAUNCHER) != 0)) { gave = gave + VR_ARK_TopUp(p, AID_ROCKETS); }
    if((p.items & IT_LIGHTNING) != 0) { gave = gave + VR_ARK_TopUp(p, AID_CELLS); }
    return gave;
}

// ---------------------------------------------------------------------------
// The hooks
// ---------------------------------------------------------------------------

// vr_arena.qc's frame (Second Wind), and a wave clear (Quartermaster; a boss wave's clear makes the offer).
void() VR_ARK_Frame =
{
    if(!VR_ARK_Running()) { return; }
    if((ar_mgr.ark_taken & ARK_WIND) != 0) { VR_ARK_SecondWind(); }
}

void(float xWave) VR_ARK_WaveDone =
{
    if(!VR_ARK_Running()) { return; }

    if((ar_mgr.ark_taken & ARK_QUART) != 0)
    {
        float gave = 0;
        float living = 0;
        entity p;
        for(p = VR_Player(); p; p = VR_NextPlayer(p))
        {
            if(!(p.flags & FL_CLIENT)) { continue; }
            if(p.health <= 0) { continue; }
            gave = gave + VR_ARK_Quartermaster(p);
            living = living + 1;
        }
        VR_ARENA_PrintAll(sprintf("ARENA - Quartermaster: %g rounds back (%g armed)\n", gave, living));
        dprint(sprintf("arena_perks: Quartermaster: wave %g gave %g rounds to %g players\n", xWave, gave, living));
    }

    if(VR_ARENA_IsBossWave(xWave)) { VR_ARK_Offer(xWave); }
}

// combat.qc's Killed: `e` fell, by `by` (Quad Strike, Feeder). The killer comes through as an argument: a foe
// killed by another foe's sword is nobody's perk.
void(entity e, entity by) VR_ARK_Kill =
{
    if(!VR_ARK_Running()) { return; }
    if(!by || !(by.flags & FL_CLIENT)) { return; }

    float bits = ar_mgr.ark_taken;
    if(bits == 0) { return; }

    if(((bits & ARK_QUAD) != 0) && (vr_hitkind == QVR_HITKIND_MELEE))
    {
        VR_ARK_GiveQuad(by, ARK_QUAD_SECS);
        dprint(sprintf("arena_perks: Quad Strike: %g s of quad for a hand kill of %s\n", ARK_QUAD_SECS, e.classname));
    }

    if(((bits & ARK_FEED) != 0) && (time >= ar_mgr.ark_feed_at))
    {
        ar_mgr.ark_feed_at = time + ARK_FEED_GAP;
        float before = by.health;
        VR_ARK_Heal(by, ARK_FEED_HP);
        dprint(sprintf("arena_perks: Feeder: %g back for %s (%g health)\n", by.health - before, e.classname, by.health));
    }
}

// vr_decap.qc: a foe beheaded by `by` (Neck Snapper).
void(entity e, entity by, float xHow) VR_ARK_Decap =
{
    if(!VR_ARK_Running()) { return; }
    if((ar_mgr.ark_taken & ARK_SNAP) == 0) { return; }
    if(!by || !(by.flags & FL_CLIENT)) { return; }

    float before = by.health;
    VR_ARK_Heal(by, ARK_SNAP_HP);
    dprint(sprintf("arena_perks: Neck Snapper: %g back for %s (%g health)\n", by.health - before, e.classname, by.health));
}

// items.qc's armor_touch, just after it set what the armour is worth (Iron Skin: 25 heavier). Its own value, so
// green goes to 125, yellow to 175, red to 225 — never more than 25 past what the shipped armour is worth.
void(entity p, float xValue) VR_ARK_ArmorBonus =
{
    if(!VR_ARK_Running()) { return; }
    if((ar_mgr.ark_taken & ARK_IRON) == 0) { return; }
    if(!(p.flags & FL_CLIENT)) { return; }
    p.armorvalue = xValue + ARK_IRON_ARM;
    dprint(sprintf("arena_perks: Iron Skin: armour worth %g, not %g\n", p.armorvalue, xValue));
}

// vr_arena.qc VR_ARENA_GunMost (vr_enemyguns.qc's cap on dropped guns, in the arena): with Spoils taken twice
// vr_arena_gun_max of the guns the foes dropped lie about (48 by default) before the oldest fades.
float() VR_ARK_Kept =
{
    if(!VR_ARK_Running()) { return FALSE; }
    return (ar_mgr.ark_taken & ARK_SPOIL) != 0;
}

// vr_agarmor.qc VR_AgArmor_Damage: what a plate's pool pays for each point of a hit it saves. 1 everywhere but a
// player's own hit with Can Opener taken: 2, so a plate breaks in half the hits (it still saves what it saves
// while it lasts -- the hit is not made bigger, the plate is made thinner).
float(entity xBy) VR_ARK_PlateCost =
{
    if(!VR_ARK_Running()) { return 1; }
    if((ar_mgr.ark_taken & ARK_OPEN) == 0) { return 1; }
    if(!(xBy.flags & FL_CLIENT)) { return 1; }
    return ARK_OPEN_COST;
}
