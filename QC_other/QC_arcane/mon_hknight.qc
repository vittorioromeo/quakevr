/*==============================================================================
KNIGHT
==============================================================================*/
$cd id1/models/knight2
$origin 0 0 24
$base base
$skin skin

// 000-008
$frame stand1 stand2 stand3 stand4 stand5 stand6 stand7 stand8 stand9

// 009-028
$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9
$frame walk10 walk11 walk12 walk13 walk14 walk15 walk16 walk17
$frame walk18 walk19 walk20

// 029-036
$frame run1 run2 run3 run4 run5 run6 run7 run8

// 037-041
$frame pain1 pain2 pain3 pain4 pain5

// 042-053
$frame death1 death2 death3 death4 death5 death6 death7 death8
$frame death9 death10 death11 death12

// 054-062
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

// 063-078
$frame char_a1 char_a2 char_a3 char_a4 char_a5 char_a6 char_a7 char_a8
$frame char_a9 char_a10 char_a11 char_a12 char_a13 char_a14 char_a15 char_a16

// 079-092
$frame magica1 magica2 magica3 magica4 magica5 magica6 magica7 magica8
$frame magica9 magica10 magica11 magica12 magica13 magica14

// 093-105
$frame magicb1 magicb2 magicb3 magicb4 magicb5 magicb6 magicb7 magicb8
$frame magicb9 magicb10 magicb11 magicb12 magicb13

// 106-111
$frame char_b1 char_b2 char_b3 char_b4 char_b5 char_b6

// 112-121
$frame slice1 slice2 slice3 slice4 slice5 slice6 slice7 slice8 slice9 slice10

// 122-132
$frame smash1 smash2 smash3 smash4 smash5 smash6 smash7 smash8 smash9 smash10
$frame smash11

// 133-154
$frame w_attack1 w_attack2 w_attack3 w_attack4 w_attack5 w_attack6 w_attack7 
$frame w_attack8 w_attack9 w_attack10 w_attack11 w_attack12 w_attack13 w_attack14
$frame w_attack15 w_attack16 w_attack17 w_attack18 w_attack19 w_attack20 
$frame w_attack21 w_attack22 

// 155-165
$frame magicc1 magicc2 magicc3 magicc4 magicc5 magicc6 magicc7 magicc8
$frame magicc9 magicc10 magicc11

void() hknight_sword_sound;
void() knight_statue;

//===========================================================================
void()    hknight_stand1    =[    $stand1,    hknight_stand2    ] {monster_idle_sound(); ai_stand();};
void()    hknight_stand2    =[    $stand2,    hknight_stand3    ] {ai_stand();};
void()    hknight_stand3    =[    $stand3,    hknight_stand4    ] {ai_stand();};
void()    hknight_stand4    =[    $stand4,    hknight_stand5    ] {ai_stand();};
void()    hknight_stand5    =[    $stand5,    hknight_stand6    ] {ai_stand();};
void()    hknight_stand6    =[    $stand6,    hknight_stand7    ] {ai_stand();};
void()    hknight_stand7    =[    $stand7,    hknight_stand8    ] {ai_stand();};
void()    hknight_stand8    =[    $stand8,    hknight_stand9    ] {ai_stand();};
void()    hknight_stand9    =[    $stand9,    hknight_stand1    ] {ai_stand();};

//===========================================================================
void()    hknight_walk1    =[    $walk1,        hknight_walk2    ] {monster_idle_sound(); ai_walk(2);};
void()    hknight_walk2    =[    $walk2,        hknight_walk3    ] {monster_footstep(FALSE);ai_walk(5);};
void()    hknight_walk3    =[    $walk3,        hknight_walk4    ] {ai_walk(5);};
void()    hknight_walk4    =[    $walk4,        hknight_walk5    ] {ai_walk(4);};
void()    hknight_walk5    =[    $walk5,        hknight_walk6    ] {ai_walk(4);};
void()    hknight_walk6    =[    $walk6,        hknight_walk7    ] {ai_walk(2);};
void()    hknight_walk7    =[    $walk7,        hknight_walk8    ] {ai_walk(2);};
void()    hknight_walk8    =[    $walk8,        hknight_walk9    ] {ai_walk(3);};
void()    hknight_walk9    =[    $walk9,        hknight_walk10    ] {ai_walk(3);};
void()    hknight_walk10    =[    $walk10,    hknight_walk11    ] {ai_walk(4);};
void()    hknight_walk11    =[    $walk11,    hknight_walk12    ] {monster_footstep(FALSE); ai_walk(3);};
void()    hknight_walk12    =[    $walk12,    hknight_walk13    ] {ai_walk(4);};
void()    hknight_walk13    =[    $walk13,    hknight_walk14    ] {ai_walk(6);};
void()    hknight_walk14    =[    $walk14,    hknight_walk15    ] {ai_walk(2);};
void()    hknight_walk15    =[    $walk15,    hknight_walk16    ] {ai_walk(2);};
void()    hknight_walk16    =[    $walk16,    hknight_walk17    ] {ai_walk(4);};
void()    hknight_walk17    =[    $walk17,    hknight_walk18    ] {ai_walk(3);};
void()    hknight_walk18    =[    $walk18,    hknight_walk19    ] {ai_walk(3);};
void()    hknight_walk19    =[    $walk19,    hknight_walk20    ] {ai_walk(3);};
void()    hknight_walk20    =[    $walk20,    hknight_walk1    ] {ai_walk(2);};

