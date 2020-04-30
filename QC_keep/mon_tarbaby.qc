/*==============================================================================
BLOB (annoying)
==============================================================================*/
$cd id1/models/tarbaby
$origin 0 0 24
$base base        

$skin skin

$frame walk1 walk2 walk3 walk4  walk5 walk6 walk7 walk8 walk9 walk10
$frame walk11 walk12 walk13 walk14 walk15 walk16 walk17 walk18 walk19
$frame walk20 walk21 walk22 walk23 walk24 walk25

$frame run1 run2 run3 run4 run5 run6  run7 run8 run9 run10 run11 run12 run13
$frame run14 run15 run16 run17 run18 run19 run20 run21 run22 run23
$frame run24 run25

$frame jump1 jump2 jump3 jump4 jump5 jump6

$frame fly1 fly2 fly3 fly4

$frame exp

void()    tbaby_jump1;
void()    tbaby_jump5;

//============================================================================
void()    tbaby_stand1    =[    $walk1,        tbaby_stand2    ] {ai_stand();};
void()    tbaby_stand2    =[    $walk2,        tbaby_stand3    ] {ai_stand();};
void()    tbaby_stand3    =[    $walk3,        tbaby_stand4    ] {ai_stand();};
void()    tbaby_stand4    =[    $walk4,        tbaby_stand5    ] {ai_stand();};
void()    tbaby_stand5    =[    $walk5,        tbaby_stand6    ] {ai_stand();
self.nextthink = 0.1 + random()*0.3;};
void()    tbaby_stand6    =[    $walk4,        tbaby_stand7    ] {ai_stand();};
void()    tbaby_stand7    =[    $walk3,        tbaby_stand8    ] {ai_stand();};
void()    tbaby_stand8    =[    $walk2,        tbaby_stand1    ] {ai_stand();
self.nextthink = 0.1 + random()*0.3;};

void()    tbaby_hang1        =[    $walk1,        tbaby_hang1    ] {ai_stand();};

//============================================================================
void()    tbaby_walk1    =[    $walk1,        tbaby_walk2    ] {ai_turn();monster_idle_sound();};
void()    tbaby_walk2    =[    $walk2,        tbaby_walk3    ] {ai_turn();};
void()    tbaby_walk3    =[    $walk3,        tbaby_walk4    ] {ai_turn();};
void()    tbaby_walk4    =[    $walk4,        tbaby_walk5    ] {ai_turn();};
void()    tbaby_walk5    =[    $walk5,        tbaby_walk6    ] {ai_turn();};
void()    tbaby_walk6    =[    $walk6,        tbaby_walk7    ] {ai_turn();};
void()    tbaby_walk7    =[    $walk7,        tbaby_walk8    ] {ai_turn();};
void()    tbaby_walk8    =[    $walk8,        tbaby_walk9    ] {ai_turn();};
void()    tbaby_walk9    =[    $walk9,        tbaby_walk10] {ai_turn();};
void()    tbaby_walk10=[    $walk10,    tbaby_walk11] {ai_turn();};
void()    tbaby_walk11=[    $walk11,    tbaby_walk12] {ai_walk(2);};
void()    tbaby_walk12=[    $walk12,    tbaby_walk13] {ai_walk(2);};
void()    tbaby_walk13=[    $walk13,    tbaby_walk14] {ai_walk(2);};
void()    tbaby_walk14=[    $walk14,    tbaby_walk15] {ai_walk(2);};
void()    tbaby_walk15=[    $walk15,    tbaby_walk16] {ai_walk(2);};
void()    tbaby_walk16=[    $walk16,    tbaby_walk17] {ai_walk(2);};
void()    tbaby_walk17=[    $walk17,    tbaby_walk18] {ai_walk(2);};
void()    tbaby_walk18=[    $walk18,    tbaby_walk19] {ai_walk(2);};
void()    tbaby_walk19=[    $walk19,    tbaby_walk20] {ai_walk(2);};
void()    tbaby_walk20=[    $walk20,    tbaby_walk21] {ai_walk(2);};
void()    tbaby_walk21=[    $walk21,    tbaby_walk22] {ai_walk(2);};
void()    tbaby_walk22=[    $walk22,    tbaby_walk23] {ai_walk(2);};
void()    tbaby_walk23=[    $walk23,    tbaby_walk24] {ai_walk(2);};
void()    tbaby_walk24=[    $walk24,    tbaby_walk25] {ai_walk(2);};
void()    tbaby_walk25=[    $walk25,    tbaby_walk1    ] {ai_walk(2);};

