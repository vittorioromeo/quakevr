void() demon1_start =
{
    entity n;

    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = "monster_demon1";
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;

        n.health = 300;
        n.th_stand = demon1_stand1;
        n.th_walk = demon1_walk1;
        n.th_run = demon1_run1;
        n.th_die = demon_die;
        n.th_melee = Demon_MeleeAttack;
        n.th_missile = demon1_jump1;
        n.th_pain = demon1_pain;

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

    setmodel(n, "progs/demon.mdl");

    setsize(n, VEC_HULL2_MIN, VEC_HULL2_MAX);

    self = n;
    teleport_glitter();
    walkmonster_start();
};
