/*==============================================================================
ENFORCER
==============================================================================*/
$cd id1/models/enforcer
$origin 0 -6 24
$base base        
$skin skin

$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10
$frame walk11 walk12 walk13 walk14 walk15 walk16

$frame run1 run2 run3 run4 run5 run6 run7 run8

$frame attack1 attack2 attack3 attack4 attack5 attack6
$frame attack7 attack8 attack9 attack10

$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12 death13 death14

$frame fdeath1 fdeath2 fdeath3 fdeath4 fdeath5 fdeath6 fdeath7 fdeath8
$frame fdeath9 fdeath10 fdeath11

$frame paina1 paina2 paina3 paina4

$frame painb1 painb2 painb3 painb4 painb5

$frame painc1 painc2 painc3 painc4 painc5 painc6 painc7 painc8

$frame paind1 paind2 paind3 paind4 paind5 paind6 paind7 paind8
$frame paind9 paind10 paind11 paind12 paind13 paind14 paind15 paind16
$frame paind17 paind18 paind19
//============================================================================

void menf_ratk9();

void() enforcer_shieldthink = {
    if (self.owner.health <= 0)
    {
        remove(self);
        return;
    }
    local float strength, i;
    local vector offset, org;

    org = self.owner.origin;

    self.angles = '0 0 0';
    self.origin = org;    // apparently I'm not supposed to do that

    strength = floor( self.owner.armorvalue / 8 );

    if (strength > 0) {
        for (i = 0; i < 5; i++) {
            self.angles_x = time * 140;
            self.angles_y = 360 * (i / 5);
            makevectors(self.angles);
            offset = v_forward * 20 + v_up * 3 + org;
//            particle (offset + '0 0 -24', '0 0 0', 227 + strength*2, strength);
            particle (offset, v_forward * 2, 227 + strength*2, strength);
//            particle (offset + '0 0 40', '0 0 0', 227 + strength*2, strength);
        }
    }
    self.nextthink = time + 0.1;
}

void() enforcer_spawnshieldfx = {
    local entity shieldfx;
    shieldfx = spawn();
//    bprint ("Shield spawnded!\n");
    shieldfx.owner = self;
    shieldfx.movetype = MOVETYPE_NONE;
    setorigin (shieldfx, self.origin);
    setsize( shieldfx, '0 0 0', '0 0 0');
    shieldfx.nextthink = time + 0.05;
    shieldfx.think = enforcer_shieldthink;
}

void() enforcer_updateshield = {
    if (self.spawnflags & ULTRA_ENF) {
        self.armorvalue = self.armorvalue + 2 + skill;
        if (self.armorvalue > 30)
            self.armorvalue = 30;
//        bprint (ftos(self.armorvalue));
//        bprint ("\n");
    }
}

// Yoinked from Zer

void() particlethink =
{
    if (self.wait < time)
        remove(self);
    particle (self.origin, self.oldorigin, 10, 4);
    particle (self.origin, self.oldorigin, 46, 6);
    self.nextthink = time + 0.05;
};

void() enforcer_fireparticlebeam =
{
local vector org, mep;
local    entity missile;
local vector lead, spot;
local float go, dot;

    self.effects = self.effects | EF_MUZZLEFLASH;
    makevectors (self.angles);
    
    org = self.origin + v_forward * 30 + v_right * 8.5 + '0 0 16';

    sound (self, CHAN_WEAPON, "enforcer/me_fire1.wav", 1, ATTN_NORM);
        
    go = vlen(self.enemy.origin - org) / 400;
    mep = self.enemy.velocity;
    mep_z = 0;
    spot = self.enemy.origin + mep * go;

    traceline(org, spot, FALSE, world);
    if (trace_fraction < 0.8 || vlen(self.origin - self.enemy.origin) > 768) {
        // Not a clear lead shot, shoot at player
        spot = self.enemy.origin;
    }
    dot = (normalize(spot - org) * v_forward);
    if ( dot < 0.75 ) {
        // Too wacky an angle, just shoot at player
        spot = self.enemy.origin;
    }
    lead = normalize(spot - org);

    missile = spawn ();
    missile.owner = self;
    missile.enemy = self.enemy;
    missile.movetype = MOVETYPE_FLY;
    missile.solid = SOLID_BBOX;
    missile.effects = EF_DIMLIGHT; 

    setmodel (missile, "progs/laser2.mdl");
    setorigin (missile, org);
    setsize (missile, '0 0 0', '0 0 0');        

    missile.velocity = lead * 500;
    missile.angles = vectoangles(missile.velocity);
    missile.touch = Laser2_Touch;
    
    missile.wait = time + 5;
    missile.nextthink = time + 0.05;
    missile.think = particlethink;
};

