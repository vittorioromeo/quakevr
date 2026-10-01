//
//
//
// ----------------------------------------------------------------------------
// The ogres' chainsaw (WID_CHAINSAW; docs/vr-port/ROUND21.md, "The ogres' chainsaw")
// ----------------------------------------------------------------------------
//
// Every ogre drops its chainsaw when it dies (gibbed or not; its death frames are drawn without it:
// vr_monstermods.cpp), a weapon lying there to be picked up (progs/v_chainsaw.mdl, Misc/quakevr/make_chainsaw.py):
// heavy, held in both hands as the laser cannon (its front handle is a two-handed grip).
//
// - Its fuel is its own: the weapon's clip (0 .. VR_CHAINSAW_TANK, percent), which goes with it from hand to hand, into
//   a holster, onto the ground and back. An ogre's is 40-100% full; impulse 164 (184: the off hand) gives a full one.
// - Its engine starts with its cord: the other hand takes the cord's handle on its top (the client: vr_chainsaw.cpp,
//   which draws the cord) and pulls it out, far and fast enough (vr_chainsaw_pull_distance, vr_chainsaw_pull_speed);
//   each such pull starts it at a chance (vr_chainsaw_start_chance), after the first one or two of a start, which always
//   fail (vr_chainsaw_fail_pulls_min..max: the author's, NOTES.md 2026-09-30 16:21): a few tries are needed. A weak pull
//   only pulls the cord. With an empty tank it never starts.
// - Running, it idles (vr_chainsaw_idle_fuel_use a second), and the trigger runs its chain (vr_chainsaw_fuel_use a
//   second more): the chain's bar cuts what it is in, monsters and their corpses' models as drawn (precise hit
//   detection: hitmodel_segment, the melee's tolerance), gibs and heads by their boxes, breakables and explosive boxes
//   by the line, vr_chainsaw_damage a second, in ticks of a tenth of a second.
//   The client lets its bar sink into a monster's model while the chain runs (vr_chainsaw_overlap, cm;
//   vr_modelcollide.cpp) instead of stopping it at the surface.
// - Out of fuel, it stalls. Holstered, it stops (its flags' QVR_WPNFLAG_SAW_STATE are a hand's). Dropped or thrown
//   running, it runs on a few seconds as it lies (vr_chainsaw_drop_run; VR_Saw_PropRuns), still running if taken back.
// - The weapon's ammo counter shows its fuel. Empty, the trigger clicks dry, as an empty gun's (every half a second held).
// - A zombie heeds no tick of it (each below the 9 it ignores): the chain's cutting adds up on it instead
//   (orig_mon_zombie.qc VR_Zombie_SawHurt): about a third of a second puts it down, as a heavy hit does; cut on while it
//   lies there (not solid: found apart), it is gibbed.

.float vr_saw_loop;       // the main hand's chainsaw's sound playing (VR_SAW_LOOP_*)
.float vr_saw_loop2;      // the off hand's
.float vr_saw_loop_next;  // (unused since the sounds loop by themselves: kept for the saves' fields)
.float vr_saw_loop_next2;
// A loaded game's (or a new map's) sounds are gone, whatever the players' vr_saw_loop say: not saved, these play the loops
// again a moment in (VR_Saw_SoundsLive).
nosave float vr_saw_sounds_live;
nosave float vr_saw_replay_at;
.float vr_saw_cut_next;   // the next cut (a tick of damage)
.float vr_saw_cut_next2;
.float vr_saw_cut_last;   // the last cut that met something (the cutting sound holds a moment)
.float vr_saw_cut_last2;
.float vr_saw_buzz_next;  // the next rumble in the hand
.float vr_saw_buzz_next2;
.float vr_saw_cord_prev;  // the engine's cord bits (.sawcord) as they were last frame: a pull's edge
.float vr_saw_pulls;      // good pulls that failed since it last ran (a start's first ones always fail)
.float vr_saw_pulls2;
.float vr_saw_fails;      // how many of them always fail this start (vr_chainsaw_fail_pulls_min..max, drawn at its first)
.float vr_saw_fails2;
.entity vr_saw_victim;    // the cut's victims, linked (findradius's .chain is not kept across T_Damage)
.vector vr_saw_at;        // and where the bar met each
.float vr_saw_click_next; // empty, the trigger's next dry click (as an empty gun's, every half a second held)
.float vr_saw_click_next2;

