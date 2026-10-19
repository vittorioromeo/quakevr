// Living hit reactions (ROUND22.md, "Active ragdolls: living hit reactions"; the engine's vr_hitreact.cpp): a living
// monster struck flinches where it was struck, and stumbles a little, its AI going on as it would. Here, the server's
// side: each blow sent to the clients (hitreact: the point struck, the push, the part, the damage and its share of the
// monster's health), which draw the flinch (vr_hit_react: its bones kicked round their joints, or the whole model
// tilted), and a short slow-down of its steps (vr_hit_react_slow): ai_run's and ai_walk's distance only
// (VR_HitReact_Dist), a leg hit's a limp with a hitch to that side. No pain, no state, no decision of its own changed;
// its damage and hitboxes are as before.
//
// Off (vr_hit_react 0 on the server): nothing sent, no slow-down.

.float vr_hr_start;  // when its slow-down began (time)
.float vr_hr_until;  // and ends
.float vr_hr_slow;   // its steps' scale at the start (easing back to 1 at vr_hr_until)
.vector vr_hr_hitch; // a leg hit's hitch: a step this way (units), taken at its next ai_run or ai_walk ('0 0 0': none)

// A shotgun blast's pellets on one target this frame (TraceAttack): their points, summed, and their directions.
entity vr_hr_pel_ent;
float vr_hr_pel_time;
float vr_hr_pel_count;
vector vr_hr_pel_sum;
vector vr_hr_pel_dir;

void(entity e, vector at, vector dir) VR_HitReact_Pellet =
{
    if(vr_hr_pel_ent != e || vr_hr_pel_time != time)
    {
        vr_hr_pel_ent = e;
        vr_hr_pel_time = time;
        vr_hr_pel_count = 0;
        vr_hr_pel_sum = '0 0 0';
        vr_hr_pel_dir = '0 0 0';
    }
    vr_hr_pel_count = vr_hr_pel_count + 1;
    vr_hr_pel_sum = vr_hr_pel_sum + at;
    vr_hr_pel_dir = vr_hr_pel_dir + dir;
};

// Its steps' scale now (1: not slowed).
float(entity m) VR_HitReact_Scale =
{
    if(m.vr_hr_until <= time || m.vr_hr_until <= m.vr_hr_start)
    {
        return 1;
    }
    float f = (time - m.vr_hr_start) / (m.vr_hr_until - m.vr_hr_start);
    return m.vr_hr_slow + (1 - m.vr_hr_slow) * f;
};

// ai_run's and ai_walk's distance (self the monster): shorter a moment after a blow; a leg hit's hitch taken first.
float(float dist) VR_HitReact_Dist =
{
    if(self.vr_hr_until <= time)
    {
        self.vr_hr_hitch = '0 0 0'; // (its window over: a hitch not taken in it is stale)
        return dist;
    }
    if(self.vr_hr_hitch != '0 0 0')
    {
        vector hitch = self.vr_hr_hitch;
        self.vr_hr_hitch = '0 0 0';
        float moved = walkmove(vectoyaw(hitch), vlen(hitch));
        if(cvar_hget(cvarh_vr_hit_react_debug))
        {
            dprint(sprintf("hitslow: %s hitch %.1f units %s\n", self.classname, vlen(hitch), moved ? "taken" : "blocked"));
        }
    }
    float scale = VR_HitReact_Scale(self);
    if(cvar_hget(cvarh_vr_hit_react_debug))
    {
        dprint(sprintf("hitslow: %s step %.2f x %.2f\n", self.classname, dist, scale));
    }
    return dist * scale;
};

// (VR_HitReact_Where's answer: below.)
vector vr_hr_at;
vector vr_hr_dir;
float vr_hr_part;
void(entity targ, vector at, vector dir, float part, float share) VR_HitReact_Slow;
void(entity targ, entity inflictor, entity attacker) VR_HitReact_Where;

