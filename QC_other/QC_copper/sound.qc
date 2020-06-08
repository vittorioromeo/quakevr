
//============================================================================

/*FGD
@baseclass base(AppearFlags) = Ambient [
	distance(choices) : "Attenuation" : 0 = [
		0 : "Ambient"
		1 : "Monster Idle"
		2 : "Monster Bark"
		3 : "Global"
	]
]
*/
void(float vol) sound_setup =
{
	if (!self.volume)
		self.volume = vol;
	if (!self.distance)
		self.distance = ATTN_STATIC;
	else if (self.distance == 1) self.distance = ATTN_IDLE;
	else if (self.distance == 2) self.distance = ATTN_NORM;
	else if (self.distance == 3) self.distance = ATTN_NONE;
	precache_sound2(self.noise);
}

void(float vol) ambient_setup =
{
	sound_setup(vol);
	ambientsound (self.origin, self.noise, self.volume, self.distance);
}

/*QUAKED ambient_sound (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
specify any ambient sound you want. any at all!

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)

"noise"  wav to play - don't leave off the ".wav"!
ambience/
  buzz1.wav	clicking sparks (ambient_flouro_buzz)
  comp1.wav	bloops and bleeps (ambient_comp_hum)
  drip1.wav	those drips (ambient_drip)
  drone6.wav	deep slipgate throb (ambient_drone)
  fire1.wav	torch crackle (ambient_)
  fl_hum1.wav	flourescent light buzz (ambient_light_buzz)
  hum1.wav	wispy teleporter wind (trigger_teleport sound)
  suck1.wav	deep hollow wind (ambient_suck_wind)
  swamp1.wav	frogs and crickets (ambient_swamp1 or 2)
  water1.wav	eerie swooshing water (waterbrush sound)
  wind2.wav	higher pitched hollow wind (skybrush sound)
  windfly.wav	the wind tunnel air sound
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_sound : "Custom ambient sound"
[
	noise(string) : "path to .wav"
	volume(string) : "Volume" : "1"
]
*/
void() ambient_sound =
{
	if (!SUB_ShouldSpawn()) return;
	if (self.noise == string_null)
		return;
	
	ambient_setup(1);
}

//============================================================================


/*QUAKED ambient_suck_wind (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
good old suck wind

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_suck_wind : "Wind sound" [
	volume(string) : "Volume" : "1"
]
*/
void() ambient_suck_wind =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/suck1.wav";
	ambient_setup(1);
}

/*QUAKED ambient_drone (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
more of a throb, really

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_drone : "Engine/machinery sound" [
	volume(string) : "Volume" : "0.55"
]
*/
void() ambient_drone =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/drone6.wav";
	ambient_setup(0.55);
}

/*QUAKED ambient_flouro_buzz (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
zaps and sparks

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_flouro_buzz : "Fluorescent buzzing sound" [
	volume(string) : "Volume" : "1"
]
*/
void() ambient_flouro_buzz =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/buzz1.wav";
	ambient_setup(1);
}
/*QUAKED ambient_drip (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
drip drip

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_drip : "Dripping sound" [
	volume(string) : "Volume" : "0.75"
]
*/
void() ambient_drip =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/drip1.wav";
	ambient_setup(0.75);
}
/*QUAKED ambient_comp_hum (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
bleeps and bloops

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_comp_hum : "Computer background sounds" [
	volume(string) : "Volume" : "1"
]
*/
void() ambient_comp_hum =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/comp1.wav";
	ambient_setup(1);
}
/*QUAKED ambient_thunder (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
what

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_thunder : "Thunder sound" [
	volume(string) : "Volume" : "0.5"
]
*/
void() ambient_thunder =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/thunder1.wav";
	ambient_setup(0.5);
}
/*QUAKED ambient_light_buzz (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
gentle lamp hum

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_light_buzz : "Buzzing sound from light" [
	volume(string) : "Volume" : "0.85"
]
*/
void() ambient_light_buzz =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/fl_hum1.wav";
	ambient_setup(0.85);
}
/*QUAKED ambient_swamp1 (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
swamp

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_swamp1 : "Frogs croaking" [
	volume(string) : "Volume" : "0.75"
]
*/
void() ambient_swamp1 =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/swamp1.wav";
	ambient_setup(0.75);
}
/*QUAKED ambient_swamp2 (0.3 0.1 0.6) (-10 -10 -8) (10 10 8)
same as swamp1

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = ambient_swamp2 : "Frogs croaking B" [
	volume(string) : "Volume" : "0.5"
]
*/
void() ambient_swamp2 =
{
	if (!SUB_ShouldSpawn()) return;
	self.noise = "ambience/swamp2.wav";
	ambient_setup(0.5);
}

//============================================================================

void() target_sound_use =
{
	sound(self, CHAN_AUTO, self.noise, self.volume, self.distance);
}

/*QUAKED target_sound (0.3 0.1 0.6) (-8 -8 -8) (8 8 8)
play a sound when triggered.

"distance" attenuation (integer) - defaults to 0, which is the same as other ambient sounds.
  1 = 'idle' - monster idle sound range
  2 = 'normal' - monster bark sound range
  3 = 'none' - loops everywhere forever (don't use this)
"volume" volume, defaults to 1.0 (full volume)
"noise"  wav to play - don't leave off the ".wav"!
*/
/*FGD
@PointClass base(Ambient) color(150 0 150) = target_sound : "Custom triggered sound"
[
	noise(string) : "path to .wav"
	volume(string) : "Volume" : "1"
]
*/
void() target_sound =
{
	sound_setup(1);
	self.use = target_sound_use;
}

//============================================================================

void() noise_think =
{
	self.nextthink = time + 0.5;
	sound (self, 1, "enforcer/enfire.wav", 1, ATTN_NORM);
	sound (self, 2, "enforcer/enfstop.wav", 1, ATTN_NORM);
	sound (self, 3, "enforcer/sight1.wav", 1, ATTN_NORM);
	sound (self, 4, "enforcer/sight2.wav", 1, ATTN_NORM);
	sound (self, 5, "enforcer/sight3.wav", 1, ATTN_NORM);
	sound (self, 6, "enforcer/sight4.wav", 1, ATTN_NORM);
	sound (self, 7, "enforcer/pain1.wav", 1, ATTN_NORM);
}

void() misc_noisemaker =
{
	precache_sound2 ("enforcer/enfire.wav");
	precache_sound2 ("enforcer/enfstop.wav");
	precache_sound2 ("enforcer/sight1.wav");
	precache_sound2 ("enforcer/sight2.wav");
	precache_sound2 ("enforcer/sight3.wav");
	precache_sound2 ("enforcer/sight4.wav");
	precache_sound2 ("enforcer/pain1.wav");
	precache_sound2 ("enforcer/pain2.wav");
	precache_sound2 ("enforcer/death1.wav");
	precache_sound2 ("enforcer/idle1.wav");

	self.nextthink = time + 0.1 + random();
	self.think = noise_think;
}
