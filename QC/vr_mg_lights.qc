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
const string LightStyles[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"};
const float LightStyles_MAX = LightStyles.length - 1;
const string LightStylesHalf[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m"};
const float LightStylesHalf_MAX = LightStylesHalf.length - 1;


//============================================================================
// No special functionality, just to hold dynamic shadowcasting lights for KEX
const float DYNAMICLIGHT_NOT_IN_COOP = 1;
/*QUAKED dynamiclight (1 .8 .4) (-8 -8 -8) (8 8 8)
KEX rendering marker: authored shadow metadata, no gameplay callback.
*/
void() dynamiclight = {
    if(!MG_WorldGuard()) return;
    if(coop && self.spawnflags & DYNAMICLIGHT_NOT_IN_COOP)
    {
        remove(self);
    }
}

//============================================================================

void() MG_InitLightStyles = {
    //
    // Setup light animation tables. 'a' is total darkness, 'z' is maxbright.
    //

    // 0 normal
    lightstyle(0, "m");
        
    // 1 FLICKER (first variety)
    lightstyle(1, "mmnmmommommnonmmonqnmmo");

    // 2 SLOW STRONG PULSE
    lightstyle(2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba");

    // 3 CANDLE (first variety)
    lightstyle(3, "mmmmmaaaaammmmmaaaaaabcdefgabcdefg");

    // 4 FAST STROBE
    lightstyle(4, "ma");

    // 5 GENTLE PULSE 1
    lightstyle(5,"jklmnopqrstuvwxyzyxwvutsrqponmlkj");

    // 6 FLICKER (second variety)
    lightstyle(6, "nmonqnmomnmomomno");

    // 7 CANDLE (second variety)
    lightstyle(7, "mmmaaaabcdefgmmmmaaaammmaamm");

    // 8 CANDLE (third variety)
    lightstyle(8, "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa");

    // 9 SLOW STROBE (fourth variety)
    lightstyle(9, "aaaaaaaazzzzzzzz");

    // 10 FLUORESCENT FLICKER
    lightstyle(10, "mmamammmmammamamaaamammma");

    // 11 SLOW PULSE NOT FADE TO BLACK
    lightstyle(11, "abcdefghijklmnopqrrqponmlkjihgfedcba");

    // New styles under here if needed:

    // styles 32-62 are assigned by the light program for switchable lights

    // 63 testing
    lightstyle(63, "a");    
}

//============================================================================

string(float frac) MG_LightFractionToStyle = {
    if(frac <= 0) return LightStyles[0];
    if(frac >= 1) return LightStyles[LightStyles_MAX];
    frac*= LightStyles.length;
    frac = floor(frac);
    return LightStyles[frac];
}

string(float frac) MG_LightFractionToStyleHalf = {
    if(frac <= 0) return LightStylesHalf[0];
    if(frac >= 1) return LightStylesHalf[LightStylesHalf_MAX];
    frac*= LightStylesHalf.length;
    frac = floor(frac);
    return LightStylesHalf[frac];
}

enum : int
{
    LightRampStateOff,
    LightRampStateFadeUp,
    LightRampStateOn,
    LightRampStateFadeDown
};

float LIGHTRAMP_FULLRANGE = 1;

void(float style, float frac) MG_target_lightramp_setlight = {
    if(self.spawnflags & LIGHTRAMP_FULLRANGE)
        lightstyle(style, MG_LightFractionToStyle(frac));
    else
        lightstyle(style, MG_LightFractionToStyleHalf(frac));
}

void(float dt) MG_target_lightramp_tick = {
    float diff = dt * self.delay;
    switch(self.state)
    {
        case LightRampStateFadeDown:
            self.cnt-= diff;
            break;
            
        case LightRampStateFadeUp:
            self.cnt+= diff;
            break;
    }
    MG_target_lightramp_setlight(self.owner.style, self.cnt);
    if(self.cnt >= 1)
    {
        self.cnt = 1;
        self.state = LightRampStateOn;
        MG_RemoveFrameTickEntity(self);
    }
    if(self.cnt <= 0)
    {
        self.cnt = 0;
        self.state = LightRampStateOff;
        MG_RemoveFrameTickEntity(self);
    }

}

void() MG_target_lightramp_use = {
    switch(self.state)
    {
        case LightRampStateFadeDown:
            self.state = LightRampStateFadeUp;
            return;

        case LightRampStateOff:
            self.state = LightRampStateFadeUp;
            MG_RegisterFrameTickEntity(self);
            return;

        case LightRampStateOn:
            self.state = LightRampStateFadeDown;
            MG_RegisterFrameTickEntity(self);
            return;
        
        case LightRampStateFadeUp:
            self.state = LightRampStateFadeDown;
            return;
    }
}
void() MG_target_lightramp_init = {
    self.think = SUB_Null;
    if(!self.target) objerror("\n\ntarget_lightramp with no target.\n\n");
    if(!self.targetname) objerror("\n\ntarget_lightramp with no targetname.\n\n");
    
    self.owner = find(world, targetname, self.target);
    if(!self.owner) objerror("\n\ntarget_lightramp with unmatched target.\n\n");

    if(self.owner.spawnflags & START_OFF)
    {
        self.state = LightRampStateOff;
        self.cnt = 0;
    }
    else
    {
        self.state = LightRampStateOn;
        self.cnt = 1;
    }
    
    self.use = MG_target_lightramp_use;
    self.MG_tick = MG_target_lightramp_tick;
}

void() target_lightramp = {
    if(!MG_WorldGuard()) return;
    
    if(!self.delay) self.delay = 1;
    self.delay = 1 / self.delay;

    self.think = MG_target_lightramp_init;
    self.nextthink = time + 0.1;
}

/*QUAKED light_flame_gas (0 1 0) (-10 -10 -40) (10 10 40) START_OFF
Gas flare (spawns two entites)
*/
void() light_flame_gas =
{
    precache_model ("progs/flame3.mdl");
	setmodel (self, "progs/flame3.mdl");
    self.alpha = 0.6;

    entity s = spawn();
    setorigin(s, self.origin);
    setmodel (s, "progs/flame3.mdl");
    s.alpha = 0.4;
    s.frame = 1;
    s.angles = self.angles;
    VR_FireSpot(0);
};

