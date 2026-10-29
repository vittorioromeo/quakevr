// vr_knockdown.qc -- knockdowns (Combat > Knockdowns; the engine's vr_box3d.cpp, "Knockdowns"): a shove can knock a
// monster down as a ragdoll, alive, and after a few seconds it gets up and fights on.
//
// - Which: the monsters with a ragdoll rig (canragdoll; vr_ragdoll on), not flying nor swimming, each with its own base
//   chance (vr_knockdown_chance_<monster>: the grunts' the highest), set as it spawns (VR_Knockdown_SetupOwn, _Reverse).
// - The chance of a shove that lands (VR_Bash_Hit, vr_melee.qc): its base chance, times vr_knockdown_chance (all of
//   them), times 1 + vr_knockdown_damage x the share of its health it has lost, times vr_knockdown_onehand or _twohand
//   (a weapon's bash: vr_knockdown_bash too), times 1 - vr_knockdown_stamina x the share of your stamina spent (with the
//   stamina pool on). vr_knockdown_debug (developer 1) prints each roll.
// - Over a ledge (vr_knockdown_ledge): a shove that would carry it over the edge of a drop of vr_knockdown_ledge_drop or
//   more (64: higher than stairs) knocks it down for sure, if its chance is above 0 (VR_Knockdown_Ledge).
// - Down: touchable, not solid, .vr_knockdown 1: the engine makes its ragdoll at once from the frame it is drawn in, its
//   weapon welded in its hand. Hits land on the ragdoll (its limbs as drawn; the hands' blows too, as a live one's:
//   beheading and limbs, vr_melee.qc VR_Melee_BoxTarget) and it reacts as it does (its pain sounds,
//   blood; no pain frames: T_DamageImpl keeps its frame and think), each keeping it down at least vr_knockdown_hit_time
//   after it; a hand holding it (dragging, lifting) keeps it down while held and vr_knockdown_keep_time after; never past
//   vr_knockdown_keep_max s down in all, its own timer never cut short (VR_Knockdown_Keep).
//   It is grabbed, pushed and thrown as a dead one's. Killed there, it screams and its death code runs at once (its
//   drops; Killed: VR_Knockdown_FastForwardDeath): the ragdoll stays as it lies. Gibbed, it gibs; beheaded or its head
//   blown off, the ragdoll goes on headless. Thrown or dropped down there, it takes fall damage as a standing monster does
//   (combat.qc VR_Monster_Fall, from its ragdoll's hardest landing on the level: vr_box3d.cpp callFalls).
// - Room: when the ragdolls are at vr_ragdoll_max, the oldest dead one goes (removed; a mapper's corpse no player is near
//   lies as placed again), else the oldest knocked-down one is killed and removed (VR_Knockdown_MakeRoom); no room at
//   all: no knockdown.
// - Getting up, after vr_knockdown_time_min to _max seconds: the engine fits its get-up's first frame to how the body
//   lies and looks for room to stand there and around it (vr_knockdown_search: never through a wall, a floor or a
//   ceiling; none: it stays down, trying again every vr_knockdown_retry seconds), the hands holding it let go, and the
//   body blends from the ragdoll into the get-up (vr_knockdown_blend). The get-up: its own fall-and-rise frames from
//   their lowest (the grunt's painb, the enforcer's paind, the zombie's and mummy's paine, the ogre's paind, the
//   rottweiler's painb, the knight's painb), at vr_knockdown_getup_speed; a monster with none (the death knight, the
//   fiend, the shambler, the gremlin) plays a death backwards (of two, the one whose end fits how it lies). Then it runs.

// Shoved (vr_ragdoll_active_stagger): staggering as a ragdoll (the engine's; VR_Knockdown_Try), not yet down nor up.
.float vr_kd_stagger;
.float vr_kd_parry;   // its stagger a parry's (vr_kdtrig.qc VR_Parry_Ragdoll): its recovery ends the parry's
.float vr_kd_due;  // when its own timer runs out (vr_kd_until as it went down, before anything kept it down)
.float vr_kd_rise; // its timer ran out: its get-up begun (nothing keeps it down from then)

// Kept down (the author, 2026-10-16: "if I keep interacting with it, I should increase that limit up to a maximum"):
// knocked-down `m`, a hit, a stomp or a hand on it (xWhat) now, stays down at least xFor s more, past its own timer, but
// never more than vr_knockdown_keep_max s down in all; its own timer never cut short. Left alone, it gets up on its own
// time, or xFor after the last if that has passed. Not once its get-up began, nor a staggering one, nor a corpse.
// Its monster group's toughness (Combat > Monster Groups, "Toughness"; the Mini Bosses' and Elite Knights' below 1):
// every chance to knock `m` down times this (a sure knockdown too: by this chance), and its time down times this.
float(entity m) VR_KD_Scale =
{
    return max(0, monstergroupget(m, MGP_KDSCALE));
};
float(entity m) VR_KD_DownScale =
{
    return max(0.05, monstergroupget(m, MGP_DOWNTIME));
};
// A sure knockdown (over a ledge, unaware, from behind) of `m` still is: by its group's Knockdown Chance below 1.
float(entity m) VR_KD_SureRoll =
{
    float k = VR_KD_Scale(m);
    return k >= 1 || random() < k;
};

