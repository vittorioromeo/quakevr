void anacdoom_1();
void anacdoom_2();
void anacdoom_3();
void anacdoom_4();

float ANAC_NOINTERMISSION = 2;

void() Puke_Touch = {
    local vector org;
    if (other == self.owner) return;
    if (pointcontents(self.origin) == CONTENT_SKY) { remove(self); return; }
    org = self.origin - MOVETYPE_NOCLIP * normalize(self.velocity);
    SpawnBlood(other,org, self.velocity * 0.2, 15);
    T_Damage(other, self, self.owner, MOVETYPE_BOUNCE,DAMARMOR);
    T_RadiusDamage(self, self.owner, SVC_INTERMISSION, other,DAMAGEALL);
    self.origin = self.origin - MOVETYPE_NOCLIP * normalize(self.velocity);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);
    BecomeExplosion();
};

void(vector p) anac_missile = {
    local vector offang,org,vec,d;

    offang = vectoangles(self.enemy.origin - self.origin);
    makevectors(offang);
    org = self.origin + p_x * v_forward + p_y * v_right + p_z * '0 0 1';
    d = self.enemy.origin;
    vec = normalize(d - org);
    newmis = spawn();
    newmis.owner = self;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    newmis.angles = vectoangles(vec);
    newmis.classname = "spike";
    newmis.think = SUB_Remove;
    newmis.nextthink = time + TE_LIGHTNING2;
    setorigin(newmis, org);
    setmodel(newmis, "progs/drake/s_explod.spr");
    newmis.avelocity = '200 100 300';
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);
    newmis.velocity = vec * 800;
    newmis.touch = Puke_Touch;
};

void() Bite_Touch = {
    local vector org;
    if (other == self.owner) return;
    if (pointcontents(self.origin) == CONTENT_SKY) { remove(self); return; }

    org = self.origin - MOVETYPE_NOCLIP * normalize(self.velocity);
    if (other.health) {
        SpawnBlood(other,org, self.velocity * 0.2, 15);
        T_Damage(other, self, self.owner, SVC_INTERMISSION,DAMARMOR);
        sound(self, CHAN_VOICE, "demon/dhit2.wav", ATTN_NORM, ATTN_IDLE);
        self.gib1mdl = "progs/gib1.mdl";
        ThrowGib(11, 1);
    } else {
        remove(self);
        return;
    }
    remove(self);
};

void(float offset) AnacMissile = {
    local vector offang;
    local vector org;
    local vector vec;
    offang = vectoangles(self.enemy.origin - self.origin);
    offang_y = offang_y + offset * MOVETYPE_TOSS;
    makevectors(offang);
    org = self.origin - SVC_INTERMISSION * normalize(self.velocity);
    vec = normalize(v_forward);
    vec_z = MSG_BROADCAST - vec_z + (random() - 0.5) * 0.1;
    newmis = spawn();
    newmis.owner = self;
    newmis.movetype = MOVETYPE_FLYMISSILE;
    newmis.solid = SOLID_BBOX;
    newmis.angles = vectoangles(vec);
    newmis.classname = "spike";
    setorigin(newmis, org);
    newmis.classname = "LaunchSpike";
    setmodel(newmis, "progs/spike2.mdl");
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);
    newmis.avelocity = '200 100 300';
    newmis.velocity = vec * 1000;
    sound(self, CHAN_WEAPON, "doors/airdoor2.wav", CHAN_WEAPON, ATTN_NORM);
    newmis.touch = Bite_Touch;
    newmis.nextthink = time + 0.2;
    newmis.think = SUB_Remove;
};

void() anac_stand1 = [79, anac_stand1] { ai_stand(); };

void() anac_fall = [80, anac_fall1] {};
void() anac_fall1 = [81, anac_fall2]
{
};
void() anac_fall2 = [82, anac_fall3]
{
};
void() anac_fall3 = [83, anac_fall4]
{
};
void() anac_fall4 = [84, anac_fall5]
{
};
void() anac_fall5 = [85, anac_down]
{
    sound(self, CHAN_WEAPON, "boss1/out1.wav", CHAN_WEAPON, ATTN_NORM);
};
void() anac_fall6 = [86, anac_fall7]
{
};
void() anac_fall7 = [87, anac_fall8]
{
};

void() anac_fall8 = [88, anac_fall9]
{
};

void() anac_fall9 = [89, anac_stand1]
{
};

