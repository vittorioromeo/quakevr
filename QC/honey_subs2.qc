

void(entity t) D_SK_debugSpline;

//Does a shitty modulo operation.
float(float v, float m) mod =
{
    float r;

    if(m == 0) objerror("Illegal op: mod(x, 0)!");
    if(fabs(v) < m) return v;
    if(v == m) return 0;

    r = floor(v / m) * m;

    return v - r;
}

vector(vector v, float m) modvec =
{
    v_x = mod(v_x, m);
    v_y = mod(v_y, m);
    v_z = mod(v_z, m);

    return v;
}

float(float v, float min, float max) clamp =
{
    if(v < min) return min;
    if(v > max) return max;
    return v;
}

float (float from, float to, float fraction) tween_f =
{
    return (from * (1 - fraction)) + (to * fraction);
}

vector (vector from, vector to, float fraction) tween_v =
{
    local vector res;

    res_x = tween_f(from_x, to_x, fraction);
    res_y = tween_f(from_y, to_y, fraction);
    res_z = tween_f(from_z, to_z, fraction);

    return res;
}

float (float from, float to, float fraction) tween_f_sq =
{

    fraction = 1 - fraction;
    fraction = fraction * fraction;
    fraction = 1 - fraction;

    return (to * (1 - fraction)) + (from * fraction);
}

vector (vector from, vector to, float fraction) tween_v_sq =
{
    local vector res;

    res_x = tween_f_sq(from_x, to_x, fraction);
    res_y = tween_f_sq(from_y, to_y, fraction);
    res_z = tween_f_sq(from_z, to_z, fraction);

    return res;
}


/*
=============
stuffcmd_float

This is a horrible hack that I am ashamed of!
===============
*/
void stuffcmd_digit(entity client, float f) =
{
    float d;
    d = floor(f);
    d = mod(d, 10);

    //CLOSE YOUR EYES, HONEY! DON'T LOOK!
    if(d == 0)
        stuffcmd(client, "0");
    else if(d == 1)
        stuffcmd(client, "1");
    else if(d == 2)
        stuffcmd(client, "2");
    else if(d == 3)
        stuffcmd(client, "3");
    else if(d == 4)
        stuffcmd(client, "4");
    else if(d == 5)
        stuffcmd(client, "5");
    else if(d == 6)
        stuffcmd(client, "6");
    else if(d == 7)
        stuffcmd(client, "7");
    else if(d == 8)
        stuffcmd(client, "8");
    else if(d == 9)
        stuffcmd(client, "9");
}

void stuffcmd_int(entity client, float f, float numdigits) =
{

    float tmp;


    if(f == 0){
        stuffcmd(client, "0");
        return;
    }

    if(f < 0){
        // Yeah sure.
        stuffcmd(client, "-");
        f = fabs(f);
    }

    if(numdigits <= 0){
        tmp = f;
        numdigits = 1;
        while(tmp >= 1){
            tmp = tmp / 10;
            numdigits = numdigits * 10;
        }
    }

    //I don't know what I'm thinking here...
    //I need to do this to get zero-padding to work.

    while(numdigits > 1){

        numdigits = numdigits / 10;
        tmp = f / numdigits;
        stuffcmd_digit(client, tmp);

    }


}

void stuffcmd_float(entity client, float f) =
{
    float intpart, decpart, mul, isNegative;

    isNegative = FALSE;

    if(f == 0){
        stuffcmd(client, "0");
        return;
    }

    if(f < 0){
        // easier this way
        isNegative = TRUE;
        f = fabs(f);
    }


    // 1: stuff the integer part.
    intpart = floor(f);
    if(isNegative)
        stuffcmd(client, "-");
    stuffcmd_int(client, intpart, 0);


    // 2: stuff the decimal point.
    stuffcmd(client, ".");


    // 3: stuff the decimal part.
    decpart = mod(f, 1);
    decpart = decpart * 10000;
    stuffcmd_int(client, decpart, 10000);

}

