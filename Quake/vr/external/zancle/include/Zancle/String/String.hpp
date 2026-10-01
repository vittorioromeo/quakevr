#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Config.hpp" // IWYU pragma: keep

#include "Zancle/Fmt/FmtAppendMixinFwd.hpp"

#include "Zancle/String/StringView.hpp"

#include "Zancle/Container/Priv/VectorUtils.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsSame.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief A string class with Small String Optimization (SSO)
///
/// This class provides a replacement for `std::string`, designed
/// to fit within the Base module. It avoids heap allocations
/// for small strings by storing them within the object itself.
///
/// Inherits `FmtAppendMixin`, which exposes `.appendFmt(fmt, args...)`
/// for formatted append. Include `<Zancle/Fmt/FmtAppendMixin.hpp>`
/// at the call site to bring in the template body.
///
////////////////////////////////////////////////////////////
class [[nodiscard]] ZA_GSL_OWNER(char) ZA_SYSTEM_API String : public FmtAppendMixin
{
public:
    ////////////////////////////////////////////////////////////
    ZA_ENABLE_TRIVIAL_RELOCATION;


    ////////////////////////////////////////////////////////////
    using iterator       = char*;
    using const_iterator = const char*;
    using value_type     = char;


    ////////////////////////////////////////////////////////////
    enum [[nodiscard]] : SizeT
    {
        nPos       = static_cast<SizeT>(-1),
        ssoSize    = sizeof(char*) + sizeof(SizeT) * 2,
        maxSsoSize = ssoSize - 1,
    };


private:
    ////////////////////////////////////////////////////////////
    enum [[nodiscard]] : unsigned char
    {
        flagIsHeap = 0x01, // Flag bit in the last byte of the union to indicate non-SSO
    };


    ////////////////////////////////////////////////////////////
    // `buffer[maxSsoSize]` (last byte of the union) overlaps with byte
    // `sizeof(SizeT) - 1` of `capacityAndFlag` in memory. Which *numeric*
    // bit that byte corresponds to depends on endianness:
    //   LE: last memory byte = MSB → flag at bit `((sizeof(SizeT) - 1) * 8)`
    //   BE: last memory byte = LSB → flag at bit `0`
    //
    // GCC/Clang always define `__BYTE_ORDER__`; MSVC is always little-endian.
    ////////////////////////////////////////////////////////////
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    static constexpr SizeT heapFlagMask = static_cast<SizeT>(flagIsHeap);
#else
    static constexpr SizeT heapFlagMask = static_cast<SizeT>(flagIsHeap) << ((sizeof(SizeT) - 1) * 8);
#endif


    ////////////////////////////////////////////////////////////
    struct [[nodiscard]] HeapRep
    {
        char* data{nullptr};
        SizeT size{0u};
        SizeT capacityAndFlag{0u}; // Stores `(capacity << 1) | heapFlagMask`
    };


    ////////////////////////////////////////////////////////////
    struct [[nodiscard]] SsoRep
    {
        alignas(HeapRep) char buffer[ssoSize]{};
    };