//===========================================================================
void() CheckForCharge =
{
    // There are 4 conditions to keep charging
    // Can see player, no attack timer, similar Z axis and outside melee range
    // Nightmare skill level can lock HK in constant charging (no attack timer)
    enemy_vis = visible(self.enemy);
    if (!enemy_vis) return;
    if (time < self.attack_finished) return;
    if (self.movespeed < 0) return;
    if (self.tetherlock) return;
    // Only charge at player if ground height < 20
    if ( fabs(self.origin_z - self.enemy.origin_z) > MONAI_CHARGEZAXIS) return;
    if (ai_checkmelee(MONAI_MELEEKNIGHT)) return;

    SUB_AttackFinished (2);
    
    // Used a pre-defined charge routine (th_charge) so that
    // hell and death knights can share the same function
    if (self.th_charge) self.th_charge();
    else self.th_run();
};

//----------------------------------------------------------------------
void()    hknight_run1    =[    $run1,        hknight_run2    ] {monster_idle_sound(); 
    ai_run (20); CheckForCharge(); };
void()    hknight_run2    =[    $run2,        hknight_run3    ] {monster_footstep(FALSE); ai_run(25);};
void()    hknight_run3    =[    $run3,        hknight_run4    ] {ai_run(18);};
void()    hknight_run4    =[    $run4,        hknight_run5    ] {ai_run(16);};
void()    hknight_run5    =[    $run5,        hknight_run6    ] {ai_run(14);};
void()    hknight_run6    =[    $run6,        hknight_run7    ] {monster_footstep(FALSE); ai_run(25);};
void()    hknight_run7    =[    $run7,        hknight_run8    ] {ai_run(21);};
void()    hknight_run8    =[    $run8,        hknight_run1    ] {ai_run(13);};

//----------------------------------------------------------------------
void()    hknight_char_a1    =[    $char_a1,    hknight_char_a2    ] {ai_charge(20);};
void()    hknight_char_a2    =[    $char_a2,    hknight_char_a3    ] {monster_footstep(FALSE); ai_charge(25);};
void()    hknight_char_a3    =[    $char_a3,    hknight_char_a4    ] {ai_charge(18);};
void()    hknight_char_a4    =[    $char_a4,    hknight_char_a5    ] {ai_charge(16);hknight_sword_sound();};
void()    hknight_char_a5    =[    $char_a5,    hknight_char_a6    ] {ai_charge(14);};
void()    hknight_char_a6    =[    $char_a6,    hknight_char_a7    ] {monster_footstep(FALSE); ai_charge(20); ai_melee();};
void()    hknight_char_a7    =[    $char_a7,    hknight_char_a8    ] {ai_charge(21); ai_melee();};
void()    hknight_char_a8    =[    $char_a8,    hknight_char_a9    ] {ai_charge(13); ai_melee();};
void()    hknight_char_a9    =[    $char_a9,    hknight_char_a10    ] {ai_charge(20); ai_melee();};
void()    hknight_char_a10=[    $char_a10,    hknight_char_a11    ] {monster_footstep(FALSE); ai_charge(20); ai_melee();};
void()    hknight_char_a11=[    $char_a11,    hknight_char_a12    ] {ai_charge(18); ai_melee();};
void()    hknight_char_a12=[    $char_a12,    hknight_char_a13    ] {ai_charge(16);};
void()    hknight_char_a13=[    $char_a13,    hknight_char_a14    ] {ai_charge(14);};
void()    hknight_char_a14=[    $char_a14,    hknight_char_a15    ] {monster_footstep(FALSE);ai_charge(25);};
void()    hknight_char_a15=[    $char_a15,    hknight_char_a16    ] {ai_charge(21);};
void()    hknight_char_a16=[    $char_a16,    hknight_run1    ] {ai_charge(13);};

//----------------------------------------------------------------------
// Never used, only called from hknight_char_b
//----------------------------------------------------------------------
void() CheckContinueCharge =
{
    if (time > self.attack_finished) {
        SUB_AttackFinished (3);
        self.th_run();
        return;        // done charging
    }
};

void()    hknight_char_b1    =[    $char_b1,    hknight_char_b2    ]
{CheckContinueCharge (); ai_charge(23); ai_melee();};
void()    hknight_char_b2    =[    $char_b2,    hknight_char_b3    ] {hknight_sword_sound(); ai_charge(17); ai_melee();};
void()    hknight_char_b3    =[    $char_b3,    hknight_char_b4    ] {ai_charge(12); ai_melee();};
void()    hknight_char_b4    =[    $char_b4,    hknight_char_b5    ] {monster_footstep(FALSE); ai_charge(22); ai_melee();};
void()    hknight_char_b5    =[    $char_b5,    hknight_char_b6    ] {ai_charge(18); ai_melee();};
void()    hknight_char_b6    =[    $char_b6,    hknight_char_b1    ] {ai_charge(8); ai_melee();};

