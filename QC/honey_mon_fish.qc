void() fish_start =
{
    entity n;

    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = "monster_fish";
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;

        n.health = 25;

        n.th_stand = f_stand1;
        n.th_walk = f_walk1;
        n.th_run = f_run1;
        n.th_die = f_death1;
        n.th_pain = fish_pain;
        n.th_melee = f_attack1;

        if(self.cnt <= -1){
            total_monsters = total_monsters + 1;
            WriteByte(MSG_ALL, SVC_UPDATESTAT);
            WriteByte(MSG_ALL, 12);
            WriteLong (MSG_ALL, total_monsters);
        }
    }else{
        n = self;
    }

    n.solid = SOLID_SLIDEBOX;
    n.movetype = MOVETYPE_STEP;

    setmodel(n, "progs/fish.mdl");

    setsize(n, '-16 -16 -24', '16 16 24');

    self = n;
    teleport_glitter();
    swimmonster_start();
};