    ////////////////////////////////////////////////////////////
    union [[nodiscard]] RepUnion
    {
        SsoRep  sso{};
        HeapRep heap;
    } m_rep;


public:
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool isSso() const noexcept
    {
        return (m_rep.sso.buffer[maxSsoSize] & flagIsHeap) == 0;
    }


private:
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void setSsoSize(const SizeT size) noexcept
    {
        m_rep.sso.buffer[maxSsoSize] = static_cast<char>((maxSsoSize - size) << 1);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT getSsoSize() const noexcept
    {
        return maxSsoSize - static_cast<SizeT>(m_rep.sso.buffer[maxSsoSize] >> 1);
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void setHeap(char* const data, const SizeT size, const SizeT capacity) noexcept
    {
        m_rep.heap.data            = data;
        m_rep.heap.size            = size;
        m_rep.heap.capacityAndFlag = (capacity << 1) | heapFlagMask;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT getHeapCapacity() const noexcept
    {
        return (m_rep.heap.capacityAndFlag & ~heapFlagMask) >> 1;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void setSize(const SizeT newSize) noexcept
    {
        if (isSso())
            setSsoSize(newSize);
        else
            m_rep.heap.size = newSize;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void setSizeAndTerminate(const SizeT newSize) noexcept
    {
        setSize(newSize);
        data()[newSize] = '\0';
    }


    ////////////////////////////////////////////////////////////
    /// \brief Make `*this` an empty SSO string (without freeing any heap buffer)
    ///
    /// Used on moved-from strings: the heap representation's bytes are left
    /// in the SSO buffer, so the terminator must be rewritten too.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void resetToEmptySso() noexcept
    {
        setSsoSize(0u);
        m_rep.sso.buffer[0] = '\0';
    }


    void createFrom(const char* cStr, SizeT count);
    void grow(SizeT minCapacity);


public:
    ////////////////////////////////////////////////////////////
    constexpr /* implicit */ String() noexcept : m_rep{}
    {
        setSizeAndTerminate(0);
    }


    ////////////////////////////////////////////////////////////
    /* implicit */ String(const char* cStr);
    explicit String(const char* cStr, SizeT count);
    explicit String(StringView view);


    ////////////////////////////////////////////////////////////
    /// \brief `count` copies of `c` (like `std::string(count, c)`)
    ///
    ////////////////////////////////////////////////////////////
    explicit String(SizeT count, char c);


    ////////////////////////////////////////////////////////////
    template <typename AnsiStringLike>
        requires(isSame<typename AnsiStringLike::value_type, char> &&
                 requires(const AnsiStringLike& s) {
                     s.data();
                     s.size();
                 })
    explicit String(const AnsiStringLike& ansiString) : String{ansiString.data(), ansiString.size()}
    {
    }


    ////////////////////////////////////////////////////////////
    String(const String& other);


    ////////////////////////////////////////////////////////////
    String(String&& other) noexcept;


    ////////////////////////////////////////////////////////////
    constexpr ~String()
    {
        if (!isSso()) // must match the `priv::VectorUtils::allocate<char>` in `String.cpp`
            priv::VectorUtils::deallocate(m_rep.heap.data, getHeapCapacity() + 1u);
    }


    ////////////////////////////////////////////////////////////
    String& operator=(const String& other);
    String& operator=(String&& other) noexcept;
    String& operator=(const char* cStr);
    String& operator=(StringView view);


    ////////////////////////////////////////////////////////////
    String& operator+=(char c);
    String& operator+=(const String& other);
    String& operator+=(const char* cStr);
    String& operator+=(StringView view);


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr iterator begin() noexcept ZA_LIFETIMEBOUND
    {
        return data();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const_iterator begin() const noexcept ZA_LIFETIMEBOUND
    {
        return data();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const_iterator cbegin() const noexcept ZA_LIFETIMEBOUND
    {
        return data();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr iterator end() noexcept ZA_LIFETIMEBOUND
    {
        return data() + size();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const_iterator end() const noexcept ZA_LIFETIMEBOUND
    {
        return data() + size();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const_iterator cend() const noexcept ZA_LIFETIMEBOUND
    {
        return data() + size();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr SizeT size() const noexcept
    {
        return isSso() ? getSsoSize() : m_rep.heap.size;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr SizeT capacity() const noexcept
    {
        return isSso() ? maxSsoSize : getHeapCapacity();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr bool empty() const noexcept
    {
        return size() == 0;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const char* data() const noexcept ZA_LIFETIMEBOUND
    {
        return isSso() ? m_rep.sso.buffer : m_rep.heap.data;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr char* data() noexcept ZA_LIFETIMEBOUND
    {
        return isSso() ? m_rep.sso.buffer : m_rep.heap.data;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr const char* cStr() const noexcept ZA_LIFETIMEBOUND
    {
        return data();
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr char& operator[](const SizeT index) noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(index < size() + 1u);
        return data()[index];
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr char operator[](const SizeT index) const noexcept
    {
        ZA_ASSERT(index < size() + 1u);
        return data()[index];
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr char& front() noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return data()[0];
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr char front() const noexcept
    {
        ZA_ASSERT(!empty());
        return data()[0];
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr char& back() noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(!empty());
        return data()[size() - 1u];
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr char back() const noexcept
    {
        ZA_ASSERT(!empty());
        return data()[size() - 1u];
    }


    ////////////////////////////////////////////////////////////
    void clear() noexcept;
    void pushBack(char ch);
    void popBack() noexcept;


    ////////////////////////////////////////////////////////////
    String& append(const String& str);
    String& append(StringView view);
    String& append(char c);
    String& append(const char* cStr);
    String& append(const char* cStr, SizeT count);
    String& append(SizeT count, char c); // `count` copies of `c`


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] constexpr StringView toStringView() const noexcept ZA_LIFETIMEBOUND
    {
        return StringView{data(), size()};
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] constexpr operator StringView() const noexcept ZA_LIFETIMEBOUND
    {
        return toStringView();
    }


    ////////////////////////////////////////////////////////////
    void reserve(SizeT newCapacity);
    void resize(SizeT newSize, char c = '\0');


    ////////////////////////////////////////////////////////////
    /// \brief Resize without zero-initializing the new range, then let `op`
    ///        write the contents and report the final size.
    ///
    /// Mirrors `std::string::resize_and_overwrite` (C++23). `op(buf, newSize)`
    /// is invoked with a pointer to the storage and the requested size; it
    /// must return the actual number of characters to keep, which is then
    /// used as the new size (and a null terminator is written there).
    ///
    /// The first `min(size(), newSize)` bytes retain their previous values;
    /// the remaining bytes up to `newSize` are indeterminate before `op` runs.
    ///
    ////////////////////////////////////////////////////////////
    template <typename Operation>
    [[gnu::always_inline]] void resizeAndOverwrite(const SizeT newSize, Operation&& op)
    {
        if (newSize > capacity())
            grow(newSize);

        const auto actualSize = static_cast<SizeT>(static_cast<Operation&&>(op)(data(), newSize));
        ZA_ASSERT(actualSize <= newSize && "resizeAndOverwrite: returned size exceeds requested size");

        setSizeAndTerminate(actualSize);
    }


    ////////////////////////////////////////////////////////////
    void erase(SizeT index, SizeT count = nPos);
    void assign(const char* cStr, SizeT count);
    void insert(SizeT pos, char c);
    void insert(SizeT pos, const char* cStr);
    void insert(SizeT pos, StringView view);


    ////////////////////////////////////////////////////////////
    /// \brief Replace `count` characters starting at `pos` with `replacement`.
    ///
    /// `count == nPos` (or any value extending past the end) is clamped to
    /// `size() - pos`, mirroring `erase`.
    ///
    ////////////////////////////////////////////////////////////
    void replace(SizeT pos, SizeT count, StringView replacement);


    ////////////////////////////////////////////////////////////
    /// \brief Find the first occurrence of `target` and replace it with `replacement`.
    ///
    /// \return `true` if a replacement occurred, `false` if `target` was not
    ///         found or was empty (an empty `target` is treated as no-match).
    ///
    ////////////////////////////////////////////////////////////
    bool replaceFirstOccurrence(StringView target, StringView replacement);


    ////////////////////////////////////////////////////////////
    /// \brief Replace every non-overlapping occurrence of `target` with `replacement`.
    ///
    /// Iteration resumes immediately past each replacement, so it terminates
    /// even when `replacement` itself contains `target`. An empty `target`
    /// performs no work and returns 0.
    ///
    /// \return Number of replacements performed.
    ///
    ////////////////////////////////////////////////////////////
    SizeT replaceAllOccurrences(StringView target, StringView replacement);


    ////////////////////////////////////////////////////////////
    friend ZA_SYSTEM_API void swap(String& lhs, String& rhs) noexcept;


    ////////////////////////////////////////////////////////////
    void swap(String& other) noexcept;


////////////////////////////////////////////////////////////
// Bridge macro: forward `methodName(...)` to the equivalent on `StringView`.
// The optional trailing argument is appended after the member function qualifiers.
#define ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(methodName, ...)                      \
    decltype(auto) methodName(auto&&... args) const __VA_ARGS__                 \
    {                                                                           \
        return toStringView().methodName(static_cast<decltype(args)>(args)...); \
    }                                                                           \
                                                                                \
    static_assert(true)

    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(substrByPosLen,
                                                                                                 ZA_LIFETIMEBOUND);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(find);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(rfind);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(findFirstOf);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(findLastOf);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(findFirstNotOf);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(findLastNotOf);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(startsWith);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(endsWith);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(contains);
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(compare);
    [[gnu::always_inline, gnu::flatten]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(forSplits);
    [[gnu::always_inline, gnu::flatten]] ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE(forLines);

#undef ZA_PRIV_DEFINE_STRING_VIEW_BRIDGE
};


////////////////////////////////////////////////////////////
[[nodiscard]] ZA_SYSTEM_API String operator+(char lhs, const String& rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(const String& lhs, char rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(const char* lhs, const String& rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(const String& lhs, const char* rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(StringView lhs, const String& rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(const String& lhs, StringView rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(const String& lhs, const String& rhs);


////////////////////////////////////////////////////////////
// Rvalue overloads: reuse the lhs's buffer via `+=` instead of allocating
[[nodiscard]] ZA_SYSTEM_API String operator+(String&& lhs, const String& rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(String&& lhs, char rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(String&& lhs, const char* rhs);
[[nodiscard]] ZA_SYSTEM_API String operator+(String&& lhs, StringView rhs);


////////////////////////////////////////////////////////////
[[nodiscard, gnu::flatten, gnu::always_inline]] inline constexpr bool operator==(const String& lhs, const StringView rhs) noexcept
{
    return lhs.toStringView() == rhs; // handles empty views without storage
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr bool operator==(const StringView lhs, const String& rhs) noexcept
{
    return rhs == lhs;
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr bool operator==(const String& lhs, const String& rhs) noexcept
{
    return lhs.toStringView() == rhs.toStringView();
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr bool operator==(const String& lhs, const char* const rhs) noexcept
{
    return lhs.toStringView() == StringView(rhs);
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr bool operator==(const char* const lhs, const String& rhs) noexcept
{
    return StringView(lhs) == rhs.toStringView();
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr bool operator<(const String& lhs, const String& rhs) noexcept
{
    return lhs.toStringView() < rhs.toStringView();
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr bool operator<=(const String& lhs, const String& rhs) noexcept
{
    return lhs.toStringView() <= rhs.toStringView();
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr bool operator>(const String& lhs, const String& rhs) noexcept
{
    return lhs.toStringView() > rhs.toStringView();
}


////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline constexpr bool operator>=(const String& lhs, const String& rhs) noexcept
{
    return lhs.toStringView() >= rhs.toStringView();
}


////////////////////////////////////////////////////////////
// Ordering against C strings. Without these, the only viable overload
// would take two `String`s, constructing (and possibly allocating) a
// temporary `String` from the C string on every comparison. (Comparisons
// against `StringView` use `StringView`'s own operators.)
////////////////////////////////////////////////////////////
#define ZA_PRIV_DEFINE_STRING_CSTR_ORDERING(op)                                                                            \
    [[nodiscard, gnu::always_inline]] inline constexpr bool operator op(const String& lhs, const char* const rhs) noexcept \
    {                                                                                                                      \
        return lhs.toStringView() op StringView{rhs};                                                                      \
    }                                                                                                                      \
                                                                                                                           \
    [[nodiscard, gnu::always_inline]] inline constexpr bool operator op(const char* const lhs, const String& rhs) noexcept \
    {                                                                                                                      \
        return StringView{lhs} op rhs.toStringView();                                                                      \
    }

ZA_PRIV_DEFINE_STRING_CSTR_ORDERING(<)
ZA_PRIV_DEFINE_STRING_CSTR_ORDERING(<=)
ZA_PRIV_DEFINE_STRING_CSTR_ORDERING(>)
ZA_PRIV_DEFINE_STRING_CSTR_ORDERING(>=)

#undef ZA_PRIV_DEFINE_STRING_CSTR_ORDERING

} // namespace za


namespace za::literals
{
////////////////////////////////////////////////////////////
[[nodiscard, gnu::always_inline]] inline String operator""_s(const char* const cStr, const SizeT len)
{
    return String{cStr, len};
}

} // namespace za::literals
