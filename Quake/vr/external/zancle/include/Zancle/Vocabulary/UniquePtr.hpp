#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/TrivialAbi.hpp"

#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsBaseOf.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsTriviallyRelocatable.hpp"


namespace za
{
// clang-format off
////////////////////////////////////////////////////////////
/// \brief Default deleter for `UniquePtr`, calls `delete` on the pointer
///
/// Statically rejects deletion of pointers to incomplete or `void` types.
///
////////////////////////////////////////////////////////////
struct ZA_TRIVIAL_ABI UniquePtrDefaultDeleter
{
    template <typename T>
    [[gnu::always_inline]] static constexpr void operator()(T* const ptr) noexcept
    {
        static_assert(!ZA_IS_SAME(T, void), "can't delete pointer to incomplete type");

        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        static_assert(sizeof(T) > 0u, "can't delete pointer to incomplete type");

        delete ptr;
    }
};
// clang-format on


////////////////////////////////////////////////////////////
/// \brief Lightweight `std::unique_ptr` replacement
///
/// Owns a single heap-allocated `T` and destroys it via `TDeleter` on
/// destruction. The deleter is stored as a private base to take
/// advantage of empty base optimization, so a `UniquePtr` with a
/// stateless deleter is the size of a single pointer.
///
/// `UniquePtr` is annotated with `ZA_TRIVIAL_ABI` so it is
/// passed in registers like a raw pointer when ABI rules allow, and
/// is trivially relocatable whenever its deleter is.
///
/// As with `std::unique_ptr`, the deleter is only invoked on non-null
/// pointers.
///
/// Move-only: copy construction and copy assignment are deleted.
///
////////////////////////////////////////////////////////////
template <typename T, typename TDeleter = UniquePtrDefaultDeleter>
class ZA_TRIVIAL_ABI UniquePtr : private TDeleter
{
    template <typename, typename>
    friend class UniquePtr;

private:
    T* m_ptr;

public:
    ////////////////////////////////////////////////////////////
    ZA_ENABLE_TRIVIAL_RELOCATION_IF(ZA_IS_TRIVIALLY_RELOCATABLE(TDeleter));


    ////////////////////////////////////////////////////////////
    /// \brief Default constructor, creates a null pointer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit UniquePtr() noexcept : m_ptr{nullptr}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct a null pointer from `nullptr`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr /* implicit */ UniquePtr(decltype(nullptr)) noexcept : m_ptr{nullptr}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Take ownership of an existing raw pointer
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit UniquePtr(T* ptr) noexcept : m_ptr{ptr}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Take ownership of an existing raw pointer with a custom deleter
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit UniquePtr(T* ptr, const TDeleter& deleter) noexcept :
        TDeleter{deleter},
        m_ptr{ptr}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Destructor, invokes the deleter on the held pointer
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ~UniquePtr() noexcept
    {
        if (m_ptr != nullptr)
            static_cast<TDeleter*>(this)->operator()(m_ptr);
    }


    ////////////////////////////////////////////////////////////
    UniquePtr(const UniquePtr&)            = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;


    ////////////////////////////////////////////////////////////
    /// \brief Move constructor, leaves `rhs` null
    ///
    /// Must not be a template (a template constructor is never a move
    /// constructor): `ZA_TRIVIAL_ABI` is silently ignored on a class
    /// whose copy and move constructors are all deleted, which would
    /// force `UniquePtr` to be passed in memory instead of in a register.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr UniquePtr(UniquePtr&& rhs) noexcept :
        TDeleter{static_cast<TDeleter&&>(rhs)},
        m_ptr{rhs.m_ptr}
    {
        rhs.m_ptr = nullptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Move-construct from a derived/related `UniquePtr`
    ///
    /// Allows storing `UniquePtr<Derived>` in a `UniquePtr<Base>`.
    ///
    ////////////////////////////////////////////////////////////
    template <typename U, typename UDeleter>
    [[nodiscard, gnu::always_inline]] constexpr UniquePtr(UniquePtr<U, UDeleter>&& rhs) noexcept
        requires(isSame<T, U> || isBaseOf<T, U>)
        : TDeleter{static_cast<UDeleter&&>(rhs)}, m_ptr{rhs.m_ptr}
    {
        rhs.m_ptr = nullptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Move-assign from a derived/related `UniquePtr`
    ///
    /// Destroys the previously held object (with the previous deleter)
    /// before stealing the new one. Self-assignment is a no-op.
    ///
    ////////////////////////////////////////////////////////////
    template <typename U, typename UDeleter>
    [[gnu::always_inline]] constexpr UniquePtr& operator=(UniquePtr<U, UDeleter>&& rhs) noexcept
        requires(isSame<T, U> || isBaseOf<T, U>)
    {
        reset(rhs.release());
        (*static_cast<TDeleter*>(this)) = static_cast<UDeleter&&>(rhs);

        return *this;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Get the underlying raw pointer (or `nullptr`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* get() const noexcept
    {
        return m_ptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Dereference the held object (asserts non-null)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr T& operator*() const noexcept
    {
        ZA_ASSERT(m_ptr != nullptr);
        return *m_ptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Member access on the held object (asserts non-null)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr T* operator->() const noexcept
    {
        ZA_ASSERT(m_ptr != nullptr);
        return m_ptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Conversion to `bool`, true when the pointer is non-null
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr explicit operator bool() const noexcept
    {
        return m_ptr != nullptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Equality comparison with `nullptr`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool operator==(decltype(nullptr)) const noexcept
    {
        return m_ptr == nullptr;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Destroy the held object and optionally take ownership of `ptr`
    ///
    /// The new pointer is stored before the old object is destroyed, so
    /// that the destructor never observes a dangling pointer.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void reset(T* const ptr = nullptr) noexcept
    {
        T* const oldPtr = m_ptr;
        m_ptr           = ptr;
        if (oldPtr != nullptr)
            static_cast<TDeleter*>(this)->operator()(oldPtr);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Release ownership without destroying; caller owns the returned pointer
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr T* release() noexcept
    {
        T* const ptr = m_ptr;
        m_ptr        = nullptr;
        return ptr;
    }
};


////////////////////////////////////////////////////////////
/// \brief `std::make_unique` equivalent; always uses brace-initialization
///
////////////////////////////////////////////////////////////
template <typename T, typename... Ts>
[[nodiscard, gnu::always_inline]] inline constexpr UniquePtr<T> makeUnique(Ts&&... xs)
{
    return UniquePtr<T>{new T{static_cast<Ts&&>(xs)...}};
}

} // namespace za
