#include "menu_keyboard.hpp"

#include "gl_util.hpp"
#include "menu.hpp"

#include <string_view>

namespace quake::vr
{

[[nodiscard]] qvec2 menu_keyboard::drag_min() noexcept
{
    return _pos - qvec2{8, 8};
}

[[nodiscard]] qvec2 menu_keyboard::drag_max() noexcept
{
    return drag_min() + qvec2{256, 16};
}

void menu_keyboard::draw_bounds() noexcept
{
    glDisable(GL_TEXTURE_2D);

    {
        const gl_util::gl_beginend_guard guard{GL_QUADS};

        const auto [min, max] = bounds();
        glColor3f(0.05, 0.05, 0.05);

        glVertex2f(min.x, min.y);
        glVertex2f(max.x, min.y);
        glVertex2f(max.x, max.y);
        glVertex2f(min.x, max.y);
    }

    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
}


void menu_keyboard::draw_dragbar() noexcept
{
    glDisable(GL_TEXTURE_2D);

    {
        const gl_util::gl_beginend_guard guard{GL_QUADS};

        const auto& min = drag_min();
        const auto& max = drag_max();

        if(_drag_hover)
        {
            glColor3f(0.65, 0.15, 0.15);
        }
        else
        {
            glColor3f(0.15, 0.15, 0.15);
        }

        glVertex2f(min.x, min.y);
        glVertex2f(max.x, min.y);
        glVertex2f(max.x, max.y);
        glVertex2f(min.x, max.y);
    }

    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
}

[[nodiscard]] qvec2 menu_keyboard::tile_pos(const qvec2& pos)
{
    return pos + _pos + qvec2{0, 24};
}

void menu_keyboard::draw_tiles()
{
    glDisable(GL_TEXTURE_2D);

    {
        const gl_util::gl_beginend_guard guard{GL_QUADS};

        for(const key& k : _keys)
        {
            const auto& min = tile_pos(k._bounds._min);
            const auto& max = tile_pos(k._bounds._max);

            if(k._hovered)
            {
                glColor3f(0.65, 0.15, 0.15);
            }
            else
            {
                glColor3f(0.15, 0.15, 0.15);
            }

            glVertex2f(min.x, min.y);
            glVertex2f(max.x, min.y);
            glVertex2f(max.x, max.y);
            glVertex2f(min.x, max.y);
        }
    }

    glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
}

void menu_keyboard::draw_letters()
{
    for(const key& k : _keys)
    {
        const auto cp = tile_pos(k._pos);

        quake::util::match(
            k._action,
            [&](const char c) {
                M_DrawCharacter(cp.x, cp.y, _caps_lock ? c - 32 : c);
            },
            [&](ka_backspace) { M_PrintWhite(cp.x, cp.y, "backspace"); }, //
            [&](ka_enter) { M_PrintWhite(cp.x, cp.y, "enter"); },         //
            [&](ka_back) { M_PrintWhite(cp.x, cp.y, "escape"); },         //
            [&](ka_up) { M_PrintWhite(cp.x, cp.y, "up"); },               //
            [&](ka_down) { M_PrintWhite(cp.x, cp.y, "down"); },           //
            [&](ka_left) { M_PrintWhite(cp.x, cp.y, "left"); },           //
            [&](ka_right) { M_PrintWhite(cp.x, cp.y, "right"); },         //
            [&](ka_space) { M_PrintWhite(cp.x, cp.y, "space"); },         //
            [&](ka_tab) { M_PrintWhite(cp.x, cp.y, "tab"); },             //
            [&](ka_console) { M_PrintWhite(cp.x, cp.y, "console"); },     //
            [&](ka_capslock) {
                M_PrintWhite(
                    cp.x, cp.y, va("caps %s", _caps_lock ? "ON" : "OFF"));
            });
    }
}

[[nodiscard]] bool menu_keyboard::inside(
    const int x, const int y, const qvec2& min, const qvec2& max)
{
    return x >= min.x && x <= max.x && y >= min.y && y <= max.y;
}

[[nodiscard]] bool menu_keyboard::hovers_tile(
    const key& k, const int x, const int y)
{
    const auto& min = tile_pos(k._bounds._min);
    const auto& max = tile_pos(k._bounds._max);

    return inside(x, y, min, max);
}

[[nodiscard]] bool menu_keyboard::hovers_drag(const int x, const int y)
{
    const float off = _drag_hover ? 16.f : 0.f;
    const qvec2 voff{off, off};

    return inside(x, y, drag_min() - voff, drag_max() + voff);
}


[[nodiscard]] menu_keyboard::aabb menu_keyboard::bounds() noexcept
{
    qvec2 min = drag_min();
    qvec2 max = drag_max();

    for(const key& k : _keys)
    {
        const auto bmin = tile_pos(k._bounds._min);
        const auto bmax = tile_pos(k._bounds._max);

        min.x = std::min(min.x, bmin.x);
        min.y = std::min(min.y, bmin.y);

        max.x = std::max(max.x, bmax.x);
        max.y = std::max(max.y, bmax.y);
    }

    min.x -= 8;
    min.y -= 8;

    max.x += 8;
    max.y += 8;

    return {min, max};
}


menu_keyboard::menu_keyboard(const qvec2 pos) noexcept : _pos{pos}
{
    using namespace std::string_view_literals;
    constexpr auto kbLayout = R"(1234567890'
qwertyuiop
asdfghjkl
zxcvbnm,.-)"sv;

