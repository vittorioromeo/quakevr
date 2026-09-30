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
//   each such pull starts it at a chance (vr_chainsaw_start_chance): a few tries may be needed. A weak pull only pulls
//   the cord. With an empty tank it never starts.
// - Running, it idles (vr_chainsaw_idle_fuel_use a second), and the trigger runs its chain (vr_chainsaw_fuel_use a
//   second more): the chain's bar cuts what it is in, monsters and their corpses' models as drawn (precise hit
//   detection: hitmodel_segment, the melee's tolerance), vr_chainsaw_damage a second, in ticks of a tenth of a second.
//   The client lets its bar sink into a monster's model while the chain runs (vr_chainsaw_overlap, cm;
//   vr_modelcollide.cpp) instead of stopping it at the surface.
// - Out of fuel, it stalls. Holstered or thrown, it stops (its flags' QVR_WPNFLAG_SAW_STATE are a hand's).
// - The weapon's ammo counter shows its fuel.

.float vr_saw_loop;       // the main hand's chainsaw's sound playing (VR_SAW_LOOP_*)
.float vr_saw_loop2;      // the off hand's
.float vr_saw_loop_next;  // when that loop is played again
.float vr_saw_loop_next2;
.float vr_saw_cut_next;   // the next cut (a tick of damage)
.float vr_saw_cut_next2;
.float vr_saw_cut_last;   // the last cut that met something (the cutting sound holds a moment)
.float vr_saw_cut_last2;
.float vr_saw_buzz_next;  // the next rumble in the hand
.float vr_saw_buzz_next2;
.float vr_saw_cord_prev;  // the engine's cord bits (.sawcord) as they were last frame: a pull's edge
.entity vr_saw_victim;    // the cut's victims, linked (findradius's .chain is not kept across T_Damage)
.vector vr_saw_at;        // and where the bar met each

float VR_SAW_LOOP_NONE = 0;
float VR_SAW_LOOP_IDLE = 1;
float VR_SAW_LOOP_RUN = 2;
float VR_SAW_LOOP_CUT = 3;
float VR_SAW_LOOP_START = 4; // the start's sound, then the idle

float VR_SAW_TICK = 0.1;      // seconds between cuts
float VR_SAW_BAR_START = 0.55; // the bar out of the engine block: this share of the way from the hand (the rear handle) to the tip

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
    precache_sound("vr/saw_pull.wav");  // the cord pulled: its zip, the engine coughing
    precache_sound("vr/saw_start.wav"); // the cord pulled and the engine catching
    precache_sound("vr/saw_idle.wav");  // idling (played back to back)
    precache_sound("vr/saw_run.wav");   // the chain running
    precache_sound("vr/saw_cut.wav");   // and cutting
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

float(float xHand) VR_Saw_LoopNext = { return xHand == cVR_OffHand ? self.vr_saw_loop_next2 : self.vr_saw_loop_next; };

void(string xWhat) VR_Saw_Debug =
{
    if(cvar_hget(cvarh_vr_debug_chainsaw))
    {
        dprint(xWhat);
    }
};

string(float xHand) VR_Saw_HandName = { return xHand == cVR_OffHand ? "off" : "main"; };

// An ogre dies: its chainsaw falls where it held it (by its right side as it stands), 40-100% full.
void() VR_DropOgreChainsaw =
{
    makevectors(self.angles);
    vector from = self.origin + '0 0 8';
    vector at = self.origin - v_forward * 12 - v_right * 16 + '0 0 8';
    traceline(from, at, TRUE, self);
    at = trace_endpos + (from - trace_endpos) * 0.2 * (trace_fraction < 1);
    vector vel = v_forward * -15 - v_right * 20 + v_right * (crandom() * 15) + '0 0 60';
    CreateThrownWeapon(self, WID_CHAINSAW, 0, floor(40 + random() * 60) /* its fuel */,
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
            if(along == -2)
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
        if(T_DamageImpl(e, self, self, dmg, 1) && alive)
        {
            VR_QuadSound(self, "the chainsaw cut");
        }
        VR_ClearHitKind();
        if(e.flags & FL_MONSTER || e.flags & FL_CLIENT)
        {
            SpawnBlood(at, (crandom() * v_right + v_up) * 60, dmg * 2);
        }
        else
        {
            particle2(at, '0 0 40', QVR_PARTICLE_PRESET_SPARKS, 4);
        }
        if(cvar_hget(cvarh_vr_debug_chainsaw))
        {
            dprint(sprintf("chainsaw: %s hand cuts %s: %g damage, health %g\n", VR_Saw_HandName(xHand), e.classname, dmg,
                e.health));
        }
    }
    return TRUE;
};

