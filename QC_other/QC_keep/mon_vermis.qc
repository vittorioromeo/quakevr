void vermis_wake1();
void vermis_trans1();
void vermis_idle1();

void() VermisFoundTarget = 
{
    if (self.enemy.classname == "player")
    {
        sight_entity = self;
        sight_entity_time = time;
    }
    self.show_hostile = time + 1;
    self.goalentity = self.enemy;
    if (self.coiled != 0)
    {
        self.think = vermis_wake1;
        sound(self, CHAN_VOICE, "vermis/sight.wav", 1, 0.4);
    }
    else
    {
        self.think = vermis_trans1;
        sound(self, CHAN_VOICE, "vermis/sight2.wav", 1, 0.4);
    }
    self.th_stand = vermis_idle1;
    self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    self.nextthink = time + 0.1;
    self.shield = 1;
};

void() vermis_use = 
{
    if (self.enemy)
    {
        return;
    }
    if (self.health <= 0)
    {
        return;
    }
    if (activator.items & IT_INVISIBILITY)
    {
        return;
    }
    if (activator.flags & FL_NOTARGET)
    {
        return;
    }
    if (activator.classname != "player")
    {
        return;
    }
    self.enemy = activator;
    self.nextthink = time + 0.1;
    self.think = VermisFoundTarget;
};

float() VermisFindTarget = 
{
    local entity client;
    local float r;
    if (sight_entity_time >= time - 0.1 && !(self.spawnflags & AS_MELEE))
    {
        client = sight_entity;
        if (client.enemy == self.enemy)
        {
            return 0;
        }
    }
    else
    {
        client = checkclient();
        if (!client)
        {
            return 0;
        }
    }
    if (client == self.enemy)
    {
        return 0;
    }
    if (client.flags & FL_NOTARGET)
    {
        return 0;
    }
    if (client.items & IT_INVISIBILITY)
    {
        return 0;
    }
    r = range(client);
    if (r == RANGE_FAR)
    {
        return 0;
    }
    if (!visible(client))
    {
        return 0;
    }
    if (r == RANGE_NEAR)
    {
        if (client.show_hostile < time && !infront(client))
        {
            return 0;
        }
    }
    else
    {
        if (r == RANGE_MID)
        {
            if (!infront(client))
            {
                return 0;
            }
        }
    }
    self.enemy = client;
    if (self.enemy.classname != "player")
    {
        self.enemy = self.enemy.enemy;
        if (self.enemy.classname != "player")
        {
            self.enemy = world;
            return 0;
        }
    }
    VermisFoundTarget();
    return 1;
};

void() vermis_start_go = 
{
    if (!(self.spawnflags & 64))
    {
        self.origin_z = self.origin_z + 1;
    }
    self.takedamage = DAMAGE_AIM;
    self.ideal_yaw = self.angles * '0 1 0';
    if (!(self.yaw_speed))
    {
        self.yaw_speed = 15;
    }
    self.view_ofs = '0 0 25';
    self.use = vermis_use;
    self.flags = self.flags | FL_MONSTER;
    self.flags = self.flags | FL_FLY;
    self.pausetime = 100000000;
    self.th_stand();
    self.nextthink = self.nextthink + random() * 0.5;
    if (self.spawnflags & 64 && (self.spawnflags & 128) && !(activator.items & IT_INVISIBILITY) && !(activator.flags & FL_NOTARGET) && activator.classname == "player")
    {
        self.enemy = activator;
        self.nextthink = time + 0.1;
        self.think = FoundTarget;
    }
    if (self.awake == 1 && !(self.enemy.items & IT_INVISIBILITY) && !(self.enemy.flags & FL_NOTARGET) && self.enemy.classname == "player")
    {
        self.nextthink = time + 0.1;
        self.think = FoundTarget;
    }
};

void() vermis_start = 
{
    if (!(self.spawnflags & 64))
    {
        self.nextthink = self.nextthink + random() * 0.5;
        self.think = vermis_start_go;
    }
    else
    {
        vermis_start_go();
    }
    total_monsters = total_monsters + 1;
};



void() ai_vermis_face = 
{
    if (visible(self.enemy))
    {
        self.ideal_yaw = vectoyaw(self.enemy.origin - self.origin);
    }
    else
    {
        self.ideal_yaw = vectoyaw(self.oldenemyorigin - self.origin);
    }
    ChangeYaw();
};

void() ai_vermis_stand = 
{
    if (VermisFindTarget())
    {
        return;
    }
};