void() anac_rise = [0, anac_rise02] { sound(self, CHAN_WEAPON, "boss1/out1.wav", CHAN_WEAPON, ATTN_NORM); };
void() anac_rise02 = [1, anac_rise03] { sound(self, CHAN_VOICE, "boss1/sight1.wav", ATTN_NORM, ATTN_NORM); };
void() anac_rise03 = [2, anac_rise04] {};
void() anac_rise04 = [3, anac_rise05] {};
void() anac_rise05 = [4, anac_rise06] {};
void() anac_rise06 = [5, anac_rise07] {};
void() anac_rise07 = [6, anac_rise08] {};
void() anac_rise08 = [7, anac_rise09] {};
void() anac_rise09 = [8, anac_rise10] {};
void() anac_rise10 = [9, anac_rise11] {};
void() anac_rise11 = [10, anac_rise12] {};
void() anac_rise12 = [11, anac_rise13] {};
void() anac_rise13 = [12, anac_rise14] {};
void() anac_rise14 = [13, anac_rise15] {};
void() anac_rise15 = [14, anac_rise16] {};
void() anac_rise16 = [15, anac_rise17] {};
void() anac_rise17 = [16, anac_run] { self.th_run = anac_run; };

void() anac_walk01 = [89, anac_walk01]
{
    ai_walk(MOVETYPE_BOUNCE);
};

void() anac_run = [17, anac_run01]
{
    ai_walk(20);
};

void() anac_run01 = [18, anac_run02]
{
    ai_walk(20);
};

void() anac_run02 = [19, anac_run03]
{
    ai_walk(20);
};

void() anac_run03 = [20, anac_run04]
{
    ai_walk(20);
};

void() anac_run04 = [21, anac_run05]
{
    ai_walk(20);
};

void() anac_run05 = [22, anac_run06]
{
    ai_walk(20);
};

void() anac_run06 = [23, anac_run07]
{
    ai_walk(20);
};

void() anac_run07 = [24, anac_run08]
{
    ai_walk(20);
};

void() anac_run08 = [25, anac_run09]
{
    ai_walk(20);
};

void() anac_run09 = [26, anac_run10]
{
    ai_walk(20);
};

void() anac_run10 = [27, anac_run11]
{
    ai_walk(20);
};

void() anac_run11 = [28, anac_run12]
{
    ai_walk(20);
};

void() anac_run12 = [29, anac_run13]
{
    ai_walk(20);
};

void() anac_run13 = [30, anac_run14]
{
    ai_walk(20);
};

void() anac_run14 = [31, anac_run15]
{
    ai_walk(20);
};

void() anac_run15 = [32, anac_run16]
{
    ai_walk(20);
};

void() anac_run16 = [33, anac_run17]
{
    ai_walk(MSG_BROADCAST);
};

void() anac_run17 = [34, anac_run18]
{
    ai_walk(MSG_BROADCAST);
};

void() anac_run18 = [35, anac_run19]
{
    ai_walk(MSG_BROADCAST);
};

void() anac_run19 = [36, anac_hmmm]
{
    ai_walk(MSG_BROADCAST);
};

void() anac_run20 = [37, anac_run21]
{
    ai_run(MSG_BROADCAST);
};

void() anac_run21 = [38, anac_run22]
{
    ai_run(MSG_BROADCAST);
};

void() anac_run22 = [39, anac_run23]
{
    ai_run(MSG_BROADCAST);
};

void() anac_run23 = [40, anac_run24]
{
    ai_run(MSG_BROADCAST);
};

void() anac_run24 = [41, anac_run25]
{
    ai_run(MSG_BROADCAST);
};

void() anac_run25 = [42, anac_run26]
{
    ai_run(MSG_BROADCAST);
};

void() anac_run26 = [43, anac_run27]
{
    ai_run(20);
};

void() anac_run27 = [44, anac_run28]
{
    ai_run(20);
};

void() anac_run28 = [45, anac_run29]
{
    ai_run(20);
};

void() anac_run29 = [46, anac_run30]
{
    ai_run(20);
};

void() anac_run30 = [47, anac_run]
{
    ai_run(20);
};

void() anac_hmmm = 
{
    if (self.enemy.health <= MSG_BROADCAST)
    {
        anac_fall();
    }
    else
    {
        anac_run20();
    }
};

void() anac_attk = [108, anac_attk1]
{
    ai_face();
    sound(self, CHAN_VOICE, "hknight/idle.wav", ATTN_NORM, ATTN_NORM);
};

void() anac_attk1 = [109, anac_attk2]
{
    ai_face();
};

void() anac_attk2 = [110, anac_attk3]
{
    ai_face();
};

void() anac_attk3 = [111, anac_attk4]
{
    ai_face();
    ai_forward(TE_LIGHTNING1);
};

