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

namespace quake::menu_util
{
    void playMenuSound(const char* sound, float fvol);
    void playMenuDefaultSound();
    void setMenuState(m_state_e state);
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
        template <typename T>
        struct menu_entry_value
        {
            std::string_view _label;
            std::function<T*()> _getter;
            menu_bounds<T> _bounds;
        };

        template <typename T>
        struct menu_entry_cvar
        {
            std::string_view _label;
            std::function<cvar_t*()> _cvar_getter;
            menu_bounds<T> _bounds;
            std::function<void(char*, int, T)> _printer;
        };

        template <typename T>
        struct menu_entry_value_labeled_cvar
        {
            std::string_view _label;
            std::function<cvar_t*()> _cvar_getter;
            menu_bounds<T> _bounds;
            std::function<std::string_view(T)> _value_label_fn;
        };

        struct menu_entry_action
        {
            std::string_view _label;
            std::function<void()> _action;
        };

        using menu_entry = std::variant<        //
            menu_entry_value<bool>,             //
            menu_entry_cvar<float>,             //
            menu_entry_cvar<int>,               //
            menu_entry_cvar<bool>,              //
            menu_entry_value_labeled_cvar<int>, //
            menu_entry_action                   //
            >;
    } // namespace impl

    class menu
    {
    private:
        std::vector<impl::menu_entry> _entries;
        std::string_view _title;
        int _cursor_idx{0};

        // TODO VR: hack for map menu, make this nicer...
        bool _two_columns{false};

        void key_option(const int key, const int idx)
        {
            const bool isLeft = (key == K_LEFTARROW);
            const auto adjustF = quake::util::makeMenuAdjuster<float>(isLeft);
            const auto adjustI = quake::util::makeMenuAdjuster<int>(isLeft);

            assert(idx >= 0);
            assert(idx < _entries.size());
            auto& e = _entries[idx];

            quake::util::match(
                e, //
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
                [&](const impl::menu_entry_action& x) { x._action(); });
        }


    public:
        menu(
            const std::string_view title, bool two_columns = false) noexcept
            : _title{title}, _two_columns{two_columns}
        {
        }

        template <typename T, typename CvarGetter>
        auto& add_cvar_getter_entry(const std::string_view label,
            CvarGetter&& cvar_getter, const menu_bounds<T> bounds = {})
        {
            auto& v = _entries.emplace_back(impl::menu_entry_cvar<T>{
                label, std::forward<CvarGetter>(cvar_getter), bounds});

            return std::get<impl::menu_entry_cvar<T>>(v);
        }

        template <typename T>
        auto& add_cvar_entry(const std::string_view label, cvar_t& cvar,
            const menu_bounds<T> bounds = {})
        {
            return add_cvar_getter_entry(
                label, [&cvar] { return &cvar; }, bounds);
        }

        template <typename T, typename CvarGetter, typename... EnumLabels>
        auto& add_cvar_getter_enum_entry(const std::string_view label,
            CvarGetter&& cvar_getter, const EnumLabels... enum_labels)
        {
            const auto enum_labels_fn = [enum_labels...](
                                            int x) -> std::string_view {
                static std::array<std::string_view, sizeof...(enum_labels)>
                    strs{enum_labels...};
                return strs[static_cast<int>(x)];
            };

            auto& v =
                _entries.emplace_back(impl::menu_entry_value_labeled_cvar<int>{
                    label, std::forward<CvarGetter>(cvar_getter),
                    menu_bounds<int>{
                        1, 0, static_cast<int>(sizeof...(enum_labels)) - 1},
                    enum_labels_fn});

            return std::get<impl::menu_entry_value_labeled_cvar<int>>(v);
        }

        template <typename T, typename Getter>
        auto& add_getter_entry(const std::string_view label, Getter&& getter,
            const menu_bounds<T> bounds = {})
        {
            auto& v = _entries.emplace_back(impl::menu_entry_value<T>{
                label, std::forward<Getter>(getter), bounds});

            return std::get<impl::menu_entry_value<T>>(v);
        }

        template <typename F>
        auto& add_action_entry(const std::string_view label, F&& f)
        {
            auto& v = _entries.emplace_back(
                impl::menu_entry_action{label, std::forward<F>(f)});

            return std::get<impl::menu_entry_action>(v);
        }

        void key(const int key)
        {
            switch(key)
            {
                case K_ESCAPE:
                {
                    VID_SyncCvars(); // sync cvars before leaving menu. FIXME:
                                     // there are other ways to leave menu
                    S_LocalSound("misc/menu1.wav");
                    M_Menu_Options_f();
                    break;
                }

                case K_UPARROW:
                {
                    S_LocalSound("misc/menu1.wav");

                    _cursor_idx--;
                    if(_cursor_idx < 0)
                    {
                        _cursor_idx = static_cast<int>(_entries.size() - 1);
                    }

                    break;
                }

                case K_DOWNARROW:
                {
                    S_LocalSound("misc/menu1.wav");

                    _cursor_idx++;
                    if(_cursor_idx >= static_cast<int>(_entries.size()))
                    {
                        _cursor_idx = 0;
                    }

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
            M_DrawTransPic(16, y, Draw_CachePic("gfx/qplaque.lmp"));

            // customize header
            qpic_t* p = Draw_CachePic("gfx/ttl_cstm.lmp");
            M_DrawPic((320 - p->width) / 2, y, p);
            y += 28;

            // title
            M_PrintWhite((320 - 8 * _title.size()) / 2, y, _title.data());
            y += 16;

            char buf[32]{};
            int idx{0};

            constexpr int char_size = 8;
            constexpr int label_padding = 26;

            const auto get_label_x = [this, &idx, &char_size, &label_padding](
                                         const std::string_view s) {
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

            for(const auto& e : _entries)
            {
                quake::util::match(
                    e, //
                    [&](const impl::menu_entry_value<bool>& x) {
                        print_label(x._label);
                        print_as_bool_str(*(x._getter()));
                    },
                    [&](const impl::menu_entry_cvar<float>& x) {
                        print_label(x._label);

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
                        print_label(x._label);
                        print_as_int_str(x._cvar_getter()->value);
                    },
                    [&](const impl::menu_entry_cvar<bool>& x) {
                        print_label(x._label);
                        print_as_bool_str(x._cvar_getter()->value);
                    },
                    [&](const impl::menu_entry_value_labeled_cvar<int>& x) {
                        print_label(x._label);
                        print_as_str(x._value_label_fn(
                            static_cast<int>(x._cvar_getter()->value)));
                    },
                    [&](const impl::menu_entry_action& x) {
                        print_label(x._label);

                        if(!_two_columns)
                        {
                            print_as_str("(X)");
                        }
                    });

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
    };
} // namespace quake
