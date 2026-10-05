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
const float MG_FORCE_SET_FOG = 1;
const float MG_SPAWNFLAG_NOTOUCH = 1;
void(entity client, float density, vector color, float transitionTime = 0) MG_SetFog = {
    client.MG_fog_valid = TRUE;
    client.fog_density = density;
    client.fog_color = color;
    // Ironwail filters server-stuffed console commands. Its reliable fog message is client-specific.
    entity oldmsg = msg_entity;
    msg_entity = client;
    WriteByte(MSG_ONE, 41); // svc_fog: density/R/G/B bytes and hundredths-of-a-second short
    WriteByte(MSG_ONE, floor(clamp(density, 0, 1) * 255 + 0.5));
    WriteByte(MSG_ONE, floor(clamp(color_x, 0, 1) * 255 + 0.5));
    WriteByte(MSG_ONE, floor(clamp(color_y, 0, 1) * 255 + 0.5));
    WriteByte(MSG_ONE, floor(clamp(color_z, 0, 1) * 255 + 0.5));
    WriteShort(MSG_ONE, floor(clamp(transitionTime, 0, 327.67) * 100 + 0.5));
    msg_entity = oldmsg;
}

entity(entity e, .string field = fog_info_entity) MG_GetFogInfoEntity = {
	if(e.field != "")
	{
		e = find(world, targetname, e.field);
		if(e.classname == "info_fog")
		{
			return e;
		}
	}
	return world;
}

float(entity client, entity source, float transitionTime) MG_FogPushSettingsFrom = {
	if(client.classname != "player") return FALSE;
	source = MG_GetFogInfoEntity(source);
	if(!source) return FALSE;
	if(source.fog_density || source.fog_color || (source.spawnflags & MG_FORCE_SET_FOG))
	{
		MG_SetFog(client, source.fog_density, source.fog_color, transitionTime);
		return TRUE;
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

/*QUAKED info_fog (0.5 .5 .8) (-8 -8 -8) (8 8 8)
Fog value definition
wait: fog density
dest: fog color
*/

void() info_fog = {
    if(!MG_WorldGuard()) return;
	if(!self.fog_density) self.fog_density = 0.05;
	if(self.fog_color_x > 1.0 || self.fog_color_y > 1.0 || self.fog_color_z > 1.0) //Not in 0..1 range?
	{
		self.fog_color = self.fog_color * (1.0 / 255);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

void() MG_trigger_fog_activate = {
    if(self == other.fog_active_fog_trigger) return;
	if (other.classname != "player") // Yoder add, 27/09/2020 to fix crash
		return;
	other.fog_active_fog_trigger = self;
	MG_FogPushSettingsFrom(other, self, self.delay);
}

/*QUAKED trigger_fog (0.5 .5 .8) (? ? ?) (? ? ?)
Trigger to transition fog
fog_info_entity: info_fog that contains values this trigger should use.
delay: time to fade the transition over
*/
void() MG_trigger_fog_use = {
    entity oother = other;
    other = activator;
    MG_trigger_fog_activate();
    other = oother;
};

void() trigger_fog = {
    if(!MG_WorldGuard()) return;
	// Fog is set on the server, so transitions and triggers work really bad in multiplayer. 
	// Just remove ourself if we're in multiplayer.
	if(coop || deathmatch) 
	{
		remove(self);
		return;
	}

	if(!self.delay) self.delay = 0.5;	
	self.use = MG_trigger_fog_use;
    if ( !(self.spawnflags & MG_SPAWNFLAG_NOTOUCH) )
    {
        self.touch = MG_trigger_fog_activate;
    }
    InitTrigger ();
}


//////////////////////////////////////////////////////////////////////////////////////////////////////

float(entity pl) MG_trigger_fog_transition_touch_get_tween = {
	vector t = pl.origin - (self.origin + self.mins);
	vector b = self.size;
	float tween = 0;

	if(self.style == 0)
	{
		if(b_x) tween = t_x / b_x;
	}
	else if(self.style == 1)
	{
		if(b_y) tween = t_y / b_y;
	}
	else if(self.style == 2)
	{
		if(b_z) tween = t_z / b_z;
	}

	if(tween < 0) tween = 0;
	if(tween > 1) tween = 1;

	return tween;
}

void() MG_trigger_fog_transition_touch = {
	if(other.classname != "player") return;
	
	float tween = MG_trigger_fog_transition_touch_get_tween(other);
	
	entity fog1 = MG_GetFogInfoEntity(self);
	entity fog2 = MG_GetFogInfoEntity(self, target);

	float density = ((1 - tween) * fog1.fog_density) + (tween * fog2.fog_density);
	vector color = ((1 - tween) * fog1.fog_color) + (tween * fog2.fog_color);

	MG_SetFog(other, density, color);
}

/*QUAKED trigger_fog_transition (0.5 .5 .8) (? ? ?) (? ? ?)
Trigger to transition fog between two sides.
fog_info_entity: info_fog that contains values this trigger should use on the 'left' side.
target: info_fog that contains values this trigger should use on the 'right' side.
style: Which axis this trigger fades across. 0 = X, 1 = Y, 2 = Z
*/
void() trigger_fog_transition = {
    if(!MG_WorldGuard()) return;
	// Fog is set on the server, so transitions and triggers work really bad in multiplayer. 
	// Just remove ourself if we're in multiplayer.
	if(coop || deathmatch) 
	{
		remove(self);
		return;
	}

	if(self.style < 0 || self.style > 2)
	{
		objerror("Invalid style for trigger_fog_transition. Needs to be 0, 1 or 2.");
	}
	self.touch = MG_trigger_fog_transition_touch;
    InitTrigger ();
}