// A living monster struck by `take` (T_DamageDeal, after the blow, before its pain): the blow sent, its steps slowed.
void(entity targ, entity inflictor, entity attacker, float take) VR_HitReact_Hit =
{
    // (none while knocked down, staggering or getting up: a ragdoll then, vr_knockdown.qc)
    if(take <= 0 || !(targ.flags & FL_MONSTER) || targ.health <= 0 || targ.vr_knockdown || !cvar_hget(cvarh_vr_hit_react))
    {
        return;
    }
    // (no flinch for a slow bleed, a burn's ticks, a liquid's)
    if(vr_hitkind == QVR_HITKIND_BLEED || inflictor.classname == "vr_burn" || inflictor == world)
    {
        return;
    }
    VR_HitReact_Where(targ, inflictor, attacker);
    vector at = vr_hr_at;
    vector dir = vr_hr_dir;
    float part = vr_hr_part;
    float share = take / max(targ.max_health, 1);
    share = min(max(share, 0), 1);
    hitreact(targ, at, dir, part, min(255, take), share);
    VR_HitReact_Slow(targ, at, dir, part, share);
};

// The slow-down after a blow: the stronger of what is left of the last one and this one's.
void(entity targ, vector at, vector dir, float part, float share) VR_HitReact_Slow =
{
    // (an older leg hit's hitch not taken yet (pain frames ran through its window) goes: a leg hit below sets its own)
    targ.vr_hr_hitch = '0 0 0';
    float s = min(1, 1.4 * sqrt(share)) * cvar_hget(cvarh_vr_hit_react_slow);
    if(s <= 0)
    {
        return;
    }
    float slow = 1 - 0.35 * s;
    float dur = 0.3;
    if(part == 3)
    {
        slow = 1 - 0.6 * s;
        dur = 0.5;
        // the leg gives: a hitch towards its side of the shot's line
        vector side = at - targ.origin;
        vector flat = dir;
        flat_z = 0;
        side_z = 0;
        if(flat != '0 0 0')
        {
            flat = normalize(flat);
            side = side - flat * (side * flat);
        }
        if(side != '0 0 0')
        {
            targ.vr_hr_hitch = normalize(side) * (4 * s) + flat * (2 * s);
        }
    }
    else if(part == 2)
    {
        slow = 1 - 0.15 * s;
        dur = 0.25;
    }
    float now = VR_HitReact_Scale(targ);
    targ.vr_hr_slow = min(now, slow);
    targ.vr_hr_start = time;
    targ.vr_hr_until = time + dur;
    if(cvar_hget(cvarh_vr_hit_react_debug))
    {
        dprint(sprintf("hitslow: %s part %g share %.2f: steps x %.2f for %.2f s\n", targ.classname, part, share,
            targ.vr_hr_slow, dur));
    }
};

// Where a blow struck `targ` and which way it went (vr_hr_at, vr_hr_dir: unit) and its part (vr_hr_part: 0 the body,
// 1 the head, 2 a limb, 3 a leg, -1 unknown), from the hit's globals (its pellets, its kind, the point PositionalDamage
// found) or the inflictor's and attacker's places. (The small reaction's and the situational knockdowns': vr_kdtrig.qc.)
void(entity targ, entity inflictor, entity attacker) VR_HitReact_Where =
{
    vector mid = (targ.absmin + targ.absmax) * 0.5;
    vector at = '0 0 0';
    vector dir = vr_hitdir;
    float part = -1;
    if(vr_hitpart_target == targ && vr_hitpart_time == time)
    {
        part = vr_hitpart;
        at = vr_hitpart_pos;
    }
    if(vr_hitpellets > 0)
    {
        part = 0;
        if(vr_hitpellets_head * 2 >= vr_hitpellets)
        {
            part = 1;
        }
        else if(vr_hitpellets_leg * 2 > vr_hitpellets)
        {
            part = 3;
        }
        else if(vr_hitpellets_limb * 2 > vr_hitpellets)
        {
            part = 2;
        }
        if(vr_hr_pel_ent == targ && vr_hr_pel_time == time && vr_hr_pel_count > 0)
        {
            at = vr_hr_pel_sum * (1 / vr_hr_pel_count);
            dir = vr_hr_pel_dir;
        }
    }
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        // A blast: from the nearest point of its box to the blast, pushed away from it
        at = inflictor.origin;
        at_x = min(max(at_x, targ.absmin_x), targ.absmax_x);
        at_y = min(max(at_y, targ.absmin_y), targ.absmax_y);
        at_z = min(max(at_z, targ.absmin_z), targ.absmax_z);
        dir = mid - inflictor.origin;
        part = -1;
    }
    if(at == '0 0 0' && vr_hitpos != '0 0 0' && vlen(vr_hitpos - mid) < 96)
    {
        at = vr_hitpos;
    }
    if(at == '0 0 0' && inflictor != targ && !(inflictor.flags & (FL_MONSTER | FL_CLIENT)) && vlen(inflictor.origin - mid) < 96)
    {
        at = inflictor.origin; // a missile where it touched
    }
    if(at == '0 0 0')
    {
        at = mid;
    }
    if(dir == '0 0 0' && inflictor != targ)
    {
        if(inflictor.velocity != '0 0 0' && !(inflictor.flags & (FL_MONSTER | FL_CLIENT)))
        {
            dir = inflictor.velocity; // a missile's flight
        }
        else
        {
            dir = mid - (inflictor.absmin + inflictor.absmax) * 0.5;
        }
    }
    if(dir == '0 0 0')
    {
        dir = mid - (attacker.absmin + attacker.absmax) * 0.5;
    }
    vr_hr_at = at;
    vr_hr_dir = normalize(dir);
    vr_hr_part = part;
};

