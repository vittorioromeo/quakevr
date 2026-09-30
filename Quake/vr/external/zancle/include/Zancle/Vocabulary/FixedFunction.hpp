#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Abort.hpp"
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Launder.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/MaxAlignT.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/PlacementNew.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/Decay.hpp"
#include "Zancle/Trait/IsCopyConstructible.hpp"
#include "Zancle/Trait/IsInvocableR.hpp"
#include "Zancle/Trait/IsPointer.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsTriviallyCopyable.hpp"
#include "Zancle/Trait/RemoveCVRef.hpp"

// TODO P1: provide triviallyrelocatable version

namespace za
{
////////////////////////////////////////////////////////////
/// \brief Non-allocating `std::function` alternative with fixed storage size
///
////////////////////////////////////////////////////////////
template <typename TSignature, SizeT TStorageSize>
class FixedFunction;


////////////////////////////////////////////////////////////
/// \brief Non-allocating `std::function` alternative with fixed storage size
///
/// Every callable (including function pointers) is stored in the internal
/// buffer. Trivially copyable callables (function pointers, most lambdas)
/// are copied and moved with `memcpy`, without any indirect call.
///
/// Move-only callables are supported, but copying a `FixedFunction`
/// that holds one aborts at run time (the callable type is erased).
///
////////////////////////////////////////////////////////////
template <typename TReturn, typename... Ts, SizeT TStorageSize>
class FixedFunction<TReturn(Ts...), TStorageSize>
{
private:
    ////////////////////////////////////////////////////////////
    enum class Operation : unsigned char
    {
        Destroy       = 0u, //!< Destroy `target`
        CopyConstruct = 1u, //!< Copy-construct `target` from `source`
        Relocate      = 2u, //!< Move-construct `target` from `source`, then destroy `source`
    };


    ////////////////////////////////////////////////////////////
    using RetType = TReturn;


    ////////////////////////////////////////////////////////////
    using FnPtrType   = RetType (*)(Ts...);
    using InvokerType = RetType (*)(char*, Ts&&...);
    using ManagerType = void (*)(char* target, char* source, Operation operation);


    ////////////////////////////////////////////////////////////
    alignas(MaxAlignT) char objStorage[TStorageSize];
    InvokerType m_invokerPtr; //!< `nullptr` if empty
    ManagerType m_managerPtr; //!< `nullptr` if empty or if the stored callable is trivially copyable


    ////////////////////////////////////////////////////////////
    template <typename StoredType, typename TFFwd>
    [[gnu::always_inline]] void emplace(TFFwd&& f)
    {
        static_assert(sizeof(StoredType) <= TStorageSize);
        static_assert(alignof(StoredType) <= alignof(MaxAlignT));

        ZA_PLACEMENT_NEW(objStorage) StoredType(ZA_FORWARD(f));

        // NOLINTNEXTLINE(readability-non-const-parameter)
        m_invokerPtr = [](char* s, Ts&&... xs) -> RetType
        {
            if constexpr (ZA_IS_SAME(RetType, void))
                (*ZA_LAUNDER_CAST(StoredType*, s))(ZA_FORWARD(xs)...); // Discard any result
            else
                return (*ZA_LAUNDER_CAST(StoredType*, s))(ZA_FORWARD(xs)...);
        };

        // Trivially copyable callables have no manager: they are copied/moved
        // with `memcpy` and there is nothing to destroy
        if constexpr (!ZA_IS_TRIVIALLY_COPYABLE(StoredType))
            m_managerPtr = makeManager<StoredType>();
    }


    ////////////////////////////////////////////////////////////
    template <typename StoredType>
    [[nodiscard, gnu::always_inline]] static ManagerType makeManager() noexcept
    {
        // NOLINTNEXTLINE(readability-non-const-parameter)
        return [](char* target, char* source, const Operation operation)
        {
            ZA_ASSERT(target != nullptr);

            if (operation == Operation::Destroy)
            {
                ZA_LAUNDER_CAST(StoredType*, target)->~StoredType();
                return;
            }

            ZA_ASSERT(source != nullptr);
            auto* const sourceObj = ZA_LAUNDER_CAST(StoredType*, source);

            if (operation == Operation::Relocate)
            {
                ZA_PLACEMENT_NEW(target) StoredType(ZA_MOVE(*sourceObj));
                sourceObj->~StoredType();
                return;
            }

            ZA_ASSERT(operation == Operation::CopyConstruct);

            if constexpr (isCopyConstructible<StoredType>)
            {
                ZA_PLACEMENT_NEW(target) StoredType(static_cast<const StoredType&>(*sourceObj));
            }
            else
            {
                ZA_ASSERT(false && "Cannot copy a `FixedFunction` holding a move-only callable");
                za::abort();
            }
        };
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] void destroyAndReset() noexcept
    {
        if (m_managerPtr != nullptr)
            m_managerPtr(objStorage, nullptr, Operation::Destroy);

        m_invokerPtr = nullptr;
        m_managerPtr = nullptr;
    }


