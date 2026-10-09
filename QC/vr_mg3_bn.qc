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
// Dawn of the Machine's changes to Quake's monsters, and their Bloody Nightmare (MG3.md M3-23), adapted from
// quakec_mg3/monsters/{soldier,enforcer,hknight,wizard,shambler,shalrath,ogre,zombie}.qc, combat.qc and items.qc's
// DropBackpack callers. Campaign 5 only (MG3_Campaign; Bloody Nightmare: MG3_BloodyNightmare): every other campaign's
// monsters are as they were. The hooks are one call each in the monsters' own files (orig_mon_*.qc), combat.qc and
// items.qc:
// - no backpacks from grunts, enforcers and ogres (Quake VR's weapon drops stay);
// - skill 3: grunts (100) and enforcers (200) shrug off more blows (no pain when random() * that > damage); the
//   nightmare pain rest (5 s after a pain) spares zombies and Chthon;
// - shamblers at skill 3 cast lightning after three or four blows (one fewer in Bloody Nightmare);
// - vores' balls steer every 0.1 s (0.2), apart from each other, at 285 on skill 3 (Quake's 350);
// - zombies: spawnflag 8388608 hangs one (still, as the crucified, its pain frame), spawnflag 128 throws flesh only up
//   close;
// - Bloody Nightmare: an enforcer fires a second laser beside its first (15, alternating sides), a death knight's
//   spikes fly at 500 (300), a scrag adds one or two spikes of MG3's (owned w_spike2.mdl, else Quake's) beside each, an
//   ogre throws two more grenades 70% of its volleys (thrown higher or lower by where you are, and aside).

float MG3_ZOMBIE_HANGING = 8388608;
float MG3_ZOMBIE_CLOSE_THROW = 128;
float mg3_bn_extra; // extra shots fired by the Bloody Nightmare branches (the tests' count)

// DropBackpack's (items.qc): a monster of Dawn of the Machine's drops none.
float(entity e) MG3_NoBackpack = { return MG3_Campaign() && (e.flags & FL_MONSTER); };

// Skill 3's harder skin (army_pain: 100, enf_pain: 200): TRUE when this blow causes no pain.
float(float xRange, float damage) MG3_ShrugsOff = { return MG3_Campaign() && skill > 2 && random() * xRange > damage; };

// T_Damage's nightmare pain rest: spared for zombies (they must flinch to be put down) and Chthon.
float(entity e) MG3_NightmarePainRest = {
    if(e.mg3_chthon) return FALSE; // (Dawn of the Machine's Chthon, in any campaign: his fits are his phases)
    return !MG3_Campaign() || (e.classname != "monster_zombie" && e.classname != "monster_boss");
};

// The shambler at skill 3: after `cap` blows (3 or 4, one fewer in Bloody Nightmare) its lightning (upstream
// nightmare_attack_check: .count, reset as it casts).
float() MG3_ShamblerCasts =
{
    if(!MG3_Campaign() || skill <= 2)
        return FALSE;
    float cap = 3 + floor(random() * 2 + 0.5);
    if(MG3_BloodyNightmare())
        cap = cap - 1;
    self.count = self.count + 1;
    return self.count > cap;
};

// ---- Vores' balls -----------------------------------------------------------------------------------------------

// The ball's steering (ShalHome) in campaign 5: every 0.1 s, at 285 on skill 3 (else 250), nudged away from the nearest
// other ball within 32 units. FALSE: Quake's own (the caller steers).
float(vector dir) MG3_ShalSteer =
{
    if(!MG3_Campaign())
        return FALSE;
    float speed = skill == 3 ? 285 : 250;
    float maxd = 32;
    float dist = maxd;
    entity nearest = world;
    entity cur = find(world, classname, "shal_missile");
    while(cur)
    {
        if(cur != self)
        {
            float d2 = vlen(cur.origin - self.origin);
            if(d2 < dist) { dist = d2; nearest = cur; }
        }
        cur = find(cur, classname, "shal_missile");
    }
    dist = dist / maxd;
    dist = 0.25 - dist * dist * 0.25;
    if(nearest != world)
        dir = dir * (1 - dist) + normalize(self.origin - nearest.origin) * dist;
    self.velocity = dir * speed;
    self.nextthink = time + 0.1;
    return TRUE;
};

// ---- Bloody Nightmare's extra shots --------------------------------------------------------------------------------

void() MG3_LaserAltTouch =
{
    if(other == self.owner)
        return;
    if(pointcontents(self.origin) == CONTENT_SKY)
    {
        remove(self);
        return;
    }
    vector org = self.origin - 8 * normalize(self.velocity);
    if(other.health)
    {
        SpawnBlood(org, self.velocity * 0.2, 15);
        T_Damage(other, self, self.owner, 15);
    }
    else
    {
        WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
        WriteByte(MSG_BROADCAST, TE_GUNSHOT);
        WriteVec3(MSG_BROADCAST, org);
    }
    remove(self);
};

