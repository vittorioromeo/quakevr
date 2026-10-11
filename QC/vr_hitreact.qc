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

// A living monster struck by `take` (T_DamageDeal, after the blow, before its pain): the blow sent, its steps slowed.
void(entity targ, entity inflictor, entity attacker, float take) VR_HitReact_Hit =
{
    if(take <= 0 || !(targ.flags & FL_MONSTER) || targ.health <= 0 || targ.vr_knockdown || !cvar_hget(cvarh_vr_hit_react))
    {
        return;
    }
    // (no flinch for a slow bleed, a burn's ticks, a liquid's)
    if(vr_hitkind == QVR_HITKIND_BLEED || inflictor.classname == "vr_burn" || inflictor == world)
    {
        return;
    }

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
    dir = normalize(dir);
    float share = take / max(targ.max_health, 1);
    share = min(max(share, 0), 1);
    hitreact(targ, at, dir, part, min(255, take), share);

    // The slow-down: the stronger of what is left of the last one and this one's
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

// Test aid `vr_test_hit <n>` (console; Misc/quakevr/ragdoll/hitreact_test.sh): the live monster nearest the player
// struck by a shot of his (vr_test_hit_damage, never a kill: left 1 health at least), struck (n - 1) on: 0 the body,
// 1 the head, 2 an arm (its right side as he sees it), 3 a leg, on its side facing him. Prints `test hit:`.
// ... and the monster it struck watched each frame for vr_test_hit_watch seconds after (`hitwatch:` lines: its frame,
// place, health, enemy and attack state; the tests compare them with the reactions on and off).
entity vr_hr_watch;
float vr_hr_watch_until;

void() VR_TestHit_Frame =
{
    if(vr_hr_watch != world && time < vr_hr_watch_until)
    {
        dprint(sprintf("hitwatch: %.3f %s frame %g at %v health %g enemy %d state %g\n", time, vr_hr_watch.classname,
            vr_hr_watch.frame, vr_hr_watch.origin, vr_hr_watch.health, vr_hr_watch.enemy.classname,
            vr_hr_watch.attack_state));
    }
    float want = cvar("vr_test_hit");
    if(want < 1)
    {
        return;
    }
    cvar_set("vr_test_hit", "0");
    entity pl = find(world, classname, "player");
    entity best = world;
    float bestD = 100000;
    entity e = nextent(world);
    while(e)
    {
        if((e.flags & FL_MONSTER) && e.health > 0 && e.takedamage && !VR_IsDummy(e) && vlen(e.origin - pl.origin) < bestD)
        {
            bestD = vlen(e.origin - pl.origin);
            best = e;
        }
        e = nextent(e);
    }
    if(best == world)
    {
        dprint("test hit: no live monster\n");
        return;
    }
    float part = want - 1;
    vector dir = best.origin - pl.origin;
    dir_z = 0;
    dir = normalize(dir);
    vector right = '0 0 0';
    right_x = dir_y;
    right_y = 0 - dir_x;
    float lo = best.absmin_z;
    float hi = best.absmax_z;
    vector at = (best.absmin + best.absmax) * 0.5 - dir * ((best.maxs_x - best.mins_x) * 0.35);
    at_z = lo + (hi - lo) * 0.6;
    if(part == 1)
    {
        at_z = hi - (hi - lo) * 0.08;
    }
    else if(part == 2)
    {
        at_z = lo + (hi - lo) * 0.62;
        at = at + right * ((best.maxs_y - best.mins_y) * 0.4);
    }
    else if(part == 3)
    {
        at_z = lo + (hi - lo) * 0.22;
        at = at + right * ((best.maxs_y - best.mins_y) * 0.2);
    }
    VR_ClearHitKind();
    VR_SetHitKind(QVR_HITKIND_SHOT, -1, at, dir);
    vr_hitpart = part;
    vr_hitpart_target = best;
    vr_hitpart_time = time;
    vr_hitpart_pos = at;
    if(cvar("vr_test_hit_watch") > 0)
    {
        vr_hr_watch = best;
        vr_hr_watch_until = time + cvar("vr_test_hit_watch");
    }
    float dmg = min(cvar("vr_test_hit_damage"), best.health - 1);
    dprint(sprintf("test hit: %s at %v (health %g) struck %g at %v for %g\n", best.classname, best.origin, best.health,
        part, at, dmg));
    if(dmg > 0)
    {
        if(cvar("vr_test_hit_calm"))
        {
            // (the flinch alone: no pain frames, and nobody's blow, so it doesn't turn on him)
            best.pain_finished = time + 5;
            T_Damage(best, pl, world, dmg);
        }
        else
        {
            T_Damage(best, pl, pl, dmg);
        }
    }
    VR_ClearHitKind();
    vr_hitpart_target = world;
};
