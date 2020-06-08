void() gug_shake_think = {
    self.nextthink = time + 0.010000;
    self.think = gug_shake_think;
    self.spawnmaster.punchangle_x = (time - self.ltime) * random() * 10;
    self.spawnmaster.punchangle_y = (time - self.ltime) * random() * 10;
    self.spawnmaster.punchangle_z = (time - self.ltime) * random() * 10;
    if (self.ltime < time)
        SUB_Remove();
};

void(float stime, entity eshake) gug_screen_shake = {
    local entity qent;
    qent = spawn();
    qent.owner = self;
    qent.movetype = MOVETYPE_NONE;
    qent.solid = SOLID_NOT;
    qent.spawnmaster = eshake;
    qent.nextthink = time + 0.010000;
    qent.think = gug_shake_think;
    qent.ltime = time + stime;
    setsize(qent, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(qent, self.origin);
};

float(float num) mrandom = { return 0.5 * num - num * random(); };

float() CheckGugAttack = {
    local vector spot1;
    local vector spot2;
    local entity targ;
    local float chance;
    local float dist;

    if (!visible(self.enemy) && self.carry_time > 8 && self.t_width < time) {
        self.th_novis();
        return 1;
    }
    dist = vlen(self.enemy.origin - self.origin);
    targ = self.enemy;
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline(spot1, spot2, 0, self);
    if (trace_ent != targ)
        return 0;

    if (trace_inopen && trace_inwater)
        return 0;

    if (dist < 170) {
        self.th_melee();
        return 1;
    }

    if (time < self.attack_finished)
        return 0;

    if (dist > 1200)
        return 0;

    if (dist >= 600) {
        chance = 0.9;
        self.attack_finished = 0;
    } else {
        if (dist >= 400) {
            chance = 0.8;
        } else {
            if (dist >= 250) {
                chance = 0.8;
            } else {
                if (dist >= 170) {
                    chance = 0.2;
                } else {
                    chance = 0;
                }
            }
        }
    }
    if (random() < chance) {
        self.th_missile();
        SUB_AttackFinished(2.3 + 0.3 * random());
        return 1;
    }
    return 0;
};

void(float dist) ai_gug_run = {
    if (!visible(self.enemy))
        self.carry_time = self.carry_time + 0.1;

    if (visible(self.enemy))
        self.carry_time = 0;

    movedist = dist;
    if (self.enemy.health <= 0) {
        self.enemy = world;
        if (self.oldenemy.health > 0) {
            self.enemy = self.oldenemy;
            HuntTarget();
        } else {
            if (self.movetarget) {
                self.th_walk();
            } else {
                self.th_stand();
            }
            return;
        }
    }
    self.show_hostile = time + 1;
    enemy_vis = visible(self.enemy);
    if (enemy_vis)
        self.search_time = time + 5;

    if (coop && self.search_time < time) {
        if (FindTarget())
            return;
    }
    enemy_infront = infront(self.enemy);
    enemy_range = range(self.enemy);
    enemy_yaw = vectoyaw(self.enemy.origin - self.origin);
    if (self.attack_state == AS_MISSILE) {
        ai_run_missile();
        return;
    }
    if (self.attack_state == AS_MELEE) {
        ai_run_melee();
        return;
    }
    if (CheckGugAttack())
        return;

    ai_shotcheck();
    movetogoal(dist);
};

void() gug_stand1 = [2, gug_stand2] { ai_stand(); };
void() gug_stand2 = [3, gug_stand3] { ai_stand(); };
void() gug_stand3 = [4, gug_stand4] { ai_stand(); };
void() gug_stand4 = [5, gug_stand5] { ai_stand(); };
void() gug_stand5 = [6, gug_stand6]{ ai_stand(); };
void() gug_stand6 = [7, gug_stand7]{ai_stand();};
void() gug_stand7 = [8, gug_stand8]{ai_stand();};
void() gug_stand8 = [9, gug_stand9]{ai_stand();};
void() gug_stand9 = [10, gug_stand10]{ai_stand();};
void() gug_stand10 = [11, gug_stand11]{ai_stand();};
void() gug_stand11 = [12, gug_stand12]{ai_stand();};
void() gug_stand12 = [13, gug_stand13]{ai_stand();};
void() gug_stand13 = [14, gug_stand14]{ai_stand();};
void() gug_stand14 = [15, gug_stand15]{ai_stand();};
void() gug_stand15 = [16, gug_stand16]{ai_stand();};
void() gug_stand16 = [17, gug_stand17]{ai_stand();};
void() gug_stand17 = [18, gug_stand18]{ai_stand();};
void() gug_stand18 = [19, gug_stand19]{ai_stand();};
void() gug_stand19 = [20, gug_stand20]{ai_stand();};
void() gug_stand20 = [21, gug_choosestand]{ai_stand();};
void() gug_stand21 = [22, gug_stand22]{ai_stand();};
void() gug_stand22 = [23, gug_stand23]{ai_stand();};
void() gug_stand23 = [24, gug_stand24]{ai_stand();};
void() gug_stand24 = [25, gug_stand25]{ai_stand();};
void() gug_stand25 = [26, gug_stand26]{ai_stand();};
void() gug_stand26 = [27, gug_stand27]{ai_stand();};
void() gug_stand27 = [28, gug_stand28]{ai_stand();};
void() gug_stand28 = [29, gug_stand29]{ai_stand();};
void() gug_stand29 = [30, gug_stand30]{ai_stand();};
void() gug_stand30 = [31, gug_stand31]{ai_stand();};
void() gug_stand31 = [32, gug_stand32]{ai_stand();};
void() gug_stand32 = [33, gug_stand33]{ai_stand();};
void() gug_stand33 = [34, gug_stand34]{ai_stand();};
void() gug_stand34 = [35, gug_stand35]{ ai_stand();};
void() gug_stand35 = [36, gug_stand36]{ai_stand();};
void() gug_stand36 = [37, gug_stand37]{ai_stand();};
void() gug_stand37 = [38, gug_stand38]{ai_stand();};
void() gug_stand38 = [39, gug_stand39]{ai_stand();};
void() gug_stand39 = [40, gug_stand40]{ai_stand();};
void() gug_stand40 = [41, gug_choosestand]{ai_stand();};
void() gug_stand41 = [42, gug_stand42]{ai_stand();};
void() gug_stand42 = [43, gug_stand43]{ai_stand();};
void() gug_stand43 = [44, gug_stand44]{ai_stand();};
void() gug_stand44 = [45, gug_stand45]{ai_stand();};
void() gug_stand45 = [46, gug_stand46]{ai_stand();};
void() gug_stand46 = [47, gug_stand47]{ai_stand();};
void() gug_stand47 = [48, gug_stand48]{ai_stand();};
void() gug_stand48 = [49, gug_stand49]{ai_stand();};
void() gug_stand49 = [50, gug_stand50]{ai_stand();};
void() gug_stand50 = [51, gug_stand51]{ai_stand();};
void() gug_stand51 = [52, gug_stand52]{ai_stand();};
void() gug_stand52 = [53, gug_stand53]{ai_stand();};
void() gug_stand53 = [54, gug_stand54]{ai_stand();};
void() gug_stand54 = [55, gug_stand55]{ai_stand();};
void() gug_stand55 = [56, gug_stand56]{ai_stand();};
void() gug_stand56 = [57, gug_stand57]{ai_stand();};
void() gug_stand57 = [58, gug_stand58]{ai_stand();};
void() gug_stand58 = [59, gug_stand59]{ai_stand();};
void() gug_stand59 = [60, gug_stand60]{ai_stand();};
void() gug_stand60 = [61, gug_choosestand]{ai_stand();};
void() gug_stand61 = [62, gug_stand62]{ai_stand();};
void() gug_stand62 = [63, gug_stand63]{ai_stand();};
void() gug_stand63 = [64, gug_stand64]{ai_stand();};
void() gug_stand64 = [65, gug_stand65]{ai_stand();};
void() gug_stand65 = [66, gug_stand66]{ai_stand();};
void() gug_stand66 = [67, gug_stand67]{ai_stand();};
void() gug_stand67 = [68, gug_stand68]{ai_stand();};
void() gug_stand68 = [69, gug_stand69]{ai_stand();};
void() gug_stand69 = [70, gug_stand70]{ai_stand();};
void() gug_stand70 = [71, gug_stand71]{ai_stand();};
void() gug_stand71 = [72, gug_stand72]{ai_stand();};
void() gug_stand72 = [73, gug_stand73]{ai_stand();};
void() gug_stand73 = [74, gug_stand74]{ai_stand();};
void() gug_stand74 = [75, gug_stand75]{ai_stand();};
void() gug_stand75 = [76, gug_stand76]{ai_stand();};
void() gug_stand76 = [77, gug_stand77]{ai_stand();};
void() gug_stand77 = [78, gug_stand78]{ai_stand();};
void() gug_stand78 = [79, gug_stand79]{ai_stand();};
void() gug_stand79 = [80, gug_stand80]{ai_stand();};
void() gug_stand80 = [81, gug_choosestand]{ai_stand();};

void() gug_choosestand = {
    local float rnd;
    if (random() < 0.3)
        sound(self, CHAN_FEET, self.idle_sound, 1, 0.7);

    rnd = random();
    if (rnd < 0.25) {
        gug_stand1();
    } else {
        if (rnd < 0.5) {
            gug_stand21();
        } else {
            if (rnd < 0.75) {
                gug_stand41();
            } else {
                gug_stand61();
            }
        }
    }
};

void() gug_step_sound = {
    local float rnd;
    local entity head;

    rnd = random();
    if (rnd < 0.25) {
        sound(self, CHAN_FEET, "gug/step1.wav", 1, 0.7);
    } else {
        if (rnd < 0.5) {
            sound(self, CHAN_FEET, "gug/step2.wav", 1, 0.7);
        } else {
            if (rnd < 0.75) {
                sound(self, CHAN_FEET, "gug/step3.wav", 1, 0.7);
            } else {
                sound(self, CHAN_FEET, "gug/step4.wav", 1, 0.7);
            }
        }
    }
    head = find(self.enemy, classname, "player");
    if (head.classname == "player" && vlen(head.origin - self.origin) < 150)
        gug_screen_shake(0.2, head);
};

void() gug_walk1 = [82, gug_walk2] { ai_walk(8); gug_step_sound(); };
void() gug_walk2 = [83, gug_walk3] { ai_walk(8); };
void() gug_walk3 = [84, gug_walk4] { ai_walk(7); };
void() gug_walk4 = [85, gug_walk5] { ai_walk(6); };
void() gug_walk5 = [86, gug_walk6] { ai_walk(5); };
void() gug_walk6 = [87, gug_walk7] { ai_walk(4); };
void() gug_walk7 = [88, gug_walk8] { ai_walk(3); };
void() gug_walk8 = [89, gug_walk9] { ai_walk(2); };
void() gug_walk9 = [90, gug_walk10] { ai_walk(8); gug_step_sound(); };
void() gug_walk10 = [91, gug_walk11] { ai_walk(8); };
void() gug_walk11 = [92, gug_walk12] { ai_walk(7); };
void() gug_walk12 = [93, gug_walk13] { ai_walk(6); };
void() gug_walk13 = [94, gug_walk14] { ai_walk(5); };
void() gug_walk14 = [95, gug_walk15] { ai_walk(4); };
void() gug_walk15 = [96, gug_walk16] { ai_walk(3); };
void() gug_walk16 = [97, gug_walk1] { ai_walk(2); };

void() gug_run1 = [98, gug_run2] { ai_gug_run(22); gug_step_sound(); };
void() gug_run2 = [99, gug_run3] { ai_gug_run(20); };
void() gug_run3 = [100, gug_run4] { ai_gug_run(18); };
void() gug_run4 = [101, gug_run5] { ai_gug_run(16); };
void() gug_run5 = [102, gug_run6] { ai_gug_run(22); gug_step_sound(); };
void() gug_run6 = [103, gug_run7] { ai_gug_run(20); };
void() gug_run7 = [104, gug_run8] { ai_gug_run(18); };
void() gug_run8 = [105, gug_run1] { ai_gug_run(16); };

void(float dist, float yawspd) ai_gug_swing = {
    self.yaw_speed = yawspd;
    ai_face();
    movetogoal(dist);
    self.yaw_speed = 15;
};

void() gug_dummy_think = {
    self.owner.gug_slapped = 0;
    self.owner.spawnmaster = world;
    self.owner.velocity = self.velocity;
    self.nextthink = time + 0.1;
    self.think = SUB_Remove;
};

void(float dist, float power, float clawdmg, float stime) gug_hit = {
    local float edist;
    local entity head;
    local vector vec;
    local vector vec2;
    local vector vecA;
    local vector vecB;
    local entity dummy;

    head = findradius(self.origin, dist);
    while (head) {
        if (visible(head) && (head.flags & FL_CLIENT || (head.flags & FL_MONSTER)) && head != self && head.health > 0) {
            makevectors(self.angles);
            vec = v_forward * 100;
            vec2 = head.origin - self.origin;
            vec2 = normalize(vec2);
            vec2 = vec2 * 100;
            vecA = self.origin + vec;
            vecB = self.origin + vec2;
            edist = vlen(vecA - vecB);
            if (edist < 100) {
                vec = head.origin - self.origin;
                vec = normalize(vec);
                vec = vec * power;
                vec_z = 330;
                if (head.classname == "monster_shambler") {
                    vec = vec * 0.3;
                } else {
                    if (head.classname == "monster_drole") {
                        vec = vec * 0.4;
                    } else {
                        if (head.classname == "monster_shalrath") {
                            vec = vec * 0.7;
                        } else {
                            if (head.classname == "monster_gug") {
                                vec = vec * 0.1;
                            } else {
                                if (head.classname == "monster_cyaegha" || head.classname == "monster_vermis" || head.classname == "monster_edie") {
                                    vec = vec * 0;
                                }
                            }
                        }
                    }
                }
                if (head == self.enemy) {
                    T_Damage(head, self, self, clawdmg,DAMARMOR);
                } else {
                    T_Damage(head, self, self, clawdmg * 0.25,DAMARMOR);
                }
                if (self.gug_slapped == 0) {
                    sound(self, CHAN_WEAPON, "shambler/smack.wav", 1, ATTN_NORM);
                    self.gug_slapped = 1;
                }
                if (head.classname == "player") {
                    head.gug_slapped = 1;
                    if (head.health <= 0) {
                        head.velocity = head.velocity + vec;
                    } else {
                        dummy = spawn();
                        setsize(dummy, VEC_HULL_MIN, '16 16 40');
                        setorigin(dummy, head.origin + '0 0 1');
                        dummy.movetype = MOVETYPE_TOSS;
                        dummy.nextthink = time + 0.1;
                        dummy.think = gug_dummy_think;
                        dummy.touch = gug_dummy_think;
                        dummy.owner = head;
                        dummy.classname = "dummy";
                        head.spawnmaster = dummy;
                        if (dummy.flags & FL_ONGROUND)
                            dummy.flags = dummy.flags - FL_ONGROUND;

                        dummy.velocity = head.velocity * 0.5 + vec;
                        if (vlen(dummy.velocity) > 600) {
                            dummy.velocity = normalize(dummy.velocity);
                            dummy.velocity = dummy.velocity * 600;
                        }
                        if (head.velocity_z > 400)
                            dummy.velocity_z = 400;
                    }
                } else {
                    if (head.flags & FL_ONGROUND)
                        head.flags = head.flags - FL_ONGROUND;

                    head.velocity = head.velocity + vec * 1;
                }
            }
        }
        head = head.chain;
    }
    self.gug_slapped = 0;
};

void() gug_swingl1 = [106, gug_swingl2] { ai_gug_swing(22, 25); gug_step_sound(); };
void() gug_swingl2 = [107, gug_swingl3] { ai_gug_swing(20, 26); };
void() gug_swingl3 = [108, gug_swingl4] { ai_gug_swing(18, 25); sound(self, CHAN_WEAPON, "gug/throw.wav", 1, ATTN_NORM); };
void() gug_swingl4 = [109, gug_swingl5] { ai_gug_swing(16, 25); };
void() gug_swingl5 = [110, gug_swingl6] { ai_gug_swing(18, 24); gug_step_sound(); };
void() gug_swingl6 = [111, gug_swingl7] { ai_gug_swing(16, 24); };
void() gug_swingl7 = [112, gug_swingl8] { ai_gug_swing(14, 25); };
void() gug_swingl8 = [113, gug_swingl9] { ai_gug_swing(10, 25); gug_hit(150, 300, 35, 0.6); };
void() gug_swingl9 = [114, gug_swingl10] { ai_gug_swing(6, 10); };
void() gug_swingl10 = [115, gug_swingl11] { ai_gug_swing(2, 11); };
void() gug_swingl11 = [116, gug_swingl12] { ai_gug_swing(1, 10); };
void() gug_swingl12 = [117, gug_swingl13] { ai_gug_swing(1, 12); };
void() gug_swingl13 = [118, gug_swingl14] {
    local float dist;

    ai_gug_swing(1, 11);
    dist = vlen(self.enemy.origin - self.origin);
    if (dist > 170)
        self.think = gug_run1;
};
void() gug_swingl14 = [119, gug_swingl15] { ai_gug_swing(1, 14); sound(self, CHAN_BODY, "gug/throw.wav", 1, ATTN_NORM); };
void() gug_swingl15 = [120, gug_swingl16] { ai_gug_swing(1, 25); };
void() gug_swingl16 = [121, gug_swingl17] { ai_gug_swing(2, 27); };
void() gug_swingl17 = [122, gug_swingl18] { ai_gug_swing(22, 28); gug_step_sound(); };
void() gug_swingl18 = [123, gug_swingl19] { ai_gug_swing(20, 27); gug_hit(150, 500, 45, 0.9); };
void() gug_swingl19 = [124, gug_swingl20] { ai_gug_swing(18, 26); };
void() gug_swingl20 = [125, gug_swingl21] { ai_gug_swing(16, 22); };
void() gug_swingl21 = [126, gug_swingl22] { ai_gug_swing(22, 21); gug_step_sound(); };
void() gug_swingl22 = [127, gug_swingl23] { ai_gug_swing(20, 21); };
void() gug_swingl23 = [128, gug_run1] { ai_gug_swing(18, 20); };

void() gug_swingr1 = [129, gug_swingr2] { ai_gug_swing(22, 25); gug_step_sound(); };
void() gug_swingr2 = [130, gug_swingr3] { ai_gug_swing(20, 26); };
void() gug_swingr3 = [131, gug_swingr4] { ai_gug_swing(18, 25); sound(self, CHAN_BODY, "gug/throw.wav", 1, ATTN_NORM); };
void() gug_swingr4 = [132, gug_swingr5] { ai_gug_swing(16, 25); };
void() gug_swingr5 = [133, gug_swingr6] { ai_gug_swing(18, 24); gug_step_sound(); };
void() gug_swingr6 = [134, gug_swingr7] { ai_gug_swing(16, 24); };
void() gug_swingr7 = [135, gug_swingr8] { ai_gug_swing(14, 25); };
void() gug_swingr8 = [136, gug_swingr9] { ai_gug_swing(10, 25); gug_hit(150, 300, 35, 0.6); };
void() gug_swingr9 = [137, gug_swingr10] { ai_gug_swing(6, 10); };
void() gug_swingr10 = [138, gug_swingr11] { ai_gug_swing(2, 11); };
void() gug_swingr11 = [139, gug_swingr12] { ai_gug_swing(1, 10); };
void() gug_swingr12 = [140, gug_swingr13] { ai_gug_swing(1, 12); };
void() gug_swingr13 = [141, gug_swingr14] {
    local float dist;

    ai_gug_swing(1, 11);
    dist = vlen(self.enemy.origin - self.origin);
    if (dist > 170) {
        self.think = gug_run1;
    }
};
void() gug_swingr14 = [142, gug_swingr15] { ai_gug_swing(1, 14); sound(self, CHAN_WEAPON, "gug/throw.wav", 1, ATTN_NORM); };
void() gug_swingr15 = [143, gug_swingr16] { ai_gug_swing(1, 25); };
void() gug_swingr16 = [144, gug_swingr17] { ai_gug_swing(2, 27); };
void() gug_swingr17 = [145, gug_swingr18] { ai_gug_swing(22, 28); gug_step_sound(); };
void() gug_swingr18 = [146, gug_swingr19] { ai_gug_swing(20, 27); gug_hit(150, 500, 45, 0.9); };
void() gug_swingr19 = [147, gug_swingr20] { ai_gug_swing(18, 26); };
void() gug_swingr20 = [148, gug_swingr21] { ai_gug_swing(16, 22); };
void() gug_swingr21 = [149, gug_swingr22] { ai_gug_swing(22, 21); gug_step_sound(); };
void() gug_swingr22 = [150, gug_swingr23] { ai_gug_swing(20, 21); };
void() gug_swingr23 = [151, gug_run1] { ai_gug_swing(18, 20); };

void() bile_expl1 = [0, bile_expl2] {};
void() bile_expl2 = [1, bile_expl3] {};
void() bile_expl3 = [2, bile_expl4] {};
void() bile_expl4 = [3, bile_expl5] {};
void() bile_expl5 = [4, bile_expl6] {};
void() bile_expl6 = [5, SUB_Remove] {};

void() BecomeBileExplosion = {
    self.movetype = MOVETYPE_NONE;
    self.velocity = VEC_ORIGIN;
    self.touch = SUB_Null;
    setmodel(self, "progs/greenexp.spr");
    self.solid = SOLID_NOT;
    bile_expl1();
};

void() BileFragTouch = {
    local vector org;

    if (other == self.owner)
        return;

    if (pointcontents(self.origin) == CONTENT_SKY) {
        remove(self);
        return;
    }
    org = self.origin - 8 * normalize(self.velocity);
    if (other.takedamage) {
        T_Damage(other, self, self.owner, 20,DAMARMOR);
        T_RadiusDamage(self, self.owner, 60, other,DAMAGEALL);
    } else {
        if (self.nextthink > time)
            return;

        T_RadiusDamage(self, self.owner, 60, world,DAMAGEALL);
    }
    WriteByte(0, SVC_TEMPENTITY);
    WriteByte(0, 12);
    WriteCoord(0, org_x);
    WriteCoord(0, org_y);
    WriteCoord(0, org_z);
    WriteByte(0, 240);
    WriteByte(0, AS_MELEE);
    sound(self, CHAN_WEAPON, "gug/bile.wav", 0.5 + 0.5 * random(), 0.8);
    BecomeBileExplosion();
};

void() BileBombTouch = {
    local vector org;
    local float bcnt;
    local entity missile;

    if (other == self.owner)
        return;

    if (pointcontents(self.origin) == CONTENT_SKY) {
        remove(self);
        return;
    }
    org = self.origin - 8 * normalize(self.velocity);
    if (other.takedamage) {
        T_Damage(other, self, self.owner, 25,DAMARMOR);
        T_RadiusDamage(self, self.owner, 85, other,DAMAGEALL);
    } else {
        T_RadiusDamage(self, self.owner, 85, world,DAMAGEALL);
    }
    while (bcnt < 5) {
        bcnt = bcnt + 1;
        missile = spawn();
        missile.owner = self;
        missile.movetype = MOVETYPE_BOUNCE;
        missile.solid = SOLID_BBOX;
        makevectors(self.angles);
        missile.velocity_x = mrandom(1000);
        missile.velocity_y = mrandom(1000);
        missile.velocity_z = 250 + 100 * random();
        missile.avelocity = '300 300 300';
        missile.angles = vectoangles(missile.velocity);
        missile.touch = BileFragTouch;
        missile.owner = self.owner;
        missile.nextthink = time + 0.2 + 0.4 * random();
        missile.think = BileFragTouch;
        setmodel(missile, "progs/bilefrag.mdl");
        setsize(missile, VEC_ORIGIN, VEC_ORIGIN);
        setorigin(missile, self.origin);
    }
    WriteByte(0, SVC_TEMPENTITY);
    WriteByte(0, 12);
    WriteCoord(0, org_x);
    WriteCoord(0, org_y);
    WriteCoord(0, org_z);
    WriteByte(0, 240);
    WriteByte(0, AS_MELEE);
    sound(self, CHAN_WEAPON, "gug/bile.wav", 1, 0.8);
    BecomeBileExplosion();
};

void() BileBombThink = {
    local entity missile;
    local float spd;
    local float t;
    local vector vec;
    local vector org;

    self.effects = self.effects | EF_MUZZLEFLASH;
    self.angles = vectoangles(self.velocity);
    makevectors(self.angles);
    while (t < 2) {
        missile = spawn();
        missile.owner = self.owner;
        missile.movetype = MOVETYPE_TOSS;
        missile.solid = SOLID_BBOX;
        spd = vlen(self.velocity);
        org = self.origin;
        vec = self.velocity;
        if (t == 0) {
            vec = vec + v_right * (spd * 0.3);
        } else {
            vec = vec - v_right * (spd * 0.3);
        }
        vec = normalize(vec);
        vec = vec * spd;
        missile.velocity = vec;
        missile.avelocity = '300 300 300';
        missile.touch = BileBombTouch;
        setmodel(missile, "progs/bilebomb.mdl");
        setsize(missile, VEC_ORIGIN, VEC_ORIGIN);
        setorigin(missile, org);
        t = t + 1;
    }
    SUB_Remove();
};

void() gug_bilebomb = {
    local entity missile;
    local float dist;
    local float spd;
    local float t;
    local vector vec;
    local vector org;
    local vector d;

    dist = vlen(self.enemy.origin - self.origin);
    self.effects = self.effects | EF_MUZZLEFLASH;
    missile = spawn();
    missile.owner = self;
    missile.movetype = MOVETYPE_TOSS;
    missile.solid = SOLID_BBOX;
    spd = 1000 / (850 / dist);
    makevectors(self.angles);
    org = self.origin;
    t = vlen(self.enemy.origin - org) / spd;
    vec = self.enemy.velocity;
    vec_z = 0;
    d = self.enemy.origin + t * vec;
    vec = normalize(d - org);
    missile.velocity = vec;
    missile.velocity_z = 0;
    missile.velocity = missile.velocity * spd;
    t = self.enemy.origin_z - org_z;
    t = 250 + 1.3 * t;
    missile.velocity_z = t;
    missile.avelocity = '300 300 300';
    missile.angles = vectoangles(missile.velocity);
    missile.touch = BileBombTouch;
    missile.nextthink = time + 0.3;
    missile.think = BileBombThink;
    missile.enemy = self.enemy;
    setmodel(missile, "progs/bilebomb.mdl");
    setsize(missile, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(missile, self.origin + '0 0 75');
};

void() gug_bile1 = [152, gug_bile3] { ai_face(); };
void() gug_bile3 = [153, gug_bile5] { ai_face(); };
void() gug_bile5 = [154, gug_bile12] { ai_face(); };
void() gug_bile12 = [155, gug_bile13] { ai_face(); };
void() gug_bile13 = [156, gug_bile14] { ai_face(); };
void() gug_bile14 = [157, gug_bile15] { gug_bilebomb(); self.pain_finished = time + 1.3; };
void() gug_bile15 = [158, gug_bile16] {};
void() gug_bile16 = [159, gug_bile17] {};
void() gug_bile17 = [160, gug_bile18] {};
void() gug_bile18 = [161, gug_bile19] {};
void() gug_bile19 = [162, gug_bile20] {};
void() gug_bile20 = [163, gug_bile21] {};
void() gug_bile21 = [164, gug_bile22] {};
void() gug_bile22 = [165, gug_bile23] {};
void() gug_bile23 = [166, gug_bile24] {};
void() gug_bile24 = [167, gug_bile25] {};
void() gug_bile25 = [168, gug_bile26] {};
void() gug_bile26 = [169, gug_run1] {};

void() gug_quake_think = {
    local float points;
    local entity head;
    local vector org;

    self.nextthink = time + 0.080000;
    self.think = gug_quake_think;
    head = findradius(self.origin, 800);
    while (head) {
        if (head.classname != "monster_gug") {
            if (head.takedamage == DAMAGE_AIM && !(head.flags & FL_FLY)) {
                org = head.origin + (head.mins + head.maxs) * 0.5;
                points = 0.010000 * vlen(self.origin - org);
                if (points < 0)
                    points = 0;

                points = TE_LIGHTNING3 - points;
                if (points > 0) {
                    head.punchangle_x = (time - self.ltime) * random() * 4;
                    head.punchangle_y = (time - self.ltime) * random() * 4;
                    head.punchangle_z = (time - self.ltime) * random() * 4;
                    if (head.flags & FL_ONGROUND) {
                        if (head.flags & FL_MONSTER)
                            head.flags = head.flags - FL_ONGROUND;

                        head.velocity = head.velocity + random() * '0 0 150';
                    }
                }
            }
        }
        head = head.chain;
    }
    if (self.ltime < time)
        SUB_Remove();
};

void(float stime) gug_quake = {
    local entity qent;

    self.t_width = time + 4.5;
    sound(self, CHAN_AUTO, "gug/gquake.wav", 1, ATTN_NONE);
    qent = spawn();
    qent.owner = self;
    qent.movetype = MOVETYPE_NONE;
    qent.solid = SOLID_NOT;
    qent.spawnmaster = self.enemy;
    qent.nextthink = time + 0.1;
    qent.think = gug_quake_think;
    qent.ltime = time + stime;
    qent.classname = "gug_quake";
    self.spawnmaster = qent;
    setsize(qent, VEC_ORIGIN, VEC_ORIGIN);
    setorigin(qent, self.origin);
};

void() gug_quake_damage = {
    local float points;
    local entity head;
    local vector org;

    head = findradius(self.origin, 800);
    while (head) {
        if (head.classname != "monster_gug") {
            if (head.takedamage == DAMAGE_AIM && !(head.flags & FL_FLY)) {
                org = head.origin + (head.mins + head.maxs) * 0.5;
                points = 0.010000 * vlen(self.origin - org);
                if (points < 0)
                    points = 0;

                points = TE_LIGHTNING3 - points;
                if (points > 0) {
                    if (head.classname == "player") {
                        if (self.enemy.health <= points && !(self.enemy.flags & FL_GODMODE) && self.enemy.invincible_finished < time)
                            self.enemy.deathtype = "quake";

                        T_Damage(head, self, self, points,DAMARMOR);
                    } else {
                        if (CanDamage(head, self))
                            T_Damage(head, self.spawnmaster, self.spawnmaster, points,DAMARMOR);
                    }
                }
            }
        }
        head = head.chain;
    }
};

void() gug_quake1 = [170, gug_quake2] { ai_face(); };
void() gug_quake2 = [171, gug_quake3] { ai_face(); };
void() gug_quake3 = [172, gug_quake4] { ai_face(); };
void() gug_quake4 = [173, gug_quake5] { ai_face(); };
void() gug_quake5 = [174, gug_quake6] { ai_face(); };
void() gug_quake6 = [175, gug_quake7] { ai_face(); };
void() gug_quake7 = [176, gug_quake8] {};
void() gug_quake8 = [177, gug_quake9] {};
void() gug_quake9 = [178, gug_quake10] {
    gug_quake(3.3);
    gug_quake_damage();
    self.pain_finished = time + 1.6;
};

void() gug_quake10 = [179, gug_quake11] {};
void() gug_quake11 = [180, gug_quake12] { gug_quake_damage(); };
void() gug_quake12 = [181, gug_quake13] {};
void() gug_quake13 = [182, gug_quake14] { gug_quake_damage(); };
void() gug_quake14 = [183, gug_quake15] {};
void() gug_quake15 = [184, gug_quake16] { gug_quake_damage(); };
void() gug_quake16 = [185, gug_quake17] {};
void() gug_quake17 = [186, gug_quake18] { gug_quake_damage(); };
void() gug_quake18 = [187, gug_quake19] {};
void() gug_quake19 = [188, gug_quake20] {};
void() gug_quake20 = [189, gug_quake21] {};
void() gug_quake21 = [190, gug_quake22] {};
void() gug_quake22 = [191, gug_quake23] {};
void() gug_quake23 = [192, gug_quake24] {};
void() gug_quake24 = [193, gug_run1] {};

void() gug_death1 = [200, gug_death2] {};
void() gug_death2 = [201, gug_death3] { monster_check_gib(); };
void() gug_death3 = [202, gug_death4] { monster_check_gib(); };
void() gug_death4 = [203, gug_death5] { self.solid = SOLID_NOT; };
void() gug_death5 = [204, gug_death6] {};
void() gug_death6 = [205, gug_death7] {};
void() gug_death7 = [206, gug_death8] {};
void() gug_death8 = [207, gug_death9] {};
void() gug_death9 = [208, gug_death10] {};
void() gug_death10 = [209, gug_death11] {};
void() gug_death11 = [210, gug_death12] {};
void() gug_death12 = [211, gug_death13] {};
void() gug_death13 = [212, gug_death14] {};
void() gug_death14 = [213, gug_death15] {};
void() gug_death15 = [214, gug_death16] {};
void() gug_death16 = [215, gug_death17] {};
void() gug_death17 = [216, gug_death18] {};
void() gug_death18 = [217, gug_death19] {};
void() gug_death19 = [218, gug_death20] {};
void() gug_death20 = [219, gug_death21] {};
void() gug_death21 = [220, gug_death22] {};
void() gug_death22 = [221, gug_death23] {};
void() gug_death23 = [222, gug_death24] {};
void() gug_death24 = [223, gug_death25] {};
void() gug_death25 = [224, gug_death26] {
    local entity head;

    head = findradius(self.origin, 550);
    while (head) {
        if (head.flags & FL_CLIENT)
            gug_screen_shake(0.4, head); // mybody makes quakes when I die!

        head = head.chain;
    }
};
void() gug_death26 = [225, gug_death27] {};
void() gug_death27 = [226, gug_death28] {};
void() gug_death28 = [227, gug_death29] {};
void() gug_death29 = [228, gug_death30] {};
void() gug_death30 = [229, gug_death31] {};
void() gug_death31 = [230, gug_death32] {};
void() gug_death32 = [231, gug_death33] {};
void() gug_death33 = [232, gug_death34] {};
void() gug_death34 = [233, gug_death35] {};
void() gug_death35 = [234, gug_death36] {};
void() gug_death36 = [235, gug_death37] {};
void() gug_death37 = [236, gug_death38] {};
void() gug_death38 = [237, gug_death39] {};
void() gug_death39 = [238, gug_death40] {};
void() gug_death40 = [239, gug_death41] { monster_death_postcheck(); };
void() gug_death41 = [240, gug_death41] {};

void() gug_die = {
    // Pre-check routine to tidy up extra entities
    monster_death_precheck();

    if (!self.gibbed) {
        sound(self, CHAN_VOICE, "gug/death.wav", 1, 0.5);
        gug_death1();
    }
};

void() gug_melee = {
    sound(self, CHAN_VOICE, "gug/swipe.wav", 1, ATTN_NORM);
    if (self.state == 0)
        self.state = ceil(2 * random());

    if (self.state == 1) {
        self.state = 2;
        gug_swingl1();
    } else {
        if (self.state == 2) {
            self.state = 1;
            gug_swingr1();
        }
    }
};

void() gug_missile = {
    local float dist;

    dist = vlen(self.enemy.origin - self.origin);
    if (random() < 0.8 || self.t_width > time) {
        gug_bile1();
        sound(self, CHAN_BODY, "gug/bileatk.wav", 1, 0.7);
    } else {
        sound(self, CHAN_BODY, "gug/quakeatk.wav", 1, 0.7);
        gug_quake1();
    }
    return;
};

void() gug_pain1 = [194, gug_pain2] { ai_pain(8); };
void() gug_pain2 = [195, gug_pain3] { ai_pain(7); };
void() gug_pain3 = [196, gug_pain4] { ai_pain(6); };
void() gug_pain4 = [197, gug_pain5] { ai_pain(5); };
void() gug_pain5 = [198, gug_pain6] { ai_pain(0); };
void() gug_pain6 = [199, gug_run1] { ai_pain(0); };

void(entity inflictor, entity attacker, float damage) gug_pain = {
    if (self.health <= 0)
        return;

    if (self.pain_finished > time)
        return;

    if (random() * 400 > damage && damage > 55)
        return;

    sound(self, CHAN_VOICE, "gug/pain.wav", 1, ATTN_NORM);
    self.pain_finished = time + AS_MELEE;
    gug_pain1();
};

void() monster_gug = {
    if (deathmatch) { remove(self); return; }

    self.mdl = "progs/gug.mdl";
    precache_model("progs/gug.mdl");
    precache_model("progs/bilebomb.mdl");
    precache_model("progs/bilefrag.mdl");
    precache_model("progs/greenexp.spr");
    precache_sound("gug/step1.wav");
    precache_sound("gug/step2.wav");
    precache_sound("gug/step3.wav");
    precache_sound("gug/step4.wav");
    precache_sound("gug/pain.wav");
    precache_sound("gug/sight.wav");
    precache_sound("gug/death.wav");
    self.idle_sound = "gug/idle.wav";
    precache_sound("gug/idle.wav");
    precache_sound("gug/bileatk.wav");
    precache_sound("gug/swipe.wav");
    precache_sound("gug/quakeatk.wav");
    precache_sound("gug/bile.wav");
    precache_sound("gug/gquake.wav");
    precache_sound("gug/throw.wav");
    precache_sound("shambler/smack.wav");

    self.solid = SOLID_NOT;            // No interaction with world
    self.movetype = MOVETYPE_NONE;    // Static item, no movement
    if (self.bboxtype < 1) self.bboxtype = BBOX_GOLEM;
    if (self.health < 1) self.health = 1000;
    self.gibhealth = -150;
    self.gibbed = FALSE;            // Still in one piece
    self.pain_flinch = 200;            // Demon level

    // Always reset Ammo Resistance to be consistent
    self.resist_shells = 0; self.resist_nails = 0;
    self.resist_rockets = 0; self.resist_cells = 0;

    self.th_stand = gug_stand1;
    self.th_walk = gug_walk1;
    self.th_run = gug_run1;
    self.th_die = gug_die;
    self.th_melee = gug_melee;
    self.th_missile = gug_missile;
    self.th_novis = gug_quake1;
    self.th_pain = gug_pain;
    self.yaw_speed = 15;            // Slow turner
    if ( (random () < 0.500) ) {
        if (self.deathstring == "")  self.deathstring = " was obliterated by a Gug\n";
    } else {
        if (self.deathstring == "")  self.deathstring = " was squashed by a Gug\n";
    }

    self.classtype = CT_MONGUG;
    self.classgroup = CG_BOSS;
    self.classmove = MON_MOVEWALK;

    monster_start();
};