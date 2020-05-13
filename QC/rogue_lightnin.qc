// lightning trail
// pmack
// sept 96

// float ltrailLastUsed;        -- now an entity field.

float LT_TOGGLE =    1;
float LT_ACTIVE =     2;

void() ltrail_chain =
{
    SUB_UseTargets();

    self.think = SUB_Null;
};

void() ltrail_fire =
{
    entity myTarget;

    if(self.classname != "ltrail_end")
    {
        sound(self, CHAN_VOICE, "weapons/lhit.wav", 1, ATTN_NORM);
        myTarget = find(world, targetname, self.target);
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_LIGHTNING2);
        WriteEntity (MSG_BROADCAST, self);
        WriteByte(MSG_BROADCAST, 0); /* disambiguator */
        WriteCoord(MSG_BROADCAST, self.origin_x);
        WriteCoord(MSG_BROADCAST, self.origin_y);
        WriteCoord(MSG_BROADCAST, self.origin_z);
        WriteCoord(MSG_BROADCAST, myTarget.origin_x);
        WriteCoord(MSG_BROADCAST, myTarget.origin_y);
        WriteCoord(MSG_BROADCAST, myTarget.origin_z);
        LightningDamage(self.origin, myTarget.origin, self, self.currentammo);
    }

    if(self.items < time)
    {
        self.think = ltrail_chain;
        self.nextthink = time + self.frags;
    }
    else
    {
        self.think = ltrail_fire;
        self.nextthink = time + 0.05;
    }
};

void() ltrail_start_fire =
{
    // if it's a toggle ltrail, we ignore triggers from ltrail_end's
    // when toggled off.
    if(self.spawnflags & LT_TOGGLE)
    {
        // user is not a lightning trail - change activity state.
        if(other.classname != "ltrail_end")
        {
            if(self.spawnflags & LT_ACTIVE)
            // currently active
            {
                self.spawnflags = self.spawnflags - LT_ACTIVE;
                return;
            }
            else
            // not active
            {
                self.spawnflags = self.spawnflags + LT_ACTIVE;
            }
        }
        // user is lightning trail, but trail has been turned off.
        // ignore the message.
        else if(!(self.spawnflags & LT_ACTIVE))
            return;
    }

    if(self.classname ==  "ltrail_start")
    {
        self.items = time + self.weapon;
        ltrail_fire();
        self.ltrailLastUsed = time;
    }
    else if(self.classname == "ltrail_relay")
    {
        self.items = time + self.weapon;
        ltrail_fire();
    }
    else
    {
        self.think = ltrail_chain;
        self.nextthink = time + self.frags;
    }
};

/*QUAKED ltrail_start (0 1 0) (-8 -8 -8) (8 8 8) LT_TOGGLE
Starting point of a lightning trail.
Set currentammo to amount of damage you want the lightning to do.
Default is 25.

Set frags to amount of time before next item is triggered.
Default is 0.3 seconds.

Set weapon to amount of time to be firing the lightning.
Default is 0.3 seconds.

Set the LT_TOGGLE checkbox if you want the lightning shooter to continuously fire until triggered again.
*/
void() ltrail_start =
{
    self.ltrailLastUsed = time;

    precache_sound("weapons/lhit.wav");
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_BBOX;
    self.use = ltrail_start_fire;

    if(self.currentammo == 0)
        self.currentammo = 25;

    if(self.weapon == 0)
        self.weapon = 0.3;

    if(self.frags == 0)
        self.frags = 0.3;

    if(self.spawnflags & LT_ACTIVE)
    {
        self.items = time + 99999999;
        self.think = ltrail_fire;
        self.nextthink = time + 0.1;
    }
};

/*QUAKED ltrail_relay (0 1 0) (-8 -8 -8) (8 8 8)
Relay point of a lightning trail.
Set currentammo to amount of damage you want the lightning to do.
Default is 25.

Set frags to amount of time before next item is triggered.
Default is 0.3 seconds.

Set weapon to amount of time to be firing the lightning.
Default is 0.3 seconds.
*/
void() ltrail_relay =
{
    precache_sound("weapons/lhit.wav");
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_BBOX;
    self.use = ltrail_start_fire;

    if(self.currentammo == 0)
        self.currentammo = 25;

    if(self.weapon == 0)
        self.weapon = 0.3;

    if(self.frags == 0)
        self.frags = 0.3;
};

/*QUAKED ltrail_end (0 1 0) (-8 -8 -8) (8 8 8)
Ending point of a lightning trail.
Does not fire any lightning.

Set frags to amount of time before next item is triggered.
Default is 0.3 seconds.
*/
void() ltrail_end =
{
    precache_sound("weapons/lhit.wav");
    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_BBOX;
    self.use = ltrail_start_fire;

    if(self.currentammo == 0)
        self.currentammo = 25;

    if(self.weapon == 0)
        self.weapon = 0.3;

    if(self.frags == 0)
        self.frags = 0.3;
};
