/*
Copyright (C) 2020-2026 Vittorio Romeo and Quake VR contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

// vr_profile.h -- the engine's (C) side of the Quake VR profiler (vr/vr_profile.cpp): named scopes
// timed on the CPU and, for the GPU variant, with OpenGL timestamp queries. With vr_profile 0 each
// call only tests a flag. Scopes nest (a call tree); names must be string literals. The C++ side
// is vr_profile.hpp (QVR_PROFILE / QVR_GPU_PROFILE).

#ifndef QVR_VR_PROFILE_H
#define QVR_VR_PROFILE_H

#ifdef __cplusplus
extern "C" {
#endif

void VR_ProfileFrame (void);				// start of _Host_Frame: ends the last frame, begins this one
void VR_ProfileFrameEnd (void);				// end of _Host_Frame: what follows (the frame rate cap's sleep) is idle
void VR_ProfileBegin (const char *name);	// a CPU scope
void VR_ProfileBeginGPU (const char *name);	// a CPU and GPU scope (the main thread, with the GL context)
void VR_ProfileEnd (void);					// ends the innermost scope
extern int vr_glpass_trace;					// vr_gl_passes: this frame's draw framebuffer binds and alias batches are being noted
void VR_GLPassBind (unsigned fbo);			// gl_vidsdl.c's bind shadow, while vr_glpass_trace: a draw framebuffer bound
void VR_GLPassBreak (const char *what);		// gl_vidsdl.c's shadows, while vr_glpass_trace: a call a tiler may end its render pass at (qfov3)
void VR_GLAliasBatch (const char *model, int instances, int draws0, int depth, int ent, int skin, int form, int needs, int tris); // R_FlushAliasInstances, while vr_glpass_trace (form: 0 depth only, 1 full, 2 opaque, 3 plain; needs: VR_AliasPlainNeeds; tris: an instance's)

// GL debug groups (vr_gl_markers, latched at each frame's start into vr_gl_markers_on): every GPU scope is also one,
// named as its profile scope; these open one alone (the main thread; balanced as scopes, each pops what its begin
// pushed). VR_GPUZoneBegin/End: the profiling builds' own Tracy GPU zone (vr_tracy.h) and the debug group, one name.
extern int vr_gl_markers_on;
void VR_GLMarkerBegin (const char *name);
void VR_GLMarkerEnd (void);
#define VR_GPUZoneBegin(name) (VR_TracyBegin (name, 1), VR_GLMarkerBegin (name))
#define VR_GPUZoneEnd() (VR_GLMarkerEnd (), VR_TracyEnd ())

// Latched at each frame's start: whether the profiler collects this frame (vr_profile, the panel or the CSV capture),
// and whether it also times every trace and builtin call (vr_profile_detail 2). The hot paths (each QuakeC call, each
// trace) test these before calling in: with profiling off they cost a load and a branch.
extern int vr_profile_on;
extern int vr_profile_fine;
extern int vr_profile_inqc; // inside a timed PR_ExecuteProgram (only the outermost call is a "quakec" scope)

// Counts, added to always (a plain increment), taken and cleared at each frame's start (vr_profile_report).
typedef struct
{
	int traces;		// SV_Move calls
	int hullchecks;	// hull traces (SV_ClipMoveToEntity, the VR code's own traces against the world)
	int drawcalls;	// OpenGL draw calls (glDraw*, one per multi-draw)
	int aliasdrawn;	// alias models drawn: instances, in every view (the shadow maps draw theirs apart, uncounted)
	int aliastris;	// their triangles, every view and pass (rs_aliaspolys counts only under r_speeds, never cleared else)
	int worldleaves;	// world leaves in each view's PVS, summed (the GPU culls the world's surfaces: no CPU polygon count)
} vr_profcounts_t;
extern vr_profcounts_t vr_profcounts;

// The perf recorder (vr/vr_perfrec.hpp, vr_perf_record): the events of a frame that may make it a hitch, each with its
// time (ms) and a name (copied; may be NULL). vr_perf_recording: latched at each frame's end; the call sites test it
// before timing anything, so with the recorder off an event costs a load and a branch.
enum
{
	VR_PERF_SHADER,		// a program compiled and linked (no binary in the program cache)
	VR_PERF_SHADERBIN,	// a program made from its binary (the program cache)
	VR_PERF_TEXTURE,	// a texture processed and uploaded
	VR_PERF_MODEL,		// a model loaded
	VR_PERF_LIGHTMAP,	// a runtime brush's lightmap uploaded
	VR_PERF_SOUND,		// a sound loaded
	VR_PERF_MAP,		// a map's load (its command)
	VR_PERF_SAVE,		// a save written
	VR_PERF_DECAL,		// decal bakes (the permanent layer's, its atlas grown)
	VR_PERF_NAV,		// a navmesh job landing (and the wait for it)
	VR_PERF_QC,			// the QuakeC (the outermost calls, summed)
	VR_PERF_PHYS,		// the Box3D steps
	VR_PERF_WORK,		// other timed work (VR_TimeAdd's kinds)
	VR_PERF_MARK,		// vr_perf_mark <label>: a script's or a tester's mark (an A/B's parts)
	VR_PERF_KINDS
};
extern int vr_perf_recording;
void VR_PerfEvent (int kind, double ms, const char *name);

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_PROFILE_H
