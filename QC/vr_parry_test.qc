// Focused parry tests, available in Debug > Tests. Invoked as spawn functions by vr_physics_spawn.
// They use the real damage/guard and dragon tail paths, with only target/route setup made deterministic.
void() VR_Parry_DragonObserve =
{
    entity m = self.owner;
    dprint(sprintf("dragon parry test: recovered %g, attacking %g, route %g, moved %g, next think %g\n",
        m.vr_parry_stagger_until == 0, m.dragonAttacking, m.movetarget == self,
        m.origin != self.pos1, m.nextthink > time));
};

void() VR_Parry_DragonTest =
{
    entity p = find(world, classname, "player");
    if(!p || !fileexists("progs/dragon.mdl"))
    {
        dprint("dragon parry test: needs the player and Dissolution of Eternity\n");
        return;
    }
    makevectors('0 1 0' * p.v_viewangle_y);
    setorigin(self, p.origin + v_forward * 120);
    self.classname = "monster_dragon";
    monster_dragon();
    self.flags = self.flags | FL_MONSTER | FL_FLY;
    self.takedamage = DAMAGE_AIM;
    self.solid = SOLID_NOT; // the test may place the large dragon close to the player
    self.angles_y = p.v_viewangle_y + 180;
    self.enemy = p;
    self.dragonAttacking = TRUE;
    entity route = spawn();
    route.classname = "vr_parry_dragon_route";
    setorigin(route, self.origin + '0 300 0');
    route.owner = self;
    route.think = VR_Parry_DragonObserve;
    route.nextthink = time + min(3, max(0.1, cvar_hget(cvarh_vr_parry_stagger))) + 0.2;
    self.movetarget = route;
    self.goalentity = route;
    self.think = dragon_melee11;
    self.nextthink = time + 0.1;
    p.health = 500;
    dragon_tail();
    route.pos1 = self.origin;
    float first = p.health;
    dragon_tail_touch(); // another damaging callback in the very same frame
    dragon_tail();
    dprint(sprintf("dragon parry test: first health %g, after follow-ups %g, interrupted %g, attacking %g, recovery think %g\n",
        first, p.health, self.vr_parry_stagger_until > time, self.dragonAttacking,
        self.think == VR_Parry_Recover));
};

void() VR_Parry_SameCallbackTest =
{
    entity p = find(world, classname, "player");
    if(!p)
        return;
    makevectors('0 1 0' * p.v_viewangle_y);
    setorigin(self, p.origin + v_forward * 48);
    self.classname = "monster_knight";
    monster_knight();
    self.flags = self.flags | FL_MONSTER;
    self.enemy = p;
    self.solid = SOLID_NOT;
    self.think = knight_atk7;
    self.nextthink = time + 0.1;
    p.health = 500;
    entity beforeSelf = self;
    v_forward = '1 2 3';
    v_right = '4 5 6';
    v_up = '7 8 9';
    T_Damage(p, self, self, 10);
    float first = p.health;
    float restored = self == beforeSelf && v_forward == '1 2 3' && v_right == '4 5 6' && v_up == '7 8 9';
    T_Damage(p, self, self, 10);
    T_Damage(p, self, self, 10);
    dprint(sprintf("parry callback test: first health %g, after follow-ups %g, self/vectors restored %g, recovery think %g\n",
        first, p.health, restored, self.think == VR_Parry_Recover));
};