// enforcer_fire's: the second laser, beside the first by turns (upstream LaunchLaserAlt: 600, 15).
void(vector org) MG3_EnforcerAltLaser =
{
    if(!MG3_BloodyNightmare())
        return;
    makevectors(self.angles);
    vector vec = normalize(VR_AI_Origin(self.enemy) - self.origin);
    vec = normalize(vec + v_right * (self.mg3_altside ? 0.05 : -0.05));
    self.mg3_altside = 1 - self.mg3_altside;
    entity m = spawn();
    m.owner = self;
    VR_AI_Tag(m);
    m.classname = "laser";
    m.movetype = MOVETYPE_FLY;
    m.solid = SOLID_BBOX;
    setmodel(m, "progs/laser.mdl");
    setsize(m, '0 0 0', '0 0 0');
    setorigin(m, org);
    m.velocity = vec * 600;
    m.angles = vectoangles(m.velocity);
    m.nextthink = time + 5;
    m.think = SUB_Remove;
    m.touch = MG3_LaserAltTouch;
    mg3_bn_extra = mg3_bn_extra + 1;
};

// hknight_shot's spike speed.
float() MG3_HknightSpikeSpeed = { return MG3_BloodyNightmare() ? 500 : 300; };

// Wiz_FastFire's (self the scrag's spike launcher, self.owner the scrag, `vec` its spike's way): one or two more spikes
// beside it (MG3's w_spike2.mdl when its data is there).
void(vector vec) MG3_WizardExtraSpikes =
{
    if(!MG3_BloodyNightmare())
        return;
    float t = floor(random() * 2 + 0.5);
    while(t > 0)
    {
        float offs = 0.05 + random() * 0.05;
        if(random() > 0.5) offs = -offs;
        float offs2 = 0.01 + random() * 0.01;
        if(random() > 0.5) offs2 = -offs2;
        vector vec2 = normalize(vec * 0.9 + v_right * offs + v_up * offs2);
        entity m = launch_spike(self.origin, vec2);
        m.velocity = vec2 * (600 - (5 * t + random() * 50));
        m.owner = self.owner;
        m.classname = "wizspike";
        setmodel(m, fileexists("owned/mg3/progs/w_spike2.mdl") ? "owned/mg3/progs/w_spike2.mdl" : "progs/w_spike.mdl");
        setsize(m, VEC_ORIGIN, VEC_ORIGIN);
        mg3_bn_extra = mg3_bn_extra + 1;
        t--;
    }
};

// A scrag's precache (monster_wizard) for its Bloody Nightmare spikes.
void() MG3_WizardPrecache =
{
    if(MG3_Campaign() && fileexists("owned/mg3/progs/w_spike2.mdl"))
        precache_model("owned/mg3/progs/w_spike2.mdl");
};

// The ogre's volley (OgreFireGrenade, self the ogre): in Bloody Nightmare 70% of its volleys two more grenades follow,
// 0.3 s apart, each thrown higher (you above it, or far) or lower (below it, or close) and up to 64 aside (upstream: the
// volley's frames looped; here a helper throws them, the ogre still alive and not a rocket ogre's).
void() MG3_OgreBurstThink =
{
    entity ogre = self.owner;
    if(ogre.health <= 0 || !ogre.enemy || ogre.mg3_burst <= 0)
    {
        remove(self);
        return;
    }
    entity oldself = self;
    self = ogre;
    self.mg3_burst = self.mg3_burst - 1;
    self.mg3_bursting = TRUE;
    OgreFireGrenade();
    self.mg3_bursting = FALSE;
    self = oldself;
    mg3_bn_extra = mg3_bn_extra + 1;
    self.nextthink = time + 0.3;
};

void() MG3_OgreBurst =
{
    if(!MG3_BloodyNightmare() || self.mg3_bursting || self.aflag || random() <= 0.3)
        return;
    self.mg3_burst = 2;
    entity h = spawn();
    h.owner = self;
    h.classname = "ogre_burst";
    h.think = MG3_OgreBurstThink;
    h.nextthink = time + 0.3;
};

// A burst grenade's throw (OgreFireGrenade's velocity, self the ogre): higher or lower by where you are, and aside.
vector(vector vel) MG3_OgreBurstVelocity =
{
    if(!self.mg3_bursting)
        return vel;
    vector at = VR_AI_Origin(self.enemy);
    float zBoost = at_z - self.origin_z;
    vector flat = at - self.origin;
    flat_z = 0;
    if(zBoost > 30 || vlen(flat) > 500)
        vel_z = 300 + crandom() * 50;
    else if(zBoost < -30 || vlen(flat) < 100)
        vel_z = 100 + crandom() * 25;
    makevectors(self.angles);
    return vel + v_right * crandom() * 64;
};
