// Flak ogre - from Kinn
float MONOGRE_LAVAFLAK = 4;

$cd id1/models/ogre_c
$origin 0 0 24
$base base        
$skin base

$frame    stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7
$frame walk8 walk9 walk10 walk11 walk12 walk13 walk14 walk15 walk16

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame swing1 swing2 swing3 swing4 swing5 swing6 swing7
$frame swing8 swing9 swing10 swing11 swing12 swing13 swing14

$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7
$frame smash8 smash9 smash10 smash11 smash12 smash13 smash14

$frame shoot1 shoot2 shoot3 shoot4 shoot5 shoot6

$frame pain1 pain2 pain3 pain4 pain5

$frame painb1 painb2 painb3

$frame painc1 painc2 painc3 painc4 painc5 painc6

$frame paind1 paind2 paind3 paind4 paind5 paind6 paind7 paind8 paind9 paind10
$frame paind11 paind12 paind13 paind14 paind15 paind16

$frame paine1 paine2 paine3 paine4 paine5 paine6 paine7 paine8 paine9 paine10
$frame paine11 paine12 paine13 paine14 paine15

$frame death1 death2 death3 death4 death5 death6
$frame death7 death8 death9 death10 death11 death12
$frame death13 death14

$frame bdeath1 bdeath2 bdeath3 bdeath4 bdeath5 bdeath6
$frame bdeath7 bdeath8 bdeath9 bdeath10

$frame pull1 pull2 pull3 pull4 pull5 pull6 pull7 pull8 pull9 pull10 pull11

//BDW 31/08/00 - evil, nasty red-hot nail cluster-bomb attack...
void() FlakDoDamage = {
    self.origin = self.oldenemy.origin + self.oldorigin;    //get correct gib direction
    T_Damage(self.oldenemy, self, self.owner, self.oldenemy.spikecount,DAMARMOR);
    self.oldenemy.spikecount = 0;
    remove(self);
};

void() FlakTouch = {
    if (other.solid == SOLID_TRIGGER)
        return;

    if (pointcontents(self.origin) == CONTENT_SKY) {
        remove(self);
        return;
    }

    // hit something that bleeds
    if (other.takedamage) {        
        spawn_touchblood(self, other, self.dmg);
        sound(self, CHAN_VOICE, "weapons/skotch.wav", 1, ATTN_NORM);

        // not the first one...
        if (other.spikecount) {
            other.spikecount = other.spikecount + self.dmg;    
            remove(self);
            return;
        }

        // the first one...
        other.spikecount = self.dmg;    

        // stick around for a little while...
        self.velocity = '0 0 0';
        self.solid = SOLID_NOT;
        self.touch = SUB_Null;
        self.model = string_null;
        self.oldorigin = self.origin - other.origin;    //displacement from enemy origin (its gonna move next frame)
        self.oldenemy = other;        
        self.think = FlakDoDamage;
        self.nextthink = time + 0.05;
        return;
    }
    
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_SUPERSPIKE);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    //bit of a hack
    if (self.spawnflags & MONSTER_FLAK_OGRE) {
        remove(self);
        return;
    }

    self.dmg = self.dmg - 5;    //gets weaker with each bounce. also stops them getting stuck in world.
    if (self.dmg <= 0) {
        remove(self);
        return;
    }
    self.velocity = self.velocity * 0.5;    //reduce crazy ricochets
    self.movetype = MOVETYPE_BOUNCE;
};

void() BDW_OgreFireFlak = {
    local float flakcount;    
    local vector dir, ang;
    local entity flak;

    flakcount = 8;
    self.effects = self.effects | EF_MUZZLEFLASH;
    if (self.spawnflags & MONOGRE_LAVAFLAK) {
        sound(self, CHAN_WEAPON, "ogre/wnalfire_lava.wav", 1, ATTN_NORM);
    } else {
        sound(self, CHAN_WEAPON, "ogre/wnalfire.wav", 1, ATTN_NORM);
    }
    ang = vectoangles(self.enemy.origin - self.origin);
    makevectors(ang);    

    while (flakcount > 0) {
        //tighter spread...
        dir = v_forward*10 + crandom()*v_right + crandom()*v_up;
        dir = normalize(dir);
        dir_z = dir_z * -1;
        dir = dir*800;

        flak = spawn();
        flak.owner = self;
        //flymissile is a bit too all-seeing for this gun...
        flak.movetype = MOVETYPE_FLY;
        //flak.movetype = MOVETYPE_FLYMISSILE;
        flak.solid = SOLID_BBOX;
        flak.flags = 8192;
        flak.touch = FlakTouch;
        flak.angles = vectoangles(dir);
        flak.velocity = dir;
        flak.think = SUB_Remove;
        flak.nextthink = time + 6;
        
        flak.spawnflags = MONSTER_FLAK_OGRE;    //this is a hack to tell FlakTouch that it came from an ogre

        if (self.spawnflags & MONOGRE_LAVAFLAK) {
            setmodel(flak, "progs/flak.mdl");
            flak.dmg = 8;
        } else {
            setmodel(flak, "progs/spike.mdl");
            flak.dmg = 4;
        }
        setsize(flak, '0 0 0', '0 0 0');
        setorigin(flak, self.origin + '0 0 16');
        flakcount = flakcount - 1;
    }
};    

