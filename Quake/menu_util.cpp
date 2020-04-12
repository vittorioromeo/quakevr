#include "menu_util.hpp"

#include "quakeglm.hpp"
#include "client.hpp"

// TODO VR: (P2) forward declaration due to crappy quake header deps

struct sfx_t;

sfx_t* S_PrecacheSound(const char* name);

void S_StartSound(int entnum, int entchannel, sfx_t* sfx,
    const glm::vec3& origin, float fvol, float attenuation);

namespace quake::menu_util
{
    void playMenuSound(const char* sound, float fvol)
    {
        if(sfx_t* const sfx = S_PrecacheSound(sound))
        {
            S_StartSound(cl.viewentity, 0, sfx, vec3_zero, fvol, 1);
        }
    }

    void playMenuDefaultSound()
    {
        playMenuSound("items/r_item1.wav", 0.5);
    }

    void setMenuState(m_state_e state)
    {
        IN_Deactivate(modestate == MS_WINDOWED);
        key_dest = key_menu;
        m_state = state;
        m_entersound = true;

        playMenuDefaultSound();
    }

    void setMenuState(quake::menu& m, m_state_e state)
    {
        setMenuState(state);
        m.enter();
    }
} // namespace quake::menu_util