void(entity m, float xFor, string xWhat) VR_Knockdown_Keep =
{
    float most = cvar_hget(cvarh_vr_knockdown_keep_max) * VR_KD_DownScale(m);
    xFor = xFor * VR_KD_DownScale(m);
    if(m.health <= 0 || m.vr_knockdown != 1 || m.vr_kd_stagger || m.vr_kd_rise || xFor <= 0 || most <= 0)
    {
        return;
    }
    float until = min(m.vr_kd_since + most, time + xFor);
    if(until <= m.vr_kd_until)
    {
        return;
    }
    // (Held: asked every think. Printed at vr_knockdown_debug 2 only, or when it first keeps it past its own timer.)
    float dbg = cvar_hget(cvarh_vr_knockdown_debug);
    if(dbg >= 2 || (dbg && (xWhat != "held" || m.vr_kd_until <= m.vr_kd_due)))
    {
        dprint(sprintf("knockdown: %s kept down (%s): up in %.2f s at the soonest, %.2f s down so far (its own %.2f s, "
            "most %.2f s)\n", m.classname, xWhat, until - time, time - m.vr_kd_since, m.vr_kd_due - m.vr_kd_since, most));
    }
    m.vr_kd_until = until;
};

// A player's hand holds `m` (dragging it, lifting it).
float(entity m) VR_Knockdown_Held =
{
    for(entity p = VR_Player(); p != world; p = VR_NextPlayer(p))
    {
        if(VR_Carry_Held(p, cVR_MainHand) == m || VR_Carry_Held(p, cVR_OffHand) == m)
        {
            return TRUE;
        }
    }
    return FALSE;
};
.float vr_kd_balance; // its balance (VR_Knockdown_Try), and the shove's way: a loaded game's stagger begun again
.vector vr_kd_dir;
float VR_KD_STAGGER_SLOPE = 2.5; // a stagger's balance: 0.5 + this x (the roll - the knockdown's chance), 0 .. 1
float VR_KD_STAGGER_MOST = 8;    // s: a stagger still on this long after it began goes down (its ragdoll lost)

// Its class's chance and get-up: its own (from frame xFrame, the lowest of its fall-and-rise, its frame function xGetup on).
void(float xChanceH, float xFrame, void() xGetup) VR_Knockdown_SetupOwn =
{
    self.vr_kd_chance_h = xChanceH;
    self.vr_kd_frame = xFrame;
    self.vr_kd_frame2 = -1;
    self.vr_kd_getup = xGetup;
};

// ... a death played backwards (xLastA down to xFirstA; or xLastB to xFirstB, -1 none: whichever fits how it lies).
void(float xChanceH, float xLastA, float xFirstA, float xLastB, float xFirstB) VR_Knockdown_SetupReverse =
{
    self.vr_kd_chance_h = xChanceH;
    self.vr_kd_frame = xLastA;
    self.vr_kd_rev_end = xFirstA;
    self.vr_kd_frame2 = xLastB;
    self.vr_kd_rev_end2 = xFirstB;
};

float() VR_Knockdown_Interval =
{
    // (self the monster getting up: times its monster group's Get-Up Speed.)
    return 0.1 / min(10, max(0.1, cvar_hget(cvarh_vr_knockdown_getup_speed) * max(0.1, monstergroupget(self, MGP_GETUP))));
};

// Its body gone (a dead one's, or one killed to make room): as a corpse is removed.
void(entity e) VR_Knockdown_RemoveBody =
{
    VR_Head_StopFlies(e);
    remove(e);
};

// Whether a player is near placed corpse `e` (vr_mg_corpses.qc): within 1.5 times vr_corpse_placed_wake, where the
// engine wakes it (vr_box3d.cpp) and only farther lets a woken one that is no ragdoll sleep again.
float(entity e) VR_PlacedCorpse_Near =
{
    float reach = max(0, cvar("vr_corpse_placed_wake")) * 1.5;
    for(entity p = VR_Player(); p; p = VR_NextPlayer(p))
    {
        if(vlen(p.origin - e.origin) < reach)
        {
            return TRUE;
        }
    }
    return FALSE;
};

// Room for one more ragdoll (vr_ragdoll_max): the oldest dead ragdoll goes first, else the oldest knocked-down monster
// (but `xFor`) is killed and goes. FALSE: none can.
float vr_kd_room_dead_only; // (VR_KdTrig_Room: only a dead body goes)
float(entity xFor) VR_Knockdown_MakeRoom =
{
    float most = cvar("vr_ragdoll_max");
    if(most < 1)
    {
        return FALSE;
    }
    float guard;
    for(guard = 0; guard < 64; guard = guard + 1)
    {
        float count = 0;
        entity dead = world;
        entity alive = world;
        entity x = nextent(world);
        while(x)
        {
            // (A placed corpse put to sleep: its ragdoll goes at the engine's next sync, vr_box3d.cpp.)
            if(x != xFor && isragdoll(x) && x.vr_corpse_placed != 1)
            {
                count = count + 1;
                if(x.health <= 0)
                {
                    // (A dead player's own body stays while he is dead: his death view is on it, vr_deathdoll.qc. A
                    // placed corpse a player is near stays a ragdoll: he may be about to take it.)
                    if(!VR_DeathDoll_InUse(x) && !(x.vr_corpse_placed && VR_PlacedCorpse_Near(x)) &&
                       (dead == world || x.vr_corpse_since < dead.vr_corpse_since))
                    {
                        dead = x;
                    }
                }
                else if(x.vr_knockdown == 1 && (alive == world || x.vr_kd_since < alive.vr_kd_since))
                {
                    alive = x;
                }
            }
            x = nextent(x);
        }
        if(count < most)
        {
            return TRUE;
        }
        if(dead != world)
        {
            if(cvar_hget(cvarh_vr_knockdown_debug))
            {
                dprint(sprintf("knockdown: ragdolls at %g: a dead %s's body %s\n", most, dead.classname,
                    dead.vr_corpse_placed ? "lies as placed again" : "goes"));
            }
            if(dead.vr_corpse_placed)
            {
                dead.vr_corpse_placed = 1; // (a mapper's corpse stays: asleep, as placed; its ragdoll goes)
            }
            else
            {
                VR_Knockdown_RemoveBody(dead);
            }
        }
        else if(alive != world && !vr_kd_room_dead_only) // (a situational knockdown's room: never a living one killed)
        {
            if(cvar_hget(cvarh_vr_knockdown_debug))
            {
                dprint(sprintf("knockdown: ragdolls at %g: a knocked-down %s is killed, its body goes\n", most, alive.classname));
            }
            alive.health = -1;
            Killed(alive, world);
            if(alive.modelindex) // (not gone already: its death code may remove it)
            {
                VR_Knockdown_RemoveBody(alive);
            }
        }
        else
        {
            return FALSE;
        }
    }
    return FALSE;
};

