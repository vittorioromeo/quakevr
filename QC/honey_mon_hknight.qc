void() hell_knight_start =
{
    local entity n;

    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = "monster_hell_knight";
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;

        n.health = 250;

        n.th_stand = hknight_stand1;
        n.th_walk = hknight_walk1;
        n.th_run = hknight_run1;
        n.th_melee = hknight_melee;
        n.th_missile = hknight_magicc1;
        n.th_pain = hknight_pain;
        n.th_die = hknight_die;
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

    setmodel(n, "progs/hknight.mdl");

    setsize(n, '-16 -16 -24', '16 16 40');

    self = n;
    teleport_glitter();
    walkmonster_start();
};
