// pendulum.qc

$frame frame1 frame2 frame3 frame4 frame5 frame6 frame7
$frame frame8 frame9 frame10 frame11 frame12 frame13

float impactVelocity;

//float pend_frame;

float PEND_X = 1;
float PEND_Y = 2;
float PEND_Z = 4;
float PEND_TRG = 8;

/*
void() pend_stand  =[ $frame4, pend_stand ] {};
*/

void() pend_swing1   =[ $frame1,  pend_swing2  ]
{
////    pend_frame = 1;
//    0 -148 88 (center)    W 56, H 80  (28,40)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -176 48', '8 -120 128' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-176 -8 48', '-120 8 128' );
    self.nextthink = time + 0.17;
    impactVelocity = 1;
};
void() pend_swing2   =[ $frame2,  pend_swing3  ]
{
////    pend_frame = pend_frame + 1;
//    0 -142 50 (center)    W 60, H 76  (30, 38)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -172 12', '8 -112 88' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-172 -8 12', '-112 8 88' );
    self.nextthink = time + 0.15;
};
void() pend_swing3   =[ $frame3,  pend_swing4  ]
{
////    pend_frame = pend_frame + 1;
//    0 -128 14 (center)    W 64, H 72  (32, 36)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -160 -22', '8 -96 50' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-160 -8 -22', '-96 8 50' );
    self.nextthink = time + 0.13;
};
void() pend_swing4   =[ $frame4,  pend_swing5  ]
{
    sound ( self, CHAN_AUTO, "pendulum/swing.wav", 0.5, ATTN_NORM);

////    pend_frame = pend_frame + 1;
//    0 -104 -17 (center)        W 68, H 68  (34, 34)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -138 -51', '8 -70 17' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-138 -8 -51', '-70 8 17' );
    self.nextthink = time + 0.11;
};
void() pend_swing5   =[ $frame5,  pend_swing6  ]
{
//    pend_frame = pend_frame + 1;
//    0 -74 40 (center)        W 72, H 64  (36, 32)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -110 -72', '8 -38 -8' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-110 -8 -72', '-38 8 -8' );
    self.nextthink = time + 0.09;
};
void() pend_swing6   =[ $frame6,  pend_swing7  ]
{
//    pend_frame = pend_frame + 1;
//    0 -38 53 (center)        W 76, H 60  (38, 30)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -76 -83', '8 0 -23' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-76 -8 -83', '0 8 -23' );
    self.nextthink = time + 0.07;
};
void() pend_swing7   =[ $frame7,  pend_swing8  ]
{
//    pend_frame = pend_frame + 1;
//    0 0 60 (center)        W 80, H 56  (40, 28)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -40 -88', '8 40 -32' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-40 -8 -88', '40 8 -32' );
    self.nextthink = time + 0.05;
};
void() pend_swing8   =[ $frame8,  pend_swing9  ]
{
//    pend_frame = pend_frame + 1;
//    0 38 53 (center)        W 76, H 60  (38, 30)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 0 -83', '8 76 -23' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '0 -8 -83', '76 8 -23' );
    self.nextthink = time + 0.07;
};
void() pend_swing9   =[ $frame9,  pend_swing10 ]
{
//    pend_frame = pend_frame + 1;
//    0 74 40 (center)        W 72, H 64  (36, 32)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 38 -72', '8 100 -8' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '38 -8 -72', '100 8 -8' );
    self.nextthink = time + 0.09;
};
void() pend_swing10  =[ $frame10, pend_swing11 ]
{
//    pend_frame = pend_frame + 1;
//    0 104 17 (center)        W 68, H 68  (34, 34)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 70 -51', '8 138 17' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '70 -8 -51', '138 8 17' );
    self.nextthink = time + 0.11;
};
void() pend_swing11  =[ $frame11, pend_swing12 ]
{
//    pend_frame = pend_frame + 1;
//    0 128 -14 (center)        W 64, H 72  (32, 36)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 96 -22', '8 160 50' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '96 -8 -22', '160 8 50' );
    self.nextthink = time + 0.13;
};
void() pend_swing12  =[ $frame12, pend_swing13 ]
{
//    pend_frame = pend_frame + 1;
//    0 142 -50 (center)        W 60, H 76 (30, 38)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 112 12', '8 172 88' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '112 -8 12', '172 8 88' );
    self.nextthink = time + 0.15;
};
void() pend_swing13  =[ $frame13, pend_swing14 ]
{
//    pend_frame = pend_frame + 1;
//    0 148 -88 (center)        W 56, H 80 (28, 40)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 120 48', '8 176 128' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '120 -8 48', '176 8 128' );
    self.nextthink = time + 0.17;
};
void() pend_swing14  =[ $frame13, pend_swing15 ]
{
//    pend_frame = pend_frame + 1;
    // same frame, same location. no change to bbox.
    self.nextthink = time + 0.17;
};