void() enforcer_missiletouch =
{
    local float    damg;

    if (other == self.owner)
        return;        // don't explode on owner
    if (pointcontents(self.origin) == CONTENT_SKY)    {
        remove(self);
        return;
    }

    damg = 50 + random()*20;
    if (other.health)
    {
        if (other.classname == "monster_enforcer")
            damg = damg * 0.5;    // not so rough on compatriots
        T_Damage (other, self, self.owner, damg , DAMARMOR);
    }
    T_RadiusDamage (self, self.owner, damg, other, IGNORECLASS);
    self.origin = self.origin - 8*normalize(self.velocity);

    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord (MSG_BROADCAST, self.origin_x);
    WriteCoord (MSG_BROADCAST, self.origin_y);
    WriteCoord (MSG_BROADCAST, self.origin_z);

    BecomeExplosion ();
};

void() enforcer_firerocket =
{
    local vector org;
    local    entity missile;
    local vector mep;
    local vector lead, spot;
    local float go;

    self.effects = self.effects | EF_MUZZLEFLASH;
    makevectors (self.angles);
    
    org = self.origin + v_forward * 30 + v_right * 8.5 + '0 0 16';
    sound (self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);

    go = vlen(self.enemy.origin - org) / 500;
    mep = self.enemy.velocity;
    mep_z = 0;
    spot = self.enemy.origin + mep * go;

    traceline(org, spot, FALSE, world);
    if (trace_fraction < 0.8)
    {
        //dprint("Not a clear lead shot, shoot at player.\n");
        spot = self.enemy.origin;
    }

    lead = normalize(spot - org);
    traceline(org, spot, FALSE, world);
    if ( trace_fraction < 0.8 ) {
        // Something got in the way, skip it
        menf_ratk9();
        return;
    }

    missile = spawn ();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "missile";
        
    setmodel (missile, "progs/missile.mdl");
    setsize (missile, '0 0 0', '0 0 0');        
    setorigin (missile, self.origin + v_forward*12 + '0 0 8');
    missile.velocity = lead * 400;
    missile.angles = vectoangles(missile.velocity);

    missile.touch = enforcer_missiletouch;
    
// set missile duration
    missile.nextthink = time + 5;
    missile.think = SUB_Remove;
};

void(float offset) enforcer_fireknight =
{
    local    vector    offang;
    local    vector    org, vec;

    offang = vectoangles (self.enemy.origin - self.origin);
    offang_y = offang_y + offset * 4;
    
    makevectors (offang);

    org = self.origin + self.mins + self.size*0.5 + v_forward * 12;

// set missile speed
    vec = normalize (v_forward);
    vec_z = 0 - vec_z + (random() - 0.5)*0.1;
    
    self.attack_speed = SPEED_HKSPIKE + ( skill * SPEED_HKSKILL);
    
    launch_projectile (org, vec, CT_PROJ_ENFHKN, self.attack_speed);
    //launch_spike (org, vec);
    //newmis.classname = "knightspike";
    //newmis.touch = Laser2_Touch;
    //setmodel (newmis, "progs/k_spike.mdl");
    //setsize (newmis, VEC_ORIGIN, VEC_ORIGIN);        
    //newmis.velocity = vec*400;
    //sound (self, CHAN_WEAPON, "enforcer/ue_fire1.wav", 1, ATTN_NORM);
    
};
    
void()    menf_atk1    =[    $attack1,    menf_atk2    ] {ai_face(); };
void()    menf_atk2    =[    $attack2,    menf_atk3    ] {ai_face(); };
void()    menf_atk3    =[    $attack3,    menf_atk4    ] {ai_face(); };
void()    menf_atk4    =[    $attack4,    menf_atk5    ] {ai_face(); };
void()    menf_atk5    =[    $attack5,    menf_atk6    ] {ai_face(); };
void()    menf_atk6    =[    $attack6,    menf_atk7    ] {ai_face(); enforcer_fireparticlebeam(); };
void()    menf_atk7    =[    $attack7,    menf_atk8    ] {ai_face(); };
void()    menf_atk8    =[    $attack5,    menf_atk9    ] {ai_face(); };
void()    menf_atk9    =[    $attack6,    menf_atk10    ] {ai_face(); enforcer_fireparticlebeam(); };
void()    menf_atk10    =[    $attack7,    menf_atk11    ] {ai_face();};
void()    menf_atk11    =[    $attack5,    menf_atk12    ] {ai_face();};
void()    menf_atk12    =[    $attack6,    menf_atk13    ] {enforcer_fireparticlebeam(); };
void()    menf_atk13    =[    $attack7,    menf_atk14    ] {ai_face();};
void()    menf_atk14    =[    $attack8,    menf_atk15    ] {ai_face(); };
void()    menf_atk15    =[    $attack9,    menf_atk16    ] {ai_face(); };
void()    menf_atk16    =[    $attack10,    enf_run1    ] {ai_face(); SUB_CheckRefire (menf_atk1);};

