// vr_ropes.qc -- climbable ropes and chains (docs/vr-port/ROPES.md). The entities only hold their settings: the engine
// (vr_climbrope.cpp) finds them, hangs each as a chain of points from its origin, steps it (it swings, falls, lies on the
// floor, is pushed by hands, bodies, props, explosions and shots), sends it to every client and draws it.

void(float chain) VR_Rope_Setup =
{
    if(chain)
    {
        self.rope_style = 1;
    }
    if(self.spawnflags & 1)
    {
        self.rope_pinned = 1;
    }
    self.solid = SOLID_NOT;
    self.movetype = MOVETYPE_NONE;
    self.modelindex = 0;
    // (ropes8c) A chain's links stop shots, nails and blows (the engine's SV_Move: vr_climbrope.cpp chainClip, the chain
    // the trace's entity): it takes damage so that they hit it (sparks off its metal: VR_MaterialOf), never dealt any
    // (T_DamageImpl). Ropes and poles too (the toolgun styles a rope after this; a pole's own boxes are what is met).
    self.takedamage = DAMAGE_YES;
    setorigin(self, self.origin);
};
// (VR_Rope_IsChain, VR_Rope_Is: vr_util.qc, before combat.qc's and vr_crates.qc's calls.)

/*QUAKED vr_rope (0.55 0.4 0.2) (-4 -4 -4) (4 4 4) PINNED
Quake VR: a climbable rope, hanging from its origin (put it just under a ceiling or a beam). A physical rope: it swings,
falls and lies on the floor where it is longer than the drop, and is pushed by hands, players, monsters, props, explosions
and shots. "rope_length": its length in units (256); "rope_thickness": units across (1.5); "rope_segments": its pieces
(one every 16 units); "rope_mass": kg, the whole rope (5); "rope_stiffness": 0.05-1, how little it stretches (1);
"rope_damping": per second, how fast its swing dies down (0.5); "rope_style": 1 drawn as a chain; "target": its bottom
pinned at that entity's origin (an info_notnull); PINNED: its bottom pinned where it first hangs. (ropes7) "rope_end": its
bottom pinned at that point instead of a target; with either, "rope_slack": its length over the straight line between its
ends (0.1: 10% longer; unset with no rope_length: taut). Strung level or slanted past vr_rope_monkey_angle from upright,
it is traversed hanging under it, hand over hand (a monkey traverse).
*/
void() vr_rope =
{
    VR_Rope_Setup(0);
};

/*QUAKED vr_chain (0.4 0.4 0.45) (-4 -4 -4) (4 4 4) PINNED
Quake VR: a climbable chain, hanging from its origin: a vr_rope drawn as a chain of iron links (heavier by default: 20 kg),
with the same keys and the same behaviour.
*/
void() vr_chain =
{
    VR_Rope_Setup(1);
};

/*QUAKED vr_pole (0.5 0.35 0.2) (-4 -4 -4) (4 4 4)
Quake VR (ropes7): a climbing pole, standing from its origin (its top) down to the floor, or its "rope_length" down, or
to its "target" (an info_notnull) or "rope_end" point (a slanted or level pole: past vr_rope_monkey_angle from upright
it is traversed hanging under it). It never moves (but with a mover), climbed as a rope is: hand over hand, the legs
round it. "rope_thickness": units across (3); "rope_style": 1 metal (else wood: its look, sounds and haptics).
*/
void() vr_pole =
{
    VR_Rope_Setup(0);
};

// (ropes8a) A climbing pole's collision box: the engine (vr_climbrope.cpp placeSolids) makes one or more along each pole,
// sizes them, keeps them with it and frees them with it; "goalentity" its pole, "rope_style" 1 metal. Solid to bodies
// (players, monsters; the climber on it passes: its owner), met by shots, nails and blows (takedamage, never dealt any:
// T_DamageImpl), which throw sparks off metal and splinters off wood (VR_MaterialOf). The navmesh cuts it out
// (vr_nav.cpp, its props' obstacles).
void() vr_pole_solid =
{
    self.solid = SOLID_BBOX;
    self.movetype = MOVETYPE_NONE;
    self.takedamage = DAMAGE_YES; // (not DAMAGE_AIM: nothing aims at it)
    self.modelindex = 0;
    setorigin(self, self.origin);
};
