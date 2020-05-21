//
// builtin functions
//

void(vector ang)    makevectors        = #1;        // sets v_forward, etc globals
void(entity e, vector o) setorigin    = #2;
void(entity e, string m) setmodel    = #3;        // set movetype and solid first
void(entity e, vector min, vector max) setsize = #4;
// #5 was removed
void() break                        = #6;
float() random                        = #7;        // returns 0 - 1

// Frikbot overrides the following:
// void(entity e, float chan, string samp, float vol, float atten) sound = #8;

vector(vector v) normalize            = #9;
void(string e) error                = #10;
void(string e) objerror                = #11;
float(vector v) vlen                = #12;
float(vector v) vectoyaw            = #13;
entity() spawn                        = #14;
void(entity e) remove                = #15;

// sets trace_* globals
// nomonsters can be:
// An entity will also be ignored for testing if forent == test,
// forent->owner == test, or test->owner == forent
// a forent of world is ignored
void(vector v1, vector v2, float nomonsters, entity forent) traceline = #16;

entity() checkclient                = #17;    // returns a client to look for
entity(entity start, .string fld, string match) find = #18;
string(string s) precache_sound        = #19;
string(string s) precache_model        = #20;

// Frikbot overrides the following:
// void(entity client, string s)stuffcmd = #21;

entity(vector org, float rad) findradius = #22;
void(string s) bprint                = #23;

// Frikbot overrides the following:
// void(entity client, string s) sprint = #24;

void(string s) dprint                = #25;
string(float f) ftos                = #26;
string(vector v) vtos                = #27;
void() coredump                        = #28;        // prints all edicts
void() traceon                        = #29;        // turns statment trace on
void() traceoff                        = #30;
void(entity e) eprint                = #31;        // prints an entire edict
float(float yaw, float dist) walkmove    = #32;    // returns TRUE or FALSE
// #33 was removed
float() droptofloor= #34;    // TRUE if landed on floor
void(float style, string value) lightstyle = #35;
float(float v) rint                    = #36;        // round to nearest int
float(float v) floor                = #37;        // largest integer <= v
float(float v) ceil                    = #38;        // smallest integer >= v
// #39 was removed
float(entity e) checkbottom            = #40;        // true if self is on ground
float(vector v) pointcontents        = #41;        // returns a CONTENT_*
// #42 was removed
float(float f) fabs = #43;

// Frikbot overrides the following:
// vector(entity e, float speed) aim = #44;        // returns the shooting vector

float(string s) cvar = #45;                        // return cvar.value
void(string s) localcmd = #46;                    // put string into local que
entity(entity e) nextent = #47;                    // for looping through all ents
void(vector o, vector d, float color, float count) particle = #48;// start a particle effect
void() ChangeYaw = #49;                        // turn towards self.ideal_yaw
                                            // at self.yaw_speed
// #50 was removed
vector(vector v) vectoangles            = #51;

//
// direct client message generation
//

// Frikbot overrides the following:
// void(float to, float f) WriteByte    = #52;
// void(float to, float f) WriteChar    = #53;
// void(float to, float f) WriteShort   = #54;
// void(float to, float f) WriteLong    = #55;
// void(float to, float f) WriteCoord   = #56;
// void(float to, float f) WriteAngle   = #57;
// void(float to, string s) WriteString = #58;
// void(float to, entity s) WriteEntity = #59;

//
// broadcast client message generation
//

// void(float f) bWriteByte    = #59;
// void(float f) bWriteChar    = #60;
// void(float f) bWriteShort   = #61;
// void(float f) bWriteLong    = #62;
// void(float f) bWriteCoord   = #63;
// void(float f) bWriteAngle   = #64;
// void(string s) bWriteString = #65;
// void(entity e) bWriteEntity = #66;

void(float step) movetogoal                = #67;

string(string s) precache_file        = #68;    // no effect except for -copy
void(entity e) makestatic        = #69;
void(string s) changelevel = #70;

//#71 was removed

void(string var, string val) cvar_set = #72;    // sets cvar.value

// Frikbot overrides the following:
// void(entity client, string s) centerprint = #73;    // sprint, but in middle

void(vector pos, string samp, float vol, float atten) ambientsound = #74;

string(string s) precache_model2    = #75;        // registered version only
string(string s) precache_sound2    = #76;        // registered version only
string(string s) precache_file2        = #77;        // registered version only

// Frikbot overrides the following:
// void(entity e) setspawnparms        = #78;   // set parm1... to the
                                                // values at level start
                                                // for coop respawn

void(vector o, vector d, float preset, float count) particle2 = #79;// start a particle2 effect
float(float base, float exp) pow = #80; // power function
void(float hand, float delay, float duration, float frequency, float amplitude) haptic = #81; // VR haptics function
float(float a, float b) min = #82;
float(float a, float b) max = #83;
void(vector ang) makeforward = #84;        // sets v_forward
float(float input, float inputmin, float inputMax, float outputMin, float outputMax) maprange = #85;        // sets v_forward

float(string s) cvar_hmake = #86;
float(float h) cvar_hget = #87;
void() cvar_hclear = #88;

vector(vector input, vector exemplar) redirectvector = #89;

void(float h, float v) cvar_hset = #90;

float() worldtext_hmake                      = #91;
void(float h, string s) worldtext_hsettext   = #92;
void(float h, vector v) worldtext_hsetpos    = #93;
void(float h, vector v) worldtext_hsetangles = #94;

void(float to, vector v) WriteVec3 = #95;

void(float h, float v) worldtext_hsethalign = #96;

float(string s) strlen = #97;
float(string s, float n) nthchar = #98;
string(string s, float b, float e) substr = #99;

float(float entGravity, float throwSpeed, vector from, vector to) calcthrowangle = #100;
vector(vector vec, vector up, float angle) rotatevec = #101;

float(float angle) sin = #102;
float(float angle) cos = #103;

float(float angle) asin = #104;
float(float angle) acos = #105;

float(float angle) tan = #106;
float(float angle) atan = #107;

float(float v) sqrt = #108;

float(float x, float y) atan2 = #109;