void() pend_swing15  =[ $frame12, pend_swing16 ]
{
//    pend_frame = pend_frame + 1;
//    0 142 -50 (center)        W 60, H 76 (30, 38)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 112 12', '8 172 88' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '112 -8 12', '172 8 88' );
    self.nextthink = time + 0.15;
    impactVelocity = -1;
};
void() pend_swing16  =[ $frame11, pend_swing17 ]
{
//    pend_frame = pend_frame + 1;
//    0 128 -14 (center)        W 64, H 72  (32, 36)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 96 -22', '8 160 50' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '96 -8 -22', '160 8 50' );
    self.nextthink = time + 0.13;
};
void() pend_swing17  =[ $frame10, pend_swing18 ]
{
    sound ( self, CHAN_AUTO, "pendulum/swing.wav", 0.5, ATTN_NORM);

//    pend_frame = pend_frame + 1;
//    0 104 17 (center)        W 68, H 68  (34, 34)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 70 -51', '8 138 17' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '70 -8 -51', '138 8 17' );
    self.nextthink = time + 0.11;
};
void() pend_swing18  =[ $frame9,  pend_swing19 ]
{
//    pend_frame = pend_frame + 1;
//    0 74 40 (center)        W 72, H 64  (36, 32)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 38 -72', '8 100 -8' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '38 -8 -72', '100 8 -8' );
    self.nextthink = time + 0.09;
};
void() pend_swing19  =[ $frame8,  pend_swing20 ]
{
//    pend_frame = pend_frame + 1;
//    0 38 53 (center)        W 76, H 60  (38, 30)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 0 -83', '8 76 -23' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '0 -8 -83', '76 8 -23' );
    self.nextthink = time + 0.07;
};
void() pend_swing20  =[ $frame7,  pend_swing21 ]
{
//    pend_frame = pend_frame + 1;
//    0 0 60 (center)        W 80, H 56  (40, 28)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -40 -88', '8 40 -32' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-40 -8 -88', '40 8 -32' );
    self.nextthink = time + 0.05;
};
void() pend_swing21  =[ $frame6,  pend_swing22 ]
{
//    pend_frame = pend_frame + 1;
//    0 -38 53 (center)        W 76, H 60  (38, 30)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -76 -83', '8 0 -23' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-76 -8 -83', '0 8 -23' );
    self.nextthink = time + 0.07;
};
void() pend_swing22  =[ $frame5,  pend_swing23 ]
{
//    pend_frame = pend_frame + 1;
//    0 -74 40 (center)        W 72, H 64  (36, 32)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -110 -72', '8 -28 -8' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-110 -8 -72', '-28 8 -8' );
    self.nextthink = time + 0.09;
};
void() pend_swing23  =[ $frame4,  pend_swing24 ]
{
//    pend_frame = pend_frame + 1;
//    0 -104 17 (center)        W 68, H 68  (34, 34)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -172 -51', '8 -70 17' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-172 -8 -51', '-70 8 17' );
    self.nextthink = time + 0.11;
};
void() pend_swing24  =[ $frame3,  pend_swing25 ]
{
//    pend_frame = pend_frame + 1;
//    0 -128 -14 (center)    W 64, H 72  (32, 36)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -160 -22', '8 -96 50' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-160 -8 -22', '-96 8 50' );
    self.nextthink = time + 0.13;
};
void() pend_swing25  =[ $frame2,  pend_swing26 ]
{
//    pend_frame = pend_frame + 1;
//    0 -142 -50 (center)    W 60, H 76  (30, 38)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -172 12', '8 -112 88' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-172 -8 12', '-112 8 88' );
    self.nextthink = time + 0.15;
};
void() pend_swing26  =[ $frame1,  pend_swing1  ]
{
//    pend_frame = pend_frame + 1;
//    0 -148 -88 (center)    W 56, H 80  (28,40)
    if(self.spawnflags & PEND_Y)
        setsize ( self, '-8 -176 48', '8 -120 128' );
    else if( self.spawnflags & PEND_X )
        setsize ( self, '-176 -8 48', '-120 8 128' );
    self.nextthink = time + 0.17;
};

