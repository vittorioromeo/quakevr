/*
 * cd_null.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "q_stdinc.hpp"
#include "cdaudio.hpp"
#include "console.hpp"

int CDAudio_Play(byte track, bool looping)
{
    (void)track;
    (void)looping;
    return -1;
}

void CDAudio_Stop()
{
}

void CDAudio_Pause()
{
}

void CDAudio_Resume()
{
}

void CDAudio_Update()
{
}

int CDAudio_Init()
{
    Con_Printf("CDAudio disabled at compile time\n");
    return -1;
}

void CDAudio_Shutdown()
{
}
