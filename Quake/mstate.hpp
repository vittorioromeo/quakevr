/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

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

enum m_state_e
{
    m_none,
    m_main,
    m_singleplayer,
    m_load,
    m_save,
    m_multiplayer,
    m_setup,
    m_net,
    m_options,
    m_quakevrsettings,
    m_quakevrdevtools,
    m_quakevrchangemap,
    m_video,
    m_vr,
    m_vrgameplay,
    m_wpn_offset,
    m_sbar_offset,
    m_hotspot,
    m_vrtorso,
    m_map,
    m_debug,
    m_keys,
    m_help,
    m_quit,
    m_lanconfig,
    m_gameoptions,
    m_search,
    m_slist,
    m_qvrs_menu,
    m_qvrs_crosshair,
    m_qvrs_particle,
    m_qvrs_locomotion,
    m_qvrs_handguncalibration,
    m_qvrs_playercalibration,
    m_qvrs_melee,
    m_qvrs_aiming,
    m_qvrs_immersion,
    m_qvrs_graphical,
    m_qvrs_hudconfiguration,
    m_qvrs_hotspot,
    m_qvrs_torso,
    m_qvrs_changemap_vanilla,
    m_qvrs_changemap_soa,
    m_qvrs_changemap_doe,
    m_qvrs_changemap_dopa,
    m_qvrs_changemap_honey,
    m_qvrs_changemap_extra,
    m_qvrs_transparencyoptions,
    m_qvrdt_weaponconfiguration1,
    m_qvrdt_weaponconfiguration2,
    m_qvrdt_weaponconfiguration3,
    m_qvrdt_weaponconfiguration4,
    m_qvrdt_fingerconfiguration,
    m_qvrdt_debugutilities,
};

extern enum m_state_e m_state;
extern enum m_state_e m_return_state;