/*
=============
dprint_float

This is a horrible hack that I am ashamed of!
===============
*/
void dprint_digit(float f) =
{
    float d;
    d = floor(f);
    d = mod(d, 10);

    //CLOSE YOUR EYES, HONEY! DON'T LOOK!
    if(d == 0)
        dprint("0");
    else if(d == 1)
        dprint("1");
    else if(d == 2)
        dprint("2");
    else if(d == 3)
        dprint("3");
    else if(d == 4)
        dprint("4");
    else if(d == 5)
        dprint("5");
    else if(d == 6)
        dprint("6");
    else if(d == 7)
        dprint("7");
    else if(d == 8)
        dprint("8");
    else if(d == 9)
        dprint("9");
}

void dprint_int(float f, float numdigits) =
{

    float tmp;


    if(f == 0){
        dprint("0");
        return;
    }

    if(f < 0){
        // Yeah sure.
        dprint("-");
        f = fabs(f);
    }

    if(numdigits <= 0){
        tmp = f;
        numdigits = 1;
        while(tmp >= 1){
            tmp = tmp / 10;
            numdigits = numdigits * 10;
        }
    }

    //I don't know what I'm thinking here...
    //I need to do this to get zero-padding to work.

    while(numdigits > 1){

        numdigits = numdigits / 10;
        tmp = f / numdigits;
        dprint_digit(tmp);

    }


}

void dprint_float(float f) =
{
    float intpart, decpart, mul, isNegative;

    isNegative = FALSE;

    if(f == 0){
        dprint("0");
        return;
    }

    if(f < 0){
        // easier this way
        isNegative = TRUE;
        f = fabs(f);
    }


    // 1: stuff the integer part.
    intpart = floor(f);
    if(isNegative)
        dprint("-");
    dprint_int(intpart, 0);


    // 2: stuff the decimal point.
    dprint(".");


    // 3: stuff the decimal part.
    decpart = mod(f, 1);
    decpart = decpart * 10000;
    dprint_int(decpart, 10000);

}

void dprint_str_f_str(string s1,  float f, string s2) =
{
    dprint(s1);
    dprint_float(f);
    dprint(s2);
}

void dprint_str_v_str(string s1,  vector v, string s2) =
{
    dprint(s1);
    dprint("[");
    dprint_float(v_x);
    dprint(", ");
    dprint_float(v_y);
    dprint(", ");
    dprint_float(v_z);
    dprint("]");
    dprint(s2);
}

/*
=============
SUB_RotatePoint

rotates a point around origin.
=============
*/
vector(vector point, vector toAngles) SUB_RotatePoint =
{
    local vector ang, noZ;
    local float oldZ;

    dprint_str_v_str("rotating point ", point, " around angles");
    dprint_str_v_str(" ", toAngles, "\n");

    oldZ = point_z;
    noZ = point;
    noZ_z = 0; //ignore Z-axis
    toAngles_x = 0; //no pitching

    ang = vectoangles(noZ);
    makevectors (toAngles + ang);

    v_forward = normalize(v_forward);
    noZ = v_forward * vlen(point);
    noZ_z = oldZ;

    dprint_str_v_str("Got ", noZ, "\n");
    return noZ;


}

/*
=============
SUB_CalcMoveRotate

calculate self.velocity, self.avelocity and self.nextthink to reach tdest at tangle from
self.origin traveling at tspeed
===============
*/

void() SUB_CalcMoveRotateDone;
void(vector tdest, float tspeed, vector tangle, void() func) SUB_CalcMoveRotate;
void(vector tdest, float ttime, vector tangle, void() func) SUB_CalcMoveRotateTime;

void(entity ent, vector tdest, float tspeed, vector tangle, void() func) SUB_CalcMoveRotateEnt =
{
local entity    stemp;
    stemp = self;
    self = ent;

    SUB_CalcMoveRotate (tdest, tspeed, tangle, func);
    self = stemp;
}

void(entity ent, vector tdest, float ttime, vector tangle, void() func) SUB_CalcMoveRotateTimeEnt =
{
local entity    stemp;
    stemp = self;
    self = ent;

    SUB_CalcMoveRotate (tdest, ttime, tangle, func);
    self = stemp;
}

