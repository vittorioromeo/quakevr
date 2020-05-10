//
// ================================================================
//
// Setup light animation tables. 'a' is total darkness, 'z' is maxbright.
	// you know what's cool?  arrays.
// 0 normal
string light0 = "m";
// 1 FLICKER (first variety)
string light1 = "mmnmmommommnonmmonqnmmo";
// 2 SLOW STRONG PULSE
string light2 = "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba";
// 3 CANDLE (first variety)
string light3 = "mmmmmaaaaammmmmaaaaaabcdefgabcdefg";
// 4 FAST STROBE
string light4 = "mamamamamama";
// 5 GENTLE PULSE 1
string light5 = "jklmnopqrstuvwxyzyxwvutsrqponmlkj";
// 6 FLICKER (second variety)
string light6 = "nmonqnmomnmomomno";
// 7 CANDLE (second variety)
string light7 = "mmmaaaabcdefgmmmmaaaammmaamm";
// 8 CANDLE (third variety)
string light8 = "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa";
// 9 SLOW STROBE (fourth variety)
string light9 = "aaaaaaaazzzzzzzz";
// 10 FLUORESCENT FLICKER
string light10 = "mmamammmmammamamaaamammma";
// 11 SLOW PULSE TO BLACK
string light11 = "abcdefghijklmnopqrrqponmlkjihgfedcba";
// styles 32-62 are assigned by the light program for switchable lights

string light_off = "a";


/*FGD
@baseclass base() color(255 255 40) size(-8 -8 -8, 8 8 8) = Light [
	light(integer) : "Brightness" : 300
	_color(string) : "Color" : "1 1 1"
	wait(integer) : "Fade distance multiplier" : 1
	mangle(string) : "Spotlight aim (yaw pitch roll, neg. pitch is down)"
	angle(integer) : "Spotlight cone angle"
	_softangle(integer) : "Spotlight inner cone angle"
	_dirt(integer) : "Override dirt"
	message(string) : "override the style string directly for the assigned style (clashes will overwrite each other)"
	delay(choices) : "Attenuation" =
	[
		0 : "Linear falloff (Default)"
		1 : "Inverse distance falloff"
		2 : "Inverse distance squared"
		3 : "No falloff"
		4 : "Local minlight"
		5 : "Inverse distance squared B"
	]
	style(Choices) : "Appearance" : 0 =
	[
		0 : "Normal"
		10: "Fluorescent flicker (mmnmmommommnonmmonqnmmo)"
		2 : "Slow, strong pulse (abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba)"
		11: "Slow pulse, black (abcdefghijklmnopqrrqponmlkjihgfedcba)"
		5 : "Gentle pulse, noblac (jklmnopqrstuvwxyzyxwvutsrqponmlkj)"
		1 : "Flicker A (mmnmmommommnonmmonqnmmo)"
		6 : "Flicker B (nmonqnmomnmomomno)"
		3 : "Candle A (mmmmmaaaaammmmmaaaaaabcdefgabcdefg)"
		7 : "Candle B (mmmaaaabcdefgmmmmaaaammmaamm)"
		8 : "Candle C (mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa)"
		4 : "Fast strobe (mamamamamama)"
		9 : "Slow strobe (aaaaaaaazzzzzzzz)"
	]
]
@baseclass base(Light, Targetname) = LightTriggerable [
	oldstyle(integer) : "Switchable light's 'on' style"
	spawnflags(flags) = [
		1 : "Start off" : 0
	]
]
*/

//
// ================================================================
//

string( float st ) GetLightStyle =
{
	if (st == 1) return light1;
	if (st == 2) return light2;
	if (st == 3) return light3;
	if (st == 4) return light4;
	if (st == 5) return light5;
	if (st == 6) return light6;
	if (st == 7) return light7;
	if (st == 8) return light8;
	if (st == 9) return light9;
	if (st == 10) return light10;
	if (st == 11) return light11;
/*	if (st == 12) return light12;
	if (st == 13) return light13;
	if (st == 14) return light14;
	if (st == 15) return light15;
	if (st == 16) return light16;
	if (st == 17) return light17;
	if (st == 18) return light18;*/
	
	if (icantsee)
		return "z";
	return light0;
}

