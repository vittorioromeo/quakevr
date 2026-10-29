#ifdef QVR_TESTS
// vr_shock's test harness (moved out of vr_shock.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// vr_shock_hit_test <damage>: a lightning bolt of that much damage (-1: just enough to kill) from the player's eyes into
// the middle of the nearest monster or corpse (within 512 units), struck as the gun's bolt strikes what it meets
// (VR_LightningHit: its hit kind, its head pops; whatever lies between); prints what it did. -2: no bolt, it is killed
// by a plain blow (its living shock carried on through its death: VR_Shock_Killed).
void() VR_Shock_TestFrame =
{
    float damage = cvar("vr_shock_hit_test");
    if(!damage)
    {
        return;
    }
    cvar_set("vr_shock_hit_test", "0");
    entity p = VR_Player();
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
    if(damage == -2)
    {
        float carried = vr_shock_carried;
        float before = max(0, best.vr_shock_until - time);
        T_Damage(best, p, p, max(1, best.health + 1));
        bprint(sprintf("shockhittest: plain kill of %s: alive_before=%g health=%g shock_left %g -> %g", best.classname, alive,
            best.health, before, max(0, best.vr_shock_until - time)));
        bprint(sprintf(" (whole from the last bolt %g, %g s ago) carried=%g\n", cvar("vr_shock_death_time"),
            time - best.vr_shock_bolt, vr_shock_carried - carried));
        return;
    }
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
#endif
