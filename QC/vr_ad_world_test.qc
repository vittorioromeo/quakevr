#ifdef QVR_TESTS
// AD support: Debug > Tests world interactions on actual AD map entities (vr_ad_test 61..64).
void(float request, entity p) AD_WorldTest =
{
    entity os = self, oo = other;
    if(request == 61)
    {
        entity button = world;
        float best = 999999;
        for(entity b = find(world, classname, "func_button"); b; b = find(b, classname, "func_button"))
        {
            if(b.ad_classtype != ad_CT_FUNCBUTTON || b.max_health) continue;
            float d = vlen((b.absmin + b.absmax) * 0.5 - '338 314 -312');
            if(d < best) { best = d; button = b; }
        }
        if(!button) { bprint("adworld: no AD touch button\n"); return; }
        float savedbits = p.vrbits0;
        vector savedhand = p.handpos, savedrot = p.handrot;
        p.vrbits0 |= QVR_VRBITS0_HANDSTRACKED;
        self = button; other = p;
        float estate = button.ad_estate;
        bprint(sprintf("adworld: button %s at %v target %s handlers %g/%g/%g\n", button.model,
            (button.absmin + button.absmax)*0.5, button.target, button.handtouch == button_handtouch,
            button.vr_wpntouch == button_wpntouch, button.touch == button_touch_any));
        for(float mode = 0; mode < 4; ++mode)
        {
            self = button; other = p;
            button.ad_estate = ad_ESTATE_ON;
            ad_func_button_done();
            setorigin(button, button.pos1);
            if(mode == 0) button.handtouch();
            else if(mode == 1)
            {
                VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, 0);
                p.handpos = (button.absmin + button.absmax) * 0.5;
                p.handrot = '0 0 0';
                self = p; VR_Buttons_WeaponFrame(); self = button;
                VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
            }
            else
            {
                entity prop = spawn();
                prop.vr_rigid = 1; prop.movetype = MOVETYPE_TOSS; prop.velocity = '200 0 0'; prop.carry_letgo = p;
                if(mode == 2) { other = prop; button.touch(); }
                else
                {
                    prop.classname = "vr_ad_button_test_prop";
                    setsize(prop, '-4 -4 -4', '4 4 4');
                    setorigin(prop, (button.absmin + button.absmax) * 0.5);
                    self = p; VR_Carry_SetHeld(p, cVR_MainHand, prop);
                    VR_Buttons_PropFrame(p, cVR_MainHand);
                    VR_Carry_SetHeld(p, cVR_MainHand, world); self = button;
                }
                remove(prop);
            }
            bprint(sprintf("adworld: button mode %g opened %g activator %g\n", mode, button.state == ad_STATE_UP, button.enemy == p));
            ad_func_button_wait(); // real AD targets, with real player activator
        }
        self = button; other = p;
        button.state = ad_STATE_BOTTOM; button.ad_estate = ad_ESTATE_DISABLE;
        button.handtouch();
        VR_Button_PressBy(p, cVR_MainHand, button);
        bprint(sprintf("adworld: disabled button stayed shut %g\n", button.state == ad_STATE_BOTTOM));
        button.ad_estate = estate;
        ad_func_button_done(); setorigin(button, button.pos1);
        p.vrbits0 = savedbits; p.handpos = savedhand; p.handrot = savedrot;
    }
    else if(request == 62)
    {
        float count = 0, physical = 0;
        for(entity b = nextent(world); b; b = nextent(b))
        {
            if(b.ad_touch2 != ad_BackpackTouch || b.model == "") continue;
            count++;
            if(b.vr_rigid && b.handtouch == VR_Carry_Handtouch && b.touch == forcegrabbable_touch) physical++;
            if(count <= 3) bprint(sprintf("adworld: map bag %s size %v holster %g rigid %g\n", b.model,
                b.maxs - b.mins, b.vr_pickup_obj, b.vr_rigid));
        }
        bprint(sprintf("adworld: map bags %g physical %g\n", count, physical));
        entity donor = spawn(); donor.origin = p.origin + '64 0 24'; donor.ad_classtype = ad_CT_MONENFORCER;
        donor.ammo_shells = 5;
        self = donor; ad_DropBackpack(); remove(donor);
        entity bag = world;
        for(entity b = find(world, classname, "item_backpack"); b; b = find(b, classname, "item_backpack"))
            if(b.ad_classtype == ad_CT_AMMODROPPACK && b.ammo_shells == 5) bag = b;
        if(!bag) { bprint("adworld: drop missing\n"); self=os;other=oo;return; }
        float savedflags = p.flags, savedbits = p.vrbits0, savedhotspot = p.mainhand_hotspot;
        vector savedhand = p.handpos;
        p.flags -= p.flags & FL_NOTARGET;
        p.vrbits0 |= QVR_VRBITS0_HANDSTRACKED;
        p.ammo_shells = 0;
        self = bag; other = p; bag.touch();
        bprint(sprintf("adworld: walkover ammo %g bag %s size %v rigid %g expiry %g\n", p.ammo_shells,
            bag.model, bag.maxs - bag.mins, bag.vr_rigid, bag.nextthink2 - time));
        // Real carry handtouch/grip edge, then actual release frame at a shoulder holster.
        p.touchinghand = cVR_MainHand; p.handpos = bag.origin;
        VRSetMainHandGrabbingBit(p, TRUE); VRSetMainHandPrevGrabbingBit(p, FALSE);
        self = bag; other = p; bag.handtouch();
        bprint(sprintf("adworld: grabbed %g carry %g\n", VR_Carry_Held(p, cVR_MainHand) == bag, bag.carry_player == p));
        VRSetMainHandGrabbingBit(p, FALSE);
        p.mainhand_hotspot = QVR_HS_RIGHT_SHOULDER_HOLSTER;
        self = p; VR_Carry_HandFrame(cVR_MainHand);
        bprint(sprintf("adworld: holster ammo %g held %g models %s/%s\n", p.ammo_shells,
            VR_Carry_Held(p, cVR_MainHand) != world, p.weaponmodel, p.weaponmodel2));

        // Map-authored respawn bags pass through AD's restore callback again on toggle/respawn.
        entity placed = spawn(); placed.classname = "item_backpack";
        placed.origin = p.origin + '96 0 24'; placed.ammo_shells = 5;
        placed.spawnflags = ad_BACKPACK_SHELLS | ad_ITEM_RESPAWN;
        self = placed; ad_item_backpack(); ad_item_backpack_setup();
        placed.ad_attack_finished = 0; ad_item_on();
        vector firstsize = placed.maxs - placed.mins;
        ad_item_off(); ad_item_on();
        bprint(sprintf("adworld: restored bag physical %g same size %g holster %g\n",
            placed.vr_rigid && placed.touch == forcegrabbable_touch && placed.handtouch == VR_Carry_Handtouch,
            firstsize == placed.maxs - placed.mins, placed.vr_pickup_obj == QVR_PICKUPOBJ_HOLSTER));
        remove(placed);
        p.flags = savedflags; p.vrbits0 = savedbits; p.mainhand_hotspot = savedhotspot; p.handpos = savedhand;
    }
    else if(request == 63)
    {
        entity ladder = find(world, classname, "trigger_ladder");
        if(!ladder) { bprint("adworld: no ladder\n"); return; }
        float bits = p.vrbits0, oldflags = ladder.spawnflags, oldgrav = p.gravity;
        float climb = cvar("vr_climb"), ladders = cvar("vr_climb_ladders");
        p.vrbits0 |= QVR_VRBITS0_HANDSTRACKED;
        ladder.ad_bmodel_act = p;
        // cfg 0: hands climb the ladder (AD's keyboard climb off); 1: Hand Climbing off, 2: Hand-Climbed Ladders off
        // (both: AD's own climb, so the ladder stays climbable).
        for(float cfg = 0; cfg < 3; ++cfg)
        {
            cvar_set("vr_climb", cfg == 1 ? "0" : "1");
            cvar_set("vr_climb_ladders", cfg == 2 ? "0" : "1");
            for(float mode = 0; mode < 2; ++mode)
            {
                p.velocity = '21 0 -17'; p.button2 = 1; p.ad_onladder = ad_LADDER_NONE; p.gravity = oldgrav;
                p.vr_ad_laddergravity = 0;
                ladder.spawnflags = mode ? ad_TRIG_LADDERVELOCITY : 0;
                self = ladder; ad_trigger_ladder_fire();
                self = p; AD_PlayerPreThink();
                bprint(sprintf("adworld: ladder cfg %g mode %g velocity %v state %g gravity %g\n", cfg, mode, p.velocity,
                    p.ad_onladder, p.gravity));
                if(p.vr_ad_laddergravity) p.gravity = p.vr_ad_laddergravity;
                p.vr_ad_laddergravity = 0; p.ad_onladder = ad_LADDER_NONE;
            }
        }
        cvar_set("vr_climb", ftos(climb)); cvar_set("vr_climb_ladders", ftos(ladders));
        p.vrbits0 = bits; p.button2 = 0; ladder.spawnflags = oldflags; p.gravity = oldgrav; p.velocity = '0 0 0';
    }
    else if(request == 64)
    {
        // A flat ladder: a ladder volume on a plain wall (no rungs), the nearest wall west of the player. Its hand
        // probes (ad_world_test.sh) must hold anywhere on it.
        makevectors('0 180 0');
        traceline(p.origin, p.origin + v_forward * 512, TRUE, p);
        vector wall = trace_endpos, n = trace_plane_normal;
        entity flat = spawn();
        flat.classname = "vr_test_flatladder";
        flat.solid = SOLID_TRIGGER;
        flat.vr_climb_ladder = 1;
        setorigin(flat, '0 0 0');
        vector lo = wall - '16 16 0', hi = wall + '16 16 128';
        if(n_x > 0) { lo_x = wall_x; hi_x = wall_x + 24; } else { lo_x = wall_x - 24; hi_x = wall_x; }
        lo_z = p.origin_z + p.mins_z;
        setsize(flat, lo, hi);
        bprint(sprintf("adworld: flat ladder wall %g %g %g normal %g %g %g fraction %g\n", wall_x, wall_y, wall_z, n_x, n_y,
            n_z, trace_fraction));
    }
    self = os; other = oo;
};
#endif
