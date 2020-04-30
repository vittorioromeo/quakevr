/*======================================================================
 MATHLIB STUFF

 Originally created by FrikaC
 Some functions provided by Kashua & LordHavoc & Preach

======================================================================*/

// for speed...
float pi = 3.14159265;
float OneEightyOverPi = 57.29577;
float PiOverOneEighty = 0.017453;

float() crandom = { return 2*(random() - 0.5); };

// You can set the accuracy of mathlib by changing this value.
// If unset, it defaults to 0.001
float mathlib_accuracy; //accuracy of sqrt & pow

// Definition/list of all math functions
float(float num) mathlib_sqrt;
float(float num) mathlib_sin;
float(float num) mathlib_cos;
float(float a, float b) mathlib_fast_pow;
float (float number, float exp) mathlib_bitshift;
float(float a, float b) mod;
float(float v) anglemod;
float (float y1, float y2) angcomp;
float (float a, float b) mathlib_min;
float (float a, float b) mathlib_max;
float(float minimum, float val, float maximum) mathlib_bound;
vector() mathlib_randomvec;
float (float num) rad2deg;
float (float num) deg2rad;
float(float theta) mathlib_tan;
float(float y, float x) mathlib_atan2;

/*----------------------------------------------------------------------
Will return 0/1 if bitflag is present in bitvalue
----------------------------------------------------------------------*/
float(float bitvalue, float bitflag) mathlib_bitvalue =
{
    return floor( (bitvalue & bitflag) / bitflag);
};

/*----------------------------------------------------------------------
square root
----------------------------------------------------------------------*/
float(float num) mathlib_sqrt =
{
    local float apr;

    //this sets a level of accuracy, it's a global float
    if(mathlib_accuracy <= 0) mathlib_accuracy = 0.001;   

    if (num < mathlib_accuracy) return 0;
    if (num>1) apr = num;
    else apr = 1;
    do {
        apr = (num + (apr * apr)) / (2 * apr);
    }
    while (fabs((apr * apr) - num) > (num * mathlib_accuracy));
    return apr;
};

/*----------------------------------------------------------------------
sine
----------------------------------------------------------------------*/
float(float num) mathlib_sin =
{
    local vector ang,vf,vu,vr;
    
    vf = v_forward;
    vu = v_up;
    vr = v_right;
    
    ang = '0 1 0' * num;
    makevectors(ang);
    num = v_forward_y;
    
    v_forward = vf;
    v_up = vu;
    v_right = vr;
    
    return num;
};

/*----------------------------------------------------------------------
cosine
----------------------------------------------------------------------*/
float(float num) mathlib_cos =
{
    local vector ang,vf,vu,vr;
    
    vf = v_forward;
    vu = v_up;
    vr = v_right;
    
    ang = '0 1 0' * num;
    makevectors(ang);
    num = v_forward_x;
    
    v_forward = vf;
    v_up = vu;
    v_right = vr;
    
    return num;
};

/*----------------------------------------------------------------------
pow

raise to a power

you ought not use this if the power you're raising to is constant
ie, just use r * r * r for cubing, it's a lot faster
----------------------------------------------------------------------*/
float(float a, float b) mathlib_fast_pow =
{
    local float fb, n, bit, factor;
    
    fb = fabs(b);
    bit = 1;
    n = 1;
    factor = a;
    while (bit <= fb)
    {
        if (fb&bit)
            n = n * factor;
        factor = factor * factor;
        bit = bit * 2;
    }
    if (b < 0)
        return 1/n;
    else
        return n;
};

