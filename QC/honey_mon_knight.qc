void() knight_start =
{
    local entity n;

    if (self.spawnflags & TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = "monster_knight";
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;

        n.health = 75;

        n.th_stand = knight_stand1;
        n.th_walk = knight_walk1;
        n.th_run = knight_run1;
        n.th_melee = knight_atk1;
        n.th_pain = knight_pain;
        n.th_die = knight_die;
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

    setmodel (n, "progs/knight.mdl");

    setsize (n, '-16 -16 -24', '16 16 40');

    self = n;
    teleport_glitter();
    walkmonster_start ();
};
