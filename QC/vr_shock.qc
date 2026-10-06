// vr_shock.qc -- lightning's lasting shock (vr_shock_death*; the engine's vr_shock.cpp and vr_box3d.cpp, "Shocked
// ragdolls"). A monster the lightning strikes, alive or dead, keeps Quad Damage's arcs crawling over it for
// vr_shock_death_time seconds from the last bolt (bodyshockdeath: every client draws them over the body as drawn, alive,
// dying, its ragdoll or its death frames: the shock goes on through its death), its ragdoll's limbs convulse once it is
// dead, until the arcs end (.vr_shock_until: the engine drives them, vr_shock_seizure), and the killing bolt chars it
// all over (vr_shock_burns marks: woundevent's lightning burns). Every hit, alive or dead, also has its own short arcs
// and a burn where it strikes (weapons.qc VR_LightningHit, vr_wounds.qc).

.float vr_shock_until; // shocked until then (its arcs); the engine reads it: its ragdoll convulses until then (vr_box3d.cpp)
.float vr_shock_len;   // ... the shock's length from the last bolt (s)
float vr_shock_sent;   // lasting shocks sent (the tests)
float vr_shock_burnt;  // burn marks sent for them (the tests)

// `n` burn marks over `t`, from all round it: from the sides at any height (standing as it starts to die), and some
// from above (lying, a corpse).
void(entity t, float n) VR_Shock_Burns =
{
    if(n <= 0 || !VR_Wound_Takes(t))
    {
        return;
    }
    vector mid = (t.absmin + t.absmax) * 0.5;
    vector size = t.absmax - t.absmin;
    float reach = max(size_x, size_y) * 0.5 + 16;
    float lying = t.vr_corpse == 2;
    float i = 0;
    while(i < n)
    {
        vector at = mid;
        vector dir = '0 0 -1';
        if(lying || random() < 0.25)
        {
            at_x = mid_x + (random() - 0.5) * size_x * 0.8;
            at_y = mid_y + (random() - 0.5) * size_y * 0.8;
            at_z = t.absmax_z + 16;
        }
        else
        {
            float a = random() * 6.2831853;
            dir_x = cos(a);
            dir_y = sin(a);
            dir_z = 0;
            at = mid - dir * reach;
            at_z = t.absmin_z + size_z * (0.15 + 0.75 * random());
        }
        woundevent(t, at, dir, QVR_WOUND_ZAP, 30 + 30 * random(), 0);
        vr_shock_burnt++;
        i++;
    }
};

// After a bolt's T_Damage to `t` (weapons.qc VR_LightningHit), struck at `xAt` going `xDir`; `xAlive`: it was alive
// before. Every bolt into a monster, alive (vr_shock_living) or dead, starts the lasting shock or keeps it fresh (sent
// again each bolt: the arcs and the convulsions end together); a killing bolt's burn marks all over, a dead one's a burn where it strikes.
void(entity t, float xAlive, vector xAt, vector xDir) VR_Shock_Hit =
{
    // (vr_shock_living 0: the lasting shock only on the bodies it kills or strikes dead; a living one has each hit's own
    // short arcs, weapons.qc's bodyshock.)
    if(!(t.flags & FL_MONSTER) || t.vr_gib || !cvar("vr_shock_death") || (t.health > 0 && !cvar("vr_shock_living")))
    {
        return;
    }
    float len = cvar("vr_shock_death_time");
    if(len <= 0)
    {
        return;
    }
    if(xAlive && t.health <= 0)
    {
        VR_Shock_Burns(t, floor(cvar("vr_shock_burns")));
    }
    else if(!xAlive && VR_Wound_Takes(t) && xDir != '0 0 0')
    {
        // (A corpse's damage paints no wound: a burn where the bolt strikes it, as a living one's.)
        xDir = normalize(xDir);
        woundevent(t, xAt - xDir * 8, xDir, QVR_WOUND_ZAP, 30, 0);
        vr_shock_burnt++;
    }
    bodyshockdeath(t, len);
    vr_shock_sent++;
    t.vr_shock_until = time + len;
    t.vr_shock_len = len;
};

// vr_shock_hit_test <damage>: a lightning bolt of that much damage (-1: just enough to kill) from the player's eyes into
// the middle of the nearest monster or corpse (within 512 units), struck as the gun's bolt strikes what it meets
// (VR_LightningHit: its hit kind, its head pops; whatever lies between); prints what it did.
void() VR_Shock_TestFrame =
{
    float damage = cvar("vr_shock_hit_test");
    if(!damage)
    {
        return;
    }
    cvar_set("vr_shock_hit_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity best = world;
    float bestD = 512;
    for(entity e = findflags(world, flags, FL_MONSTER); e; e = findflags(e, flags, FL_MONSTER))
    {
        float d = vlen((e.absmin + e.absmax) * 0.5 - p.origin);
        if(e.takedamage && !e.vr_gib && d < bestD)
        {
            best = e;
            bestD = d;
        }
    }
    if(best == world)
    {
        bprint("shockhittest: no monster near\n");
        return;
    }
    float sent = vr_shock_sent;
    float burnt = vr_shock_burnt;
    float wounds = vr_wound_sent;
    float alive = best.health > 0;
    if(damage < 0)
    {
        damage = alive ? best.health + 1 : 1; // (-1: just enough to kill it, not to gib it)
    }
    entity oself = self;
    self = p;
    vector from = p.origin + p.view_ofs;
    vector to = (best.absmin + best.absmax) * 0.5;
    VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', normalize(to - from));
    vr_decap_weapon = QVR_DECAP_LIGHTNING;
    trace_endpos = to; // (struck at its middle, whatever lies between: the gun's own hit, VR_LightningHit)
    trace_ent = best;
    VR_LightningHit(best, p, damage, to - from);
    vr_decap_weapon = 0;
    VR_ClearHitKind();
    self = oself;
    bprint(sprintf("shockhittest: target=%s at=%v alive_before=%g health=%g corpse=%g\n", best.classname, best.origin,
        alive, best.health, best.vr_corpse));
    bprint(sprintf("shockhittest: shock_left=%g lasting_sent=%g burns=%g hit_wounds=%g time=%g\n",
        max(0, best.vr_shock_until - time), vr_shock_sent - sent, vr_shock_burnt - burnt, vr_wound_sent - wounds, time));
};