//===========================================================================
void()    hknight_slice1    =[    $slice1,    hknight_slice2    ] {ai_charge(9);};
void()    hknight_slice2    =[    $slice2,    hknight_slice3    ] {ai_charge(6);};
void()    hknight_slice3    =[    $slice3,    hknight_slice4    ] {monster_footstep(FALSE); ai_charge(13);};
void()    hknight_slice4    =[    $slice4,    hknight_slice5    ] {hknight_sword_sound(); ai_charge(4);};
void()    hknight_slice5    =[    $slice5,    hknight_slice6    ] {ai_charge(7); ai_melee();};
void()    hknight_slice6    =[    $slice6,    hknight_slice7    ] {ai_charge(15); ai_melee();};
void()    hknight_slice7    =[    $slice7,    hknight_slice8    ] {monster_footstep(FALSE); ai_charge(8); ai_melee();};
void()    hknight_slice8    =[    $slice8,    hknight_slice9    ] {ai_charge(2); ai_melee();};
void()    hknight_slice9    =[    $slice9,    hknight_slice10    ] {ai_melee();};
void()    hknight_slice10    =[    $slice10,    hknight_run1    ] {ai_charge(3);};

//----------------------------------------------------------------------
void()    hknight_smash1    =[    $smash1,    hknight_smash2    ] {ai_charge(1);};
void()    hknight_smash2    =[    $smash2,    hknight_smash3    ] {ai_charge(13);};
void()    hknight_smash3    =[    $smash3,    hknight_smash4    ] {hknight_sword_sound(); ai_charge(9);};
void()    hknight_smash4    =[    $smash4,    hknight_smash5    ] {monster_footstep(FALSE); ai_charge(11);};
void()    hknight_smash5    =[    $smash5,    hknight_smash6    ] {ai_charge(10); ai_melee();};
void()    hknight_smash6    =[    $smash6,    hknight_smash7    ] {ai_charge(7); ai_melee();};
void()    hknight_smash7    =[    $smash7,    hknight_smash8    ] {ai_charge(12); ai_melee();};
void()    hknight_smash8    =[    $smash8,    hknight_smash9    ] {ai_charge(2); ai_melee();};
void()    hknight_smash9    =[    $smash9,    hknight_smash10    ] {monster_footstep(FALSE); ai_charge(3); ai_melee();};
void()    hknight_smash10    =[    $smash10,    hknight_smash11    ] {ai_charge(0);};
void()    hknight_smash11    =[    $smash11,    hknight_run1    ] {ai_charge(0);};

//----------------------------------------------------------------------
void()    hknight_watk1    =[    $w_attack1,    hknight_watk2    ] {ai_charge(2);};
void()    hknight_watk2    =[    $w_attack2,    hknight_watk3    ] {ai_charge(0);};
void()    hknight_watk3    =[    $w_attack3,    hknight_watk4    ] {ai_charge(0);};
void()    hknight_watk4    =[    $w_attack4,    hknight_watk5    ] {hknight_sword_sound(); ai_melee();};
void()    hknight_watk5    =[    $w_attack5,    hknight_watk6    ] {ai_melee();};
void()    hknight_watk6    =[    $w_attack6,    hknight_watk7    ] {ai_melee();};
void()    hknight_watk7    =[    $w_attack7,    hknight_watk8    ] {ai_charge(1);};
void()    hknight_watk8    =[    $w_attack8,    hknight_watk9    ] {ai_charge(4);};
void()    hknight_watk9    =[    $w_attack9,    hknight_watk10    ] {ai_charge(5);};
void()    hknight_watk10    =[    $w_attack10,    hknight_watk11    ] {ai_charge(3); ai_melee();};
void()    hknight_watk11    =[    $w_attack11,    hknight_watk12    ] {monster_footstep(FALSE); ai_charge(2); ai_melee();};
void()    hknight_watk12    =[    $w_attack12,    hknight_watk13    ] {ai_charge(2); ai_melee();};
void()    hknight_watk13    =[    $w_attack13,    hknight_watk14    ] {ai_charge(0);};
void()    hknight_watk14    =[    $w_attack14,    hknight_watk15    ] {hknight_sword_sound(); ai_charge(0);};
void()    hknight_watk15    =[    $w_attack15,    hknight_watk16    ] {ai_charge(0);};
void()    hknight_watk16    =[    $w_attack16,    hknight_watk17    ] {ai_charge(1);};
void()    hknight_watk17    =[    $w_attack17,    hknight_watk18    ] {ai_charge(1); ai_melee();};
void()    hknight_watk18    =[    $w_attack18,    hknight_watk19    ] {ai_charge(3); ai_melee();};
void()    hknight_watk19    =[    $w_attack19,    hknight_watk20    ] {ai_charge(4); ai_melee();};
void()    hknight_watk20    =[    $w_attack20,    hknight_watk21    ] {ai_charge(6);};
void()    hknight_watk21    =[    $w_attack21,    hknight_watk22    ] {ai_charge(7);};
void()    hknight_watk22    =[    $w_attack22,    hknight_run1    ] {monster_footstep(FALSE); ai_charge(3);};

//============================================================================
// Cycle between 3 melee attacks
// Attack 1 - Slice (one hand slashing run attack)
// Attack 2 - Smash (chop overhead to ground)
// Attack 3 - W_Attack (two handed slashing attack)

//----------------------------------------------------------------------
void() hknight_melee =
{
    self.meleeattack = self.meleeattack + 1;

    if (self.meleeattack == 1) hknight_slice1 ();
    else if (self.meleeattack == 2) hknight_smash1 ();
    else {
        hknight_watk1 ();
        self.meleeattack = 0;
    }
};

