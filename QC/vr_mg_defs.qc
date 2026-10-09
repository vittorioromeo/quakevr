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
// Native official environment framework. Helpers are namespaced; mapper keys retain their authored names.
// Source snapshot and license: docs/vr-port/OFFICIAL_QC_SOURCE.md, QC/COPYING-rerelease.txt.
.float mapversion;
.float MG_registered;
.void(float dt) MG_tick;
.float MG_fog_valid;
.float fog_density;
.vector fog_color;
.entity fog_active_fog_trigger;
.string fog_info_entity;
.string endtext;
float() MG_WorldCampaign = { return cvar("vr_campaign") >= 3 && cvar("vr_campaign") <= 5; };
float() MG_WorldGuard = { if(MG_WorldCampaign()) return TRUE; remove(self); return FALSE; };
void(entity ent) MG_RegisterFrameTickEntity = { ent.MG_registered = TRUE; };
void(entity ent) MG_RemoveFrameTickEntity = { ent.MG_registered = FALSE; };
// Scan registered entities: ordinary QC remove/killtarget cannot leave a dangling linked-list node.
// QVR: the unregistered ones are skipped in the engine (findflags), the next entity still taken before each tick and
// tested after it, as the plain nextent walk did: the same ticks in the same order (MG3.md M3-28: 1800 entities a
// frame walked in QuakeC on secret2).
void() MG_WorldFrame = {
    if(!MG_WorldCampaign()) return;
    entity oself = self;
    entity ent = nextent(world);
    if(ent && !ent.MG_registered) ent = findflags(ent, MG_registered, 1);
    while(ent) {
        self = ent;
        ent = nextent(ent);
        if(self.MG_registered && self.MG_tick) self.MG_tick(frametime);
        if(ent && !ent.MG_registered) ent = findflags(ent, MG_registered, 1);
    }
    self = oself;
};
float(float value, float modulus) MG_mod = { return value - floor(value / modulus) * modulus; };
float(float degrees) MG_sin = { return sin(degrees * 0.01745329252); };
float(float degrees) MG_cos = { return cos(degrees * 0.01745329252); };
