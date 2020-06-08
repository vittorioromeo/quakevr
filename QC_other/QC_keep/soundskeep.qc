float SOUND_ALTVERSION = 4;
float SOUND_EXTENSION1 = 16;

void() SuperDamageSound = {
    if (self.super_damage_finished > time)
    {
        if (self.super_sound < time)
        {
            self.super_sound = time + 1;
            sound (self, CHAN_BODY, "items/damage3.wav", 1, ATTN_NORM);
        }
    }
    return;
};

void() ambient_drain = {
    self.noise = "ambience/drain.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_eerie = {
    self.noise = "ambience/rych1.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_factory = {
    self.noise = "ambience/factory.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_fan_blowing = {
    self.noise = "ambience/fanblow.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_flies = {
    self.noise = "ambience/bugs.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_flys = {
    self.noise = "misc/flys.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_grinder = {
    self.noise = "ambience/grinder.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_highwind = {
    self.noise = "ambience/highwind.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    self.count = 99;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_humming = {
    if (self.spawnflags & SOUND_ALTVERSION) {
        self.noise = "ambience/maydn.wav"; // Support for Nehahra ambient_humming
        if (self.volume <= 0) self.volume = 0.5;
    } else { 
        self.noise = "ambience/humming.wav";
        if (self.volume <= 0) self.volume = 0.5;
    }
    precache_sound (self.noise);
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_hum1 = {
    self.noise = "ambience/hum1.wav";
    if (self.volume <= 0) self.volume = 0.5;
    precache_sound (self.noise);
    ambient_sound_setup();
};
void() ambient_lapping = {
    self.noise = "ambience/wat_amb.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    self.count = 99;
    ambient_sound_setup();
};
void() ambient_lapping2 = {
    self.noise = "ambience/wat_cave.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    self.count = 99;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_machines = {
    self.noise = "ambience/begths.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_nehum = {
    self.noise = "ambience/filt.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_rain = {
    if (self.count == 0)
        self.noise = "ambience/rain.wav"; // Rain Runoff

    if (self.count == 1)
        self.noise = "ambience/rain3.wav"; // Steady Rain

    if (self.count == 2)
        self.noise = "ambience/rain2.wav"; // Heavy Rain

    if (self.count > 2 || self.count < 0)
        self.noise = "ambience/rain3.wav";

    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_riftpower = {
    self.noise = "ambience/riftpowr.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_running_lava = {
    self.noise = "ambience/runlava.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_running_water = {
    self.noise = "ambience/runwater.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_rushing = {
    self.noise = "ambience/rushing.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_sizzle = {
    self.noise = "ambience/amsizl.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
void() ambient_stargate_hum = {
    self.noise = "ambience/sgatehum.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    self.count = 99;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_swamp3 = {
    self.noise = "ambience/swamp3.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_teeth = {
    self.noise = "ambience/teeth.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_teleport = {
    self.noise = "ambience/hum1.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_thrum = {
    self.noise = "ambience/thrum.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() thunder_go_boom = {
    if (random() < 0.5) {
        sound(self, CHAN_AUTO, self.noise, self.volume, ATTN_NONE);
        SUB_UseTargets();
    } else {
        sound(self, CHAN_AUTO, self.noise3, self.volume, ATTN_NONE);
        SUB_UseTargets();
    }
     self.think = thunder_go_boom;
    if (self.spawnflags & SOUND_EXTENSION1) {
         self.nextthink = time + self.wait * random();
    } else {
        self.nextthink = time + self.wait;
    }
}
//void() ambient_water_drip = {
//    self.noise = "ambience/wat_drip.wav";  // !! wat_drip.wav DOES NOT EXIST!!
//    precache_sound (self.noise);
//    if (self.volume <= 0) self.volume = 1;
//    self.count = 99;
//    ambient_sound_setup();
//};
//----------------------------------------------------------------------
void() ambient_waterfall = {
    self.noise = "ambience/waterfal.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 0.5;
    ambient_sound_setup();
};
void() ambient_water_fall = {
    self.noise = "ambience/wat_fall.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    self.count = 99;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_whnoise = {
    self.noise = "ambience/whn.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};
//----------------------------------------------------------------------
void() ambient_zzxzz = {
    self.noise = "ambience/flash.wav";
    precache_sound (self.noise);
    if (self.volume <= 0) self.volume = 1;
    ambient_sound_setup();
};