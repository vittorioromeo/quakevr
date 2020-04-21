#pragma once

#include "quakedef.hpp"
#include "util.hpp"

#include <string>
#include <cassert>
#include <array>
#include <tuple>
#include <variant>
#include <vector>
#include <string_view>
#include <functional>

namespace quake
{
    class menu;
}

namespace quake::menu_util
{
    void playMenuSound(const char* sound, float fvol);
    void playMenuDefaultSound();
    void setMenuState(m_state_e state);
    void setMenuState(quake::menu& m, m_state_e state);
} // namespace quake::menu_util

namespace quake
{
    template <typename T>
    struct menu_bounds
    {
        T _inc, _min, _max;

        constexpr menu_bounds(const T inc, const T min, const T max) noexcept
            : _inc{inc}, _min{min}, _max{max}
        {
        }
    };

    template <>
    struct menu_bounds<bool>
    {
    };

    namespace impl
    {
        struct menu_entry_common
        {
            std::string_view _label;
            std::function<void(bool)> _hover_change;
            std::string_view _tooltip;
        };

        template <typename T>
        struct menu_entry_value
        {
            std::function<T*()> _getter;
            menu_bounds<T> _bounds;
        };

        template <typename T>
        struct menu_entry_cvar
        {
            std::function<cvar_t*()> _cvar_getter;
            menu_bounds<T> _bounds;
            std::function<void(char*, int, T)> _printer;
        };

        template <typename T>
        struct menu_entry_value_labeled_cvar
        {
            std::function<cvar_t*()> _cvar_getter;
            menu_bounds<T> _bounds;
            std::function<std::string_view(T)> _value_label_fn;
        };

        struct menu_entry_action
        {
            std::function<void()> _action;
        };

        struct menu_entry_separator
        {
        };

        struct menu_entry_action_slider
        {
            std::function<void(int)> _action_slide;
            std::function<float()> _range;
        };

        using menu_entry_variant = std::variant< //
            menu_entry_value<bool>,              //
            menu_entry_cvar<float>,              //
            menu_entry_cvar<int>,                //
            menu_entry_cvar<bool>,               //
            menu_entry_value_labeled_cvar<int>,  //
            menu_entry_action,                   //
            menu_entry_action_slider,            //
            menu_entry_separator                 //
            >;

        struct menu_entry
        {
            menu_entry_common _common;
            menu_entry_variant _variant;

            explicit menu_entry(
                menu_entry_common c, menu_entry_variant v) noexcept
                : _common{std::move(c)}, _variant{std::move(v)}
            {
            }
        };
    } // namespace impl

    class menu
    {
    public:
        template <typename T>
        class entry_handle
        {
        private:
            menu* _menu;
            std::size_t _index;

        public:
            explicit entry_handle(menu& m, const std::size_t i) noexcept
                : _menu{&m}, _index{i}
            {
            }

            T& operator*() noexcept
            {
                return _menu->access_variant<T>(_index);
            }

            const T& operator*() const noexcept
            {
                return _menu->access_variant<T>(_index);
            }

            T* operator->() noexcept
            {
                return &_menu->access_variant<T>(_index);
            }

            const T* operator->() const noexcept
            {
                return &_menu->access_variant<T>(_index);
            }

            template <typename F>
            entry_handle& hover(F&& f) noexcept
            {
                _menu->access(_index)._common._hover_change =
                    std::forward<F>(f);

                return *this;
            }

            entry_handle& tooltip(const std::string_view x) noexcept
            {
                _menu->access(_index)._common._tooltip = x;
                return *this;
            }
        };

    private:
        std::vector<impl::menu_entry> _entries;
        std::string_view _title;
        int _cursor_idx{0};
        std::function<void()> _escape_fn;

        // TODO VR: (P2) hack for map menu, make this nicer...
        bool _two_columns{false};

        void assert_valid_idx(const int idx) const
        {
            assert(idx >= 0);
            assert(idx < static_cast<int>(_entries.size()));
        }

