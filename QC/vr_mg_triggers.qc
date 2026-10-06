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
// Shared authored triggers, adapted from quakec_mg1/mg3 triggers.qc, misc_fx.qc and misc.qc.
// License/provenance: docs/vr-port/OFFICIAL_QC_SOURCE.md and COPYING-rerelease.txt.
.float speed2;
float() MG_TriggerGuard = {
    if(!MG_WorldGuard()) return FALSE;
    return !MG_MonsterInhibited();
};
float(entity e) MG_MessageAll = {
    if(!MG_WorldCampaign()) return FALSE;
    return (cvar("vr_campaign") == 5) ? (e.spawnflags & 8388608) : (e.spawnflags & 2097152);
};
void(string text) MG_PrintAll = {
    entity p = find(world, classname, "player");
    while(p) { centerprint(p, text); p = find(p, classname, "player"); }
};
float(entity e) MG_CountTargets = {
    float count = 0;
    entity t = find(world, targetname, e.target);
    while(t) { if(t.classname != "DelayedUse") count++; t = find(t, targetname, e.target); }
    return count;
};
void(float count) MG_CounterMessage = {
    if((activator.classname != "player" && !MG_MessageAll(self)) || (self.spawnflags & 1)) return;
    if(count >= 4) MG_PrintAll("$qc_more_go");
    else if(count == 3) MG_PrintAll("$qc_three_more");
    else if(count == 2) MG_PrintAll("$qc_two_more");
    else if(count == 1) MG_PrintAll("$qc_one_more");
    else MG_PrintAll("$qc_sequence_completed");
};
void() MG_counter_use = {
    self.count--;
    if(self.count < 0) return;
    MG_CounterMessage(self.count);
    if(self.count) return;
    self.enemy = activator;
    if(self.spawnflags & 2) self.count = self.wait;
    multi_trigger();
};
void() MG_counter_timed_think = { self.count = self.cnt; };
void() MG_counter_timed_use = {
    self.count--;
    if(self.count < 0) return;
    self.nextthink = time + self.delay;
    MG_CounterMessage(self.count);
    if(self.count) return;
    self.enemy = activator;
    self.delay = 0;
    self.nextthink = -1;
    self.think = SUB_Null;
    multi_trigger();
    remove(self);
};
/*QUAKED trigger_counter_timed (.5 .5 .5) (-8 -8 -8) (8 8 8) nomessage
Authored timed sequence. Count defaults to 2, delay is the inactivity timeout (default 2 seconds); completion fires targets immediately once.
*/
void() trigger_counter_timed = {
    if(!MG_TriggerGuard()) return;
    self.wait = -1;
    if(!self.count) self.count = 2;
    if(!self.delay) self.delay = 2;
    self.cnt = self.count;
    self.use = MG_counter_timed_use;
    self.think = MG_counter_timed_think;
};
void() MG_repeater_think = {
    activator = self.enemy;
    SUB_UseTargets();
    self.nextthink = time + self.wait + self.pausetime * random();
};
void() MG_repeater_use = {
    if(self.spawnflags & 1) {
        self.spawnflags = self.spawnflags - 1;
        self.nextthink = 0; self.think = SUB_Null;
    } else {
        self.spawnflags = self.spawnflags | 1;
        self.enemy = activator;
        self.nextthink = time + self.wait + self.pausetime * random();
        self.think = MG_repeater_think;
    }
};
/*QUAKED trigger_repeater (.5 .5 .5) (-8 -8 -8) (8 8 8) start_on
Toggle target repeater. Wait defaults to 1 second; pausetime adds random interval jitter. Start-on schedules its first activation after that interval.
*/
void() trigger_repeater = {
    if(!MG_TriggerGuard()) return;
    if(!self.wait) self.wait = 1;
    self.use = MG_repeater_use;
    if(self.spawnflags & 1) {
        self.nextthink = time + self.wait + self.pausetime * random();
        self.think = MG_repeater_think;
    } else { self.nextthink = 0; self.think = SUB_Null; }
};
void() MG_changetarget_use = {
    entity e = find(world, targetname, self.target);
    while(e) { e.target = self.killtarget; e = find(e, targetname, self.target); }
};
/*QUAKED trigger_changetarget (.5 .5 .5) (-8 -8 -8) (8 8 8)
Rewrite the target field of all entities named target to killtarget. Does not destroy those entities.
*/
void() trigger_changetarget = {
    if(!MG_TriggerGuard()) return;
    if(!self.target) objerror("Missing target.");
    if(!self.killtarget) objerror("Missing target to change to.");
    self.use = MG_changetarget_use;
};
void() MG_cleanup_corpses_use = {
    entity e = nextent(world);
    while(e) {
        entity n = nextent(e);
        if((e.flags & FL_MONSTER) && e.health <= 0) remove(e);
        e = n;
    }
    remove(self);
};
/*QUAKED trigger_cleanup_corpses (.5 .5 .5) (-8 -8 -8) (8 8 8)
Cooperative-only one-shot cleanup of dead monsters, including native ragdoll bodies; living monsters and players remain.
*/
void() trigger_cleanup_corpses = {
    if(!MG_TriggerGuard()) return;
    if(!coop) { remove(self); return; }
    self.use = MG_cleanup_corpses_use;
};
.float MG_storednextthink;
.void() MG_storedthink;

