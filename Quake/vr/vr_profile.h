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
} vr_profcounts_t;
extern vr_profcounts_t vr_profcounts;

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_PROFILE_H
