// New Items
//
// Items added for the Rogue XPACK.

void() newitems_touch =
{
    if(other.classname != "player")
        return;

    if(other.health <= 0)
        return;

    // only one per person, please.
    if(self.classname == "item_sphere")
    {
        if(VR_ItemUtil_EntHasItem(other, IID_V_SPHERE))
        {
            return;
        }
    }

    sprint(other, "You got the ");
    sprint(other, self.netname);
    sprint(other,"\n");

    if(deathmatch)
    {
        if(self.classname == "item_random_powerup")
        {
            self.nextthink = time + 60;
            self.think = random_regen;
        }
        else if(self.classname == "item_sphere")
        {
            self.mdl = self.model;
            self.nextthink = time + 60*3;
            self.think = SUB_regen;
        }
        else
        {
            self.mdl = self.model;
            self.nextthink = time + 60;
            self.think = SUB_regen;
        }
    }

    sound(other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    stuffcmd (other, "bf\n");
    self.solid = SOLID_NOT;
    VR_ItemUtil_EntAddItem(other, self.vr_itemId);
    self.model = string_null;

// do the apropriate action
    if(self.netname == "Power Shield")
    {
        other.shield_time = 1;
        other.shield_finished = time + 30;
    }
    else if(self.netname == "Anti-Grav Belt")
    {
        other.antigrav_time = 1;
        other.antigrav_finished = time + 45;
        other.gravity = 0.25;
    }
    else if(self.classname == "item_sphere")
    {
        VR_ItemUtil_EntAddItem(other, IID_V_SPHERE);
        sphere_spawn();
    }

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};


/*QUAKED item_powerup_shield (0 .5 .8) (-16 -16 -24) (16 16 32)
The shield upgrade
*/
void() item_powerup_shield =
{
    self.touch = newitems_touch;

    precache_model("progs/shield.mdl");
    precache_model("progs/p_shield.mdl");
    precache_sound("shield/pickup.wav");
    precache_sound("shield/hit.wav");
    precache_sound("shield/fadeout.wav");
    self.noise = "shield/pickup.wav";
    setmodel(self, "progs/shield.mdl");
    self.netname = "Power Shield";
    self.vr_itemId = IID_SHIELD;
    setsize(self, '-16 -16 -24', '16 16 32');
    StartItem();
};

/*QUAKED item_powerup_belt (0 .5 .8) (-16 -16 -24) (16 16 32)
The anti-grav belt
*/
void() item_powerup_belt =
{
    self.touch = newitems_touch;

    precache_model("progs/beltup.mdl");
    precache_sound("belt/pickup.wav");
    precache_sound("belt/use.wav");
    precache_sound("belt/fadeout.wav");
    self.noise = "belt/pickup.wav";
    setmodel(self, "progs/beltup.mdl");
    self.netname = "Anti-Grav Belt";
    self.vr_itemId = IID_ANTIGRAV;
    setsize(self, '-16 -16 -24', '16 16 32');
    StartItem();
};




