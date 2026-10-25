#ifdef QVR_TESTS
// vr_grapple's test harness (moved out of vr_grapple.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// impulse 231 (tests): an empty hand (the main one first) takes the nearest grappling gun lying about, as a hand grabbing
// it (its hand touch).
void() VR_Grapple_TestTake =
{
    float hand = cVR_MainHand;
    if(!VRIsHandEmpty(self, cVR_MainHand))
    {
        hand = cVR_OffHand;
        if(!VRIsHandEmpty(self, cVR_OffHand))
        {
            dprint("grapple test: no empty hand to take a gun with\n");
            return;
        }
    }
    entity best = world;
    float bestD = 100000;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(e.weapon == WID_GRAPPLE && e.model != string_null && e.handtouch && vlen(e.origin - self.origin) < bestD)
        {
            best = e;
            bestD = vlen(e.origin - self.origin);
        }
        e = find(e, classname, "thrown_weapon");
    }
    if(best == world)
    {
        dprint("grapple test: no grappling gun lying about\n");
        return;
    }
    dprint(sprintf("grapple test: hand %g takes the gun at %v (tag %g)\n", hand, best.origin, VR_Grapple_TagOf(best.weaponflags)));
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

// impulse 233 (tests): the player takes what every hook is in, as walking into it (a pickup is taken).
void() VR_Grapple_TestTouch =
{
    entity h = find(world, classname, "hook");
    while(h)
    {
        entity t = h.enemy;
        if(h.gh_state == GS_BITTEN && t != world && (t.carry_use || t.handtouch))
        {
            dprint(sprintf("grapple test: you touch %s (hook %g)\n", t.classname != string_null ? t.classname : t.model, h.gh_tag));
            entity oldSelf = self;
            entity oldOther = other;
            // As the body's touch takes it (the "fake hand": vr_util.qc), by its pickup (a carried box's carry_use); a
            // thrown weapon as an empty hand catching it (a force grab's catch: into the hand).
            float hand = cVR_FakeHand;
            if(t.classname == "thrown_weapon")
            {
                if(VRIsHandEmpty(self, cVR_MainHand))     { hand = cVR_MainHand; }
                else if(VRIsHandEmpty(self, cVR_OffHand)) { hand = cVR_OffHand; }
            }
            self.touchinghand = hand;
            t.handtouch_hand = hand;
            t.handtouch_ent = self;
            other = self;
            self = t;
            float oldCatching = forcegrabCatching;
            forcegrabCatching = TRUE;
            if(t.carry_use)
            {
                t.carry_use();
            }
            else
            {
                t.handtouch();
            }
            forcegrabCatching = oldCatching;
            self = oldSelf;
            other = oldOther;
        }
        h = find(h, classname, "hook");
    }
};
#endif