void() anac_attk4 = [112, anac_attk5]
{
    ai_face();
    ai_forward(MOVETYPE_BOUNCE);
};

void() anac_attk5 = [113, anac_attk6]
{
    ai_face();
    ai_forward(MOVETYPE_BOUNCE);
};

void() anac_attk6 = [114, anac_attk7]
{
    ai_face();
    ai_forward(MOVETYPE_BOUNCE);
};

void() anac_attk7 = [115, anac_attk8]
{
    ai_face();
    ai_forward(MOVETYPE_BOUNCE);
};

void() anac_attk8 = [116, anac_attk9]
{
    anac_missile('100 0 200');
    sound(self, CHAN_VOICE, "boss2/pop2.wav", ATTN_NORM, ATTN_NONE);
    ai_face();
    ai_forward(20);
};

void() anac_attk9 = [117, anac_attk10]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(20);
};

void() anac_attk10 = [118, anac_attk11]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(20);
};

void() anac_attk11 = [119, anac_attk12]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(20);
};

void() anac_attk12 = [120, anac_attk13]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(20);
};

void() anac_attk13 = [121, anac_attk14]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(20);
};

void() anac_attk14 = [122, anac_attk15]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(20);
};

void() anac_attk15 = [123, anac_attk16]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(20);
};

void() anac_attk16 = [124, anac_attk17]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(MOVETYPE_BOUNCE);
};

void() anac_attk17 = [125, anac_attk18]
{
    anac_missile('100 0 200');
    ai_face();
    ai_forward(TE_LIGHTNING1);
};

void() anac_attk18 = [126, anac_attk19]
{
    anac_missile('100 0 200');
    ai_face();
};

void() anac_attk19 = [127, anac_attk20]
{
    anac_missile('100 0 200');
    ai_face();
};

void() anac_attk20 = [128, anac_attk21]
{
    anac_missile('100 0 200');
    ai_face();
};

void() anac_attk21 = [129, anac_attk22]
{
    anac_missile('100 0 200');
    ai_face();
};

void() anac_attk22 = [130, anac_attk23]
{
    ai_face();
};

void() anac_attk23 = [131, anac_attk24]
{
    ai_face();
};

void() anac_attk24 = [132, anac_run]
{
    ai_face();
};

void() anac_eat = [90, anac_eat1]
{
    ai_charge(SVC_INTERMISSION);
    ai_face();
    sound(self, CHAN_VOICE, "hknight/idle.wav", ATTN_NORM, ATTN_NORM);
    sound(self, CHAN_WEAPON, "zombie/z_shot1.wav", CHAN_WEAPON, ATTN_NORM);
};

void() anac_eat1 = [91, anac_eat2]
{
    ai_charge(-20);
    ai_face();
};

void() anac_eat2 = [92, anac_eat3]
{
    ai_charge(-20);
    ai_face();
};

void() anac_eat3 = [93, anac_eat4]
{
    ai_charge(-10);
    ai_face();
    AnacMissile(ATTN_NONE);
    AnacMissile(ATTN_NORM);
    AnacMissile(CONTENT_EMPTY);
    AnacMissile(CHAN_VOICE);
    AnacMissile(CONTENT_SOLID);
};

void() anac_eat4 = [94, anac_eat5]
{
    ai_charge(-10);
    ai_face();
};

void() anac_eat5 = [95, anac_eat6]
{
    ai_charge(-10);
    ai_face();
};

void() anac_eat6 = [96, anac_eat7]
{
    ai_charge(MOVETYPE_BOUNCE);
    ai_face();
};

void() anac_eat7 = [97, anac_eat8]
{
    ai_charge(MOVETYPE_BOUNCE);
    ai_face();
};

void() anac_eat8 = [98, anac_eat9]
{
    ai_charge(MOVETYPE_BOUNCE);
    ai_face();
};

void() anac_eat9 = [99, anac_eat10]
{
    ai_charge(-10);
    ai_face();
    sound(self, CHAN_VOICE, "hknight/idle.wav", ATTN_NORM, ATTN_NORM);
    sound(self, CHAN_WEAPON, "zombie/z_shot1.wav", CHAN_WEAPON, ATTN_NORM);
};

void() anac_eat10 = [100, anac_eat11]
{
    ai_charge(-10);
    ai_face();
};

void() anac_eat11 = [101, anac_eat12]
{
    ai_charge(-10);
    ai_face();
};

void() anac_eat12 = [102, anac_eat13]
{
    ai_charge(-10);
    ai_face();
    AnacMissile(ATTN_NONE);
    AnacMissile(ATTN_NORM);
    AnacMissile(CONTENT_EMPTY);
};

