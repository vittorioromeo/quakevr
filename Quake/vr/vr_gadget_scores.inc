// vr_gadget_scores.inc -- the wrist gadget's multiplayer scoreboard page (docs/vr-port/MULTIPLAYER.md, "The scoreboard on
// the wrist"; its outer button: vr_ad_gearlights.inc; the pages' turn: vr_gadget.cpp togglePage). Included by
// vr_gadget.cpp (inside its anonymous namespace: its drawing helpers).

// The scoreboard's page, in place of the HUD while it is chosen in a multiplayer game (coop or deathmatch, bots
// included): the game's own scoreboard's rows in its order (Sbar_SortFrags: the most frags first), each player's
// colours, frags and name, you on a lit strip, the talking indicator of voice chat; coop's line of the level's kills
// and secrets above them (the game's coop scoreboard). Live: drawn again every frame as the screen is. Up to 11 rows
// at the screen's text size, then smaller (all 16 fit), then "+N MORE".
//   SCOREBOARD                  DEATHMATCH
//   ----------------------------------
//   (KILLS 12/40  SECRETS 1/5)               coop's line
//        FRAGS NAME
//   [==]   12  Hudson               ((o))
//   [==]    3  Player                       (you: the lit strip)
struct ScoreRow
{
    int slot{0};
    int frags{0};
    bool you{false};
    bool talking{false};
};

[[nodiscard]] bool scoresAvailable()
{
    return cls.state == ca_connected && cl.maxclients > 1 && cl.scores != nullptr;
}

[[nodiscard]] bool scoresPageShown()
{
    return pageState.page == PageScores && scoresAvailable();
}

// The scoreboard's rows in its order (the game's Sbar_SortFrags, shared with the flat screen's scoreboard): how many.
[[nodiscard]] int scoreRows(ScoreRow (&rows)[MAX_SCOREBOARD])
{
    if(!scoresAvailable())
    {
        return 0;
    }
    Sbar_SortFrags();
    int n = 0;
    for(int i = 0; i < scoreboardlines && n < MAX_SCOREBOARD; i++)
    {
        const int slot = fragsort[i];
        rows[n++] = {slot, cl.scores[slot].frags, slot == cl.viewentity - 1, voicechat::talkingNow(slot)};
    }
    return n;
}

// A shirt or trousers colour (a 4-bit colour of scoreboard_t's colors, shifted to the palette's row) as the screen
// draws it.
[[nodiscard]] glm::vec3 playerColour(int row)
{
    const unsigned c = d_8to24table[Sbar_ColorForMap(row) & 255];
    const glm::vec3 rgb{static_cast<float>(c & 255u), static_cast<float>((c >> 8) & 255u), static_cast<float>((c >> 16) & 255u)};
    return glm::min(rgb / 255.f * screenBrightness(), glm::vec3{1.f});
}

void scoresPage(const Readout& r, const Palette& pal)
{
    const bool coop = cl.gametype != GAME_DEATHMATCH;
    drawText(margin, 5.f, 8.f, "SCOREBOARD", pal.value, pal);
    const char* mode = coop ? "COOP" : "DEATHMATCH";
    drawText(width - margin - 8.f * static_cast<float>(strlen(mode)), 5.f, 8.f, mode, pal.label, pal);
    fill(margin, 16.f, width - 2.f * margin, 1.f, pal.line);

    float y = 20.f;
    if(coop)
    {
        char line[48];
        q_snprintf(line, sizeof(line), "KILLS %d/%d  SECRETS %d/%d", r.kills, r.totalKills, r.secrets, r.totalSecrets);
        drawText(margin, y, 8.f, line, pal.label, pal);
        y += 11.f;
    }

    ScoreRow rows[MAX_SCOREBOARD];
    const int n = scoreRows(rows);
    const float bottom = static_cast<float>(height) - 5.f;
    // At the screen's text size while they fit (the column header's line too), else smaller.
    const bool small = y + 10.f * static_cast<float>(n + 1) > bottom;
    const float size = small ? 6.f : 8.f, step = small ? 7.f : 10.f;
    const float swatchX = margin, fragsEnd = margin + 18.f + 4.f * size, nameX = fragsEnd + size;
    drawText(fragsEnd - 5.f * 6.f, y, 6.f, "FRAGS", pal.dim, pal);
    drawText(nameX, y, 6.f, "NAME", pal.dim, pal);
    y += small ? 7.f : 9.f;

    const int fit = za::max(0, static_cast<int>((bottom - y) / step));
    const int shown = n > fit ? za::max(0, fit - 1) : n; // (the last line says how many more)
    for(int i = 0; i < shown; i++, y += step)
    {
        const ScoreRow& row = rows[i];
        const scoreboard_t& s = cl.scores[row.slot];
        if(row.you)
        {
            fill(margin - 3.f, y - 1.f, width - 2.f * margin + 6.f, size + 2.f, pal.dim * 0.55f);
        }
        // Its colours: the shirt over the trousers (as the game's scoreboard).
        const float half = za::floor(size * 0.5f);
        fill(swatchX, y, 14.f, half, playerColour(s.colors & 0xf0));
        fill(swatchX, y + half, 14.f, size - half, playerColour((s.colors & 15) << 4));

        char frags[12];
        q_snprintf(frags, sizeof(frags), "%d", CLAMP(-999, row.frags, 9999));
        drawText(fragsEnd - size * static_cast<float>(strlen(frags)), y, size, frags, row.you ? pal.value : pal.label, pal);
        char name[24];
        q_snprintf(name, sizeof(name), "%.15s", s.name);
        drawText(nameX, y, size, name, row.you ? pal.value : pal.label, pal);
        if(row.talking)
        {
            drawText(width - margin - 5.f * size, y, size, "((o))", pal.value, pal);
        }
    }
    if(shown < n)
    {
        char more[24];
        q_snprintf(more, sizeof(more), "+%d MORE", n - shown);
        drawText(nameX, y, size, more, pal.dim, pal);
    }
    if(!n)
    {
        drawText(margin, y, 8.f, "NO PLAYERS", pal.dim, pal);
    }
}

// vr_gadget_scores: the scoreboard page's rows, as it draws them (for tests and the Debug menu's Wrist Gadget
// Scoreboard row): a line for the page, then a line a row in its order.
void scoresReport_f()
{
    ScoreRow rows[MAX_SCOREBOARD];
    const int n = scoreRows(rows);
    Con_Printf("wristscore: page=%d available=%d shown=%d mode=%s rows=%d\n", gadget::page(), scoresAvailable() ? 1 : 0,
        scoresPageShown() ? 1 : 0, cl.gametype == GAME_DEATHMATCH ? "dm" : "coop", n);
    for(int i = 0; i < n; i++)
    {
        Con_Printf("wristscore: row=%d slot=%d frags=%d you=%d talking=%d name=\"%s\"\n", i, rows[i].slot, rows[i].frags,
            rows[i].you ? 1 : 0, rows[i].talking ? 1 : 0, cl.scores[rows[i].slot].name);
    }
}