// A zombie and the chain (orig_mon_zombie.qc: its knockdown and gibbing add the chain's ticks up).
float(entity z, float hurt) VR_Zombie_SawHurt;
float(entity z) VR_Zombie_SawDown;

float VR_SAW_LOOP_NONE = 0;
float VR_SAW_LOOP_IDLE = 1;
float VR_SAW_LOOP_RUN = 2;
float VR_SAW_LOOP_CUT = 3;
float VR_SAW_LOOP_START = 4; // the start's sound, then the idle

float VR_SAW_TICK = 0.1;      // seconds between cuts
float VR_SAW_BAR_START = 0.55; // the bar out of the engine block: this share of the way from the hand (the rear handle) to the tip
float VR_SAW_BAR_HALF = 3;     // units: half the bar's height with its teeth (a gib's box is grown by it)

// .sawcord (the engine, from the client's move: vr_chainsaw.cpp): the off hand holds the cord of the main hand's
// chainsaw, and the other way round; a pull of it (held for 0.15 s after the pull, so that no server frame misses it).
float VR_SAWCORD_OFF_HOLDS = 1;
float VR_SAWCORD_MAIN_HOLDS = 2;
float VR_SAWCORD_OFF_PULLED = 4;
float VR_SAWCORD_MAIN_PULLED = 8;

float VR_SAW_PULLED_FRAME = 9; // v_chainsaw.mdl's frame without the cord's handle (it is in the hand)

void() VR_Chainsaw_Precache =
{
    precache_model("progs/v_chainsaw.mdl");
    // Recorded (a Stihl MS260; Misc/quakevr/make_chainsaw_sounds.py): the loops carry cue points, so a channel playing
    // one loops it until another sound replaces it.
    precache_sound("vr/saw_pull_weak.wav"); // the cord pulled, the engine turned over and not firing
    precache_sound("vr/saw_pull.wav");      // a strong pull: its zip, the engine firing a moment and dying
    precache_sound("vr/saw_start.wav");     // the cord pulled and the engine catching, then idling (its loop)
    precache_sound("vr/saw_idle.wav");      // idling (a loop)
    precache_sound("vr/saw_run.wav");       // the chain running (a loop)
    precache_sound("vr/saw_cut.wav");       // and cutting (a loop)
    precache_sound("vr/saw_stall.wav"); // out of fuel
    precache_sound("misc/null.wav");
};

float(float xHand) VR_Saw_Loop = { return xHand == cVR_OffHand ? self.vr_saw_loop2 : self.vr_saw_loop; };

void(float xHand, float xLoop, float xNext) VR_Saw_SetLoop =
{
    if(xHand == cVR_OffHand)
    {
        self.vr_saw_loop2 = xLoop;
        self.vr_saw_loop_next2 = xNext;
    }
    else
    {
        self.vr_saw_loop = xLoop;
        self.vr_saw_loop_next = xNext;
    }
};

// A server's first chainsaw frames (a new map, a loaded game): the loops played again half a second in. Not at once: the
// client hears nothing before it has begun (Host_Begin_f; a sound sent before is lost), and QC can't tell when that is.
void() VR_Saw_SoundsLive =
{
    if(!vr_saw_sounds_live)
    {
        vr_saw_sounds_live = TRUE;
        vr_saw_replay_at = time + 0.5;
        return;
    }
    if(!vr_saw_replay_at || time < vr_saw_replay_at)
    {
        return;
    }
    vr_saw_replay_at = 0;
    VR_Saw_Debug("chainsaw: loops played again (a new map or a loaded game)\n");
    entity e = find(world, classname, "player");
    while(e)
    {
        e.vr_saw_loop = VR_SAW_LOOP_NONE;
        e.vr_saw_loop2 = VR_SAW_LOOP_NONE;
        e = find(e, classname, "player");
    }
};