float() CheckVermisAttack = 
{
    local vector spot1;
    local vector spot2;
    local entity targ;
    local float chance;
    local float dist;
    targ = self.enemy;
    dist = vlen(self.enemy.origin - self.origin);
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline(spot1, spot2, 0, self);
    if (trace_inopen && trace_inwater)
    {
        return 0;
    }
    if (dist < 340 && self.enemy.movetype != MOVETYPE_FLY)
    {
        if (self.th_melee)
        {
            self.th_melee();
            return 1;
        }
    }
    if (!(self.th_missile))
    {
        return 0;
    }
    if (time < self.attack_finished)
    {
        return 0;
    }
    chance = 0.9;
    if (trace_ent != targ && random() < 0.3)
    {
        return 0;
    }
    if (random() < chance)
    {
        self.th_missile();
        SUB_AttackFinished(2 + 1 * random());
        return 1;
    }
    return 0;
};

void() ai_vermis_run = {
    if (self.enemy.health <= 0) {
        self.enemy = world;
        if (self.oldenemy.health > 0) {
            self.enemy = self.oldenemy;
            HuntTarget();
        } else {
            self.th_stand();
            return;
        }
    }
    self.show_hostile = time + 1;
    enemy_vis = visible(self.enemy);
    if (enemy_vis)
    {
        self.search_time = time + 5;
    }
    if (coop && self.search_time < time)
    {
        if (FindTarget())
        {
            return;
        }
    }
    enemy_infront = infront(self.enemy);
    enemy_range = range(self.enemy);
    enemy_yaw = vectoyaw(self.enemy.origin - self.origin);
    CheckVermisAttack();
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_touch = {
    if (other.classname == "player") {
        T_Damage(other, self, self, AS_MELEE + 2 * random(),DAMARMOR);
        self.touch = SUB_Null;
    }
};

void() vermis_wake_things = {
    self.spawnmaxs_z = self.spawnmaxs_z + self.height;
    setsize(self, '-64 -64 -512', self.spawnmaxs);
    self.touch = vermis_touch;
};

void() vermis_coil1 = [2, vermis_coil1]
{
    self.height = 8;
};

void() vermis_wake1 = [2, vermis_wake2]
{
    ai_vermis_face();
    self.spawnmaxs = '64 64 -64';
    setsize(self, '-64 -64 -512', self.spawnmaxs);
    self.touch = vermis_touch;
};

void() vermis_wake2 = [3, vermis_wake3]
{
    ai_vermis_face();
    vermis_wake_things();
    self.coiled = 0;
};

void() vermis_wake3 = [4, vermis_wake4]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake4 = [5, vermis_wake5]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake5 = [6, vermis_wake6]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake6 = [7, vermis_wake7]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake7 = [8, vermis_wake8]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake8 = [9, vermis_wake9]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake9 = [10, vermis_wake10]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake10 = [11, vermis_wake11]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake11 = [12, vermis_wake12]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake12 = [13, vermis_wake13]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake13 = [14, vermis_wake14]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake14 = [15, vermis_wake15]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake15 = [16, vermis_wake16]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake16 = [17, vermis_wake17]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake17 = [18, vermis_wake18]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake18 = [19, vermis_wake19]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake19 = [20, vermis_wake20]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake20 = [21, vermis_wake21]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake21 = [22, vermis_wake22]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake22 = [23, vermis_wake23]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake23 = [24, vermis_wake24]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake24 = [25, vermis_wake25]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake25 = [26, vermis_wake26]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake26 = [27, vermis_wake27]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake27 = [28, vermis_wake28]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake28 = [29, vermis_wake29]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake29 = [30, vermis_wake30]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake30 = [31, vermis_wake31]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake31 = [32, vermis_wake32]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake32 = [33, vermis_wake33]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake33 = [34, vermis_wake34]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake34 = [35, vermis_wake35]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake35 = [36, vermis_wake36]
{
    ai_vermis_face();
    vermis_wake_things();
};

void() vermis_wake36 = [37, vermis_wake37]
{
    ai_vermis_face();
};

void() vermis_wake37 = [38, vermis_wake38]
{
    ai_vermis_face();
};

void() vermis_wake38 = [39, vermis_wake39]
{
    ai_vermis_face();
};

void() vermis_wake39 = [40, vermis_wake40]
{
    ai_vermis_face();
};

void() vermis_wake40 = [41, vermis_wake41]
{
    ai_vermis_face();
};

void() vermis_wake41 = [42, vermis_wake42]
{
    ai_vermis_face();
};

void() vermis_wake42 = [43, vermis_wake43]
{
    ai_vermis_face();
};

void() vermis_wake43 = [44, vermis_wake44]
{
    ai_vermis_face();
};

void() vermis_wake44 = [45, vermis_wake45]
{
    ai_vermis_face();
};

void() vermis_wake45 = [46, vermis_wake46]
{
    ai_vermis_face();
};

void() vermis_wake46 = [47, vermis_wake47]
{
    ai_vermis_face();
};

void() vermis_wake47 = [48, vermis_wake48]
{
    ai_vermis_face();
};

void() vermis_wake48 = [49, vermis_wake49]
{
    ai_vermis_face();
};

void() vermis_wake49 = [50, vermis_wake50]
{
    self.th_stand = vermis_idle1;
};

void() vermis_wake50 = [51, vermis_run1]
{
    ai_vermis_face();
    self.touch = SUB_Null;
};

void() vermis_trans1 = [212, vermis_trans2]
{
    ai_vermis_face();
};

void() vermis_trans2 = [213, vermis_trans3]
{
    ai_vermis_face();
};

void() vermis_trans3 = [214, vermis_trans4]
{
    ai_vermis_face();
};

void() vermis_trans4 = [215, vermis_trans5]
{
    ai_vermis_face();
};

void() vermis_trans5 = [216, vermis_trans21]
{
    ai_vermis_face();
};

void() vermis_trans21 = [22, vermis_trans22]
{
    ai_vermis_face();
};

void() vermis_trans22 = [23, vermis_trans23]
{
    ai_vermis_face();
};

void() vermis_trans23 = [24, vermis_trans24]
{
    ai_vermis_face();
};

void() vermis_trans24 = [25, vermis_trans25]
{
    ai_vermis_face();
};

void() vermis_trans25 = [26, vermis_trans26]
{
    ai_vermis_face();
};

void() vermis_trans26 = [27, vermis_trans27]
{
    ai_vermis_face();
};

void() vermis_trans27 = [28, vermis_trans28]
{
    ai_vermis_face();
};

void() vermis_trans28 = [29, vermis_trans29]
{
    ai_vermis_face();
};

void() vermis_trans29 = [30, vermis_trans30]
{
    ai_vermis_face();
};

void() vermis_trans30 = [31, vermis_trans31]
{
    ai_vermis_face();
};

void() vermis_trans31 = [32, vermis_trans32]
{
    ai_vermis_face();
};

void() vermis_trans32 = [33, vermis_trans33]
{
    ai_vermis_face();
};

void() vermis_trans33 = [34, vermis_trans34]
{
    ai_vermis_face();
};

void() vermis_trans34 = [35, vermis_trans35]
{
    ai_vermis_face();
};

void() vermis_trans35 = [36, vermis_trans36]
{
    ai_vermis_face();
};

void() vermis_trans36 = [37, vermis_trans37]
{
    ai_vermis_face();
};

void() vermis_trans37 = [38, vermis_trans38]
{
    ai_vermis_face();
};

void() vermis_trans38 = [39, vermis_trans39]
{
    ai_vermis_face();
};

void() vermis_trans39 = [40, vermis_trans40]
{
    ai_vermis_face();
};

void() vermis_trans40 = [41, vermis_trans41]
{
    ai_vermis_face();
};

void() vermis_trans41 = [42, vermis_trans42]
{
    ai_vermis_face();
};

void() vermis_trans42 = [43, vermis_trans43]
{
    ai_vermis_face();
};

void() vermis_trans43 = [44, vermis_trans44]
{
    ai_vermis_face();
};

void() vermis_trans44 = [45, vermis_trans45]
{
    ai_vermis_face();
};

void() vermis_trans45 = [46, vermis_trans46]
{
    ai_vermis_face();
};

void() vermis_trans46 = [47, vermis_trans47]
{
    ai_vermis_face();
};

void() vermis_trans47 = [48, vermis_trans48]
{
    ai_vermis_face();
};

void() vermis_trans48 = [49, vermis_trans49]
{
    ai_vermis_face();
};

void() vermis_trans49 = [50, vermis_trans50]
{
    ai_vermis_face();
    self.th_stand = vermis_idle1;
};

void() vermis_trans50 = [51, vermis_run1]
{
    ai_vermis_face();
};

void() vermis_idle1 = [52, vermis_idle2]
{
    ai_vermis_stand();
};

void() vermis_idle2 = [53, vermis_idle3]
{
    ai_vermis_stand();
};

void() vermis_idle3 = [54, vermis_idle4]
{
    ai_vermis_stand();
};

void() vermis_idle4 = [55, vermis_idle5]
{
    ai_vermis_stand();
};

void() vermis_idle5 = [56, vermis_idle6]
{
    ai_vermis_stand();
};

void() vermis_idle6 = [57, vermis_idle7]
{
    ai_vermis_stand();
};

void() vermis_idle7 = [58, vermis_idle8]
{
    ai_vermis_stand();
};

void() vermis_idle8 = [59, vermis_idle9]
{
    ai_vermis_stand();
};

void() vermis_idle9 = [60, vermis_idle10]
{
    ai_vermis_stand();
};

void() vermis_idle10 = [61, vermis_idle11]
{
    ai_vermis_stand();
};

void() vermis_idle11 = [62, vermis_idle12]
{
    ai_vermis_stand();
};

void() vermis_idle12 = [63, vermis_idle13]
{
    ai_vermis_stand();
};

void() vermis_idle13 = [64, vermis_idle14]
{
    ai_vermis_stand();
};

void() vermis_idle14 = [65, vermis_idle15]
{
    ai_vermis_stand();
};

void() vermis_idle15 = [66, vermis_idle16]
{
    ai_vermis_stand();
};

void() vermis_idle16 = [67, vermis_idle17]
{
    ai_vermis_stand();
};

void() vermis_idle17 = [68, vermis_idle18]
{
    ai_vermis_stand();
};

void() vermis_idle18 = [69, vermis_idle19]
{
    ai_vermis_stand();
};

void() vermis_idle19 = [70, vermis_idle20]
{
    ai_vermis_stand();
};

void() vermis_idle20 = [71, vermis_idle21]
{
    ai_vermis_stand();
};

void() vermis_idle21 = [70, vermis_idle22]
{
    ai_vermis_stand();
};

void() vermis_idle22 = [69, vermis_idle23]
{
    ai_vermis_stand();
};

void() vermis_idle23 = [68, vermis_idle24]
{
    ai_vermis_stand();
};

void() vermis_idle24 = [67, vermis_idle25]
{
    ai_vermis_stand();
};

void() vermis_idle25 = [66, vermis_idle26]
{
    ai_vermis_stand();
};

void() vermis_idle26 = [65, vermis_idle27]
{
    ai_vermis_stand();
};

void() vermis_idle27 = [64, vermis_idle28]
{
    ai_vermis_stand();
};

void() vermis_idle28 = [63, vermis_idle29]
{
    ai_vermis_stand();
};

void() vermis_idle29 = [62, vermis_idle30]
{
    ai_vermis_stand();
};

void() vermis_idle30 = [61, vermis_idle31]
{
    ai_vermis_stand();
};

void() vermis_idle31 = [60, vermis_idle32]
{
    ai_vermis_stand();
};

void() vermis_idle32 = [59, vermis_idle33]
{
    ai_vermis_stand();
};

void() vermis_idle33 = [58, vermis_idle34]
{
    ai_vermis_stand();
};

void() vermis_idle34 = [57, vermis_idle35]
{
    ai_vermis_stand();
};

void() vermis_idle35 = [56, vermis_idle36]
{
    ai_vermis_stand();
};

void() vermis_idle36 = [55, vermis_idle37]
{
    ai_vermis_stand();
};

void() vermis_idle37 = [54, vermis_idle38]
{
    ai_vermis_stand();
};

void() vermis_idle38 = [53, vermis_idle1]
{
    ai_vermis_stand();
};

void() vermis_run1 = [92, vermis_run2]
{
    ai_vermis_run();
};

void() vermis_run2 = [93, vermis_run3]
{
    ai_vermis_run();
};

void() vermis_run3 = [94, vermis_run4]
{
    ai_vermis_run();
};

void() vermis_run4 = [95, vermis_run5]
{
    ai_vermis_run();
};

void() vermis_run5 = [96, vermis_run6]
{
    ai_vermis_run();
};

void() vermis_run6 = [97, vermis_run7]
{
    ai_vermis_run();
};

void() vermis_run7 = [98, vermis_run8]
{
    ai_vermis_run();
};

void() vermis_run8 = [99, vermis_run9]
{
    ai_vermis_run();
};

void() vermis_run9 = [100, vermis_run10]
{
    ai_vermis_run();
};

void() vermis_run10 = [101, vermis_run11]
{
    ai_vermis_run();
};

void() vermis_run11 = [102, vermis_run12]
{
    ai_vermis_run();
};

void() vermis_run12 = [103, vermis_run13]
{
    ai_vermis_run();
};

void() vermis_run13 = [104, vermis_run14]
{
    ai_vermis_run();
};

void() vermis_run14 = [105, vermis_run15]
{
    ai_vermis_run();
};

void() vermis_run15 = [106, vermis_run16]
{
    ai_vermis_run();
};

void() vermis_run16 = [107, vermis_run17]
{
    ai_vermis_run();
};

void() vermis_run17 = [108, vermis_run18]
{
    ai_vermis_run();
};

void() vermis_run18 = [109, vermis_run19]
{
    ai_vermis_run();
};

void() vermis_run19 = [110, vermis_run20]
{
    ai_vermis_run();
};

void() vermis_run20 = [111, vermis_run21]
{
    ai_vermis_run();
};

void() vermis_run21 = [112, vermis_run22]
{
    ai_vermis_run();
};

void() vermis_run22 = [113, vermis_run23]
{
    ai_vermis_run();
};

void() vermis_run23 = [114, vermis_run24]
{
    ai_vermis_run();
};

void() vermis_run24 = [115, vermis_run25]
{
    ai_vermis_run();
};

void() vermis_run25 = [116, vermis_run26]
{
    ai_vermis_run();
};

void() vermis_run26 = [117, vermis_run27]
{
    ai_vermis_run();
};

void() vermis_run27 = [118, vermis_run28]
{
    ai_vermis_run();
};

void() vermis_run28 = [119, vermis_run29]
{
    ai_vermis_run();
};

void() vermis_run29 = [120, vermis_run30]
{
    ai_vermis_run();
};

void() vermis_run30 = [121, vermis_run1]
{
    ai_vermis_run();
};

void() vermis_spore_explode = 
{
    T_RadiusDamage(self, self.owner, 55, other,DAMAGEALL);
    if (other)
    {
        T_Damage(other, self, self.owner, 24 + 2 * random(),DAMARMOR);
    }
    sound(self, CHAN_VOICE, "vermis/pop.wav", 0.6, 1.5);
    WriteByte(0, SVC_TEMPENTITY);
    WriteByte(0, TE_TAREXPLOSION);
    WriteCoord(0, self.origin_x);
    WriteCoord(0, self.origin_y);
    WriteCoord(0, self.origin_z);
    self.velocity = VEC_ORIGIN;
    self.touch = SUB_Null;
    SUB_Remove();
};

void() vermis_spore_touch = 
{
    local float rndsnd;
    if (other == self.owner)
    {
        return;
    }
    if (other.takedamage == DAMAGE_AIM)
    {
        vermis_spore_explode();
        return;
    }
    rndsnd = random();
    if (rndsnd < 0.33)
    {
        sound(self, CHAN_VOICE, "vermis/splash1.wav", 1, 1.7);
    }
    else
    {
        if (rndsnd < 0.66)
        {
            sound(self, CHAN_VOICE, "vermis/splash2.wav", 1, 1.7);
        }
        else
        {
            sound(self, CHAN_VOICE, "vermis/splash3.wav", 1, 1.7);
        }
    }
    if (self.velocity == VEC_ORIGIN)
    {
        self.avelocity = VEC_ORIGIN;
    }
    if (self.lip == 0)
    {
        self.ltime = time + 0.3 + 0.4 * random();
        self.movetype = MOVETYPE_BOUNCE;
    }
    self.lip = 1;
};

void() vermis_spore_think = 
{
    if (self.ltime < time)
    {
        vermis_spore_explode();
    }
    self.nextthink = time + 0.1;
    self.think = vermis_spore_think;
};

void(float xoff, float yoff, float zoff) vermis_shoot = 
{
    local entity missile;
    local vector org;
    local float t;
    local vector d;
    local vector vec;
    local float dist;
    local float numspores;
    if (visible(self.enemy))
    {
        dist = vlen(self.enemy.origin - self.origin);
    }
    else
    {
        dist = vlen(self.oldenemyorigin - self.origin);
    }
    if (dist < 450)
    {
        numspores = 5;
    }
    else
    {
        if (dist < 800)
        {
            numspores = 8;
        }
        else
        {
            numspores = TE_BEAM;
        }
    }
    if (numspores == 0)
    {
        numspores = 1;
    }
    self.effects = self.effects | EF_MUZZLEFLASH;
    while (numspores > 0)
    {
        missile = spawn();
        missile.owner = self;
        missile.movetype = MOVETYPE_FLYMISSILE;
        missile.solid = SOLID_BBOX;
        makevectors(self.angles);
        org = self.origin + v_right * xoff + v_forward * yoff + v_up * zoff;
        if (visible(self.enemy))
        {
            t = vlen(self.enemy.origin - org) / 1800;
            vec = self.enemy.velocity;
            vec_z = 0;
            d = self.enemy.origin + t * vec;
            vec = normalize(d - org);
            missile.velocity = vec;
        }
        else
        {
            missile.velocity = normalize(self.oldenemyorigin - org);
        }
        missile.velocity = missile.velocity * 600 + (400 * random() - 200) * v_right + (300 * random() - 150) * v_up;
        missile.avelocity = '297 324 333';
        missile.angles = vectoangles(missile.velocity);
        missile.touch = vermis_spore_touch;
        missile.nextthink = time + 0.1;
        missile.think = vermis_spore_think;
        missile.ltime = time + 2;
        setmodel(missile, "progs/spore.mdl");
        setsize(missile, VEC_ORIGIN, VEC_ORIGIN);
        setorigin(missile, org);
        numspores = numspores - 1;
    }
};

void() vermis_shoot1 = [122, vermis_shoot2]
{
    ai_vermis_face();
    ai_shotcheck();
    sound(self, CHAN_WEAPON, "vermis/spore.wav", 1, 0.5);
};

void() vermis_shoot2 = [123, vermis_shoot3]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot3 = [124, vermis_shoot4]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot4 = [125, vermis_shoot5]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot5 = [126, vermis_shoot6]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot6 = [127, vermis_shoot7]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot7 = [128, vermis_shoot8]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot8 = [129, vermis_shoot9]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot9 = [130, vermis_shoot10]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot10 = [131, vermis_shoot11]
{
    ai_vermis_face();
    ai_shotcheck();
    vermis_shoot(0, 209, 73);
};

void() vermis_shoot11 = [132, vermis_shoot12]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot12 = [133, vermis_shoot13]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot13 = [134, vermis_shoot14]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot14 = [135, vermis_shoot15]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot15 = [136, vermis_shoot16]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot16 = [137, vermis_shoot17]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot17 = [138, vermis_shoot18]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot18 = [139, vermis_shoot19]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot19 = [140, vermis_shoot20]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_shoot20 = [141, vermis_run1]
{
    ai_vermis_face();
    ai_shotcheck();
};

void(float xoff, float yoff, float zoff) vermis_grab = 
{
    local vector org;
    local float dist;
    if (self.enemy.flags & FL_MONSTER && !(self.enemy.flags & FL_CLIENT))
    {
        T_Damage(self.enemy, self, self.owner, 15 + 5 * random(),DAMARMOR);
    }
    makevectors(self.angles);
    org = self.origin + v_right * xoff + v_forward * yoff + v_up * zoff;
    dist = vlen(self.enemy.origin - org);
    if (dist < 155)
    {
        self.enemy.vermis_grabbed = 1;
        sound(self, CHAN_VOICE, "vermis/grabbed.wav", 1, 0.5);
        T_Damage(self.enemy, self, self, SVC_FINALE + 4 * random(),DAMARMOR);
        return;
    }
    sound(self, CHAN_VOICE, "vermis/grndhit.wav", 1, 0.5);
};

void(float xoff, float yoff, float zoff, float xoff2, float yoff2, float zoff2) vermis_move = {
    local vector reqvel;
    local vector position1;
    local vector position2;
    local float dist;
    makevectors(self.angles);
    if (self.enemy.vermis_grabbed != 1)
    {
        return;
    }
    if (self.enemy.flags & FL_ONGROUND)
    {
        self.enemy.flags = self.enemy.flags - FL_ONGROUND;
    }
    if (xoff == 666)
    {
        xoff = self.enemy.origin_x;
        yoff = self.enemy.origin_y;
        zoff = self.enemy.origin_z;
    }
    position1_x = xoff;
    position1_y = yoff;
    position1_z = zoff;
    position2_x = xoff2;
    position2_y = yoff2;
    position2_z = zoff2;
    position1 = self.origin + v_right * xoff + v_forward * yoff + v_up * zoff;
    position2 = self.origin + v_right * xoff2 + v_forward * yoff2 + v_up * zoff2;
    setorigin(self.enemy, position1);
    self.enemy.oldorigin = self.origin;
    dist = vlen(position2 - position1);
    reqvel = normalize(position2 - position1) * dist * TE_LAVASPLASH;
    self.enemy.velocity = reqvel;
    self.enemy.teleport_time = time + 0.2;
};

void() dummy_think = 
{
    self.owner.vermis_grabbed = 0;
    self.owner.spawnmaster = world;
    self.owner.velocity = self.velocity;
    self.nextthink = time + 0.1;
    self.think = SUB_Remove;
};

void() vermis_launch = 
{
    local entity dummy;
    if (self.enemy.vermis_grabbed != 1)
    {
        return;
    }
    makevectors(self.angles);
    self.enemy.vermis_grabbed = 2;
    SUB_AttackFinished(1);
    if (self.enemy.health <= 0)
    {
        self.enemy.velocity = v_forward * 700 + v_up * 50;
    }
    else
    {
        dummy = spawn();
        setsize(dummy, VEC_HULL_MIN, '16 16 40');
        setorigin(dummy, self.enemy.origin);
        dummy.movetype = MOVETYPE_BOUNCE;
        dummy.velocity = v_forward * 700 + v_up * 50;
        dummy.nextthink = time + 1;
        dummy.think = dummy_think;
        dummy.owner = self.enemy;
        dummy.classname = "dummy";
        self.enemy.spawnmaster = dummy;
    }
    if (self.enemy.classname != "player")
    {
        self.enemy.velocity = v_forward * 700 + v_up * 50;
    }
    self.vermis_grabbed = time + 2;
};

void() vermis_grabmis1 = [217, vermis_grabmis2]
{
    ai_vermis_face();
};

void() vermis_grabmis2 = [217, vermis_run1]
{
    ai_vermis_face();
    SUB_AttackFinished(0.2);
};

void() vermis_grab1 = [142, vermis_grab2]
{
    self.drop = self.yaw_speed;
    self.yaw_speed = 25;
    ai_vermis_face();
    ai_shotcheck();
    sound(self, CHAN_AUTO, "vermis/grab.wav", 1, 0.5);
};

void() vermis_grab2 = [143, vermis_grab3]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_grab3 = [144, vermis_grab4]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_grab4 = [145, vermis_grab5]
{
    ai_vermis_face();
    ai_shotcheck();
};

void() vermis_grab5 = [146, vermis_grab6]
{
    ai_vermis_face();
    ai_shotcheck();
    vermis_grab(14, 174, 24);
    vermis_move(666, 0, 0, 14, 174, 24);
};

void() vermis_grab6 = [147, vermis_grab7]
{
    vermis_move(19, 208, -7, 18, 203, -14);
};

void() vermis_grab7 = [148, vermis_grab8]
{
    vermis_move(18, 203, -14, 16, 193, CONTENT_LAVA);
};

void() vermis_grab8 = [149, vermis_grab9]
{
    vermis_move(16, 193, CONTENT_LAVA, 14, 182, 11);
    if (self.enemy.vermis_grabbed != 1)
    {
        self.think = vermis_grabmis1;
    }
};

void() vermis_grab9 = [150, vermis_grab10]
{
    vermis_move(14, 182, 11, 11, 172, 34);
};

void() vermis_grab10 = [151, vermis_grab11]
{
    vermis_move(11, 172, 34, 9, 166, 59);
};

void() vermis_grab11 = [152, vermis_grab12]
{
    vermis_move(9, 166, 59, 8, 164, 92);
};

void() vermis_grab12 = [153, vermis_grab13]
{
    vermis_move(8, 164, 92, 6, 156, 136);
};

void() vermis_grab13 = [154, vermis_grab14]
{
    vermis_move(6, 156, 136, 5, 133, 187);
};

void() vermis_grab14 = [155, vermis_grab15]
{
    vermis_move(5, 133, 187, 4, 89, 236);
};

void() vermis_grab15 = [156, vermis_grab16]
{
    vermis_move(4, 89, 236, 5, SVC_TEMPENTITY, 273);
};

void() vermis_grab16 = [157, vermis_grab17]
{
    vermis_move(5, SVC_TEMPENTITY, 273, AS_MELEE, -38, 331);
};

void() vermis_grab17 = [158, vermis_grab18]
{
    vermis_move(AS_MELEE, -38, 331, AS_MELEE, -38, 331);
};

void() vermis_grab18 = [159, vermis_grab19]
{
    vermis_launch();
};

void() vermis_grab19 = [160, vermis_grab20] {};
void() vermis_grab20 = [161, vermis_run1] {};

void() vermis_melee = {
    if (self.vermis_grabbed > time)
        return;

    if (!(self.enemy.flags & FL_FLY))
        vermis_grab1();
};

void(entity inflictor, entity attacker, float damage) vermis_pain = {
    if (self.shield == 0 && self.coiled != 0) {
        self.shield = 1;
        vermis_wake1();
    }
    if (self.shield == 0 && self.coiled == 0) {
        self.shield = 1;
        vermis_trans1();
    }
    return;
};

void() vermis_death1 = [162, vermis_death2]
{
};

void() vermis_death2 = [163, vermis_death3]
{
};

void() vermis_death3 = [164, vermis_death4]
{
};

void() vermis_death4 = [165, vermis_death5]
{
};

void() vermis_death5 = [166, vermis_death6]
{
};

void() vermis_death6 = [167, vermis_death7]
{
};

void() vermis_death7 = [168, vermis_death8]
{
};

void() vermis_death8 = [169, vermis_death9]
{
};

void() vermis_death9 = [170, vermis_death10]
{
};

void() vermis_death10 = [171, vermis_death11]
{
};

void() vermis_death11 = [172, vermis_death12]
{
};

void() vermis_death12 = [173, vermis_death13]
{
};

void() vermis_death13 = [174, vermis_death14]
{
};

void() vermis_death14 = [175, vermis_death15]
{
};

void() vermis_death15 = [176, vermis_death16]
{
};

void() vermis_death16 = [177, vermis_death17]
{
};

void() vermis_death17 = [178, vermis_death18]
{
};

void() vermis_death18 = [179, vermis_death19]
{
};

void() vermis_death19 = [180, vermis_death20]
{
};

void() vermis_death20 = [181, vermis_death21]
{
};

void() vermis_death21 = [182, vermis_death22]
{
};

void() vermis_death22 = [183, vermis_death23]
{
};

void() vermis_death23 = [184, vermis_death24]
{
};

void() vermis_death24 = [185, vermis_death25]
{
};

void() vermis_death25 = [186, vermis_death26]
{
};

void() vermis_death26 = [187, vermis_death27]
{
};

void() vermis_death27 = [188, vermis_death28]
{
};

void() vermis_death28 = [189, vermis_death29]
{
};

void() vermis_death29 = [190, vermis_death30]
{
    self.movetype = MOVETYPE_NOCLIP;
};

void() vermis_death30 = [191, vermis_death31]
{
};

void() vermis_death31 = [192, vermis_death32]
{
};

void() vermis_death32 = [193, vermis_death33]
{
};

void() vermis_death33 = [194, vermis_death34]
{
};

void() vermis_death34 = [195, vermis_death35]
{
    self.velocity = '0 0 -10';
};

void() vermis_death35 = [196, vermis_death36]
{
    self.velocity = '0 0 -20';
};

void() vermis_death36 = [197, vermis_death37]
{
    self.velocity = '0 0 -40';
};

void() vermis_death37 = [198, vermis_death38]
{
    self.velocity = '0 0 -80';
};

void() vermis_death38 = [199, vermis_death39]
{
    self.velocity = '0 0 -160';
};

void() vermis_death39 = [200, vermis_death40]
{
    self.velocity = '0 0 -320';
};

void() vermis_death40 = [201, vermis_death41]
{
    self.velocity = '0 0 -400';
};

void() vermis_death41 = [202, vermis_death42]
{
    self.velocity = '0 0 -450';
};

void() vermis_death42 = [203, vermis_death43]
{
};

void() vermis_death43 = [204, vermis_death44]
{
};

void() vermis_death44 = [205, vermis_death45]
{
};

void() vermis_death45 = [206, vermis_death46]
{
};

void() vermis_death46 = [207, vermis_death47]
{
};

void() vermis_death47 = [208, vermis_death48]
{
};

void() vermis_death48 = [209, vermis_death49] {};
void() vermis_death49 = [210, vermis_death50] {};
void() vermis_death50 = [211, vermis_death51] {};
void() vermis_death51 = [210, vermis_death52] {};
void() vermis_death52 = [209, vermis_death47] {
    if (self.inv == 1)
        self.think = SUB_Remove;

    self.inv = 1;
};

void() vermis_die = {
    sound(self, CHAN_WEAPON, "vermis/death.wav", 1, 0);
    self.solid = SOLID_NOT;
    if (self.enemy)
        self.enemy.vermis_grabbed = 0;

    vermis_death1();
};

void() monster_vermis = {
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/vermis.mdl";
    precache_model("progs/vermis.mdl");
    precache_model2("progs/spore.mdl");
    precache_sound("vermis/grab.wav");
    precache_sound("vermis/death.wav");
    precache_sound("vermis/grabbed.wav");
    precache_sound("vermis/grndhit.wav");
    precache_sound("vermis/spore.wav");
    precache_sound("vermis/pop.wav");
    precache_sound("vermis/splash1.wav");
    precache_sound("vermis/splash2.wav");
    precache_sound("vermis/splash3.wav");
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_NOCLIP;
    setmodel(self, "progs/vermis.mdl");
    self.health = 3000;
    if (self.coiled != 0) {
        precache_sound("vermis/sight.wav");
        precache_sound("vermis/sight2.wav");
        self.th_stand = vermis_coil1;
        setsize(self, '-64 -64 -512', '64 64 -64');
    } else {
        precache_sound("vermis/sight2.wav");
        self.th_stand = vermis_idle1;
        setsize(self, '-64 -64 -512', '64 64 256');
    }
    self.th_run = vermis_run1;
    self.th_missile = vermis_shoot1;
    self.th_melee = vermis_melee;
    self.th_pain = vermis_pain;
    self.th_die = vermis_die;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    if (self.deathstring == "") self.deathstring = " was eradicated by a Vermis\n";

    self.classtype = CT_MONVERMIS;
    self.classgroup = CG_BOSS;
    self.classmove = FALSE;
    if (world.devdata & DEV_AMMO_N_HEALTH) sendmonsterdevdata(self);
    vermis_start();
};