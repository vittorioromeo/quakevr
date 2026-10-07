// button and multiple button

// VR: a brush entity drawing an external brush model instead of one of the map's own ("*N"): an entity
// file can add those (vrfiringrange's second row of monster buttons: maps/vr_spawnpanel.bsp on a
// func_wall, maps/vr_spawnbutton.bsp on a func_button; Misc/quakevr/make_spawn_buttons.py). Unlike the
// map's own models they must be precached before setmodel.
void() VR_PrecacheOwnModel =
{
    if(strlen(self.model) > 0 && substring(self.model, 0, 1) != "*")
    {
        precache_model(self.model);
    }
};

void(entity entPlayer) VRButtonPressHaptic =
{
    if(!entPlayer.ishuman)
    {
        return;
    }

    // (haptic buzzes `self`'s controllers: the touch's self is the button.)
    entity oldSelf = self;
    self = entPlayer;
    if(VRIsTouchingHandFake(entPlayer))
    {
        haptic(0, 0.0, 0.3, 75, 1.0);
        haptic(1, 0.0, 0.3, 75, 1.0);
    }
    else
    {
        haptic(VRGetTouchingHand(entPlayer), 0.0, 0.3, 75, 1.0);
    }
    self = oldSelf;
}

void() button_wait =
{
    self.state = STATE_TOP;
    self.nextthink = self.ltime + self.wait;
    self.think = button_return;
    activator = self.enemy;
    SUB_UseTargets();
    self.frame = 1;            // use alternate textures
    if(cvar_hget(cvarh_vr_debug_wallbuttons))
    {
        dprint(sprintf("button: %s in, frame %.0f, by %s\n", self.model, self.frame, self.enemy.classname));
    }
};

void() button_done =
{
    self.state = STATE_BOTTOM;
};