// A hand's loop stopped (the chainsaw left it, the player died, the intermission).
void(float xHand) VR_Saw_Silence =
{
    if(VR_Saw_Loop(xHand) != VR_SAW_LOOP_NONE)
    {
        sound(self, VRGetGunChannel(xHand), "misc/null.wav", 1, ATTN_NORM);
        VR_Saw_SetLoop(xHand, VR_SAW_LOOP_NONE, 0);
    }
};

// PlayerPostThink's early returns (dead, the intermission): the loops would play on.
void() VR_Chainsaw_Silence =
{
    VR_Saw_SoundsLive();
    VR_Saw_Silence(cVR_MainHand);
    VR_Saw_Silence(cVR_OffHand);
};

void(string xWhat) VR_Saw_Debug =
{
    if(cvar_hget(cvarh_vr_debug_chainsaw))
    {
        dprint(xWhat);
    }
};

string(float xHand) VR_Saw_HandName = { return xHand == cVR_OffHand ? "off" : "main"; };

// A running chainsaw dropped or thrown (weapons.qc DropWeaponInHandScaled) runs on as it lies, vr_chainsaw_drop_run
// seconds, its idle heard from it as it flies (the engine's sounds follow their entity: vr_audio.cpp), then dies; taken
// back into a hand meanwhile, it is still running, no cord to pull (NOTES.md vrfiringrange_2026-10-01_11-33-00). Its
// chain stops as it leaves the hand. Its weaponflags keep QVR_WPNFLAG_SAW_RUNNING while it runs.
.float vr_saw_prop_on; // when it was let go running (0: not running)

void(entity xProp) VR_Saw_PropRuns =
{
    xProp.weaponflags = xProp.weaponflags | QVR_WPNFLAG_SAW_RUNNING;
    xProp.vr_saw_prop_on = time;
    sound(xProp, CHAN_WEAPON, "vr/saw_idle.wav", 1, ATTN_NORM);
    VR_Saw_Debug(sprintf("chainsaw: let go running: runs on %g s, fuel %g\n", cvar_hget(cvarh_vr_chainsaw_drop_run),
        xProp.weaponclip));
};

// Its fuel as burnt idling since it was let go; it stops (not running, its flag cleared) when `xStop`.
void(entity xProp, float xStop) VR_Saw_PropSettle =
{
    xProp.weaponclip = max(0, xProp.weaponclip - (time - xProp.vr_saw_prop_on) * cvar_hget(cvarh_vr_chainsaw_idle_fuel_use));
    if(xStop)
    {
        xProp.vr_saw_prop_on = 0;
        xProp.weaponflags = xProp.weaponflags - (xProp.weaponflags & QVR_WPNFLAG_SAW_STATE);
    }
};

// It is removed (a deathmatch timeout, gone on a hit): its loop would play on.
void(entity xProp) VR_Saw_PropStop =
{
    if(xProp.vr_saw_prop_on)
    {
        VR_Saw_PropSettle(xProp, TRUE);
        sound(xProp, CHAN_WEAPON, "misc/null.wav", 1, ATTN_NORM);
    }
};

// Taken into a hand (wpnthrow_handtouch_impl): still running, its sound the hand's from now on (VR_Chainsaw_Frame).
void(entity xProp) VR_Saw_PropTaken =
{
    if(xProp.vr_saw_prop_on)
    {
        VR_Saw_PropSettle(xProp, xProp.weaponclip <= 0 || time >= xProp.vr_saw_prop_on + cvar_hget(cvarh_vr_chainsaw_drop_run));
        if(xProp.weaponflags & QVR_WPNFLAG_SAW_RUNNING)
        {
            xProp.vr_saw_prop_on = 0; // (the hand runs it now)
        }
        sound(xProp, CHAN_WEAPON, "misc/null.wav", 1, ATTN_NORM);
        VR_Saw_Debug(sprintf("chainsaw: taken back %s, fuel %g\n",
            (xProp.weaponflags & QVR_WPNFLAG_SAW_RUNNING) ? "running" : "stopped", xProp.weaponclip));
    }
};

