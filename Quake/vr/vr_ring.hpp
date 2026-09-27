// vr_ring.hpp -- a first-in first-out queue in a vector used as a ring: pushed at the back, popped at the front, read
// by position from the oldest. Nothing is allocated once it has its capacity; the elements stay in their slots when
// they are popped or erased, so that what they own (a vector's buffer) is reused by the next one pushed there.

#pragma once

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace qvr
{

template <typename T>
class Ring
{
public:
    explicit Ring(std::size_t capacity = 0) : slots(capacity) {}

    [[nodiscard]] std::size_t capacity() const { return slots.size(); }
    [[nodiscard]] std::size_t size() const { return count; }
    [[nodiscard]] bool empty() const { return count == 0; }
    [[nodiscard]] bool full() const { return count == slots.size(); }

    // The i-th oldest.
    [[nodiscard]] T& operator[](std::size_t i) { return slots[physical(i)]; }
    [[nodiscard]] const T& operator[](std::size_t i) const { return slots[physical(i)]; }
    [[nodiscard]] T& front() { return (*this)[0]; }
    [[nodiscard]] T& back() { return (*this)[count - 1]; }

    // The slot after the newest, now the newest: what it holds is a popped element's (or a default one), for the
    // caller to assign. Not when full.
    [[nodiscard]] T& pushBack()
    {
        assert(!full());
        count++;
        return back();
    }

    void popFront()
    {
        assert(count > 0);
        head = head + 1 == slots.size() ? 0 : head + 1;
        count--;
    }

    void clear()
    {
        head = 0;
        count = 0;
    }

    // Removes the i-th oldest, the ones before it moving up one (swapped: their slots' contents move with them).
    void eraseAt(std::size_t i)
    {
        assert(i < count);
        for(; i > 0; i--)
        {
            using std::swap;
            swap((*this)[i], (*this)[i - 1]);
        }
        popFront();
    }

    // Another capacity; the oldest go first if there are more than that. (Allocates: for a changed setting.)
    void setCapacity(std::size_t capacity)
    {
        while(count > capacity)
        {
            popFront();
        }
        std::vector<T> other(capacity);
        for(std::size_t i = 0; i < count; i++)
        {
            other[i] = std::move((*this)[i]);
        }
        slots.swap(other);
        head = 0;
    }

private:
    [[nodiscard]] std::size_t physical(std::size_t i) const
    {
        assert(i < count);
        const std::size_t p = head + i;
        return p >= slots.size() ? p - slots.size() : p;
    }

    std::vector<T> slots;
    std::size_t head{0};
    std::size_t count{0};
};

} // namespace qvr
