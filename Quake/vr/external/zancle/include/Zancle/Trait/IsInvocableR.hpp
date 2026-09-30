#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Trait/IsConvertible.hpp"
#include "Zancle/Trait/IsVoid.hpp"
#include "Zancle/Trait/ReferenceConvertsFromTemporary.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
// `Ret` (the type of a call expression) can be returned as `R` without
// binding a returned reference to a temporary (which would dangle)
template <typename Ret, typename R>
inline constexpr bool isResultConvertible = ZA_IS_VOID(R) ||
                                            (isConvertible<Ret, R> && !ZA_REFERENCE_CONVERTS_FROM_TEMPORARY(R, Ret));

} // namespace za::priv


namespace za
{
////////////////////////////////////////////////////////////
/// \brief `true` if `F` can be called with `Args...` and the result converts to `R`
///
/// Any result is accepted if `R` is (possibly cv-qualified) `void`. Like
/// `std::is_invocable_r_v` (C++23), a reference `R` is rejected if the
/// result would have to be materialized as a temporary to bind to it
/// (e.g. `R = const int&` with a callable returning `int` by value).
/// Unlike `std::is_invocable_r_v`, only plain call syntax is supported
/// (no pointers to members), which keeps it cheap to compile.
///
////////////////////////////////////////////////////////////
template <typename F, typename R, typename... Args>
inline constexpr bool isInvocableR = requires(F&& f, Args&&... args) {
    requires priv::isResultConvertible<decltype(static_cast<F&&>(f)(static_cast<Args&&>(args)...)), R>;
};

} // namespace za
