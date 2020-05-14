/*
==============================================================================

LavaMan (Smaller version of Boss E1M7)

==============================================================================
*/
$frame rise1 rise2 rise3 rise4 rise5 rise6 rise7 rise8 rise9 rise10
$frame rise11 rise12 rise13 rise14 rise15 rise16 rise17

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15
$frame walk16 walk17 walk18 walk19 walk20 walk21 walk22
$frame walk23 walk24 walk25 walk26 walk27 walk28 walk29 walk30 walk31

$frame death1 death2 death3 death4 death5 death6 death7 death8 death9

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11 attack12 attack13 attack14 attack15
$frame attack16 attack17 attack18 attack19 attack20 attack21 attack22
$frame attack23

$frame shocka1 shocka2 shocka3 shocka4 shocka5 shocka6 shocka7 shocka8
$frame shocka9 shocka10

//$frame shockb1 shockb2 shockb3 shockb4 shockb5 shockb6

//$frame shockc1 shockc2 shockc3 shockc4 shockc5 shockc6 shockc7 shockc8
//$frame shockc9 shockc10

/*
===========
LavaManCheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
float() LavaManCheckAttack =
{
    vector spot1;
    entity targ;

    ai_face();

    targ = self.enemy;

// see if any entities are in the way of the shot
    spot1 = self.origin + '0 0 64';

    traceline (spot1, targ.origin, FALSE, self);

    if(trace_inopen && trace_inwater)
        return FALSE;            // sight line crossed contents
    if(trace_ent != targ)
        return FALSE;            // don't have a clear shot

// missile attack
    if(time < self.attack_finished)
        return FALSE;

    self.th_missile ();
    SUB_AttackFinished (1 + random());
    return TRUE;
};

// ============
// ============
void() lavaman_hunt =
{
    entity    possibleEnemy;
    vector    viewPoint;

// go for another player if multi player
    if(self.enemy.health <= 0)
    {
        viewPoint = self.origin + '0 0 96';
        possibleEnemy = find(self.enemy, classname, "player");
        traceline (viewPoint, possibleEnemy.origin, TRUE, world);
        if(trace_fraction == 1)
            self.enemy = possibleEnemy;
    }
    if(self.enemy)
    {
        ai_face();
        self.movetarget = self.enemy;
        self.goalentity = self.enemy;
    }
};
/*
void(float dist) lavaman_walk =
{
    lavaman_hunt();
    ai_walk(dist);
};
*/

void() lavaman_rise1    =[    $rise1, lavaman_rise2 ]
    { sound(self, CHAN_WEAPON, "boss1/out1.wav", 1, ATTN_NORM);
};
void() lavaman_rise2    =[    $rise2, lavaman_rise3 ]
    {
sound(self, CHAN_VOICE, "boss1/sight1.wav", 1, ATTN_NORM); };
void() lavaman_rise3    =[    $rise3, lavaman_rise4 ] {};
void() lavaman_rise4    =[    $rise4, lavaman_rise5 ] {};
void() lavaman_rise5    =[    $rise5, lavaman_rise6 ] {};
void() lavaman_rise6    =[    $rise6, lavaman_rise7 ] {};
void() lavaman_rise7    =[    $rise7, lavaman_rise8 ] {};
void() lavaman_rise8    =[    $rise8, lavaman_rise9 ] {};
void() lavaman_rise9    =[    $rise9, lavaman_rise10 ] {};
void() lavaman_rise10    =[    $rise10, lavaman_rise11 ] {};
void() lavaman_rise11    =[    $rise11, lavaman_rise12 ] {};
void() lavaman_rise12    =[    $rise12, lavaman_rise13 ] {};
void() lavaman_rise13    =[    $rise13, lavaman_rise14 ] {};
void() lavaman_rise14    =[    $rise14, lavaman_rise15 ] {};
void() lavaman_rise15    =[    $rise15, lavaman_rise16 ] {};
void() lavaman_rise16    =[    $rise16, lavaman_rise17 ] {};
void() lavaman_rise17    =[    $rise17, lavaman_fire1 ] {};

