void() dog_start =
{
    local entity n;

    if (self.spawnflags & TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = "monster_dog";
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;

        n.health = 25;
        n.th_stand = dog_stand1;
        n.th_walk = dog_walk1;
        n.th_run = dog_run1;
        n.th_pain = dog_pain;
        n.th_die = dog_die;
        n.th_melee = dog_atta1;
        n.th_missile = dog_leap1;
        if(self.cnt <= -1){
            total_monsters = total_monsters + 1;
            WriteByte (MSG_ALL, SVC_UPDATESTAT);
            WriteByte (MSG_ALL, 12);
            WriteLong (MSG_ALL, total_monsters);
        }
    }else{
        n = self;
    }


    n.solid = SOLID_SLIDEBOX;
    n.movetype = MOVETYPE_STEP;

    setmodel (n, "progs/dog.mdl");

    setsize (n, '-32 -32 -24', '32 32 40');

    self = n;

    teleport_glitter();
    walkmonster_start();
};