void() anac_eat13 = [103, anac_eat14]
{
    ai_charge(-10);
    ai_face();
};

void() anac_eat14 = [104, anac_eat15]
{
    ai_charge(-10);
    ai_face();
};

void() anac_eat15 = [105, anac_eat16]
{
    ai_charge(MOVETYPE_BOUNCE);
    ai_face();
};

void() anac_eat16 = [106, anac_eat17]
{
    ai_charge(MOVETYPE_BOUNCE);
    ai_face();
};

void() anac_eat17 = [107, anac_run]
{
    ai_charge(MOVETYPE_BOUNCE);
    ai_face();
};

void(entity inflictor, entity attacker, float damage) anac_pain01 = { return; }; // no response to pain

void() anac_die = [10, anac_die]
{
    anacdoom_1();
};

void() anac_doom = [4, anac_doom02]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "m");
};

void() anac_doom02 = [5, anac_doom03]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "k");
};

void() anac_doom03 = [6, anac_doom04]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "k");
};

void() anac_doom04 = [5, anac_doom05]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "i");
};

void() anac_doom05 = [6, anac_doom06]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "g");
};

void() anac_doom06 = [4, anac_doom07]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "e");
};

void() anac_doom07 = [5, anac_doom08]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "a");
};

void() anac_doom08 = [6, anac_doom09]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "c");
};

void() anac_doom09 = [5, anac_doom10]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "e");
};

void() anac_doom10 = [4, anac_doom11]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "i");
};

void() anac_doom11 = [5, anac_doom12]
{
    ai_pain(ATTN_NORM);
    lightstyle(ATTN_NONE, "m");
};

void() anac_doom12 = [6, anac_doom12] {
    anacdoom_4();
    lightstyle(ATTN_NONE, "m");
};

void() anacdoom_1 = {
    /*
    local entity pos;
    local entity pl;
    local entity timer;

    
    if (!(self.spawnflags & ANAC_NOINTERMISSION)) {
        intermission_exittime = time + 10000000;
        intermission_running = ATTN_NORM;
        pos = find(world, classname, "info_intermission");
        if (!pos) error("no info_intermission");
        WriteByte(MSG_ALL, SVC_FINALE);
        WriteString(MSG_ALL, "");
        pl = find(world, classname, "player");
        pl.angles = pos.mangle;
        pl.takedamage = DAMAGE_NO;
        pl.solid = SOLID_NOT;
        pl.movetype = MOVETYPE_NONE;
        pl.modelindex = MOVETYPE_NONE;
        setorigin(pl, pos.origin);
        pl = find(pl, classname, "player");
    }
    timer = spawn();
    timer.nextthink = time;
    timer.think = anacdoom_2;*/
};

void() anacdoom_2 = {
    local vector o;

    o = annie.origin - '0 100 0';
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_TAREXPLOSION);
    WriteCoord(MSG_BROADCAST, o_x);
    WriteCoord(MSG_BROADCAST, o_y);
    WriteCoord(MSG_BROADCAST, o_z);
    self.nextthink = time + 1.5;
    self.nextthink = time;
    self.think = anacdoom_3;
};

void() anacdoom_3 = {
    annie.think = anac_doom;
    sound(annie, CHAN_VOICE, "boss2/death.wav", ATTN_NORM, ATTN_NONE);
    lightstyle(ATTN_NONE, "abcdefghijklmlkjihgfedcb");
};

void() anacdoom_4 = {
    local vector oldo;
    local float x,y,z,r;

    oldo = self.origin;
    sound(self, CHAN_VOICE, "boss2/pop2.wav", ATTN_NORM, ATTN_NONE);
    z = 16;
    self.gib1mdl = annie.gib1mdl;
    self.gib2mdl = annie.gib2mdl;
    self.gib3mdl = annie.gib3mdl;
    while (z <= 144) {
        x = -64;
        while (x <= 64) {
            y = -64;
            while (y <= 64) {
                self.origin_x = oldo_x + x;
                self.origin_y = oldo_y + y;
                self.origin_z = oldo_z + z;
                r = random();
                if (r < 0.3) {
                    ThrowGib(11, 1);
                } else {
                    if (r < 0.6) {
                        ThrowGib(12, 1);
                    } else {
                        ThrowGib(13, 1);
                    }
                }
                y = y + 32;
            }
            x = x + 32;
        }
        z = z + 96;
    }
    if (!(self.spawnflags & ANAC_NOINTERMISSION)) {
        WriteByte(MSG_ALL, SVC_FINALE);
        WriteString(MSG_ALL, "Congratulations, soldier...\nYou've single handedly destroyed the\nvile Anaconda beast and all her\nspawn... Good work..\n\n-CREDITS-\nQUAKE(r) is a registered\ntrademark of Id Software, Inc.\nQUAKE(r) Scrag,Rottfish,Ogre characters\n& Textures included within this product\n(c)1996 Id Software, Inc.\nAll Rights reserved.\nLevel design: S.Thoms aka. MAGICMAN\nAnaconda skin: L.Pope\nSnakeman skin: C.Bolin\nSnakeModels+Code: S.Y.Tan aka. BUBBAH\nLevel Concept by:\nPrecision Entertainment, Inc.\n");
    }
    remove(self);
    WriteByte(MSG_ALL, SVC_CDTRACK);
    WriteByte(MSG_ALL, TE_EXPLOSION);
    WriteByte(MSG_ALL, TE_EXPLOSION);
    lightstyle(ATTN_NONE, "z");
};