        impl::menu_entry& access(const int idx) noexcept
        {
            assert_valid_idx(idx);
            return _entries[idx];
        }

        const impl::menu_entry& access(const int idx) const noexcept
        {
            assert_valid_idx(idx);
            return _entries[idx];
        }

        template <typename T>
        T& access_variant(const int idx) noexcept
        {
            return std::get<T>(access(idx)._variant);
        }

        template <typename T>
        const T& access_variant(const int idx) const noexcept
        {
            return std::get<T>(access(idx)._variant);
        }

        void key_option(const int key, const int idx)
        {
            const bool isLeft = (key == K_LEFTARROW);
            const auto adjustF = quake::util::makeMenuAdjuster<float>(isLeft);
            const auto adjustI = quake::util::makeMenuAdjuster<int>(isLeft);

            assert_valid_idx(idx);

            auto& e = _entries[idx];

            quake::util::match(
                e._variant, //
                [&](const impl::menu_entry_value<bool>& x) {
                    bool& v = *(x._getter());
                    v = !v;
                },
                [&](const impl::menu_entry_cvar<float>& x) {
                    const auto& [inc, min, max] = x._bounds;
                    adjustF(*(x._cvar_getter()), inc, min, max);
                },
                [&](const impl::menu_entry_cvar<int>& x) {
                    const auto& [inc, min, max] = x._bounds;
                    adjustI(*(x._cvar_getter()), inc, min, max);
                },
                [&](const impl::menu_entry_cvar<bool>& x) {
                    adjustI(*(x._cvar_getter()), 1, 0, 1);
                },
                [&](const impl::menu_entry_value_labeled_cvar<int>& x) {
                    const auto& [inc, min, max] = x._bounds;
                    adjustI(*(x._cvar_getter()), inc, min, max);
                },
                [&](const impl::menu_entry_action& x) { x._action(); },
                [&](const impl::menu_entry_action_slider& x) {
                    x._action_slide(isLeft ? -1 : 1);
                },
                [&](const impl::menu_entry_separator&) {});
        }

        [[nodiscard]] bool entry_is_selectable_at(const int idx)
        {
            assert_valid_idx(idx);
            return !std::holds_alternative<impl::menu_entry_separator>(
                _entries[idx]._variant);
        }

        template <typename T, typename... Args>
        entry_handle<T> emplace_and_get_handle(
            const impl::menu_entry_common& common, Args&&... args) noexcept
        {
            const auto index = _entries.size();

            _entries.emplace_back(common,
                impl::menu_entry_variant{T{std::forward<Args>(args)...}});

            return entry_handle<T>{*this, index};
        }


    public:
        menu(const std::string_view title, std::function<void()> escape_fn,
            bool two_columns = false) noexcept
            : _title{title}, _escape_fn{std::move(escape_fn)}, _two_columns{
                                                                   two_columns}
        {
        }

        template <typename T, typename CvarGetter>
        auto add_cvar_getter_entry(const std::string_view label,
            CvarGetter&& cvar_getter, const menu_bounds<T> bounds = {})
        {
            return emplace_and_get_handle<impl::menu_entry_cvar<T>>(
                {label}, std::forward<CvarGetter>(cvar_getter), bounds);
        }

        template <typename T>
        auto add_cvar_entry(const std::string_view label, cvar_t& cvar,
            const menu_bounds<T> bounds = {})
        {
            return add_cvar_getter_entry(
                label, [&cvar] { return &cvar; }, bounds);
        }

        template <typename T, typename CvarGetter, typename... EnumLabels>
        auto add_cvar_getter_enum_entry(const std::string_view label,
            CvarGetter&& cvar_getter, const EnumLabels... enum_labels)
        {
            const auto enum_labels_fn = [enum_labels...](
                                            int x) -> std::string_view {
                static std::array<std::string_view, sizeof...(enum_labels)>
                    strs{enum_labels...};
                return strs[static_cast<int>(x)];
            };

            return emplace_and_get_handle<
                impl::menu_entry_value_labeled_cvar<int>>({label},
                std::forward<CvarGetter>(cvar_getter),
                menu_bounds<int>{
                    1, 0, static_cast<int>(sizeof...(enum_labels)) - 1},
                enum_labels_fn);
        }