    constexpr int distance = 28;
    constexpr int off = 8;

    int cx = 0;
    int cy = 0;

    for(const char c : kbLayout)
    {
        if(c == '\n')
        {
            cy += distance;
            cx = 0;

            continue;
        }

        const aabb key_bounds{
            {cx - off, cy - off},        //
            {cx + 8 + off, cy + 8 + off} //
        };

        _keys.emplace_back(key{qvec2{cx, cy}, key_bounds, key_action{c}});
        cx += distance;
    }

    const auto add_special_key = [&](const int x, const int y, const int width,
                                     auto action) {
        constexpr int off = 8;
        constexpr int height = 8;

        const aabb key_bounds{
            {x - off, y - off},                 //
            {x + width + off, y + height + off} //
        };

        _keys.emplace_back(key{qvec2{x, y}, key_bounds, key_action{action}});
    };

    add_special_key(280 + 32, -24, 76, ka_back{});
    add_special_key(280 + 32, 4, 76, ka_backspace{});
    add_special_key(280 + 32, 8 + 24, 76, ka_enter{});
    add_special_key(280 + 32 + 20 + 76, -24, 76, ka_console{});
    add_special_key(280 + 32 + 20 + 76, 4, 76, ka_capslock{});
    add_special_key(280 + 32 + 20 + 76, 8 + 24, 76, ka_tab{});
    add_special_key(55, 114, 150, ka_space{});

    add_special_key(64 + 280 + 48, 26 + 4 + 8 + 48, 32, ka_up{});
    add_special_key(64 + 280 + 48, 26 + 4 + 4 + 24 + 8 + 48, 32, ka_down{});
    add_special_key(64 + 280 + 0 - 8, 26 + 4 + 4 + 24 + 8 + 48, 32, ka_left{});
    add_special_key(
        64 + 280 + 48 + 48 + 8, 26 + 4 + 4 + 24 + 8 + 48, 38, ka_right{});
}

void menu_keyboard::update_click(const int mx, const int my, const bool click)
{
    _rising_edge = click && !_last_click;
    _falling_edge = !click && _last_click;
    _last_click = click;
}


void menu_keyboard::update_drag(const int mx, const int my, const bool click)
{
    _drag_hover = hovers_drag(mx, my);

    if(_drag_hover && _rising_edge)
    {
        _old_pos = _pos;
        _drag_start = qvec2{mx, my};
    }

    if(_drag_hover && click)
    {
        _pos = _old_pos + qvec2{mx, my} - _drag_start;
    }
}

void menu_keyboard::draw()
{
    draw_bounds();
    draw_dragbar();
    draw_tiles();
    draw_letters();
}

[[nodiscard]] bool menu_keyboard::hovered(const int mx, const int my) noexcept
{
    const auto [min, max] = bounds();
    return inside(mx, my, min, max);
}

void menu_keyboard::toggle_caps_lock() noexcept
{
    _caps_lock = !_caps_lock;
}

[[nodiscard]] bool menu_keyboard::caps_lock() noexcept
{
    return _caps_lock;
}

} // namespace quake::vr
