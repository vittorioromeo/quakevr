
void() shalrath_start =
{
    local entity n;

    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = "monster_shalrath";
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;

        n.health = 400;

        n.th_stand = shal_stand;
        n.th_walk = shal_walk1;
        n.th_run = shal_run1;
        n.th_die = shalrath_die;
        n.th_pain = shalrath_pain;
        n.th_missile = shal_attack1;
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

    setmodel(n, "progs/shalrath.mdl");

    setsize(n, VEC_HULL2_MIN, VEC_HULL2_MAX);

    self = n;
    teleport_glitter();
    walkmonster_start();
};
