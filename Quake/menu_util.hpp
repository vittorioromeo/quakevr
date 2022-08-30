#pragma once

#include "quakedef.hpp"
#include "mstate.hpp"

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

template <typename T>
struct menu_entry_value_labeled
{
    std::function<T*()> _getter;
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

struct menu_entry;

struct menu_entry_ptr
{
    menu_entry* _ptr;
};

using menu_entry_variant = std::variant< //
    menu_entry_value<bool>,              //
    menu_entry_cvar<float>,              //
    menu_entry_cvar<int>,                //
    menu_entry_cvar<bool>,               //
    menu_entry_value_labeled_cvar<int>,  //
    menu_entry_value_labeled<int>,       //
    menu_entry_action,                   //
    menu_entry_action_slider,            //
    menu_entry_separator,                //
    menu_entry_ptr                       //
    >;

struct menu_entry
{
    menu_entry_common _common;
    menu_entry_variant _variant;

    explicit menu_entry(menu_entry_common c, menu_entry_variant v) noexcept
        : _common{std::move(c)}, _variant{std::move(v)}
    {
    }
};

} // namespace impl

class menu
{
public:
    class entry_handle_base
    {
    protected:
        menu* _menu;
        std::size_t _index;

    public:
        explicit entry_handle_base(menu& m, const std::size_t i) noexcept
            : _menu{&m}, _index{i}
        {
        }

        template <typename F>
        entry_handle_base& hover(F&& f) noexcept
        {
            entry()._common._hover_change = std::forward<F>(f);
            return *this;
        }

        entry_handle_base& tooltip(const std::string_view x) noexcept
        {
            entry()._common._tooltip = x;
            return *this;
        }

        [[nodiscard]] impl::menu_entry& entry() noexcept
        {
            return _menu->access(_index);
        }

        [[nodiscard]] const impl::menu_entry& entry() const noexcept
        {
            return _menu->access(_index);
        }
    };

    template <typename T>
    class entry_handle : public entry_handle_base
    {
    public:
        using entry_handle_base::entry_handle_base;

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
    };

private:
    std::vector<impl::menu_entry> _entries;
    std::string_view _title;
    int _cursor_idx{0};
    std::function<void()> _escape_fn;
    std::function<void(int, impl::menu_entry&)> _key_fn;

    static constexpr int items_per_column = 25;

    // TODO VR: (P2) hack for map menu, make this nicer...
    bool _two_columns{false};

    void assert_valid_idx(const int idx) const;

    impl::menu_entry& access(const int idx) noexcept;

    const impl::menu_entry& access(const int idx) const noexcept;

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

    void key_option(const int key, const int idx);

    [[nodiscard]] bool entry_is_selectable_at(const int idx);

    std::size_t emplace_and_get_handle_impl(
        const impl::menu_entry_common& common,
        impl::menu_entry_variant&& entry_variant) noexcept
    {
        const auto index = _entries.size();
        _entries.emplace_back(common, std::move(entry_variant));
        return index;
    }

    template <typename T, typename... Args>
    entry_handle<T> emplace_and_get_handle(
        const impl::menu_entry_common& common, Args&&... args) noexcept
    {
        return entry_handle<T>{*this,
            emplace_and_get_handle_impl(common,
                impl::menu_entry_variant{T{std::forward<Args>(args)...}})};
    }

    void update_hover(
        const int prev_cursor_idx, const int curr_cursor_idx) noexcept;

    void remove_entry_at(const std::size_t index) noexcept;
    void move_cursor(const int offset) noexcept;

public:
    menu(const std::string_view title, std::function<void()> escape_fn,
        bool two_columns = false) noexcept;

    void clear();

    template <typename F>
    void on_key(F&& f)
    {
        _key_fn = std::forward<F>(f);
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
        const auto enum_labels_fn = [enum_labels...](int x) -> std::string_view
        {
            static std::array<std::string_view, sizeof...(enum_labels)> strs{
                enum_labels...};
            return strs[static_cast<int>(x)];
        };

        return emplace_and_get_handle<impl::menu_entry_value_labeled_cvar<int>>(
            {label}, std::forward<CvarGetter>(cvar_getter),
            menu_bounds<int>{
                1, 0, static_cast<int>(sizeof...(enum_labels)) - 1},
            enum_labels_fn);
    }

    template <typename T, typename Getter, typename... EnumLabels>
    auto add_getter_enum_entry(const std::string_view label, Getter&& getter,
        const EnumLabels... enum_labels)
    {
        const auto enum_labels_fn = [enum_labels...](int x) -> std::string_view
        {
            static std::array<std::string_view, sizeof...(enum_labels)> strs{
                enum_labels...};
            return strs[static_cast<int>(x)];
        };

        return emplace_and_get_handle<impl::menu_entry_value_labeled<int>>(
            {label}, std::forward<Getter>(getter),
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
        return emplace_and_get_handle<impl::menu_entry_action_slider>({label},
            std::forward<FAction>(fAction), std::forward<FRange>(fRange));
    }

    void add_separator();

    auto add_entry_ptr(impl::menu_entry& entry)
    {
        return emplace_and_get_handle<impl::menu_entry_ptr>(
            entry._common, &entry);
    }

    void enter();
    void leave();
    void key(const int key);
    void draw(const int offset_x = 0, const int offset_y = 0);

    [[nodiscard]] int cursor_idx() noexcept;
    [[nodiscard]] const std::string_view& title() noexcept;
    [[nodiscard]] int entry_count() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
};

} // namespace quake
