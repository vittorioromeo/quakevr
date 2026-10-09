// vr_menu_console.inc -- Console (the corner's Console button, vr_menuui.cpp): Quake's console in the VR menus, for
// commands typed in the headset. Its text fills the page (the newest lines at the bottom, scrolled with the stick, the
// wheel or Page Up and Down); under it the line being typed and a keyboard. The line is the console's own
// (key_lines[edit_line]: its history, Tab completion and hint, shared with the desktop console), edited through keys.c's
// Key_Console and Char_Console. Included by vr_menu.cpp after vr_menu_search.inc (its keys, keyHit's sizes and
// searchText).

constexpr char keyTab = 9;
constexpr char keyRun = 13;
constexpr char keyPrev = 2;
constexpr char keyNext = 3;

struct ConsolePage
{
    int scroll{0};      // rows up from the newest
    int hoverKey{-1};   // under the laser or the mouse
    int focusKey{-1};   // the sticks' selection (-1: the laser's)
    int visibleRows{1}; // as last drawn
    int rows{0};        // the console's rows as last drawn (wrapped to the page)
};
ConsolePage consolePage;

struct ConsoleLayout
{
    float left, right;
    float textTop, textBottom; // the console's text
    float boxY;                // the line typed: its middle
    float half, k;             // as SearchLayout's
    za::Vector<SearchKey> keys;
};

constexpr float consoleTextSize = 6.f; // the console's letters (menu pixels)
constexpr float consoleRowHeight = 7.f;

char consoleKeyLetters[4][12][2]; // the keys' labels (SearchKey points at them)

void consoleEdges(float& left, float& right)
{
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float cl, ct, cr, cb;
    Draw_GetTransformBounds(&t, &cl, &ct, &cr, &cb);
    left = cl + 4.f;
    if(!menuui::toolbarRow())
    {
        left = za::fmax(left, menuui::toolbarRight() + 4.f); // (right of the corner's buttons: the text from the top)
    }
    right = za::fmin(cr - 4.f, left + 460.f);
}

[[nodiscard]] ConsoleLayout consoleLayout()
{
    const Layout l = layout();
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float cl, ct, cr, cb;
    Draw_GetTransformBounds(&t, &cl, &ct, &cr, &cb);
    ConsoleLayout c;
    c.k = za::fmax(1.f, -t.scale[1] * vid.guiheight / (t.scale[0] * vid.guiwidth));
    c.half = 8.5f; // (a little taller than the 7 they were: easier to hit with the laser)
    consoleEdges(c.left, c.right);
    const float rowStep = (2.f * c.half + 3.f) / c.k;

    // From the bottom up: the last row (Tab, Space... Run), four rows of keys, the line typed, the console's text.
    static constexpr const char* rows[] = {"1234567890-+", "QWERTYUIOP_\"", "ASDFGHJKL;./", "ZXCVBNM,=*'"};
    const float keyW = (c.right - c.left) / 12.5f; // (the lower rows set in by half a key at most)
    const float lastY = static_cast<float>(l.bottom) - 2.f - c.half / c.k;
    float y = lastY - 4.f * rowStep;
    c.boxY = y - rowStep * 1.3f;
    c.textTop = static_cast<float>(l.top) + 2.f; // (no title: the page is the console's)
    if(menuui::toolbarRow())
    {
        c.textTop = za::fmax(c.textTop, menuui::toolbarBottom() + 2.f); // (below the flat screen's row of icons)
    }
    c.textBottom = c.boxY - (c.half + 4.f) / c.k;
    for(int r = 0; r < 4; r++, y += rowStep)
    {
        const int n = static_cast<int>(strlen(rows[r]));
        const float x = c.left + (r == 2 ? keyW * 0.25f : r == 3 ? keyW * 0.5f : 0.f);
        for(int i = 0; i < n; i++)
        {
            consoleKeyLetters[r][i][0] = rows[r][i];
            consoleKeyLetters[r][i][1] = '\0';
            c.keys.pushBack({static_cast<char>(rows[r][i] >= 'A' && rows[r][i] <= 'Z' ? rows[r][i] - 'A' + 'a' : rows[r][i]),
                consoleKeyLetters[r][i], x + keyW * i + 1.f, x + keyW * (i + 1) - 1.f, y});
        }
    }
    struct Wide
    {
        char ch;
        const char* label;
        float keys;
    };
    static constexpr Wide last[] = {{keyTab, "Tab", 1.5f}, {' ', "Space", 3.5f}, {keyBackspace, "Delete", 2.f},
        {keyClear, "Clear", 1.5f}, {keyPrev, "Prev", 1.25f}, {keyNext, "Next", 1.25f}, {keyRun, "Run", 1.5f}};
    float x = c.left;
    for(const Wide& w : last)
    {
        c.keys.pushBack({w.ch, w.label, x + 1.f, x + keyW * w.keys - 1.f, y});
        x += keyW * w.keys;
    }
    return c;
}