void(vector tdest, float tspeed, vector tangle, void() func) SUB_CalcMoveRotate =
{
    local vector    vdestdelta, adestdelta;
    local float        len, traveltime;

    if(!tspeed)
        objerror("No speed is defined!");

    self.think1 = func;
    self.finaldest = tdest;
    //self.finalangle = modvec(tangle, 360); //Causes jerkiness
    self.finalangle = tangle;
    self.think = SUB_CalcMoveRotateDone;



    if(tdest == self.origin)
    {
        self.velocity = '0 0 0';
        self.angles = tangle;
        self.avelocity = '0 0 0';
        self.nextthink = time + 0.1;
        return;
    }

    // set adestdelta to the angle needed to move
    adestdelta = tangle - self.angles;
    //Do we really need to rotate so much?
    if(adestdelta_x > 180) adestdelta_x = adestdelta_x - 360;
    else if(adestdelta_x < -180) adestdelta_x = adestdelta_x + 360;

    if(adestdelta_y > 180) adestdelta_y = adestdelta_y - 360;
    else if(adestdelta_y < -180) adestdelta_y = adestdelta_y + 360;

    if(adestdelta_z > 180) adestdelta_z = adestdelta_z - 360;
    else if(adestdelta_z < -180) adestdelta_z = adestdelta_z + 360;



    // set vdestdelta to the vector needed to move
    vdestdelta = tdest - self.origin;

    // calculate length of vector
    len = vlen(vdestdelta);
    // divide by speed to get time to reach dest
    traveltime = len / tspeed;

    if(traveltime < 0.1)
    {
        self.velocity = '0 0 0';
        self.nextthink = time + 0.1;
        return;
    }

    // set nextthink to trigger a think when dest is reached
    self.nextthink = time + traveltime;

    // scale the destdelta vector by the time spent traveling to get velocity
    self.velocity = vdestdelta * (1/traveltime);    // qcc won't take vec/float
    // scale the destdelta vector by the time spent traveling to get velocity
    self.avelocity = adestdelta * (1 / traveltime);


}

void(vector tdest, float ttime, vector tangle, void() func) SUB_CalcMoveRotateTime =
{
    local vector    vdestdelta, adestdelta;
    local float        len, travelspeed;

    if(!ttime)
        objerror("No time is defined!");

    self.think1 = func;
    self.finaldest = tdest;
    //self.finalangle = modvec(tangle, 360); //Causes jerkiness
    self.finalangle = tangle;
    self.think = SUB_CalcMoveRotateDone;



    if(tdest == self.origin)
    {
        self.velocity = '0 0 0';
        self.angles = tangle;
        self.avelocity = '0 0 0';
        self.nextthink = time + 0.1;
        return;
    }

    // set adestdelta to the angle needed to move
    adestdelta = self.finalangle - self.angles;

//*    //Do we really need to rotate so much?
    if(adestdelta_x > 180) adestdelta_x = adestdelta_x - 360;
    else if(adestdelta_x < -180) adestdelta_x = adestdelta_x + 360;

    if(adestdelta_y > 180) adestdelta_y = adestdelta_y - 360;
    else if(adestdelta_y < -180) adestdelta_y = adestdelta_y + 360;

    if(adestdelta_z > 180) adestdelta_z = adestdelta_z - 360;
    else if(adestdelta_z < -180) adestdelta_z = adestdelta_z + 360;
//*/


    // set vdestdelta to the vector needed to move
    vdestdelta = tdest - self.origin;

    // calculate length of vector
    len = vlen(vdestdelta);
    // divide by time to get speed to reach dest
    travelspeed = len / ttime;


    // set nextthink to trigger a think when dest is reached
    self.nextthink = time + ttime;

    // scale the destdelta vector by the time spent traveling to get velocity
    self.velocity = vdestdelta * (1/ttime);    // qcc won't take vec/float
    // scale the destdelta vector by the time spent traveling to get velocity
    self.avelocity = adestdelta * (1 / ttime);
}