//============================================================================
void(float offset) hknight_shot =
{
    local    vector    offang;
    local    vector    org, dir;
    
    if (!self.enemy) return;
    if (self.health < 1) return;

    sound (self, CHAN_WEAPON, "hknight/attack1.wav", 1, ATTN_NORM);

    self.attack_speed = SPEED_HKSPIKE + ( skill * SPEED_HKSKILL);
    offang = vectoangles (self.enemy.origin - self.origin);
    offang_y = offang_y + offset * 6;
    makevectors (offang);

    org = self.origin + self.mins + self.size*0.5 + v_forward * 20;
    dir = normalize (v_forward);
    dir_z = 0 - dir_z + (random() - 0.5)*0.1;
    
    launch_projectile (org, dir, CT_PROJ_HKN, self.attack_speed);
};
//----------------------------------------------------------------------
// Magic attack A (over head attack) - not used
void()    hknight_magica1 =[    $magica1,    hknight_magica2    ] {ai_face();};
void()    hknight_magica2 =[    $magica2,    hknight_magica3    ] {ai_face();};
void()    hknight_magica3 =[    $magica3,    hknight_magica4    ] {ai_face();};
void()    hknight_magica4 =[    $magica4,    hknight_magica5    ] {ai_face();};
void()    hknight_magica5 =[    $magica5,    hknight_magica6    ] {ai_face();};
void()    hknight_magica6 =[    $magica6,    hknight_magica7    ] {ai_face();};
void()    hknight_magica7 =[    $magica7,    hknight_magica8    ] {hknight_shot(-2);};
void()    hknight_magica8 =[    $magica8,    hknight_magica9    ] {hknight_shot(-1);};
void()    hknight_magica9 =[    $magica9,    hknight_magica10] {hknight_shot(0);};
void()    hknight_magica10 =[    $magica10,    hknight_magica11] {hknight_shot(1);};
void()    hknight_magica11 =[    $magica11,    hknight_magica12] {hknight_shot(2);};
void()    hknight_magica12 =[    $magica12,    hknight_magica13] {hknight_shot(3);};
void()    hknight_magica13 =[    $magica13,    hknight_magica14] {ai_face();};
void()    hknight_magica14 =[    $magica14,    hknight_run1    ] {ai_face();};

//----------------------------------------------------------------------
// Magic attack B (pointing sword) - not used
void()    hknight_magicb1 =[    $magicb1,    hknight_magicb2    ] {ai_face();};
void()    hknight_magicb2 =[    $magicb2,    hknight_magicb3    ] {ai_face();};
void()    hknight_magicb3 =[    $magicb3,    hknight_magicb4    ] {ai_face();};
void()    hknight_magicb4 =[    $magicb4,    hknight_magicb5    ] {ai_face();};
void()    hknight_magicb5 =[    $magicb5,    hknight_magicb6    ] {ai_face();};
void()    hknight_magicb6 =[    $magicb6,    hknight_magicb7    ] {ai_face();};
void()    hknight_magicb7 =[    $magicb7,    hknight_magicb8    ] {hknight_shot(-2);};
void()    hknight_magicb8 =[    $magicb8,    hknight_magicb9    ] {hknight_shot(-1);};
void()    hknight_magicb9 =[    $magicb9,    hknight_magicb10] {hknight_shot(0);};
void()    hknight_magicb10 =[    $magicb10,    hknight_magicb11] {hknight_shot(1);};
void()    hknight_magicb11 =[    $magicb11,    hknight_magicb12] {hknight_shot(2);};
void()    hknight_magicb12 =[    $magicb12,    hknight_magicb13] {hknight_shot(3);};
void()    hknight_magicb13 =[    $magicb13,    hknight_run1] {ai_face();};

//----------------------------------------------------------------------
// Magic attack C (swing sword in arc)
void()    hknight_magicc1 =[    $magicc1,    hknight_magicc2    ] {ai_face();};
void()    hknight_magicc2 =[    $magicc2,    hknight_magicc3    ] {ai_face();};
void()    hknight_magicc3 =[    $magicc3,    hknight_magicc4    ] {ai_face();};
void()    hknight_magicc4 =[    $magicc4,    hknight_magicc5    ] {ai_face();};
void()    hknight_magicc5 =[    $magicc5,    hknight_magicc6    ] {ai_face();};
void()    hknight_magicc6 =[    $magicc6,    hknight_magicc7    ] {hknight_shot(-2);};
void()    hknight_magicc7 =[    $magicc7,    hknight_magicc8    ] {hknight_shot(-1);};
void()    hknight_magicc8 =[    $magicc8,    hknight_magicc9    ] {hknight_shot(0);};
void()    hknight_magicc9 =[    $magicc9,    hknight_magicc10] {hknight_shot(1);};
void()    hknight_magicc10 =[    $magicc10,    hknight_magicc11] {hknight_shot(2);};
void()    hknight_magicc11 =[    $magicc11,    hknight_run1] {hknight_shot(3);};

//============================================================================
// Special setup - Hell Knight firing electricity at target
//----------------------------------------------------------------------
void() hknight_create_attachment =
{
    // Are the attachments setup yet?
    if (!self.attachment) {
        self.attachment = spawn();
        self.attachment.owner = self;
        self.attachment.classtype = CT_ATTACHMENT;
        self.attachment.mdl = self.weaponglow;
        setmodel(self.attachment, self.attachment.mdl);
        setsize (self.attachment, VEC_ORIGIN, VEC_ORIGIN);
        self.attachment.movetype = MOVETYPE_NONE;
        self.attachment.solid = SOLID_NOT;
    }
};

//----------------------------------------------------------------------
void() hknight_remove_attachment =
{
    if (self.attachment) {
        self.attachment.think = SUB_Remove;
        self.attachment.nextthink = time + 0.1;
    }
}

