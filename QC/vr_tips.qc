// vr_tips.qc -- tips for new players placed in a map (func_vr_tip).
//
// The server owns the list (the vr_tip_* builtins; Quake/vr/vr_tips.cpp): it is sent to every client, and the whole
// list to each client as it spawns. A tip shows as a small CRT screen floating near the player, with a cable to what
// it is about, when he comes within its distance of it: once, unless it repeats. What it is about is a point in the
// map, or another entity, which the client follows live.

float QVR_TIP_REPEAT = 1;   // shown every time you come near, not remembered (func_vr_tip's REPEAT)
float QVR_TIP_HOLOGRAM = 2; // in the wrist gadget's hologram, not the floating screen (HOLOGRAM)
float QVR_TIP_ANYANGLE = 4; // shown even out of the view angle or hidden from the eyes (ANYANGLE)
float QVR_TIP_WAITING = 8;  // not shown until used (TRIGGERED, until its targetname is fired)

// The entity `tip` follows: the one its `target` names (world: none, it stays at its own origin). Its targetname is
// only its name: cleared as it spawns (func_vr_tip), so a teleporter's destination or a monster's path looked up by
// that name never finds the tip.
entity(entity tip) VR_Tip_FollowTarget =
{
    if(tip.target == NULLSTR)
        return world;
    return find(world, targetname, tip.target);
}

// A tip whose target is placed after it in the map looks for it again in the map's first frame (its record is already
// sent: the engine updates the client's copy). The first frame, not later: a saved game is loaded over a freshly spawned
// map after its two first frames, so a later retry would never run there (its think is not in the save's edict).
// A tip without a target whose targetname another entity has was made for the old rule (a tip followed what its
// targetname named): it stays at its own origin now, and says so to the mapper (developer 1).
void() VR_Tip_Retry =
{
    entity e;
    if(self.target != NULLSTR)
    {
        e = VR_Tip_FollowTarget(self);
        if(e != world)
            vr_tip_setentity(self.tiphandle, e);
        return;
    }

    e = find(world, targetname, self.tip_targetname);
    if(e != world)
        dprint(sprintf("func_vr_tip at %v: its targetname \"%s\" names a %s, which it no longer follows (a tip "
                       "follows only its target): set its target to \"%s\" to follow it\n",
            self.origin, self.tip_targetname, e.classname, self.tip_targetname));
}

// Its engine flags (QVR_TIP_*): its spawnflags', and WAITING while a TRIGGERED one has not been used.
float(entity xTip) VR_Tip_Flags =
{
    float flags = 0;
    if(xTip.spawnflags & 1)
        flags = flags + QVR_TIP_REPEAT;
    if(xTip.spawnflags & 2)
        flags = flags + QVR_TIP_HOLOGRAM;
    if(xTip.spawnflags & 4)
        flags = flags + QVR_TIP_ANYANGLE;
    if((xTip.spawnflags & 8) && !xTip.tip_on)
        flags = flags + QVR_TIP_WAITING;
    return flags;
};

// A TRIGGERED tip used: shown from now on (when he comes near, as any tip).
void() VR_Tip_Use =
{
    if(self.tip_on)
        return;
    self.tip_on = TRUE;
    vr_tip_setflags(self.tiphandle, VR_Tip_Flags(self));
};

// A saved game loaded: its tips were made afresh as the map spawned (WAITING); the ones used before are shown again.
void() VR_Tips_Reapply =
{
    for(entity e = find(world, classname, "func_vr_tip"); e; e = find(e, classname, "func_vr_tip"))
    {
        if(e.tip_on)
            vr_tip_setflags(e.tiphandle, VR_Tip_Flags(e));
    }
};

/*QUAKED func_vr_tip (0.2 0.7 0.9) (-8 -8 -8) (8 8 8) REPEAT HOLOGRAM ANYANGLE TRIGGERED
Quake VR: a tip for new players, placed by hand. A small CRT screen floating near the player, with a cable to what it
is about, shown when he comes within `distance` of it: the first time only, unless REPEAT or the map's worldspawn
_vr_tips_repeat (the worldspawn's _vr_tips_reset_on_start: once in each play of the map from its start). "message": its text (\n starts a new line). "distance": how near in units (0: the player's own
setting). "target": another entity's targetname — the tip follows that entity, live, instead of staying at its own
origin. "targetname": only the tip's own name (when it has no tipname): nothing finds the tip by it, and it is not what
the tip follows (a map made when it was gets a developer warning). "tipname": names this tip (its name in
tips_seen.txt; use it when two tips follow the same thing, or to try it with vr_tips_test). "tip_size": its text
size (0: the player's setting). "tip_delay": seconds he must stay near before it shows (0: at once; -1 or unset: the
player's setting). REPEAT: shown every time he comes near. HOLOGRAM: in the wrist gadget's hologram instead of the
floating screen. ANYANGLE: shown even out of the view angle or hidden from his eyes. TRIGGERED: not shown until its "targetname" is
fired (a monster's death, a crate broken...); then as any tip (the tutorial's "take its rifle").
*/
void() func_vr_tip =
{
    if(self.message == NULLSTR)
    {
        dprint("func_vr_tip: no message\n");
        remove(self);
        return;
    }

    self.movetype = MOVETYPE_NONE;
    self.solid = SOLID_NOT;

    float flags = VR_Tip_Flags(self);
    // (The worldspawn's _vr_tips_repeat, every tip in the map repeating, is the engine's: QC never sees '_' keys.)

    float h = vr_tip_make();
    self.tiphandle = h;
    vr_tip_settext(h, self.message);
    vr_tip_setpos(h, self.origin);
    vr_tip_setflags(h, flags);

    if(self.tipname != NULLSTR)
        vr_tip_setname(h, self.tipname);
    else if(self.targetname != NULLSTR)
        vr_tip_setname(h, self.targetname);

    if(self.spawnflags & 8)
    {
        // TRIGGERED: its targetname is what shows it (kept), not a name nor a thing to follow
        self.use = VR_Tip_Use;
        if(self.distance != 0)
            vr_tip_setdistance(h, self.distance);
        if(self.tip_size != 0)
            vr_tip_setsize(h, self.tip_size);
        if(self.tip_delay != NULLSTR && stof(self.tip_delay) >= 0)
            vr_tip_setdelay(h, stof(self.tip_delay));
        entity t = VR_Tip_FollowTarget(self);
        if(t != world)
            vr_tip_setentity(h, t);
        else if(self.target != NULLSTR)
        {
            self.nextthink = time + 0.1;
            self.think = VR_Tip_Retry;
        }
        return;
    }

    // Its targetname only names it (above): nothing finds the tip by it (VR_Tip_FollowTarget).
    self.tip_targetname = self.targetname;
    self.targetname = NULLSTR;

    if(self.distance != 0)
        vr_tip_setdistance(h, self.distance);
    if(self.tip_size != 0)
        vr_tip_setsize(h, self.tip_size);
    // tip_delay: 0 shows it at once; below 0 (-1), or no key (a map made when 0 meant the player's), his vr_tips_delay.
    if(self.tip_delay != NULLSTR && stof(self.tip_delay) >= 0)
        vr_tip_setdelay(h, stof(self.tip_delay));

    entity e = VR_Tip_FollowTarget(self);
    if(e != world)
        vr_tip_setentity(h, e);
    else if(self.target != NULLSTR || self.tip_targetname != NULLSTR)
    {
        self.nextthink = time + 0.1;
        self.think = VR_Tip_Retry;
    }
};
