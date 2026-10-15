// vr_ad_gearlights.inc -- AD support: the gadget's outer button (x -0.55), the screen's pages (gadget::togglePage:
// Arcane Dimensions' page and back, vr_ad_gadget.inc). Included by vr_gearlights.cpp (inside its anonymous namespace:
// the inner button's hit volume, fingertip, cooldown and buzz).

// AD support: the outer button (x -0.55), the screen's pages' (gadget::togglePage: Arcane Dimensions' page and back,
// while its data is loaded; else it does nothing). The same hit volume, cut and cooldown as the inner one's.
constexpr glm::vec3 pageButtonLocal{-0.55f, -1.38f, -0.05f};
// The outer button's (AD support): the fingertip in its hit volume, and its last press that counted.
struct PageButton
{
    bool pressing = false;
    double pressedAt = -1.0;
};
PageButton pageButton;

// A press of the outer button (the fingertip's arrival, or vr_gadget_page_press): the screen's other page (AD support),
// with a click and a tick; nothing without AD's data, while Doom is on the gadget or a screen tap is under way, or
// within vr_gadget_button_cooldown of its last press.
void pressPage(int hand)
{
    if(bullettime::tapping() || doom::onGadget())
    {
        return;
    }
    if(pageButton.pressedAt >= 0.0 && realtime - pageButton.pressedAt < za::max(0.f, vr_gadget_button_cooldown.value))
    {
        return;
    }
    pageButton.pressedAt = realtime;
    if(!gadget::togglePage())
    {
        if(vr_debug_gadget_button.value)
        {
            Con_Printf("gadget page button: pressed (no other page: Arcane Dimensions' data isn't loaded)\n");
        }
        return;
    }
    if(vr_gear_lights_sound_on.string && vr_gear_lights_sound_on.string[0] && cls.state == ca_connected)
    {
        S_LocalSound(vr_gear_lights_sound_on.string);
    }
    buzz(hand, 0.35f);
    buzz(hands::gadgetHand(), 0.2f);
    if(vr_debug_gadget_button.value)
    {
        Con_Printf("gadget page button: page %d\n", gadget::page());
    }
}

void pressPageButton(const hands::State& s, int hand)
{
    glm::vec3 at, out;
    float radius = 0.f;
    if(!buttonAt(pageButtonLocal, at, out, radius))
    {
        pageButton.pressing = false;
        return;
    }
    const glm::vec3 tip = fingertip(s, hand);
    const float d = glm::length(tip - at);
    const float side = glm::dot(tip - at, out);
    const bool in = d <= radius && side >= -cutBehind * radius;
    if(pageButton.pressing && (d > radius * 1.5f || side < -cutBehind * radius * 2.f))
    {
        pageButton.pressing = false;
    }
    if(pageButton.pressing || !in)
    {
        return;
    }
    pageButton.pressing = true;
    pressPage(hand);
}

// AD support: the outer button's press without a fingertip; the page toggled without the button.
void pressPage_f()
{
    pressPage(-1);
}

void page_f()
{
    (void)gadget::togglePage();
}

void pageCommands()
{
    Cmd_AddCommand("vr_gadget_page_press", pressPage_f);
    Cmd_AddCommand("vr_gadget_page", page_f);
}