// Each think of a thrown weapon (wpnthrow_think): a chainsaw let go running dies when its time is up (or its tank dry).
void() VR_Saw_PropThink =
{
    if(!self.vr_saw_prop_on)
    {
        return;
    }
    float left = self.weaponclip - (time - self.vr_saw_prop_on) * cvar_hget(cvarh_vr_chainsaw_idle_fuel_use);
    if(left > 0 && time < self.vr_saw_prop_on + cvar_hget(cvarh_vr_chainsaw_drop_run))
    {
        return;
    }
    VR_Saw_PropSettle(self, TRUE);
    sound(self, CHAN_WEAPON, "vr/saw_stall.wav", 1, ATTN_NORM); // (the engine dying: replaces the idle's loop)
    VR_Saw_Debug(sprintf("chainsaw: let go: its engine stopped, fuel %g\n", self.weaponclip));
};

// An ogre dies: its chainsaw falls where it held it (by its right side as it stands), 40-100% full (vr_chainsaw_drop_fuel_min).
void() VR_DropOgreChainsaw =
{
    makevectors(self.angles);
    vector from = self.origin + '0 0 8';
    vector at = self.origin - v_forward * 12 - v_right * 16 + '0 0 8';
    traceline(from, at, TRUE, self);
    at = trace_endpos + (from - trace_endpos) * 0.2 * (trace_fraction < 1);
    vector vel = v_forward * -15 - v_right * 20 + v_right * (crandom() * 15) + '0 0 60';
    float least = min(max(0, cvar_hget(cvarh_vr_chainsaw_drop_fuel_min)), VR_CHAINSAW_TANK); // Enemy Weapons: Fuel When Dropped
    CreateThrownWeapon(self, WID_CHAINSAW, 0, floor(least + random() * (VR_CHAINSAW_TANK - least)) /* its fuel */,
        at, self.angles, vel, randomVec(40),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(WID_CHAINSAW));
};

