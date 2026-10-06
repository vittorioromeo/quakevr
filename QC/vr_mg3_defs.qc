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
// Dawn of the Machine (MG3, campaign 5): the native port's shared definitions. Helpers are MG3_-prefixed; mapper keys
// and classnames keep their authored names. Plan and task list: docs/vr-port/MG3_PLAN.md. Source snapshot and
// license: docs/vr-port/OFFICIAL_QC_SOURCE.md, QC/COPYING-rerelease.txt (quakec_mg3).
// Gated: vr_gamedir.cpp keeps MG3 nativeReady false (developer path: `vr_campaign_native mg3`).
float() MG3_Campaign = { return cvar("vr_campaign") == 5; };