        template <typename T, typename Getter>
        auto add_getter_entry(const std::string_view label, Getter&& getter,
            const menu_bounds<T> bounds = {})
        {
            return emplace_and_get_handle<impl::menu_entry_value<T>>(
                {label}, std::forward<Getter>(getter), bounds);
        }

        template <typename F>
        auto add_action_entry(const std::string_view label, F&& f)
        {
            return emplace_and_get_handle<impl::menu_entry_action>(
                {label}, std::forward<F>(f));
        }

        template <typename FAction, typename FRange>
        auto add_action_slider_entry(
            const std::string_view label, FAction&& fAction, FRange&& fRange)
        {
            return emplace_and_get_handle<impl::menu_entry_action_slider>(
                {label}, std::forward<FAction>(fAction),
                std::forward<FRange>(fRange));
        }

        void add_separator()
        {
            // TODO VR: (P2) not nice, separator should not have a common
            _entries.emplace_back(
                impl::menu_entry_common{}, impl::menu_entry_separator{});
        }

        void enter()
        {
            // Hover current entry.
            auto& curr_hover_change_fn =
                access(_cursor_idx)._common._hover_change;

            if(curr_hover_change_fn)
            {
                curr_hover_change_fn(true);
            }
        }

        void leave()
        {
            // Un-hover current entry.
            auto& curr_hover_change_fn =
                access(_cursor_idx)._common._hover_change;

            if(curr_hover_change_fn)
            {
                curr_hover_change_fn(false);
            }
        }

        void key(const int key)
        {
            const auto update_hover = [this](const int prev_cursor_idx,
                                          const int curr_cursor_idx) {
                if(curr_cursor_idx == prev_cursor_idx)
                {
                    return;
                }

                auto& prev_hover_change_fn =
                    access(prev_cursor_idx)._common._hover_change;

                if(prev_hover_change_fn)
                {
                    prev_hover_change_fn(false);
                }

                auto& curr_hover_change_fn =
                    access(curr_cursor_idx)._common._hover_change;

                if(curr_hover_change_fn)
                {
                    curr_hover_change_fn(true);
                }
            };

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
                    _escape_fn();

                    leave();

                    break;
                }

                case K_UPARROW:
                {
                    S_LocalSound("misc/menu1.wav");

                    const auto prev_cursor_idx = _cursor_idx;

                    do
                    {
                        --_cursor_idx;
                    } while(_cursor_idx >= 0 &&
                            !entry_is_selectable_at(_cursor_idx));

                    if(_cursor_idx < 0)
                    {
                        _cursor_idx = static_cast<int>(_entries.size() - 1);

                        while(!entry_is_selectable_at(_cursor_idx))
                        {
                            --_cursor_idx;
                        }
                    }

                    update_hover(prev_cursor_idx, _cursor_idx);
                    break;
                }

                case K_DOWNARROW:
                {
                    S_LocalSound("misc/menu1.wav");

                    const auto prev_cursor_idx = _cursor_idx;

                    do
                    {
                        ++_cursor_idx;
                    } while(_cursor_idx < static_cast<int>(_entries.size()) &&
                            !entry_is_selectable_at(_cursor_idx));

                    if(_cursor_idx >= static_cast<int>(_entries.size()))
                    {
                        _cursor_idx = 0;

                        while(!entry_is_selectable_at(_cursor_idx))
                        {
                            ++_cursor_idx;
                        }
                    }

                    update_hover(prev_cursor_idx, _cursor_idx);
                    break;
                }