//----------------------------------------------------------------------
void() hknight_pointfire =
{
    local vector hkp_start, hkp_rand, hkp_end;
    
    // Make sure the knight is alive
    if (self.health < 1) return;

    // Time for a new LG hit sound?
    if (self.t_width < time) {
        // Lower volume and attenuation just in case of several together
        sound (self, CHAN_WEAPON, "weapons/lhit.wav", 0.75, ATTN_IDLE);
        self.t_width = time + 0.6;
    }

    // Update animation frames (default = MagicB6-11)
    self.frame = self.frame + self.pos3_z;
    if (self.frame < self.pos3_x) self.frame = self.pos3_y;
    if (self.frame > self.pos3_y) self.frame = self.pos3_x;
    
    // Turn the Knight towards lightning target
    self.ideal_yaw = vectoyaw(self.movetarget.origin - self.origin);
    ChangeYaw ();
    // Work out start point of lightning bolt
    makevectors(self.angles);
    
    // Work out starting point for lightning on sword
    hkp_start = v_forward*self.pos1_x + v_right*self.pos1_y + v_up*self.pos1_z;
    hkp_rand = v_forward * (self.pos2_x * random());
    hkp_rand = hkp_rand + v_right * (self.pos2_y * random());
    hkp_rand = hkp_rand + v_up * (self.pos2_z * random());
    hkp_start = hkp_start + self.origin + hkp_rand;
    
    // Check for any random target origin variance
    if (self.t_length > 0) {
        hkp_end = vecrand(0,self.t_length,1);
        hkp_end = hkp_end + self.movetarget.origin;
    }
    // Default (0) = exact target position
    else hkp_end = self.movetarget.origin;

    // Update sword with lightning visuals
    if (self.attachment) {
        setorigin(self.attachment, self.origin);
        self.attachment.frame = self.frame;
        self.attachment.angles = self.angles;
        // Allow for no sword effect by going over max and reset
        self.attachment.alpha = self.attachment.alpha + random();
        if (self.attachment.alpha > 1) self.attachment.alpha = 0.01;
        // The four skins are randomly setup already
        self.lip = self.attachment.skin + 1;
        if (self.lip > 3) self.lip = 0;
        self.attachment.skin = self.lip;
        // Draw paticles on sword where lightning starts
        particle_explode(hkp_start, 2, 1, PARTICLE_BURST_BLUE, PARTICLE_BURST_LOSTUP);
    }
    
    // Create lightning bolt
    WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
    WriteEntity (MSG_BROADCAST, self);
    WriteCoord (MSG_BROADCAST, hkp_start_x);
    WriteCoord (MSG_BROADCAST, hkp_start_y);
    WriteCoord (MSG_BROADCAST, hkp_start_z);
    WriteCoord (MSG_BROADCAST, hkp_end_x);
    WriteCoord (MSG_BROADCAST, hkp_end_y);
    WriteCoord (MSG_BROADCAST, hkp_end_z);

    // Can the lightning damage things?
    if (self.dmg > 0) 
        LightningDamage (hkp_start, hkp_end, self, self.dmg);
    
    // Keep Looping
    self.think = self.th_run;
    self.nextthink = time + 0.1;
    
};

