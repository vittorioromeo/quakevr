#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp" // IWYU pragma: keep

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/PlacementNew.hpp"

#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsCopyAssignable.hpp"
#include "Zancle/Trait/IsCopyConstructible.hpp"
#include "Zancle/Trait/IsMoveAssignable.hpp"
#include "Zancle/Trait/IsMoveConstructible.hpp"
#include "Zancle/Trait/IsTriviallyCopyAssignable.hpp"
#include "Zancle/Trait/IsTriviallyCopyConstructible.hpp"
#include "Zancle/Trait/IsTriviallyDestructible.hpp"
#include "Zancle/Trait/IsTriviallyMoveAssignable.hpp"
#include "Zancle/Trait/IsTriviallyMoveConstructible.hpp"
#include "Zancle/Trait/IsTriviallyRelocatable.hpp"
#include "Zancle/Trait/RemoveCVRef.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
/// \brief Throws (or aborts) when an empty `Optional` is accessed via `value()`
///
////////////////////////////////////////////////////////////
[[noreturn, gnu::cold]] ZA_SYSTEM_API void onBadOptionalAccess();

} // namespace za::priv


namespace za
{
// clang-format off
////////////////////////////////////////////////////////////
/// \brief Exception type thrown by `Optional::value()` on an empty optional
///
////////////////////////////////////////////////////////////
struct BadOptionalAccess { };


////////////////////////////////////////////////////////////
/// \brief Tag type used to request in-place construction of an `Optional`'s value
///
////////////////////////////////////////////////////////////
inline constexpr struct InPlace  { } inPlace;

////////////////////////////////////////////////////////////
/// \brief Tag type used to construct an empty `Optional` (engaged == false)
///
////////////////////////////////////////////////////////////
inline constexpr struct NullOpt  { } nullOpt;

////////////////////////////////////////////////////////////
/// \brief Tag type used to construct an `Optional` from the result of an invocable
///
////////////////////////////////////////////////////////////
inline constexpr struct FromFunc { } fromFunc;
// clang-format on


////////////////////////////////////////////////////////////
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ZA_PRIV_OPTIONAL_DESTROY_IF_ENGAGED(T, engaged, buffer) \
    do                                                          \
    {                                                           \
        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(T))         \
        {                                                       \
            if (engaged)                                        \
                buffer.obj.~T();                                \
        }                                                       \
    } while (false)
// NOLINTEND(bugprone-macro-parentheses)


////////////////////////////////////////////////////////////
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ZA_PRIV_OPTIONAL_DESTROY(T, buffer)             \
    do                                                  \
    {                                                   \
        if constexpr (!ZA_IS_TRIVIALLY_DESTRUCTIBLE(T)) \
        {                                               \
            buffer.obj.~T();                            \
        }                                               \
    } while (false)
// NOLINTEND(bugprone-macro-parentheses)


////////////////////////////////////////////////////////////
/// \brief Lightweight replacement for `std::optional`
///
/// Wraps an instance of `T` together with a boolean engagement flag,
/// avoiding the heavy `<optional>` standard header. Storage is in-place
/// and never allocates. Special member functions are conditionally
/// `default`ed when `T` is trivially copy/move/destructible to keep the
/// type trivial whenever possible.
///
/// `Optional<T>` propagates trivial relocatability from `T` so that it
/// can be moved with `memcpy` inside Zancle containers when applicable.
///
/// `T`'s move operations must not throw, as `Optional`'s move constructor
/// and move assignment are unconditionally `noexcept`. If constructing
/// the value throws elsewhere (e.g. in `emplace`, `emplaceFromFunc`, or
/// copy assignment into an empty optional), the optional is left empty.
///
////////////////////////////////////////////////////////////
template <typename T>
class [[nodiscard]] Optional
{
private:
    ////////////////////////////////////////////////////////////
    // Assignment can engage or disengage the optional, so a byte-wise
    // copy is only correct if construction and destruction are trivial too
    static inline constexpr bool triviallyCopyAssignable = isTriviallyCopyAssignable<T> &&
                                                           isTriviallyCopyConstructible<T> && isTriviallyDestructible<T>;

