void() scrag_start =
{
    local entity n;

    if (self.spawnflags & TRIGGER_SPAWN_MULTIPLE && self.cnt != 0)
    {
        self.cnt = self.cnt - 1;
        n = spawn();
        setorigin(n, self.origin);
        n.angles = self.angles;
        n.classname = "monster_wizard";
        n.spawnflags = self.spawnflags;
        n.target = self.target;
        n.killtarget = self.killtarget;

        n.health = 80;

        n.th_stand = wiz_stand1;
        n.th_walk = wiz_walk1;
        n.th_run = wiz_run1;
        n.th_missile = Wiz_Missile;
        n.th_pain = Wiz_Pain;
        n.th_die = wiz_die;
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

    setmodel (n, "progs/wizard.mdl");

    setsize (n, '-16 -16 -24', '16 16 40');

    self = n;
    teleport_glitter();
    flymonster_start ();
};