// Its death code at once (Killed, knocked down): each of its death frames' functions run now, in turn (its sounds, its
// drops, not solid), to its last; the ragdoll stays as it lies (the engine keeps it, a dead one now). Stops at a think
// due later than a frame (a corpse's removal) or one that loops.
void() VR_Knockdown_FastForwardDeath =
{
    float i;
    for(i = 0; i < 40; i = i + 1)
    {
        if(!self.think || !self.modelindex || self.nextthink <= 0 || self.nextthink > time + 0.15)
        {
            return;
        }
        void() f = self.think;
        f();
        if(!self.modelindex || self.think == f)
        {
            return;
        }
    }
};

// The training dummy (self) is up: no AI, back to its training pose (the caller sets the next think).
void() VR_Knockdown_EaseDummy =
{
    self.think = vr_dummy_think;
    self.movetype = MOVETYPE_NONE;
    self.vr_dummy_painframe = 0;
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("knockdown: %s eased up, back in its training pose after %.1f s\n", self.classname,
            time - self.vr_kd_since));
    }
};

// Getting up, a death backwards (self): a frame back each step to its first, then it runs.
void() VR_Knockdown_Reverse =
{
    if(self.health <= 0 || self.vr_knockdown != 2)
    {
        return;
    }
    if(self.frame <= self.vr_kd_rev_to)
    {
        self.vr_knockdown = 0;
        self.think = self.th_run;
        self.nextthink = time + 0.1;
        if(VR_IsDummy(self)) // (the training dummy: no AI, back to its training pose; VR_Knockdown_Stand)
        {
            VR_Knockdown_EaseDummy();
            return;
        }
        if(self.vr_ad_mon)
        {
            VR_AD_RunNow(); // AD support: AD's monsters run by AD's th_run (vr_ad_monsters.qc)
        }
        return;
    }
    self.frame = self.frame - 1;
    self.think = VR_Knockdown_Reverse;
    self.nextthink = time + VR_Knockdown_Interval();
};

// Getting up, its own frames (self): the frame function due (each sets its frame and the next), at the get-up's speed,
// until it reaches its run (then it is up).
void() VR_Knockdown_Step =
{
    if(self.health <= 0 || self.vr_knockdown != 2)
    {
        return;
    }
    void() f = self.vr_kd_next;
    f();
    if(self.health <= 0 || self.vr_knockdown != 2)
    {
        return;
    }
    // (The training dummy has no th_run: its enemy's, kept, ends its get-up instead.)
    if(self.think == self.th_run || self.think == f || VR_AD_IsRunning(self) || // (AD support: AD's th_run)
       (VR_IsDummy(self) && self.think == self.vr_dummy_th_run))
    {
        self.vr_knockdown = 0; // (its run goes on at its own pace)
        if(VR_IsDummy(self)) // (the training dummy: no AI, back to its training pose)
        {
            VR_Knockdown_EaseDummy();
            self.nextthink = time + 0.05;
        }
        return;
    }
    self.vr_kd_next = self.think;
    self.think = VR_Knockdown_Step;
    self.nextthink = time + VR_Knockdown_Interval();
};

// Standing again (self): solid, its box and movement back, still, on the ground.
void() VR_Knockdown_Upright =
{
    self.solid = SOLID_SLIDEBOX;
    self.movetype = self.vr_kd_movetype;
    setsize(self, self.vr_kd_mins, self.vr_kd_maxs);
    setorigin(self, self.origin);
    self.velocity = '0 0 0';
    self.flags = self.flags | FL_ONGROUND;
    // (Stood a little over a slope, its box lifted clear of it, or forced up anywhere: it drops onto the floor.)
    tracebox(self.origin, self.mins, self.maxs, self.origin - '0 0 2', FALSE, self);
    if(trace_fraction >= 1 && !trace_startsolid)
    {
        self.flags = self.flags - (self.flags & FL_ONGROUND);
    }
};

// Up on its feet already (self; the engine handed its ragdoll back standing: a stagger recovered, its body risen through
// its get-up, vr_ragdoll_active_stagger and _getup): its AI on at once. xWhat: for vr_knockdown_debug.
void(string xWhat) VR_Knockdown_Afoot =
{
    VR_Knockdown_Upright();
    self.vr_knockdown = 0;
    self.vr_kd_stagger = 0;
    VR_Manhandle_Shaken(self); // (just back on its feet: a hand's hold may knock it down again a moment)
    // (Its run; the training dummy (a grunt with neither run nor AI) back to standing.)
    self.think = self.th_run;
    if(VR_IsDummy(self))
    {
        self.think = vr_dummy_think;
        self.movetype = MOVETYPE_NONE;
        self.vr_dummy_painframe = 0;
    }
    else if(self.vr_ad_mon)
    {
        VR_AD_RunNow(); // AD support: AD's monsters run by AD's th_run (its own th_run none: it stood frozen)
    }
    else if(!self.think)
    {
        self.think = self.th_stand;
    }
    if(!self.think)
    {
        self.think = SUB_Null;
    }
    self.nextthink = time + 0.1;
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("knockdown: %s %s after %.1f s\n", self.classname, xWhat, time - self.vr_kd_since));
    }
};