void() InitLightStyles =
{
	lightstyle(0, light0);
	lightstyle(1, light1);
	lightstyle(2, light2);
	lightstyle(3, light3);
	lightstyle(4, light4);
	lightstyle(5, light5);
	lightstyle(6, light6);
	lightstyle(7, light7);
	lightstyle(8, light8);
	lightstyle(9, light9);
	lightstyle(10, light10);
	lightstyle(11, light11);
/*	lightstyle(12, light12);
	lightstyle(13, light13);
	lightstyle(14, light14);
	lightstyle(15, light15);
	lightstyle(16, light16);
	lightstyle(17, light17);
	lightstyle(18, light18);*/
	
	// 63 testing
	//lightstyle(63, "a");
}

void() light_use =
{
	//dprint("light_use\n");
	if (self.spawnflags & START_OFF)
	{
		if (self.message != string_null)
			lightstyle(self.style, self.message);
		else
			lightstyle(self.style, GetLightStyle(self.oldstyle));
		self.spawnflags = self.spawnflags - START_OFF;
		//dprint("turning light on\n");
	}
	else
	{
		lightstyle(self.style, light_off);
		self.spawnflags = self.spawnflags | START_OFF;
		//dprint("turning light off\n");
	}
}

// Lights with "count" will gradually change brightness each time they're triggered, reaching normal brightness or complete darkness after 'count' triggers.
void() light_inc_use =
{
	local float bright;
	
	if (self.spawnflags & START_OFF)
	{
		if (self.cnt == self.count) return;
		self.cnt = self.cnt + 1;
	}
	else
	{
		if (self.cnt == 0) return;
		self.cnt = self.cnt - 1;
	}
	
	// you know what's cool?  arrays.
	bright = floor( (self.cnt / self.count) * 13);
	if (bright >= 13)		lightstyle(self.style, "m");
	else if (bright == 12) lightstyle(self.style, "l");
	else if (bright == 11) lightstyle(self.style, "k");
	else if (bright == 10) lightstyle(self.style, "j");
	else if (bright == 9 ) lightstyle(self.style, "i");
	else if (bright == 8 ) lightstyle(self.style, "h");
	else if (bright == 7 ) lightstyle(self.style, "g");
	else if (bright == 6 ) lightstyle(self.style, "f");
	else if (bright == 5 ) lightstyle(self.style, "e");
	else if (bright == 4 ) lightstyle(self.style, "d");
	else if (bright == 3 ) lightstyle(self.style, "c");
	else if (bright == 2 ) lightstyle(self.style, "b");
	else if (bright <= 1 ) lightstyle(self.style, "a");
}