float(float a, float b) mathlib_pow =
{
    local float e1,e2,f,i;
    
    if (mathlib_accuracy <= 0)
        mathlib_accuracy = 0.001; 
    if (fabs(rint(b) - b) <= mathlib_accuracy)
        return mathlib_fast_pow(a, rint(b));
        
    f = (a - 1) / (a + 1);
    //this is the first trick
    //we're essentially doing exp(b*log(a))
    //but the power series for log (1+x) is only defined for small x
    //however log x = 2 * arctanh((x-1)/(x+1)) which will converge for any x we choose

    e1 = 0;                
    e2 = 2 * f;
    i = 1;
    f = f * f;
    
    // this calculates successive terms of arctanh
    //when the absolute value of a term drops
    //below accuracy we call it a day
    //note that this doesn't actually mean
    //the output is accurate to 0.001, there's no
    //direct bound on accuracy
    while(fabs(e2) > mathlib_accuracy)
    {
        e1 = e1 + e2;
        e2 = e2 * f * ((2 * i) - 1) / ((2 * i) + 1);
        i = i + 1;
    }

    f = e2 = e1 * b;
    e1 = 1;
    i = 1;
    //same idea, this is the exponential function
    //which has a nice power series
    //same comments about accuracy apply, except
    //the rapid decay of terms mean it's probably
    //close to the true value of exp f, if not pow(a,b)
    while(fabs(e2) > mathlib_accuracy)
    {
        e1 = e1 + e2;
        i = i + 1;
        e2 = e2 * f / i;
    }
    return e1;
} ;

/*----------------------------------------------------------------------
Bitshift
----------------------------------------------------------------------*/
float (float number, float exp) mathlib_bitshift =
{
    local float bit, fexp;
    bit = 1;
    fexp = fabs(exp);
    number = rint(number);
    
    while (bit <= fexp) {
        if (fexp & bit) {
            if (exp > 0) number = number * bit * 2;
            else number = floor(number / (bit * 2));
        }
        bit = bit * 2;
    }
    return number;
};
/*----------------------------------------------------------------------
Remainder

Similar to C's % operator, but deals with
negative numbers differently.
----------------------------------------------------------------------*/
float(float a, float b) mod =
{
    return a - (floor(a / b) * b);
};

/*----------------------------------------------------------------------
mathlib_anglemod 

faster version of id's anglemod
----------------------------------------------------------------------*/
float(float v) anglemod =
{
    return v - floor(v/360) * 360;
};

//----------------------------------------------------------------------
// Original version from ai.qc
/*float(float v) anglemod =
{
    while (v >= 360)
        v = v - 360;
    while (v < 0)
        v = v + 360;
    return v;
};
*/

/*----------------------------------------------------------------------
angcomp (part of FrikBot)

subtracts one angle from another
----------------------------------------------------------------------*/
float (float y1, float y2) angcomp = 
{
    y1 = anglemod(y1);
    y2 = anglemod(y2);

    local float answer;
    answer = y1 - y2;
    if (answer > 180)
        answer = answer - 360;
    else if (answer < -180)
        answer = answer + 360;
    return answer;
};

/*----------------------------------------------------------------------
min
Returns the lesser of two (or more) numbers
----------------------------------------------------------------------*/
float (float a, float b) mathlib_min =
{
    if (a<b)
        return a;
    else
        return b;
};

float (float a, float b, float c) mathlib_min3 =
{
    if (a<b)
    {
        if (c<a)
            return c;
        else
            return a;
    }
    else
    {
        if (c<b)
            return c;
        else
            return b;
    }
};

float (float a, float b, float c, float d) mathlib_min4 =
{
    return mathlib_min(mathlib_min3(a,b,c),d);
};
float (float a, float b, float c, float d, float e) mathlib_min5 =
{
    return mathlib_min3(mathlib_min3(a,b,c),d,e);
};
float (float a, float b, float c, float d, float e, float f) mathlib_min6 =
{
    return mathlib_min(mathlib_min3(a,b,c), mathlib_min3(d,e,f));
};
float (float a, float b, float c, float d, float e, float f, float g) mathlib_min7 =
{
    return mathlib_min3(mathlib_min3(a,b,c), mathlib_min3(d,e,f), g);
};
float (float a, float b, float c, float d, float e, float f, float g, float h) mathlib_min8 =
{
    return mathlib_min3(mathlib_min3(a,b,c), mathlib_min3(d,e,f), mathlib_min(g, h));
};

