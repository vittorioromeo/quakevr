/*==============================================================================
PLAYER
==============================================================================*/
$cd id1/models/player_4
$origin 0 -6 24
$base base        
$skin skin

$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6

$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6

$frame stand1 stand2 stand3 stand4 stand5

$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12

$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6

$frame pain1 pain2 pain3 pain4 pain5 pain6

$frame axdeth1 axdeth2 axdeth3 axdeth4 axdeth5 axdeth6
$frame axdeth7 axdeth8 axdeth9

$frame deatha1 deatha2 deatha3 deatha4 deatha5 deatha6 deatha7 deatha8
$frame deatha9 deatha10 deatha11

$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8
$frame deathb9

$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8
$frame deathc9 deathc10 deathc11 deathc12 deathc13 deathc14 deathc15

$frame deathd1 deathd2 deathd3 deathd4 deathd5 deathd6 deathd7
$frame deathd8 deathd9

$frame deathe1 deathe2 deathe3 deathe4 deathe5 deathe6 deathe7
$frame deathe8 deathe9

// attacks
$frame nailatt1 nailatt2
$frame light1 light2
$frame rockatt1 rockatt2 rockatt3 rockatt4 rockatt5 rockatt6
$frame shotatt1 shotatt2 shotatt3 shotatt4 shotatt5 shotatt6
$frame axatt1 axatt2 axatt3 axatt4 axatt5 axatt6
$frame axattb1 axattb2 axattb3 axattb4 axattb5 axattb6
$frame axattc1 axattc2 axattc3 axattc4 axattc5 axattc6
$frame axattd1 axattd2 axattd3 axattd4 axattd5 axattd6

void() player_run;

// Updated player idle/fire weapon frame logic to be consistent
// Bug highlighted by Stas "dwere" Kuznetsov
float IDLE_WEAPON_FRAME = 0;    // Default idle state
float FIRE_WEAPON_FRAME = 1;    // Start of firing frames

/*==============================================================================
PLAYER AXE

Frame 0 - Idle LOW
Frame 1 - Idle HIGH

Axe A     Original - 1,2,3,4        New - 3,4,5,7                a3,a4,a5,a7
Axe B        Original - 5,6,7,8        New - 10,11,13,14        b3,b4,b6,b7
Axe C        Original - 1,2,3,4        New - 17,18,20,21        c3,c4,c6,c7
Axe D        Original - 5,6,7,8        New - 24,25,26,28        d3,d4,d5,d7
Axe E        Original - x,x,x,x        New - 31,32,34,35        e3,e4,e6,e7

============================================================================*/
void()    player_axe1 =    [$axatt1, player_axe2    ] {self.weaponframe=1;};
void()    player_axe2 =    [$axatt2, player_axe3    ] {self.weaponframe=2;};
void()    player_axe3 =    [$axatt3, player_axe4    ] {self.weaponframe=3;W_FireAxe();};
void()    player_axe4 =    [$axatt4, player_run    ] {self.weaponframe=4;};

void()    player_axeb1 =    [$axattb1, player_axeb2    ] {self.weaponframe=5;};
void()    player_axeb2 =    [$axattb2, player_axeb3    ] {self.weaponframe=6;};
void()    player_axeb3 =    [$axattb3, player_axeb4    ] {self.weaponframe=7;W_FireAxe();};
void()    player_axeb4 =    [$axattb4, player_run    ] {self.weaponframe=8;};

void()    player_axec1 =    [$axattc1, player_axec2    ] {self.weaponframe=9;};
void()    player_axec2 =    [$axattc2, player_axec3    ] {self.weaponframe=10;};
void()    player_axec3 =    [$axattc3, player_axec4    ] {self.weaponframe=11;W_FireAxe();};
void()    player_axec4 =    [$axattc4, player_run    ] {self.weaponframe=12;};

void()    player_axed1 =    [$axattd1, player_axed2    ] {self.weaponframe=13;};
void()    player_axed2 =    [$axattd2, player_axed3    ] {self.weaponframe=14;};
void()    player_axed3 =    [$axattd3, player_axed4    ] {self.weaponframe=15;W_FireAxe();};
void()    player_axed4 =    [$axattd4, player_run    ] {self.weaponframe=16;};

