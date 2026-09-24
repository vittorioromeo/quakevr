// vr_gadget.cpp -- see vr_gadget.hpp.
//
// The screen is drawn with Ironwail's own 2D functions (the status bar's pictures from gfx.wad,
// the console font) into an offscreen texture, on a canvas of its own: a virtual 240 x 150
// screen covering the whole texture. The panel (vr_panel.cpp) draws the texture over the model's
// screen in each eye, a frame later, as it does the rest of the HUD.

#include "vr_gadget.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"

#include <cstdio>
#include <cstring>

namespace qvr::gadget
{
namespace
{

constexpr int width = 240;  // the virtual screen, and the texture's size (at twice that)
constexpr int height = 150;

Pose current;
GLuint texture = 0;
GLuint fbo = 0;

struct Pics
{
    bool loaded{false};
    qpic_t* nums[2][11]{}; // [0] normal, [1] red (anum_); [10] is the minus sign
    qpic_t* faces[5]{};    // face5 (hurt) .. face1 (healthy)
    qpic_t* armor[3]{};
    qpic_t* ammo[4]{};
    qpic_t* keys[2]{};
    qpic_t* powerups[4]{}; // invisibility, invulnerability, suit, quad
    qpic_t* sigils[4]{};
};

Pics pics;

void loadPics()
{
    if(pics.loaded)
    {
        return;
    }
    pics.loaded = true;

    char name[32];
    for(int i = 0; i < 10; i++)
    {
        q_snprintf(name, sizeof(name), "num_%d", i);
        pics.nums[0][i] = Draw_PicFromWad(name);
        q_snprintf(name, sizeof(name), "anum_%d", i);
        pics.nums[1][i] = Draw_PicFromWad(name);
    }
    pics.nums[0][10] = Draw_PicFromWad("num_minus");
    pics.nums[1][10] = Draw_PicFromWad("anum_minus");
    for(int i = 0; i < 5; i++)
    {
        q_snprintf(name, sizeof(name), "face%d", 5 - i);
        pics.faces[i] = Draw_PicFromWad(name);
    }
    pics.armor[0] = Draw_PicFromWad("sb_armor1");
    pics.armor[1] = Draw_PicFromWad("sb_armor2");
    pics.armor[2] = Draw_PicFromWad("sb_armor3");
    pics.ammo[0] = Draw_PicFromWad("sb_shells");
    pics.ammo[1] = Draw_PicFromWad("sb_nails");
    pics.ammo[2] = Draw_PicFromWad("sb_rocket");
    pics.ammo[3] = Draw_PicFromWad("sb_cells");
    pics.keys[0] = Draw_PicFromWad("sb_key1");
    pics.keys[1] = Draw_PicFromWad("sb_key2");
    pics.powerups[0] = Draw_PicFromWad("sb_invis");
    pics.powerups[1] = Draw_PicFromWad("sb_invuln");
    pics.powerups[2] = Draw_PicFromWad("sb_suit");
    pics.powerups[3] = Draw_PicFromWad("sb_quad");
    for(int i = 0; i < 4; i++)
    {
        q_snprintf(name, sizeof(name), "sb_sigil%d", i + 1);
        pics.sigils[i] = Draw_PicFromWad(name);
    }
}

void ensureTarget()
{
    if(texture)
    {
        return;
    }

    glGenTextures(1, &texture);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, width * 2, height * 2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GL_GenFramebuffersFunc(1, &fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
}

void fill(float x, float y, float w, float h, float r, float g, float b)
{
    const float rgb[3] = {r, g, b};
    Draw_FillEx(x, y, w, h, rgb, 1.f);
}

void pic(float x, float y, qpic_t* p, float scale = 1.f)
{
    if(p)
    {
        Draw_SubPic(x, y, p->width * scale, p->height * scale, p, 0.f, 0.f, 1.f, 1.f, rgb_white, 1.f);
    }
}

// Big numbers, right-aligned in `digits` places of 24 x 24 (scaled).
void number(float x, float y, int value, int digits, bool red, float scale)
{
    char str[16];
    q_snprintf(str, sizeof(str), "%d", CLAMP(-99, value, 999));
    const int length = static_cast<int>(strlen(str));
    x += static_cast<float>(digits - length) * 24.f * scale;
    for(const char* c = str; *c; c++)
    {
        pic(x, y, pics.nums[red ? 1 : 0][*c == '-' ? 10 : *c - '0'], scale);
        x += 24.f * scale;
    }
}

void text(float x, float y, float size, const char* str)
{
    Draw_StringEx(x, y, size, str);
}

void layout()
{
    // A green-tinted screen with faint scanlines and a frame.
    fill(0.f, 0.f, width, height, 0.03f, 0.07f, 0.04f);
    for(int y = 0; y < height; y += 3)
    {
        fill(0.f, static_cast<float>(y), width, 1.f, 0.02f, 0.05f, 0.03f);
    }
    fill(0.f, 0.f, width, 2.f, 0.25f, 0.6f, 0.3f);
    fill(0.f, height - 2.f, width, 2.f, 0.25f, 0.6f, 0.3f);
    fill(0.f, 0.f, 2.f, height, 0.25f, 0.6f, 0.3f);
    fill(width - 2.f, 0.f, 2.f, height, 0.25f, 0.6f, 0.3f);

    GL_SetCanvasColor(0.45f, 1.f, 0.55f, 1.f);
    text(8.f, 6.f, 8.f, "RANGER STATUS");
    GL_SetCanvasColor(1.f, 1.f, 1.f, 1.f);
    fill(8.f, 16.f, width - 16.f, 1.f, 0.25f, 0.6f, 0.3f);

    // Face and health, armour.
    const int health = cl.stats[STAT_HEALTH];
    const int face = health >= 100 ? 4 : CLAMP(0, health / 20, 4);
    pic(8.f, 22.f, pics.faces[face]);
    number(36.f, 22.f, health, 3, health < 25, 1.f);

    const int armor = cl.stats[STAT_ARMOR];
    const int armorType = (cl.items & IT_ARMOR3) ? 2 : (cl.items & IT_ARMOR2) ? 1 : 0;
    if(armor > 0)
    {
        pic(128.f, 22.f, pics.armor[armorType]);
    }
    number(156.f, 22.f, armor, 3, false, 1.f);

    // Ammo: four columns, icon over count.
    const int ammo[4] = {cl.stats[STAT_SHELLS], cl.stats[STAT_NAILS], cl.stats[STAT_ROCKETS], cl.stats[STAT_CELLS]};
    for(int i = 0; i < 4; i++)
    {
        const float x = 14.f + i * 58.f;
        pic(x, 56.f, pics.ammo[i]);
        char count[8];
        q_snprintf(count, sizeof(count), "%3d", ammo[i]);
        GL_SetCanvasColor(0.45f, 1.f, 0.55f, 1.f);
        text(x - 3.f, 84.f, 10.f, count);
        GL_SetCanvasColor(1.f, 1.f, 1.f, 1.f);
    }

    // Keys, powerups and sigils in a row.
    float x = 12.f;
    const int keyBits[2] = {IT_KEY1, IT_KEY2};
    for(int i = 0; i < 2; i++)
    {
        if(cl.items & keyBits[i])
        {
            pic(x, 100.f, pics.keys[i]);
            x += 20.f;
        }
    }
    const int powerupBits[4] = {IT_INVISIBILITY, IT_INVULNERABILITY, IT_SUIT, IT_QUAD};
    for(int i = 0; i < 4; i++)
    {
        if(cl.items & powerupBits[i])
        {
            pic(x, 100.f, pics.powerups[i]);
            x += 20.f;
        }
    }
    for(int i = 0; i < 4; i++)
    {
        if(cl.items & (1 << (28 + i)))
        {
            pic(x, 100.f, pics.sigils[i]);
            x += 12.f;
        }
    }

    // The level, kills and secrets.
    fill(8.f, 122.f, width - 16.f, 1.f, 0.25f, 0.6f, 0.3f);
    char line[64];
    q_snprintf(line, sizeof(line), "%.22s", cl.levelname);
    GL_SetCanvasColor(0.45f, 1.f, 0.55f, 1.f);
    text(8.f, 127.f, 8.f, line);
    q_snprintf(line, sizeof(line), "K %d/%d  S %d/%d", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS],
        cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
    text(8.f, 138.f, 8.f, line);
    GL_SetCanvasColor(1.f, 1.f, 1.f, 1.f);
}

} // namespace

bool active()
{
    return vr_hud_mode.value == 1.f && vrActive() && cls.state == ca_connected && cls.signon == SIGNONS &&
           !cl.intermission;
}

void setPose(const Pose& pose)
{
    current = pose;
}

const Pose& pose()
{
    return current;
}

void screenRect(glm::vec3& corner, glm::vec2& size)
{
    // make_gadget.py: the screen spans x -1.5..1.5, y -0.93..0.93, over the top face at z 0.41.
    corner = {-1.5f, -0.93f, 0.43f};
    size = {3.f, 1.86f};
}

void renderScreen()
{
    if(!active())
    {
        return;
    }

    loadPics();
    Draw_Flush();

    GLint previousFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFbo);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const glcanvas_t savedCanvas = glcanvas;

    ensureTarget();
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width * 2, height * 2);

    // A canvas of its own: the virtual screen over the whole texture, y down.
    glcanvas.type = CANVAS_INVALID;
    glcanvas.transform.scale[0] = 2.f / width;
    glcanvas.transform.scale[1] = -2.f / height;
    glcanvas.transform.offset[0] = -1.f;
    glcanvas.transform.offset[1] = 1.f;
    Draw_GetTransformBounds(&glcanvas.transform, &glcanvas.left, &glcanvas.top, &glcanvas.right, &glcanvas.bottom);
    glcanvas.blendmode = GLS_BLEND_ALPHA;
    GL_SetCanvasColor(1.f, 1.f, 1.f, 1.f);

    layout();
    Draw_Flush();

    glcanvas = savedCanvas;
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

unsigned screenTexture()
{
    return texture;
}

} // namespace qvr::gadget