/*QUAKED light (0 .75 0) (-8 -8 -8) (8 8 8) start_off
Non-displayed light. If targeted, it will toggle between on or off. 

"start_off"	starts off until triggered

Keys:
"light"		sets brightness (default: 300)
"targetname"	entity name
"mangle"	spotlight direction - the first # is yaw, 0 to 360 around Z. second # is pitch, 90 up to -90 down.
"angle"		spotlight cone size
"_softangle" spotlight inner cone size
"wait"		scale the falloff distance.  N>1 will shorten range, n<1 will extend it
"delay"		attenuation formula
	0 = Linear, original id (default)
	1 = 1/x attenuation formula
	2 = 1/(x^2) attenuation formula
	3 = No attenuation (light stays same brightness at any distance)
"_color" "# # #" 0-255
"style"
    0 = normal
    1 = flicker: "mmnmmommommnonmmonqnmmo"
    2 = slow strong pulse: "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba"
    3 = candle: "mmmmmaaaaammmmmaaaaaabcdefgabcdefg"
    4 = fast strobe: "mamamamamama"
    5 = slow pulse, no black: "jklmnopqrstuvwxyzyxwvutsrqponmlkj"
    6 = flicker: "nmonqnmomnmomomno"
    7 = candle: "mmmaaaabcdefgmmmmaaaammmaamm"
    8 = candle2: "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"
    9 = slow strobe: "aaaaaaaazzzzzzzz"
    10 = flourescent flicker: mmamammmmammamamaaamammma"
    11 = slow pulse, fading to black: "abcdefghijklmnopqrrqponmlkjihgfedcba"

    styles 32-62 are assigned by the light program for switchable lights 
    (default: 0)
"oldstyle"	make a targeted light switchable between that style and off.
"message"	override the style string directly for the assigned style (clashes will overwrite each other)
*/
/*FGD
@PointClass base(LightTriggerable) = light : "Invisible light source" []
*/
void() light =
{
	if (self.message != string_null)
	{
		if (self.oldstyle > 11)
			lightstyle(self.oldstyle, self.message);
		else if (self.style > 11)
			lightstyle(self.style, self.message);
		else
			dprint5("WARNING: light at ", vtos(self.origin), " with message ", self.message, " but no style # set\n");
	}
	
	if (self.targetname == string_null)
	{	// inert light
		remove(self);
		return;
	}
	
	if (self.style >= 32)
	{
		if (self.count > 0)
		{
			self.use = light_inc_use;
			if (self.spawnflags & START_OFF)
			{
				lightstyle(self.style, light_off);
				self.cnt = 0;
			}
			else
			{
				lightstyle(self.style, GetLightStyle(self.oldstyle));
				self.cnt = self.count;
			}
			return;
		}
		
		self.use = light_use;
		if (self.spawnflags & START_OFF)
			lightstyle(self.style, light_off);
		else
			lightstyle(self.style, GetLightStyle(self.oldstyle));
	}
}

//============================================================================

/*QUAKED light_fluoro (1 1 0) (-8 -8 -8) (8 8 8) START_OFF
Non-displayed light. Makes steady fluorescent humming sound.

Flags:
"start_off"	starts off until triggered

Keys:
"light"		sets brightness (default: 300)
"targetname"	entity name
"mangle"	spotlight direction - the first # is yaw, 0 to 360 around Z. second # is pitch, 90 up to -90 down.
"angle"		spotlight cone size
"_softangle" spotlight inner cone size
"wait"		scale the falloff distance.  N>1 will shorten range, n<1 will extend it
"delay"		attenuation formula
	0 = Linear, original id (default)
	1 = 1/x attenuation formula
	2 = 1/(x^2) attenuation formula
	3 = No attenuation (light stays same brightness at any distance)
"_color" "# # #" 0-255
"style"
    0 = normal
    1 = flicker: "mmnmmommommnonmmonqnmmo"
    2 = slow strong pulse: "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba"
    3 = candle: "mmmmmaaaaammmmmaaaaaabcdefgabcdefg"
    4 = fast strobe: "mamamamamama"
    5 = slow pulse, no black: "jklmnopqrstuvwxyzyxwvutsrqponmlkj"
    6 = flicker: "nmonqnmomnmomomno"
    7 = candle: "mmmaaaabcdefgmmmmaaaammmaamm"
    8 = candle2: "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"
    9 = slow strobe: "aaaaaaaazzzzzzzz"
    10 = flourescent flicker: mmamammmmammamamaaamammma"
    11 = slow pulse, fading to black: "abcdefghijklmnopqrrqponmlkjihgfedcba"

    styles 32-62 are assigned by the light program for switchable lights 
    (default: 0)
"oldstyle"	make a targeted light switchable between that style and off.
"message"	override the style string directly for the assigned style (clashes will overwrite each other)
*/
/*FGD
@PointClass base(LightTriggerable) = light_fluoro : "Humming fluorescent light source" []
*/
void() light_fluoro =
{
	if (self.style >= 32)
	{
		self.use = light_use;
		if (self.spawnflags & START_OFF)
			lightstyle(self.style, "a");
		else
			lightstyle(self.style, "m");
	}
	
	precache_sound ("ambience/fl_hum1.wav");
	ambientsound (self.origin, "ambience/fl_hum1.wav", 0.5, ATTN_STATIC);
}