// ============
// ============
void() lavaman_stand =
{
    if(self.enemy)
        LavaManCheckAttack();
    else
        lavaman_hunt();

    ai_stand ();
};

void() lavaman_idle1    =[    $walk1, lavaman_idle2 ]
{
    if(random() < 0.2)
        sound(self, CHAN_VOICE, "boss1/sight1.wav", 1, ATTN_NORM);
// look for other players
};
void() lavaman_idle2    =[    $walk2, lavaman_idle3 ] {lavaman_stand();};
void() lavaman_idle3    =[    $walk3, lavaman_idle4 ] {lavaman_stand();};
void() lavaman_idle4    =[    $walk4, lavaman_idle5 ] {lavaman_stand();};
void() lavaman_idle5    =[    $walk5, lavaman_idle6 ] {lavaman_stand();};
void() lavaman_idle6    =[    $walk4, lavaman_idle7 ] {lavaman_stand();};
void() lavaman_idle7    =[    $walk3, lavaman_idle8 ] {lavaman_stand();};
void() lavaman_idle8    =[    $walk2, lavaman_idle9 ] {lavaman_stand();};
void() lavaman_idle9    =[    $walk1, lavaman_idle1 ] {lavaman_stand();};

// ============
// ============
void() lavaman_walk =
{
    if(self.enemy)
        LavaManCheckAttack();
    else
        lavaman_hunt();
    ai_walk (2);
};

void() lavaman_walk1    =[    $walk1, lavaman_walk2 ] {lavaman_walk();};
void() lavaman_walk2    =[    $walk2, lavaman_walk3 ] {lavaman_walk();};
void() lavaman_walk3    =[    $walk3, lavaman_walk4 ] {lavaman_walk();};
void() lavaman_walk4    =[    $walk4, lavaman_walk5 ] {lavaman_walk();};
void() lavaman_walk5    =[    $walk5, lavaman_walk6 ] {lavaman_walk();};
void() lavaman_walk6    =[    $walk6, lavaman_walk7 ] {lavaman_walk();};
void() lavaman_walk7    =[    $walk7, lavaman_walk8 ] {lavaman_walk();};
void() lavaman_walk8    =[    $walk8, lavaman_walk9 ] {lavaman_walk();};
void() lavaman_walk9    =[    $walk9, lavaman_walk10 ] {lavaman_walk();};
void() lavaman_walk10    =[    $walk10, lavaman_walk11 ] {lavaman_walk();};
void() lavaman_walk11    =[    $walk11, lavaman_walk12 ] {lavaman_walk();};
void() lavaman_walk12    =[    $walk12, lavaman_walk13 ] {lavaman_walk();};
void() lavaman_walk13    =[    $walk13, lavaman_walk14 ] {lavaman_walk();};
void() lavaman_walk14    =[    $walk14, lavaman_walk15 ] {lavaman_walk();};
void() lavaman_walk15    =[    $walk15, lavaman_walk16 ] {lavaman_walk();};
void() lavaman_walk16    =[    $walk16, lavaman_walk17 ] {lavaman_walk();};
void() lavaman_walk17    =[    $walk17, lavaman_walk18 ] {lavaman_walk();};
void() lavaman_walk18    =[    $walk18, lavaman_walk19 ] {lavaman_walk();};
void() lavaman_walk19    =[    $walk19, lavaman_walk20 ] {lavaman_walk();};
void() lavaman_walk20    =[    $walk20, lavaman_walk21 ] {lavaman_walk();};
void() lavaman_walk21    =[    $walk21, lavaman_walk22 ] {lavaman_walk();};
void() lavaman_walk22    =[    $walk22, lavaman_walk23 ] {lavaman_walk();};
void() lavaman_walk23    =[    $walk23, lavaman_walk24 ] {lavaman_walk();};
void() lavaman_walk24    =[    $walk24, lavaman_walk25 ] {lavaman_walk();};
void() lavaman_walk25    =[    $walk25, lavaman_walk26 ] {lavaman_walk();};
void() lavaman_walk26    =[    $walk26, lavaman_walk27 ] {lavaman_walk();};
void() lavaman_walk27    =[    $walk27, lavaman_walk28 ] {lavaman_walk();};
void() lavaman_walk28    =[    $walk28, lavaman_walk29 ] {lavaman_walk();};
void() lavaman_walk29    =[    $walk29, lavaman_walk30 ] {lavaman_walk();};
void() lavaman_walk30    =[    $walk30, lavaman_walk31 ] {lavaman_walk();};
void() lavaman_walk31    =[    $walk31, lavaman_walk1 ] {};