//============================================================================
void()    tbaby_run1    =[    $run1,        tbaby_run2    ] {ai_face();};
void()    tbaby_run2    =[    $run2,        tbaby_run3    ] {ai_face();};
void()    tbaby_run3    =[    $run3,        tbaby_run4    ] {ai_face();};
void()    tbaby_run4    =[    $run4,        tbaby_run5    ] {ai_face();};
void()    tbaby_run5    =[    $run5,        tbaby_run6    ] {ai_face();};
void()    tbaby_run6    =[    $run6,        tbaby_run7    ] {ai_face();};
void()    tbaby_run7    =[    $run7,        tbaby_run8    ] {ai_face();};
void()    tbaby_run8    =[    $run8,        tbaby_run9    ] {ai_face();};
void()    tbaby_run9    =[    $run9,        tbaby_run10    ] {ai_face();};
void()    tbaby_run10    =[    $run10,        tbaby_run11    ] {ai_face();};
void()    tbaby_run11    =[    $run11,        tbaby_run12    ] {ai_run(2);};
void()    tbaby_run12    =[    $run12,        tbaby_run13    ] {ai_run(2);};
void()    tbaby_run13    =[    $run13,        tbaby_run14    ] {ai_run(2);};
void()    tbaby_run14    =[    $run14,        tbaby_run15    ] {ai_run(2);};
void()    tbaby_run15    =[    $run15,        tbaby_run16    ] {ai_run(2);};
void()    tbaby_run16    =[    $run16,        tbaby_run17    ] {ai_run(2);};
void()    tbaby_run17    =[    $run17,        tbaby_run18    ] {ai_run(2);};
void()    tbaby_run18    =[    $run18,        tbaby_run19    ] {ai_run(2);};
void()    tbaby_run19    =[    $run19,        tbaby_run20    ] {ai_run(2);};
void()    tbaby_run20    =[    $run20,        tbaby_run21    ] {ai_run(2);};
void()    tbaby_run21    =[    $run21,        tbaby_run22    ] {ai_run(2);};
void()    tbaby_run22    =[    $run22,        tbaby_run23    ] {ai_run(2);};
void()    tbaby_run23    =[    $run23,        tbaby_run24    ] {ai_run(2);};
void()    tbaby_run24    =[    $run24,        tbaby_run25    ] {ai_run(2);};
void()    tbaby_run25    =[    $run25,        tbaby_run1    ] {ai_run(2);};

//============================================================================
void()    Tar_JumpTouch =
{
    local    float    ldmg;

    if (self.health <= 0) return;
    ai_jumpbreakable(20);            // Damage any breakables

    if (other.takedamage && other.classname != self.classname) {
        if ( vlen(self.velocity) > 400 ) {
            ldmg = 10 + 10*random();
            // Can only poison the player
            if (other.flags & FL_CLIENT && self.poisonous) {
                // Check if player is posioned already?
                if (other.poisoned) T_Damage (other, self, self, ldmg * 0.25, DAMARMOR);
                // Make sure player is posioned
                else PoisonDeBuff(other);
            }
            // Everything else gets the usual tarbaby thump!
            else T_Damage (other, self, self, ldmg, DAMARMOR);
            sound (self, CHAN_WEAPON, "blob/hit1.wav", 1, ATTN_NORM);
        }
    }
    else
        sound (self, CHAN_WEAPON, "blob/land1.wav", 1, ATTN_NORM);

    // Is the tarbaby floating in the air?
    if (!checkbottom(self)) {
        // Is the tarbaby standing on something?        
        if (self.flags & FL_ONGROUND) {    
            self.touch = SUB_Null;
            self.think = tbaby_run1;
            self.movetype = MOVETYPE_STEP;
            self.nextthink = time + 0.1;
        }
        return;    // not on ground yet
    }

    self.touch = SUB_Null;
    if (self.spawnflags & MON_TARBYLESSJUMP) {
        // RRP - Ijed (reduce constant jumping around like id version)
        // Skill percentage Easy 0% = never jump, Nightmare 60% = good chance
        if (random() > skill / 5) self.think =  tbaby_run1;
        else self.think = tbaby_jump1;
    }
    // Original ID behaviour, constantly jump around like crazy
    else self.think = self.th_jump;
    self.nextthink = time + 0.1;
};