void()    player_axee1 =    [$axattd1, player_axee2    ] {self.weaponframe=17;};
void()    player_axee2 =    [$axattd2, player_axee3    ] {self.weaponframe=18;};
void()    player_axee3 =    [$axattd3, player_axee4    ] {self.weaponframe=19;W_FireAxe();};
void()    player_axee4 =    [$axattd4, player_run    ] {self.weaponframe=20;};

//============================================================================
void()    player_stand1 =[    $axstnd1,    player_stand1    ] {
    self.weaponframe=IDLE_WEAPON_FRAME;
    if (self.velocity_x || self.velocity_y) {
        self.walkframe = 0;
        player_run();
        return;
    }

    if (self.weapon == IT_AXE) {
        if (self.walkframe >= 12) self.walkframe = 0;
        self.frame = $axstnd1 + self.walkframe;
    }
    else {
        if (self.walkframe >= 5) self.walkframe = 0;
        self.frame = $stand1 + self.walkframe;
    }
    self.walkframe = self.walkframe + 1;    
};

//============================================================================
void()    player_run =[ $rockrun1, player_run ] {
    
    // Get out of any weapon animation
    self.weaponframe = IDLE_WEAPON_FRAME;
    // Standing around has different animation sets
    if (!self.velocity_x && !self.velocity_y) {
        self.walkframe = 0;
        player_stand1();
        return;
    }
    // make sure correct player animation active
    if (self.weapon == IT_AXE) {
        if (self.walkframe >= 6) self.walkframe = 0;
        self.frame = $axrun1 + self.walkframe;
    }
    else {
        if (self.walkframe >= 6) self.walkframe = 0;
        self.frame = self.frame + self.walkframe;
    }
    
    // Check for player feet sound, using velocity & ground flag
    if ((self.walkframe == 2 || self.walkframe == 5) && 
        self.flags & FL_ONGROUND) {
        self.cnt = fabs(self.velocity_x + self.velocity_y);
        // Need to be moving before feet sounds
        if (self.cnt > 50) monster_footstep(FALSE);
    }
    // Update frame
    self.walkframe = self.walkframe + 1;
};

//============================================================================
void()    player_run_nofeet =[ $rockrun1, player_run ] {
    
    // Get out of any weapon animation
    self.weaponframe = IDLE_WEAPON_FRAME;
    // Standing around has different animation sets
    if (!self.velocity_x && !self.velocity_y) {
        self.walkframe = 0;
        player_stand1();
        return;
    }
    // make sure correct player animation active
    if (self.weapon == IT_AXE) {
        if (self.walkframe >= 6) self.walkframe = 0;
        self.frame = $axrun1 + self.walkframe;
    }
    else {
        if (self.walkframe >= 6) self.walkframe = 0;
        self.frame = self.frame + self.walkframe;
    }
        
    // Update frame
    self.walkframe = self.walkframe + 1;
};

//----------------------------------------------------------------------
void(entity targ) remoteplayer_run =
{
    local entity tself;
    tself = self; self = targ;
    // This has to run the block above otherwise weaponframe errors
    // the animation code "xx = [ $x, x ]" has to be run
    // The above code cannot be pasted into this function
    // Be careful with this function, it needs a certain layout!
    player_run_nofeet();
    self = tself;
};

//============================================================================
void()    player_sgreset =    [$shotatt1, player_sg2    ] {
    self.weaponframe=FIRE_WEAPON_FRAME;
};

void()    player_sg1 =    [$shotatt1, player_sg1    ] { 
    self.weaponframe=IDLE_WEAPON_FRAME;
    if (!self.button0) { player_run(); return; }
    W_FireShotgun ();
};
void()    player_sg2 =    [$shotatt2, player_sg3    ] { self.weaponframe=2; };
void()    player_sg3 =    [$shotatt3, player_sg4    ] { self.weaponframe=3; };
void()    player_sg4 =    [$shotatt4, player_sg5    ] { self.weaponframe=4; };
void()    player_sg5 =    [$shotatt5, player_sg1    ] { self.weaponframe=5; };