// Up from the ragdoll (self): solid again, standing, its get-up playing. xWhich: the frame the engine chose (1 or 2;
// 0: no ragdoll to blend from, up where it is).
void(float xWhich) VR_Knockdown_Stand =
{
    self.vr_knockdown = 2;
    VR_Knockdown_Upright();
    // (The training dummy too: it eases up through its get-up or its death backwards; VR_Knockdown_Step and _Reverse
    // hand it back to its idle callback, its practice pose/home, where a monster's would run.) One made before its
    // enemy's run was kept (an older save) can't tell its get-up's end: straight back to its training pose.
    if(VR_IsDummy(self) && self.vr_kd_getup && !self.vr_dummy_th_run)
    {
        VR_Knockdown_Afoot("returns to its training pose");
        return;
    }
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("knockdown: %s gets up after %.1f s\n", self.classname, time - self.vr_kd_since));
    }
    if(self.vr_kd_getup)
    {
        self.vr_kd_next = self.vr_kd_getup;
        VR_Knockdown_Step();
        return;
    }
    self.frame = xWhich == 2 ? self.vr_kd_frame2 : self.vr_kd_frame;
    self.vr_kd_rev_to = xWhich == 2 ? self.vr_kd_rev_end2 : self.vr_kd_rev_end;
    self.think = VR_Knockdown_Reverse;
    self.nextthink = time + VR_Knockdown_Interval();
};

// Down (self): waits, then tries to get up (room for it: VR_Knockdown_Stand; none: tries again a little later).
// In water, slime or lava (NOTES.md vrstart_2026-10-09_17-55-49): the engine looks for a floor where its head is out
// (the shallows' bottom, the bank: vr_knockdown_water_search); none for vr_knockdown_water_giveup s, it gets up where it
// floats, a walking monster again (it sinks; under water it can drown, vr_liquids.qc), never a ragdoll for good.
.float vr_kd_wet_since; // when its tries in a liquid began failing (0: not)
void() VR_Knockdown_Think =
{
    if(self.health <= 0 || self.vr_knockdown != 1)
    {
        return;
    }
    self.think = VR_Knockdown_Think;
    self.nextthink = time + 0.1;
    if(self.vr_kd_stagger)
    {
        // Staggering (the engine's ragdoll, pinned up): on, back on its feet (its AI on), or gone over (down from now).
        float st = ragdollstaggered(self, self.vr_kd_mins, self.vr_kd_maxs);
        // (Never on for long: a loaded game's whose ragdoll is never made again goes down.)
        if(st == 1 && (time - self.vr_kd_since < VR_KD_STAGGER_MOST || self.vr_ag_tug_rag)) // (a tug's: its own end)
        {
            self.nextthink = time + 0.05;
            return;
        }
        if(st == 2)
        {
            if(self.vr_kd_parry) // (a parry's: VR_Parry_Ragdoll, as VR_Parry_Recover ends the pose's)
            {
                self.vr_kd_parry = 0;
                self.vr_parry_stagger_until = 0;
                self.vr_melee_begun = 0;
                self.attack_state = AS_STRAIGHT;
                dprint(sprintf("parry interrupt: %s recovered (a ragdoll's stagger, %.2f s)\n", self.classname,
                    time - self.vr_kd_since));
            }
            VR_Knockdown_Afoot("recovers from its stagger");
            return;
        }
        // A loaded game's (its ragdoll made again as it was drawn, its stagger not): begun again where it stands, no
        // push, its balance as it was (it recovers, or gives way after its steps' time).
        if(st == 3 && ragdollstagger(self, self.vr_kd_dir, 0, self.vr_kd_balance, 0))
        {
            self.vr_kd_since = time;
            if(cvar_hget(cvarh_vr_knockdown_debug))
            {
                dprint(sprintf("knockdown: %s's stagger begun again (a loaded game), balance %.2f\n", self.classname,
                    self.vr_kd_balance));
            }
            self.nextthink = time + 0.05;
            return;
        }
        self.vr_kd_stagger = 0;
        if(self.vr_kd_parry)
        {
            self.vr_kd_parry = 0;
            dprint(sprintf("parry interrupt: %s went down from a ragdoll's stagger after %.2f s\n", self.classname,
                time - self.vr_kd_since));
        }
        float lo = max(0, cvar_hget(cvarh_vr_knockdown_time_min)) * VR_KD_DownScale(self);
        float hi = max(lo, cvar_hget(cvarh_vr_knockdown_time_max) * VR_KD_DownScale(self));
        self.vr_kd_since = time;
        self.vr_kd_until = time + lo + random() * (hi - lo);
        self.vr_kd_due = self.vr_kd_until;
        self.vr_kd_rise = 0;
        if(cvar_hget(cvarh_vr_knockdown_debug))
        {
            dprint(sprintf("knockdown: %s staggered and fell: down %.1f s\n", self.classname, self.vr_kd_until - time));
        }
        return;
    }
    // (A hand on it, dragging or lifting it: kept down while held and vr_knockdown_keep_time after.)
    if(!self.vr_kd_rise && VR_Knockdown_Held(self))
    {
        VR_Knockdown_Keep(self, cvar_hget(cvarh_vr_knockdown_keep_time), "held");
    }
    if(time < self.vr_kd_until)
    {
        return;
    }
    if(!self.vr_kd_rise)
    {
        self.vr_kd_rise = 1;
        if(cvar_hget(cvarh_vr_knockdown_debug))
        {
            dprint(sprintf("knockdown: %s's get-up due after %.2f s down (its own timer %.2f s)\n", self.classname,
                time - self.vr_kd_since, self.vr_kd_due - self.vr_kd_since));
        }
    }
    if(!isragdoll(self))
    {
        // (Its ragdoll gone: ragdolls turned off, a loaded game's not made yet. Up where it is, if it fits.)
        setsize(self, self.vr_kd_mins, self.vr_kd_maxs);
        self.solid = SOLID_SLIDEBOX;
        if(!walkmove(0, 0))
        {
            self.solid = SOLID_NOT_BUT_TOUCHABLE;
            self.vr_kd_until = time + max(0.1, cvar_hget(cvarh_vr_knockdown_retry));
            return;
        }
        VR_Knockdown_Stand(0);
        return;
    }
    float search = max(0, cvar_hget(cvarh_vr_knockdown_search));
    float which = ragdollgetup(self, self.vr_kd_frame, self.vr_kd_frame2, self.vr_kd_mins, self.vr_kd_maxs, search);
    if(which < 0)
    {
        // (Its body rising through its get-up: asked again until it stands. Or held in a liquid by a hand: it stays
        // down, its give-up in the liquid timed from when it is let go of.)
        self.vr_kd_wet_since = 0;
        self.nextthink = time + 0.05;
        return;
    }
    if(which >= 3)
    {
        VR_Knockdown_Afoot("rises through its get-up");
        return;
    }
    float floating = FALSE;
    if(!which)
    {
        float giveup = cvar_hget(cvarh_vr_knockdown_water_giveup);
        if(giveup > 0 && VR_Liquid_Level(self) > 0)
        {
            if(!self.vr_kd_wet_since)
            {
                self.vr_kd_wet_since = time;
            }
            if(time >= self.vr_kd_wet_since + giveup)
            {
                which = ragdollgetup(self, self.vr_kd_frame, self.vr_kd_frame2, self.vr_kd_mins, self.vr_kd_maxs,
                    max(48, search), TRUE);
                floating = which != 0;
            }
        }
        else
        {
            self.vr_kd_wet_since = 0;
            // No room to stand (a slope steeper than vr_knockdown_getup_slope it slides on, a cramped spot): down this
            // long since it went down (vr_knockdown_getup_force), it stands up where it lies, forced (never a ragdoll
            // for ever; ROUND22.md, "Knocked down on a slope").
            float force = cvar_hget(cvarh_vr_knockdown_getup_force);
            if(force > 0 && time >= self.vr_kd_since + force)
            {
                which = ragdollgetup(self, self.vr_kd_frame, self.vr_kd_frame2, self.vr_kd_mins, self.vr_kd_maxs,
                    max(48, search), TRUE);
                if(which && cvar_hget(cvarh_vr_knockdown_debug))
                {
                    dprint(sprintf("knockdown: %s forced up after %.2f s down (no room to stand)\n", self.classname,
                        time - self.vr_kd_since));
                }
            }
        }
        if(!which)
        {
            self.vr_kd_until = time + max(0.1, cvar_hget(cvarh_vr_knockdown_retry));
            return;
        }
    }
    self.vr_kd_wet_since = 0;
    VR_Knockdown_Stand(which);
    if(floating)
    {
        self.flags = self.flags - (self.flags & FL_ONGROUND); // (nothing under it: it sinks)
        if(cvar_hget(cvarh_vr_knockdown_debug))
        {
            dprint(sprintf("knockdown: %s found no floor out of the liquid: up where it floats, at %v\n", self.classname,
                self.origin));
        }
    }
};

