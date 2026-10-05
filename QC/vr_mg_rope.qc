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


vector tdist = '0 0 2048';

string misc_rope_mdl = "progs/ropex.mdl";

float(float cnt) MG_misc_rope_spawn_models = {
    if(cnt < 0) cnt = 0;
    if(cnt > 32) cnt = 32;
    if(cnt == self.count) return cnt;
    entity e;
    while(cnt < self.count)
    {
        // remove entities
        e = self.chain.chain;
        if(self.chain.classname == "misc_rope_segment")
        {
            remove(self.chain);
        }
        self.chain = e;
        self.count--;
    }
    while(cnt > self.count)
    {
        // spawn entities
        e = spawn();
        e.classname = "misc_rope_segment";
        e.solid = SOLID_NOT;
        e.takedamage = DAMAGE_NO;
        e.movetype = MOVETYPE_NONE;
        setsize(e, '-4 -4 0', '4 4 128');
        setmodel(e, misc_rope_mdl);
        e.frame = 3;
        e.skin = self.skin;
        e.chain = self.chain;
        self.chain = e;
        self.count++;
    }
    return cnt;
}

void(float dt) MG_misc_rope_tick = {
    vector top_pos;
    entity e;
    
    traceline(self.oldorigin, self.oldorigin + tdist, TRUE, self);
    top_pos = trace_endpos;
    traceline(self.oldorigin, self.oldorigin - tdist, TRUE, self);
    float dist = vlen(trace_endpos - top_pos);
    
    float num_segments = floor(dist / 32);
    
    float num_models = floor(num_segments / 4);
    num_models = MG_misc_rope_spawn_models(num_models);
    
    e = self.chain;
    while(num_models > 0)
    {
        setorigin(e, trace_endpos);
        trace_endpos += '0 0 128';
        e = e.chain;
        num_models--;
        num_segments -= 4;
    }
    
    setorigin(self, trace_endpos);
    self.frame = num_segments;
    vector sz = '4 4 32';
    sz_z = 32 * (num_segments + 1);
    setsize(self, '-4 -4 0', sz);
}

void() misc_rope = {
    if(!MG_WorldGuard()) return;
    precache_model(misc_rope_mdl);
    
    setmodel(self, misc_rope_mdl);
    
    self.oldorigin = self.origin;
    self.count = 0;
    self.solid = SOLID_NOT;
    self.takedamage = DAMAGE_NO;
    self.movetype = MOVETYPE_NONE;
    setsize(self, '-4 -4 0', '4 4 32');
    
    self.MG_tick = MG_misc_rope_tick;
    MG_RegisterFrameTickEntity(self);
}