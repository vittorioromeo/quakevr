void () effect_teleport_use = {
   WriteByte (MSG_BROADCAST,SVC_TEMPENTITY);
   WriteByte (MSG_BROADCAST,TE_TELEPORT);
   WriteCoord (MSG_BROADCAST,self.origin_x);
   WriteCoord (MSG_BROADCAST,self.origin_y);
   WriteCoord (MSG_BROADCAST,self.origin_z);
   sound (self,CHAN_VOICE,"misc/r_tele1.wav",TRUE,ATTN_NORM);
};

void () effect_teleport = {
   precache_sound ("misc/r_tele1.wav");
   self.use = effect_teleport_use;
};

void() effect_pulse_launch = {
    local entity missile;
    missile = spawn();
    missile.owner = self;
    missile.movetype = MOVETYPE_FLYMISSILE;
    missile.solid = SOLID_BBOX;
    missile.classname = "pulse";
    missile.velocity = self.movedir * self.speed;
    missile.touch = SUB_Remove;
    missile.nextthink = time + 5;
    missile.think = SUB_Remove;
    missile.effects = EF_DIMLIGHT;
    setmodel(missile, "progs/s_null.spr");
    setsize(missile, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(missile, self.origin);
};

void() effect_pulse_loop = {
    if (self.wait > 0) self.nextthink = time + self.wait;
    self.think = effect_pulse_loop;
    effect_pulse_launch();
    if (self.wait == CONTENT_EMPTY) {
        SUB_Remove();
        return;
    }
};

void() effect_pulse_start = {
    self.nextthink = time + self.delay;
    self.think = effect_pulse_loop;
};

void() effect_pulse = {
    precache_model("progs/s_null.spr");
    SetMovedir();
    if (!(self.speed)) self.speed = 400;
    if (!(self.delay)) self.delay = 0.1;
    if (!(self.wait)) self.wait = 1;
    self.nextthink = time + self.delay;
    if (self.targetname) {
        self.think = SUB_Null;
        self.use = effect_pulse_start;
        return;
    }
    self.think = effect_pulse_loop;
};

void() info_effect_pulse = {
    effect_pulse();
};

void() shaker_think = {
    self.nextthink = time + self.lip;
    self.think = shaker_think;
    if (self.ltime > time) {
        self.spawnmaster.punchangle_x = (time - self.ltime) * random() * self.multiplier;
        self.spawnmaster.punchangle_y = (time - self.ltime) * random() * self.multiplier;
        self.spawnmaster.punchangle_z = (time - self.ltime) * random() * self.multiplier;
    }
    if (self.ltime < time) SUB_Remove();
};

void() shake_use = {
    local entity qent;
    qent = spawn();
    qent.owner = self;
    qent.movetype = MOVETYPE_NONE;
    qent.solid = SOLID_NOT;
    qent.spawnmaster = activator;
    qent.nextthink = time + self.lip;
    qent.think = shaker_think;
    qent.multiplier = self.multiplier;
    qent.lip = self.lip;
    qent.duration = time + self.duration;
    qent.ltime = time + self.ltime;
    setsize(qent, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(qent, self.origin);
};

void() info_screenshake = {
    if (!(self.ltime)) self.ltime = 1;
    if (!(self.lip)) self.lip = 0.010000;
    if (!(self.multiplier)) self.multiplier = 0.000500;
    self.use = shake_use;
};

void(entity e, float magnitude, float interval, float mult) shake_player = {
    local entity qent;
    qent = spawn();
    qent.owner = self;
    qent.movetype = MOVETYPE_NONE;
    qent.solid = SOLID_NOT;
    qent.spawnmaster = e;
    qent.nextthink = time + 0.1;
    qent.think = shaker_think;
    qent.ltime = time + magnitude;
    qent.lip = interval;
    qent.multiplier = mult;
    setsize(qent, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(qent, e.origin);
};

void() bombthink = {
    T_RadiusDamage(self, self.owner, 512, world,DAMAGEALL);
    self.frame = self.frame + 1;
    if (self.frame == 24) spawn_big_explosion(self.origin, 0);
    if (self.frame > 25) {
        SUB_Remove();
        return;
    }
    self.think = bombthink;
    self.nextthink = time + 0.1;
};

void() ringthink = {
    self.nextthink = time + 0.1;
    self.think = ringthink;
    self.frame = self.frame + 1;
    if (self.frame > 20) SUB_Remove();
};

void() ringprethink = {
    self.nextthink = time + 0.1;
    self.think = ringthink;
    self.avelocity = '0 0 200';
    setmodel(self, "progs/shockwave.mdl");
};

void() W_Bomb = {
    local entity bomb;
    local entity t;
    local float dist;
    local entity ring;
    if (coop == 1 && self.gotbomb != -1) self.gotbomb = 1;
    bomb = spawn();
    bomb.solid = SOLID_NOT;
    setsize(bomb, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(bomb, self.origin);
    setmodel(bomb, "progs/boom.mdl");
    bomb.think = bombthink;
    bomb.nextthink = time + 0.1;
    bomb.owner = self;
    bomb.effects = EF_BRIGHTLIGHT + 32;
    bomb.glow_color = 239;
    bomb.glow_size = 1000;
    if (cvar("pr_checkextension")) {
        if (checkextension("DP_TE_CUSTOMFLASH")) {
            te_customflash(self.origin, 1000, 5, '1 0.6 0.6');
            bomb.effects = 32;
        }
    }
    spawn_big_explosion(self.origin,1);
    t = findradius(self.origin, 10000);
    while (t) {
        if (t.classname == "player") {
            dist = vlen(self.origin - t.origin);
            if (dist < 700) {
                shake_player(t, 2.5, 0.010000, 8);
            } else {
                if (dist < 1200) {
                    shake_player(t, 1, 0.010000, 7);
                } else {
                    shake_player(t, 0.5, 0.010000, 10);
                }
            }
        }
        t = t.chain;
    }
    ring = spawn();
    setorigin(ring, self.origin);
    ring.think = ringprethink;
    ring.nextthink = time + 0.3;
    ring.movetype = MOVETYPE_FLY;
    ring.effects = 32;
    ring = spawn();
    setorigin(ring, self.origin);
    ring.think = ringprethink;
    ring.nextthink = time + 0.3;
    ring.movetype = MOVETYPE_FLY;
    ring.effects = 32;
    ring.angles = '0 90 0';
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);
    if (self.invincible_finished) {
        self.items = self.items - IT_INVULNERABILITY;
        self.invincible_time = 0;
        self.invincible_finished = 0;
    }
    if (self.cross_finished) {
        //self.items2 = self.items2 - IT2_CROSS;
        self.cross_finished = 0;
        self.cross_time = 0;
        self.cross_start = 2;
        stuffcmd(self, "v_cshift 0 0 0 0\n");
    }
    T_RadiusDamage(bomb, self, 512, self,DAMAGEALL);
    T_Damage(self, self, self, -666,DAMARMOR);
    self.velocity_z = 700;
    sound(self, CHAN_AUTO, "weapons/bombfar.wav", 1, ATTN_NONE);
    sound(self, CHAN_AUTO, "weapons/bomb.wav", 1, 0.7);
};

void() info_bomb = {
    precache_sound("weapons/bomb.wav");
    precache_sound("weapons/bombfar.wav");
    precache_model("progs/v_bomb.mdl");
    precache_model("progs/smlexp.spr");
    precache_model("progs/bigexp.spr");
    precache_model("progs/boom.mdl");
    precache_model("progs/shockwave.mdl");
    self.use = W_Bomb;
};

void() splash_think = {
    local vector vec;
    local float variance;
    variance = vlen(self.movedir) / 2;
    vec_x = self.movedir_x - variance + random() * variance * 2;
    vec_y = self.movedir_y - variance + random() * variance * 2;
    vec_z = self.movedir_z - variance + random() * variance * 2;
    particle (self.origin, vec, self.col, self.volume);
    self.nextthink = time + self.wait;
}


/*QUAKED misc_splash (0 .5 .8) (-8 -8 -8) (8 8 8)
Produces a continuous particle splash for waterfalls
"color" color of particles.  0 through 15, corresponds to a row of the quake palette. (default 0)
"movedir" average movement vector of particles (default 0 0 4)
"wait" time between particle generation cycles.  (default 0.1)
"volume" density of particles. (default 10)
*/
void() misc_splash = {
    if (!self.wait) self.wait = 0.1;
    if (!self.movedir) self.movedir = '0 0 4';
    if (!self.volume) self.volume = 10;
    self.col = self.col * 16;
    self.nextthink = time + self.wait;
    self.think = splash_think;
};

float FLAG_REDORANGE = 1;
float FLAG_YELLOW    = 2;
float FLAG_BLUE      = 4;
float FLAG_WHITE     = 8;
float FLAG_DARKRED   = 16;

void () light_globecolor = {

   if ( (self.spawnflags & FLAG_REDORANGE) ) {

      precache_model ("progs/s_light2.spr");
      setmodel (self,"progs/s_light2.spr");
      makestatic (self);

   }
   if ( (self.spawnflags & FLAG_YELLOW) ) {

      precache_model ("progs/s_light3.spr");
      setmodel (self,"progs/s_light3.spr");
      makestatic (self);

   }
   if ( (self.spawnflags & FLAG_BLUE) ) {

      precache_model ("progs/s_light4.spr");
      setmodel (self,"progs/s_light4.spr");
      makestatic (self);

   }
   if ( (self.spawnflags & FLAG_WHITE) ) {

      precache_model ("progs/s_light5.spr");
      setmodel (self,"progs/s_light5.spr");
      makestatic (self);

   }
   if ( (self.spawnflags & FLAG_DARKRED) ) {

      precache_model ("progs/s_light6.spr");
      setmodel (self,"progs/s_light6.spr");
      makestatic (self);

   }
   if ( (self.spawnflags & FALSE) ) {

      precache_model ("progs/s_light.spr");
      setmodel (self,"progs/s_light.spr");
      makestatic (self);

   }

};