// Knocks `e` down now (its ragdoll made, it lies there): FALSE if it could not be.
float(entity e) VR_Knockdown_Start =
{
    if(e.vr_shove != world && e.vr_shove.classname == "vr_shove" && e.vr_shove.owner == e)
    {
        remove(e.vr_shove); // (its slide: the ragdoll is thrown instead)
    }
    e.vr_shove = world;
    e.vr_kd_stagger = 0;
    e.vr_kd_parry = 0;
    e.vr_kd_mins = e.mins;
    e.vr_kd_maxs = e.maxs;
    e.vr_kd_movetype = e.movetype;
    float oldSolid = e.solid;
    e.vr_knockdown = 1;
    e.solid = SOLID_NOT_BUT_TOUCHABLE;
    e.velocity = e.velocity * max(0, cvar_hget(cvarh_vr_knockdown_push));
    e.flags = e.flags - (e.flags & FL_ONGROUND);
    if(!ragdollknockdown(e))
    {
        e.vr_knockdown = 0;
        e.solid = oldSolid;
        return FALSE;
    }
    float lo = max(0, cvar_hget(cvarh_vr_knockdown_time_min)) * VR_KD_DownScale(e);
    float hi = max(lo, cvar_hget(cvarh_vr_knockdown_time_max) * VR_KD_DownScale(e));
    e.vr_kd_since = time;
    e.vr_kd_until = time + lo + random() * (hi - lo);
    e.vr_kd_due = e.vr_kd_until;
    e.vr_kd_rise = 0;
    e.think = VR_Knockdown_Think;
    e.nextthink = time + 0.1;
    return TRUE;
};

// A zombie's own fall (BACKLOG "Zombies stuck knocked down"; ROUND22.md, "Zombies fall as ragdolls"): Quake's,
// Rogue's, Hipnotic's and Dawn of the Machine's zombies (zombie_pain), AD's zombies, mummies and zombie knight
// (VR_AD_ZombiePain). The stock fall lay there not solid, out of reach of shots, and got up only where it fit (it could
// stay down for good, untouchable). Instead `e`, put down by xBy's hit, falls away from xBy as a knockdown's ragdoll
// (room made from the dead only): grabbed, dragged, hit, beheaded or gibbed down there (its own rules: only a hit of 60
// gibs it), up through its get-up after the knockdown's time. FALSE: no ragdoll (ragdolls off, no room): the stock fall.
float VR_KD_ZOMBIE_PUSH = 0.6;  // the topple's push (a two-hand shove's 1)
float VR_KD_ZOMBIE_REACH = 16;  // and its carry, units
float(entity e, entity xBy) VR_Knockdown_ZombieFall =
{
    if(e.health <= 0 || e.vr_knockdown || e.vr_corpse || !e.vr_kd_chance_h || !canragdoll(e))
    {
        return FALSE;
    }
    vr_kd_room_dead_only = TRUE;
    float room = VR_Knockdown_MakeRoom(e);
    vr_kd_room_dead_only = FALSE;
    if(!room || !VR_Knockdown_Start(e))
    {
        return FALSE;
    }
    vector way = e.origin - xBy.origin;
    way_z = 0;
    if(xBy == world || xBy == e || vlen(way) < 1)
    {
        makevectors(e.angles);
        way = '0 0 0' - v_forward;
    }
    ragdollshovetopple(e, normalize(way), VR_KD_ZOMBIE_PUSH, FALSE, VR_KD_ZOMBIE_REACH);
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("knockdown: %s falls as a ragdoll (its own fall): down %.1f s\n", e.classname,
            e.vr_kd_until - time));
    }
    return TRUE;
};

