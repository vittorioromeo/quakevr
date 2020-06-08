void(vector p) newboss_missile;

entity() FindNewBossSpot = {
    local entity spot;
    local float cyc;
    spot = find(world, classname, "info_boss_spot");
    if (spot)
    {
        cyc = random() * MOVETYPE_STEP;
        while (cyc > 1)
        {
            spot = find(spot, classname, "info_boss_spot");
            if (!spot)
            {
                spot = find(spot, classname, "info_boss_spot");
            }
            cyc = cyc - 1;
        }
        return spot;
    }
    return self;
};

void() newboss_face = 
{
    if (self.enemy.health <= 0 || random() < 0.020000)
    {
        self.enemy = find(self.enemy, classname, "player");
        if (!(self.enemy))
        {
            self.enemy = find(self.enemy, classname, "player");
        }
    }
    ai_face();
};

void() newboss_rise1 = [1, newboss_rise2]
{
    setmodel(self, self.mdl);
    setsize(self, VEC_HULL2_MIN, '32 32 256');
    self.solid = SOLID_SLIDEBOX;
    sound(self, CHAN_WEAPON, "boss1/out1.wav", 1, ATTN_NORM);
    newboss_face();
};

void() newboss_rise2 = [2, newboss_rise3]
{
    newboss_face();
};

void() newboss_rise3 = [3, newboss_rise4]
{
    newboss_face();
};

void() newboss_rise4 = [4, newboss_rise5]
{
    newboss_face();
};

void() newboss_rise5 = [5, newboss_rise6]
{
    newboss_face();
};

void() newboss_rise6 = [6, newboss_rise7]
{
    newboss_face();
};

void() newboss_rise7 = [7, newboss_rise8]
{
    newboss_face();
};

void() newboss_rise8 = [8, newboss_missile1]
{
    self.takedamage = DAMAGE_AIM;
    self.th_stand();
};

void() newboss_newplace = 
{
    local entity newspot;
    self.model = string_null;
    self.nextthink = time + 1;
    self.think = newboss_rise1;
    newspot = FindNewBossSpot();
    setorigin(self, newspot.origin);
    //self.origin_z = self.origin_z + 24;
};

void() newboss_sink1 = [8, newboss_sink2]
{
    self.takedamage = DAMAGE_NO;
    setsize(self, VEC_HULL2_MIN, '32 32 0');
};

void() newboss_sink2 = [7, newboss_sink3]
{
};

void() newboss_sink3 = [6, newboss_sink4]
{
};

void() newboss_sink4 = [5, newboss_sink5]
{
};

void() newboss_sink5 = [4, newboss_sink6]
{
};

void() newboss_sink6 = [3, newboss_sink7]
{
};

void() newboss_sink7 = [2, newboss_sink8]
{
};

void() newboss_sink8 = [1, newboss_newplace]
{
    sound(self, CHAN_WEAPON, "boss1/out1.wav", 1, ATTN_NORM);
};

void() newboss_idle1 = [16, newboss_idle2]
{
    if (self.enemy.health > 0 && (random() < 0.5 || (skill == MOVETYPE_WALK && random() < 0.8)))
    {
        newboss_missile1();
    }
};

void() newboss_idle2 = [15, newboss_idle3]
{
    newboss_face();
};

void() newboss_idle3 = [14, newboss_idle4]
{
    newboss_face();
};

void() newboss_idle4 = [15, newboss_idle5]
{
    newboss_face();
};

void() newboss_idle5 = [16, newboss_idle6]
{
    newboss_face();
};

void() newboss_idle6 = [8, newboss_idle1]
{
    newboss_face();
};

void() newboss_missile1 = [9, newboss_missile2]
{
    newboss_missile('66 0 180');
};

void() newboss_missile2 = [10, newboss_missile3]
{
    newboss_face();
};

void() newboss_missile3 = [11, newboss_missile4]
{
    newboss_face();
};