//============================================================================
// SCREENSHAKE
//============================================================================
const float MG_SILENT_SHAKE = 1;
.float MG_is_frozen;

void() MG_T_Shake =
{
	local entity t;
	local float starttime;
	local float hit;

	starttime = self.MG_storednextthink - self.wait;

	// early out if intermission is running
	if (intermission_running)
		return;

	// Completed, cleanup!
	if (time > self.MG_storednextthink)
	{
		if (!(self.spawnflags & MG_SILENT_SHAKE))
			sound (self, CHAN_VOICE, self.noise1, 1, ATTN_NORM);

		t = find( world, classname, "player" );
		while (t)
		{
			// No reset of player view or tracking angles in VR.
			t = find( t, classname, "player" );
		}
		return;
	}

	// Ramp up?
	if (time < self.delay)
		hit = (time - starttime) / (self.wait/3);

	else
		hit = 1;

	// dprint("intensity: ");
	// dprint(ftos(hit));
	// dprint("\n");

	hit = self.dmg * hit;

	// for each player
	t = find( world, classname, "player" );
	while (t)
	{
        // Controller vibration conveys the quake without moving the tracked view.
        if(t.ishuman) {
            entity shaker = self;
            self = t;
            float intensity = min(1, max(0, hit / 10));
            haptic(cVR_MainHand, 0, 0.055, 35, intensity);
            haptic(cVR_OffHand, 0, 0.055, 35, intensity);
            self = shaker;
        }

		t = find( t, classname, "player" );
	}

	self.nextthink = time + 0.05;
};

void() MG_screenshake_use =
{
	self.MG_storednextthink = time + self.wait; // when to end
	self.delay = time + (self.wait / 3); // how long to ramp up

	if (!(self.spawnflags & MG_SILENT_SHAKE))
		sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);

	self.think = MG_T_Shake;
	self.nextthink = time + 0.05;
};

/*QUAKED trigger_screenshake

wait is duration of the shake
dmg is the intensity of the shake
*/
void() trigger_screenshake =
{
	if (!MG_TriggerGuard()) return;

	// wait is sustain time
	// dmg is intensity
	if (!(self.spawnflags & MG_SILENT_SHAKE))
	{
		self.noise = "misc/quake.wav";
		self.noise1 = "misc/quakeend.wav";
		precache_sound(self.noise);
		precache_sound(self.noise1);
	}

	if (!self.wait) // no duration!
		self.wait = 2;

	if (!self.dmg) // no intensity!
		self.dmg = 3;

	self.use = MG_screenshake_use;
}

//============================================================================
// SOUND TRIGGER
//============================================================================