//============================================================================
void()    player_ssgreset =    [$shotatt1, player_supersg2    ] {
    self.weaponframe=FIRE_WEAPON_FRAME;
};

void()    player_supersg1 =    [$shotatt1, player_supersg1    ] { 
    self.weaponframe=IDLE_WEAPON_FRAME;
    if (!self.button0) { player_run(); return; }
    W_FireSuperShotgun ();
};
void()    player_supersg2 =    [$shotatt2, player_supersg3    ] { self.weaponframe=2; };
void()    player_supersg3 =    [$shotatt3, player_supersg4    ] { self.weaponframe=3; };
void()    player_supersg4 =    [$shotatt4, player_supersg5    ] { self.weaponframe=4; };
void()    player_supersg5 =    [$shotatt5, player_supersg6    ] { self.weaponframe=5; };
void()    player_supersg6 =    [$shotatt6, player_supersg1    ] { self.weaponframe=6;
    // If player has one shell left, switch to SG after SSG fire animation
    if (self.ammo_shells < 2) { forceweaponswitch(0.2); return;}};

//============================================================================
// The Nailgun
//============================================================================
void(float oframe, float ox) nail_upd = {
    if (!self.button0) { player_run(); return; }
    self.weaponframe = oframe;
    W_FireSpikes (0,ox);
};

void() player_nail1 = [$nailatt1, player_nail2 ] {nail_upd(1,2);};
void() player_nail2 = [$nailatt2, player_nail3 ] {nail_upd(2,-2);};
void() player_nail3 = [$nailatt1, player_nail4 ] {nail_upd(3,2);};
void() player_nail4 = [$nailatt2, player_nail5 ] {nail_upd(4,-2);};
void() player_nail5 = [$nailatt1, player_nail6 ] {nail_upd(5,2);};
void() player_nail6 = [$nailatt2, player_nail7 ] {nail_upd(6,-2);};
void() player_nail7 = [$nailatt1, player_nail8 ] {nail_upd(7,2);};
void() player_nail8 = [$nailatt2, player_nail1 ] {nail_upd(8,-2);};

//============================================================================
// The Super Nailgun
// I really liked the idea of saving the frame number of the SNG
// so that when it stops/starts it remembers where it left off
// Unfortunately the gun is reset to frame 0 - the idle state
// when the weapon is not being fired.
// - SNG projectile offset idea by Kinn
//============================================================================
void(float oframe, float oz, float ox) snail_upd = {
    if (!self.button0) { player_run(); return; }
    self.weaponframe = oframe;
    W_FireSpikes (oz,ox);
};

void() player_snail1 = [$nailatt1, player_snail2 ] {snail_upd(1,8,2.5);};
void() player_snail2 = [$nailatt2, player_snail3 ] {snail_upd(2,5.5,-3.5);};
void() player_snail3 = [$nailatt1, player_snail4 ] {snail_upd(3,3,2.5);};
void() player_snail4 = [$nailatt2, player_snail5 ] {snail_upd(4,9,0);};
void() player_snail5 = [$nailatt1, player_snail6 ] {snail_upd(5,3,-2.5);};
void() player_snail6 = [$nailatt2, player_snail7 ] {snail_upd(6,5.5,3.5);};
void() player_snail7 = [$nailatt1, player_snail8 ] {snail_upd(7,8,-2.5);};
void() player_snail8 = [$nailatt2, player_snail1 ] {snail_upd(8,2,0);};

//============================================================================
void() player_plasma1   =[$light1, player_plasma2  ] {
    if (!self.button0) { player_run(); return; }
    self.weaponframe = self.weaponframe + 1;
    if (self.weaponframe >= 5) self.weaponframe = FIRE_WEAPON_FRAME;
    W_FirePlasma();
};
void() player_plasma2   =[$light2, player_plasma1  ] {
    if (!self.button0) { player_run(); return; }
    self.weaponframe = self.weaponframe + 1;
    if (self.weaponframe >= 5) self.weaponframe = FIRE_WEAPON_FRAME;
    W_FirePlasma();
};

