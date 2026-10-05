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


const float MISC_MODEL_ANIMATED = 1;
const float MISC_MODEL_ANIMATED_ONCE = 2;
const float MISC_MODEL_ANIMATED_START_OFF = 4;

///////////////////////////////
// Behaviour for a looping animation
///////////////////////////////

void() MG_misc_model_think_loop = {
    self.frame += 1;
    if(self.frame == self.cnt)
    {
        self.frame = self.count; // Back to start
    }
    self.nextthink = time + 0.1;
}

void() MG_misc_model_use_loop = {
    if(self.spawnflags & MISC_MODEL_ANIMATED_START_OFF)
    {
        self.spawnflags (-) MISC_MODEL_ANIMATED_START_OFF;
        self.think = MG_misc_model_think_loop;
        MG_misc_model_think_loop();
    }
    else
    {
        self.spawnflags (+) MISC_MODEL_ANIMATED_START_OFF;
        self.think = SUB_Null;
        self.nextthink = -1;
    }
    
}

///////////////////////////////
// Behaviour for a single animation
///////////////////////////////

void() MG_misc_model_think_once = {
    self.frame += 1;
    if(self.frame < self.cnt)
    {
        self.nextthink = time + 0.1;
    }
}

void() MG_misc_model_use_once = {
    self.frame = self.count;
    self.think = MG_misc_model_think_once;
    self.nextthink = time + 0.1;
}

///////////////////////////////

void() misc_model = {
    if(!MG_WorldGuard()) return;
    if(self.model == "") { objerror("misc_model with no model specified"); }
    precache_model(self.model);
    setmodel(self, self.model);
    setorigin(self, self.origin);
    self.solid = SOLID_NOT;

    if(self.spawnflags == 0)
    {
        makestatic(self);
        return;
    }

    if(self.cnt < self.frame) { objerror("misc_model with invalid frame range (cnt < frame)"); }
    if(self.targetname == "") { objerror("misc_model with no targetname"); }
    self.count = self.frame;
    
    if(self.spawnflags & MISC_MODEL_ANIMATED_ONCE)
    {
        self.use = MG_misc_model_use_once;
    }
    else if(self.spawnflags & (MISC_MODEL_ANIMATED | MISC_MODEL_ANIMATED_START_OFF))
    {
        self.use = MG_misc_model_use_loop;
        // Stupid way to do it but just flip the bit flag and pretend we just used it.
        self.spawnflags ^= MISC_MODEL_ANIMATED_START_OFF;
        MG_misc_model_use_loop();
    }

}

