// `e`, standing with its head under water, out of breath (vr_liquids.qc's first drowning tick): by
// vr_ragdoll_active_drown's chance it struggles as a ragdoll (knocked down: the engine's living swim paddles, its strokes
// frantic at first, its head up, towards where it can stand; it gets up there as any knockdown in water). TRUE if it
// does (that tick's damage spared). Only the kinds driven alive (ragdolldrown: the active classes, Swim and Flail on).
float(entity e) VR_Knockdown_Drown =
{
    if(!cvar_hget(cvarh_vr_knockdown) || e.health <= 0 || e.vr_knockdown || !e.vr_kd_chance_h ||
       (e.flags & (FL_FLY | FL_SWIM)) || !(e.flags & FL_MONSTER) || random() >= cvar_hget(cvarh_vr_ragdoll_active_drown) ||
       !ragdolldrown(e, 0))
    {
        return FALSE;
    }
    if(!VR_Knockdown_MakeRoom(e) || !VR_Knockdown_Start(e))
    {
        return FALSE;
    }
    ragdolldrown(e, 1);
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("knockdown: %s drowning: it struggles as a ragdoll, down %.1f s\n", e.classname,
            e.vr_kd_until - time));
    }
    return TRUE;
};

float VR_KD_LEDGE_STEP = 8;   // units between the points along its way where the ground under it is looked for
float vr_kd_ledge_past;        // VR_Knockdown_Ledge's last edge found: units along the shove to past it (its half width on)

// How far the shove that just landed on `e` (its velocity VR_Push's: along the shove, and the hop) carries it standing:
// the hop (2 x up / gravity in the air, at its speed), then the slide (speed^2 / 2 VR_SHOVE_FRICTION; none under
// VR_SHOVE_MINSPEED, as VR_Push), units.
float(entity e) VR_Shove_Reach =
{
    vector dir = e.velocity;
    dir_z = 0;
    float speed = vlen(dir);
    float g = max(1, cvar_hget(cvarh_sv_gravity) * (e.gravity ? e.gravity : 1));
    float travel = speed * 2 * max(0, e.velocity_z) / g;
    if(speed >= VR_SHOVE_MINSPEED)
    {
        travel = travel + speed * speed / (2 * VR_SHOVE_FRICTION);
    }
    return travel;
};

// Ledges (vr_knockdown_ledge; Combat > Knockdowns, "Over a Ledge"): whether the shove that just landed on `e` (its
// velocity VR_Push's: along the shove, and the hop) carries it over the edge of a drop of vr_knockdown_ledge_drop or more.
// - Its reach: the hop (2 x up / gravity in the air, at its speed), then the slide (speed^2 / 2 VR_SHOVE_FRICTION; none
//   under VR_SHOVE_MINSPEED, as VR_Push), times vr_knockdown_ledge_reach (1), plus vr_knockdown_ledge_margin (16).
// - Its hull is traced along the way, a unit off the floor: a wall, a railing or a step up stops it there (no ledge past
//   it). Only the world and brush entities count (lifts, plats, doors); monsters and props do not.
// - Every VR_KD_LEDGE_STEP units the ground under the middle of its feet is looked for (where the slide sends it on over:
//   VR_Shove_Think). A ledge: no ground within the height below the last point's ground (so stairs and slopes, a step at
//   a time, are none), with room past the edge for its whole hull to drop that far (a crack is none).
// - Liquids are not ground: a drop into water, slime or lava is measured to the bottom under it.
// - Its ground may be a lift or a plat (moving or not): the drop is measured from it.
float(entity e) VR_Knockdown_Ledge =
{
    float height = cvar_hget(cvarh_vr_knockdown_ledge_drop);
    vector dir = e.velocity;
    dir_z = 0;
    float speed = vlen(dir);
    if(!cvar_hget(cvarh_vr_knockdown_ledge) || height <= 0 || speed < 1)
    {
        return FALSE;
    }
    dir = dir * (1 / speed);
    float travel = VR_Shove_Reach(e);
    float reach = travel * max(0, cvar_hget(cvarh_vr_knockdown_ledge_reach)) + max(0, cvar_hget(cvarh_vr_knockdown_ledge_margin));
    if(reach < 1)
    {
        return FALSE;
    }
    vector start = e.origin + '0 0 1';
    tracebox(start, e.mins, e.maxs, start + dir * reach, TRUE, e);
    if(trace_startsolid)
    {
        start = e.origin;
        tracebox(start, e.mins, e.maxs, start + dir * reach, TRUE, e);
        if(trace_startsolid)
        {
            return FALSE;
        }
    }
    float clear = trace_fraction * reach;
    float ground = e.origin_z + e.mins_z; // (its feet)
    float half = max(e.maxs_x, e.maxs_y);
    float d = 0;
    while(d < clear)
    {
        d = min(clear, d + VR_KD_LEDGE_STEP);
        vector p = start + dir * d;
        traceline(p, p - '0 0 1' * (p_z - ground + height), TRUE, e);
        if(trace_startsolid)
        {
            break;
        }
        if(trace_fraction < 1)
        {
            ground = trace_endpos_z;
            continue;
        }
        // An edge: room for its hull past it, as far down?
        vector q = p + dir * half;
        tracebox(q, e.mins, e.maxs, q - '0 0 1' * (q_z + e.mins_z - (ground - height)), TRUE, e);
        if(!trace_startsolid && trace_fraction == 1)
        {
            vr_kd_ledge_past = d + half;
            if(cvar_hget(cvarh_vr_knockdown_debug))
            {
                dprint(sprintf("knockdown ledge: %s: an edge %.0f units ahead (reach %.0f, clear %.0f), a drop of %g or more\n",
                    e.classname, d, reach, clear, height));
            }
            return TRUE;
        }
    }
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("knockdown ledge: %s: none (reach %.0f, clear %.0f)\n", e.classname, reach, clear));
    }
    return FALSE;
};