    static inline constexpr bool triviallyMoveAssignable = isTriviallyMoveAssignable<T> &&
                                                           isTriviallyMoveConstructible<T> && isTriviallyDestructible<T>;

public:
    ////////////////////////////////////////////////////////////
    ZA_ENABLE_TRIVIAL_RELOCATION_IF(ZA_IS_TRIVIALLY_RELOCATABLE(T));


    ////////////////////////////////////////////////////////////
    /// \brief Default constructor, creates an empty optional
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr /* implicit */ Optional() noexcept : m_engaged{false}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct an empty optional from `nullOpt`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr /* implicit */ Optional(NullOpt) noexcept : m_engaged{false}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct an engaged optional by copying `object`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit Optional(const T& object) :
        m_buffer{inPlace, object},
        m_engaged{true}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct an engaged optional by moving `object`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit Optional(T&& object) noexcept :
        m_buffer{inPlace, ZA_MOVE(object)},
        m_engaged{true}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Copy constructor (non-trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr /* implicit */ Optional(const Optional& rhs)
        requires(!isTriviallyCopyConstructible<T> && isCopyConstructible<T>)
        : m_buffer{Uninit{}}, m_engaged{rhs.m_engaged}
    {
        if (m_engaged)
            ZA_PLACEMENT_NEW(&m_buffer.obj) T(rhs.m_buffer.obj);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Copy constructor (trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr /* implicit */ Optional(const Optional& rhs)
        requires(isTriviallyCopyConstructible<T>)
    = default;


    ////////////////////////////////////////////////////////////
    /// \brief Move constructor (non-trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr /* implicit */ Optional(Optional&& rhs) noexcept
        requires(!isTriviallyMoveConstructible<T> && isMoveConstructible<T>)
        : m_buffer{Uninit{}}, m_engaged{rhs.m_engaged}
    {
        if (m_engaged)
            ZA_PLACEMENT_NEW(&m_buffer.obj) T(ZA_MOVE(rhs.m_buffer.obj));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Move constructor (trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr /* implicit */ Optional(Optional&& rhs)
        requires(isTriviallyMoveConstructible<T>)
    = default;


    ////////////////////////////////////////////////////////////
    /// \brief Destructor, destroys the contained value if engaged (non-trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ~Optional() noexcept
        requires(!isTriviallyDestructible<T>)
    {
        if (m_engaged)
            m_buffer.obj.~T();
    }


    ////////////////////////////////////////////////////////////
    /// \brief Destructor (trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ~Optional() noexcept
        requires(isTriviallyDestructible<T>)
    = default;


    ////////////////////////////////////////////////////////////
    /// \brief Copy assignment (non-trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Optional& operator=(const Optional& rhs)
        requires(!triviallyCopyAssignable && isCopyAssignable<T> && isCopyConstructible<T>)
    {
        if (&rhs == this || (!m_engaged && !rhs.m_engaged))
            return *this;

        if (m_engaged && !rhs.m_engaged)
        {
            m_engaged = false;
            ZA_PRIV_OPTIONAL_DESTROY(T, m_buffer);
        }
        else if (!m_engaged && rhs.m_engaged)
        {
            ZA_PLACEMENT_NEW(&m_buffer.obj) T(rhs.m_buffer.obj);
            m_engaged = true; // only after construction succeeded
        }
        else
        {
            ZA_ASSERT(m_engaged && rhs.m_engaged);
            m_buffer.obj = rhs.m_buffer.obj;
        }

        return *this;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Copy assignment (trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Optional& operator=(const Optional& rhs)
        requires(triviallyCopyAssignable)
    = default;


    ////////////////////////////////////////////////////////////
    /// \brief Move assignment (non-trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Optional& operator=(Optional&& rhs) noexcept
        requires(!triviallyMoveAssignable && isMoveAssignable<T> && isMoveConstructible<T>)
    {
        if (&rhs == this || (!m_engaged && !rhs.m_engaged))
            return *this;

        if (m_engaged && !rhs.m_engaged)
        {
            m_engaged = false;
            ZA_PRIV_OPTIONAL_DESTROY(T, m_buffer);
        }
        else if (!m_engaged && rhs.m_engaged)
        {
            ZA_PLACEMENT_NEW(&m_buffer.obj) T(ZA_MOVE(rhs.m_buffer.obj));
            m_engaged = true; // only after construction succeeded
        }
        else
        {
            ZA_ASSERT(m_engaged && rhs.m_engaged);
            m_buffer.obj = ZA_MOVE(rhs.m_buffer.obj);
        }

        return *this;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Move assignment (trivial overload)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Optional& operator=(Optional&& rhs)
        requires(triviallyMoveAssignable)
    = default;


    ////////////////////////////////////////////////////////////
    /// \brief Disabled in-place constructor: copying another optional makes no sense here
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit Optional(InPlace, const Optional&) = delete;


    ////////////////////////////////////////////////////////////
    /// \brief Disabled in-place constructor: moving another optional makes no sense here
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit Optional(InPlace, Optional&&) = delete;


    ////////////////////////////////////////////////////////////
    /// \brief Construct an engaged optional in-place by forwarding `args` to `T`'s constructor
    ///
    ////////////////////////////////////////////////////////////
    template <typename... Args>
    [[nodiscard, gnu::always_inline]] constexpr explicit Optional(InPlace, Args&&... args) :
        m_buffer{inPlace, ZA_FORWARD(args)...},
        m_engaged{true}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct an engaged optional from `func()` (preserves guaranteed copy elision)
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    [[nodiscard, gnu::always_inline]] constexpr explicit Optional(FromFunc, F&& func) : m_engaged{true}
    {
        ZA_PLACEMENT_NEW(&m_buffer.obj) T(ZA_FORWARD(func)());
    }


    ////////////////////////////////////////////////////////////
    /// \brief Destroy the previous value (if any) and construct a new `T` in-place from `args`
    ///
    /// \return Reference to the newly constructed value
    ///
    ////////////////////////////////////////////////////////////
    template <typename... Args>
    [[gnu::always_inline]] constexpr T& emplace(Args&&... args)
    {
        ZA_PRIV_OPTIONAL_DESTROY_IF_ENGAGED(T, m_engaged, m_buffer);
        m_engaged = false; // in case construction throws

        T& result = *(ZA_PLACEMENT_NEW(&m_buffer.obj) T(ZA_FORWARD(args)...));
        m_engaged = true;

        return result;
    }


    ////////////////////////////////////////////////////////////
    /// \brief `emplace` from `func()`; works when `T` is not movable thanks to copy elision
    ///
    /// \return Reference to the newly constructed value
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    [[gnu::always_inline]] constexpr T& emplaceFromFunc(F&& func)
    {
        ZA_PRIV_OPTIONAL_DESTROY_IF_ENGAGED(T, m_engaged, m_buffer);
        m_engaged = false; // in case construction throws

        T& result = *(ZA_PLACEMENT_NEW(&m_buffer.obj) T(ZA_FORWARD(func)()));
        m_engaged = true;

        return result;
    }


    ////////////////////////////////////////////////////////////
    /// \brief `emplace` if currently empty, otherwise no-op (returns the existing value)
    ///
    /// \return Reference to the contained value
    ///
    ////////////////////////////////////////////////////////////
    template <typename... Args>
    [[gnu::always_inline]] constexpr T& emplaceIfNeeded(Args&&... args)
    {
        if (m_engaged) [[likely]]
            return m_buffer.obj;

        T& result = *(ZA_PLACEMENT_NEW(&m_buffer.obj) T(ZA_FORWARD(args)...));
        m_engaged = true; // only after construction succeeded

        return result;
    }


    ////////////////////////////////////////////////////////////
    /// \brief `emplaceFromFunc` if currently empty, otherwise no-op
    ///
    /// \return Reference to the contained value
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    [[gnu::always_inline]] constexpr T& emplaceFromFuncIfNeeded(F&& func)
    {
        if (m_engaged) [[likely]]
            return m_buffer.obj;

        T& result = *(ZA_PLACEMENT_NEW(&m_buffer.obj) T(ZA_FORWARD(func)()));
        m_engaged = true; // only after construction succeeded

        return result;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Disengage the optional, destroying the contained value if any
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void reset() noexcept
    {
        ZA_PRIV_OPTIONAL_DESTROY_IF_ENGAGED(T, m_engaged, m_buffer);
        m_engaged = false;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Access the contained value; throws `BadOptionalAccess` (or aborts) if empty
    ///
    /// \return Reference to the contained value
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr T& value() & ZA_LIFETIMEBOUND
    {
        if (!m_engaged) [[unlikely]]
            priv::onBadOptionalAccess();

        return m_buffer.obj;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Access the contained value, throwing if empty (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr const T& value() const& ZA_LIFETIMEBOUND
    {
        if (!m_engaged) [[unlikely]]
            priv::onBadOptionalAccess();

        return m_buffer.obj;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Access the contained value, throwing if empty (rvalue overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr T&& value() && ZA_LIFETIMEBOUND
    {
        if (!m_engaged) [[unlikely]]
            priv::onBadOptionalAccess();

        return ZA_MOVE(m_buffer.obj);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Reference to the contained value, or `defaultValue` if empty
    ///
    /// Returns by reference (unlike `std::optional::value_or`) to avoid
    /// copying `defaultValue` in the empty case.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T& valueOr(T& defaultValue ZA_LIFETIMEBOUND) & noexcept ZA_LIFETIMEBOUND
    {
        return m_engaged ? m_buffer.obj : defaultValue;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Return the contained value if engaged, otherwise `defaultValue` (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T& valueOr(
        const T& defaultValue ZA_LIFETIMEBOUND) const& noexcept ZA_LIFETIMEBOUND
    {
        return m_engaged ? m_buffer.obj : defaultValue;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Return the contained value if engaged, otherwise `defaultValue` (rvalue overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T&& valueOr(T&& defaultValue ZA_LIFETIMEBOUND) && noexcept ZA_LIFETIMEBOUND
    {
        return ZA_MOVE(m_engaged ? m_buffer.obj : defaultValue);
    }


    ////////////////////////////////////////////////////////////
    /// \brief `true` if the optional is engaged
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool hasValue() const noexcept
    {
        return m_engaged;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Conversion to `bool`, equivalent to `hasValue()`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr explicit operator bool() const noexcept
    {
        return m_engaged;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Member access on the contained value (asserts engagement)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* operator->() & noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(m_engaged);
        return &m_buffer.obj;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Member access on the contained value (const overload, asserts engagement)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* operator->() const& noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(m_engaged);
        return &m_buffer.obj;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Dereference the contained value (asserts engagement)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T& operator*() & noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(m_engaged);
        return m_buffer.obj;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Dereference the contained value (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T& operator*() const& noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(m_engaged);
        return m_buffer.obj;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Dereference the contained value (rvalue overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T&& operator*() && noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(m_engaged);
        return ZA_MOVE(m_buffer.obj);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Pointer to the contained value, or `nullptr` if empty
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* asPtr() noexcept ZA_LIFETIMEBOUND
    {
        return m_engaged ? &m_buffer.obj : nullptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Get a pointer to the contained value, or `nullptr` if empty (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* asPtr() const noexcept ZA_LIFETIMEBOUND
    {
        return m_engaged ? &m_buffer.obj : nullptr;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator==(const Optional& lhs,
                                                                                         const Optional& rhs) noexcept
        requires requires { *lhs == *rhs; }
    {
        return lhs.m_engaged == rhs.m_engaged && (!lhs.m_engaged || *lhs == *rhs);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator>=(const Optional& lhs,
                                                                                         const Optional& rhs) noexcept
        requires requires { *lhs >= *rhs; }
    {
        return !rhs.m_engaged || (lhs.m_engaged && *lhs >= *rhs);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator<=(const Optional& lhs,
                                                                                         const Optional& rhs) noexcept
        requires requires { *lhs <= *rhs; }
    {
        return !lhs.m_engaged || (rhs.m_engaged && *lhs <= *rhs);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator>(const Optional& lhs,
                                                                                        const Optional& rhs) noexcept
        requires requires { *lhs > *rhs; }
    {
        return lhs.m_engaged && (!rhs.m_engaged || *lhs > *rhs);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend inline constexpr bool operator<(const Optional& lhs,
                                                                                        const Optional& rhs) noexcept
        requires requires { *lhs < *rhs; }
    {
        return rhs.m_engaged && (!lhs.m_engaged || *lhs < *rhs);
    }

private:
    ////////////////////////////////////////////////////////////
    struct Uninit
    {
    };


    ////////////////////////////////////////////////////////////
    union Buffer
    {
        char dummy;
        T    obj;

        // clang-format off
        // Empty state: activates `dummy`, as GCC requires an active union member for
        // constant initialization (e.g. `constinit Optional<T>`), even with GCC 16.x
        [[gnu::always_inline]] constexpr Buffer() noexcept : dummy{} { }

        // Engaged state: constructs the value directly (no dummy store, usable in constant expressions)
        template <typename... Args>
        [[gnu::always_inline]] constexpr explicit Buffer(InPlace, Args&&... args) : obj(ZA_FORWARD(args)...) { }

        // No active member: the caller placement-news `obj` when needed (no dummy store)
        [[gnu::always_inline]] constexpr explicit Buffer(Uninit) noexcept { }

        constexpr ~Buffer() requires(isTriviallyDestructible<T>) = default;
        [[gnu::always_inline]] constexpr ~Buffer() requires(!isTriviallyDestructible<T>) { }

        constexpr Buffer(const Buffer&) = default;
        constexpr Buffer& operator=(const Buffer&) = default;

        constexpr Buffer(Buffer&&) = default;
        constexpr Buffer& operator=(Buffer&&) = default;
        // clang-format on
    } m_buffer;

    bool m_engaged;
};

////////////////////////////////////////////////////////////
#undef ZA_PRIV_OPTIONAL_DESTROY
#undef ZA_PRIV_OPTIONAL_DESTROY_IF_ENGAGED


////////////////////////////////////////////////////////////
/// \brief Construct an `Optional` with element type deduced as `RemoveCVRef<Object>`
///
////////////////////////////////////////////////////////////
template <typename Object>
[[nodiscard, gnu::always_inline]] inline constexpr auto makeOptional(Object&& object)
{
    return Optional<ZA_REMOVE_CVREF(Object)>{ZA_FORWARD(object)};
}


////////////////////////////////////////////////////////////
/// \brief Construct an `Optional<T>` in-place by forwarding `args` to `T`'s constructor
///
////////////////////////////////////////////////////////////
template <typename T, typename... Args>
[[nodiscard, gnu::always_inline]] inline constexpr Optional<T> makeOptional(Args&&... args)
{
    return Optional<T>{inPlace, ZA_FORWARD(args)...};
}


////////////////////////////////////////////////////////////
/// \brief Construct an `Optional` from `f()` with element type deduced from its return type;
///        guaranteed copy elision means this works for non-movable types
///
/// The element type is the decayed return type: a function returning a
/// reference yields an `Optional` holding a copy of the referenced object.
///
////////////////////////////////////////////////////////////
template <typename F>
[[nodiscard, gnu::always_inline]] inline constexpr auto makeOptionalFromFunc(F&& f)
{
    return Optional<ZA_REMOVE_CVREF(decltype(ZA_FORWARD(f)()))>{fromFunc, ZA_FORWARD(f)};
}

} // namespace za