                case K_LEFTARROW: [[fallthrough]];
                case K_RIGHTARROW:
                {
                    S_LocalSound("misc/menu3.wav");
                    key_option(key, _cursor_idx);
                    break;
                }

                case K_ENTER:
                {
                    m_entersound = true;
                    key_option(key, _cursor_idx);
                    break;
                }
            }
        }

        void draw()
        {
            int y = 4;

            // plaque
            // M_DrawTransPic(16, y, Draw_CachePic("gfx/qplaque.lmp"));

            // customize header
            // qpic_t* p = Draw_CachePic("gfx/ttl_cstm.lmp");
            // M_DrawPic((320 - p->width) / 2, y, p);

            y += 28;

            // title
            M_PrintWhite((320 - 8 * _title.size()) / 2, y, _title.data());
            y += 16;

            char buf[512]{};
            int idx{0};

            constexpr int char_size = 8;
            constexpr int label_padding = 26;

            const auto get_label_x = [&, this](const std::string_view s) {
                if(_two_columns)
                {
                    return 70 + (120 * (idx / 25));
                }

                return 8 + ((label_padding - static_cast<int>(s.size())) *
                               char_size);
            };

            const auto print_label = [&get_label_x, &y](
                                         const std::string_view s) {
                M_Print(get_label_x(s), y, s.data());
            };

            const auto print_as_float_str = [&buf, &y](const float value) {
                snprintf(buf, sizeof(buf), "%.4f", value);
                M_Print(240, y, buf);
            };

            const auto print_as_int_str = [&buf, &y](const int value) {
                snprintf(buf, sizeof(buf), "%d", value);
                M_Print(240, y, buf);
            };

            const auto print_as_bool_str = [&buf, &y](const bool value) {
                snprintf(buf, sizeof(buf), value ? "On" : "Off");
                M_Print(240, y, buf);
            };

            const auto print_as_str = [&buf, &y](const std::string_view value) {
                snprintf(buf, sizeof(buf), "%s", value.data());
                M_Print(240, y, buf);
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

                quake::util::match(
                    e._variant, //
                    [&](const impl::menu_entry_value<bool>& x) {
                        print_label(e_label);
                        print_as_bool_str(*(x._getter()));
                    },
                    [&](const impl::menu_entry_cvar<float>& x) {
                        print_label(e_label);

                        const float value = x._cvar_getter()->value;
                        if(x._printer == nullptr)
                        {
                            print_as_float_str(value);
                        }
                        else
                        {
                            x._printer(buf, sizeof(buf), value);
                        }
                    },
                    [&](const impl::menu_entry_cvar<int>& x) {
                        print_label(e_label);
                        print_as_int_str(x._cvar_getter()->value);
                    },
                    [&](const impl::menu_entry_cvar<bool>& x) {
                        print_label(e_label);
                        print_as_bool_str(x._cvar_getter()->value);
                    },
                    [&](const impl::menu_entry_value_labeled_cvar<int>& x) {
                        print_label(e_label);
                        print_as_str(x._value_label_fn(
                            static_cast<int>(x._cvar_getter()->value)));
                    },
                    [&](const impl::menu_entry_action&) {
                        print_label(e_label);

                        if(!_two_columns)
                        {
                            print_as_str("(X)");
                        }
                    },
                    [&](const impl::menu_entry_action_slider& x) {
                        print_label(e_label);
                        M_DrawSlider(250, y, x._range());
                    },
                    [&](const impl::menu_entry_separator&) {});

                if(_cursor_idx == idx)
                {
                    if(_two_columns)
                    {
                        M_DrawCharacter((70 - 15) + (120 * (idx / 25)), y,
                            12 + ((int)(realtime * 4) & 1));
                    }
                    else
                    {
                        M_DrawCharacter(230, y, 12 + ((int)(realtime * 4) & 1));
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

        [[nodiscard]] auto cursor_idx() noexcept
        {
            return _cursor_idx;
        }
    };
} // namespace quake
