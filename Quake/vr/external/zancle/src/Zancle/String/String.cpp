// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/String/String.hpp"

#include "Zancle/String/StringView.hpp"

#include "Zancle/Container/Priv/VectorUtils.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Memmove.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strlen.hpp"


namespace
{
////////////////////////////////////////////////////////////
[[gnu::always_inline]] inline za::String operatorPlusImpl(
    const char* const lhs,
    const za::SizeT   lhsSize,
    const char* const rhs,
    const za::SizeT   rhsSize)
{
    za::String result;
    result.reserve(lhsSize + rhsSize);

    result.append(za::StringView{lhs, lhsSize});
    result.append(za::StringView{rhs, rhsSize});

    return result;
}

} // namespace


namespace za
{
////////////////////////////////////////////////////////////
void String::grow(const SizeT minCapacity)
{
    const SizeT oldCapacity   = capacity();
    const SizeT newCapacity   = oldCapacity + oldCapacity / 2u + 8u;
    const SizeT finalCapacity = newCapacity > minCapacity ? newCapacity : minCapacity;

    char* newData = priv::VectorUtils::allocate<char>(finalCapacity + 1u); // +1 for null terminator

    const SizeT currentSize = size();
    ZA_MEMCPY(newData, data(), currentSize);

    if (!isSso())
        priv::VectorUtils::deallocate(m_rep.heap.data, oldCapacity + 1u);

    setHeap(newData, currentSize, finalCapacity);
    data()[currentSize] = '\0'; // `setHeap` doesn't null terminate
}


////////////////////////////////////////////////////////////
void String::createFrom(const char* const cStr, const SizeT count)
{
    ZA_ASSERT(cStr != nullptr);

    if (count <= maxSsoSize)
    {
        ZA_MEMCPY(m_rep.sso.buffer, cStr, count);
        setSizeAndTerminate(count);
    }
    else
    {
        char* const newData = priv::VectorUtils::allocate<char>(count + 1);
        ZA_MEMCPY(newData, cStr, count);
        setHeap(newData, count, count);
        newData[count] = '\0'; // `setHeap` doesn't null terminate
    }
}


////////////////////////////////////////////////////////////
String::String(const char* const cStr) : String{cStr, ZA_STRLEN(cStr)}
{
    ZA_ASSERT(cStr != nullptr);
}


////////////////////////////////////////////////////////////
String::String(const char* const cStr, const SizeT count) : m_rep{}
{
    createFrom(cStr, count);
}


////////////////////////////////////////////////////////////
String::String(const StringView view) : String{view.data(), view.size()}
{
}


////////////////////////////////////////////////////////////
String::String(const String& other) : String{other.data(), other.size()}
{
}


////////////////////////////////////////////////////////////
String::String(String&& other) noexcept : m_rep{}
{
    ZA_MEMCPY(&m_rep, &other.m_rep, sizeof(m_rep));
    other.setSsoSize(0); // prevent double-free
}


////////////////////////////////////////////////////////////
String& String::operator=(const String& other)
{
    if (this == &other)
        return *this;

    return *this = other.toStringView();
}


////////////////////////////////////////////////////////////
String& String::operator=(String&& other) noexcept
{
    if (this == &other)
        return *this;

    // Free our own resources before taking others
    if (!isSso())
        priv::VectorUtils::deallocate(m_rep.heap.data, getHeapCapacity() + 1u);

    ZA_MEMCPY(&m_rep, &other.m_rep, sizeof(m_rep));
    other.setSsoSize(0); // prevent double-free

    return *this;
}


////////////////////////////////////////////////////////////
String& String::operator=(const StringView view)
{
    const char* const src     = view.data();
    const SizeT       newSize = view.size();

    const char* const myData = data();
    const SizeT       mySize = size();

    const bool srcInsideThis = (src >= myData) && (src < myData + mySize);

    if (srcInsideThis)
    {
        const auto offset = static_cast<SizeT>(src - myData);

        // If it fits in current capacity we can memmove in-place (safe for overlap).
        if (newSize <= capacity())
        {
            ZA_MEMMOVE(data(), data() + offset, newSize);
            setSizeAndTerminate(newSize);
            return *this;
        }

        // Need a new allocation; copy from the old buffer before deallocating it.
        char* const newData = priv::VectorUtils::allocate<char>(newSize + 1u);
        ZA_MEMCPY(newData, myData + offset, newSize);
        newData[newSize] = '\0';

        if (!isSso())
            priv::VectorUtils::deallocate(m_rep.heap.data, getHeapCapacity() + 1u);

        setHeap(newData, newSize, newSize);
        return *this;
    }

    if (!isSso() && view.size() <= getHeapCapacity())
    {
        ZA_MEMCPY(m_rep.heap.data, view.data(), view.size());
        m_rep.heap.size         = view.size();
        data()[m_rep.heap.size] = '\0';
        return *this;
    }

    if (!isSso())
        priv::VectorUtils::deallocate(m_rep.heap.data, getHeapCapacity() + 1u);

    createFrom(view.data(), view.size());
    return *this;
}


////////////////////////////////////////////////////////////
String& String::operator=(const char* cStr)
{
    return *this = StringView{cStr};
}


////////////////////////////////////////////////////////////
String& String::operator+=(const char c)
{
    return append(c);
}


////////////////////////////////////////////////////////////
String& String::operator+=(const String& other)
{
    return append(other);
}


////////////////////////////////////////////////////////////
String& String::operator+=(const char* const cStr)
{
    ZA_ASSERT(cStr != nullptr);
    return append(StringView{cStr});
}


////////////////////////////////////////////////////////////
String& String::operator+=(const StringView view)
{
    return append(view);
}


////////////////////////////////////////////////////////////
void String::clear() noexcept
{
    setSizeAndTerminate(0u);
}


////////////////////////////////////////////////////////////
void String::pushBack(const char c)
{
    const SizeT currentSize = size();
    if (currentSize == capacity())
        grow(currentSize + 1);

    char* const d  = data();
    d[currentSize] = c;

    setSizeAndTerminate(currentSize + 1);
}


////////////////////////////////////////////////////////////
void String::popBack() noexcept
{
    ZA_ASSERT(!empty() && "popBack on empty string");
    setSizeAndTerminate(size() - 1u);
}


////////////////////////////////////////////////////////////
String& String::append(const char c)
{
    pushBack(c);
    return *this;
}


////////////////////////////////////////////////////////////
String& String::append(const String& str)
{
    return append(str.toStringView());
}


////////////////////////////////////////////////////////////
String& String::append(const StringView view)
{
    const SizeT otherSize = view.size();

    if (otherSize == 0u)
        return *this;

    const SizeT currentSize = size();

    // Check for self-append
    const char* const src    = view.data();
    const char* const myData = data();

    const bool srcInsideThis = (src >= myData) && (src < myData + currentSize);

    // If we will reallocate, compute offset first so we can still copy from original location
    const SizeT srcOffset = srcInsideThis ? static_cast<SizeT>(src - myData) : 0u;

    if (currentSize + otherSize > capacity())
        grow(currentSize + otherSize);

    char* const newMyData = data(); // May have changed after grow

    if (srcInsideThis)
        ZA_MEMMOVE(newMyData + currentSize, newMyData + srcOffset, otherSize);
    else
        ZA_MEMCPY(newMyData + currentSize, src, otherSize);

    setSizeAndTerminate(currentSize + otherSize);
    return *this;
}


////////////////////////////////////////////////////////////
String& String::append(const char* const cStr)
{
    ZA_ASSERT(cStr != nullptr);
    return append(StringView{cStr});
}


////////////////////////////////////////////////////////////
String& String::append(const char* const cStr, const SizeT count)
{
    ZA_ASSERT(cStr != nullptr);
    return append(StringView{cStr, count});
}


////////////////////////////////////////////////////////////
void String::reserve(const SizeT newCapacity)
{
    if (newCapacity > capacity())
        grow(newCapacity);
}


////////////////////////////////////////////////////////////
void String::resize(const SizeT newSize, const char c)
{
    const SizeT currentSize = size();

    if (newSize < currentSize)
    {
        setSizeAndTerminate(newSize);
        return;
    }

    if (newSize > currentSize)
    {
        reserve(newSize);

        char* const d = data();
        for (SizeT i = currentSize; i < newSize; ++i)
            d[i] = c;

        setSizeAndTerminate(newSize);
    }
}


////////////////////////////////////////////////////////////
void String::erase(const SizeT index, SizeT count)
{
    const SizeT currentSize = size();
    ZA_ASSERT(index <= currentSize && "Index is out of bounds");

    // If count is nPos or goes past the end, clamp it to erase until the end.
    if (count == nPos || index + count > currentSize)
        count = currentSize - index;

    if (count == 0u)
        return;

    const SizeT tailLength = currentSize - (index + count);

    ZA_MEMMOVE(data() + index, data() + index + count, tailLength);
    setSizeAndTerminate(currentSize - count);
}


////////////////////////////////////////////////////////////
void String::assign(const char* const cStr, const SizeT count)
{
    ZA_ASSERT(cStr != nullptr);
    this->operator=(StringView{cStr, count});
}


////////////////////////////////////////////////////////////
void String::insert(const SizeT pos, const char c)
{
    ZA_ASSERT(pos <= size() && "Insertion position is out of bounds");

    const SizeT oldSize = size();
    const SizeT newSize = oldSize + 1u;

    reserve(newSize);

    char* const d = data();

    // Make space for the new character
    if (pos < oldSize)
        ZA_MEMMOVE(d + pos + 1, d + pos, oldSize - pos);

    // Insert the character
    d[pos] = c;

    setSizeAndTerminate(newSize);
}


////////////////////////////////////////////////////////////
void String::insert(const SizeT pos, const char* const cStr)
{
    ZA_ASSERT(pos <= size() && "Insertion position is out of bounds");
    ZA_ASSERT(cStr != nullptr);

    const SizeT insertCount = ZA_STRLEN(cStr);
    if (insertCount == 0u)
        return;

    const SizeT oldSize = size();
    const SizeT newSize = oldSize + insertCount;

    const char* const myData        = data();
    const bool        srcInsideThis = (cStr >= myData) && (cStr < myData + oldSize);

    if (srcInsideThis)
    {
        const String insertedCopy{cStr, insertCount};
        insert(pos, insertedCopy.cStr());
        return;
    }

    reserve(newSize); // Ensure we have enough capacity

    char* const d = data();

    // Make space for the new content by shifting the existing part to the right
    // (use memmove as the source and destination memory regions overlap)
    if (pos < oldSize)
    {
        ZA_MEMMOVE(d + pos + insertCount, // Destination
                   d + pos,               // Source
                   oldSize - pos);        // Number of characters to move
    }

    // Copy new content into the created space and update size
    ZA_MEMCPY(d + pos, cStr, insertCount);
    setSizeAndTerminate(newSize);
}


////////////////////////////////////////////////////////////
void String::replace(const SizeT pos, SizeT count, const StringView replacement)
{
    const SizeT oldSize = size();
    ZA_ASSERT(pos <= oldSize && "Replacement position is out of bounds");

    // Clamp `count` to the rest of the string (matches `erase` semantics).
    if (count == nPos || pos + count > oldSize)
        count = oldSize - pos;

    // Self-aliasing: if `replacement` views into our own buffer, copy it first
    // so the tail-shift below cannot smash it before we read it.
    const char* const myData        = data();
    const bool        srcInsideThis = (replacement.data() >= myData) && (replacement.data() < myData + oldSize);

    if (srcInsideThis)
    {
        const String replacementCopy{replacement};
        replace(pos, count, replacementCopy.toStringView());
        return;
    }

    const SizeT replSize = replacement.size();
    const SizeT newSize  = oldSize - count + replSize;

    // Ensure capacity. Only grow when expanding; this might reallocate and invalidate `data()`.
    if (newSize > oldSize)
        reserve(newSize);

    char* const d          = data();
    const SizeT tailLength = oldSize - pos - count;

    // Shift the tail (`[pos+count, oldSize)`) left or right by `replSize - count`.
    if (replSize != count && tailLength > 0u)
        ZA_MEMMOVE(d + pos + replSize, d + pos + count, tailLength);

    // Copy the replacement into the freshly-sized hole.
    if (replSize > 0u)
        ZA_MEMCPY(d + pos, replacement.data(), replSize);

    setSizeAndTerminate(newSize);
}


////////////////////////////////////////////////////////////
bool String::replaceFirstOccurrence(const StringView target, const StringView replacement)
{
    if (target.empty())
        return false;

    const SizeT pos = toStringView().find(target);
    if (pos == nPos)
        return false;

    replace(pos, target.size(), replacement);
    return true;
}


////////////////////////////////////////////////////////////
SizeT String::replaceAllOccurrences(const StringView target, const StringView replacement)
{
    if (target.empty())
        return 0u;

    // Either input might view into our own buffer. The buffer can move
    // mid-loop (any expanding `replace` may reallocate), so copy aliasing
    // inputs into stable storage once before iterating.
    const char* const myData = data();
    const SizeT       myLen  = size();
    const auto aliasesUs     = [&](const StringView v) { return (v.data() >= myData) && (v.data() < myData + myLen); };

    if (aliasesUs(target) || aliasesUs(replacement))
    {
        const String targetCopy{target};
        const String replacementCopy{replacement};
        return replaceAllOccurrences(targetCopy.toStringView(), replacementCopy.toStringView());
    }

    SizeT count = 0u;
    SizeT pos   = 0u;

    while ((pos = toStringView().find(target, pos)) != nPos)
    {
        replace(pos, target.size(), replacement);
        pos += replacement.size(); // skip past the just-inserted replacement to avoid re-matching it
        ++count;
    }

    return count;
}


////////////////////////////////////////////////////////////
void swap(String& lhs, String& rhs) noexcept
{
#ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

    alignas(String::RepUnion) char temp[sizeof(String::RepUnion)];

    ZA_MEMCPY(&temp, &lhs.m_rep, sizeof(String::RepUnion));
    ZA_MEMCPY(&lhs.m_rep, &rhs.m_rep, sizeof(String::RepUnion));
    ZA_MEMCPY(&rhs.m_rep, &temp, sizeof(String::RepUnion));

#ifdef __GNUC__
    #pragma GCC diagnostic pop
#endif
}


////////////////////////////////////////////////////////////
String operator+(const char lhs, const String& rhs)
{
    return operatorPlusImpl(&lhs, 1u, rhs.data(), rhs.size());
}


////////////////////////////////////////////////////////////
String operator+(const String& lhs, const char rhs)
{
    return operatorPlusImpl(lhs.data(), lhs.size(), &rhs, 1u);
}


////////////////////////////////////////////////////////////
String operator+(const char* const lhs, const String& rhs)
{
    ZA_ASSERT(lhs != nullptr);
    return operatorPlusImpl(lhs, ZA_STRLEN(lhs), rhs.data(), rhs.size());
}


////////////////////////////////////////////////////////////
String operator+(const String& lhs, const char* const rhs)
{
    ZA_ASSERT(rhs != nullptr);
    return operatorPlusImpl(lhs.data(), lhs.size(), rhs, ZA_STRLEN(rhs));
}


////////////////////////////////////////////////////////////
String operator+(const StringView lhs, const String& rhs)
{
    return operatorPlusImpl(lhs.data(), lhs.size(), rhs.data(), rhs.size());
}


////////////////////////////////////////////////////////////
String operator+(const String& lhs, const StringView rhs)
{
    return operatorPlusImpl(lhs.data(), lhs.size(), rhs.data(), rhs.size());
}


////////////////////////////////////////////////////////////
String operator+(const String& lhs, const String& rhs)
{
    return operatorPlusImpl(lhs.data(), lhs.size(), rhs.data(), rhs.size());
}


////////////////////////////////////////////////////////////
String operator+(String&& lhs, const String& rhs)
{
    lhs += rhs;
    return ZA_MOVE(lhs);
}


////////////////////////////////////////////////////////////
String operator+(String&& lhs, const char rhs)
{
    lhs += rhs;
    return ZA_MOVE(lhs);
}


////////////////////////////////////////////////////////////
String operator+(String&& lhs, const char* const rhs)
{
    ZA_ASSERT(rhs != nullptr);
    lhs += rhs;
    return ZA_MOVE(lhs);
}


////////////////////////////////////////////////////////////
String operator+(String&& lhs, const StringView rhs)
{
    lhs += rhs;
    return ZA_MOVE(lhs);
}

} // namespace za