/*----------------------------------------------------------------------
max
Returns the greater of two (or more) numbers
----------------------------------------------------------------------*/
float (float a, float b) mathlib_max =
{
    if (a>b)
        return a;
    else
        return b;
};
float (float a, float b, float c) mathlib_max3 =
{
    if (a>b)
    {
        if (c>a)
            return c;
        else
            return a;
    }
    else
    {
        if (c>b)
            return c;
        else
            return b;
    }
};
float (float a, float b, float c, float d) mathlib_max4 =
{
    return mathlib_max(mathlib_max3(a,b,c),d);
};
float (float a, float b, float c, float d, float e) mathlib_max5 =
{
    return mathlib_max3(mathlib_max3(a,b,c),d,e);
};
float (float a, float b, float c, float d, float e, float f) mathlib_max6 =
{
    return mathlib_max(mathlib_max3(a,b,c), mathlib_max3(d,e,f));
};
float (float a, float b, float c, float d, float e, float f, float g) mathlib_max7 =
{
    return mathlib_max3(mathlib_max3(a,b,c), mathlib_max3(d,e,f), g);
};
float (float a, float b, float c, float d, float e, float f, float g, float h) mathlib_max8 =
{
    return mathlib_max3(mathlib_max3(a,b,c), mathlib_max3(d,e,f), mathlib_max(g, h));
};

/*----------------------------------------------------------------------
bound
Returns a number bound to certain limits
----------------------------------------------------------------------*/
float(float minimum, float val, float maximum) mathlib_bound =
{
    if (val<minimum)
        val=minimum;
    else if (val>minimum)
        val=maximum;
    return val;
};

/*----------------------------------------------------------------------
randomvec
Returns a random vector of length < 1
----------------------------------------------------------------------*/
vector () mathlib_randomvec =
{
    local vector v;
    do
    {
        v_x = random() * 2 - 1;
        v_y = random() * 2 - 1;
        v_z = random() * 2 - 1;
    }
    while(vlen(v) > 1);
    return v;
};

/*----------------------------------------------------------------------
Radians <--> Degrees

Simple functions that convert radians to
degrees.
----------------------------------------------------------------------*/
float (float num) rad2deg = { return num*OneEightyOverPi; };
float (float num) deg2rad = { return num*PiOverOneEighty; };

/*----------------------------------------------------------------------
TAN
Changes v_forward, v_right, v_up
----------------------------------------------------------------------*/
float(float theta) mathlib_tan =
{
  local vector ang; //temporary used to calculate trig values
  ang = '0 0 0';
  ang_y = theta; //assign theta to the yaw to simplify reasoning
  makevectors(ang);
  return v_forward_y / v_forward_x;
};

/*----------------------------------------------------------------------
Inverse TAN
----------------------------------------------------------------------*/
float(float y, float x) mathlib_atan2 =
{
  local vector ang; //temporary used to calculate trig values
  ang = '0 0 0';
  ang_x = x;
  ang_y = y;
  return vectoyaw(ang);
};
 