void() lavaman_run =
{
    if(self.enemy)
        LavaManCheckAttack();
    else
        lavaman_hunt();
    ai_run (2);
};

void() lavaman_run1    =[    $walk1, lavaman_run2 ] {lavaman_run();};
void() lavaman_run2    =[    $walk2, lavaman_run3 ] {lavaman_run();};
void() lavaman_run3    =[    $walk3, lavaman_run4 ] {lavaman_run();};
void() lavaman_run4    =[    $walk4, lavaman_run5 ] {lavaman_run();};
void() lavaman_run5    =[    $walk5, lavaman_run6 ] {lavaman_run();};
void() lavaman_run6    =[    $walk6, lavaman_run7 ] {lavaman_run();};
void() lavaman_run7    =[    $walk7, lavaman_run8 ] {lavaman_run();};
void() lavaman_run8    =[    $walk8, lavaman_run9 ] {lavaman_run();};
void() lavaman_run9    =[    $walk9, lavaman_run10 ] {lavaman_run();};
void() lavaman_run10 =[    $walk10, lavaman_run11 ] {lavaman_run ();};
void() lavaman_run11 =[    $walk11, lavaman_run12 ] {lavaman_run ();};
void() lavaman_run12 =[    $walk12, lavaman_run13 ] {lavaman_run ();};
void() lavaman_run13 =[    $walk13, lavaman_run14 ] {lavaman_run ();};
void() lavaman_run14 =[    $walk14, lavaman_run15 ] {lavaman_run ();};
void() lavaman_run15 =[    $walk15, lavaman_run16 ] {lavaman_run ();};
void() lavaman_run16 =[    $walk16, lavaman_run17 ] {lavaman_run ();};
void() lavaman_run17 =[    $walk17, lavaman_run18 ] {lavaman_run ();};
void() lavaman_run18 =[    $walk18, lavaman_run19 ] {lavaman_run ();};
void() lavaman_run19 =[    $walk19, lavaman_run20 ] {lavaman_run ();};
void() lavaman_run20 =[    $walk20, lavaman_run21 ] {lavaman_run ();};
void() lavaman_run21 =[    $walk21, lavaman_run22 ] {lavaman_run ();};
void() lavaman_run22 =[    $walk22, lavaman_run23 ] {lavaman_run ();};
void() lavaman_run23 =[    $walk23, lavaman_run24 ] {lavaman_run ();};
void() lavaman_run24 =[    $walk24, lavaman_run25 ] {lavaman_run ();};
void() lavaman_run25 =[    $walk25, lavaman_run26 ] {lavaman_run ();};
void() lavaman_run26 =[    $walk26, lavaman_run27 ] {lavaman_run ();};
void() lavaman_run27 =[    $walk27, lavaman_run28 ] {lavaman_run ();};
void() lavaman_run28 =[    $walk28, lavaman_run29 ] {lavaman_run ();};
void() lavaman_run29 =[    $walk29, lavaman_run30 ] {lavaman_run ();};
void() lavaman_run30 =[    $walk30, lavaman_run31 ] {lavaman_run ();};
void() lavaman_run31 =[    $walk31, lavaman_run1 ] {};

