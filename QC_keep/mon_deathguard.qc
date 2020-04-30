float() DGuardCheckAttack;

void() deathguard_hit = {
    if (vlen(self.enemy.origin - self.origin) > 100) {
        if (random() < 0.5) {
            sound(self, CHAN_WEAPON, "dguard/slmiss1.wav", 1, ATTN_NORM);
        } else {
            sound(self, CHAN_WEAPON, "dguard/slmiss2.wav", 1, ATTN_NORM);
        }
        return;
    }
    sound(self, CHAN_WEAPON, "dguard/slhit.wav", 1, ATTN_NORM);
    T_Damage(self.enemy, self, self, 30,DAMARMOR);

    // Check for poisonous blades!
    if (self.poisonous) PoisonDeBuff(self.enemy);
};

void() deathguard_slash1 = [79, deathguard_slash2] { sound(self, CHAN_VOICE, "dguard/slash.wav", 1, ATTN_NORM); ai_face(); ai_forward(1); };
void() deathguard_slash2 = [80, deathguard_slash3] { ai_face(); ai_forward(1); };
void() deathguard_slash3 = [81, deathguard_slash4] { ai_face(); ai_forward(1); monster_footstep(FALSE); };
void() deathguard_slash4 = [82, deathguard_slash5] { ai_face(); ai_forward(1); };
void() deathguard_slash5 = [83, deathguard_slash6] { ai_face(); ai_forward(1); };
void() deathguard_slash6 = [84, deathguard_slash7] { ai_face(); ai_forward(1); };
void() deathguard_slash7 = [85, deathguard_slash8] { ai_face(); ai_forward(1); };
void() deathguard_slash8 = [86, deathguard_slash9] { ai_face(); ai_forward(1); };
void() deathguard_slash9 = [87, deathguard_slash10] { ai_face(); ai_forward(1); monster_footstep(FALSE); };
void() deathguard_slash10 = [88, deathguard_slash11] { ai_face(); deathguard_hit(); ai_forward(1); };
void() deathguard_slash11 = [89, deathguard_slash12] { ai_face(); };
void() deathguard_slash12 = [90, deathguard_slash13] { ai_face(); };
void() deathguard_slash13 = [91, deathguard_slash14] { ai_face(); };
void() deathguard_slash14 = [92, deathguard_run1] { ai_face(); };

void(float dmgamount) dg_slice = {
    if (vlen(self.enemy.origin - self.origin) > 100)
        return;

    T_Damage(self.enemy, self, self, dmgamount,DAMARMOR);

    // Check for poisonous blades!
    if (self.poisonous) PoisonDeBuff(self.enemy);
};

void() deathguard_charge1 = [106, deathguard_charge2] {
    ai_charge(24);
    if (random() < 0.5) {
        sound(self, CHAN_VOICE, "knight/sword1.wav", 1, ATTN_NORM);
    } else {
        sound(self, CHAN_VOICE, "knight/sword2.wav", 1, ATTN_NORM);
    }
};
void() deathguard_charge2 = [107, deathguard_charge3] { ai_charge(29); };
void() deathguard_charge3 = [108, deathguard_charge4] { ai_charge(22); monster_footstep(FALSE); };
void() deathguard_charge4 = [109, deathguard_charge5] { ai_charge(20); dg_slice(1); };
void() deathguard_charge5 = [110, deathguard_charge6] { ai_charge(18); dg_slice(3); };
void() deathguard_charge6 = [111, deathguard_run1] { ai_charge(20); dg_slice(2); };

void() deathguard_slowcharge1 = [112, deathguard_slowcharge2] {
    ai_charge(TE_BEAM);
    if (random() < 0.5) {
        sound(self, CHAN_VOICE, "knight/sword1.wav", 1, ATTN_NORM);
    } else {
        sound(self, CHAN_VOICE, "knight/sword2.wav", 1, ATTN_NORM);
    }
};
void() deathguard_slowcharge2 = [113, deathguard_slowcharge3] { ai_charge(14); };
void() deathguard_slowcharge3 = [114, deathguard_slowcharge4] { ai_charge(19); };
void() deathguard_slowcharge4 = [115, deathguard_slowcharge5] { ai_charge(17); };
void() deathguard_slowcharge5 = [116, deathguard_slowcharge6] { ai_charge(12); dg_slice(2); };
void() deathguard_slowcharge6 = [117, deathguard_slowcharge7] { ai_charge(8); dg_slice(4); };
void() deathguard_slowcharge7 = [118, deathguard_slowcharge8] { ai_charge(10); dg_slice(2); monster_footstep(FALSE); };
void() deathguard_slowcharge8 = [119, deathguard_slowcharge9] { ai_charge(6);};
void() deathguard_slowcharge9 = [120, deathguard_slowcharge10] { ai_charge(3); };
void() deathguard_slowcharge10 = [121, deathguard_run1] { ai_charge(2); };