//============================================================================
void() player_light1   =[$light1, player_light2  ] {
    if (!self.button0) { player_run(); return; }
    self.weaponframe = self.weaponframe + 1;
    if (self.weaponframe >= 5) self.weaponframe = FIRE_WEAPON_FRAME;
    W_FireLightning();
};
void() player_light2   =[$light2, player_light1  ] {
    if (!self.button0) { player_run(); return; }
    self.weaponframe = self.weaponframe + 1;
    if (self.weaponframe >= 5) self.weaponframe = FIRE_WEAPON_FRAME;
    W_FireLightning();
};

//============================================================================
void() player_rockreset   =[$rockatt1, player_rocket2  ] {
    self.weaponframe=FIRE_WEAPON_FRAME;
};

void() player_rocket1   =[$rockatt1, player_rocket1  ] { 
    self.weaponframe=IDLE_WEAPON_FRAME;
    if (!self.button0) { player_run(); return; }
    W_FireRocket();
};
void() player_rocket2   =[$rockatt2, player_rocket3  ] { self.weaponframe=2;};
void() player_rocket3   =[$rockatt3, player_rocket4  ] { self.weaponframe=3;};
void() player_rocket4   =[$rockatt4, player_rocket5  ] { self.weaponframe=4;};
void() player_rocket5   =[$rockatt5, player_rocket6  ] { self.weaponframe=5;};
void() player_rocket6   =[$rockatt6, player_rocket1  ] { self.weaponframe=6;};

//============================================================================
void() player_grenreset   =[$rockatt1, player_grenade2  ] {
    self.weaponframe=FIRE_WEAPON_FRAME;
};

void() player_grenade1   =[$rockatt1, player_grenade1  ] {
    self.weaponframe=IDLE_WEAPON_FRAME;
    if (!self.button0) { player_run(); return; }
    W_FireGrenade();
};
void() player_grenade2   =[$rockatt2, player_grenade3  ] { self.weaponframe=2;};
void() player_grenade3   =[$rockatt3, player_grenade4  ] { self.weaponframe=3;};
void() player_grenade4   =[$rockatt4, player_grenade5  ] { self.weaponframe=4;};
void() player_grenade5   =[$rockatt5, player_grenade6  ] { self.weaponframe=5;};
void() player_grenade6   =[$rockatt6, player_grenade1  ] { self.weaponframe=6;};

//----------------------------------------------------------------------
// Only used for the player when they die
//----------------------------------------------------------------------
void() misc_bubble_bob;
void() death_bubble_spawn =
{
    local entity bubble;
    
    if (self.owner.waterlevel != 3) return;
    
    bubble = spawn();
    bubble.classname = "bubble";
    bubble.solid = SOLID_NOT;
    bubble.movetype = MOVETYPE_NOCLIP;
    setmodel (bubble, SBUBBLE_DROWN);
    bubble.frame = bubble.cnt = 0;
    setorigin (bubble, self.owner.origin + '0 0 24');
    setsize (bubble, '-8 -8 -8', '8 8 8');
    bubble.velocity = '0 0 15';
    bubble.nextthink = time + 0.5;
    bubble.think = misc_bubble_bob;
    
    self.nextthink = time + 0.1;
    self.think = death_bubble_spawn;
    self.air_finished = self.air_finished + 1;
    if (self.air_finished >= self.bubble_count) remove(self);
};

//----------------------------------------------------------------------
void(float num_bubbles) death_bubbles =
{
    local entity bubble_spawner;
    
    bubble_spawner = spawn();
    bubble_spawner.owner = self;
    bubble_spawner.air_finished = 0;
    bubble_spawner.bubble_count = num_bubbles;
    setorigin (bubble_spawner, self.origin);
    bubble_spawner.nextthink = time + 0.1;
    bubble_spawner.think = death_bubble_spawn;
};