/*QUAKED light_fluorospark (1 1 0) (-8 -8 -8) (8 8 8)
Non-displayed light. Default style is 10. Makes sparking, broken fluorescent sound.

Keys:
"light"		sets brightness (default: 300)
"mangle"	spotlight direction - the first # is yaw, 0 to 360 around Z. second # is pitch, 90 up to -90 down.
"angle"		spotlight cone size
"_softangle" spotlight inner cone size
"wait"		scale the falloff distance.  N>1 will shorten range, n<1 will extend it
"delay"		attenuation formula
	0 = Linear, original id (default)
	1 = 1/x attenuation formula
	2 = 1/(x^2) attenuation formula
	3 = No attenuation (light stays same brightness at any distance)
"_color" "# # #" 0-255
"style"
    0 = normal
    1 = flicker: "mmnmmommommnonmmonqnmmo"
    2 = slow strong pulse: "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba"
    3 = candle: "mmmmmaaaaammmmmaaaaaabcdefgabcdefg"
    4 = fast strobe: "mamamamamama"
    5 = slow pulse, no black: "jklmnopqrstuvwxyzyxwvutsrqponmlkj"
    6 = flicker: "nmonqnmomnmomomno"
    7 = candle: "mmmaaaabcdefgmmmmaaaammmaamm"
    8 = candle2: "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"
    9 = slow strobe: "aaaaaaaazzzzzzzz"
    10 = flourescent flicker: mmamammmmammamamaaamammma"
    11 = slow pulse, fading to black: "abcdefghijklmnopqrrqponmlkjihgfedcba"

    styles 32-62 are assigned by the light program for switchable lights 
    (default: 0)
"message"	override the style string directly for the assigned style (clashes will overwrite each other)
*/
/*FGD
@PointClass base(Light) = light_fluorospark : "Sparking flickering light source" []
*/
void() light_fluorospark =
{
	if (!self.style)
		self.style = 10;

	precache_sound ("ambience/buzz1.wav");
	ambientsound (self.origin, "ambience/buzz1.wav", 0.5, ATTN_STATIC);
}

/*QUAKED light_globe (1 1 0) (-8 -8 -8) (8 8 8)
Sphere globe light.  Can't be triggered.

Keys:
"light"		sets brightness (default: 300)
"mangle"	spotlight direction - the first # is yaw, 0 to 360 around Z. second # is pitch, 90 up to -90 down.
"angle"		spotlight cone size
"_softangle" spotlight inner cone size
"wait"		scale the falloff distance.  N>1 will shorten range, n<1 will extend it
"delay"		attenuation formula
	0 = Linear, original id (default)
	1 = 1/x attenuation formula
	2 = 1/(x^2) attenuation formula
	3 = No attenuation (light stays same brightness at any distance)
"_color" "# # #" 0-255
"style"
    0 = normal
    1 = flicker: "mmnmmommommnonmmonqnmmo"
    2 = slow strong pulse: "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba"
    3 = candle: "mmmmmaaaaammmmmaaaaaabcdefgabcdefg"
    4 = fast strobe: "mamamamamama"
    5 = slow pulse, no black: "jklmnopqrstuvwxyzyxwvutsrqponmlkj"
    6 = flicker: "nmonqnmomnmomomno"
    7 = candle: "mmmaaaabcdefgmmmmaaaammmaamm"
    8 = candle2: "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"
    9 = slow strobe: "aaaaaaaazzzzzzzz"
    10 = flourescent flicker: mmamammmmammamamaaamammma"
    11 = slow pulse, fading to black: "abcdefghijklmnopqrrqponmlkjihgfedcba"

    styles 32-62 are assigned by the light program for switchable lights 
    (default: 0)
"message"	override the style string directly for the assigned style (clashes will overwrite each other)
*/
/*FGD
@PointClass base(Light) = light_globe : "Globe light" []
*/
void() light_globe =
{
	precache_model ("progs/s_light.spr");
	setmodel (self, "progs/s_light.spr");
	makestatic (self);
}