void() anac_explode1 = [133, anac_explode2] { self.takedamage = DAMAGE_NO; };
void() anac_explode2 = [133, anac_explode2] {
    T_RadiusDamage(self, self, 20, world,DAMAGEALL);
    sound(self, CHAN_VOICE, "blob/death1.wav", ATTN_NORM, ATTN_NORM);
    self.origin = self.origin - MOVETYPE_NOCLIP * normalize(self.velocity);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_TAREXPLOSION);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);
    BecomeExplosion();
    if (!(self.spawnflags & ANAC_NOINTERMISSION)) {
        WriteByte(MSG_ALL, SVC_FINALE);
        WriteString(MSG_ALL, "Congratulations, soldier...\nAs the Anaconda's Huge body\nslithers lifeless\naway from this earth,\nyou must agree, that this\nmother of snakemen,\nthis vile devilthing,\nwas a pretty tough cookie\n\nJump through the slipgate to exit\nthis mission...\n\nGood work.\n\nLevel design: MAGICMAN\nAnaconda skin: POPE\nSnakeman skin: BOLIN\nSnakeModels + Coding: BUBBAH\n");
    }
};

void() anac_awake = {
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);
    self.yaw_speed = 20;
    anac_rise();
};

void() anac_down = {
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LAVASPLASH);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);
    self.yaw_speed = 20;
    anac_fall6();
};

void() check_anac = {
    local vector dist;
    local float d;
    dist = self.enemy.origin - self.origin;
    d = vlen(dist);
    if (d < 250) {
        anac_eat();
    } else {
        anac_attk();
    }
};

void() monster_anac = {
    if (deathmatch) { remove(self); return; }
    precache_sound2("blob/death1.wav");
    precache_sound2("boss1/out1.wav");
    precache_sound2("boss1/sight1.wav");
    precache_sound2("hknight/idle.wav");
    precache_sound2("demon/dhit2.wav");
    precache_sound2("enforcer/enfstop.wav");
    precache_sound2("zombie/z_shot1.wav");
    precache_sound2("items/damage2.wav");
    precache_sound2("doors/airdoor2.wav");
    precache_sound2("boss2/pop2.wav");
    precache_sound2("boss2/death.wav");
    precache_model2("progs/anaconda/anaconda.mdl");
    precache_model2("progs/drake/s_explod.spr");
    precache_model2("progs/spike2.mdl");
    precache_model("progs/gib1.mdl");
    self.gib1mdl = "progs/gib1.mdl";
    precache_model("progs/gib2.mdl");
    self.gib2mdl = "progs/gib2.mdl";
    precache_model("progs/gib3.mdl");
    self.gib3mdl = "progs/gib3.mdl";
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.mdl = "progs/anaconda/anaconda.mdl";
    self.sight_sound = "boss1/sight1.wav";
    if (!self.bboxtype) self.bboxtype = BBOX_ANACONDA;
    if (self.health < 1) self.health = 1700;
    self.th_stand = anac_stand1;
    self.th_walk = anac_walk01;
    self.th_run = anac_awake;
    self.th_pain = anac_pain01;
    self.th_missile = check_anac;
    self.th_melee = check_anac;
    self.th_die = anac_die;
    self.bossflag = TRUE;
    annie = self;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;
    if (self.deathstring == "")  self.deathstring = " was killed by a giant anaconda\n";

    if(!self.classtype) self.classtype = CT_MONANACONDA;
    if (!self.classgroup) self.classgroup = CG_BOSS;
    self.classmove = MON_MOVEWALK;    

    monster_start();
};