void() lavaman_fire1 =[ $attack1, lavaman_fire2 ] { ai_face();};
void() lavaman_fire2 =[ $attack2, lavaman_fire3 ] {ai_charge(1);};
void() lavaman_fire3 =[ $attack3, lavaman_fire4 ] { ai_face();};
void() lavaman_fire4 =[ $attack4, lavaman_fire5 ] {ai_face();};
void() lavaman_fire5 =[ $attack5, lavaman_fire6 ] {ai_charge(1);};
void() lavaman_fire6 =[ $attack6, lavaman_fire7 ] {ai_face();};
void() lavaman_fire7 =[ $attack7, lavaman_fire8 ] {lavaman_missile(1);};
void() lavaman_fire8 =[ $attack8, lavaman_fire9 ] {ai_face();};
void() lavaman_fire9 =[ $attack9, lavaman_fire10 ] {ai_face();};
void() lavaman_fire10 =[ $attack10, lavaman_fire11 ] {ai_charge(1);};
void() lavaman_fire11 =[ $attack11, lavaman_fire12 ] {ai_face();};
void() lavaman_fire12 =[ $attack12, lavaman_fire13 ] {ai_charge(1);};
void() lavaman_fire13 =[ $attack13, lavaman_fire14 ] {ai_face();};
void() lavaman_fire14 =[ $attack14, lavaman_fire15 ] {ai_face();};
void() lavaman_fire15 =[ $attack15, lavaman_fire16 ] {ai_charge(1);};
void() lavaman_fire16 =[ $attack16, lavaman_fire17 ] {ai_charge(1);};
void() lavaman_fire17 =[ $attack17, lavaman_fire18 ] {ai_face();};
void() lavaman_fire18 =[ $attack18, lavaman_fire19 ] {lavaman_missile(2);};
void() lavaman_fire19 =[ $attack19, lavaman_fire20 ] {ai_face();};
void() lavaman_fire20 =[ $attack20, lavaman_fire21 ] {ai_face();};
void() lavaman_fire21 =[ $attack21, lavaman_fire22 ] {ai_face();};
void() lavaman_fire22 =[ $attack22, lavaman_fire23 ] {ai_charge(1);};
void() lavaman_fire23 =[ $attack23, lavaman_run1 ] {};

void() lavaman_shocka1 =[ $shocka1, lavaman_shocka2 ] {};
void() lavaman_shocka2 =[ $shocka2, lavaman_shocka3 ] {};
void() lavaman_shocka3 =[ $shocka3, lavaman_shocka4 ] {};
void() lavaman_shocka4 =[ $shocka4, lavaman_shocka5 ] {};
void() lavaman_shocka5 =[ $shocka5, lavaman_shocka6 ] {};
void() lavaman_shocka6 =[ $shocka6, lavaman_shocka7 ] {};
void() lavaman_shocka7 =[ $shocka7, lavaman_shocka8 ] {};
void() lavaman_shocka8 =[ $shocka8, lavaman_shocka9 ] {};
void() lavaman_shocka9 =[ $shocka9, lavaman_shocka10 ] {};
void() lavaman_shocka10 =[ $shocka10, lavaman_run1 ] {};

void(entity attacker, float damage) lavaman_pain =
{
    if(self.pain_finished > time)
        return;

    if(random() < 0.05)
    {
        self.pain_finished = time + 2;
        lavaman_shocka1();
    }
};

void() lavaman_death1 = [$death1, lavaman_death2]
    {
sound(self, CHAN_VOICE, "boss1/death.wav", 1, ATTN_NORM);
};
void() lavaman_death2 = [$death2, lavaman_death3] {};
void() lavaman_death3 = [$death3, lavaman_death4] {};
void() lavaman_death4 = [$death4, lavaman_death5] {};
void() lavaman_death5 = [$death5, lavaman_death6] {};
void() lavaman_death6 = [$death6, lavaman_death7] {};
void() lavaman_death7 = [$death7, lavaman_death8] {};
void() lavaman_death8 = [$death8, lavaman_death9] {};
void() lavaman_death9 = [$death9, lavaman_death10]
{
    sound(self, CHAN_BODY, "boss1/out1.wav", 1, ATTN_NORM);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);
};

void() lavaman_death10 = [$death9, lavaman_death10]
{
    remove(self);
};

void() lavaman_touch =
{
    float    damg;

    if(other == self.owner)
        return;        // don't explode on owner

    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    damg = 40;
    if(other.health)
    {
        if(other.classname == "monster_shambler")
            damg = damg * 0.5;    // mostly immune
        T_Damage(other, self, self.owner, damg);
    }

    // don't do radius damage to the other, because all the damage
    // was done in the impact
    T_RadiusDamage(self, self.owner, 40, other);

//    sound(self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);
    self.origin = self.origin - 8*normalize(self.velocity);

// Change to an explosion2!!! Ramp in the non-diminishing reds...
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);

    BecomeExplosion ();
};

