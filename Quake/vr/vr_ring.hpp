// vr_ring.hpp -- a first-in first-out queue in a vector used as a ring: pushed at the back, popped at the front, read
// by position from the oldest. Nothing is allocated once it has its capacity; the elements stay in their slots when
// they are popped or erased, so that what they own (a vector's buffer) is reused by the next one pushed there.

#pragma once

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/Vector.hpp"

namespace qvr
{

template <typename T>
class Ring
{
public:
    explicit Ring(za::SizeT capacity = 0) : slots(capacity) {}

    [[nodiscard]] za::SizeT capacity() const { return slots.size(); }
    [[nodiscard]] za::SizeT size() const { return count; }
    [[nodiscard]] bool empty() const { return count == 0; }
    [[nodiscard]] bool full() const { return count == slots.size(); }

    // The i-th oldest.
    [[nodiscard]] T& operator[](za::SizeT i) { return slots[physical(i)]; }
    [[nodiscard]] const T& operator[](za::SizeT i) const { return slots[physical(i)]; }
    [[nodiscard]] T& front() { return (*this)[0]; }
    [[nodiscard]] T& back() { return (*this)[count - 1]; }

    // The slot after the newest, now the newest: what it holds is a popped element's (or a default one), for the
    // caller to assign. Not when full.
    [[nodiscard]] T& pushBack()
    {
        ZA_ASSERT(!full());
        count++;
        return back();
    }

    void popFront()
    {
        ZA_ASSERT(count > 0);
        head = head + 1 == slots.size() ? 0 : head + 1;
        count--;
    }

    void clear()
    {
        head = 0;
        count = 0;
    }

    // Removes the i-th oldest, the ones before it moving up one (swapped: their slots' contents move with them).
    void eraseAt(za::SizeT i)
    {
        ZA_ASSERT(i < count);
        for(; i > 0; i--)
        {
            za::genericSwap((*this)[i], (*this)[i - 1]);
        }
        popFront();
    }

    // Another capacity; the oldest go first if there are more than that. (Allocates: for a changed setting.)
    void setCapacity(za::SizeT capacity)
    {
        while(count > capacity)
        {
            popFront();
        }
        za::Vector<T> other(capacity);
        for(za::SizeT i = 0; i < count; i++)
        {
            other[i] = ZA_MOVE((*this)[i]);
        }
        slots.swap(other);
        head = 0;
    }

private:
    [[nodiscard]] za::SizeT physical(za::SizeT i) const
    {
        ZA_ASSERT(i < count);
        const za::SizeT p = head + i;
        return p >= slots.size() ? p - slots.size() : p;
    }

    za::Vector<T> slots;
    za::SizeT head{0};
    za::SizeT count{0};
};

} // namespace qvr
