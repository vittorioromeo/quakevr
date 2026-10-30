// vr_ad_gadget.inc -- AD support: the wrist gadget's Arcane Dimensions page (docs/vr-port/ARCANE_DIMENSIONS.md; its
// outer button: vr_ad_gearlights.inc). Included by vr_gadget.cpp (inside its anonymous namespace: its drawing helpers).

// AD support: the gadget's Arcane Dimensions page (its outer button: vr_gearlights.cpp; vr_gadget_page), in place of
// the HUD while it is chosen and AD's data is loaded: every artifact held with its own time left (the main page shows
// their icons and the soonest's), the custom keys, the debuffs with what is still to come, and the weapons carried that
// have an upgrade (the Shadow Axe, the Widowmaker, the Plasma Gun: vr_ad_bridge.qc), with where each is.
//   ARCANE DIMENSIONS
//   ----------------------------------
//   ARTIFACTS           KEYS
//   [i] AEGIS     27    [k] [k]
//   [i] AIR TANK  --    DEBUFFS
//   ...                 POISON    18
//   ----------------------------------
//   UPGRADES
//   SHADOW AXE   MAIN
//   WIDOWMAKER   HIP R, BACK L
struct PageState
{
    int page{0}; // 0 the HUD, 1 the AD page (togglePage)
};
PageState pageState;

[[nodiscard]] bool adPageShown()
{
    return pageState.page == 1 && vr_ad_mounted.value != 0.f;
}

void adPage(const Readout& r, const Palette& pal)
{
    constexpr float colX[2] = {margin, 128.f};
    constexpr float sectionY = 20.f, rowStep = 10.f;
    drawText(margin, 5.f, 8.f, "ARCANE DIMENSIONS", pal.value, pal);
    fill(margin, 16.f, width - 2.f * margin, 1.f, pal.line);

    // The artifacts: each held one, its own time (-- for good).
    drawText(colX[0], sectionY, 8.f, "ARTIFACTS", pal.label, pal);
    static constexpr const char* artifacts[6] = {"AEGIS", "AIR TANK", "BLASTBELT", "JUMPBOOTS", "SHARPSHOT", "PIERCER"};
    float y = sectionY + rowStep;
    int shown = 0;
    for(int i = 0; i < 6; i++)
    {
        if(!(r.adHud & (1 << i)))
        {
            continue;
        }
        const int secs = (r.adArt[i / 3] >> (8 * (i % 3))) & 255;
        if(picOk(PicAdArtifact + i))
        {
            drawPic(colX[0], y - 2.f, PicAdArtifact + i, 0.5f, pal.value, pal);
        }
        drawText(colX[0] + 14.f, y, 8.f, artifacts[i], pal.value, pal);
        char t[8] = "--";
        if(secs > 0 && secs < 255)
        {
            q_snprintf(t, sizeof(t), "%d", secs);
        }
        const bool ending = secs > 0 && secs <= 5;
        drawText(colX[1] - 12.f - 8.f * static_cast<float>(strlen(t)), y, 8.f, t, ending ? blinking(pal.warn) : pal.value, pal);
        y += rowStep;
        shown++;
    }
    if(!shown)
    {
        drawText(colX[0], y, 8.f, "NONE", pal.dim, pal);
    }

    // The custom keys (by their HUD skins).
    drawText(colX[1], sectionY, 8.f, "KEYS", pal.label, pal);
    float x = colX[1];
    bool anyKey = false;
    for(int k = 0; k < 4; k++)
    {
        const int pic = PicAdKey + CLAMP(0, (r.adKeys >> (4 * k)) & 15, 13);
        if((r.adHud & (512 << k)) && picOk(pic))
        {
            drawPic(x, sectionY + rowStep - 1.f, pic, 1.f, pal.label, pal);
            x += (greyPic(pic) ? static_cast<float>(greyPic(pic)->width) : 16.f) + 3.f;
            anyKey = true;
        }
    }
    if(!anyKey)
    {
        drawText(colX[1], sectionY + rowStep, 8.f, "NONE", pal.dim, pal);
    }

    // The debuffs: the poison and the bleeding still to come (damage), the burning's seconds.
    const float debuffY = sectionY + rowStep * 2.5f;
    drawText(colX[1], debuffY, 8.f, "DEBUFFS", pal.label, pal);
    static constexpr const char* debuffs[3] = {"POISON", "BLEEDING", "BURNING"};
    static constexpr int bits[3] = {128, 256, 64};
    y = debuffY + rowStep;
    bool anyDebuff = false;
    for(int d = 0; d < 3; d++)
    {
        if(!(r.adHud & bits[d]))
        {
            continue;
        }
        const int amount = (r.adDebuff >> (8 * d)) & 255;
        drawText(colX[1], y, 8.f, debuffs[d], blinking(pal.warn), pal);
        char t[12] = "";
        if(amount > 0)
        {
            q_snprintf(t, sizeof(t), d == 2 ? "%ds" : "%d", amount);
        }
        drawText(width - margin - 8.f * static_cast<float>(strlen(t)), y, 8.f, t, pal.warn, pal);
        y += rowStep;
        anyDebuff = true;
    }
    if(!anyDebuff)
    {
        drawText(colX[1], y, 8.f, "NONE", pal.dim, pal);
    }

    // The upgrades: each kind's weapons carried, where (the hands, the holsters).
    const float upY = 96.f;
    fill(margin, upY - 4.f, width - 2.f * margin, 1.f, pal.line);
    drawText(margin, upY, 8.f, "UPGRADES", pal.label, pal);
    static constexpr const char* kinds[3] = {"SHADOW AXE", "WIDOWMAKER", "PLASMA GUN"};
    static constexpr const char* places[8] = {"MAIN", "OFF", "BACK L", "BACK R", "HIP L", "HIP R", "CHEST L", "CHEST R"};
    y = upY + rowStep;
    bool anyUp = false;
    for(int k = 0; k < 3; k++)
    {
        char where[64] = "";
        for(int i = 0; i < 8; i++)
        {
            if((r.adUp >> (3 * i)) & (1 << k))
            {
                q_strlcat(where, where[0] ? ", " : "", sizeof(where));
                q_strlcat(where, places[i], sizeof(where));
            }
        }
        if(!where[0])
        {
            continue;
        }
        drawText(margin, y, 8.f, kinds[k], pal.value, pal);
        char line[32];
        q_snprintf(line, sizeof(line), "%.15s", where); // (never past the screen: 15 columns)
        drawText(margin + 12.f * 8.f, y, 8.f, line, pal.label, pal);
        y += rowStep;
        anyUp = true;
    }
    if(!anyUp)
    {
        drawText(margin, y, 8.f, "NONE CARRIED", pal.dim, pal);
    }
}

// togglePage (vr_gadget.hpp): the AD page and back while AD's data is loaded; false (the HUD kept) without it.
[[nodiscard]] bool adTogglePage()
{
    if(vr_ad_mounted.value == 0.f)
    {
        pageState.page = 0;
        return false;
    }
    pageState.page = pageState.page == 0 ? 1 : 0;
    return true;
}