// `e` (its push just landed: VR_Push) staggering as a ragdoll along xForward (vr_ragdoll_active_stagger; the caller asked
// ragdollcanstagger): xPush the push (two hands 1), xBalance 0 about to fall .. 1 steady. Its ragdoll made (room for it
// first); one that cannot stagger topples over. FALSE: no room, no ragdoll (it stands as it was).
// (A shove's, VR_Knockdown_Try; a strong hit's, vr_hitstagger.qc.) Its travel's limit times vr_kd_stagger_far (VR_Bash_Hit's;
// 0: 1).
float(entity e, vector xForward, float xPush, float xBalance) VR_Knockdown_Stagger =
{
    if(!VR_Knockdown_MakeRoom(e))
    {
        return FALSE;
    }
    float slide = VR_Shove_Reach(e); // (before its launch is cut: the shove's own)
    if(!VR_Knockdown_Start(e))
    {
        return FALSE;
    }
    if(ragdollstagger(e, xForward, xPush, xBalance, slide, 0, 0, max(1, vr_kd_stagger_far)))
    {
        e.vr_kd_stagger = 1;
        e.vr_kd_balance = xBalance;
        e.vr_kd_dir = xForward;
        e.nextthink = time + 0.05;
        return TRUE;
    }
    ragdollshovetopple(e, xForward, xPush, FALSE, slide);
    return TRUE;
};

// A shove (or a bash) by self (a player) that landed on `e` (VR_Bash_Hit): by chance it knocks it down. xKind: the hit's
// (QVR_HITKIND_SHOVE or _BASH); xHand -1: both hands; xPush: its push (VR_Shove_HandsPush: two hands 1, one 0.7, a counter more,
// tired less): down, its ragdoll is carried on along xForward and topples over its feet (ragdollshovetopple:
// vr_knockdown_shove_*).
float(entity e, vector xForward, float xKind, float xHand, float xPush) VR_Knockdown_Try =
{
    if(!cvar_hget(cvarh_vr_knockdown) || !(self.flags & FL_CLIENT) || e.health <= 0 || e.vr_knockdown || !e.vr_kd_chance_h ||
       (e.flags & (FL_FLY | FL_SWIM)) || !(e.flags & FL_MONSTER) || !canragdoll(e) ||
       (VR_IsDummy(e) && cvar_hget(cvarh_vr_dummy_anchored)))
    {
        return FALSE;
    }
    VR_Manhandle_Shaken(e); // (a hand's hold may knock it down a moment: vr_foegrab_manhandle_stagger)
    // (Its monster group's: vr_knockdown_chance_<group>; a heavy group's times its poise factor, combat.qc VR_Poise_Factor.)
    float base = monstergroupget(e, MGP_SHOVE) * VR_Poise_Factor(e);
    float chance = base * max(0, cvar_hget(cvarh_vr_knockdown_chance));
    float full = e.max_health > 0 ? e.max_health : e.health;
    float lost = full > 0 ? min(1, max(0, 1 - e.health / full)) : 0;
    chance = chance * (1 + max(0, cvar_hget(cvarh_vr_knockdown_damage)) * lost);
    float hands = xHand == -1 ? cvar_hget(cvarh_vr_knockdown_twohand) : cvar_hget(cvarh_vr_knockdown_onehand);
    chance = chance * max(0, hands);
    if(xKind != QVR_HITKIND_SHOVE)
    {
        chance = chance * max(0, cvar_hget(cvarh_vr_knockdown_bash));
    }
    float stamina = 1;
    if(VR_Stamina_On())
    {
        float most = VR_Stamina_Max();
        float left = most > 0 ? VR_Stamina_Left(self) / most : 1;
        stamina = max(0, 1 - max(0, cvar_hget(cvarh_vr_knockdown_stamina)) * (1 - left));
    }
    chance = chance * stamina * VR_KD_Scale(e); // (its group's Knockdown Chance: the Mini Bosses' half)
    // Shoved over a ledge (VR_Knockdown_Ledge): down for sure, if this shove could knock it down at all (a tough
    // group's: by its Knockdown Chance).
    float ledge = chance > 0 && VR_Knockdown_Ledge(e) && VR_KD_SureRoll(e);
    // Unaware of the shover or shoved from behind (vr_knockdown_surprise; VR_Bash_Hit took it): down for sure (a stagger
    // that gives way after its steps), if this shove could knock it down at all (likewise).
    float surprise = (chance > 0 && VR_KD_SureRoll(e)) ? vr_kd_surprise : 0;
    float roll = random();
    // A stagger (vr_ragdoll_active_stagger; not over a ledge): every shove that could knock it down makes it stagger as a
    // ragdoll, its balance by the roll against the chance (below it: likely to go over), the shove's push on it; it falls
    // (down, as below) or recovers (VR_Knockdown_Think: back on its feet, fighting).
    if(chance > 0 && !ledge && ragdollcanstagger(e))
    {
        float balance = surprise ? 0 : min(1, max(0, 0.5 + (roll - chance) * VR_KD_STAGGER_SLOPE));
        if(cvar_hget(cvarh_vr_knockdown_debug))
        {
            dprint(sprintf("knockdown: %s %s (%s): base %.2f, health %g of %g, stamina x%.2f",
                xKind == QVR_HITKIND_SHOVE ? "shove" : "bash", e.classname, xHand == -1 ? "two hands" : "one hand", base,
                e.health, full, stamina));
            dprint(sprintf(": chance %.2f, roll %.2f: a stagger, balance %.2f%s\n", chance, roll, balance,
                surprise == 1 ? " (unaware: down)" : surprise == 2 ? " (from behind: down)" : ""));
        }
        return VR_Knockdown_Stagger(e, xForward, xPush, balance);
    }
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("knockdown: %s %s (%s): base %.2f, health %g of %g, stamina x%.2f",
            xKind == QVR_HITKIND_SHOVE ? "shove" : "bash", e.classname, xHand == -1 ? "two hands" : "one hand", base, e.health,
            full, stamina));
        dprint(sprintf(": chance %.2f%s, roll %.2f%s\n", chance,
            ledge ? " (over a ledge: down)" : surprise ? " (surprised: down)" : "", roll,
            (ledge || surprise || roll < chance) ? ": down" : ""));
    }
    if(roll >= chance && !ledge && !surprise)
    {
        return FALSE;
    }
    if(!VR_Knockdown_MakeRoom(e))
    {
        return FALSE;
    }
    float reach = ledge ? vr_kd_ledge_past : VR_Shove_Reach(e); // (before its launch is cut: the shove's own)
    if(!VR_Knockdown_Start(e))
    {
        return FALSE;
    }
    // Carried on as far as the shove would carry it standing (times vr_knockdown_shove_travel) and turned over about its
    // feet a quarter turn (vr_knockdown_shove_topple_*); over a ledge, pushed whole so it goes off, turned as it falls
    // (vr_knockdown_shove_ledge_*).
    ragdollshovetopple(e, xForward, xPush, ledge, reach);
    return TRUE;
};

