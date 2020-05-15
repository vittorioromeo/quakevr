// button and multiple button

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
    if(self.effects == 1)
    {
        bprint("EFFECTS BUTTON!");
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
"effects" // VR
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

    bprint("state: ");
    bprint(ftos(self.state));
    bprint("| lip: ");
    bprint(ftos(self.lip));
    bprint("| b: ");
    bprint(ftos(startIdx));
    bprint("| e: ");
    bprint(ftos(endIdx));
    bprint("\n");

    string newText = substr(self.worldtext, startIdx, endIdx);
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
        if(nthchar(s, i) == '$')
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