void() FireAmbient =
{
	if (self.spawnflags & 2) return;	// silent spawnflag
	precache_sound ("ambience/fire1.wav");
// attenuate fast
	ambientsound (self.origin, "ambience/fire1.wav", 0.5, ATTN_STATIC);
}

/*QUAKED light_torch_small_walltorch (1 1 0) (-4 -4 -12) (4 4 20)
Short wall torch. Makes crackly torch sound.

Flags:
"start_off"	starts off until triggered

Keys:
"light"		sets brightness (default: 300)
"mangle"	spotlight direction - the first # is yaw, 0 to 360 around Z. second # is pitch, 90 up to -90 down.
"angle"		spotlight cone size
"_softangle" spotlight inner cone size
"wait"		scale the falloff distance.  N>1 will shorten range, n<1 will extend it
"delay"		attenuation formula
	0 = Linear, original id (default)
	1 = 1/x attenuation formula
	2 = 1/(x^2) attenuation formula
	3 = No attenuation (light stays same brightness at any distance)
"_color" "# # #" 0-255
"style"
    0 = normal
    1 = flicker: "mmnmmommommnonmmonqnmmo"
    2 = slow strong pulse: "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba"
    3 = candle: "mmmmmaaaaammmmmaaaaaabcdefgabcdefg"
    4 = fast strobe: "mamamamamama"
    5 = slow pulse, no black: "jklmnopqrstuvwxyzyxwvutsrqponmlkj"
    6 = flicker: "nmonqnmomnmomomno"
    7 = candle: "mmmaaaabcdefgmmmmaaaammmaamm"
    8 = candle2: "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"
    9 = slow strobe: "aaaaaaaazzzzzzzz"
    10 = flourescent flicker: mmamammmmammamamaaamammma"
    11 = slow pulse, fading to black: "abcdefghijklmnopqrrqponmlkjihgfedcba"

    styles 32-62 are assigned by the light program for switchable lights 
    (default: 0)
"message"	override the style string directly for the assigned style (clashes will overwrite each other)
*/
/*FGD
@PointClass base(Light) size(-4 -4 -12, 4 4 20) model({ "path": ":progs/flame.mdl" }) = 
	light_torch_small_walltorch : "Small walltorch" []
*/
void() light_torch_small_walltorch =
{
	precache_model ("progs/flame.mdl");
	setmodel (self, "progs/flame.mdl");
	FireAmbient ();
	makestatic (self);
}