// Every frame, each hand (PlayerPostThink): a chainsaw held there runs, cuts, drinks its fuel, stalls; its cord.
void(float xHand) VR_Chainsaw_Frame =
{
    float cord = self.sawcord;
    float heldBit = xHand == cVR_OffHand ? VR_SAWCORD_MAIN_HOLDS : VR_SAWCORD_OFF_HOLDS;
    float pullBit = xHand == cVR_OffHand ? VR_SAWCORD_MAIN_PULLED : VR_SAWCORD_OFF_PULLED;
    float pulled = (cord & pullBit) && !(self.vr_saw_cord_prev & pullBit);
    self.vr_saw_cord_prev = (self.vr_saw_cord_prev - (self.vr_saw_cord_prev & pullBit)) | (cord & pullBit);

    float loop = VR_Saw_Loop(xHand);
    if(VRGetWeapon(xHand) != WID_CHAINSAW || self.health <= 0)
    {
        if(loop != VR_SAW_LOOP_NONE)
        {
            sound(self, VRGetGunChannel(xHand), "misc/null.wav", 1, ATTN_NORM); // (it left the hand)
            VR_Saw_SetLoop(xHand, VR_SAW_LOOP_NONE, 0);
        }
        return;
    }

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
            sound(self, CHAN_AUTO, "vr/saw_pull.wav", 0.5, ATTN_NORM);
            VR_Saw_Debug(sprintf("chainsaw: %s hand's cord pulled: already running\n", VR_Saw_HandName(xHand)));
        }
        else if(fuel <= 0)
        {
            sound(self, CHAN_AUTO, "vr/saw_pull.wav", 0.7, ATTN_NORM);
            VR_Saw_Debug(sprintf("chainsaw: %s hand's cord pulled: no fuel\n", VR_Saw_HandName(xHand)));
        }
        else if(random() < cvar_hget(cvarh_vr_chainsaw_start_chance))
        {
            running = TRUE;
            sound(self, chan, "vr/saw_start.wav", 1, ATTN_NORM);
            VR_Saw_SetLoop(xHand, VR_SAW_LOOP_START, time + 0.9);
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
        chain = VRGetFireButtonPressed(xHand) && !VRIsHandCarryingByForegrip(self, xHand);
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
        cutting = chain && time - last < 0.2;

        // Its sound: the start's until it ends, then idling, the chain running, cutting; each played again as it ends.
        float want = cutting ? VR_SAW_LOOP_CUT : chain ? VR_SAW_LOOP_RUN : VR_SAW_LOOP_IDLE;
        if(loop == VR_SAW_LOOP_START && time < VR_Saw_LoopNext(xHand) && want == VR_SAW_LOOP_IDLE)
        {
            want = VR_SAW_LOOP_START;
        }
        if(want != loop || time >= VR_Saw_LoopNext(xHand))
        {
            if(want == VR_SAW_LOOP_CUT)
            {
                sound(self, chan, "vr/saw_cut.wav", 1, ATTN_NORM);
                VR_Saw_SetLoop(xHand, want, time + 0.3);
            }
            else if(want == VR_SAW_LOOP_RUN)
            {
                sound(self, chan, "vr/saw_run.wav", 1, ATTN_NORM);
                VR_Saw_SetLoop(xHand, want, time + 0.3);
            }
            else
            {
                sound(self, chan, "vr/saw_idle.wav", 0.8, ATTN_NORM);
                VR_Saw_SetLoop(xHand, VR_SAW_LOOP_IDLE, time + 0.4);
            }
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
        VR_Saw_SetLoop(xHand, VR_SAW_LOOP_NONE, 0);
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