void() button_return =
{
    vector porg;

    porg_x = self.absmin_x + ((self.absmax_x - self.absmin_x) / 2);
    porg_y = self.absmin_y + ((self.absmax_y - self.absmin_y) / 2);
    porg_z = self.absmin_z + ((self.absmax_z - self.absmin_z) / 2);
    particle2(porg, '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 64);

    self.state = STATE_DOWN;

    SUB_CalcMove (self.pos1, self.speed, button_done);

    self.frame = 0;            // use normal textures
    if(self.health)
        self.takedamage = DAMAGE_YES;    // can be shot again
};


void() button_blocked =
{    // do nothing, just don't ome all the way back out
};


string vr_button_how; // what pressed the button now firing, for vr_debug_wallbuttons (button_fire clears it)

void() button_fire =
{
    if(self.buttonEffect == 3 &&
        ((substring(self.targetname, 0, 28) == "vr_activestartpaknameidx 1; " && !VR_Pack_Hipnotic()) ||
         (substring(self.targetname, 0, 28) == "vr_activestartpaknameidx 2; " && !VR_Pack_Rogue())))
    {
        if(self.enemy.classname == "player")
        {
            centerprint(self.enemy, "Mission pack unavailable. Restore its owned data; Quake is available.");
        }
        return;
    }

    if(self.state == STATE_UP || self.state == STATE_TOP || self.state == STATE_DISABLED)
    {
        return;
    }

    if(MG1_Campaign() && self.classname == "mge2m2_electrode_button") {
        if(self.MG1_electrode_pressed) return;
        MG1_ElectrodePressed();
    }

    if(cvar_hget(cvarh_vr_debug_wallbuttons))
    {
        dprint(sprintf("button: %s (target \"%s\") pressed by %s: %s\n", self.model, self.target, self.enemy.classname,
            vr_button_how != NULLSTR ? vr_button_how : "used or shot"));
    }
    vr_button_how = NULLSTR;
    particle2(realorigin(self), '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 64);

    if(MG_WorldCampaign() && self.items) {
        sound(self, CHAN_ITEM, self.noise4, 1, ATTN_NORM);
        if(MGH_Active()) MGH_horde_key_spend(self.items);
        if(!(self.spawnflags & 32)) self.items = 0;
    }
    sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

    self.state = STATE_UP;
    SUB_CalcMove(self.pos2, self.speed, button_wait);

    // VR
    if(self.buttonEffect == 1) // Test
    {
        bprint("EFFECTS BUTTON!");
    }
    else if(self.buttonEffect == 2) // Tutorial button
    {
        bprint("Congratulations! You have hands.\n");
    }
    else if(self.buttonEffect == 3) // Command button
    {
        localcmd(self.targetname);
    }
};

void() button_use =
{
    self.enemy = activator;
    button_fire();
};

void() button_touch =
{
    if(other.classname != "player")
    {
        return;
    }
    // VR: in, or coming out: not pressed again (nor a buzz each frame the hand rests on it).
    if(self.state == STATE_UP || self.state == STATE_TOP || self.state == STATE_DISABLED)
    {
        vr_button_how = NULLSTR;
        return;
    }

    if(!MG_ButtonCanTouch(other)) return;
    VRButtonPressHaptic(other);

    self.enemy = other;
    button_fire();
};

// ----------------------------------------------------------------------------
// VR: what presses a wall button besides a hand (NOTES.md vrfiringrange_2026-10-02_00-56-19, _01-01-58, _01-35-19;
// ROUND21.md, "Wall buttons: weapons and thrown things press them"): all of a held weapon (VR_Buttons_WeaponFrame), and
// a thing thrown at it (button_touch_any). Shot buttons (with health) are as Quake's: shot, or hit hard (T_Damage).

// A hand on it (handtouch), or the engine's line from a hand to its gun's muzzle (vr_wpntouch).
void() button_handtouch =
{
    if(other.classname == "player")
    {
        vr_button_how = VRIsTouchingHandFake(other) ? "the body (hands not tracked)" :
            sprintf("hand %g", VRGetTouchingHand(other));
    }
    button_touch();
};

void() button_wpntouch =
{
    if(other.classname == "player")
    {
        vr_button_how = sprintf("the line from hand %g to its muzzle", VRGetTouchingHand(other));
    }
    button_touch();
};

// The wall buttons a held weapon presses (a chain through .vr_button_next), listed once a map (0: a new map, or a save
// from before them: listed again).
.entity vr_button_next;
entity vr_buttons;
float vr_buttons_listed;

void() button_touch_any;

void() VR_Buttons_List =
{
    vr_buttons = world;
    entity b = nextent(world);
    while(b != world)
    {
        if(b.classname != "func_button" && !(MG1_Campaign() && b.classname == "mge2m2_electrode_button")) { b = nextent(b); continue; }
        if(b.vr_wpntouch == button_touch) // a save from before: its touches as func_button sets them now
        {
            b.touch = button_touch_any;
            b.handtouch = button_handtouch;
            b.vr_wpntouch = button_wpntouch;
        }
        if(b.vr_wpntouch == button_wpntouch)
        {
            b.vr_button_next = vr_buttons;
            vr_buttons = b;
        }
        b = nextent(b);
    }
    vr_buttons_listed = TRUE;
};

// Whether the segment xA..xB, grown by xR all round, meets the box xLo..xHi (its shares within each axis's slab).
float vr_seg_t0;
float vr_seg_t1;
float(float xA, float xD, float xLo, float xHi) VR_Seg_Slab =
{
    if(fabs(xD) < 0.001)
    {
        return xA >= xLo && xA <= xHi;
    }
    float ta = (xLo - xA) / xD;
    float tb = (xHi - xA) / xD;
    vr_seg_t0 = max(vr_seg_t0, min(ta, tb));
    vr_seg_t1 = min(vr_seg_t1, max(ta, tb));
    return vr_seg_t0 <= vr_seg_t1;
};

float(vector xA, vector xB, float xR, vector xLo, vector xHi) VR_SegmentMeetsBox =
{
    vector d = xB - xA;
    vector r = '1 1 1' * xR;
    xLo = xLo - r;
    xHi = xHi + r;
    vr_seg_t0 = 0;
    vr_seg_t1 = 1;
    if(!VR_Seg_Slab(xA_x, d_x, xLo_x, xHi_x))
    {
        return FALSE;
    }
    if(!VR_Seg_Slab(xA_y, d_y, xLo_y, xHi_y))
    {
        return FALSE;
    }
    return VR_Seg_Slab(xA_z, d_z, xLo_z, xHi_z);
};

// A wall button a held thing may press now: not freed, still pressed by touch, not in or coming out.
float(entity xButton) VR_Button_Pressable =
{
    return xButton.solid == SOLID_BSP && xButton.vr_wpntouch == button_wpntouch && xButton.state != STATE_UP &&
           xButton.state != STATE_TOP && xButton.state != STATE_DISABLED;
};

// Player `xPlayer`'s hand `xHand` presses wall button `xButton` (with what: vr_button_how).
void(entity xPlayer, float xHand, entity xButton) VR_Button_PressBy =
{
    xPlayer.touchinghand = xHand; // the buzz's hand (VRButtonPressHaptic)
    entity oself = self;
    self = xButton;
    if(!MG_ButtonCanTouch(xPlayer)) { self = oself; return; }
    self.enemy = xPlayer;
    VRButtonPressHaptic(xPlayer);
    button_fire();
    self = oself;
};

// A prop held in hand `xHand` (a health or ammo box, a crate, a rock, a gib; NOTES.md vrfiringrange_2026-10-03_02-23-25)
// presses a wall button it touches, as a held weapon does: its shape as the hand holds it (the engine's held body:
// heldbox), grown by vr_button_prop_reach; its box (absmin..absmax) if it has no held body.
void(entity xPlayer, float xHand) VR_Buttons_PropFrame =
{
    entity held = VR_Carry_Held(xPlayer, xHand);
    if(held == world || (xHand == cVR_OffHand && held == xPlayer.mainhand_held)) // (held in both: once)
    {
        return;
    }
    float reach = max(0, cvar_hget(cvarh_vr_button_prop_reach)) * 0.01 * VRMetersToUnits();
    if(cvar_hget(cvarh_vr_debug_wallbuttons) >= 2)
    {
        dprint(sprintf("button: hand %g holds %s, box %v to %v, reach %.1f\n", xHand, held.classname, held.absmin,
            held.absmax, reach));
    }
    entity b;
    for(b = vr_buttons; b != world; b = b.vr_button_next)
    {
        if(!VR_Button_Pressable(b))
        {
            continue;
        }
        vector lo = b.origin + b.mins;
        vector hi = b.origin + b.maxs;
        float meets = heldbox(held, lo, hi, reach);
        string how = "shape";
        if(meets < 0)
        {
            how = "box";
            float overlaps = (held.absmin_x <= hi_x + reach) && (held.absmax_x >= lo_x - reach) &&
                             (held.absmin_y <= hi_y + reach) && (held.absmax_y >= lo_y - reach) &&
                             (held.absmin_z <= hi_z + reach) && (held.absmax_z >= lo_z - reach);
            meets = overlaps;
        }
        if(meets > 0)
        {
            vr_button_how = sprintf("the %s held in hand %g (its %s)", held.classname, xHand, how);
            VR_Button_PressBy(xPlayer, xHand, b);
        }
    }
};

// Each frame (PlayerVRMelee, after VR_Melee_Frame): a weapon held in either hand presses a wall button it touches, all
// of it as its blows strike (VR_Melee_Line: from its pommel, handle's end or butt to its tip or muzzle; carried off its
// handle, its two ends), grown by vr_button_weapon_reach. (The engine's test, a thin line from the hand to the muzzle
// only, missed a sword laid against a button: its blade's middle line never reached into it.) A held prop too
// (vr_button_prop: VR_Buttons_PropFrame).
void() VR_Buttons_WeaponFrame =
{
    if(!vr_buttons_listed)
    {
        VR_Buttons_List();
    }
    if(vr_buttons == world || !VRHandsTracked(self))
    {
        return;
    }
    float weapons = cvar_hget(cvarh_vr_button_weapon);
    float props = cvar_hget(cvarh_vr_button_prop);
    float reach = max(0, cvar_hget(cvarh_vr_button_weapon_reach)) * 0.01 * VRMetersToUnits();
    entity p = self;
    float h;
    for(h = 0; h < 2; ++h)
    {
        if(props)
        {
            VR_Buttons_PropFrame(p, h);
        }
        if(!weapons)
        {
            continue;
        }
        float thing = VR_Melee_Thing(p, h);
        if(!VR_Melee_HoldsWeapon(thing) || !VR_Melee_Line(p, h, thing))
        {
            continue;
        }
        vector lineNear = vr_mline_near;
        vector lineFar = vr_mline_far;
        if(cvar_hget(cvarh_vr_debug_wallbuttons) >= 2)
        {
            dprint(sprintf("button: hand %g weapon line %v to %v, reach %.1f\n", h, lineNear, lineFar, reach));
        }
        entity b;
        for(b = vr_buttons; b != world; b = b.vr_button_next)
        {
            if(!VR_Button_Pressable(b) ||
               !VR_SegmentMeetsBox(lineNear, lineFar, reach, b.origin + b.mins, b.origin + b.maxs))
            {
                continue;
            }
            vr_button_how = sprintf("the weapon in hand %g (all of it: %v to %v)", h, lineNear, lineFar);
            VR_Button_PressBy(p, h, b);
        }
    }
};

// A thing thrown at a wall button presses it (vr_button_throw): a weapon, a rock, a box, a gib or a head, going at
// vr_button_throw_speed or faster (the physics' props touch it with the speed they came at: vr_box3d.cpp, touches).
// Not a monster or a player, nor a missile (a shot: as Quake's, only a shot button minds it).
float(entity xThing) VR_Button_Thrown =
{
    // Cosmetic fragments have no gameplay interaction; large gibs still press buttons.
    if(xThing.vr_sgib || xThing.classname == "vr_smallgib")
    {
        return FALSE;
    }
    if((xThing.flags & (FL_CLIENT | FL_MONSTER)) || xThing.movetype == MOVETYPE_FLYMISSILE ||
       xThing.movetype == MOVETYPE_BOUNCEMISSILE || xThing.movetype == MOVETYPE_FLY)
    {
        return FALSE;
    }
    return xThing.classname == "thrown_weapon" || xThing.vr_rigid || xThing.handtouch == VR_Carry_Handtouch;
};

// The touch of a button pressed by touch (not a shot one): a floor button stepped on (VR: as Quake's; a wall button
// isn't pressed by the body, only by a hand), and anything thrown at it.
void() button_touch_any =
{
    local vector dir;

    if(other.classname == "player")
    {
        // (by its direction: the official campaigns keep an authored movedir as the whole move, '0 0 -8')
        dir = normalize(self.movedir);
        if(dir_z < -0.7)
        {
            vr_button_how = "stepped on";
            button_touch();
        }
        return;
    }
    if(!cvar_hget(cvarh_vr_button_throw) || !VR_Button_Thrown(other))
    {
        return;
    }
    float speed = vlen(other.velocity);
    if(speed < cvar_hget(cvarh_vr_button_throw_speed) || self.state == STATE_UP || self.state == STATE_TOP ||
       self.state == STATE_DISABLED)
    {
        return;
    }
    // Who threw it is who pressed it (the targets' activator), else the thing itself.
    entity who = other;
    if(other.owner.flags & FL_CLIENT)
    {
        who = other.owner;
    }
    else if(other.carry_letgo.flags & FL_CLIENT)
    {
        who = other.carry_letgo;
    }
    if(!MG_ButtonCanTouch(who)) return;
    vr_button_how = sprintf("%s thrown at it, %.0f u/s", other.classname, speed);
    self.enemy = who;
    button_fire();
};

void() button_killed =
{
    self.enemy = damage_attacker;
    self.health = self.max_health;
    self.takedamage = DAMAGE_NO;    // wil be reset upon return
    button_fire();
};


/*QUAKED func_button (0 .5 .8) ?
When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

"angle"        determines the opening direction
"target"    all entities with a matching targetname will be used
"speed"        override the default 40 speed
"wait"        override the default 1 second wait (-1 = never return)
"lip"        override the default 4 pixel lip remaining at end of move
"health"    if set, the button must be killed instead of touched
"sounds"
0) steam metal
1) wooden clunk
2) metallic click
3) in-out
"buttonEffect" // VR
0) normal button
N) code button
*/
void() func_button =
{
    if(MG_MonsterInhibited()) return;
    if(MG_WorldCampaign()) MG_ButtonInit();
    if(self.buttonEffect == 3 &&
       ((substring(self.targetname, 0, 28) == "vr_activestartpaknameidx 1; " && !VR_Pack_Hipnotic()) ||
        (substring(self.targetname, 0, 28) == "vr_activestartpaknameidx 2; " && !VR_Pack_Rogue())))
    {
        self.worldtext = strcat(self.worldtext, "\n(unavailable)");
    }

    if(self.sounds == 0)
    {
        precache_sound("buttons/airbut1.wav");
        self.noise = "buttons/airbut1.wav";
    }
    else if(self.sounds == 1)
    {
        precache_sound("buttons/switch21.wav");
        self.noise = "buttons/switch21.wav";
    }
    else if(self.sounds == 2)
    {
        precache_sound("buttons/switch02.wav");
        self.noise = "buttons/switch02.wav";
    }
    else if(self.sounds == 3)
    {
        precache_sound("buttons/switch04.wav");
        self.noise = "buttons/switch04.wav";
    }

    float mgExplicitMove = MG_WorldCampaign() && self.movedir != '0 0 0';
    SetMovedir();

    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    VR_PrecacheOwnModel();
    setmodel(self, self.model);

    self.blocked = button_blocked;
    self.use = button_use;

    if(self.health)
    {
        self.max_health = self.health;
        self.th_die = button_killed;
        self.takedamage = DAMAGE_YES;
    }
    else
    {
        // VR: For vertical button, let's enable normal touch as well so that
        // they can activated by stepping on them; and every one is pressed by
        // what is thrown at it (button_touch_any).
        self.touch = button_touch_any;

        self.handtouch = button_handtouch;
        self.vr_wpntouch = button_wpntouch;
    }

    if(!self.speed)
        self.speed = 40;
    if(!self.wait)
        self.wait = 1;
    if(!self.lip)
        self.lip = 4;

    // HONEY
    if(!MG_WorldCampaign() && self.items) {
        self.flags = self.flags | FL_SPECIFICDAMAGE;
    }

    self.state = STATE_BOTTOM;

    self.pos1 = self.origin;
    if(mgExplicitMove) self.pos2 = self.pos1 + self.movedir;
    else self.pos2 = self.pos1 + self.movedir*(fabs(self.movedir*self.size) - self.lip);

    // HONEY
    if(!MG_WorldCampaign() && (self.spawnflags & START_DISABLED)) {
        button_disable(self);
    }

    if(self.worldtext != NULLSTR)
    {
        vector dir = self.movedir;
        vector ang = vectoangles(dir);

        float wth = worldtext_hmake();
        worldtext_hsettext(wth, self.worldtext);
        worldtext_hsetpos(wth, realorigin(self) + '0 0 12' - dir * 6.0);
        worldtext_hsetangles(wth, ang);
        worldtext_hsethalign(wth, self.worldtext_halign);
        
        if(self.worldtext_scale != 0)
        {
            worldtext_hsetscale(wth, self.worldtext_scale);
        }
    }
};

// TODO VR: (P2) move
void() func_particle_emitter_think =
{
    particle2(self.origin, '0 0 0', QVR_PARTICLE_PRESET_BIGSMOKE, 1);
    self.nextthink = time + 0.1;
}

void() func_particle_emitter =
{
    self.think = func_particle_emitter_think;
    self.nextthink = time + 0.1;
}

void() func_worldtext_banner_think =
{
    float wth = self.weapon;
    float len = self.t_length;
    float nPages = self.lip;

    float startIdx;
    float endIdx;

    if(self.state == 0) // On first page
    {
        startIdx = 0;
        endIdx = VR_WorldTextUtil_GetPageStringCharStartIdx(self, 0);
    }
    else if(self.state == (nPages - 1)) // On last page
    {
        startIdx = VR_WorldTextUtil_GetPageStringCharStartIdx(self, self.state - 1) + 1;
        endIdx = len;
    }
    else
    {
        startIdx = VR_WorldTextUtil_GetPageStringCharStartIdx(self, self.state - 1) + 1;
        endIdx = VR_WorldTextUtil_GetPageStringCharStartIdx(self, self.state);
    }

    string newText = substring(self.worldtext, startIdx, endIdx - startIdx);
    worldtext_hsettext(wth, newText);

    self.state += 1;
    if(self.state == nPages)
    {
        self.state = 0;
    }

    self.nextthink = time + (self.speed * (endIdx - startIdx));
}

float(entity xEntWorldText, float xPageIdx)
VR_WorldTextUtil_GetPageStringCharStartIdx =
{
    ASSERT(xPageIdx >= 0);
    ASSERT(xPageIdx < 6);
    ASSERT(xPageIdx < xEntWorldText.lip);

    return xEntWorldText.holsterhover[xPageIdx];
}

void(entity xEntWorldText, float xPageIdx, float xCharStartIdx)
VR_WorldTextUtil_SetPageStringCharStartIdx =
{
    ASSERT(xPageIdx >= 0);
    ASSERT(xPageIdx < 6);
    ASSERT(xPageIdx < xEntWorldText.lip);

    xEntWorldText.holsterhover[xPageIdx] = xCharStartIdx;
}

float(entity xEntWorldText)
VR_WorldTextUtil_InitializePages =
{
    string s = xEntWorldText.worldtext;
    ASSERT(s != NULLSTR);

    float nSeps = 0;
    float len = xEntWorldText.t_length;
    for(float i = 0; i < len; ++i)
    {
        if(str2chr(s, i) == '$')
        {
            VR_WorldTextUtil_SetPageStringCharStartIdx(xEntWorldText, nSeps, i);
            ++nSeps;
        }
    }

    return nSeps + 1;
}

// Parameters:
// * `.worldtext`: Text to display. The single character `$` is interpreted as
//   a page break and will make the banner automatically scroll between pages.
// * `.worldtext_halign`: Horizontal text alignment. (`0`: Left, `1`: Center,
//   `2`: Right)
// * `.speed`: Time to scroll to the next page, in seconds per character.
void() func_worldtext_banner =
{
    ASSERT(self.worldtext_halign == 0 ||
           self.worldtext_halign == 1 ||
           self.worldtext_halign == 2);

    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel(self, self.model);

    if(self.worldtext == NULLSTR)
    {
        return;
    }

    // Cache string length
    self.t_length = strlen(self.worldtext);

    // Initialize pages
    self.state = 0; // Current page
    self.lip = VR_WorldTextUtil_InitializePages(self); // Number of pages

    // Create and cache world text handle
    float wth = worldtext_hmake();
    self.weapon = wth;

    worldtext_hsetpos(wth, realorigin(self));
    worldtext_hsetangles(wth, self.angles + '0 180 0');
    worldtext_hsethalign(wth, self.worldtext_halign);

    if(self.worldtext_scale != 0)
    {
        worldtext_hsetscale(wth, self.worldtext_scale);
    }

    if(self.lip > 1)
    {
        self.think = func_worldtext_banner_think;
        self.nextthink = time + 0.05;
    }
    else
    {
        worldtext_hsettext(wth, self.worldtext);
    }
}

void() func_weapon_dispenser_use =
{
    CreateThrownWeapon(self, self.weapon, 0 /* weaponflags */,
        WeaponInst_New(WeaponIdToBaseClipSize(self.weapon)) /* a new one, loaded */,
        self.origin, randomVec(180),
        '0 0 0', LegacyThrowSpin('0 0 0', randomVec(20)),
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(self.weapon)
    );
}

void() func_weapon_dispenser =
{
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel(self, self.model);

    self.use = func_weapon_dispenser_use;
}

// A weapon lying ready to be picked up: "weapon" its id (WID_*), "weaponflags" its flags (a knight's
// sword, WID_SWORD 13, with 1 is the hell knight's; an ogre's chainsaw, WID_CHAINSAW 14, full of fuel; a grunt's
// shotgun, WID_GRUNTGUN 15, and an enforcer's laser rifle, WID_ENFRIFLE 16, full: Enemy Weapons' Shells, Shots; the
// crowbar, WID_CROWBAR 17: vr_crowbar.qc, whose weapon_crowbar is the same).
void() func_weapon_grabbable =
{
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel(self, self.model);

    // Turned any way, still: it falls and settles on its drawn shape (a rigid body, MakeThrown).
    CreateThrownWeapon(self, self.weapon, self.weaponflags,
        WeaponInst_New(WeaponIdToBaseClipSize(self.weapon)) /* a new one, loaded */,
        self.origin, randomVec(180),
        '0 0 0', '0 0 0',
        FALSE /* disappear on hit */,
        FALSE /* deal damage */,
        WeaponIdToThrowDamage(self.weapon)
    );
}

// A monster dispenser (vrfiringrange's monster buttons): when used, spawns the monster "weapon" at its
// origin. 0..4 are the map's own row of buttons (west railing), the rest the second row its entity file
// adds (east edge):
//   0 soldier, 1 ogre, 2 zombie, 3 shambler, 4 scrag,
//   5 knight, 6 hell knight, 7 rottweiler, 8 enforcer, 9 fiend, 10 vore, 11 spawn,
//   12 gremlin, 13 centroid (Scourge of Armagon), 14 mummy, 15 phantom swordsman, 16 wrath,
//   17 overlord (Dissolution of Eternity).
// Tests only (impulse 241's vr_test_spawn; no button): 18 the Guardian (Dissolution of Eternity's monster_morph, its
// Egyptian look), 19 the dragon (Dissolution of Eternity), 20 the marksman ogre (Honey's model, progs/mogre.mdl).
// The model of the second row's monsters: the mission packs' (and the registered game's) may not be
// installed, and precaching a missing model is an error.
string(float choice) VR_EnemyDispenser_Model =
{
         if(choice == 5)  { return "progs/knight.mdl"; }
    else if(choice == 6)  { return "progs/hknight.mdl"; }
    else if(choice == 7)  { return "progs/dog.mdl"; }
    else if(choice == 8)  { return "progs/enforcer.mdl"; }
    else if(choice == 9)  { return "progs/demon.mdl"; }
    else if(choice == 10) { return "progs/shalrath.mdl"; }
    else if(choice == 11) { return "progs/tarbaby.mdl"; }
    else if(choice == 12) { return "progs/grem.mdl"; }
    else if(choice == 13) { return "progs/scor.mdl"; }
    else if(choice == 14) { return "progs/mummy.mdl"; }
    else if(choice == 15) { return "progs/sword.mdl"; }
    else if(choice == 16) { return "progs/wrath.mdl"; }
    else if(choice == 17) { return "progs/s_wrath.mdl"; }
    else if(choice == 18) { return "progs/morph_eg.mdl"; }
    else if(choice == 19) { return "progs/dragon.mdl"; }
    else if(choice == 20) { return "progs/mogre.mdl"; }

    return NULLSTR;
};

// The monster `choice` (a dispenser's "weapon") spawned on self: its classname and its spawn function, as a map's
// (VR_Probe_Kinds makes each kind the map's dispensers can make this way as the map loads, then removes it).
void(float choice) VR_EnemyDispenser_Spawn =
{
    // The classname a map's monster has: sight sounds, obituaries and the like go by it.
         if(choice == 0)  { self.classname = "monster_army";        monster_army(); }
    else if(choice == 1)  { self.classname = "monster_ogre";        monster_ogre(); }
    else if(choice == 2)  { self.classname = "monster_zombie";      monster_zombie(); }
    else if(choice == 3)  { self.classname = "monster_shambler";    monster_shambler(); }
    else if(choice == 4)  { self.classname = "monster_wizard";      monster_wizard(); }
    else if(choice == 5)  { self.classname = "monster_knight";      monster_knight(); }
    else if(choice == 6)  { self.classname = "monster_hell_knight"; monster_hell_knight(); }
    else if(choice == 7)  { self.classname = "monster_dog";         monster_dog(); }
    else if(choice == 8)  { self.classname = "monster_enforcer";    monster_enforcer(); }
    else if(choice == 9)  { self.classname = "monster_demon1";      monster_demon1(); }
    else if(choice == 10) { self.classname = "monster_shalrath";    monster_shalrath(); }
    else if(choice == 11) { self.classname = "monster_tarbaby";     monster_tarbaby(); }
    else if(choice == 12) { self.classname = "monster_gremlin";     monster_gremlin(); }
    else if(choice == 13) { self.classname = "monster_scourge";     monster_scourge(); }
    else if(choice == 14) { self.classname = "monster_mummy";       monster_mummy(); }
    else if(choice == 15) { self.classname = "monster_sword";       monster_sword(); }
    else if(choice == 16) { self.classname = "monster_wrath";       monster_wrath(); }
    else if(choice == 17) { self.classname = "monster_super_wrath"; monster_super_wrath(); }
    else if(choice == 18) { self.classname = "monster_morph";       self.spawnflags = 4 /* MORPH_EGYPT */; monster_morph(); }
    else if(choice == 19) { self.classname = "monster_dragon";      monster_dragon(); }
    else if(choice == 20) { self.classname = "monster_ogre_marksman"; monster_ogre_marksman(); }

};

void() func_enemy_dispenser_use =
{
    vector org = self.origin;
    float choice = self.weapon;

    // Not installed: say what it needs instead.
    string mdl = VR_EnemyDispenser_Model(choice);
    if(mdl != NULLSTR && !fileexists(mdl)) // (its model there: the monster's own spawn checks the rest)
    {
        if(activator.classname == "player")
        {
            if(choice == 20)
            {
                centerprint(activator, "Needs Honey's marksman ogre (progs/mogre.mdl)");
            }
            else if(choice >= 14)
            {
                centerprint(activator, "Needs Dissolution of Eternity (rogue)");
            }
            else if(choice >= 12)
            {
                centerprint(activator, "Needs Scourge of Armagon (hipnotic)");
            }
            else
            {
                centerprint(activator, "Needs the registered Quake (pak1.pak)");
            }
        }

        return;
    }

    self = spawn();
    self.spawnflags = cvar("vr_test_spawn_flags"); // (0; the tests' variants: 2 a statue knight, a multi-grenade ogre)
    if((self.spawnflags & 2) && (choice == 5 || choice == 6))
    {
        self.targetname = "vr_test_statue"; // (a statue wakes when used: woken below, as its map's trigger would)
    }

    VR_EnemyDispenser_Spawn(choice);

    setorigin(self, org);
    if(self.targetname == "vr_test_statue" && self.use)
    {
        self.use(); // (Rogue's statue knight or hell knight: stone come alive)
    }
}

void() func_enemy_dispenser =
{
    if(self.weapon < 5)
    {
        monster_army_precache();
        monster_ogre_precache();
        monster_zombie_precache();
        monster_shambler_precache();
        monster_wizard_precache();
    }
    else
    {
        // The model now, if installed (no hitch at the first press); the monster's other models and
        // its sounds as the map ends spawning (vr_probe.qc: one made then removed).
        string mdl = VR_EnemyDispenser_Model(self.weapon);
        if(mdl != NULLSTR && fileexists(mdl))
        {
            precache_model(mdl);
        }
    }

    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel(self, self.model);

    self.use = func_enemy_dispenser_use;
}