/*QUAKED light_flame_large_yellow (1 1 0) (-8 -8 -12) (8 8 20)
Large yellow flame ball.  Makes crackly torch sound.

Flags:
"start_off"	starts off until triggered

Keys:
"light"		sets brightness (default: 300)
"mangle"	spotlight direction - the first # is yaw, 0 to 360 around Z. second # is pitch, 90 up to -90 down.
"angle"		spotlight cone size
"_softangle" spotlight inner cone size
"wait"		scale the falloff distance.  N>1 will shorten range, n<1 will extend it
"delay"		attenuation formula
	0 = Linear, original id (default)
	1 = 1/x attenuation formula
	2 = 1/(x^2) attenuation formula
	3 = No attenuation (light stays same brightness at any distance)
"_color" "# # #" 0-255
"style"
    0 = normal
    1 = flicker: "mmnmmommommnonmmonqnmmo"
    2 = slow strong pulse: "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba"
    3 = candle: "mmmmmaaaaammmmmaaaaaabcdefgabcdefg"
    4 = fast strobe: "mamamamamama"
    5 = slow pulse, no black: "jklmnopqrstuvwxyzyxwvutsrqponmlkj"
    6 = flicker: "nmonqnmomnmomomno"
    7 = candle: "mmmaaaabcdefgmmmmaaaammmaamm"
    8 = candle2: "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"
    9 = slow strobe: "aaaaaaaazzzzzzzz"
    10 = flourescent flicker: mmamammmmammamamaaamammma"
    11 = slow pulse, fading to black: "abcdefghijklmnopqrrqponmlkjihgfedcba"

    styles 32-62 are assigned by the light program for switchable lights 
    (default: 0)
"message"	override the style string directly for the assigned style (clashes will overwrite each other)

*/
/*FGD
@PointClass base(Light) size(-8 -8 -12, 8 8 20) model({ "path": ":progs/flame2.mdl" }) = 
	light_flame_large_yellow : "Large yellow flame" []
*/
void() light_flame_large_yellow =
{
	precache_model ("progs/flame2.mdl");
	setmodel (self, "progs/flame2.mdl");
	self.frame = 1;
	FireAmbient ();
	makestatic (self);
}

/*QUAKED light_flame_small_yellow (1 1 0) (-4 -4 -12) (4 4 20)
Small yellow flame ball.  Makes crackly torch sound.

Keys:
"light"		sets brightness (default: 300)
"mangle"	spotlight direction - the first # is yaw, 0 to 360 around Z. second # is pitch, 90 up to -90 down.
"angle"		spotlight cone size
"_softangle" spotlight inner cone size
"wait"		scale the falloff distance.  N>1 will shorten range, n<1 will extend it
"delay"		attenuation formula
	0 = Linear, original id (default)
	1 = 1/x attenuation formula
	2 = 1/(x^2) attenuation formula
	3 = No attenuation (light stays same brightness at any distance)
"_color" "# # #" 0-255
"style"
    0 = normal
    1 = flicker: "mmnmmommommnonmmonqnmmo"
    2 = slow strong pulse: "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba"
    3 = candle: "mmmmmaaaaammmmmaaaaaabcdefgabcdefg"
    4 = fast strobe: "mamamamamama"
    5 = slow pulse, no black: "jklmnopqrstuvwxyzyxwvutsrqponmlkj"
    6 = flicker: "nmonqnmomnmomomno"
    7 = candle: "mmmaaaabcdefgmmmmaaaammmaamm"
    8 = candle2: "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa"
    9 = slow strobe: "aaaaaaaazzzzzzzz"
    10 = flourescent flicker: mmamammmmammamamaaamammma"
    11 = slow pulse, fading to black: "abcdefghijklmnopqrrqponmlkjihgfedcba"

    styles 32-62 are assigned by the light program for switchable lights 
    (default: 0)
"message"	override the style string directly for the assigned style (clashes will overwrite each other)

*/
/*FGD
@PointClass base(Light) size(-4 -4 -12, 4 4 20) model({ "path": ":progs/flame2.mdl" }) = 
	light_flame_small_yellow : "Small yellow flame"  []
*/
void() light_flame_small_yellow =
{
	precache_model ("progs/flame2.mdl");
	setmodel (self, "progs/flame2.mdl");
	FireAmbient ();
	makestatic (self);
}

/*QUAKED light_flame_small_white (1 1 0) (-4 -4 -12) (4 4 20)
Same as light_flame_small_yellow. Use that one.
*/
/*FGD
@PointClass base(Light) size(-4 -4 -12, 4 4 20) model({ "path": ":progs/flame2.mdl" }) = 
	light_flame_small_white : "Small white flame (same as light_flame_small_yellow)" []
*/
void() light_flame_small_white =
{
	light_flame_small_yellow();
}
