void emit_particles();
void trigger_particles();
void particles_reset();

void info_particles() = {
  if (self.speed <= 0) {
    self.speed = 1;
    }
  if (self.delay <= 0) {
    self.delay = 0.1;
    }
  if (self.targetname) {
    if (self.spawnflags & 64) {
      self.use = particles_reset;
      self.think = emit_particles;
      self.nextthink = time + self.wait;
      return;
      }
    self.use = trigger_particles;
    return;
    }
  trigger_particles();
};

void trigger_particles() = {
  self.count = 0;
  self.use = particles_reset;
  self.think = emit_particles;
  self.nextthink = time + self.delay;
};

void particles_reset() = {
  self.use = trigger_particles;
  self.think = SUB_Null;
  self.nextthink = time + LARGE_TIMER;
};

void emit_particles() = {
  local vector spot1;

  if (self.endtime > 0) {
    self.count = self.count + self.delay;
    if (self.count > self.endtime) {
      particles_reset();
      return;
      }
    }
  if (self.method < 1) {
    particle(self.origin, self.movedir, self.cnt, self.duration);
    }
  else {
    makevectors(self.aether);
    self.aether_y = self.aether_y + self.speed;
    if (self.aether_y > 360) {
      self.aether_y = self.aether_y - 360;
      }
    if (self.aether_y < 0) {
      self.aether_y = self.aether_y + 360;
      }
    spot1 = self.origin + v_forward * self.impulse;
    particle(spot1, self.movedir, self.cnt, self.duration);
    if (self.method == 2) {
      spot1 = self.origin + v_forward * (0 - self.impulse);
      particle(spot1, self.movedir, self.cnt, self.duration);
      }
    }
  self.think = emit_particles;
  self.nextthink = time + self.delay;
};

void trigger_smoke();
void spawn_smokethang();
void stop_smoke();

// Launches invisible touchless grenades that have grenade particle trails
void misc_smokemaker() = {
  precache_model("progs/null.mdl");
  if (!(self.speed)) {
    self.speed = 2000;
  }
  if (self.cnt == 0) {
    self.cnt = 0.2;
  }
  if (self.wait == 0) {
    self.wait = 0.5;
  }
  if (self.targetname) {
    self.use = trigger_smoke;
    return;
  }
  self.nextthink = time + random();
  self.think = spawn_smokethang;
};

void trigger_smoke() = {
  self.use = stop_smoke;
  self.nextthink = time + random();
  self.think = spawn_smokethang;
};

void stop_smoke() = {
  self.nextthink = time + 99999;
  self.think = SUB_Null;
  self.use = trigger_smoke;
};

void spawn_smokethang() = {
  local entity head;

  head = spawn();
  head.alpha = -1;
  head.solid = SOLID_TRIGGER;
  head.movetype = MOVETYPE_NOCLIP;
  head.velocity = '0 0 1000';
  head.velocity_z = self.speed + random() * 200;
  setmodel(head, "progs/null.mdl");
  setsize(head, VEC_ORIGIN, VEC_ORIGIN);
  setorigin(head, self.origin);
  head.effects = head.effects & EF_NODRAW;
  head.nextthink = time + self.wait;
  head.think = SUB_Remove;
  if (self.cnt == -1) {
    self.nextthink = time + random();
  } else {
    self.nextthink = time + self.cnt;
  }
  self.think = spawn_smokethang;
};

void big_bang() = {
  local vector newo;
  local float x, y, z;
  if (self.wait == 1) {
    sound(self, CHAN_VOICE, "boss2/pop2.wav", 1, ATTN_NONE);
  }
  x = random() * FL_NOTARGET - FL_GODMODE;
  y = random() * FL_NOTARGET - FL_GODMODE;
  z = random() * FL_NOTARGET - FL_GODMODE;
  newo_x = self.origin_x + x;
  newo_y = self.origin_y + y;
  newo_z = self.origin_z + z;
  WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
  WriteByte(MSG_BROADCAST, TE_EXPLOSION);
  WriteCoord(MSG_BROADCAST, newo_x);
  WriteCoord(MSG_BROADCAST, newo_y);
  WriteCoord(MSG_BROADCAST, newo_z);
  self.wait = self.wait - 1;
  if (self.wait < 0) {
    entity_hide(self);
  }
  self.nextthink = time + 0.2 * random() + 0.1;
  self.think = big_bang;
};

void misc_boom() = {
  precache_sound2("boss2/pop2.wav");
  self.use = big_bang;
  self.wait = 1;
};