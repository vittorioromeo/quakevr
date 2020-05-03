void() trigger_spawn_use =
{
    self.sounds = 0;
    self.targetname = "";
    SUB_DoLater(self.think1, 0.1);
}

void() trigger_once_spawn =
{
    self.think1 = trigger_once;
    self.use = trigger_spawn_use;
}

void() trigger_multiple_spawn =
{
    self.think1 = trigger_multiple;
    self.use = trigger_spawn_use;
}

void() trigger_monsterjump_spawn =
{
    self.think1 = trigger_monsterjump;
    self.use = trigger_spawn_use;
}

void() relay_many_think =
{
    activator = self.enemy;
    SUB_UseTargets();
    self.cnt = self.cnt - 1;
    if(self.cnt > 0)
    {
        self.nextthink = time + self.wait;
    }
}

void() relay_many_use =
{
    if(self.cnt > 0) return;
    self.enemy = activator;
    self.cnt = self.count;
    relay_many_think();
}

/*QUAKED trigger_relay_many (.5 .5 .5) (-8 -8 -8) (8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.  It can contain killtargets, targets, delays, and messages.
*/
void() trigger_relay_many =
{
    self.use = relay_many_use;
    self.think = relay_many_think;
    if(!self.count) self.count = 3;
    if(!self.wait) self.wait = 1;
};

void() trigger_skill_use =
{
    cvar_set ("skill", self.message);
};

void() target_setskill =
{
    self.use = trigger_skill_use;
};

void() trigger_babyhop_touch =
{
    if(other.classname != "monster_tarbaby")
        return;


    other.velocity_z = other.velocity_z * -1;
}

void() trigger_babyhop =
{
    self.touch = trigger_babyhop_touch;
    InitTrigger();
}

void() trigger_babyboost_touch =
{
    if(other.classname != "monster_tarbaby")
        return;

    //dprint("touched by an angel!\n");
    other.distance = self.speed;
}

void() trigger_babyboost =
{
    if(!self.speed) self.speed = 500;
    InitTrigger();
    self.touch = trigger_babyboost_touch;
}