// The chainsaw in `xHand` cuts (its chain running): what its bar is in takes a tick's damage. Whether it met anything.
float(float xHand) VR_Chainsaw_Cut =
{
    vector hand = VRGetHandPos(xHand);
    vector tip = VRGetMuzzlePos(xHand);
    vector root = hand + (tip - hand) * VR_SAW_BAR_START;
    vector mid = (root + tip) * 0.5;

    // What the bar is in: the models of monsters and their corpses (the box's broad phase grown by a model's reach); a
    // thing without a model tested (a breakable's brushes, a player), by the line through its box.
    entity list = world;
    entity e = findradius(mid, vlen(tip - root) * 0.5 + VR_HITMODEL_REACH);
    while(e)
    {
        if(e != self && e.takedamage && (!(e.flags & FL_CLIENT) || deathmatch || teamplay))
        {
            float along = hitmodel_segment(e, root, tip, 1, HITCLASS_MELEE);
            if(along == -2 && e.vr_gib)
            {
                // A loose gib or head (vr_carry.qc VR_MakeGibGrabbable): no longer a monster (hitmodel_segment's -2)
                // and not solid (SOLID_NOT_BUT_TOUCHABLE: a traceline passes it), so by its box (its model's), grown by
                // the bar's half-thickness: as the melee sweeps them (vr_melee.qc). One held in a hand is left alone.
                along = -1;
                if(e.solid == SOLID_NOT_BUT_TOUCHABLE && e.carry_player == world)
                {
                    along = VR_SegmentBox(root, tip, e.origin + e.mins - '1 1 1' * VR_SAW_BAR_HALF,
                        e.origin + e.maxs + '1 1 1' * VR_SAW_BAR_HALF);
                }
            }
            else if(along == -2)
            {
                traceline(root, tip, FALSE, self);
                along = trace_ent == e ? trace_fraction : -1;
            }
            if(cvar_hget(cvarh_vr_debug_chainsaw) >= 2)
            {
                dprint(sprintf("chainsaw: bar %v .. %v near %s at %v: %g\n", root, tip, e.classname, e.origin, along));
            }
            if(along >= 0)
            {
                e.vr_saw_victim = list;
                e.vr_saw_at = root + (tip - root) * along;
                list = e;
            }
        }
        e = e.chain;
    }
    // A zombie knocked down lies there not solid (orig_mon_zombie.qc zombie_paine10), which findradius passes by: by its
    // model (else its box, grown by the bar's half-thickness), to be gibbed.
    e = find(world, classname, "monster_zombie");
    while(e)
    {
        if(e.solid == SOLID_NOT && e.takedamage && VR_Zombie_SawDown(e) &&
           vlen(e.origin - mid) < vlen(tip - root) * 0.5 + VR_HITMODEL_REACH)
        {
            float lying = hitmodel_segment(e, root, tip, 1, HITCLASS_MELEE);
            if(lying == -2)
            {
                lying = VR_SegmentBox(root, tip, e.origin + e.mins - '1 1 1' * VR_SAW_BAR_HALF,
                    e.origin + e.maxs + '1 1 1' * VR_SAW_BAR_HALF);
            }
            if(cvar_hget(cvarh_vr_debug_chainsaw) >= 2)
            {
                dprint(sprintf("chainsaw: bar %v .. %v near a zombie lying at %v: %g\n", root, tip, e.origin, lying));
            }
            if(lying >= 0)
            {
                e.vr_saw_victim = list;
                e.vr_saw_at = root + (tip - root) * lying;
                list = e;
            }
        }
        e = find(e, classname, "monster_zombie");
    }
    if(list == world)
    {
        return FALSE;
    }

    float dmg = cvar_hget(cvarh_vr_chainsaw_damage) * VR_SAW_TICK;
    vector dir = normalize(tip - root);
    while(list != world)
    {
        e = list;
        list = e.vr_saw_victim;
        e.vr_saw_victim = world;
        vector at = e.vr_saw_at;
        VR_SetHitKind(QVR_HITKIND_MELEE, xHand, at, dir);
        self.damage_weapon = IT_AXE;
        float alive = e.health > 0;
        float flesh = (e.flags & FL_MONSTER) || (e.flags & FL_CLIENT) || e.vr_gib; // (a gib may be gone after it)
        float hurt = dmg;
        if(e.vr_gib)
        {
            // The chain shreds a gib or a head at once (a tick's blow would knock it off the bar).
            hurt = max(dmg, e.vr_gib_hp);
        }
        hurt = VR_Zombie_SawHurt(e, hurt); // (a zombie: the tick that puts it down, or gibs it down)
        if(T_DamageImpl(e, self, self, hurt, 1) && alive)
        {
            VR_QuadSound(self, "the chainsaw cut");
        }
        VR_ClearHitKind();
        if(flesh)
        {
            SpawnBlood(at, (crandom() * v_right + v_up) * 60, dmg * 2);
        }
        else
        {
            particle2(at, '0 0 40', QVR_PARTICLE_PRESET_SPARKS, 4);
        }
        if(cvar_hget(cvarh_vr_debug_chainsaw))
        {
            dprint(sprintf("chainsaw: %s hand cuts %s: %g damage, health %g\n", VR_Saw_HandName(xHand), e.classname, hurt,
                e.health));
        }
    }
    return TRUE;
};

// A good pull of `xHand`'s chainsaw's cord, not running, with fuel: counted; TRUE if it is one of the start's first,
// which always fail (vr_chainsaw_fail_pulls_min..max, drawn at the start's first pull).
float(float xHand) VR_Saw_ForcedFail =
{
    float pulls = xHand == cVR_OffHand ? self.vr_saw_pulls2 : self.vr_saw_pulls;
    float fails = xHand == cVR_OffHand ? self.vr_saw_fails2 : self.vr_saw_fails;
    if(pulls <= 0)
    {
        float lo = max(0, floor(cvar_hget(cvarh_vr_chainsaw_fail_pulls_min)));
        float hi = max(lo, floor(cvar_hget(cvarh_vr_chainsaw_fail_pulls_max)));
        fails = min(lo + floor(random() * (hi - lo + 1)), hi);
    }
    pulls = pulls + 1;
    if(xHand == cVR_OffHand)
    {
        self.vr_saw_pulls2 = pulls;
        self.vr_saw_fails2 = fails;
    }
    else
    {
        self.vr_saw_pulls = pulls;
        self.vr_saw_fails = fails;
    }
    VR_Saw_Debug(sprintf("chainsaw: %s hand's pull %g of the start (%g always fail)\n", VR_Saw_HandName(xHand), pulls, fails));
    return pulls <= fails;
};

