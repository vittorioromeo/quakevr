#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/Macros.hpp"

#include "Zancle/Trait/IsFunction.hpp"
#include "Zancle/Trait/IsInvocableR.hpp"
#include "Zancle/Trait/IsPointer.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/RemoveCVRef.hpp"
#include "Zancle/Trait/RemoveReference.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Non-owning, lightweight reference to a callable
///
////////////////////////////////////////////////////////////
template <typename TSignature>
class FunctionRef;


////////////////////////////////////////////////////////////
/// \brief Non-owning, lightweight reference to a callable
///
/// `FunctionRef` is a simplified alternative to `std::function_ref`
/// intended for use as a function parameter, to pass any callable
/// (free function, lambda, functor) without type-erasure allocation
/// and without copying the callable.
///
/// The referenced callable must outlive the `FunctionRef`. In typical
/// use as a function parameter this is automatically the case because
/// the caller's argument lives through the full-expression of the call.
///
////////////////////////////////////////////////////////////
template <typename TReturn, typename... Ts>
class FunctionRef<TReturn(Ts...)>
{
private:
    ////////////////////////////////////////////////////////////
    using FnPtrType = TReturn (*)(Ts...);
    using ThunkType = TReturn (*)(void*, Ts&&...);


    ////////////////////////////////////////////////////////////
    void*     m_obj;
    ThunkType m_thunk;

public:
    ////////////////////////////////////////////////////////////
    FunctionRef() = delete;


    ////////////////////////////////////////////////////////////
    /// \brief Construct from a function pointer (must be non-null)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] FunctionRef(FnPtrType fn) noexcept :
        m_obj{reinterpret_cast<void*>(fn)},
        m_thunk{[](void* o, Ts&&... args) -> TReturn { return reinterpret_cast<FnPtrType>(o)(ZA_FORWARD(args)...); }}
    {
        ZA_ASSERT(fn != nullptr);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct from a function pointer (or function) with a compatible signature
    ///
    /// The pointer is stored by value, like with an exact signature,
    /// rather than referenced (which would dangle once the pointer
    /// temporary is destroyed). Must be non-null.
    ///
    ////////////////////////////////////////////////////////////
    template <typename TFnReturn, typename... TFnArgs>
        requires za::isInvocableR<TFnReturn (*&)(TFnArgs...), TReturn, Ts...>
    [[nodiscard, gnu::always_inline]] FunctionRef(TFnReturn (*fn)(TFnArgs...)) noexcept :
        m_obj{reinterpret_cast<void*>(fn)},
        m_thunk{[](void* o, Ts&&... args) -> TReturn
    {
        const auto typedFn = reinterpret_cast<TFnReturn (*)(TFnArgs...)>(o);

        if constexpr (ZA_IS_SAME(TReturn, void))
            typedFn(ZA_FORWARD(args)...); // Discard any result
        else
            return typedFn(ZA_FORWARD(args)...);
    }}
    {
        ZA_ASSERT(fn != nullptr);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct from any callable object (lambda, functor, ...)
    ///
    /// The callable is referenced, not copied. Its lifetime must
    /// extend across any subsequent calls to `operator()`. Only
    /// participates in overload resolution if the callable is
    /// invocable with the signature's arguments and return type.
    /// Functions and function pointers use the constructors above.
    ///
    ////////////////////////////////////////////////////////////
    template <typename TFFwd>
        requires(!za::isSame<za::RemoveCVRefIndirect<TFFwd>, FunctionRef> &&
                 !za::isPointer<za::RemoveCVRefIndirect<TFFwd>> && !za::isFunction<za::RemoveCVRefIndirect<TFFwd>> &&
                 za::isInvocableR<TFFwd&, TReturn, Ts...>)
    [[nodiscard, gnu::always_inline]] FunctionRef(TFFwd&& f ZA_LIFETIMEBOUND) noexcept :
        m_obj{const_cast<void*>(static_cast<const void*>(&f))},
        m_thunk{[](void* o, Ts&&... args) -> TReturn
    {
        using UnrefType = ZA_REMOVE_REFERENCE(TFFwd);

        if constexpr (ZA_IS_SAME(TReturn, void))
            (*reinterpret_cast<UnrefType*>(o))(ZA_FORWARD(args)...); // Discard any result
        else
            return (*reinterpret_cast<UnrefType*>(o))(ZA_FORWARD(args)...);
    }}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Invoke the referenced callable
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] TReturn operator()(Ts... args) const
    {
        return m_thunk(m_obj, ZA_FORWARD(args)...);
    }
};

} // namespace za


////////////////////////////////////////////////////////////
/// \class za::FunctionRef
/// \ingroup system
///
/// Non-owning, lightweight reference to a callable, intended for
/// use as a function parameter. Does not allocate, does not copy
/// the referenced callable.
///
////////////////////////////////////////////////////////////