[[nodiscard]] bool consoleKeyHit(const ConsoleLayout& c, const SearchKey& key, float x, float y)
{
    const float h = (c.half + 1.5f) / c.k;
    return x >= key.x0 - 1.f && x <= key.x1 + 1.f && y >= key.yc - h && y <= key.yc + h;
}

// A key pressed: a letter typed into the console's line, or what the console's own keys do (Enter runs the line and
// keeps it in the history, Tab completes, Up and Down walk the history).
void typeInConsole(char ch)
{
    switch(ch)
    {
        case keyBackspace: Key_Console(K_BACKSPACE); break;
        case keyTab: Key_Console(K_TAB); break;
        case keyPrev: Key_Console(K_UPARROW); break;
        case keyNext: Key_Console(K_DOWNARROW); break;
        case keyRun:
            Key_Console(K_ENTER);
            consolePage.scroll = 0; // (to see what it printed)
            break;
        case keyClear:
            key_lines[edit_line][1] = '\0';
            key_linepos = 1;
            key_tabpartial[0] = '\0';
            key_tabhint[0] = '\0';
            break;
        default:
            if(ch >= 32 && ch < 127)
            {
                Char_Console(ch);
            }
            break;
    }
}

// The console's rows, newest first, each line cut to `fit` letters a row (con_text's lines are as wide as the desktop
// console). The newest line left out while empty (the one being printed to).
struct ConsoleRow
{
    const char* text;
    int length;
};

// The Console page's scratch (vr_mem.hpp): the rows as wrapped for the page. Its capacity stays from draw to draw, the
// whole set is given back at every map change.
struct MenuConsoleScratch
{
    za::Vector<ConsoleRow> rows;
    auto members() { return qvr::mem::list(rows); }
};
mem::Scratch<MenuConsoleScratch> consoleScratch{"menu console rows"};

void consoleRows(int fit, za::Vector<ConsoleRow>& out)
{
    out.clear();
    if(!con_text || con_linewidth <= 0 || con_totallines <= 0 || fit <= 0)
    {
        return;
    }
    for(int line = con_current; line >= 0 && line > con_current - con_totallines; line--)
    {
        const char* text = con_text + (line % con_totallines) * con_linewidth;
        int length = con_linewidth;
        while(length > 0 && (text[length - 1] & 0x7f) == ' ')
        {
            length--;
        }
        if(line == con_current && length == 0)
        {
            continue;
        }
        const int parts = q_max((length + fit - 1) / fit, 1);
        for(int part = parts - 1; part >= 0; part--)
        {
            out.pushBack({text + part * fit, q_min(fit, length - part * fit)});
        }
    }
}