void() newboss_missile4 = [12, newboss_missile5]
{
    newboss_face();
};

void() newboss_missile5 = [13, newboss_missile6]
{
    newboss_face();
};

void() newboss_missile6 = [14, newboss_missile7]
{
    newboss_face();
};

void() newboss_missile7 = [15, newboss_missile8]
{
    newboss_face();
};

void() newboss_missile8 = [16, newboss_missile9]
{
    newboss_face();
};

void() newboss_missile9 = [8, newboss_idle1]
{
    newboss_face();
};

void() newboss_hurt1 = [29, newboss_hurt2]
{
};

void() newboss_hurt2 = [30, newboss_hurt3]
{
};

void() newboss_hurt3 = [31, newboss_hurt4]
{
};

void() newboss_hurt4 = [32, newboss_idle1]
{
};

void(entity inflictor, entity attacker, float damage) newboss_damage = {
    if (self.health <= 0) { self.th_die(); remove(self); return; }
    if (random() < 0.2) {
        newboss_sink1();
        return;
    }
    if (random() * 300 > damage) {
        return;
    }
    if (self.pain_finished > time) {
        return;
    }
    self.pain_finished = time + FL_SWIM;
    newboss_hurt1();
};

void() newboss_finale2 = {
    if (!(self.spawnflags & 2)) {
        WriteByte(MSG_ALL, SVC_FINALE);
        WriteString(MSG_ALL, "Designers & Artists:\nIntroduction - Eric Sage\nMunitions Warehouse - Eric Sage\nResearch Lab - John Zwicker\nMax Security Prison - Chris Haire\nUnderground Testing Station - \nKristian Hawkinson\nStation Omega - Eric Sage\nSubway Station - Bryan Baldwin\nThe BackAlleys - Eric Sage\nOffice Park - Sean Conlon\nFinal Confrontation - \nKristian Hawkinson, Eric Sage\nNew Textures - Brenda Lo, Eric Sage\n3D Content - Matt Peckham\nThe Wyrm - Eric Sage, John Zwicker");
    }
    remove(self);
};

void() newboss_finale = {
    local entity pos;
    local entity pl;
    killed_monsters = killed_monsters + 1;
    self.model = string_null;
    self.mdl = string_null;
    WriteByte(MSG_ALL, SVC_KILLEDMONSTER);
    if (!(self.spawnflags & 2)) {
        intermission_exittime = time + 10000000;
        intermission_running = 1;
        pos = find(world, classname, "info_intermission");
        if (!pos) {
            pos = find(world, classname, "info_boss_spot");
        }
        if (!pos) {
            error("no info_intermission");
        }
    
        WriteByte(MSG_ALL, SVC_FINALE);
        WriteString(MSG_ALL, "Congratulations! You have completed the\ndemo episodes created for you\nby Virtus!\nNow your mission is to create\nyour own nightmare!\n");
        pl = find(world, classname, "player");
        while (pl != world) {
            pl.view_ofs = VEC_ORIGIN;
            pl.angles = pos.mangle;
            pl.fixangle = 1;
            pl.map = self.map;
            pl.nextthink = time + 0.5;
            //pl.takedamage = DAMAGE_NO;
            pl.solid = SOLID_NOT;
            pl.movetype = MOVETYPE_NONE;
            pl.modelindex = 0;
            setorigin(pl, pos.origin);
            pl = find(pl, classname, "player");
        }
    }
    self.nextthink = time + 20;
    self.think = newboss_finale2;
};

void() newboss_death1 = [17, newboss_death2] {
    sound(self, CHAN_VOICE, "boss1/death.wav", 1, ATTN_NORM);
};

void() newboss_death2 = [18, newboss_death3]
{
};

void() newboss_death3 = [19, newboss_death4]
{
};

void() newboss_death4 = [20, newboss_death5]
{
};

void() newboss_death5 = [21, newboss_death6]
{
};