void(vector tdest, float ttime, void() func) SUB_CalcMoveTime =
{
    local vector    vdestdelta;
    local float        len, travelspeed;

    if(!ttime)
        objerror("No time is defined!");

    self.think1 = func;
    self.finaldest = tdest;
    self.think = SUB_CalcMoveRotateDone;



    if(tdest == self.origin)
    {
        self.velocity = '0 0 0';
        self.nextthink = time + 0.1;
        return;
    }

    // set vdestdelta to the vector needed to move
    vdestdelta = tdest - self.origin;

    // calculate length of vector
    len = vlen(vdestdelta);
    // divide by time to get speed to reach dest
    travelspeed = len / ttime;


    // set nextthink to trigger a think when dest is reached
    self.nextthink = time + ttime;

    // scale the destdelta vector by the time spent traveling to get velocity
    self.velocity = vdestdelta * (1/ttime);    // qcc won't take vec/float

}

/*
============
After moving, set origin and angles to exact final destination
============
*/
void()  SUB_CalcMoveRotateDone =
{
    setorigin(self, self.finaldest);
    self.velocity = '0 0 0';
    self.angles = self.finalangle;
    self.avelocity = '0 0 0';

    self.nextthink = -1;
    if(self.think1)
        self.think1();
}


/*
============
Spline movement?!
============
*/

//New fields:
.vector    s_p0, s_p1, s_p2, s_p3; // The four points of the spline
.vector angle_ofs;        // Angle offset the entity should have to spline tangent (To make it appear it's moving backwards, sideways, etc)
.vector s_finalangle;         // Angle the entity should end at (This is used to calculate .s_p2)
.float numTicks, currentTick;   // Number of 0.1 second ticks the movement takes, and current tick.
.void() think2;            // What to ACTUALLY do when the movement ends (SUB_CalcMoveRotateTime overwrites .think1)



void() SUB_CalcMoveSplineTick;
void() SUB_CalcMoveSplineTick_norotate;
void() SUB_CalcMoveSplineDone;
void() SUB_CalcMoveSplineDone_norotate;


void(vector tdest, vector tangle, float ttime, float cpOffset, vector aOffset, void() func) SUB_CalcMoveSpline =
{
    local vector tmp, a;

    if(!cpOffset) cpOffset = 128;

    if(!ttime)
        objerror("No time is defined!");


    //dprint_str_v_str("Starting angle = ", self.angles, "\n");

    self.think2 = func; //What to do when done

    self.numTicks = ttime * 10; //Number of interpolation points
    self.currentTick = 0;

    /** Set up the control points **/

    self.s_p0 = self.origin; //First point

    a = self.angles;
    a_x = a_x * -1;
    makevectors(a);
    tmp = v_forward * cpOffset;
    self.s_p1 = self.s_p0 + tmp; //Second point

    self.s_p3 = tdest; //Final point

    a = tangle;
    a_x = a_x * -1;
    makevectors(a);
    tmp = v_forward * cpOffset;
    self.s_p2 = self.s_p3 - tmp; //Third point



    self.angle_ofs = aOffset;
    self.s_finalangle = tangle;

    self.think = SUB_CalcMoveSplineTick;
    SUB_CalcMoveSplineTick();
}

void(vector tdest, vector tangle, float ttime, float cpOffset, void() func) SUB_CalcMoveSpline_norotate =
{
    // .oldorigin is used as angles
    local vector tmp, a;

    if(!cpOffset) cpOffset = 128;

    if(!ttime)
        objerror("No time is defined!");


    //dprint_str_v_str("Starting angle = ", self.angles, "\n");

    self.think2 = func; //What to do when done

    self.numTicks = ttime * 10; //Number of interpolation points
    self.currentTick = 0;

    /** Set up the control points **/

    self.s_p0 = self.origin; //First point

    a = self.oldorigin;
    a_x = a_x * -1;
    makevectors(a);
    tmp = v_forward * cpOffset;
    self.s_p1 = self.s_p0 + tmp; //Second point

    self.s_p3 = tdest; //Final point

    a = tangle;
    a_x = a_x * -1;
    makevectors(a);
    tmp = v_forward * cpOffset;
    self.s_p2 = self.s_p3 - tmp; //Third point



    self.s_finalangle = tangle;

    self.think = SUB_CalcMoveSplineTick_norotate;
    SUB_CalcMoveSplineTick_norotate();
}