// For tests (the engine's vr_knockdown_test <mode>), self the player: 0 knocks the nearest monster that can be down
// (regardless of the chance; pushed away from you), 1 gets every knocked-down one up now (their next try), 2 hits the
// nearest knocked-down one for 5 (its pain), 3 kills it (a hit for its health), 4 gibs it, 5 lists them, 20 removes every
// monster, standing, knocked down or dead (the ledge tests' clean slate between trials), 21 and 22 shove the nearest with
// one hand and with two (its chance as a real shove's: vr_knockdown_chance 100 for sure; its topple traced with
// vr_knockdown_debug).
// vr_knockdown_test 10 (Debug > Combat, "Why Can't I Knock It Down?"): why a shove, a throw or a lift of the nearest
// living monster `m` makes no ragdoll now: each gate VR_Knockdown_Try and VR_FoeGrab_Lift test, and the ragdoll pool as
// VR_Knockdown_MakeRoom counts it (the ragdolls it can free and those it cannot), side-effect free.
void(entity m) VR_Knockdown_WhyNot =
{
    float most = cvar("vr_ragdoll_max");
    dprint(sprintf("whynot: %s health %g, vr_knockdown %g, chance handle %g, flags fly/swim %g", m.classname, m.health,
        m.vr_knockdown, m.vr_kd_chance_h, m.flags & (FL_FLY | FL_SWIM))); // (sprintf: 8 arguments at most)
    dprint(sprintf(", monster group %g's chance %g x vr_knockdown_chance %g\n", monstergroup(m),
        monstergroupget(m, MGP_SHOVE), cvar_hget(cvarh_vr_knockdown_chance)));
    dprint(sprintf("whynot: vr_knockdown %g, canragdoll %g, ragdollcanstagger %g, isragdoll %g, dummy anchored %g, stamina %g of %g\n",
        cvar_hget(cvarh_vr_knockdown), canragdoll(m), ragdollcanstagger(m), isragdoll(m),
        VR_IsDummy(m) && cvar_hget(cvarh_vr_dummy_anchored), VR_Stamina_On() ? VR_Stamina_Left(self) : -1, VR_Stamina_Max()));
    float count = 0;
    float freeable = 0;
    for(entity x = nextent(world); x; x = nextent(x))
    {
        if(x == m || !isragdoll(x) || x.vr_corpse_placed == 1)
        {
            continue;
        }
        count = count + 1;
        string why;
        if(x.health <= 0)
        {
            if(VR_DeathDoll_InUse(x))
            {
                why = "kept: a dead player's body";
            }
            else if(x.vr_corpse_placed && VR_PlacedCorpse_Near(x))
            {
                why = "kept: a placed corpse near you";
            }
            else
            {
                why = "dead: can go";
                freeable = freeable + 1;
            }
        }
        else if(x.vr_knockdown == 1)
        {
            why = "knocked down: can be killed for room";
            freeable = freeable + 1;
        }
        else
        {
            why = "ALIVE, not knocked down: never freed";
        }
        dprint(sprintf("whynot:   ragdoll %s at %v health %g, vr_knockdown %g: %s\n", x.classname, x.origin, x.health,
            x.vr_knockdown, why));
    }
    dprint(sprintf("whynot: ragdolls %g of vr_ragdoll_max %g, %g can be freed: %s\n", count, most, freeable,
        (most >= 1 && (count < most || freeable > 0)) ? "room" : "NO ROOM (no shove, throw or lift makes a ragdoll)"));
};

void() VR_Buzzsaw_Test; // (rogue_buzzsaw.qc)
