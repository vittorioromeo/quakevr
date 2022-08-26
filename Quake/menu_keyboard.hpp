#pragma once

#include "variantutil.hpp"
#include "quakeglm_qvec2.hpp"

#include <variant>
#include <vector>

namespace quake::vr
{

class menu_keyboard
{
public:
    // clang-format off
    struct ka_backspace { };
    struct ka_enter { };
    struct ka_back { };
    struct ka_up { };
    struct ka_down { };
    struct ka_left { };
    struct ka_right { };
    struct ka_space { };
    struct ka_capslock { };
    struct ka_tab { };
    struct ka_console { };
    // clang-format on

    using key_action =
        std::variant<char, ka_backspace, ka_enter, ka_back, ka_up, ka_down,
            ka_left, ka_right, ka_space, ka_capslock, ka_tab, ka_console>;

private:
    struct aabb
    {
        qvec2 _min;
        qvec2 _max;
    };

    struct key
    {
        qvec2 _pos;
        aabb _bounds;
        key_action _action;
        bool _hovered{false};
    };

    std::vector<key> _keys;
    qvec2 _pos;
    bool _last_click{false};
    bool _rising_edge{false};
    bool _falling_edge{false};

    bool _drag_hover{false};
    qvec2 _old_pos{};
    qvec2 _drag_start{};

    bool _caps_lock{};

    [[nodiscard]] qvec2 drag_min() noexcept;

    [[nodiscard]] qvec2 drag_max() noexcept;

    void draw_bounds() noexcept;

    void draw_dragbar() noexcept;

    [[nodiscard]] qvec2 tile_pos(const qvec2& pos);

    void draw_tiles();

    void draw_letters();

    [[nodiscard]] bool inside(
        const int x, const int y, const qvec2& min, const qvec2& max);

    [[nodiscard]] bool hovers_tile(const key& k, const int x, const int y);

    [[nodiscard]] bool hovers_drag(const int x, const int y);

    [[nodiscard]] aabb bounds() noexcept;

public:
    menu_keyboard(const qvec2 pos) noexcept;

    void update_click(const int mx, const int my, const bool click);

    template <typename F>
    void update_letters(const int mx, const int my, const bool click, F&& f)
    {
        for(key& k : _keys)
        {
            k._hovered = hovers_tile(k, mx, my);

            if(k._hovered && _rising_edge)
            {
                quake::util::match(k._action, f);
            }
        }
    }

    void update_drag(const int mx, const int my, const bool click);

    void draw();

    [[nodiscard]] bool hovered(const int mx, const int my) noexcept;

    void toggle_caps_lock() noexcept;

    [[nodiscard]] bool caps_lock() noexcept;
};

} // namespace quake::vr