void() MG_trigger_sound_play =
{
	if (!self.noise)
		return;

	sound(self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
};
/*QUAKED trigger_sound
*/
void() trigger_sound =
{
	if (!MG_TriggerGuard()) return;
	if (!self.noise) { remove(self); return; }

	precache_sound (self.noise);
	self.use = MG_trigger_sound_play;

};

//============================================================================
// TRIGGER LIGHTNING
//============================================================================

const float MG_LIGHTNING_RANDOM_TARGET = 1;
const float MG_LIGHTNING_TRACE_BACKWARDS = 2;
const float MG_LIGHTNING_NO_ACTIVATE_TARGET = 4;
const float MG_LIGHTNING_TRIGGER_TARGETS_ONCE = 8;
const float MG_LIGHTNING_SILENT = 16;
// DelayedUse shares the endpoint targetname for cancellation, but is never a beam endpoint.
// Excluding it prevents the lightning traversal from recursively creating delayed uses forever.
entity(entity start, string name) MG_NextLightningTarget = {
    entity e = find(start,targetname,name);
    while(e && e.classname == "DelayedUse") e = find(e,targetname,name);
    return e;
};
void() MG_play_lightning =
{
	if (!self.target)
	{
		dprint("lightning has no target!\n");
		return;
	}

	float rt = 0;
	if(self.spawnflags & MG_LIGHTNING_RANDOM_TARGET)
	{
		float cnt = MG_CountTargets(self);
		if(cnt == 0) return;
		rt = floor(cnt * random());
	}
	float triggertwice = (self.spawnflags & MG_LIGHTNING_TRIGGER_TARGETS_ONCE) ? FALSE : TRUE; //Note boolean reversal
	float activatetargets = (self.spawnflags & MG_LIGHTNING_NO_ACTIVATE_TARGET) ? FALSE : TRUE; //Note boolean reversal

	local entity t, oself;
    entity lightning_actor = activator;

	float i = 0;
	t = MG_NextLightningTarget(world, self.target);
	while(t)
	{
		if(self.spawnflags & MG_LIGHTNING_RANDOM_TARGET)
		{
			if (i < rt){
				t = MG_NextLightningTarget(t, self.target);
				i++;
				continue;
			}
			if (i > rt) return;
			i++;
		}

		vector endpos, startpos;
		if(self.spawnflags & MG_LIGHTNING_TRACE_BACKWARDS)
		{
			traceline(t.origin, self.origin, TRUE, self);
			endpos = trace_endpos;
			startpos = t.origin;
		}
		else
		{
			traceline(self.origin, t.origin, TRUE, self);
			endpos = trace_endpos;
			startpos = self.origin;
		}

		if (cvar("vr_campaign") != 5 || !(self.spawnflags & MG_LIGHTNING_SILENT))
			sound (t, CHAN_AUTO, self.noise, self.volume, ATTN_NORM); //Changed to CHAN_AUTO to allow more overlapping sounds

		// create the temp lightning entity
		WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
		WriteByte (MSG_BROADCAST, self.style);
		WriteEntity (MSG_BROADCAST, t);
        WriteByte(MSG_BROADCAST, 0); // Native VR beam protocol disambiguator; world endpoint, no hand remap.
		WriteCoord (MSG_BROADCAST, startpos_x);
		WriteCoord (MSG_BROADCAST, startpos_y);
		WriteCoord (MSG_BROADCAST, startpos_z);
		WriteCoord (MSG_BROADCAST, endpos_x);
		WriteCoord (MSG_BROADCAST, endpos_y);
		WriteCoord (MSG_BROADCAST, endpos_z);

		if(self.dmg)
		{
			LightningDamage (startpos, endpos, self, self.dmg);
		}

		//If activatetargets and t has a target, activate it.
		if(activatetargets && t.target)
		{
			oself = self;
			self = t;
            activator = lightning_actor;
			SUB_UseTargets();
			if(triggertwice)
			{
				float odelay = self.delay;
				self.delay = odelay + 0.2;
				SUB_UseTargets(); //Quickly turn on and off
				self.delay = odelay;
			}
			self = oself;
            activator = lightning_actor;
		}

		t = MG_NextLightningTarget(t, self.target);
	}
};

void() trigger_lightning =
{
    if (!MG_TriggerGuard()) return;
	if(!self.noise) self.noise = "misc/power.wav";
	precache_sound(self.noise);
	self.use = MG_play_lightning;

	switch(self.style)
	{
		default:
		case 0:
			self.style = TE_LIGHTNING3;
			break;
		case 1:
			self.style = TE_LIGHTNING1;
			break;
		case 2:
			self.style = TE_LIGHTNING2;
			break;
	}

	if(!self.volume)
		self.volume = 1;

	if(cvar("vr_campaign") == 5 && (self.spawnflags & MG_LIGHTNING_SILENT))
		self.volume = 0;

};

//============================================================================
// TRIGGER FADE
//============================================================================
/* Checks all targets to see if they are dead. If they are, then it starts a fade
*/
void() MG_SUB_FadeTargets =
{
	local entity t;
	local float count; // how many entities are being faded
	count = 0;

	if (self.target != NULLSTR)
	{
		if(!self.state)
		{
			t = world;
			//Initialize all the targets with alpha = 1.0, so the fade out works.
			t = find (t, targetname, self.target);
			while (t)
			{
				t.alpha = 1.0;
				t = find (t, targetname, self.target);
			}
			self.state = 1; // Don't repeat this initialization after the first think.
		}

		t = world;
		t = find (t, targetname, self.target);
		while (t)
		{
			if (t.health <= 0)
			{
				if (t.alpha > 0.001)
				{
					t.alpha = max(0.001, t.alpha - (frametime/self.delay));
					count = count + 1;
				}
				else
				{
					dprint("fade complete, removing ");
					dprint(t.classname);
					dprint("\n");
					remove(t);
				}
			}
			else
			{
				dprint ("target is still alive\n");
			}
			t = find (t, targetname, self.target);
		}
	}
	if (count > 0)
	{
		self.nextthink = time;
	}
	else
	{
		remove(self);
		dprint("removed fade manager\n");
	}
};

void() MG_spawn_fade_manager =
{
	local entity fademanager;
	fademanager = spawn();
	dprint("spawn fade manager\n");
	fademanager.think = MG_SUB_FadeTargets;
	fademanager.nextthink = time;
	fademanager.target = self.target;
	fademanager.delay = self.delay; // use delay to determine fade time, default to 1
}

void() trigger_fade =
{
	if (!MG_TriggerGuard()) return;

	self.use = MG_spawn_fade_manager;

	if (!self.delay)
		self.delay = 1; // delay determines fade time
};

//============================================================================
// TRIGGER FREEZE
//============================================================================
// Yoder Jan 14 2021


void() MG_SUB_Freeze =
{
	self.MG_storednextthink = self.nextthink;
	self.MG_storedthink = self.think;
	self.nextthink = time;
	self.think = SUB_Null;
	self.MG_is_frozen = 1;
};

void() MG_SUB_Unfreeze =
{
	self.nextthink = self.MG_storednextthink;
	self.think = self.MG_storedthink;
	self.MG_storednextthink = -1;
	self.MG_is_frozen = 0;
};

void() MG_SUB_FreezeTargets =
{
	local entity t;

	if (self.target != NULLSTR)
	{
		t = world;
		do
		{
			t = find (t, targetname, self.target);
			if (!t)
				return;

			if (!(t.MG_is_frozen)) // check if already frozen
			{
				// freeze
				t.MG_storednextthink = t.nextthink;
				t.MG_storedthink = t.think;
				t.nextthink = time;
				t.think = SUB_Null;
				t.MG_is_frozen = 1;

			}
			else
			{
				// unfreeze
				t.nextthink = t.MG_storednextthink;
				t.think = t.MG_storedthink;
				t.MG_storednextthink = -1;
				t.MG_is_frozen = 0;
			}
		} while (1);
	}
};

/* Define the entity, trigger_freeze
	Works like a trigger relay
*/
void() trigger_freeze =
{
	if (!MG_TriggerGuard()) return;

	self.use = MG_SUB_FreezeTargets;
};


// Keep keys in the common physical activation path: hands, held props and thrown weapons agree.
float(entity player) MG_ButtonCanTouch = {
    if(!MG_WorldCampaign() || !self.items) return TRUE;
    if(player.classname != "player" || (self.items & player.items) != self.items) {
        if(time >= self.attack_finished) {
            self.attack_finished = time + 2;
            sound(self, CHAN_VOICE, self.noise3, 1, ATTN_NORM);
            if(player.classname == "player") {
                string text = self.items == IT_KEY1 ? "$qc_need_silver_key" : "$qc_need_gold_key";
                if(self.worldtype == 2) text = self.items == IT_KEY1 ? "$qc_need_silver_keycard" : "$qc_need_gold_keycard";
                else if(self.worldtype == 1) text = self.items == IT_KEY1 ? "$qc_need_silver_runekey" : "$qc_need_gold_runekey";
                centerprint(player, text);
            }
        }
        return FALSE;
    }
    player.items = player.items - self.items;
    return TRUE;
};
void() MG_ButtonInit = {
    if(self.spawnflags & 16) self.items = IT_KEY1;
    if(self.spawnflags & 8) self.items = IT_KEY2;
    self.worldtype = world.worldtype;
    if(self.worldtype == 2) { self.noise3 = "doors/basetry.wav"; self.noise4 = "doors/baseuse.wav"; }
    else if(self.worldtype == 1) { self.noise3 = "doors/runetry.wav"; self.noise4 = "doors/runeuse.wav"; }
    else { self.noise3 = "doors/medtry.wav"; self.noise4 = "doors/meduse.wav"; }
    precache_sound(self.noise3); precache_sound(self.noise4);
};



// Official trigger_explosion is an activated point effect, distinct from Rogue's damageable brush relay.
void() MG_trigger_explosion_activate = {
    activator = self.enemy;
    self.delay = 0;
    SUB_UseTargets();
    // (world the attacker, as upstream: the activator as one would halve its own damage, credit it with the
    // monsters' kills and anger them at it, and teamplay would spare the other players)
    if(!(self.spawnflags & 1)) T_RadiusDamage(self, world, 120, self);
    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
    WriteVec3(MSG_BROADCAST, self.origin);
    BecomeExplosion();
};
void() MG_trigger_explosion_use = {
    self.enemy = activator;
    if(self.delay > 0) { self.think = MG_trigger_explosion_activate; self.nextthink = time + self.delay; }
    else MG_trigger_explosion_activate();
};