//USE CAREFULLY, SPAWNS LOTS OF ENTITIES!
void(vector tdest, vector tangle, float ttime, float cpOffset, vector aOffset, void() func) SUB_CalcMoveSplineDEBUG =
{
    SUB_CalcMoveSpline(tdest,tangle,ttime,cpOffset,aOffset,func);
    D_SK_debugSpline(self);
}

vector() InterpolatePoint;
vector() InterpolateAngle;

//FIXME: Try to eliminate all the crazy jerkings at start and end.
void() SUB_CalcMoveSplineTick =
{
    vector nextOrigin, nextAngle;

    self.currentTick = self.currentTick + 1;

    if(self.currentTick < self.numTicks){
        nextOrigin = InterpolatePoint();
        nextAngle = InterpolateAngle();

        SUB_CalcMoveRotateTime(nextOrigin, 0.1, nextAngle, SUB_CalcMoveSplineTick);
    }else{
        nextOrigin = self.s_p3;
        nextAngle = self.s_finalangle;

        SUB_CalcMoveRotateTime(nextOrigin, 0.1, nextAngle, SUB_CalcMoveSplineDone);
    }


}

void() SUB_CalcMoveSplineTick_norotate =
{
    vector nextOrigin;

    self.currentTick = self.currentTick + 1;

    if(self.currentTick < self.numTicks){
        nextOrigin = InterpolatePoint();

        SUB_CalcMove_time(nextOrigin, 0.1, SUB_CalcMoveSplineTick_norotate);
    }else{
        nextOrigin = self.s_p3;

        SUB_CalcMove_time(nextOrigin, 0.1, SUB_CalcMoveSplineDone_norotate);
    }


}

void() SUB_CalcMoveSplineDone =
{
    local void() doIt; //Amazed that this actually works...
    setorigin(self, self.s_p3);
    self.velocity = '0 0 0';
    self.angles = self.s_finalangle;
    self.avelocity = '0 0 0';

    self.nextthink = -1;
    if(self.think2){
        doIt = self.think2;
        self.think2 = SUB_Null;
        doIt();
    }
}

void() SUB_CalcMoveSplineDone_norotate =
{
    local void() doIt; //Amazed that this actually works...
    setorigin(self, self.s_p3);
    self.velocity = '0 0 0';
    self.oldorigin = self.s_finalangle;

    self.nextthink = -1;
    if(self.think2){
        doIt = self.think2;
        self.think2 = SUB_Null;
        doIt();
    }
}


//Actual spline math bits below here:

//Basis Functions:
float(float t) B1 = { return t * t * t;}
float(float t) B2 = { return 3 * t * t * (1 - t);}
float(float t) B3 = { return 3 * t * (1 - t) * (1 - t);}
float(float t) B4 = { return (1 - t) * (1 - t) * (1 - t);}

vector() InterpolatePoint =
{
    float t;
    vector ret;

    t = 1 - (self.currentTick / self.numTicks);

    ret = self.s_p0 * B1(t);
    ret = ret + self.s_p1 * B2(t);
    ret = ret + self.s_p2 * B3(t);
    ret = ret + self.s_p3 * B4(t);

    return ret;
}

//vector(vector a, vector b) AngleMatch;

vector() InterpolateAngle =
{
    float t;

    t = self.currentTick / self.numTicks;

    vector A; A = self.s_p0 + (t * (self.s_p1 - self.s_p0));
    vector B; B = self.s_p1 + (t * (self.s_p2 - self.s_p1));
    vector C; C = self.s_p2 + (t * (self.s_p3 - self.s_p2));

    vector AB; AB = B - A;
    vector BC; BC = C - B;

    vector DE; DE = (B + (BC * t)) - (A + (AB * t));

    DE = normalize(DE);

    //dprint("For tangent ");
    //dprint(vtos(DE));
    //dprint(" the angle is ");

    DE = vectoangles(DE);
    DE = DE + self.angle_ofs;
    //dprint(vtos(DE));
    //dprint("\n");

    return DE;
}