/* *********************************************************

!!!FTEQCC or FrikQCC only code follows !!!

This code allows the engine to accelerate math functions

If not needed, you can simply delete the rest of the file
and call the mathlib functions above directly.

It checks for the extensions DP_QC_MINMAXBOUND,
DP_QC_RANDOMVEC, DP_QC_SINCOSSQRTPOW, EXT_BITSHIFT

********************************************************* */
/*
var float (float num) sqrt;
var float (float num) sin;
var float (float num) cos;
var float (float num, float exp) pow;
var float (float number, float exp) bitshift;
var float (float a, float b) min;
var float (float a, float b, float c) min3;
var float (float a, float b, float c, float d) min4;
var float (float a, float b, float c, float d, float e) min5;
var float (float a, float b, float c, float d, float e, float f) min6;
var float (float a, float b, float c, float d, float e, float f, float g) min7;
var float (float a, float b, float c, float d, float e, float f, float g, float h) min8;
var float (float a, float b) max;
var float (float a, float b, float c) max3;
var float (float a, float b, float c, float d) max4;
var float (float a, float b, float c, float d, float e) max5;
var float (float a, float b, float c, float d, float e, float f) max6;
var float (float a, float b, float c, float d, float e, float f, float g) max7;
var float (float a, float b, float c, float d, float e, float f, float g, float h) max8;
var float (float minimum, float val, float maximum) bound;
var vector() randomvec;

float(float val) ext_sin = #60;
float(float val) ext_cos = #61;
float(float val) ext_sqrt = #62;
vector() ext_randomvec = #91;
float(float a, float b) ext_min = #94;
float(float a, float b, float c) ext_min3 = #94;
float(float a, float b, float c, float d) ext_min4 = #94;
float(float a, float b, float c, float d, float e) ext_min5 = #94;
float(float a, float b, float c, float d, float e, float f) ext_min6 = #94;
float(float a, float b, float c, float d, float e, float f, float g) ext_min7 = #94;
float(float a, float b, float c, float d, float e, float f, float g, float h) ext_min8 = #94;
float(float a, float b) ext_max = #95;
float(float a, float b, float c) ext_max3 = #95;
float(float a, float b, float c, float d) ext_max4 = #95;
float(float a, float b, float c, float d, float e) ext_max5 = #95;
float(float a, float b, float c, float d, float e, float f) ext_max6 = #95;
float(float a, float b, float c, float d, float e, float f, float g) ext_max7 = #95;
float(float a, float b, float c, float d, float e, float f, float g, float h) ext_max8 = #95;
float(float minimum, float val, float maximum) ext_bound = #96;
float(float a, float b) ext_pow = #97;
float(float number, float quantity) ext_bitshift = #218;

// This is DP extension, may need to comment out if already defined.
float(string s) checkextension = #99;

// The engine sin/cos builtins use radians rather than degrees. This is less useful, so convert
float(float val) wrap_sin = { return ext_sin(val*PiOverOneEighty); };
float(float val) wrap_cos = { return ext_cos(val*PiOverOneEighty); };

// this must be called in worldspawn before using any above functions
void () MathlibCheckBuiltins =
{
    local float checkext;

    sqrt = mathlib_sqrt;
    cos = mathlib_cos;
    sin = mathlib_sin;
    pow = mathlib_pow;
    randomvec = mathlib_randomvec;
    min = mathlib_min;
    min3 = mathlib_min3;
    min4 = mathlib_min4;
    min5 = mathlib_min5;
    min6 = mathlib_min6;
    min7 = mathlib_min7;
    min8 = mathlib_min8;
    max = mathlib_max;
    max3 = mathlib_max3;
    max4 = mathlib_max4;
    max5 = mathlib_max5;
    max6 = mathlib_max6;
    max7 = mathlib_max7;
    max8 = mathlib_max8;
    bound = mathlib_bound;
    bitshift = mathlib_bitshift;
    checkext = cvar("pr_checkextension");
    if (checkext)
    {
        if (checkextension("DP_QC_SINCOSSQRTPOW"))
        {
            sqrt = ext_sqrt;
            cos = wrap_cos; 
            sin = wrap_sin;
            pow = ext_pow;
        }

        if (checkextension("DP_QC_RANDOMVEC"))
            randomvec = ext_randomvec;
        if (checkextension("DP_QC_MINMAXBOUND"))
        {
            min = ext_min;
            min3 = ext_min3;
            min4 = ext_min4;
            min5 = ext_min5;
            min6 = ext_min6;
            min7 = ext_min7;
            min8 = ext_min8;
            max = ext_max;
            max3 = ext_max3;
            max4 = ext_max4;
            max5 = ext_max5;
            max6 = ext_max6;
            max7 = ext_max7;
            max8 = ext_max8;
            bound = ext_bound;
        }
        if (checkextension("EXT_BITSHIFT"))
            bitshift = ext_bitshift;
    }
};
*/