// Every frame, each hand (PlayerPostThink): a chainsaw held there runs, cuts, drinks its fuel, stalls; its cord.
void(float xHand) VR_Chainsaw_Frame =
{
    float cord = self.sawcord;
    float heldBit = xHand == cVR_OffHand ? VR_SAWCORD_MAIN_HOLDS : VR_SAWCORD_OFF_HOLDS;
    float pullBit = xHand == cVR_OffHand ? VR_SAWCORD_MAIN_PULLED : VR_SAWCORD_OFF_PULLED;
    float pulled = (cord & pullBit) && !(self.vr_saw_cord_prev & pullBit);
    self.vr_saw_cord_prev = (self.vr_saw_cord_prev - (self.vr_saw_cord_prev & pullBit)) | (cord & pullBit);

    VR_Saw_SoundsLive();
    if(VRGetWeapon(xHand) != WID_CHAINSAW || self.health <= 0)
    {
        VR_Saw_Silence(xHand); // (it left the hand)
        if(xHand == cVR_OffHand) self.vr_saw_pulls2 = 0;
        else                     self.vr_saw_pulls = 0;
        return;
    }
    float loop = VR_Saw_Loop(xHand);

    float flags = VRGetWeaponFlags(xHand);
    float fuel = max(0, VRGetWeaponClip(xHand));
    float running = !!(flags & QVR_WPNFLAG_SAW_RUNNING);
    float chan = VRGetGunChannel(xHand);

    // The cord's handle is in the other hand: the model without it (the client draws it there).
    VRSetWeaponFrame(xHand, (cord & heldBit) ? VR_SAW_PULLED_FRAME : 0);

    if(pulled)
    {
        if(running)
        {
            sound(self, CHAN_AUTO, "vr/saw_pull_weak.wav", 0.6, ATTN_NORM);
            VR_Saw_Debug(sprintf("chainsaw: %s hand's cord pulled: already running\n", VR_Saw_HandName(xHand)));
        }
        else if(fuel <= 0)
        {
            sound(self, CHAN_AUTO, "vr/saw_pull_weak.wav", 1, ATTN_NORM);
            VR_Saw_Debug(sprintf("chainsaw: %s hand's cord pulled: no fuel\n", VR_Saw_HandName(xHand)));
        }
        else if(!VR_Saw_ForcedFail(xHand) && random() < cvar_hget(cvarh_vr_chainsaw_start_chance))
        {
            running = TRUE;
            if(xHand == cVR_OffHand) self.vr_saw_pulls2 = 0;
            else                     self.vr_saw_pulls = 0;
            sound(self, chan, "vr/saw_start.wav", 1, ATTN_NORM);
            VR_Saw_SetLoop(xHand, VR_SAW_LOOP_START, 0);
            loop = VR_SAW_LOOP_START;
            VRGunHaptic(xHand, 0.3, 60, 0.8);
            VR_Saw_Debug(sprintf("chainsaw: %s hand's cord pulled: started, fuel %g\n", VR_Saw_HandName(xHand), fuel));
        }
        else
        {
            sound(self, CHAN_AUTO, "vr/saw_pull.wav", 1, ATTN_NORM);
            VRGunHaptic(xHand, 0.15, 40, 0.4);
            VR_Saw_Debug(sprintf("chainsaw: %s hand's cord pulled: it didn't catch\n", VR_Saw_HandName(xHand)));
        }
    }

    float chain = FALSE;
    float cutting = FALSE;
    if(running)
    {
        // Held by its front handle alone (the hand-off), the trigger's hand is gone: it idles.
        chain = (VRGetFireButtonPressed(xHand) && !VRIsHandCarryingByForegrip(self, xHand));
        fuel = fuel - frametime * (cvar_hget(cvarh_vr_chainsaw_idle_fuel_use) +
                                   chain * cvar_hget(cvarh_vr_chainsaw_fuel_use));
        if(fuel <= 0)
        {
            fuel = 0;
            running = FALSE;
            chain = FALSE;
            sound(self, chan, "vr/saw_stall.wav", 1, ATTN_NORM);
            VR_Saw_SetLoop(xHand, VR_SAW_LOOP_NONE, 0);
            loop = VR_SAW_LOOP_NONE;
            VRGunHaptic(xHand, 0.2, 30, 0.5);
            // (the trigger held on: its first dry click after the stall's sound)
            if(xHand == cVR_OffHand) self.vr_saw_click_next2 = time + 0.5;
            else                     self.vr_saw_click_next = time + 0.5;
            VR_Saw_Debug(sprintf("chainsaw: %s hand's stalled: out of fuel\n", VR_Saw_HandName(xHand)));
        }
    }

    if(running)
    {
        float next = xHand == cVR_OffHand ? self.vr_saw_cut_next2 : self.vr_saw_cut_next;
        float last = xHand == cVR_OffHand ? self.vr_saw_cut_last2 : self.vr_saw_cut_last;
        if(chain && time >= next)
        {
            if(VR_Chainsaw_Cut(xHand))
            {
                last = time;
            }
            next = max(next + VR_SAW_TICK, time);
        }
        else if(!chain)
        {
            next = time;
        }
        if(xHand == cVR_OffHand)
        {
            self.vr_saw_cut_next2 = next;
            self.vr_saw_cut_last2 = last;
        }
        else
        {
            self.vr_saw_cut_next = next;
            self.vr_saw_cut_last = last;
        }
        // (An if: `cutting = chain && time - last < 0.2` is `(cutting = chain) && ...` to fteqcc, TRUE with nothing cut;
        // docs/vr-port/CODE_STYLE.md, QuakeC.)
        if(chain && time < last + 0.2)
        {
            cutting = TRUE;
        }

        // Its sound: idling, the chain running, cutting; each a loop, played once as it begins (the start's sound ends
        // in the idle's loop: it stands for idling).
        float want = cutting ? VR_SAW_LOOP_CUT : chain ? VR_SAW_LOOP_RUN : VR_SAW_LOOP_IDLE;
        float playing = loop == VR_SAW_LOOP_START ? VR_SAW_LOOP_IDLE : loop;
        if(want != playing)
        {
            if(want == VR_SAW_LOOP_CUT)
            {
                sound(self, chan, "vr/saw_cut.wav", 1, ATTN_NORM);
            }
            else if(want == VR_SAW_LOOP_RUN)
            {
                sound(self, chan, "vr/saw_run.wav", 1, ATTN_NORM);
            }
            else
            {
                sound(self, chan, "vr/saw_idle.wav", 1, ATTN_NORM);
            }
            VR_Saw_SetLoop(xHand, want, 0);
        }

        // It shakes in the hand: harder with the chain running, hardest cutting.
        float buzz = xHand == cVR_OffHand ? self.vr_saw_buzz_next2 : self.vr_saw_buzz_next;
        if(time >= buzz)
        {
            VRGunHaptic(xHand, 0.12, cutting ? 90 : chain ? 70 : 45, cutting ? 0.8 : chain ? 0.4 : 0.15);
            if(xHand == cVR_OffHand)
            {
                self.vr_saw_buzz_next2 = time + 0.1;
            }
            else
            {
                self.vr_saw_buzz_next = time + 0.1;
            }
        }
    }
    else if(loop != VR_SAW_LOOP_NONE)
    {
        VR_Saw_Silence(xHand); // (stopped some other way: its loop would play on)
    }

    // Empty, its trigger clicks dry, as an empty gun's (weapons.qc W_AttackImpl): every half a second held (NOTES.md
    // vrfiringrange_2026-10-01_00-12-44). Held by its front handle alone, the trigger's hand is gone.
    if(!running && fuel <= 0 && VRGetFireButtonPressed(xHand) && !VRIsHandCarryingByForegrip(self, xHand))
    {
        float click = xHand == cVR_OffHand ? self.vr_saw_click_next2 : self.vr_saw_click_next;
        if(time >= click)
        {
            sound(self, CHAN_AUTO, "gunclick.wav", 1, ATTN_NORM);
            if(xHand == cVR_OffHand) self.vr_saw_click_next2 = time + 0.5;
            else                     self.vr_saw_click_next = time + 0.5;
            VR_Saw_Debug(sprintf("chainsaw: %s hand's trigger: empty click at %g\n", VR_Saw_HandName(xHand), time));
        }
    }

    flags = flags - (flags & QVR_WPNFLAG_SAW_STATE);
    if(running)
    {
        flags = flags | QVR_WPNFLAG_SAW_RUNNING;
    }
    if(chain)
    {
        flags = flags | QVR_WPNFLAG_SAW_CHAIN;
    }
    VRSetWeaponFlags(xHand, flags);
    VRSetWeaponClip(xHand, fuel);
    VRSetAmmoCounter(xHand, ceil(fuel));
};

