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
//   blood; no pain frames: T_DamageImpl keeps its frame and think), each keeping it down vr_knockdown_hit_time longer.
//   It is grabbed, pushed and thrown as a dead one's. Killed there, it screams and its death code runs at once (its
//   drops; Killed: VR_Knockdown_FastForwardDeath): the ragdoll stays as it lies. Gibbed, it gibs; beheaded or its head
//   blown off, the ragdoll goes on headless. Thrown or dropped down there, it takes fall damage as a standing monster does
//   (combat.qc VR_Monster_Fall, from its ragdoll's hardest landing on the level: vr_box3d.cpp callFalls).
// - Room: when the ragdolls are at vr_ragdoll_max, the oldest dead one goes (removed), else the oldest knocked-down one
//   is killed and removed (VR_Knockdown_MakeRoom); no room at all: no knockdown.
// - Getting up, after vr_knockdown_time_min to _max seconds: the engine fits its get-up's first frame to how the body
//   lies and looks for room to stand there and around it (vr_knockdown_search: never through a wall, a floor or a
//   ceiling; none: it stays down, trying again every vr_knockdown_retry seconds), the hands holding it let go, and the
//   body blends from the ragdoll into the get-up (vr_knockdown_blend). The get-up: its own fall-and-rise frames from
//   their lowest (the grunt's painb, the enforcer's paind, the zombie's and mummy's paine, the ogre's paind, the
//   rottweiler's painb, the knight's painb), at vr_knockdown_getup_speed; a monster with none (the death knight, the
//   fiend, the shambler, the gremlin) plays a death backwards (of two, the one whose end fits how it lies). Then it runs.

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
    return 0.1 / min(10, max(0.1, cvar_hget(cvarh_vr_knockdown_getup_speed)));
};

// Its body gone (a dead one's, or one killed to make room): as a corpse is removed.
void(entity e) VR_Knockdown_RemoveBody =
{
    VR_Head_StopFlies(e);
    remove(e);
};

// Room for one more ragdoll (vr_ragdoll_max): the oldest dead ragdoll goes first, else the oldest knocked-down monster
// (but `xFor`) is killed and goes. FALSE: none can.
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
            if(x != xFor && isragdoll(x))
            {
                count = count + 1;
                if(x.health <= 0)
                {
                    // (A dead player's own body stays while he is dead: his death view is on it, vr_deathdoll.qc.)
                    if(!VR_DeathDoll_InUse(x) && (dead == world || x.vr_corpse_since < dead.vr_corpse_since))
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
                dprint(sprintf("knockdown: ragdolls at %g: a dead %s's body goes\n", most, dead.classname));
            }
            VR_Knockdown_RemoveBody(dead);
        }
        else if(alive != world)
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
    if(self.think == self.th_run || self.think == f)
    {
        self.vr_knockdown = 0; // (its run goes on at its own pace)
        return;
    }
    self.vr_kd_next = self.think;
    self.think = VR_Knockdown_Step;
    self.nextthink = time + VR_Knockdown_Interval();
};