/*
============
Utility for invoking a function call later. Costs one entity.
============
*/

void() SUB_DoLater_think =
{
    entity oself, oactivator;



    oself = self;
    self = self.enemy;

    oactivator = activator;
    activator = oself.movetarget;

    //self.think1 = oself.think1; // This doesn't work if world calls SUB_DoLater
    oself.think1(); //this looks confusing because it looks like we're calling a method on the tmp entity,
                    //but we're just calling the function and self is still self

    self = oself;
    activator = oactivator;

    SUB_Remove();
}

void(void() func, float later) SUB_DoLater =
{
    if(later <= 0){
        func();
        return;
    }

    entity tmp;
    tmp = spawn();
    tmp.think1 = func;
    tmp.enemy = self;
    tmp.movetarget = activator;
    tmp.think = SUB_DoLater_think;
    tmp.nextthink = time + later;
}

void() SUB_DoLaterSelf_think =
{
    entity oactivator;

    oactivator = activator;
    activator = self.movetarget;

    self.think1();

    activator = oactivator;
}

void(void() func, float later) SUB_DoLaterSelf =
{
    if(later <= 0){
        func();
        return;
    }
    self.think1 = func;
    self.movetarget = activator;
    self.think = SUB_DoLaterSelf_think;
    self.nextthink = time + later;
}


//
// Some shitty sine table?
//

float sin_tbl (float v) =
{
    local float r;
    v = mod(v, 20);
//*
         if(v <=    1) r =  0;
    else if(v <=    2) r =  0.30901699437495;
    else if(v <=    3) r =  0.58778525229247;
    else if(v <=    4)    r =  0.80901699437495;
    else if(v <=    5)    r =  0.95105651629515;
    else if(v <=    6)    r =  1;
    else if(v <=    7)    r =  0.95105651629515;
    else if(v <=    8)    r =  0.80901699437495;
    else if(v <=    9)    r =  0.58778525229247;
    else if(v <=   10)    r =  0.30901699437495;
    else if(v <=   11) r =  0;
    else if(v <=   12)    r = -0.30901699437495;
    else if(v <=   13)    r = -0.58778525229247;
    else if(v <=   14)    r = -0.80901699437495;
    else if(v <=   15)    r = -0.95105651629515;
    else if(v <=   16)    r = -1;
    else if(v <=   17)    r = -0.95105651629515;
    else if(v <=   18)    r = -0.80901699437495;
    else if(v <=   19)    r = -0.58778525229247;
    else if(v <=   20)    r = -0.30901699437495;
    else                r =  0;
//*/
/* //Doesn't work?
         if(v <=    1) r =  1;
    else if(v <=    2) r =  0.95105651629515;
    else if(v <=    3) r =  0.80901699437495;
    else if(v <=    4)    r =  0.58778525229247;
    else if(v <=    5)    r =  0.30901699437495;
    else if(v <=    6)    r =  0;
    else if(v <=    7)    r = -0.30901699437495;
    else if(v <=    8)    r = -0.58778525229247;
    else if(v <=    9)    r = -0.80901699437495;
    else if(v <=   10)    r = -0.95105651629515;
    else if(v <=   11) r = -1;
    else if(v <=   12)    r = -0.95105651629515;
    else if(v <=   13)    r = -0.80901699437495;
    else if(v <=   14)    r = -0.58778525229247;
    else if(v <=   15)    r = -0.30901699437495;
    else if(v <=   16)    r =  0;
    else if(v <=   27)    r =  0.30901699437495;
    else if(v <=   18)    r =  0.58778525229247;
    else if(v <=   19)    r =  0.80901699437495;
    else if(v <=   20)    r =  0.95105651629515;
    else                r =  1;
*/
    return r;
}

float sin_interpolate (float v) =
{
    local float y1, y2, frac;

    y1 = sin_tbl(v);
    y2 = sin_tbl(v+1);

    frac = mod(v, 1);

    return tween_f (y1, y2, frac);
}