//----------------------------------------------------------------------
void() hknight_pointsetup = 
{
    // Find Lightning bolt target
    self.movetarget = find(world, targetname, self.target);
    if (!self.movetarget) {
        dprint("\b[HKNIGHT]\b Pointing knight invalid target!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
        return;
    }
    // Start pointing lightning sword!
    else {
        // Save pain threshold and timeout for later
        self.finaldest_x = self.pain_flinch;
        self.finaldest_y = self.pain_timeout;
        // Reset all pain parameters so it always happens
        self.pain_flinch = self.pain_timeout = 0;
        self.pain_finished = self.pain_ignore = 0;
        self.infightextra = self.noinfighting = 0;
        // Stop other monsters attacking pointy knights
        self.flags = self.flags - (self.flags & FL_MONSTER);
        self.enemy = world;
        // Create glowing sword and start pointing
        hknight_create_attachment();
        self.th_run();
    }
};

//============================================================================
void()    hknight_pain1    =[    $pain1,        hknight_pain2    ] {};
void()    hknight_pain2    =[    $pain2,        hknight_pain3    ] {};
void()    hknight_pain3    =[    $pain3,        hknight_pain4    ] {};
void()    hknight_pain4    =[    $pain4,        hknight_pain5    ] {};
void()    hknight_pain5    =[    $pain5,        hknight_run1    ] {};

//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) hknight_pain =
{
    // Check all pain conditions and set up what to do next
    monster_pain_check(attacker, damage);

    // Any pain animation/sound required?
    if (self.pain_check > 0) {
        sound (self, CHAN_VOICE, self.pain_sound, 1, ATTN_NORM);
        if (self.pain_check == 1) hknight_pain1 ();
        else if (self.pain_check == 2) {
            // reset axe hit and setup short pain recovery
            self.pain_finished = time + 0.5;
            self.axhitme = 0;
            hknight_pain1 ();
        }
    }
};

//============================================================================
void()    hknight_die1    =[    $death1,    hknight_die2    ] {ai_forward(10);};
void()    hknight_die2    =[    $death2,    hknight_die3    ] {monster_check_gib();ai_forward(8);};
void()    hknight_die3    =[    $death3,    hknight_die4    ] {monster_check_gib();
    self.solid = SOLID_NOT; ai_forward(7);};
void()    hknight_die4    =[    $death4,    hknight_die5    ] {};
void()    hknight_die5    =[    $death5,    hknight_die6    ] {};
void()    hknight_die6    =[    $death6,    hknight_die7    ] {};
void()    hknight_die7    =[    $death7,    hknight_die8    ] {};
void()    hknight_die8    =[    $death8,    hknight_die9    ] {ai_forward(10);};
void()    hknight_die9    =[    $death9,    hknight_die10    ] {ai_forward(11);};
void()    hknight_die10    =[    $death10,    hknight_die11    ] {};
void()    hknight_die11    =[    $death11,    hknight_die12    ] {monster_death_postcheck();};
void()    hknight_die12    =[    $death12,    hknight_die12    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void()    hknight_dieb1    =[    $deathb1,    hknight_dieb2    ] {};
void()    hknight_dieb2    =[    $deathb2,    hknight_dieb3    ] {monster_check_gib();};
void()    hknight_dieb3    =[    $deathb3,    hknight_dieb4    ] {monster_check_gib();
    self.solid = SOLID_NOT;};
void()    hknight_dieb4    =[    $deathb4,    hknight_dieb5    ] {};
void()    hknight_dieb5    =[    $deathb5,    hknight_dieb6    ] {};
void()    hknight_dieb6    =[    $deathb6,    hknight_dieb7    ] {};
void()    hknight_dieb7    =[    $deathb7,    hknight_dieb8    ] {};
void()    hknight_dieb8    =[    $deathb8,    hknight_dieb9    ] {monster_death_postcheck();};
void()    hknight_dieb9    =[    $deathb9,    hknight_dieb9    ] {monster_deadbody_check();};

//----------------------------------------------------------------------
void() hknight_die =
{
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();
    // Make sure attachments are removed
    hknight_remove_attachment();
    
    // regular death
    if (!self.gibbed) {
    sound (self, CHAN_VOICE, self.death_sound, 1, ATTN_NORM);
        if (random() > 0.5) hknight_die1 ();    // Forward - standard
        else hknight_dieb1 ();                    // Backward - standard
    }
};

//============================================================================
void() hknight_sword_sound =
{
    self.lip = random();
    if (self.lip < 0.25) sound (self, CHAN_WEAPON, "weapons/sword1a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.5) sound (self, CHAN_WEAPON, "weapons/sword2a.wav", 1, ATTN_NORM);
    else if (self.lip < 0.75) sound (self, CHAN_WEAPON, "weapons/sword1b.wav", 1, ATTN_NORM);
    else sound (self, CHAN_WEAPON, "weapons/sword2b.wav", 1, ATTN_NORM);
};

//============================================================================
// Used by stone knights to wakup and attack the player
//----------------------------------------------------------------------
void() hknight_aistates =
{
    self.th_stand = hknight_stand1;        // Idling on feet
    self.th_walk = hknight_walk1;        // Walking
    self.th_run = hknight_run1;            // Running
    self.th_melee = hknight_melee;        // Sword attack
    self.th_missile = hknight_magicc1;    // Fire spikes
    self.th_charge = hknight_char_a1;    // Run + melee
    self.th_pain = hknight_pain;        // Stumble and pain
    self.th_die = hknight_die;            // Face meets floor
};

//----------------------------------------------------------------------
void() hknight_wakeup =
{
    if (self.health < 0) return;        // Check for sudden death!
    
    self.use = SUB_Null;                // no re-trigger
    self.takedamage = DAMAGE_AIM;        // Can receive damage
    self.spawnstatue = FALSE;            // No longer stationary

    hknight_aistates();                    // Restore AI state functions
    monster_targets();                    // Check for targets
};

//----------------------------------------------------------------------
// Restore pointy lightning knights to normal hell knights
//----------------------------------------------------------------------
void() hknight_pointwakeup =
{
    // Check for sudden death!
    if (self.health < 0) return;
    // Always reset health+max back to default
    self.health = self.max_health = self.reset_health;
    // Fire any trigger event when breaking pointy pose
    if (self.target2 != "") {
        trigger_strs(self.target2, self.enemy);
        self.target2 = "";        // Trigger once
    }

    // Restore pain threshold and timeout
    self.pain_flinch = self.finaldest_x;
    self.pain_timeout = self.finaldest_y;
    self.pain_finished = time + 2;
    self.flags = self.flags | FL_MONSTER;

    // Restore the nail resistance if setup to be a statue skin
    if (self.spawnflags & MON_STATUE) self.resist_nails = 0.2;
    
    hknight_remove_attachment();        // Remove glowing sword
    hknight_aistates();                    // Restore AI state functions
    // Check for any enemy target?
    if (self.enemy) {
        monster_sightsound();            // Sword sounds!
        FoundHuntTarget(TRUE);            // Setup goals and targets
        self.think = self.th_run;        // Move to running
    }
    else self.think = self.th_stand;    // Stand around
    self.nextthink = time + 0.1;
};

//----------------------------------------------------------------------
// Special pain function for pointy lightning knights
//----------------------------------------------------------------------
void(entity inflictor, entity attacker, float damage) hknight_pointpain =
{
    self.pain_finished = 0;
    if (!(attacker.flags & FL_CLIENT)) return;
    self.enemy = attacker;
    hknight_pointwakeup();
};

//----------------------------------------------------------------------
// Special use/trigger function for pointy lightning knights
//----------------------------------------------------------------------
void() hknight_pointuse = 
{
    // Check for enemy/health/player exceptions
    if (self.enemy) return;
    if (self.health < 1) return;
    if (activator.items & IT_INVISIBILITY) return;
    if (activator.flags & FL_NOTARGET) return;

    // Trigger once
    self.use = SUB_Null;
    // Is the trigger the player?
    if (activator.flags & FL_CLIENT) self.enemy = activator;
    else self.enemy = world;
    hknight_pointwakeup();
};

/*======================================================================
 QUAKED monster_hell_knight (1 0 0) (-16 -16 -24) (16 16 40) Ambush
======================================================================*/
void() monster_hell_knight =
{
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/mon_hknight.mdl";            // Original ID Knight + extra skins
    self.headmdl = "progs/h_hellkn.mdl";
    self.gib1mdl = "progs/w_hknightsword.mdl";    // Unique sword
    self.gib2mdl = "progs/gib_knfoot_l.mdl";    // left foot
    self.gib3mdl = "progs/gib_knfoot_r.mdl";    // right foot
    
    precache_model (self.mdl);
    precache_model (self.headmdl);
    precache_model (MODEL_PROJ_HKN);
    precache_model (self.gib1mdl);
    precache_model (self.gib2mdl);
    precache_model (self.gib3mdl);

    self.gib1sound = GIB_IMPACT_METALA;
    if (random() < 0.5) self.gib2mdl = string_null;
    if (random() < 0.5) self.gib3mdl = string_null;

    // Firing of magic spikes (first 0.1s must loop)
    precache_sound ("hknight/attack1.wav");

    // TE_KNIGHTSPIKE = Magic spike hitting the wall
    // Has got to be in this location because the 
    // filename is hardcoded (engine.exe)
    precache_sound ("hknight/hit.wav");                

    //----------------------------------------------------------------------
    if (self.spawnflags & MON_STATUE) {
        self.idle_sound = "statue/idle1.wav";
        self.idle_sound2 = "statue/idle2.wav";
        self.idle_soundcom = "statue/comidle.wav";
        precache_sound (self.idle_sound);
        precache_sound (self.idle_sound2);
        precache_sound (self.idle_soundcom);

        self.sight_sound = "statue/sight.wav";
        self.pain_sound = "statue/hurt.wav";
        self.death_sound = "statue/death.wav";
        self.gib1skin = 1;                    // Stoney sword
        precache_stonegibs();                // Cache gib models
        self.gibtype = GIBTYPE_STONE;        // Stoney gibs!
        self.gib2mdl = self.gib3mdl = string_null;
    }
    else {
        self.idle_sound = "hknight/idle.wav";
        precache_sound (self.idle_sound);

        self.sight_sound = "hknight/sight1.wav";
        self.pain_sound = "hknight/pain1.wav";
        self.death_sound = "hknight/death1.wav";

        // Check for poisonous entity flag
        if (self.poisonous) {
            precache_poisongibs();            // precache gibs
            self.gibtype = GIBTYPE_POISON;    // Poisonous blood trails
            precache_model (MODEL_PROJ_HKNGRN);
        }
    }
    // precache sight, pain and death (changes on type)
    precache_sound (self.sight_sound);
    precache_sound (self.pain_sound);
    precache_sound (self.death_sound);

    // New sword sweep/slash sounds
    precache_sound ("weapons/sword1a.wav");
    precache_sound ("weapons/sword1b.wav");
    precache_sound ("weapons/sword2a.wav");
    precache_sound ("weapons/sword2b.wav");
    
    self.solid = SOLID_NOT;                    // No interaction with world
    self.movetype = MOVETYPE_NONE;            // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 250;
    self.gibhealth = -40;                    // Not easy to gib
    self.gibbed = FALSE;                    // Still in one piece
    self.pain_flinch = 40;                    // Medium pain threshold
    self.pain_timeout = 1.5;                // Higher than a knight
    self.steptype = FS_TYPEHEAVY;            // Heavy player
    self.meleeattack = rint(random()*3);    // randomize last melee attack
    self.th_die = hknight_die;                // Face meets floor
    self.deathstring = " was slain by a Hell Knight\n";
    
    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    //----------------------------------------------------------------------
    // Special lightning point knight (metal/stone skins)
    //----------------------------------------------------------------------
    if (self.spawnflags & MON_POINT_KNIGHT) {
        self.weaponglow = "progs/mon_hknight_glow.mdl";
        precache_model (self.weaponglow);    // Glowing magical swords
        if (self.spawnflags & MON_STATUE) {
            self.classgroup = CG_STONE;        // Stone hates metal!
            self.skin = self.exactskin = 1;    // Stone skin
            self.pain_flinch = 60;            // Higher resistance
            self.pain_longanim = FALSE;        // resistant to shadow axe
            self.pain_timeout = 2;            // high pain threshold
            self.no_liquiddmg = TRUE;        // Immune to slime/lava
        }
        self.th_stand = hknight_pointsetup;    // Find target before shoot
        self.th_run = hknight_pointfire;    // Keep Looping
        self.th_pain = hknight_pointpain;    // Release from pointing
        self.think1 = hknight_pointuse;        // Special trigger event
        self.reset_health = self.health;    // Save for later
        self.health = MEGADEATH;            // Should be impossible to kill
        if (self.t_length < 0) self.t_length = 5;    // Random target factor
        if (self.dmg < 0) self.dmg = 5;                // Contact damage
        
        // Setup default pose start pos + random modifier
        if (CheckZeroVector(self.pos1)) self.pos1 = '20 0 24';
        if (CheckZeroVector(self.pos2)) self.pos2 = '30 0 2';
        // Setup default animation pose (magicB6 - magicB11)
        if (CheckZeroVector(self.pos3)) self.pos3 = '98 103 1';
    }
    //----------------------------------------------------------------------
    // Detect statues and setup different parameters
    // Tougher skin, less pain, heavy feet etc    
    //----------------------------------------------------------------------
    else if (self.spawnflags & MON_STATUE) {
        self.classgroup = CG_STONE;        // Stone hates metal!
        self.poisonous = FALSE;            // Cannot be poisonous and stone!
        self.resist_nails = 0.5;        // Tough skin for nails
        self.reflectnails = TRUE;        // Reflect nail projectiles
        self.skin = self.exactskin = 1;    // Stone skin
        self.pain_flinch = 60;            // Higher resistance
        self.pain_longanim = FALSE;        // resistant to shadow axe
        self.pain_timeout = 2;            // high pain threshold
        self.no_liquiddmg = TRUE;        // Immune to slime/lava
        self.deathstring = " was smashed by a Stone Hell Knight\n";

        // Allow knights to start with a stone skin, but not be frozen!
        if (self.spawnflags & MON_NOTFROZEN) {
            self.spawnflags = self.spawnflags - MON_STATUE;
            hknight_aistates();            // setup AI state functions
        }
        else {
            self.spawnstatue = TRUE;        // Start as a statue
            self.th_stand = knight_statue;    // Stone idle sound
            self.think1 = hknight_wakeup;    // Wakeup function when triggered
            
            // statues can change position when the player is not looking
            // Specify a start/finish frame using pos1 or use frame
            if (!self.pos1_x) {
                self.pos1_x = $w_attack1;        // Default pose (attacking)
                self.pos1_y = $w_attack15;
            }
            // A frame has been specified, create a small range from that
            if (self.frame > 0) {
                self.pos1_x = self.pos1_y = self.frame;
            }
            // Work out range, make sure no negative values
            self.pos1_z = fabs(self.pos1_y - self.pos1_x);
            self.frame = self.pos1_x;        // Setup frame / pose
        }
    }
    else {
        // Default Hell knight (like a death knight really)
        self.pain_longanim = TRUE;        // can be chopped with shadow axe
        self.classgroup = CG_DKNIGHT;    // Classic tincan!
        hknight_aistates();                // setup AI state functions
    }
    
    self.classtype = CT_MONHELLK;
    self.classmove = MON_MOVEWALK;
    monster_start();
};

//======================================================================
/*QUAKED monster_hell_pointknight (0.75 0.5 0) (-16 -16 -24) (16 16 40) AMBUSH STATUE x NOSIGHT NOIDLE NOGFX STARTOFF x Not_Easy Not_Normal Not_Hard Not_DM
{ model(":progs/mon_hknight.mdl"); }
Special Electricity Pointing Hell Knight
-------- KEYS --------
targetname : monster/trigger name
target : target for electricity
target2 : trigger to fire when electricity stopped
pos1 : XYZ offset to start lightning (uses v_forward/v_right/v_up)
pos2 : XYZ random offset to start lightning (uses v_forward/v_right/v_up)
pos3 : Animation lightning frame set (Start/Finish/Direction)
t_length : random wobble to electricity target origin (0=def 5)
dmg : amount of contact damage with electricity (0=def 5)
deathtarget : entity to trigger upon death (useful if target field already in use)
health : Override default health settings
upgrade_ssg : 1 = will only spawn if shotgun upgrade active on server
upgrade_axe : 1 = will only spawn if axe upgrade active on server
upgrade_lg : 1 = will only spawn if lightning gun upgrade active on server
nomonstercount : will not be included in any monster count functionality
no_zaware : All Z Aware projectiles will be disabled
bboxtype : Change bbox 1=Tiny,4=Short,5=Tall,7=Wide,8=Giant,10=Massive
gibondeath : 1 = always explode in a shower of gibs on death
bodyfadeaway : Time (secs) before body/head will fade away (default=0)
-------- SPAWNFLAGS --------
AMBUSH : the monster will only wake up on seeing the player, not by another monster
STATUE : Stone statue skin
NOSIGHT : No sight sound
NOIDLE : No idle sound
NOGFX : No spawn effect or sound when triggered
STARTOFF : Trigger Spawn
-------- NOTES --------
Special Electricity Pointing Hell Knight*/
//======================================================================
void() monster_hell_pointknight =
{
    // Setup with new special spawnflag (not mapper controlled)
    self.spawnflags = self.spawnflags | MON_POINT_KNIGHT;
    // remove notfrozen and angry, always start/spawn firing lightning
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_NOTFROZEN);
    self.spawnflags = self.spawnflags - (self.spawnflags & MON_SPAWN_ANGRY);
    
    // Override early to prevent unnecesaary cache of assets
    self.poisonous = FALSE;            // Never poisonous
    
    // If not target setup then stop!
    if (self.target == "") {
        dprint("\b[HKNIGHT]\b Pointing knight needs target!\n");
        spawn_marker(self.origin, SPNMARK_YELLOW);
        remove(self);
        return;
    }
    // Loop back through existing hell knight function
    else monster_hell_knight();
};