// impulse 227: the chainsaws in your hands nearly empty (5%: to see one stall). impulse 228: print them.
void(float xEmpty) VR_Chainsaw_Impulse =
{
    for(float h = 0; h < 2; h++)
    {
        float hand = h ? cVR_MainHand : cVR_OffHand;
        if(VRGetWeapon(hand) != WID_CHAINSAW)
        {
            continue;
        }
        if(xEmpty)
        {
            VRSetWeaponClip(hand, 5);
        }
        float flags = VRGetWeaponFlags(hand);
        bprint(sprintf("chainsaw: %s hand: fuel %g, running %g, chain %g, cord %g\n", VR_Saw_HandName(hand),
            VRGetWeaponClip(hand), !!(flags & QVR_WPNFLAG_SAW_RUNNING), !!(flags & QVR_WPNFLAG_SAW_CHAIN), self.sawcord));
    }
};

// impulse 230 (a test aid; Debug > Tests): the chainsaws in your hands started, as a good pull of the cord does.
void() VR_Chainsaw_StartAll =
{
    for(float h = 0; h < 2; h++)
    {
        float hand = h ? cVR_MainHand : cVR_OffHand;
        if(VRGetWeapon(hand) != WID_CHAINSAW || VRGetWeaponClip(hand) <= 0)
        {
            continue;
        }
        float flags = VRGetWeaponFlags(hand);
        if(!(flags & QVR_WPNFLAG_SAW_RUNNING))
        {
            VRSetWeaponFlags(hand, flags | QVR_WPNFLAG_SAW_RUNNING);
            sound(self, VRGetGunChannel(hand), "vr/saw_start.wav", 1, ATTN_NORM);
            VR_Saw_SetLoop(hand, VR_SAW_LOOP_START, 0);
        }
        bprint(sprintf("chainsaw: %s hand's started (test), fuel %g\n", VR_Saw_HandName(hand), VRGetWeaponClip(hand)));
    }
};

