
void() shambler_start =
{
    local entity n;

    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = "monster_shambler";
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;

        n.health = 600;

        n.th_stand = sham_stand1;
        n.th_walk = sham_walk1;
        n.th_run = sham_run1;
        n.th_die = sham_die;
        n.th_melee = sham_melee;
        n.th_missile = sham_magic1;
        n.th_pain = sham_pain;
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
    setmodel(n, "progs/shambler.mdl");

    setsize(n, VEC_HULL2_MIN, VEC_HULL2_MAX);

    self = n;
    teleport_glitter();
    walkmonster_start();
};
