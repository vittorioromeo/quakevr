/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

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

#pragma once

#include "vid.hpp"
#include "quakeglm_qvec3.hpp"

struct refdef_t
{
    vrect_t vrect;               // subwindow in video for refresh
                                 // FIXME: not need vrect next field here?
    vrect_t aliasvrect;          // scaled Alias version
    int vrectright, vrectbottom; // right & bottom screen coords
    int aliasvrectright, aliasvrectbottom; // scaled Alias versions
    float vrectrightedge;           // rightmost right edge we care about,
                                    //  for use in edge list
    float fvrectx, fvrecty;         // for floating-point compares
    float fvrectx_adj, fvrecty_adj; // left and top edges, for clamping
    int vrect_x_adj_shift20;        // (vrect.x + 0.5 - epsilon) << 20
    int vrectright_adj_shift20;     // (vrectright + 0.5 - epsilon) << 20
    float fvrectright_adj, fvrectbottom_adj;
    // right and bottom edges, for clamping
    float fvrectright;           // rightmost edge, for Alias clamping
    float fvrectbottom;          // bottommost edge, for Alias clamping
    float horizontalFieldOfView; // at Z = 1.0, this many X is visible
                                 // 2.0 = 90 degrees
    float xOrigin;               // should probably allways be 0.5
    float yOrigin;               // between be around 0.3 to 0.5

    qvec3 vieworg;
    qvec3 viewangles;
    qvec3 aimangles;

    float fov_x, fov_y;

    int ambientlight;
};

extern refdef_t r_refdef;
