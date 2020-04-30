// elevator button
// pmack
// sept 96

float ELVTR_DOWN = 1;

void() elvtr_button_wait;
void() elvtr_button_return;

void() elvtr_button_wait =
{
    elv_button_dir = 0;
    if (self.spawnflags & ELVTR_DOWN)
        elv_button_dir = -1;
    else
        elv_button_dir = 1;
        
    self.state = STATE_TOP;
    self.nextthink = self.ltime + self.wait;
    self.think = elvtr_button_return;
    activator = self.enemy;
    SUB_UseTargets();
    self.frame = 1;            // use alternate textures
};

void() elvtr_button_done =
{
    self.state = STATE_BOTTOM;
};

void() elvtr_button_return =
{
    self.state = STATE_DOWN;
    SUB_CalcMove (self.pos1, self.speed, elvtr_button_done);
    self.frame = 0;            // use normal textures
    if (self.health)
        self.takedamage = DAMAGE_YES;    // can be shot again
};


void() elvtr_button_blocked =
{    // do nothing, just don't ome all the way back out
};


void() elvtr_button_fire =
{
    if (self.state == STATE_UP || self.state == STATE_TOP)
        return;

    sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

    self.state = STATE_UP;
    SUB_CalcMove (self.pos2, self.speed, elvtr_button_wait);
};


void() elvtr_button_use =
{
    self.enemy = activator;
    elvtr_button_fire ();
};

void() elvtr_button_touch =
{
    if (other.classname != "player")
        return;
    self.enemy = other;
    elvtr_button_fire ();
};

void() elvtr_button_killed =
{
    self.enemy = damage_attacker;
    self.health = self.max_health;
    self.takedamage = DAMAGE_NO;    // wil be reset upon return
    elvtr_button_fire ();
};


/*QUAKED func_elvtr_button (0 .5 .8) ? ELVTR_DOWN
ELEVATOR BUTTON ONLY!

ELVTR_DOWN causes this to be a DOWN button.
Default is UP.

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
*/
void() func_elvtr_button = {
    if (self.sounds == 0)
    {
        precache_sound ("buttons/airbut1.wav");
        self.noise = "buttons/airbut1.wav";
    }
    if (self.sounds == 1)
    {
        precache_sound ("buttons/switch21.wav");
        self.noise = "buttons/switch21.wav";
    }
    if (self.sounds == 2)
    {
        precache_sound ("buttons/switch02.wav");
        self.noise = "buttons/switch02.wav";
    }
    if (self.sounds == 3)
    {
        precache_sound ("buttons/switch04.wav");
        self.noise = "buttons/switch04.wav";
    }
    
    SetMovedir ();

    self.movetype = MOVETYPE_PUSH;
    self.solid = SOLID_BSP;
    setmodel (self, self.model);

    self.blocked = elvtr_button_blocked;
    self.use = elvtr_button_use;

    if (self.health)
    {
        self.max_health = self.health;
        self.th_die = elvtr_button_killed;
        self.takedamage = DAMAGE_YES;
    }
    else
        self.touch = elvtr_button_touch;

    if (!self.speed)
        self.speed = 40;
    if (!self.wait)
        self.wait = 1;
    if (!self.lip)
        self.lip = 4;

    self.state = STATE_BOTTOM;

    self.pos1 = self.origin;
    self.pos2 = self.pos1 + self.movedir*(fabs(self.movedir*self.size) - self.lip);
};