/*======================================================================
 Range - Flak (uses nails)
======================================================================*/
void() ogre_nail1    =[    $shoot1,        ogre_nail2    ] {ai_face();};
void() ogre_nail2    =[    $shoot2,        ogre_nail3    ] {ai_face();};
void() ogre_nail3    =[    $shoot2,        ogre_nail4    ] {ai_face();};
void() ogre_nail4    =[    $shoot3,        ogre_nail5    ] {ai_face();BDW_OgreFireFlak();};
void() ogre_nail5    =[    $shoot4,        ogre_nail6    ] {ai_face();};
void() ogre_nail6    =[    $shoot5,        ogre_nail7    ] {ai_face();};
void() ogre_nail7    =[    $shoot6,        ogre_run1    ] {ai_face();};

void() monster_ogre_spikeshooter = {
    if (deathmatch) { remove(self); return; }
        
    self.mdl = "progs/ad171/mon_ogre.mdl";        // Rogue Ogre
    self.headmdl = "progs/h_ogre.mdl";
    self.gib1mdl = "progs/ad171/w_chainsaw.mdl";    // Chainsaw
    self.gib2mdl = "progs/ad171/gib_ogfoot1.mdl";    // Left foot
    self.gib3mdl = "progs/ad171/gib_ogfoot2.mdl";    // Right foot
    self.gib4mdl = "progs/ad171/w_ogregl.mdl";    // Grenade Launcher

    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_GRENADE);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);
    precache_model (self.gib4mdl);

    self.gib1sound = GIB_IMPACT_WOOD;
    if (random() < 0.5) self.gib1mdl = self.gib4mdl;

    self.idle_sound = "ogre/ogidle.wav";
    precache_sound (self.idle_sound);
    self.idle_soundcom = "ogre/ogidle2.wav";
    precache_sound (self.idle_soundcom);

    precache_sound ("ogre/ogdth.wav");
    self.pain_sound = "ogre/ogpain1.wav";
    precache_sound (self.pain_sound);
    
    precache_sound ("ogre/ogdrag.wav");
    precache_sound ("ogre/ogsawatk.wav");

    self.sight_sound = "ogre/ogwake.wav";
    precache_sound (self.sight_sound);

    precache_model("progs/flak.mdl");
    precache_model("progs/spike.mdl");
    precache_sound("ogre/wnalfire.wav");
    precache_sound("ogre/wnalfire_lava.wav");
    precache_sound("weapons/skotch.wav");

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    self.idmins = VEC_HULL2_MIN;        // -32 -32 -24, 32 32 64
    self.idmaxs = VEC_HULL2_MAX;
    if (self.bboxtype < 1) self.bboxtype = BBOX_WIDE;
    self.gibhealth = -80;
    self.gibbed = FALSE;
    self.pain_flinch = 30;
    self.pain_longanim = TRUE;
    self.steptype = FS_TYPESLOW;
    self.meleerange = MONAI_MELEEOGRE;    // Chainsaw range
    self.no_zaware = FALSE;
    self.skin = 1;
    if (self.health < 1) self.health = 200;
    if (self.spawnflags & MONOGRE_LAVAFLAK) {
        self.health = self.health + MON_OGRE_GRNUPG;
    }
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_checkattack = OgreCheckAttack;
    self.th_stand = ogre_stand1;
    self.th_walk = ogre_walk1;
    self.th_run = ogre_run1;
    self.th_die = ogre_die;
    self.th_melee = ogre_melee;
    self.th_missile = ogre_nail1;
    self.th_pain = ogre_pain;
    self.th_wakeup = ogre_wakeup1;
    if (self.deathstring == "")  self.deathstring = " was skotched by a Flak Ogre\n";
    
    self.ammo_nails = 10;
    self.classtype = CT_MONOGRE;        // Old style ogres
    self.classgroup = CG_OGRE;        // One big happy ogre family!
    self.classmove = MON_MOVEWALK;

    monster_start();
};

void () monster_ogre_flak = {
    monster_ogre_spikeshooter();
};