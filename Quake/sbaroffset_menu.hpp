#include "quakedef.hpp"
#include "vr.hpp"

#ifndef __R_SBAROFFSET_MENU_H
#define __R_SBAROFFSET_MENU_H

enum class SbarOffsetMenuOpt
{
    OffsetX,
    OffsetY,
    OffsetZ,
    Scale,
    Roll,
    Pitch,
    Yaw,

    Max
};

void M_Menu_SbarOffset_f(void);
void M_SbarOffset_Draw(void);
void M_SbarOffset_Key(int key);

#endif