void()    menf_zatk1    =[    $attack1,    menf_zatk2    ] {ai_face(); enforcer_updateshield(); };
void()    menf_zatk2    =[    $attack2,    menf_zatk3    ] {ai_face(); enforcer_updateshield(); };
void()    menf_zatk3    =[    $attack3,    menf_zatk4    ] {ai_face(); enforcer_updateshield(); };
void()    menf_zatk4    =[    $attack4,    menf_zatk5    ] {ai_face(); enforcer_updateshield(); };
void()    menf_zatk5    =[    $attack5,    menf_zatk6    ] {ai_face();};
void()    menf_zatk6    =[    $attack6,    menf_zatk7    ] {enforcer_fireknight(-3); };
void()    menf_zatk7    =[    $attack7,    menf_zatk8    ] {enforcer_fireknight(-2); };
void()    menf_zatk8    =[    $attack6,    menf_zatk9    ] {enforcer_fireknight(-1); };
void()    menf_zatk9    =[    $attack5,    menf_zatk10    ] {enforcer_fireknight(0); };
void()    menf_zatk10    =[    $attack6,    menf_zatk11    ] {enforcer_fireknight(1); };
void()    menf_zatk11    =[    $attack7,    menf_zatk12    ] {enforcer_fireknight(2); };
void()    menf_zatk12    =[    $attack6,    menf_zatk13    ] {enforcer_fireknight(3); };
void()    menf_zatk13    =[    $attack8,    menf_zatk14    ] {ai_face();};
void()    menf_zatk14    =[    $attack9,    menf_zatk15    ] {ai_face(); enforcer_updateshield(); };
void()    menf_zatk15    =[    $attack10,    enf_run1    ] {ai_face();  enforcer_updateshield(); SUB_CheckRefire (menf_zatk1);};

void()    menf_gatk1    =[    $attack1,    menf_gatk2    ] {ai_face();};
void()    menf_gatk2    =[    $attack2,    menf_gatk3    ] {ai_face();};
void()    menf_gatk3    =[    $attack3,    menf_gatk4    ] {ai_face(); MonsterGrenadeSound(); self.attack_speed = MonsterGrenadeSpeed();
    self.attack_elev = SUB_Elevation(ELEV_DEFAULT, self.origin, self.enemy.origin, self.attack_speed); };
void()    menf_gatk4    =[    $attack4,    menf_gatk5    ] {ai_face();self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void()    menf_gatk5    =[    $attack5,    menf_gatk6    ] {ai_face();self.attack_elev = SUB_Elevation(self.attack_elev, self.origin, self.enemy.origin, self.attack_speed); };
void()    menf_gatk6    =[    $attack6,    menf_gatk7    ] {ai_face();     makevectors(self.angles);
    self.pos1 = self.origin + attack_vector(self.attack_offset);
    MonsterFireGrenade(self.pos1, self.enemy.origin);};
void()    menf_gatk7    =[    $attack7,    menf_gatk8    ] {ai_face();};
void()    menf_gatk8    =[    $attack8,    menf_gatk9    ] {ai_face();};
void()    menf_gatk9    =[    $attack9,    menf_gatk10    ] {ai_face();};
void()    menf_gatk10    =[    $attack10,    enf_run1    ] {ai_face(); SUB_CheckRefire (menf_gatk1);};

void()    menf_ratk1    =[    $attack1,    menf_ratk2    ] {ai_face(); enforcer_updateshield(); };
void()    menf_ratk2    =[    $attack2,    menf_ratk3    ] {ai_face(); enforcer_updateshield(); };
void()    menf_ratk3    =[    $attack3,    menf_ratk4    ] {ai_face(); enforcer_updateshield(); };
void()    menf_ratk4    =[    $attack4,    menf_ratk5    ] {ai_face();};
void()    menf_ratk5    =[    $attack5,    menf_ratk6    ] {ai_face();};
void()    menf_ratk6    =[    $attack6,    menf_ratk7    ] {ai_face(); enforcer_firerocket();};
void()    menf_ratk7    =[    $attack7,    menf_ratk8    ] {ai_face();};
void()    menf_ratk8    =[    $attack8,    menf_ratk9    ] {ai_face();};
void()    menf_ratk9    =[    $attack9,    menf_ratk10    ] {ai_face(); enforcer_updateshield(); };
void()    menf_ratk10    =[    $attack10,    enf_run1    ] {ai_face();  enforcer_updateshield(); SUB_CheckRefire (menf_ratk1);};