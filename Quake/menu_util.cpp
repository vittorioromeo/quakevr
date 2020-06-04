#include "menu_util.hpp"

#include "quakeglm_qvec3.hpp"
#include "client.hpp"
#include "menu.hpp"
#include "util.hpp"
#include "keys.hpp"
#include "q_sound.hpp"
#include "input.hpp"

// TODO VR: (P2) forward declaration due to crappy quake header deps

struct sfx_t;

sfx_t* S_PrecacheSound(const char* name);

void S_StartSound(int entnum, int entchannel, sfx_t* sfx, const qvec3& origin,
    float fvol, float attenuation);

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

namespace quake
{

void menu::clear()
{
    _entries.clear();
    _cursor_idx = 0;
}

void menu::assert_valid_idx(const int idx) const
{
    assert(idx >= 0);
    assert(idx < entry_count());
}

impl::menu_entry& menu::access(const int idx) noexcept
{
    assert_valid_idx(idx);
    return _entries[idx];
}

const impl::menu_entry& menu::access(const int idx) const noexcept
{
    assert_valid_idx(idx);
    return _entries[idx];
}

void menu::key_option(const int key, const int idx)
{
    if(empty())
    {
        return;
    }

    const bool isLeft = (key == K_LEFTARROW);

    const auto adjustValueI = quake::util::makeMenuValueAdjuster<int>(isLeft);

    const auto adjustCVarF = quake::util::makeMenuCVarAdjuster<float>(isLeft);
    const auto adjustCVarI = quake::util::makeMenuCVarAdjuster<int>(isLeft);

    impl::menu_entry& e = access(idx);

    const auto match_entry = [&](auto& self, auto&& entry) -> void {
        quake::util::match(
            std::forward<decltype(entry)>(entry), //
            [&](const impl::menu_entry_value<bool>& x) {
                bool& v = *(x._getter());
                v = !v;
            },
            [&](const impl::menu_entry_cvar<float>& x) {
                const auto& [inc, min, max] = x._bounds;
                adjustCVarF(*(x._cvar_getter()), inc, min, max);
            },
            [&](const impl::menu_entry_cvar<int>& x) {
                const auto& [inc, min, max] = x._bounds;
                adjustCVarI(*(x._cvar_getter()), inc, min, max);
            },
            [&](const impl::menu_entry_cvar<bool>& x) {
                adjustCVarI(*(x._cvar_getter()), 1, 0, 1);
            },
            [&](const impl::menu_entry_value_labeled_cvar<int>& x) {
                const auto& [inc, min, max] = x._bounds;
                adjustCVarI(*(x._cvar_getter()), inc, min, max);
            },
            [&](const impl::menu_entry_value_labeled<int>& x) {
                const auto& [inc, min, max] = x._bounds;
                adjustValueI(*(x._getter()), inc, min, max);
            },
            [&](const impl::menu_entry_action& x) { x._action(); },
            [&](const impl::menu_entry_action_slider& x) {
                x._action_slide(isLeft ? -1 : 1);
            },
            [&](const impl::menu_entry_separator&) {},
            [&](const impl::menu_entry_ptr& ptr) {
                self(self, ptr._ptr->_variant);
            });
    };

    match_entry(match_entry, e._variant);
}

[[nodiscard]] bool menu::entry_is_selectable_at(const int idx)
{
    assert_valid_idx(idx);
    return !std::holds_alternative<impl::menu_entry_separator>(
        _entries[idx]._variant);
}


menu::menu(const std::string_view title, std::function<void()> escape_fn,
    bool two_columns) noexcept
    : _title{title}, _escape_fn{std::move(escape_fn)}, _two_columns{two_columns}
{
}

void menu::add_separator()
{
    // TODO VR: (P2) not nice, separator should not have a common
    _entries.emplace_back(
        impl::menu_entry_common{}, impl::menu_entry_separator{});
}

void menu::enter()
{
    if(empty())
    {
        return;
    }

    // Hover current entry.
    auto& curr_hover_change_fn = access(_cursor_idx)._common._hover_change;

    if(curr_hover_change_fn)
    {
        curr_hover_change_fn(true);
    }
}

void menu::leave()
{
    if(empty())
    {
        return;
    }

    // Un-hover current entry.
    auto& curr_hover_change_fn = access(_cursor_idx)._common._hover_change;

    if(curr_hover_change_fn)
    {
        curr_hover_change_fn(false);
    }
}

void menu::remove_entry_at(const std::size_t index) noexcept
{
    assert_valid_idx(index);
    _entries.erase(_entries.begin() + index);

    move_cursor(0);
}

void menu::update_hover(
    const int prev_cursor_idx, const int curr_cursor_idx) noexcept
{
    if(curr_cursor_idx == prev_cursor_idx)
    {
        return;
    }

    auto& prev_hover_change_fn = access(prev_cursor_idx)._common._hover_change;

    if(prev_hover_change_fn)
    {
        prev_hover_change_fn(false);
    }

    auto& curr_hover_change_fn = access(curr_cursor_idx)._common._hover_change;

    if(curr_hover_change_fn)
    {
        curr_hover_change_fn(true);
    }
}

void menu::move_cursor(const int offset) noexcept
{
    if(empty())
    {
        _cursor_idx = 0;
        return;
    }

    const auto prev_cursor_idx = _cursor_idx;
    const int dir = offset > 0 ? 1 : -1;

    _cursor_idx += offset;

    while(_cursor_idx >= 0 && _cursor_idx < entry_count() &&
          !entry_is_selectable_at(_cursor_idx))
    {
        _cursor_idx += dir;
    }

    if(_cursor_idx < 0)
    {
        _cursor_idx = entry_count() - 1;

        while(!entry_is_selectable_at(_cursor_idx))
        {
            --_cursor_idx;
        }
    }
    else if(_cursor_idx >= entry_count())
    {
        _cursor_idx = 0;

        while(!entry_is_selectable_at(_cursor_idx))
        {
            ++_cursor_idx;
        }
    }

    update_hover(prev_cursor_idx, _cursor_idx);
}

void menu::key(const int key)
{
    switch(key)
    {
        case K_ESCAPE:
        {
            VID_SyncCvars(); // sync cvars before leaving menu.
                             // FIXME: there are other ways to leave
                             // menu
            S_LocalSound("misc/menu1.wav");

            // TODO VR: (P2) have some sort of menu stack instead of
            // going back manually
            assert(_escape_fn);
            _escape_fn();

            leave();

            break;
        }

        case K_UPARROW:
        {
            S_LocalSound("misc/menu1.wav");

            move_cursor(-1);
            break;
        }

        case K_DOWNARROW:
        {
            S_LocalSound("misc/menu1.wav");

            move_cursor(1);
            break;
        }

        case K_LEFTARROW:
        {
            if(_two_columns)
            {
                S_LocalSound("misc/menu1.wav");
                move_cursor(-items_per_column);
            }
            else
            {
                S_LocalSound("misc/menu3.wav");
                key_option(key, _cursor_idx);
            }

            break;
        }

        case K_RIGHTARROW:
        {
            if(_two_columns)
            {
                S_LocalSound("misc/menu1.wav");
                move_cursor(items_per_column);
            }
            else
            {
                S_LocalSound("misc/menu3.wav");
                key_option(key, _cursor_idx);
            }

            break;
        }

        case K_ENTER:
        {
            m_entersound = true;
            key_option(key, _cursor_idx);
            break;
        }
    }

    if(_key_fn)
    {
        _key_fn(key, access(_cursor_idx));
    }
}

void menu::draw(const int offset_x, const int offset_y)
{
    int x = offset_x + 240;

    if(_two_columns)
    {
        x -= 120 * (_cursor_idx / items_per_column);
    }

    int y = offset_y + 4;

    // plaque
    // M_DrawTransPic(16, y, Draw_CachePic("gfx/qplaque.lmp"));

    // customize header
    // qpic_t* p = Draw_CachePic("gfx/ttl_cstm.lmp");
    // M_DrawPic((320 - p->width) / 2, y, p);

    y += 28;

    // title
    M_PrintWhite((320 - 8 * _title.size()) / 2, y, _title.data());
    y += 16;

    if(empty())
    {
        return;
    }

    char buf[512]{};
    int idx{0};

    constexpr int char_size = 8;
    constexpr int label_padding = 26;

    const auto get_label_x = [&, this](const std::string_view s) {
        if(_two_columns)
        {
            return (x - 240) + 70 + (120 * (idx / items_per_column));
        }

        return (x - 240) + 8 +
               ((label_padding - static_cast<int>(s.size())) * char_size);
    };

    const auto print_label = [this, &get_label_x, &y, &buf](
                                 std::string_view s) {
        if(_two_columns && s.size() > 13)
        {
            s.remove_suffix(s.size() - 13);
        }

        for(std::size_t i = 0; i < s.size(); ++i)
        {
            buf[i] = s[i];
        }
        buf[s.size()] = '\0';

        M_Print(get_label_x(s), y, buf);
    };

    const auto print_as_float_str = [&buf, &x, &y](const float value) {
        snprintf(buf, sizeof(buf), "%.4f", value);
        M_Print(x, y, buf);
    };

    const auto print_as_int_str = [&buf, &x, &y](const int value) {
        snprintf(buf, sizeof(buf), "%d", value);
        M_Print(x, y, buf);
    };

    const auto print_as_bool_str = [&buf, &x, &y](const bool value) {
        snprintf(buf, sizeof(buf), value ? "On" : "Off");
        M_Print(x, y, buf);
    };

    const auto print_as_str = [&buf, &x, &y](const std::string_view value) {
        snprintf(buf, sizeof(buf), "%s", value.data());
        M_Print(x, y, buf);
    };

    const auto print_tooltip = [&buf](const std::string_view value) {
        snprintf(buf, sizeof(buf), "%s", value.data());
        M_PrintWhiteByWrapping(28, 340, 50, buf);
    };

    {
        const auto& curr_tooltip = access(_cursor_idx)._common._tooltip;
        if(!curr_tooltip.empty())
        {
            print_tooltip(curr_tooltip);
        }
    }

    for(const impl::menu_entry& e : _entries)
    {
        const std::string_view& e_label = e._common._label;

        const auto match_entry = [&](auto& self, auto&& entry) -> void {
            quake::util::match(
                std::forward<decltype(entry)>(entry), //
                [&](const impl::menu_entry_value<bool>& entry) {
                    print_label(e_label);
                    print_as_bool_str(*(entry._getter()));
                },
                [&](const impl::menu_entry_cvar<float>& entry) {
                    print_label(e_label);

                    const float value = entry._cvar_getter()->value;
                    if(entry._printer == nullptr)
                    {
                        print_as_float_str(value);
                    }
                    else
                    {
                        entry._printer(buf, sizeof(buf), value);
                        M_Print(x, y, buf);
                    }
                },
                [&](const impl::menu_entry_cvar<int>& entry) {
                    print_label(e_label);

                    const int value = entry._cvar_getter()->value;
                    if(entry._printer == nullptr)
                    {
                        print_as_int_str(value);
                    }
                    else
                    {
                        entry._printer(buf, sizeof(buf), value);
                        M_Print(x, y, buf);
                    }
                },
                [&](const impl::menu_entry_cvar<bool>& entry) {
                    print_label(e_label);
                    print_as_bool_str(entry._cvar_getter()->value);
                },
                [&](const impl::menu_entry_value_labeled_cvar<int>& entry) {
                    print_label(e_label);
                    print_as_str(entry._value_label_fn(
                        static_cast<int>(entry._cvar_getter()->value)));
                },
                [&](const impl::menu_entry_value_labeled<int>& entry) {
                    print_label(e_label);
                    print_as_str(entry._value_label_fn(
                        static_cast<int>(*entry._getter())));
                },
                [&](const impl::menu_entry_action&) {
                    print_label(e_label);

                    if(!_two_columns)
                    {
                        print_as_str("(X)");
                    }
                },
                [&](const impl::menu_entry_action_slider& entry) {
                    print_label(e_label);
                    M_DrawSlider(x + 10, y, entry._range());
                },
                [&](const impl::menu_entry_separator&) {},
                [&](const impl::menu_entry_ptr& ptr) {
                    self(self, ptr._ptr->_variant);
                });
        };

        match_entry(match_entry, e._variant);

        if(_cursor_idx == idx)
        {
            if(_two_columns)
            {
                M_DrawCharacter(
                    (x - 240) + (70 - 15) + (120 * (idx / items_per_column)), y,
                    12 + ((int)(realtime * 4) & 1));
            }
            else
            {
                M_DrawCharacter(x - 10, y, 12 + ((int)(realtime * 4) & 1));
            }
        }

        ++idx;
        y += 8;

        if(_two_columns)
        {
            if(idx % 25 == 0)
            {
                y = 32 + 16;
            }
        }
    }
}

[[nodiscard]] int menu::cursor_idx() noexcept
{
    return _cursor_idx;
}

[[nodiscard]] const std::string_view& menu::title() noexcept
{
    return _title;
}

[[nodiscard]] int menu::entry_count() const noexcept
{
    return static_cast<int>(_entries.size());
}

[[nodiscard]] bool menu::empty() const noexcept
{
    return _entries.empty();
}

} // namespace quake