void drawConsolePage()
{
    const ConsoleLayout c = consoleLayout();
    const menupaint::Painter p;
    namespace colors = menupaint::colors;
    // The console's text, on a dark panel: the newest at the bottom.
    const float textHalf = (c.textBottom - c.textTop) * 0.5f;
    p.rounded(c.left, c.right, c.textTop + textHalf, textHalf * c.k, 3.f, colors::boxFill);
    const int fit = static_cast<int>((c.right - c.left - 6.f) / consoleTextSize);
    za::Vector<ConsoleRow>& rows = consoleScratch.rows;
    consoleRows(fit, rows);
    consolePage.rows = static_cast<int>(rows.size());
    consolePage.visibleRows = q_max(static_cast<int>((c.textBottom - c.textTop - 4.f) / consoleRowHeight), 1);
    consolePage.scroll = CLAMP(0, consolePage.scroll, q_max(consolePage.rows - consolePage.visibleRows, 0));
    const int note = consolePage.scroll > 0 ? 1 : 0; // (scrolled up: the bottom row says so)
    for(int i = 0; i < consolePage.visibleRows - note; i++)
    {
        const int r = consolePage.scroll + i;
        if(r >= consolePage.rows)
        {
            break;
        }
        const float y = c.textBottom - 2.f - consoleRowHeight * static_cast<float>(i + 1 + note);
        for(int j = 0; j < rows[r].length; j++)
        {
            Draw_CharacterEx(c.left + 3.f + consoleTextSize * static_cast<float>(j), y, consoleTextSize, consoleTextSize,
                static_cast<unsigned char>(rows[r].text[j]));
        }
    }
    if(consolePage.scroll > 0)
    {
        char more[48];
        q_snprintf(more, sizeof(more), "%d rows newer below", consolePage.scroll);
        searchText(c.right - 5.f * static_cast<float>(strlen(more)) - 4.f, c.textBottom - 2.f - consoleRowHeight + 1.f, 5.f,
            c.right - c.left, more, false);
    }

    // The line typed: "]" and the text, the cursor where it is (the line's end shown if it is long), the completion's
    // hint after it.
    p.rounded(c.left, c.right, c.boxY, c.half, 3.f, colors::boxBorder);
    p.rounded(c.left + 1.f, c.right - 1.f, c.boxY, c.half - 1.f, 2.f, colors::boxFill);
    const char* line = key_lines[edit_line];
    const int length = static_cast<int>(strlen(line));
    const int boxFit = static_cast<int>((c.right - c.left - 10.f) / 8.f);
    const int first = q_max(0, q_max(key_linepos, length) - boxFit + 1);
    const float tx = c.left + 5.f;
    const float ty = c.boxY - 4.f;
    int shown = 0;
    for(int i = first; i < length && shown < boxFit; i++, shown++)
    {
        Draw_CharacterEx(tx + 8.f * static_cast<float>(shown), ty, 8.f, 8.f, static_cast<unsigned char>(line[i]));
    }
    if(key_linepos >= length && key_tabhint[0])
    {
        for(int i = 0; key_tabhint[i] && shown < boxFit; i++, shown++)
        {
            Draw_CharacterEx(tx + 8.f * static_cast<float>(shown), ty, 8.f, 8.f, key_tabhint[i] | 128); // (dim)
        }
    }
    if(static_cast<int>(realtime * 2.0) % 2 == 0)
    {
        Draw_CharacterEx(tx + 8.f * static_cast<float>(key_linepos - first), ty, 8.f, 8.f, 11); // the cursor
    }

    // The keys.
    for(int i = 0; i < static_cast<int>(c.keys.size()); i++)
    {
        const SearchKey& key = c.keys[i];
        const bool hot = consolePage.hoverKey == i || consolePage.focusKey == i;
        p.rounded(key.x0, key.x1, key.yc, c.half, 3.f, hot ? colors::highlightEdge : colors::boxBorder);
        p.rounded(key.x0 + 1.f, key.x1 - 1.f, key.yc, c.half - 1.f, 2.f, hot ? colors::buttonHover : colors::boxFill);
        const float w = 8.f * static_cast<float>(strlen(key.label));
        searchText((key.x0 + key.x1 - w) * 0.5f, key.yc - 4.f, 8.f, key.x1 - key.x0, key.label, hot);
    }
}

