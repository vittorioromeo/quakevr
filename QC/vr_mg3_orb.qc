/*  Copyright (C) 1996-2026 id Software LLC

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/
// Dawn of the Machine's orb (monster_orb; MG3_PLAN.md M3-19), adapted from quakec_mg3/monsters/mg3_orb.qc (and its
// ai.qc branches: a second eye at its back, the strafe that gives up, the sight sound). A flying eye of 300 health
// (progs/teleporter_eye_blink.mdl: three frames, hover and two blinks) that blinks and bursts 4-6 spheres at you (18
// each), throws lightning about when hurt, and dies flung, falling until it touches something: there it blows up (100,
// radius). No ragdoll (a rigid eye, no rig), no head.
//
// Available wherever Dawn of the Machine's data is (MG3_PLAN.md, "Decisions" 5): its model and sounds are read in place
// from the owned pack (owned/mg3/...: vr_gamedir.cpp VR_OwnedFile), in any campaign; without it the orb is not spawned.
//
// Upstream's bug kept out: its OrbCheckAttack, at RANGE_FAR, called wiz_run1 (the scrag's run, left from the scrag it
// was made from): the eye ran the scrag's 14 run frames (frames it has not) with the scrag's idle sounds until its next
// attack check. Here it calls orb_run1, as its other branches do (the same 16-unit run).

$frame hover1 blink1 blink2

float mg3_orb_spheres; // spheres fired, deaths blown up (the tests' counts: vr_mg3_btest)
float mg3_orb_blowups;

// Its files, read in place from the owned Dawn of the Machine pack (VR_OwnedFile): literals for the precaches.
float() MG3_Orb_Available =
{
    return fileexists("owned/mg3/progs/teleporter_eye_blink.mdl") && fileexists("owned/mg3/progs/rogue/sphere.mdl");
};

void() orb_run1;
void() orb_side1;
void() orb_blast;
void() orb_death_touch;
void() T_OrbTouch;

// A bolt of lightning from `self` + `p` to a wall somewhere above the horizon (upstream boss_final.qc's pain_lightning:
// the orb's pain, the super shambler's and the bosses' fits).
void(vector p) MG3_PainLightning =
{
    vector tang = '0 0 0';
    tang_x = random() * 180 + 180; // (a positive pitch: above the horizon)
    tang_y = random() * 360;
    makevectors(tang);
    vector org = self.origin + p;
    traceline(org, org + v_forward * 1000, TRUE, self);

    sound(self, CHAN_BODY, "misc/power.wav", 1, ATTN_NORM);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_LIGHTNING3);
    WriteEntity(MSG_BROADCAST, self);
    WriteByte(MSG_BROADCAST, 0); // (Quake VR's beam id: VR_ParseBeamEntity)
    WriteVec3(MSG_BROADCAST, org);
    WriteVec3(MSG_BROADCAST, trace_endpos);
};

// ai.qc's CheckAnyAttack for the orb (upstream OrbCheckAttack): the scrag's, without its far-range bug (above).
float() OrbCheckAttack =
{
    if(time < self.attack_finished)
        return FALSE;
    if(!enemy_vis)
        return FALSE;

    if(enemy_range == RANGE_FAR)
    {
        if(self.attack_state != AS_STRAIGHT)
        {
            self.attack_state = AS_STRAIGHT;
            orb_run1(); // (upstream: wiz_run1)
        }
        return FALSE;
    }

    entity targ = self.enemy;

    // anything in the way of the shot: move to a side
    vector spot1 = self.origin + self.view_ofs;
    vector spot2 = VR_AI_Origin(targ) + targ.view_ofs;
    traceline(spot1, spot2, self.vr_ai_gate > 0 ? MOVE_PORTALS : FALSE, self);
    if(trace_ent != targ)
    {
        if(self.attack_state != AS_STRAIGHT)
        {
            self.attack_state = AS_STRAIGHT;
            orb_run1();
        }
        return FALSE;
    }

    float chance;
    if(enemy_range == RANGE_MELEE)
        chance = 0.9;
    else if(enemy_range == RANGE_NEAR)
        chance = 0.6;
    else if(enemy_range == RANGE_MID)
        chance = 0.2;
    else
        chance = 0;

    if(random() < chance)
    {
        self.attack_state = AS_MISSILE;
        return TRUE;
    }

    if(enemy_range == RANGE_MID)
    {
        if(self.attack_state != AS_STRAIGHT)
        {
            self.attack_state = AS_STRAIGHT;
            orb_run1();
        }
    }
    else
    {
        if(self.attack_state != AS_SLIDING)
        {
            self.attack_state = AS_SLIDING;
            orb_side1();
        }
    }

    return FALSE;
};

void() OrbAttackFinished =
{
    if(enemy_range >= RANGE_MID || !enemy_vis)
    {
        self.attack_state = AS_STRAIGHT;
        self.think = orb_run1;
    }
    else
    {
        self.attack_state = AS_SLIDING;
        self.think = orb_side1;
    }
};

void() Orb_idlesound =
{
    if(self.waitmin < time)
    {
        self.waitmin = time + 15 + random() * 10;
        sound(self, CHAN_VOICE, "boss2/sight.wav", 1, ATTN_IDLE);
    }
};

void() orb_stand1 = [$hover1, orb_stand1] {ai_stand();};
void() orb_walk1 = [$hover1, orb_walk1] {ai_walk(8); Orb_idlesound();};
void() orb_side1 = [$hover1, orb_side1] {ai_face(); ai_run(16); Orb_idlesound();};
void() orb_run1 = [$hover1, orb_run1] {ai_run(16); Orb_idlesound();};

// Its attack: a blink, then 1-3 bursts of spheres 0.2 s apart (ammo_nails counts them).
void() orb_fast1 = [$hover1, orb_fast2] {self.ammo_nails = 1 + floor(random() * 3 + 0.5); ai_face();};
void() orb_fast2 = [$blink1, orb_fast3] {};
void() orb_fast3 = [$blink2, orb_fast4] {self.nextthink = time + 0.3; ai_face();};
void() orb_fast4 = [$hover1, orb_fast5]
{
    ai_face();
    orb_blast();
    self.nextthink = time + 0.2;
    self.ammo_nails = self.ammo_nails - 1;
    if(self.ammo_nails > 0)
        self.think = orb_fast2;
};
void() orb_fast5 = [$blink2, orb_run1] {ai_face(); SUB_AttackFinished(2); OrbAttackFinished();};

// Its pain: eyes shut, three bolts of lightning about it (ammo_shells counts them).
void() orb_pain1 = [$blink2, orb_pain2] {self.ammo_shells = 3; self.nextthink = time + 0.2;};
void() orb_pain2 = [$blink2, orb_pain3]
{
    MG3_PainLightning('0 0 0');
    self.nextthink = time + 0.3;
    self.ammo_shells = self.ammo_shells - 1;
    if(self.ammo_shells > 0)
        self.think = orb_pain2;
};
void() orb_pain3 = [$blink2, orb_pain4] {};
void() orb_pain4 = [$hover1, orb_run1] {};

// Its death: flung (monster_death_use has taken its flight away: it falls), and from its third frame it blows up on the
// first thing it touches that is not a trigger nor a projectile (the floor, a wall, you).
void() orb_death1 = [$blink1, orb_death2]
{
    self.velocity_x = -200 + 400 * random();
    self.velocity_y = -200 + 400 * random();
    self.velocity_z = 150 + 150 * random();
    self.flags = self.flags - (self.flags & FL_ONGROUND);
    self.solid = SOLID_BBOX;
    setsize(self, '0 0 0', '0 0 0');
    sound(self, CHAN_VOICE, "owned/mg3/orb/orb_death.wav", 1, ATTN_NORM);
};
void() orb_death2 = [$blink2, orb_death3] {};
void() orb_death3 = [$blink2, orb_death4] {self.touch = orb_death_touch;};
void() orb_death4 = [$blink2, orb_death4] {self.nextthink = time + 9999;};

void() orb_death_touch =
{
    if(other.solid == SOLID_TRIGGER)
        return; // trigger field, do nothing
    if((other.solid == SOLID_BBOX) && !(other.health))
        return; // don't explode on projectiles

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION2);
    WriteCoord(MSG_BROADCAST, self.origin_x);
    WriteCoord(MSG_BROADCAST, self.origin_y);
    WriteCoord(MSG_BROADCAST, self.origin_z);
    WriteByte(MSG_BROADCAST, 244);
    WriteByte(MSG_BROADCAST, 3);

    mg3_orb_blowups = mg3_orb_blowups + 1; // (the tests' count)
    T_RadiusDamage(self, self, 100, world);

    remove(self);
};

void() orb_die =
{
    // (upstream's gib branch is commented out: it always dies flung)
    orb_death1();
};

void(entity attacker, float damage) Orb_Pain =
{
    if(self.pain_finished > time)
        return;
    if(random() * 200 > damage)
        return; // didn't flinch

    sound(self, CHAN_VOICE, "owned/mg3/orb/orb_pain.wav", 1, ATTN_NORM);
    self.pain_finished = time + 8;
    orb_pain1();
};

void() Orb_Missile =
{
    orb_fast1();
};

// A burst: 4-6 spheres from its eye at you, leading you a quarter of the way on harder skills (400, 450, 500 a second,
// give or take 100), a little spread.
void() orb_blast =
{
    if(!VR_AI_ShotReady(self.origin)) { return; }
    float baseSpeed = 400;
    if(skill > 2)
        baseSpeed = 500;
    else if(skill > 0)
        baseSpeed = 450;
    vector spreadVec = '0.1 0.1 0.01';
    float orbs = 4 + floor(random() * 2 + 0.5);

    vector target = VR_AI_Origin(self.enemy);
    makevectors(vectoangles(target - self.origin));
    vector org = self.origin + 15 * v_forward;

    float t = vlen(target - org) / baseSpeed;
    vector vel = VR_AI_Vector(self.enemy.velocity);
    vel_z = 0;
    vector vec = normalize(target + t / 4 * vel - org);

    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteCoord(MSG_BROADCAST, org_x);
    WriteCoord(MSG_BROADCAST, org_y);
    WriteCoord(MSG_BROADCAST, org_z);

    while(orbs > 0)
    {
        vector newVec = normalize(vec + crandom() * spreadVec_x * v_right + crandom() * spreadVec_y * v_up);
        entity mis = launch_spike(org + newVec * 8, newVec);
        mis.classname = "orb_sphere";
        mis.owner = self;
        setmodel(mis, "owned/mg3/progs/rogue/sphere.mdl");
        setsize(mis, '0 0 0', '0 0 0');
        mis.velocity = newVec * (baseSpeed + crandom() * 100);
        mis.avelocity = '300 300 300';
        mis.avelocity_x = mis.avelocity_x * crandom();
        mis.avelocity_y = mis.avelocity_y * crandom();
        mis.avelocity_z = mis.avelocity_z * crandom();
        if(orbs % 2 == 0)
            mis.frags = 1; // (every other one sparks on a wall)
        mis.touch = T_OrbTouch;
        mg3_orb_spheres = mg3_orb_spheres + 1;
        orbs = orbs - 1;
    }
};

// A sphere: 18 to what bleeds; passes through orbs, lava men and super shamblers (theirs too: upstream's), and other
// spheres.
void() T_OrbTouch =
{
    if(other == self.owner)
        return;
    if(other.classname == "monster_orb" || other.classname == "monster_lava_man" ||
       other.classname == "monster_super_shambler")
    {
        remove(self);
        return;
    }
    if(other.classname == self.classname)
        return;
    if(other.solid == SOLID_TRIGGER)
        return; // trigger field, do nothing
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }

    if(other.takedamage)
    {
        spawn_touchblood(18);
        T_Damage(other, self, self.owner, 18);
    }
    else if(self.frags)
    {
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_KNIGHTSPIKE);
        WriteCoord(MSG_BROADCAST, self.origin_x);
        WriteCoord(MSG_BROADCAST, self.origin_y);
        WriteCoord(MSG_BROADCAST, self.origin_z);
    }

    remove(self);
};

void() monster_orb_precache =
{
    precache_model("owned/mg3/progs/teleporter_eye_blink.mdl");
    precache_model("owned/mg3/progs/rogue/sphere.mdl");
    precache_sound("wizard/hit.wav"); // (the engine's)
    precache_sound("wizard/wattack.wav");
    precache_sound("owned/mg3/orb/orb_death.wav");
    precache_sound("owned/mg3/orb/orb_pain.wav");
    precache_sound("boss2/sight.wav");
    precache_sound("misc/power.wav");
};

/*QUAKED monster_orb (1 0 0) (-32 -32 -24) (32 32 64) Ambush
Dawn of the Machine's orb (needs the owned Dawn of the Machine data, else nothing spawns): a flying eye of 300 health that sees behind it too, bursts spheres at you (18 each) and throws lightning when hurt; killed, it is flung and blows up (100) where it lands.
*/
void() monster_orb =
{
    if(MG_MonsterInhibited()) return;
    if(deathmatch)
    {
        remove(self);
        return;
    }
    if(!VR_Pack_RequireSpawn(MG3_Orb_Available(), "Dawn of the Machine")) return;

    monster_orb_precache();

    self.health = 300;

    self.th_stand = orb_stand1;
    self.th_walk = orb_walk1;
    self.th_run = orb_run1;
    self.th_missile = Orb_Missile;
    self.th_pain = Orb_Pain;
    self.th_die = orb_die;

    monster_add_to_counter();

    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_STEP;

    setmodel(self, "owned/mg3/progs/teleporter_eye_blink.mdl");
    setsize(self, VEC_HULL2_MIN, VEC_HULL2_MAX); // (upstream: MONSTER_SIZE_LARGE)

    flymonster_start();
};
