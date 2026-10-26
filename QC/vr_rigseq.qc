// vr_rigseq.qc -- the Rig Studio's sequences played by monsters (the engine's vr_rigseq.cpp; docs/vr-port/RIGSTUDIO.md,
// "QC"): a rigged monster plays a new keyed animation made in the studio (quakevr/anims/<model>/<name>.anim.txt) over
// the frame it holds, blended in from what it was doing and back out, its events sent here as its frames pass them.
//
// No builtin: the engine reads and writes these fields at each server frame's end (the builtin numbers are full).
// - VR_RigSeq_Play(m, "taunt") asks for it (the field .vr_rigseq; "-" stops it); the engine takes the request at the
//   frame's end: .vr_rigseq_len its length then (seconds; -1: the model has no such sequence), .vr_rigseq_frame its
//   frame each frame it plays (-1 when none).
// - VR_RigSeq_Event(seq, ev, frame) is called by the engine (self the monster) for each event of the sequence as its
//   frame passes it ("fire", "footstep", "release grip": the names the studio gave them), "end" at its end and
//   "stopped" when it is cut short (stopped, or with its Hold on, the monster's own frame changed: a pain). It calls the
//   monster's .vr_rigseq_on, if any.
// - The sequence holds the frame it starts on (its Hold, the default: its base frame set by the engine), so a monster
//   playing one keeps its think on a wait of its own (the armed grunt's taunt: vr_armedgrunt.qc).

.string vr_rigseq;      // a request: the sequence's name ("-": stop); the engine clears it once taken
.float vr_rigseq_len;   // the engine: the length (s) of the sequence started (-1: none such for its model)
.float vr_rigseq_frame; // the engine: its frame now (-1: none playing)
.float vr_rigseq_t0;    // when it was asked for (its start: the engine takes the request at the frame's end)
.void(string seq, string ev, float frame) vr_rigseq_on; // the monster's own handler of its events

// The Rig Studio's subject's aim target (vr_rigstudio.cpp writes them each server frame; 0 on every other entity): where
// it aims, as at an enemy standing there (vr_armedgrunt_pose.qc VR_AGK_Hold).
.vector vr_rs_aim;  // the point (world)
.float vr_rs_aimon; // 1: aimed at it

void(entity m, string seq) VR_RigSeq_Play =
{
    m.vr_rigseq = seq;
    m.vr_rigseq_t0 = time;
    m.vr_rigseq_len = 0; // (not known yet: the engine says at the frame's end)
    m.vr_rigseq_frame = -1;
};

void(entity m) VR_RigSeq_Stop =
{
    m.vr_rigseq = "-";
};

// The Rig Studio's subject gone (vr_rigstudio.cpp removeSubject: another subject or weapon picked, the studio closed or
// opened again): `m`, whatever it owns (its held weapons, vr_ag_weapon, with their records; its worn tank) and whatever
// those own, each as the scene's cleanup removes it (VR_Scene_Remove: a fire out, AD's attachments).
void(entity m) VR_RigStudio_Remove =
{
    if(m == world || (m.flags & FL_CLIENT))
    {
        return;
    }
    for(float pass = 0; pass < 2; pass++)
    {
        entity e = nextent(world);
        while(e)
        {
            entity o = e.owner;
            float mine = pass == 0 ? (o != world && o != m && o.owner == m) : o == m;
            if(mine && !(e.flags & (FL_CLIENT | FL_MONSTER)))
            {
                if(e.classname == "vr_ag_weapon" && e.weaponinst && e.weaponinst.classname == "weapon_inst")
                {
                    remove(e.weaponinst);
                    e.weaponinst = world;
                }
                VR_Scene_Remove(e);
            }
            e = nextent(e); // (after the removal: nextent skips the freed)
        }
    }
    VR_Scene_Remove(m);
};

// Called by the engine (vr_rigseq.cpp), self the monster.
void(string seq, string ev, float frame) VR_RigSeq_Event =
{
    if(self.vr_rigseq_on)
    {
        self.vr_rigseq_on(seq, ev, frame);
    }
};