// The sticks' selection moved across the keys: along a row, or to the nearest key across in the row above or below
// (up from the top row: the corner's buttons).
void consoleMove(int dx, int dy)
{
    const ConsoleLayout c = consoleLayout();
    const int keys = static_cast<int>(c.keys.size());
    if(consolePage.focusKey < 0 || consolePage.focusKey >= keys)
    {
        consolePage.focusKey = 0;
        return;
    }
    const SearchKey& at = c.keys[consolePage.focusKey];
    if(dx != 0)
    {
        const int next = consolePage.focusKey + dx;
        if(next >= 0 && next < keys && c.keys[next].yc == at.yc)
        {
            consolePage.focusKey = next;
        }
        return;
    }
    const float xc = (at.x0 + at.x1) * 0.5f;
    int best = -1;
    float bestRow = 1e9f, bestDx = 1e9f;
    for(int i = 0; i < keys; i++)
    {
        const float row = (c.keys[i].yc - at.yc) * static_cast<float>(dy);
        if(row <= 0.f)
        {
            continue;
        }
        const float d = za::fabs((c.keys[i].x0 + c.keys[i].x1) * 0.5f - xc);
        if(row < bestRow - 0.5f || (row < bestRow + 0.5f && d < bestDx))
        {
            best = i;
            bestRow = row;
            bestDx = d;
        }
    }
    if(best >= 0)
    {
        consolePage.focusKey = best;
    }
    else if(dy < 0)
    {
        consolePage.focusKey = -1;
        menuui::focusToolbar(-1); // up from the top row: the corner's buttons
    }
}

// The console's text scrolled by `rows` (up: older).
void consoleScroll(int rows)
{
    consolePage.scroll = CLAMP(0, consolePage.scroll + rows, q_max(consolePage.rows - consolePage.visibleRows, 0));
}

void consoleKey(int key)
{
    const ConsoleLayout c = consoleLayout();
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4: goBack(); return;
        case K_ENTER:
        case K_KP_ENTER: typeInConsole(keyRun); return;
        case K_TAB: typeInConsole(keyTab); return;
        case K_BACKSPACE: typeInConsole(keyBackspace); return;
        case K_DEL:
        case K_HOME:
        case K_END: Key_Console(key); return; // (a keyboard's)
        case K_UPARROW: consoleMove(0, -1); S_LocalSound("misc/menu1.wav"); return;
        case K_DOWNARROW: consoleMove(0, 1); S_LocalSound("misc/menu1.wav"); return;
        case K_LEFTARROW: consoleMove(-1, 0); S_LocalSound("misc/menu1.wav"); return;
        case K_RIGHTARROW: consoleMove(1, 0); S_LocalSound("misc/menu1.wav"); return;
        case K_MWHEELUP: consoleScroll(2); return;
        case K_MWHEELDOWN: consoleScroll(-2); return;
        case K_PGUP: consoleScroll(q_max(consolePage.visibleRows - 2, 1)); return;
        case K_PGDN: consoleScroll(-q_max(consolePage.visibleRows - 2, 1)); return;
        case K_ABUTTON:
            if(consolePage.focusKey >= 0 && consolePage.focusKey < static_cast<int>(c.keys.size()))
            {
                typeInConsole(c.keys[consolePage.focusKey].ch);
                S_LocalSound("misc/menu1.wav");
            }
            return;
        case K_MOUSE1:
            for(int i = 0; i < static_cast<int>(c.keys.size()); i++)
            {
                if(consoleKeyHit(c, c.keys[i], m_mousex, m_mousey))
                {
                    typeInConsole(c.keys[i].ch);
                    S_LocalSound("misc/menu1.wav");
                    return;
                }
            }
            return;
        default: return; // (typing: VR_Menu_Char)
    }
}

void consoleMouse(float x, float y)
{
    const ConsoleLayout c = consoleLayout();
    int key = -1;
    for(int i = 0; i < static_cast<int>(c.keys.size()) && key < 0; i++)
    {
        key = consoleKeyHit(c, c.keys[i], x, y) ? i : -1;
    }
    if(key >= 0 && key != consolePage.hoverKey)
    {
        consolePage.focusKey = -1; // the laser's now
        if(ui_mouse_sound.value)
        {
            S_LocalSound("misc/menu1.wav");
        }
    }
    consolePage.hoverKey = key;
}

// Console, from any menu (the corner's button): the line as it was left, the newest text.
void openConsolePage()
{
    const int target = pageIndex(pageConsole);
    if(m_state == m_vr && page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    consolePage.scroll = 0;
    consolePage.hoverKey = -1;
    consolePage.focusKey = 0;
    openFromAnywhere(target); // (Back: where it was opened from)
}