    ////////////////////////////////////////////////////////////
    // Precondition: `*this` is empty
    void copyFrom(const FixedFunction& rhs)
    {
        if (rhs.m_invokerPtr == nullptr)
            return;

        if (rhs.m_managerPtr == nullptr)
            ZA_MEMCPY(objStorage, rhs.objStorage, TStorageSize);
        else
            rhs.m_managerPtr(objStorage, const_cast<char*>(rhs.objStorage), Operation::CopyConstruct);

        m_invokerPtr = rhs.m_invokerPtr;
        m_managerPtr = rhs.m_managerPtr;
    }


    ////////////////////////////////////////////////////////////
    // Precondition: `*this` is empty. Leaves `rhs` empty.
    void relocateFrom(FixedFunction& rhs) noexcept
    {
        if (rhs.m_invokerPtr == nullptr)
            return;

        if (rhs.m_managerPtr == nullptr)
            ZA_MEMCPY(objStorage, rhs.objStorage, TStorageSize);
        else
            rhs.m_managerPtr(objStorage, rhs.objStorage, Operation::Relocate);

        m_invokerPtr = rhs.m_invokerPtr;
        m_managerPtr = rhs.m_managerPtr;

        rhs.m_invokerPtr = nullptr;
        rhs.m_managerPtr = nullptr;
    }

public:
    ////////////////////////////////////////////////////////////
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    [[nodiscard]] FixedFunction() noexcept : m_invokerPtr{nullptr}, m_managerPtr{nullptr}
    {
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct from a callable, stored (decayed) in internal storage by copy or move
    ///
    /// Only participates in overload resolution if the callable is
    /// invocable with the signature's arguments and return type.
    /// Functions are stored as function pointers, and a null function
    /// pointer results in an empty function.
    ///
    ////////////////////////////////////////////////////////////
    template <typename TFFwd>
        requires(!za::isSame<za::RemoveCVRefIndirect<TFFwd>, FixedFunction> &&
                 za::isInvocableR<za::RemoveCVRefIndirect<TFFwd>&, TReturn, Ts...>)
    [[nodiscard]] FixedFunction(TFFwd&& f) : FixedFunction()
    {
        using StoredType = ZA_DECAY(TFFwd); // not `ZA_REMOVE_CVREF`: a function type cannot be stored

        if constexpr (ZA_IS_POINTER(ZA_REMOVE_CVREF(TFFwd))) // a function reference is never null
            if (f == nullptr)
                return;

        emplace<StoredType>(ZA_FORWARD(f));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Construct from a function pointer; a null pointer results in an empty function
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] FixedFunction(FnPtrType f) noexcept : FixedFunction()
    {
        if (f != nullptr)
            emplace<FnPtrType>(f);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit FixedFunction(decltype(nullptr)) noexcept : FixedFunction()
    {
    }


    ////////////////////////////////////////////////////////////
    FixedFunction(const FixedFunction& rhs) : FixedFunction()
    {
        copyFrom(rhs);
    }


    ////////////////////////////////////////////////////////////
    FixedFunction& operator=(const FixedFunction& rhs)
    {
        if (this == &rhs)
            return *this;

        destroyAndReset();
        copyFrom(rhs);

        return *this;
    }


    ////////////////////////////////////////////////////////////
    FixedFunction& operator=(decltype(nullptr)) noexcept
    {
        destroyAndReset();
        return *this;
    }


    ////////////////////////////////////////////////////////////
    FixedFunction(FixedFunction&& rhs) noexcept : FixedFunction()
    {
        relocateFrom(rhs);
    }


    ////////////////////////////////////////////////////////////
    FixedFunction& operator=(FixedFunction&& rhs) noexcept
    {
        if (this == &rhs)
            return *this;

        destroyAndReset();
        relocateFrom(rhs);

        return *this;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] ~FixedFunction() noexcept
    {
        if (m_managerPtr != nullptr)
            m_managerPtr(objStorage, nullptr, Operation::Destroy);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Invoke the wrapped callable
    ///
    /// Mirrors `std::function::operator() const`. The wrapped callable
    /// is invoked through a non-`const` path regardless of `*this`'s
    /// const-ness, so a callable whose `operator()` is non-`const`
    /// will mutate its captured state. This is a *logical* const
    /// violation. Wrap mutable closures in a non-`const`
    /// `FixedFunction` if you don't want this behavior.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] RetType operator()(Ts... args) const
    {
        ZA_ASSERT(m_invokerPtr != nullptr);
        return m_invokerPtr(const_cast<char*>(objStorage), ZA_FORWARD(args)...);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] explicit operator bool() const
    {
        return m_invokerPtr != nullptr;
    }
};

} // namespace za


////////////////////////////////////////////////////////////
/// \class za::FixedFunction
/// \ingroup system
///
/// Non-allocating `std::function` alternative with fixed storage size
///
////////////////////////////////////////////////////////////