// Test aid `vr_test_hit <n>` (console; Misc/quakevr/ragdoll/hitreact_test.sh): the live monster nearest the player
// struck by a shot of his (vr_test_hit_damage, never a kill: left 1 health at least), struck (n - 1) on: 0 the body,
// 1 the head, 2 an arm (its right side as he sees it), 3 a leg, on its side facing him. Prints `test hit:`.
// ... and the monster it struck watched each frame for vr_test_hit_watch seconds after (`hitwatch:` lines: its frame,
// place, health, enemy and attack state; the tests compare them with the reactions on and off).
entity vr_hr_watch;
float vr_hr_watch_until;

void(entity pl, entity best, float part) VR_TestHit_One;

// ----------------------------------------------------------------------------
// Guarding the head in melee (vr_ragdoll_active_guard; ROUND22.md, "Guarding the head"): a player's hand swung at a
// living monster's head (fast enough to strike: vr_melee_speed, whatever it holds: a sword, an axe, a fist, a gun's
// bash), coming at it within the next fifth of a second, makes it (by the chance, once a swing) throw its arms up before
// its face a moment: standing, drawn by every client as a hit reaction's guard (hitreact's part 9, its hold in
// hundredths of a second: vr_hitreact.cpp); a ragdoll (a stagger, knocked down, getting up), its body's arms
// (ragdollguard: vr_ragactive.cpp). Shots never: only hands. Its looks alone: the blow lands where it lands.
// ----------------------------------------------------------------------------

.float vr_guard_next;           // when it may guard again (a swing's guard decided: once a swing)
float VR_GUARD_NEAR = 22;       // units: the hand's way passing this near its head
float VR_GUARD_AHEAD = 0.2;     // s: the hand's way looked at this far ahead
float VR_GUARD_HOLD = 0.6;      // s: its arms up this long (then down over a quarter of a second)
float VR_GUARD_PART = 9;        // hitreact's part for a guard
float VR_GUARD_UPM = 32;        // units a metre (the hand's speed is m/s)

// Its head (standing: its eyes' height; a ragdoll: its head's middle).
vector(entity m) VR_Guard_Head =
{
    if(isragdoll(m))
    {
        return ragdollcut(m, 6);
    }
    return m.origin + '0 0 1' * (m.view_ofs_z > 0 ? m.view_ofs_z : m.maxs_z * 0.8);
};

// `m` guards against a blow coming from `at` along `dir`.
void(entity m, vector at, vector dir) VR_Guard_Raise =
{
    if(isragdoll(m))
    {
        if(!ragdollguard(m, at, VR_GUARD_HOLD))
        {
            return;
        }
    }
    else
    {
        hitreact(m, at, dir, VR_GUARD_PART, VR_GUARD_HOLD * 100, 1);
    }
    if(cvar_hget(cvarh_vr_hit_react_debug))
    {
        dprint(sprintf("guard: %s %s guards its head (a hand at %v)\n", m.classname, isragdoll(m) ? "(a ragdoll)" : "standing",
            at));
    }
};

