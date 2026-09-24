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

// vr_api.h -- the only interface between the Ironwail engine and the Quake VR module.
//
// Engine code calls into VR exclusively through the functions declared here, at call sites
// marked with a "QVR" comment. The implementation lives in Quake/vr/ (C++), which in turn may
// use any engine header through vr_engine.hpp.

#ifndef QVR_VR_API_H
#define QVR_VR_API_H

#ifdef __cplusplus
extern "C" {
#endif

// Host lifetime (host.c).
void VR_Init (void);		// client init, after CL_Init: registers cvars and commands
void VR_Shutdown (void);	// client shutdown, before video shutdown
void VR_BeginFrame (void);	// once per host frame, after input events and before console commands

// Queries.
int VR_IsActive (void);		// nonzero while vr_enabled is set and a backend session is running

#ifdef __cplusplus
}
#endif

#endif // QVR_VR_API_H
