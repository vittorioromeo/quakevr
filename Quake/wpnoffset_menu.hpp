#pragma once

enum class WpnOffsetMenuOpt
{
    OffHand,

    OffsetX,
    OffsetY,
    OffsetZ,
    Scale,
    Roll,
    Pitch,
    Yaw,

    Max
};

void M_Menu_WpnOffset_f(void);
void M_WpnOffset_Draw(void);
void M_WpnOffset_Key(int key);