// Whether `m` knows of player `pl` (its enemy is the player: alerted, the stealth AI's found) and has the player in front
// of it (within VR_GUARD_FOV degrees of its facing): an unaware monster, or one facing away, does not guard.
float VR_GUARD_FOV = 105;
float(entity m, entity pl) VR_Guard_Aware =
{
    if(m.enemy != pl)
    {
        return FALSE;
    }
    vector d = pl.origin - m.origin;
    d_z = 0;
    if(vlen(d) < 1)
    {
        return TRUE;
    }
    vector f = m.angles;
    f_x = 0;
    makevectors(f);
    return normalize(d) * v_forward >= cos(VR_GUARD_FOV * 0.017453292);
};

// Player `pl`'s hand `xHand` (self: pl): swung at a head?
void(entity pl, float xHand) VR_Guard_Hand =
{
    vector hv = VRGetHandVel(xHand);
    float speed = vlen(hv);
    if(speed < VR_Melee_MinSpeed() || VR_Melee_Thing(pl, xHand) == VR_MTHING_NONE)
    {
        return;
    }
    vector hp = VRGetHandPos(xHand);
    vector ahead = hp + hv * (VR_GUARD_UPM * VR_GUARD_AHEAD);
    float chance = cvar_hget(cvarh_vr_ragdoll_active_guard);
    entity m = findradius(hp, VR_GUARD_NEAR + speed * VR_GUARD_UPM * VR_GUARD_AHEAD + 48);
    while(m)
    {
        if((m.flags & FL_MONSTER) && m.health > 0 && time >= m.vr_guard_next && !VR_IsDummy(m) && canragdoll(m) && VR_Guard_Aware(m, pl))
        {
            vector head = VR_Guard_Head(m);
            // (Its way's nearest point to the head, and the hand coming towards it.)
            vector seg = ahead - hp;
            float len2 = seg * seg;
            float f = len2 > 0.01 ? min(1, max(0, ((head - hp) * seg) / len2)) : 0;
            if(vlen(head - (hp + seg * f)) < VR_GUARD_NEAR && (head - hp) * hv > 0)
            {
                m.vr_guard_next = time + VR_GUARD_HOLD;
                if(random() < chance)
                {
                    VR_Guard_Raise(m, hp, normalize(hv));
                }
            }
        }
        m = m.chain;
    }
};

// StartFrame: each player's hands (vr_test_guard <n>: the live monster nearest the first player guards as from a swing,
// n 1 standing or as it is; Debug > Tests).
void() VR_Guard_Frame =
{
    float want = cvar("vr_test_guard");
    if(want >= 1)
    {
        cvar_set("vr_test_guard", "0");
        entity pl1 = find(world, classname, "player");
        entity best = world;
        float bestD = 100000;
        entity e = nextent(world);
        while(e)
        {
            if((e.flags & FL_MONSTER) && e.health > 0 && !VR_IsDummy(e) && !VR_TestSkips(e) && vlen(e.origin - pl1.origin) < bestD)
            {
                bestD = vlen(e.origin - pl1.origin);
                best = e;
            }
            e = nextent(e);
        }
        if(best != world)
        {
            VR_Guard_Raise(best, pl1.origin + pl1.view_ofs, normalize(VR_Guard_Head(best) - pl1.origin - pl1.view_ofs));
        }
    }
    if(cvar_hget(cvarh_vr_ragdoll_active_guard) <= 0)
    {
        return;
    }
    entity oldSelf = self;
    entity pl = find(world, classname, "player");
    while(pl)
    {
        if(pl.health > 0 && VRHandsTracked(pl))
        {
            self = pl;
            VR_Guard_Hand(pl, cVR_MainHand);
            VR_Guard_Hand(pl, cVR_OffHand);
        }
        pl = find(pl, classname, "player");
    }
    self = oldSelf;
};