//============================================================================
void() PainSound =
{
    if (self.health < 0) return;
    if (self.pain_finished > time) { self.axhitme = 0; return;    }

    // don't make multiple pain sounds right after each other
    self.pain_finished = time + 0.5;

    if (damage_attacker.classname == "teledeath") {
        sound (self, CHAN_VOICE, "player/teledth1.wav", 1, ATTN_NONE);
    }
    // water pain sounds
    else if (self.watertype == CONTENT_WATER && self.waterlevel == 3) {
        death_bubbles(1);
        if (random() > 0.5) sound (self, CHAN_VOICE, "player/drown1.wav", 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, "player/drown2.wav", 1, ATTN_NORM);
    }
    // slime pain sounds
    else if (self.watertype == CONTENT_SLIME) {
        // FIX ME    put in some steam here
        if (random() > 0.5) sound (self, CHAN_VOICE, "player/lburn1.wav", 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, "player/lburn2.wav", 1, ATTN_NORM);
    }
    else if (self.watertype == CONTENT_LAVA) {
        if (random() > 0.5) sound (self, CHAN_VOICE, "player/lburn1.wav", 1, ATTN_NORM);
        else sound (self, CHAN_VOICE, "player/lburn2.wav", 1, ATTN_NORM);
    }
    else {
        // ax pain sound
        if (self.axhitme > 0) {
            self.axhitme = 0;
            // Player been hit by an axe (MP thing)
            sound (self, CHAN_VOICE, SOUND_AXE_PLAYER, 1, ATTN_NORM);
        }
        else {
            self.lip = rint((random() * 5) + 1);
            if (self.lip == 1) self.noise = "player/pain1.wav";
            else if (self.lip == 2) self.noise = "player/pain2.wav";
            else if (self.lip == 3) self.noise = "player/pain3.wav";
            else if (self.lip == 4) self.noise = "player/pain4.wav";
            else if (self.lip == 5) self.noise = "player/pain5.wav";
            else self.noise = "player/pain6.wav";

            sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
        }
    }
};

//============================================================================
void()    player_pain1 =    [    $pain1,    player_pain2    ] {
    PainSound();
    self.weaponframe=IDLE_WEAPON_FRAME;
};
void()    player_pain2 =    [    $pain2,    player_pain3    ] {};
void()    player_pain3 =    [    $pain3,    player_pain4    ] {};
void()    player_pain4 =    [    $pain4,    player_pain5    ] {};
void()    player_pain5 =    [    $pain5,    player_pain6    ] {};
void()    player_pain6 =    [    $pain6,    player_run    ] {};

void()    player_axpain1 =    [    $axpain1,    player_axpain2    ] {
    PainSound();
    self.weaponframe=IDLE_WEAPON_FRAME;
};
void()    player_axpain2 =    [    $axpain2,    player_axpain3    ] {};
void()    player_axpain3 =    [    $axpain3,    player_axpain4    ] {};
void()    player_axpain4 =    [    $axpain4,    player_axpain5    ] {};
void()    player_axpain5 =    [    $axpain5,    player_axpain6    ] {};
void()    player_axpain6 =    [    $axpain6,    player_run    ] {};

//============================================================================
void(entity inflictor, entity attacker, float damage) player_pain =
{
    if (self.weaponframe) return;
    if (self.invisible_finished > time) return;        // eyes don't have pain frames
    if (self.weapon == IT_AXE) player_axpain1 ();
    else player_pain1 ();
};

//============================================================================
void() player_dead =
{
    self.nextthink = -1;
    // allow respawn after a certain time
    self.deadflag = DEAD_DEAD;
};

//============================================================================
void()    player_diea1    =    [    $deatha1,    player_diea2    ] {};
void()    player_diea2    =    [    $deatha2,    player_diea3    ] {};
void()    player_diea3    =    [    $deatha3,    player_diea4    ] {};
void()    player_diea4    =    [    $deatha4,    player_diea5    ] {};
void()    player_diea5    =    [    $deatha5,    player_diea6    ] {};
void()    player_diea6    =    [    $deatha6,    player_diea7    ] {};
void()    player_diea7    =    [    $deatha7,    player_diea8    ] {};
void()    player_diea8    =    [    $deatha8,    player_diea9    ] {};
void()    player_diea9    =    [    $deatha9,    player_diea10    ] {};
void()    player_diea10    =    [    $deatha10,    player_diea11    ] {};
void()    player_diea11    =    [    $deatha11,    player_diea11 ] {player_dead();};