void(float AttackNumber) lavaman_missile =
{
    entity fireball;
    vector org, vec, d;
    float  t;

    makevectors(self.angles);

    // Right handed throw
    if(AttackNumber == 1)
    {
        org = self.origin + v_forward*40 + v_right*65 + v_up*130;
    }
    // Left handed throw
    else
    {
        ASSERT(AttackNumber == 2);
        org = self.origin + v_forward*40 - v_right*75 + v_up*125;
    }

    d = self.enemy.origin;
    t = vlen(d - org);
    t = t / 380;
    if(t > 1.75)
        t = 1.75;
    if(t < 1)
        t = 1;

    vec = normalize(d - org);

    fireball = spawn();
    fireball.owner = self;
//    fireball.movetype = MOVETYPE_FLYMISSILE;
    fireball.movetype = MOVETYPE_BOUNCE;
    fireball.solid = SOLID_BBOX;
    fireball.angles = vectoangles(vec);
    fireball.touch = lavaman_touch;
    fireball.classname = "lavaman_ball";
    setmodel(fireball, "progs/lavaball.mdl");
    setsize(fireball, '0 0 0', '0 0 0');
    setorigin(fireball, org);

//    fireball.velocity = vec * 600 + '0 0 200';
    fireball.velocity = vec * 600 * t;
    fireball.velocity_z = fireball.velocity_z + (200 * t);

    fireball.avelocity = '200 100 300';
    fireball.think = SUB_Remove;
    fireball.nextthink = time + 6;

    sound(self, CHAN_WEAPON, "boss1/throw.wav", 1, ATTN_NORM);

// check for dead enemy
    if(self.enemy.health <= 0)
        lavaman_idle1();
};

void() lavaman_awake =
{
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    self.takedamage = DAMAGE_AIM;
    self.flags = self.flags | FL_MONSTER;
    self.ideal_yaw = self.angles * '0 1 0';
    if(!self.yaw_speed)
        self.yaw_speed = 20;

    setmodel(self, "progs/lavaman.mdl");
//    setsize(self, '-64 -64 -12', '64 64 128');
//    setsize(self, '-32 -32 0', '32 32 120');
    setsize(self, VEC_HULL2_MIN, VEC_HULL2_MAX);
    self.view_ofs = '0 0 48';

//    self.health = 1500;

    self.health = 1250 + (250 * cvar_hget(cvarh_skill));

    self.th_stand = lavaman_idle1;
    self.th_walk = lavaman_walk1;
    self.th_run = lavaman_walk1;
    self.th_missile = lavaman_fire1;
    self.th_pain = lavaman_pain;
    self.th_die = lavaman_death1;
    self.th_melee = lavaman_fire1;

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);

    if(!VR_ItemUtil_EntHasItem(activator, IID_INVISIBILITY))
    {
        if(!(activator.flags & FL_NOTARGET))
        {
            if(activator.classname == "player")
                self.enemy = activator;
        }
    }
    droptofloor();

    lavaman_rise1();
};

//                                  (-64 -64 -12) (64 64 128)
/*QUAKED monster_lava_man (1 0 0) (-32 -32 -24) (32 32 64) Ambush Sleeping
*/
void() monster_lava_man =
{
    if(deathmatch)
    {
        remove(self);
        return;
    }

    precache_model("progs/lavaman.mdl");
    precache_model("progs/lavaball.mdl");

    precache_sound("weapons/rocket1i.wav");
    precache_sound("boss1/out1.wav");
    precache_sound("boss1/sight1.wav");
    precache_sound("misc/power.wav");
    precache_sound("boss1/throw.wav");
    precache_sound("boss1/pain.wav");
    precache_sound("boss1/death.wav");

    // HONEY
    monster_add_to_counter();

    // cause the lavaman to rise from the lava when triggered.
    if(self.spawnflags & 2)
    {
        self.use = lavaman_awake;
        return;
    }

    lavaman_awake();
};