//============================================================================
void()    tbaby_fly1        =[    $fly1,    tbaby_fly2    ] {};
void()    tbaby_fly2        =[    $fly2,    tbaby_fly3    ] {};
void()    tbaby_fly3        =[    $fly3,    tbaby_fly4    ] {};
void()    tbaby_fly4        =[    $fly4,    tbaby_fly1    ] { 
    self.cnt = self.cnt + 1;
    if (self.cnt == 4) {
    tbaby_jump5 ();
    }
};

//============================================================================
void()    tbaby_jump1        =[    $jump1,    tbaby_jump2        ] {ai_face();};
void()    tbaby_jump2        =[    $jump2,    tbaby_jump3        ] {ai_face();};
void()    tbaby_jump3        =[    $jump3,    tbaby_jump4        ] {ai_face();};
void()    tbaby_jump4        =[    $jump4,    tbaby_jump5        ] {ai_face();};
void()    tbaby_jump5        =[    $jump5,    tbaby_jump6        ]
{    
    self.movetype = MOVETYPE_BOUNCE;
    self.touch = Tar_JumpTouch;
    makevectors (self.angles);
    self.origin_z = self.origin_z + 1;
    self.velocity = v_forward * 600 + '0 0 200';
    self.velocity_z = self.velocity_z + random()*150;
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.cnt = 0;
};
void()    tbaby_jump6    =[ $jump6, tbaby_fly1 ] {};

//============================================================================
void(entity inflictor, entity attacker, float damage)    tbaby_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
    }
};

//=============================================================================
void() tbaby_die1 =[ $exp, tbaby_die2 ] {};
void() tbaby_die2 =[ $exp, tbaby_run1 ] {
    // Tarbabies explode when they die
    T_RadiusDamage (self, self, self.death_dmg, world, DAMAGEALL);

    sound (self, CHAN_BODY, "blob/death1.wav", 1, ATTN_NORM);
    self.origin = self.origin - 8*normalize(self.velocity);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);    // Hard coded explosion
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);
    // Sprite explosion for Fitz engines
    // Check for DP particle system as visuals is done with TE_TAREXPLOSION
    if (!ext_dppart) SpawnExplosion(EXPLODE_SMALL, self.origin, SOUND_REXP3);
    else sound(self, CHAN_WEAPON, SOUND_REXP3, 1, ATTN_NORM);
    // Hide monster, do not remove, still playing sound
    entity_hide(self);
};

//----------------------------------------------------------------------
void() tbaby_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    self.takedamage = DAMAGE_NO;
    tbaby_die1();
};

//=============================================================================
//QUAKED monster_tarbaby (1 0 0) (-16 -16 -24) (16 16 24) Ambush x Lessjump
//=============================================================================
void() monster_tarbaby =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_tarbaby.mdl";
    self.headmdl = string_null;
    precache_model (self.mdl);

    self.idle_sound = "blob/sight1.wav";
    precache_sound (self.idle_sound);
    self.sight_sound = self.idle_sound;
    self.pain_sound = self.idle_sound;

    precache_sound ("blob/death1.wav");
    
    precache_sound ("blob/hit1.wav");
    precache_sound ("blob/land1.wav");
    
    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 80;
    self.gibhealth = MON_NEVERGIB;        // Cannot be gibbed by weapons
    self.gibbed = FALSE;
    self.pain_flinch = 20;
    
    // Allow for extra functionality (death damage and poison)
    if (self.death_dmg < 1) self.death_dmg = DAMAGE_TARBABY;
    if (self.poisonous < 1) self.poisonous = FALSE;
    else self.poisonous = TRUE;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    if (self.deathstring == "") self.deathstring = " was slimed by a Spawn\n";

    self.th_checkattack = SpawnCheckAttack;
    self.th_stand = tbaby_stand1;
    self.th_walk = tbaby_walk1;
    self.th_run = tbaby_run1;
    self.th_jump = tbaby_jump1;
    self.th_melee = tbaby_jump1;
    self.th_pain = tbaby_pain;
    self.th_die = tbaby_die;
    
    self.classtype = CT_MONSPAWN;
    self.classgroup = CG_SPAWN;
    self.classmove = MON_MOVEWALK;
    
    monster_start();
};