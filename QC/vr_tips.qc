// vr_tips.qc -- tips for new players placed in a map (func_vr_tip).
//
// The server owns the list (the vr_tip_* builtins; Quake/vr/vr_tips.cpp): it is sent to every client, and the whole
// list to each client as it spawns. A tip shows as a small CRT screen floating near the player, with a cable to what
// it is about, when he comes within its distance of it: once, unless it repeats. What it is about is a point in the
// map, or another entity, which the client follows live.

float QVR_TIP_REPEAT = 1;   // shown every time you come near, not remembered (func_vr_tip's REPEAT)
float QVR_TIP_HOLOGRAM = 2; // in the wrist gadget's hologram, not the floating screen (HOLOGRAM)
float QVR_TIP_ANYANGLE = 4; // shown even out of the view angle or hidden from the eyes (ANYANGLE)

// The entity `tip` follows: the one named by its `target`, or, without one, by its `targetname` (a tip naming itself
// follows nothing: it stays at its own origin). world: nothing.
entity(entity tip) VR_Tip_FollowTarget =
{
    string name = tip.target;
    if(name == NULLSTR)
        name = tip.targetname;
    if(name == NULLSTR)
        return world;

    entity e = find(world, targetname, name);
    while(e != world && e == tip)
        e = find(e, targetname, name);
    return e;
}

// A tip whose target is placed after it in the map looks for it again in the map's first frame (its record is already
// sent: the engine updates the client's copy). The first frame, not later: a saved game is loaded over a freshly spawned
// map after its two first frames, so a later retry would never run there (its think is not in the save's edict).
void() VR_Tip_Retry =
{
    entity e = VR_Tip_FollowTarget(self);
    if(e != world)
        vr_tip_setentity(self.tiphandle, e);
}

/*QUAKED func_vr_tip (0.2 0.7 0.9) (-8 -8 -8) (8 8 8) REPEAT HOLOGRAM ANYANGLE
Quake VR: a tip for new players, placed by hand. A small CRT screen floating near the player, with a cable to what it
is about, shown when he comes within `distance` of it: the first time only, unless REPEAT or the map's worldspawn
_vr_tips_repeat. "message": its text (\n starts a new line). "distance": how near in units (0: the player's own
setting). "targetname" or "target": another entity's targetname — the tip follows that entity, live, instead of staying
at its own origin. "tipname": names this tip (its name in vr_tips_seen; use it when two tips follow the same thing, or
to try it with vr_tips_test). "tip_size": its text size (0: the player's setting). "tip_delay": seconds he must stay
near before it shows (unset: the player's setting). REPEAT: shown every time he comes near. HOLOGRAM: in the wrist
gadget's hologram instead of the floating screen. ANYANGLE: shown even out of the view angle or hidden from his eyes.
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

    float flags = 0;
    if(self.spawnflags & 1)
        flags = flags + QVR_TIP_REPEAT;
    if(self.spawnflags & 2)
        flags = flags + QVR_TIP_HOLOGRAM;
    if(self.spawnflags & 4)
        flags = flags + QVR_TIP_ANYANGLE;
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

    if(self.distance != 0)
        vr_tip_setdistance(h, self.distance);
    if(self.tip_size != 0)
        vr_tip_setsize(h, self.tip_size);
    if(self.tip_delay != 0)
        vr_tip_setdelay(h, self.tip_delay);

    entity e = VR_Tip_FollowTarget(self);
    if(e != world)
        vr_tip_setentity(h, e);
    else if(self.target != NULLSTR || self.targetname != NULLSTR)
    {
        self.nextthink = time + 0.1;
        self.think = VR_Tip_Retry;
    }
};