void()    player_dieb1    =    [    $deathb1,    player_dieb2    ] {};
void()    player_dieb2    =    [    $deathb2,    player_dieb3    ] {};
void()    player_dieb3    =    [    $deathb3,    player_dieb4    ] {};
void()    player_dieb4    =    [    $deathb4,    player_dieb5    ] {};
void()    player_dieb5    =    [    $deathb5,    player_dieb6    ] {};
void()    player_dieb6    =    [    $deathb6,    player_dieb7    ] {};
void()    player_dieb7    =    [    $deathb7,    player_dieb8    ] {};
void()    player_dieb8    =    [    $deathb8,    player_dieb9    ] {};
void()    player_dieb9    =    [    $deathb9,    player_dieb9    ] {player_dead();};

void()    player_diec1    =    [    $deathc1,    player_diec2    ] {};
void()    player_diec2    =    [    $deathc2,    player_diec3    ] {};
void()    player_diec3    =    [    $deathc3,    player_diec4    ] {};
void()    player_diec4    =    [    $deathc4,    player_diec5    ] {};
void()    player_diec5    =    [    $deathc5,    player_diec6    ] {};
void()    player_diec6    =    [    $deathc6,    player_diec7    ] {};
void()    player_diec7    =    [    $deathc7,    player_diec8    ] {};
void()    player_diec8    =    [    $deathc8,    player_diec9    ] {};
void()    player_diec9    =    [    $deathc9,    player_diec10    ] {};
void()    player_diec10    =    [    $deathc10,    player_diec11    ] {};
void()    player_diec11    =    [    $deathc11,    player_diec12    ] {};
void()    player_diec12    =    [    $deathc12,    player_diec13    ] {};
void()    player_diec13    =    [    $deathc13,    player_diec14    ] {};
void()    player_diec14    =    [    $deathc14,    player_diec15    ] {};
void()    player_diec15    =    [    $deathc15,    player_diec15 ] {player_dead();};

void()    player_died1    =    [    $deathd1,    player_died2    ] {};
void()    player_died2    =    [    $deathd2,    player_died3    ] {};
void()    player_died3    =    [    $deathd3,    player_died4    ] {};
void()    player_died4    =    [    $deathd4,    player_died5    ] {};
void()    player_died5    =    [    $deathd5,    player_died6    ] {};
void()    player_died6    =    [    $deathd6,    player_died7    ] {};
void()    player_died7    =    [    $deathd7,    player_died8    ] {};
void()    player_died8    =    [    $deathd8,    player_died9    ] {};
void()    player_died9    =    [    $deathd9,    player_died9    ] {player_dead();};

void()    player_diee1    =    [    $deathe1,    player_diee2    ] {};
void()    player_diee2    =    [    $deathe2,    player_diee3    ] {};
void()    player_diee3    =    [    $deathe3,    player_diee4    ] {};
void()    player_diee4    =    [    $deathe4,    player_diee5    ] {};
void()    player_diee5    =    [    $deathe5,    player_diee6    ] {};
void()    player_diee6    =    [    $deathe6,    player_diee7    ] {};
void()    player_diee7    =    [    $deathe7,    player_diee8    ] {};
void()    player_diee8    =    [    $deathe8,    player_diee9    ] {};
void()    player_diee9    =    [    $deathe9,    player_diee9    ] {player_dead();};

void()    player_die_ax1    =    [    $axdeth1,    player_die_ax2    ] {};
void()    player_die_ax2    =    [    $axdeth2,    player_die_ax3    ] {};
void()    player_die_ax3    =    [    $axdeth3,    player_die_ax4    ] {};
void()    player_die_ax4    =    [    $axdeth4,    player_die_ax5    ] {};
void()    player_die_ax5    =    [    $axdeth5,    player_die_ax6    ] {};
void()    player_die_ax6    =    [    $axdeth6,    player_die_ax7    ] {};
void()    player_die_ax7    =    [    $axdeth7,    player_die_ax8    ] {};
void()    player_die_ax8    =    [    $axdeth8,    player_die_ax9    ] {};
void()    player_die_ax9    =    [    $axdeth9,    player_die_ax9    ] {player_dead();};