void() dg_fireball_touch = {
    local vector org;

    if (other == self.owner)
        return;

    if (pointcontents(self.origin) == CONTENT_SKY) {
        remove(self);
        return;
    }
    org = self.origin - 8 * normalize(self.velocity);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord(MSG_BROADCAST, org_x);
    WriteCoord(MSG_BROADCAST, org_y);
    WriteCoord(MSG_BROADCAST, org_z);
    sound(self, CHAN_VOICE, "dguard/hit.wav", 1, ATTN_NORM);
    T_RadiusDamage(self, self.owner, 40, other,DAMAGEALL);
    T_Damage(other, self, self.owner, 27 + 2 * random(),DAMARMOR);
    if (other.classname != "player") {
        if (other.classname == "monster_zombie" || other.classname == "monster_zombiek" || other.classname == "monster_zombiep")
            T_Damage(other, self, self.owner, 120,DAMARMOR); // gib zombies!
    }
    remove(self);
};

void() deathguard_shot = {
    local vector org;
    local vector vec;
    local vector spot1;
    local vector spot2;
    local entity targ;

    targ = self.enemy;
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline(spot1, spot2, 0, self);
    if (trace_ent != targ) {
        self.think = self.th_run;
        return;
    }
    hknight_shot(0);
    newmis.mdl = "progs/quoth2pt2full/fireball.mdl";
    org = self.origin + '0 0 24';
    setmodel(newmis, "progs/quoth2pt2full/fireball.mdl");
    setsize(newmis, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(newmis, org);
    if (visible(self.enemy)) {
        vec = self.enemy.origin - '0 0 16';
        vec = normalize(vec - org);
    } else {
        vec = self.oldenemyorigin - '0 0 16';
        vec = normalize(vec - org);
    }
    newmis.velocity = vec * 900;
    newmis.angles = vectoangles(newmis.velocity);
    newmis.nextthink = time + 5;
    newmis.think = SUB_Remove;
    newmis.touch = dg_fireball_touch;
    newmis.classname = "dguardfireball";
};

void() deathguard_fire1 = [93, deathguard_fire2] { ai_shotcheck(); ai_face(); };
void() deathguard_fire2 = [94, deathguard_fire3] { ai_shotcheck(); ai_face(); };
void() deathguard_fire3 = [95, deathguard_fire4] { ai_shotcheck(); ai_face(); };
void() deathguard_fire4 = [96, deathguard_fire5] { ai_shotcheck(); ai_face(); };
void() deathguard_fire5 = [97, deathguard_fire6] { ai_shotcheck(); ai_face(); };
void() deathguard_fire6 = [98, deathguard_fire7] { ai_shotcheck(); ai_face(); };
void() deathguard_fire7 = [99, deathguard_fire8] { ai_shotcheck(); ai_face(); };
void() deathguard_fire8 = [100, deathguard_fire9] { ai_shotcheck(); ai_face(); sound(self, CHAN_VOICE, "dknight/dserg_magic.wav", 1, ATTN_NORM); };
void() deathguard_fire9 = [101, deathguard_fire10] { ai_shotcheck(); ai_face(); deathguard_shot(); };
void() deathguard_fire10 = [102, deathguard_fire11] { ai_face(); };
void() deathguard_fire11 = [103, deathguard_fire12] { ai_face(); };
void() deathguard_fire12 = [104, deathguard_fire13] { ai_face(); };
void() deathguard_fire13 = [105, deathguard_run1] { ai_face(); };

void() deathguard_stand1 = [0, deathguard_stand2] { ai_stand(); };
void() deathguard_stand2 = [1, deathguard_stand3] { ai_stand(); };
void() deathguard_stand3 = [2, deathguard_stand4] { ai_stand(); };
void() deathguard_stand4 = [3, deathguard_stand5] { ai_stand(); };
void() deathguard_stand5 = [4, deathguard_stand6] { ai_stand(); };
void() deathguard_stand6 = [5, deathguard_stand7] { ai_stand(); };
void() deathguard_stand7 = [6, deathguard_stand8] { ai_stand(); };
void() deathguard_stand8 = [7, deathguard_stand9] { ai_stand(); };
void() deathguard_stand9 = [8, deathguard_stand1] { ai_stand(); };

void() deathguard_walk1 = [9, deathguard_walk2] { monster_idle_sound(); ai_walk(2); };
void() deathguard_walk2 = [10, deathguard_walk3] { ai_walk(5); };
void() deathguard_walk3 = [11, deathguard_walk4] { ai_walk(5); };
void() deathguard_walk4 = [12, deathguard_walk5] { ai_walk(4); };
void() deathguard_walk5 = [13, deathguard_walk6] { ai_walk(4); };
void() deathguard_walk6 = [14, deathguard_walk7] { ai_walk(2); };
void() deathguard_walk7 = [15, deathguard_walk8] { ai_walk(2); };
void() deathguard_walk8 = [16, deathguard_walk9] { ai_walk(3); };
void() deathguard_walk9 = [17, deathguard_walk10] { ai_walk(3); };
void() deathguard_walk10 = [18, deathguard_walk11] { ai_walk(4); };
void() deathguard_walk11 = [19, deathguard_walk12] { ai_walk(3); monster_footstep(FALSE); };
void() deathguard_walk12 = [20, deathguard_walk13] { ai_walk(4); };
void() deathguard_walk13 = [21, deathguard_walk14] { ai_walk(6); };
void() deathguard_walk14 = [22, deathguard_walk15] { ai_walk(2); };
void() deathguard_walk15 = [23, deathguard_walk16] { ai_walk(2); };
void() deathguard_walk16 = [24, deathguard_walk17] { ai_walk(4); };
void() deathguard_walk17 = [25, deathguard_walk18] { ai_walk(3); };
void() deathguard_walk18 = [26, deathguard_walk19] { ai_walk(3); };
void() deathguard_walk19 = [27, deathguard_walk20] { ai_walk(3); };
void() deathguard_walk20 = [28, deathguard_walk1] { ai_walk(2); monster_footstep(FALSE); };

void() deathguard_run1 = [29, deathguard_run2] { monster_idle_sound(); ai_run(24); };
void() deathguard_run2 = [30, deathguard_run3] { ai_run(29); monster_footstep(FALSE); };
void() deathguard_run3 = [31, deathguard_run4] { ai_run(22); };
void() deathguard_run4 = [32, deathguard_run5] { ai_run(20); };
void() deathguard_run5 = [33, deathguard_run6] { ai_run(18); monster_footstep(FALSE); };
void() deathguard_run6 = [34, deathguard_run7] { ai_run(29); };
void() deathguard_run7 = [35, deathguard_run8] { ai_run(25); };
void() deathguard_run8 = [36, deathguard_run1] { ai_run(17); };
void() deathguard_pain1 = [37, deathguard_pain2] { sound(self, CHAN_VOICE, "dguard/pain1.wav", 1, ATTN_NORM); };
void() deathguard_pain2 = [38, deathguard_pain3]{monster_footstep(FALSE);};
void() deathguard_pain3 = [39, deathguard_pain4]{};
void() deathguard_pain4 = [40, deathguard_pain5]{};
void() deathguard_pain5 = [41, deathguard_run1] {};

void() deathguard_die1 = [42, deathguard_die2] { ai_forward(10); sound(self, CHAN_VOICE, "dguard/death2.wav", 1, ATTN_NORM); };
void() deathguard_die2 = [43, deathguard_die3] { monster_check_gib(); ai_forward(8); };
void() deathguard_die3 = [44, deathguard_die4] { monster_check_gib(); ai_forward(7); };
void() deathguard_die4 = [45, deathguard_die5] { self.solid = SOLID_NOT; };
void() deathguard_die5 = [46, deathguard_die6] { self.nextthink = time + 0.2; };
void() deathguard_die6 = [47, deathguard_die7] { self.nextthink = time + 0.2; };
void() deathguard_die7 = [48, deathguard_die8] { self.nextthink = time + 0.2; };
void() deathguard_die8 = [49, deathguard_die9] { ai_forward(10); };
void() deathguard_die9 = [50, deathguard_die10] { ai_forward(11); };
void() deathguard_die10 = [51, deathguard_die11] {};
void() deathguard_die11 = [52, deathguard_die12] {monster_death_postcheck();};
void() deathguard_die12 = [53, deathguard_die12] { self.nextthink = time + 0.1; };

void() deathguard_dieb1 = [54, deathguard_dieb2] { sound(self, CHAN_VOICE, "dguard/death1.wav", 1, ATTN_NORM); };
void() deathguard_dieb2 = [55, deathguard_dieb3] { monster_check_gib(); };
void() deathguard_dieb3 = [56, deathguard_dieb4] { monster_check_gib(); };
void() deathguard_dieb4 = [57, deathguard_dieb5] { self.solid = SOLID_NOT;};
void() deathguard_dieb5 = [58, deathguard_dieb6] {};
void() deathguard_dieb6 = [59, deathguard_dieb7] {};
void() deathguard_dieb7 = [60, deathguard_dieb8] {};
void() deathguard_dieb8 = [61, deathguard_dieb9] {monster_death_postcheck(); };
void() deathguard_dieb9 = [62, deathguard_dieb9] { self.nextthink = time + 0.1; };

void() deathguard_die = {
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    if (!self.gibbed) {
        if (random() > 0.5) {
            deathguard_die1();
        } else {
            deathguard_dieb1();
        }
    }
};

void(entity inflictor, entity attacker, float damage) deathguard_pain = {
    if (self.pain_finished > time)
        return;

    if (random() > 0.5) {
        sound(self, CHAN_VOICE, "dguard/pain1.wav", 1, ATTN_NORM);
    } else {
        sound(self, CHAN_VOICE, "dguard/pain2.wav", 1, ATTN_NORM);
    }
    if (random() * 30 > damage)
        return;

    self.pain_finished = time + 5;
    deathguard_pain1();
};

void() deathguard_melee = { deathguard_slash1(); };

void() monster_deathguard = {
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/quoth2pt2full/dguard.mdl";
    precache_model(self.mdl);
    self.headmdl = "progs/quoth2pt2full/h_dguard.mdl";
    precache_model(self.headmdl);
    precache_model("progs/quoth2pt2full/fireball.mdl");
    self.gib1sound = GIB_IMPACT_METALA;
    self.idle_sound = "dguard/idle.wav";
    precache_sound(self.idle_sound);
    precache_sound("dguard/fire.wav");
    precache_sound("dknight/dserg_magic.wav");
    precache_sound("dguard/death1.wav");
    precache_sound("dguard/death2.wav");
    precache_sound("dguard/pain1.wav");
    precache_sound("dguard/pain2.wav");
    precache_sound("dguard/sight1.wav");
    precache_sound("dguard/sight2.wav");
    precache_sound("dguard/sight3.wav");
    precache_sound("dguard/sight4.wav");
    precache_sound("dguard/hit.wav");
    precache_sound("dguard/slash.wav");
    precache_sound("knight/sword1.wav");
    precache_sound("knight/sword2.wav");
    precache_sound("dguard/slhit.wav");
    precache_sound("dguard/slmiss1.wav");
    precache_sound("dguard/slmiss2.wav");

    self.solid = SOLID_NOT;                // No interaction with world
    self.movetype = MOVETYPE_NONE;        // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_TALL;
    if (self.health < 1) self.health = 200;
    self.gibhealth = -50;
    self.gibbed = FALSE;
    self.pain_flinch = 40;
    self.steptype = FS_TYPEMEDIUM;
    self.hittype = HIT_METAL;

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = self.resist_nails = 0;
    self.resist_rockets = self.resist_cells = 0;

    self.th_stand = deathguard_stand1;
    self.th_walk = deathguard_walk1;
    self.th_run = deathguard_run1;
    self.th_melee = deathguard_slash1;
    self.th_missile = deathguard_fire1;
    self.th_pain = deathguard_pain;
    self.th_die = deathguard_die;

    self.classtype = CT_MONDGUARD;
    self.classgroup = CG_KNIGHT;
    self.classmove = MON_MOVEWALK;
    if (self.deathstring == "")  self.deathstring =  "was slaughtered by a Death Brigade Death Guard\n";

    monster_start();
};

void() monster_death_guard = {
    self.classname = "monster_deathguard";
    monster_deathguard();
};

float() DGuardCheckAttack = {
    local vector spot1;
    local vector spot2;
    local entity targ;
    local float chance;
    local float dgdist;
    targ = self.enemy;
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline(spot1, spot2, 0, self);
    if (trace_ent != targ)
    {
        return 0;
    }
    if (trace_inopen && trace_inwater)
    {
        return 0;
    }
    dgdist = vlen(self.enemy.origin - self.origin);
    if (dgdist < 100)
    {
        self.th_melee();
        return 1;
    }
    else
    {
        if (dgdist < 190 && dgdist > 140)
        {
            deathguard_charge1();
            return 1;
        }
    }
    if (time < self.attack_finished)
    {
        return 0;
    }
    if (enemy_range == RANGE_FAR)
    {
        return 0;
    }
    if (dgdist < 300)
    {
        return 0;
    }
    else
    {
        if (dgdist >= 300)
        {
            chance = 0.5;
        }
    }
    if (random() < chance)
    {
        self.th_missile();
        SUB_AttackFinished(1.4 * random() + 0.8);
        return 1;
    }
    return 0;
};