// Up from the ragdoll (self): solid again, standing, its get-up playing. xWhich: the frame the engine chose (1 or 2;
// 0: no ragdoll to blend from, up where it is).
void(float xWhich) VR_Knockdown_Stand =
{
    self.vr_knockdown = 2;
    self.solid = SOLID_SLIDEBOX;
    self.movetype = self.vr_kd_movetype;
    setsize(self, self.vr_kd_mins, self.vr_kd_maxs);
    setorigin(self, self.origin);
    self.velocity = '0 0 0';
    self.flags = self.flags | FL_ONGROUND;
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
void() VR_Knockdown_Think =
{
    if(self.health <= 0 || self.vr_knockdown != 1)
    {
        return;
    }
    self.think = VR_Knockdown_Think;
    self.nextthink = time + 0.1;
    if(time < self.vr_kd_until)
    {
        return;
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
    float which = ragdollgetup(self, self.vr_kd_frame, self.vr_kd_frame2, self.vr_kd_mins, self.vr_kd_maxs,
        max(0, cvar_hget(cvarh_vr_knockdown_search)));
    if(!which)
    {
        self.vr_kd_until = time + max(0.1, cvar_hget(cvarh_vr_knockdown_retry));
        return;
    }
    VR_Knockdown_Stand(which);
};

// Knocks `e` down now (its ragdoll made, it lies there): FALSE if it could not be.
float(entity e) VR_Knockdown_Start =
{
    if(e.vr_shove != world && e.vr_shove.classname == "vr_shove" && e.vr_shove.owner == e)
    {
        remove(e.vr_shove); // (its slide: the ragdoll is thrown instead)
    }
    e.vr_shove = world;
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
    float lo = max(0, cvar_hget(cvarh_vr_knockdown_time_min));
    float hi = max(lo, cvar_hget(cvarh_vr_knockdown_time_max));
    e.vr_kd_since = time;
    e.vr_kd_until = time + lo + random() * (hi - lo);
    e.think = VR_Knockdown_Think;
    e.nextthink = time + 0.1;
    return TRUE;
};

float VR_KD_LEDGE_STEP = 8;    // units between the points along its way where the ground under it is looked for
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

// A shove (or a bash) by self (a player) that landed on `e` (VR_Bash_Hit): by chance it knocks it down. xKind: the hit's
// (QVR_HITKIND_SHOVE or _BASH); xHand -1: both hands; xPush: its push (VR_Shove_HandsPush: two hands 1, one 0.7, a counter more,
// tired less): down, its ragdoll is carried on along xForward and topples over its feet (ragdollshovetopple:
// vr_knockdown_shove_*).
float(entity e, vector xForward, float xKind, float xHand, float xPush) VR_Knockdown_Try =
{
    if(!cvar_hget(cvarh_vr_knockdown) || !(self.flags & FL_CLIENT) || e.health <= 0 || e.vr_knockdown || !e.vr_kd_chance_h ||
       (e.flags & (FL_FLY | FL_SWIM)) || !(e.flags & FL_MONSTER) || !canragdoll(e))
    {
        return FALSE;
    }
    float base = cvar_hget(e.vr_kd_chance_h);
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
    chance = chance * stamina;
    // Shoved over a ledge (VR_Knockdown_Ledge): down for sure, if this shove could knock it down at all.
    float ledge = chance > 0 && VR_Knockdown_Ledge(e);
    float roll = random();
    if(cvar_hget(cvarh_vr_knockdown_debug))
    {
        dprint(sprintf("knockdown: %s %s (%s): base %.2f, health %g of %g, stamina x%.2f",
            xKind == QVR_HITKIND_SHOVE ? "shove" : "bash", e.classname, xHand == -1 ? "two hands" : "one hand", base, e.health,
            full, stamina));
        dprint(sprintf(": chance %.2f%s, roll %.2f%s\n", chance, ledge ? " (over a ledge: down)" : "", roll,
            (ledge || roll < chance) ? ": down" : ""));
    }
    if(roll >= chance && !ledge)
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
void(float xMode) VR_Knockdown_Test =
{
    if(xMode == 20)
    {
        float removed = 0;
        entity next = world;
        for(entity mon = nextent(world); mon; mon = next)
        {
            next = nextent(mon);
            if(mon.flags & FL_MONSTER)
            {
                if(mon.vr_shove != world && mon.vr_shove.classname == "vr_shove" && mon.vr_shove.owner == mon)
                {
                    remove(mon.vr_shove);
                }
                VR_Knockdown_RemoveBody(mon);
                removed = removed + 1;
            }
        }
        dprint(sprintf("knockdown test: %g monsters removed\n", removed));
        return;
    }
    // Feedback regression probes: 6 lists bodies and their breathing point;
    // 7 moves a living knockdown to the player (underwater test); 8 fires a
    // real nail at the nearest living monster at one health.
    if(xMode >= 6 && xMode <= 8)
    {
        entity m = VR_Decap_TestTarget(TRUE);
        if(xMode == 7)
        {
            m = world;
            float nearest = 999999;
            for(entity down = nextent(world); down; down = nextent(down))
            {
                float distance = vlen(down.origin - self.origin);
                if(down.vr_knockdown == 1 && down.health > 0 && distance < nearest)
                {
                    m = down;
                    nearest = distance;
                }
            }
        }
        if(xMode == 7 && m != world)
        {
            setorigin(m, self.origin);
            m.velocity = '0 0 0';
            m.vr_kd_until = time + 60;
        }
        if(xMode == 8 && m != world)
        {
            m.health = 1;
            VR_Test_Fire(1, (m.absmin + m.absmax) * 0.5);
        }
        for(entity body = nextent(world); body; body = nextent(body))
        {
            if((body.flags & FL_MONSTER) && !body.vr_gib)
            {
                float level = VR_Liquid_Level(body);
                dprint(sprintf("bodycheck: %s health=%g ragdoll=%g headless=%g knocked=%g water=%g type=%g\n",
                    body.classname, body.health, isragdoll(body), body.vr_headless, body.vr_knockdown,
                    level, vr_liquid_type));
                dprint(sprintf("breathcheck: %s breath=%g at=%v\n", body.classname, body.vr_liquid_air - time, body.origin));
            }
        }
        return;
    }
    entity best = world;
    float bestDist = 999999;
    entity x = nextent(world);
    while(x)
    {
        if((x.flags & FL_MONSTER) && x.health > 0)
        {
            float d = vlen(x.origin - self.origin);
            if(xMode == 5)
            {
                dprint(sprintf("knockdown: %s: state %g, health %g, ragdoll %g, at %v\n", x.classname,
                    x.vr_knockdown, x.health, isragdoll(x), x.origin));
            }
            else if(xMode == 1)
            {
                if(x.vr_knockdown == 1)
                {
                    x.vr_kd_until = time;
                }
            }
            else if(d < bestDist && ((xMode == 0 || xMode == 21 || xMode == 22) ? (!x.vr_knockdown && x.vr_kd_chance_h) :
                                               x.vr_knockdown == 1))
            {
                best = x;
                bestDist = d;
            }
        }
        x = nextent(x);
    }
    if(xMode == 1 || xMode == 5)
    {
        return;
    }
    if(best == world)
    {
        dprint("knockdown test: no monster\n");
        return;
    }
    if(xMode == 21 || xMode == 22)
    {
        // 21, 22: a one- or two-handed open-palm shove of the nearest (VR_Bash_Hit, as a real one: its push, its stagger,
        // its chance; vr_knockdown_chance 100 for a sure knockdown), fresh (not tired).
        vector away = best.origin - self.origin;
        away_z = 0;
        vr_bash_exhaust_dmg = 0;
        vr_bash_exhaust_push = 0;
        float two = xMode == 22;
        dprint(sprintf("knockdown test: %s shove of %s\n", two ? "two-handed" : "one-handed", best.classname));
        VR_Bash_Hit(best, best.origin, normalize(away), QVR_HITKIND_SHOVE, two ? -1 : cVR_MainHand,
            two ? 1 : VR_BASH_ONE_DAMAGE, VR_Shove_HandsPush(two));
        return;
    }
    if(xMode == 0)
    {
        if(!(canragdoll(best)) || !VR_Knockdown_MakeRoom(best))
        {
            dprint(sprintf("knockdown test: %s cannot be knocked down\n", best.classname));
            return;
        }
        vector dir = normalize(best.origin - self.origin);
        dir_z = 0;
        best.velocity = normalize(dir) * 220 + '0 0 120';
        dprint(sprintf("knockdown test: %s down: %g\n", best.classname, VR_Knockdown_Start(best)));
        return;
    }
    float dmg = xMode == 2 ? 5 : (xMode == 3 ? best.health : best.health + 200);
    dprint(sprintf("knockdown test: %s hit for %g\n", best.classname, dmg));
    T_Damage(best, self, self, dmg);
};