//============================================================================
void() player_death_sound =
{
    // water death sounds
    if (self.waterlevel == 3) {
        death_bubbles(20);
        sound (self, CHAN_VOICE, "player/h2odeath.wav", 1, ATTN_NONE);
    }
    else {
        self.lip = rint ((random() * 4) + 1);
        if (self.lip == 2) self.noise = "player/death2.wav";
        else if (self.lip == 3) self.noise = "player/death3.wav";
        else if (self.lip == 4) self.noise = "player/death4.wav";
        else if (self.lip == 5) self.noise = "player/death5.wav";
        else self.noise = "player/death1.wav";
    
        sound (self, CHAN_VOICE, self.noise, 1, ATTN_NONE);
    }
};

//============================================================================
void() player_gib =
{
    // Set all gib/death flags correctly
    self.gibbed = TRUE;
    self.deadflag = DEAD_DEAD;
    self.takedamage = DAMAGE_NO;
    self.nextthink = -1;

    // Let the head model bounce around
    self.movetype = MOVETYPE_BOUNCE;
    setmodel (self, self.headmdl);
    setsize (self, '-16 -16 0', '16 16 32');
    self.frame = 0;        // Reset frame, head model only has 1 frame
    self.height = 1;    // Do not remove after model fadeout


    ThrowGib(1, 1);
    ThrowGib(2, 1);
    ThrowGib(3, 1);
    ThrowGib(4, rint(random()*2));
    ThrowGib(5, 1);

    // Lots of vertical momentum
    self.velocity_x = 50 * crandom();
    self.velocity_y = 50 * crandom();
    self.velocity_z = 200 + 100 * random();

    // Minimal angle rotation
    self.avelocity = '0 0 0';
    self.avelocity_y = 200 + random() * 200;
    self.angles = '0 0 0';
    self.angles_y = self.ideal_yaw = random() * 360;

    // Play GIB death sound
    if (damage_attacker.classname == "teledeath") 
        sound (self, CHAN_VOICE, "player/teledth1.wav", 1, ATTN_NONE);
    else if (damage_attacker.classname == "teledeath2")
        sound (self, CHAN_VOICE, "player/teledth1.wav", 1, ATTN_NONE);
    else {
        if (random() < 0.5) sound (self, CHAN_VOICE, "player/gib.wav", 1, ATTN_NONE);
        else sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NONE);
    }
};

//============================================================================
void() player_death =
{
    // don't let sbar look bad if a player
    if (self.health < -99) self.health = -99;    
    if (deathmatch || coop) DropBackpack();
    
    // Remove all powerup/debuff effects
    ResetPowerSystem(self);
    ResetDebuffSystem(self);

    self.weaponmodel="";
    self.view_ofs = '0 0 8';
    self.deadflag = DEAD_DYING;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_TOSS;
    
    // Remove floor/ground flag, time to fly
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    setorigin(self, self.origin + '0 0 1');
    
    // Did the player explode from high damage?
    if (self.health < self.gibhealth) player_gib();
    else {
        player_death_sound();
        self.angles_x = self.angles_z = 0;
        if (self.velocity_z < 10)
            self.velocity_z = self.velocity_z + random()*300;
        
        if (self.weapon == IT_AXE) player_die_ax1 ();
        else {
            self.lip = 1 + floor(random()*6);
            if (self.lip == 1) player_diea1();
            else if (self.lip == 2) player_dieb1();
            else if (self.lip == 3) player_diec1();
            else if (self.lip == 4) player_died1();
            else player_diee1();
        }
    }
};

//============================================================================
void() set_suicide_frame =
{    
    // used by kill command and diconnect command
    if (self.gibbed == TRUE) return;    // already gibbed
    self.frame = $deatha11;
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_TOSS;
    self.deadflag = DEAD_DEAD;
    self.nextthink = -1;
};
