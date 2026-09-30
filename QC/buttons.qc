// button and multiple button

// VR: a brush entity drawing an external brush model instead of one of the map's own ("*N"): an entity
// file can add those (vrfiringrange's second row of monster buttons: maps/vr_spawnpanel.bsp on a
// func_wall, maps/vr_spawnbutton.bsp on a func_button; Misc/quakevr/make_spawn_buttons.py). Unlike the
// map's own models they must be precached before setmodel.
void() VR_PrecacheOwnModel =
{
    if(self.model != NULLSTR && substring(self.model, 0, 1) != "*")
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

    if(VRIsTouchingHandFake(entPlayer))
    {
        haptic(0, 0.0, 0.3, 75, 1.0);
        haptic(1, 0.0, 0.3, 75, 1.0);
        return;
    }

    haptic(VRGetTouchingHand(entPlayer), 0.0, 0.3, 75, 1.0);
}

void() button_wait =
{
    self.state = STATE_TOP;
    self.nextthink = self.ltime + self.wait;
    self.think = button_return;
    activator = self.enemy;
    SUB_UseTargets();
    self.frame = 1;            // use alternate textures
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


void() button_fire =
{
    if(self.state == STATE_UP || self.state == STATE_TOP || self.state == STATE_DISABLED)
    {
        return;
    }

    particle2(realorigin(self), '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 64);

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

    VRButtonPressHaptic(other);

    self.enemy = other;
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
        // they can activated by stepping on them.
        if(self.movedir_z == -1 || self.movedir_z == -2)
        {
            self.touch = button_touch;
        }

        self.handtouch = button_touch;
        self.vr_wpntouch = button_touch;
    }

    if(!self.speed)
        self.speed = 40;
    if(!self.wait)
        self.wait = 1;
    if(!self.lip)
        self.lip = 4;

    // HONEY
    if(self.items) {
        self.flags = self.flags | FL_SPECIFICDAMAGE;
    }

    self.state = STATE_BOTTOM;

    self.pos1 = self.origin;
    self.pos2 = self.pos1 + self.movedir*(fabs(self.movedir*self.size) - self.lip);

    // HONEY
    if(self.spawnflags & START_DISABLED) {
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
        WeaponIdToBaseClipSize(self.weapon) /* weaponclip */,
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
// shotgun, WID_GRUNTGUN 15, and an enforcer's laser rifle, WID_ENFRIFLE 16, full: Enemy Weapons' Shells, Shots).
void() func_weapon_grabbable =
{
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;
    setmodel(self, self.model);

    // Turned any way, still: it falls and settles on its drawn shape (a rigid body, MakeThrown).
    CreateThrownWeapon(self, self.weapon, self.weaponflags,
        WeaponIdToBaseClipSize(self.weapon) /* weaponclip */,
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

    return NULLSTR;
};

void() func_enemy_dispenser_use =
{
    vector org = self.origin;
    float choice = self.weapon;

    // Not installed: say what it needs instead.
    string mdl = VR_EnemyDispenser_Model(choice);
    if(mdl != NULLSTR && !fileexists(mdl))
    {
        if(activator.classname == "player")
        {
            if(choice >= 14)
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

    setorigin(self, org);
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
        // its sounds are precached (late) when it first spawns.
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