void() pend_touch =
{
    if(other.health < 1 || other.takedamage == DAMAGE_NO)
        return;

    if(self.attack_finished < time)
    {
        sound ( self, CHAN_VOICE, "pendulum/hit.wav", 1, ATTN_NORM);
        self.attack_finished = time + 1;
    }

    T_Damage (other, self, self, self.currentammo);

    // Fling the impactee in the direction the blade was swinging.
    if(self.spawnflags & PEND_Y)
    {
//        other.velocity_y = other.velocity_y - impactVelocity * 250;
        other.velocity_y = impactVelocity * -250;
    }
    else if(self.spawnflags & PEND_X)
    {
//        other.velocity_x = other.velocity_x + impactVelocity * 250;
        other.velocity_x = impactVelocity * 250;
    }

    // Fling the impactee up a little bit.
//    other.velocity_z = other.velocity_z + 200;
    other.velocity_z = 200;

    SpawnMeatSpray ( other.origin, other.velocity);
};

void() pend_use =
{
    self.nextthink = time + self.delay;
    self.think = pend_swing1;
};


/*QUAKED pendulum (0 .5 .8) (-8 -24 -100) (8 24 100) PEND_X PEND_Y PEND_TRG
The Pendulum

Values:
delay - amount of time (in seconds) before starting swinging. default 1.
currentammo - amount of damage for each contact. default 5.

Directional Flags: Pendulum swings perpendicular to it's angle. (eg. A pendulum facing east will swing north/south).

Default: PEND_Y

The pendulum angle is set automatically depending on the PEND_ choice.

PEND_X - a pendulum hanging from the ceiling swinging parallel to the X axis.
PEND_Y - a pendulum hanging from the ceiling swinging parallel to the Y axis.

PEND_TRG - the pendulum must be triggered to start swinging. The delay value still affects the pendulum.
*/
void() pendulum =
{
    local vector     center;

    precache_model ("progs/pendulum.mdl");
    precache_sound ("pendulum/swing.wav");
    precache_sound ("pendulum/hit.wav");

    setmodel (self, "progs/pendulum.mdl");

    if(!self.spawnflags)
        self.spawnflags = self.spawnflags + PEND_Y;

    if(self.spawnflags & PEND_Y)
    {
        setsize (self, '-8 -24 -100', '8 24 100');
        self.angles = '0 0 0';
        self.v_angle = '0 0 0';
//        if(self.angles_y != 0 )
//            self.angles_y = 0;
    }
    else if( self.spawnflags & PEND_X )
    {
        setsize (self, '-24 -8 -100', '24 8 100');
        self.angles = '0 270 0';
        self.v_angle = '0 270 0';
//        if(self.angles_y != 270 )
//            self.angles_y = 270;
    }
    else
        error ( "ERROR: Unimplemented Pendulum Type (pendulum.qc)" );

    if(!self.currentammo)
        self.currentammo = 5;
    if(!self.delay)
        self.delay = 1;

    impactVelocity = 0;
    self.solid = SOLID_TRIGGER;
    self.takedamage = DAMAGE_NO;
    self.touch = pend_touch;

    if(self.spawnflags & PEND_TRG)
    {
        self.use = pend_use;
    }
    else
    {
        self.nextthink = time + self.delay;
        self.think = pend_swing1;
    }
};