// impulse 229 (a test aid; Debug > Tests): the chainsaw lying nearest you taken into an empty hand, as a hand closing on
// it does (its handtouch, as a force grab's catch).
void() VR_Chainsaw_TakeNearest =
{
    entity best = world;
    float bestDist = 1000;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(e.weapon == WID_CHAINSAW && vlen(e.origin - self.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - self.origin);
        }
        e = find(e, classname, "thrown_weapon");
    }
    float hand = VRIsHandEmpty(self, cVR_MainHand) ? cVR_MainHand : VRIsHandEmpty(self, cVR_OffHand) ? cVR_OffHand : -1;
    if(best == world || hand < 0 || !best.handtouch)
    {
        bprint("chainsaw: none lying within 1000 units, or no empty hand\n");
        return;
    }
    bprint(sprintf("chainsaw: taking the one %g units away (fuel %g) into the %s hand\n", bestDist, best.weaponclip,
        VR_Saw_HandName(hand)));
    entity oldSelf = self;
    entity oldOther = other;
    self.touchinghand = hand;
    best.handtouch_hand = hand;
    best.handtouch_ent = self;
    other = self;
    self = best;
    float oldCatching = forcegrabCatching;
    forcegrabCatching = TRUE;
    best.handtouch();
    forcegrabCatching = oldCatching;
    self = oldSelf;
    other = oldOther;
};
