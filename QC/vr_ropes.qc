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
    setorigin(self, self.origin);
};

/*QUAKED vr_rope (0.55 0.4 0.2) (-4 -4 -4) (4 4 4) PINNED
Quake VR: a climbable rope, hanging from its origin (put it just under a ceiling or a beam). A physical rope: it swings,
falls and lies on the floor where it is longer than the drop, and is pushed by hands, players, monsters, props, explosions
and shots. "rope_length": its length in units (256); "rope_thickness": units across (1.5); "rope_segments": its pieces
(one every 16 units); "rope_mass": kg, the whole rope (5); "rope_stiffness": 0.05-1, how little it stretches (1);
"rope_damping": per second, how fast its swing dies down (0.5); "rope_style": 1 drawn as a chain; "target": its bottom
pinned at that entity's origin (an info_notnull); PINNED: its bottom pinned where it first hangs.
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