void() newboss_death6 = [22, newboss_death7]
{
};

void() newboss_death7 = [23, newboss_death8]
{
};

void() newboss_death8 = [24, newboss_death9]
{
};

void() newboss_death9 = [25, newboss_death10]
{
};

void() newboss_death10 = [26, newboss_death11]
{
};

void() newboss_death11 = [27, newboss_death12]
{
};

void() newboss_death12 = [28, newboss_death12]
{
    local float z;
    z = self.origin_z - 50;
    sound(self, CHAN_BODY, "boss1/out1.wav", 1, ATTN_NORM);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, z);
    self.use = SUB_Null;
    self.th_die = SUB_Null;
    self.nextthink = time + 0.2;
    self.solid = SOLID_NOT;
    self.think = newboss_finale;
};

void(vector p) newboss_missile = 
{
    local vector offang;
    local vector org;
    local vector vec;
    local vector d;
    local float t;
    offang = vectoangles(self.enemy.origin - self.origin);
    makevectors(offang);
    org = self.origin + p_x * v_forward + p_y * v_right + p_z * '0 0 1';
    if (skill > 1) {
        t = vlen(self.enemy.origin - org) / 300;
        vec = self.enemy.velocity;
        vec_z = 0;
        d = self.enemy.origin + t * vec;
    } else {
        d = self.enemy.origin;
    }
    vec = normalize(d - org);
    newmis = spawn();
    newmis.owner = self;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    newmis.angles = vectoangles(vec);
    newmis.classname = "spike";
    newmis.think = SUB_Remove;
    newmis.nextthink = time + MOVETYPE_TOSS;
    setorigin(newmis, org);
    setmodel(newmis, "progs/lavaball.mdl");
    newmis.avelocity = '200 100 300';
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);
    newmis.velocity = vec * 300;
    newmis.classtype = CT_PROJ_FIRETOP;
    newmis.touch = Touch_Missile;
    sound(self, CHAN_WEAPON, "weapons/sgun1.wav", 1, ATTN_NORM);
    if (self.enemy.health <= 0) newboss_idle1();
};

void() newboss_awake = {
    local float z;
    self.use = SUB_Null;
    z = self.origin_z - 50;
    self.flags = FL_MONSTER;
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;
    self.takedamage = DAMAGE_AIM;
    self.th_pain = newboss_damage;
    setmodel(self, "progs/virtus/newboss.mdl");
    setsize(self, VEC_HULL2_MIN, '32 32 256');
    self.enemy = activator;
    self.yaw_speed = 20;
    self.th_stand = newboss_missile1;
    self.th_run = newboss_missile1;
    newboss_rise1();
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, z);
};

void() monster_newboss = {
    if (deathmatch) { remove(self); return; }
    precache_model("progs/virtus/newboss.mdl");
    precache_model("progs/lavaball.mdl");
    precache_sound("weapons/rocket1i.wav");
    precache_sound("boss1/out1.wav");
    precache_sound("boss1/sight1.wav");
    precache_sound("misc/power.wav");
    precache_sound("boss1/throw.wav");
    precache_sound("boss1/pain.wav");
    precache_sound("boss1/death.wav");
    total_monsters = total_monsters + 1;
    self.use = newboss_awake;
    self.mdl = "progs/virtus/newboss.mdl";
    if (self.health < 1) self.health = 1800;
    self.th_run = SUB_Null;
    self.max_health = self.health;
    if (world.devdata & DEV_AMMO_N_HEALTH) sendmonsterdevdata(self);
    if (self.deathstring == "") self.deathstring = " was destroyed by a Virtus Worm\n";
    self.th_die = newboss_death1;
    self.th_pain = newboss_damage;
    self.classtype = CT_MONVIRTUS;
};

void() monster_virtus = { self.classname = "monster_newboss"; monster_newboss(); };

void() info_boss_spot = {};