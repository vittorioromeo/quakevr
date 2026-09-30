#pragma once

// vr_mem.hpp -- the VR systems' scratch buffers and caches: each system owns its own, as one struct of containers
// wrapped in mem::Scratch or mem::Cache, which registers it here. Registered sets are
// - released: a scratch set at every map change (a one-off peak isn't kept for the rest of the session), a cache at the
//   events it names (the map, the game directory, a model reload);
// - counted: vr_memstats and vr_limits print their bytes, by system.
// Main thread only: a worker thread's buffers are its own (a local, a thread_local, or a context it is handed), never a
// registered set. How to add one: docs/vr-port/CODE_STYLE.md, "Scratch buffers and caches".
//
//     struct ClimbScratch
//     {
//         za::Vector<int> nearby;          // holds near a hand (nearbyHolds)
//         za::Vector<edict_t*> movers;     // (findHolds)
//         auto members() { return qvr::mem::list(nearby, movers); }
//     };
//     mem::Scratch<ClimbScratch> scratch{"climb"};
//
//     void findHolds(...)
//     {
//         za::Vector<int>& nearby = scratch.nearby;  // cleared by its user: it keeps its capacity between calls
//         nearby.clear();

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Trait/DeclVal.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

// TRANSITION (removed when every file is on Zancle): the std containers' overloads.
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace qvr::mem
{

// What makes a cache stale (a scratch set is released at every map change).
enum Event : unsigned
{
    MapChange = 1u << 0,     // VR_OnClearMemory: a map, a changelevel, a loaded game, a disconnect (the hunk freed)
    GameDirChange = 1u << 1, // VR_OnGameDirChanged: the models' slots reused for other models, other games' files
    ModelReload = 1u << 2,   // vr_model_reload, vr_hand_reload: the VR models' data made again
    Never = 0,               // a cache only counted: released by its owner alone (its data pointed into, say)
};

enum class Kind
{
    Scratch, // reused only to spare allocations: nothing in it means anything from one call to the next
    Cache,   // results kept for their key (a model, a texture, a setting): released on its events
};

// The heap memory a container holds (its capacity, and what its elements hold in turn). Every overload is declared
// first, so that each finds the others for its elements.
template <class T>
[[nodiscard]] za::SizeT heldBytes(const T&);
[[nodiscard]] inline za::SizeT heldBytes(const za::String& s);
template <class A, class B>
[[nodiscard]] za::SizeT heldBytes(const ankerl::unordered_dense::detail::pair<A, B>& p);
template <class T>
[[nodiscard]] za::SizeT heldBytes(const za::Vector<T>& v);
template <class T, za::SizeT N>
[[nodiscard]] za::SizeT heldBytes(const za::SmallVector<T, N>& v);
template <class T, za::SizeT N>
[[nodiscard]] za::SizeT heldBytes(const za::InPlaceVector<T, N>& v);
template <class T, za::SizeT N>
[[nodiscard]] za::SizeT heldBytes(const za::Array<T, N>& a);
template <class T, za::SizeT N>
[[nodiscard]] za::SizeT heldBytes(const T (&a)[N]);
template <class K, class V, class H, class E>
[[nodiscard]] za::SizeT heldBytes(const ankerl::unordered_dense::map<K, V, H, E>& m);
template <class K, class H, class E>
[[nodiscard]] za::SizeT heldBytes(const ankerl::unordered_dense::set<K, H, E>& s);
template <class T>
[[nodiscard]] za::SizeT heldBytes(const za::UniquePtr<T>& p);
// TRANSITION
[[nodiscard]] inline za::SizeT heldBytes(const std::string& s);
template <class A, class B>
[[nodiscard]] za::SizeT heldBytes(const std::pair<A, B>& p);
template <class T, class Alloc>
[[nodiscard]] za::SizeT heldBytes(const std::vector<T, Alloc>& v);
template <class T, za::SizeT N>
[[nodiscard]] za::SizeT heldBytes(const std::array<T, N>& a);
template <class K, class V, class H, class E, class A>
[[nodiscard]] za::SizeT heldBytes(const std::unordered_map<K, V, H, E, A>& m);
template <class K, class H, class E, class A>
[[nodiscard]] za::SizeT heldBytes(const std::unordered_set<K, H, E, A>& s);

inline za::SizeT heldBytes(const std::string& s)
{
    return s.capacity() > std::string{}.capacity() ? s.capacity() + 1 : 0;
}
template <class A, class B>
za::SizeT heldBytes(const std::pair<A, B>& p)
{
    return heldBytes(p.first) + heldBytes(p.second);
}
template <class T, class Alloc>
za::SizeT heldBytes(const std::vector<T, Alloc>& v)
{
    za::SizeT n = v.capacity() * sizeof(T);
    for(const T& e : v)
    {
        n += heldBytes(e);
    }
    return n;
}
template <class T, za::SizeT N>
za::SizeT heldBytes(const std::array<T, N>& a)
{
    za::SizeT n = 0;
    for(const T& e : a)
    {
        n += heldBytes(e);
    }
    return n;
}
template <class K, class V, class H, class E, class A>
za::SizeT heldBytes(const std::unordered_map<K, V, H, E, A>& m)
{
    za::SizeT n = m.bucket_count() * sizeof(void*) + m.size() * (sizeof(std::pair<const K, V>) + 2 * sizeof(void*));
    for(const auto& [k, v] : m)
    {
        n += heldBytes(k) + heldBytes(v);
    }
    return n;
}
template <class K, class H, class E, class A>
za::SizeT heldBytes(const std::unordered_set<K, H, E, A>& s)
{
    za::SizeT n = s.bucket_count() * sizeof(void*) + s.size() * (sizeof(K) + 2 * sizeof(void*));
    for(const K& k : s)
    {
        n += heldBytes(k);
    }
    return n;
}
// TRANSITION end

template <class T>
za::SizeT heldBytes(const T&)
{
    return 0; // a value that holds no heap memory
}

inline za::SizeT heldBytes(const za::String& s)
{
    return s.capacity() > za::String{}.capacity() ? s.capacity() + 1 : 0; // (within the small-string buffer: none)
}

template <class A, class B>
za::SizeT heldBytes(const ankerl::unordered_dense::detail::pair<A, B>& p)
{
    return heldBytes(p.first) + heldBytes(p.second);
}

template <class Range>
za::SizeT heldBytesOfElements(const Range& r)
{
    za::SizeT n = 0;
    for(const auto& e : r)
    {
        n += heldBytes(e);
    }
    return n;
}

template <class T>
za::SizeT heldBytes(const za::Vector<T>& v)
{
    return v.capacity() * sizeof(T) + heldBytesOfElements(v);
}

template <class T, za::SizeT N>
za::SizeT heldBytes(const za::SmallVector<T, N>& v)
{
    return (v.capacity() > N ? v.capacity() * sizeof(T) : 0) + heldBytesOfElements(v); // (within its own buffer: none)
}

template <class T, za::SizeT N>
za::SizeT heldBytes(const za::InPlaceVector<T, N>& v)
{
    return heldBytesOfElements(v);
}

template <class T, za::SizeT N>
za::SizeT heldBytes(const za::Array<T, N>& a)
{
    return heldBytesOfElements(a);
}

template <class T, za::SizeT N>
za::SizeT heldBytes(const T (&a)[N])
{
    return heldBytesOfElements(a);
}

template <class T>
za::SizeT heldBytes(const za::UniquePtr<T>& p)
{
    return p ? sizeof(T) + heldBytes(*p) : 0;
}

// A dense map: its values' vector and its buckets.
template <class K, class V, class H, class E>
za::SizeT heldBytes(const ankerl::unordered_dense::map<K, V, H, E>& m)
{
    using Map = ankerl::unordered_dense::map<K, V, H, E>;
    return m.values().capacity() * sizeof(typename Map::value_type) +
           m.bucket_count() * sizeof(ankerl::unordered_dense::bucket_type::standard) + heldBytesOfElements(m);
}

template <class K, class H, class E>
za::SizeT heldBytes(const ankerl::unordered_dense::set<K, H, E>& s)
{
    return s.values().capacity() * sizeof(K) + s.bucket_count() * sizeof(ankerl::unordered_dense::bucket_type::standard) +
           heldBytesOfElements(s);
}

// Emptied, its memory given back.
template <class T>
void release(T& c)
{
    T empty{};
    za::genericSwap(c, empty);
}

// A registered set (the list is walked by the events and the reports). Static storage only: made before main (other
// files' static initialisers; the list's head is constant-initialised) and never copied.
class Registration
{
public:
    Registration(const char* system, Kind kind, unsigned events) noexcept;
    virtual ~Registration();
    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;

    [[nodiscard]] const char* system() const { return system_; }
    [[nodiscard]] Kind kind() const { return kind_; }
    [[nodiscard]] unsigned events() const { return events_; }
    [[nodiscard]] virtual za::SizeT bytes() = 0;
    virtual void release() = 0;

    [[nodiscard]] static Registration* first();
    [[nodiscard]] Registration* next() const { return next_; }

private:
    const char* system_;
    Kind kind_;
    unsigned events_;
    Registration* next_;
};

// A scratch or cache struct's members, as its members() lists them: `return qvr::mem::list(a, b, c);`.
template <class Visit, class... M>
struct List
{
    Visit visit; // visit(f): f(a, b, c)
    static constexpr za::SizeT listedBytes = (sizeof(M) + ... + 0);
};

template <class... M>
[[nodiscard]] auto list(M&... m)
{
    const auto visit = [&m...](auto&& f) -> decltype(auto) { return f(m...); };
    return List<decltype(visit), M...>{visit};
}

namespace detail
{

// A set: T's members, as its members() lists them (every member listed: checked by their sizes adding up to T's).
template <class T>
class Set : public T, public Registration
{
    static constexpr za::SizeT listed = decltype(za::declVal<T&>().members())::listedBytes;
    static_assert(listed <= sizeof(T) && sizeof(T) - listed < alignof(T),
        "a scratch or cache struct lists every member in members()");

public:
    Set(const char* system, Kind kind, unsigned events) noexcept : Registration(system, kind, events) {}

    [[nodiscard]] za::SizeT bytes() override
    {
        return T::members().visit([](auto&... m) { return (heldBytes(m) + ... + za::SizeT{0}); });
    }

    void release() override
    {
        T::members().visit([](auto&... m) { (mem::release(m), ...); });
    }
};

} // namespace detail

// Buffers reused only to spare allocations (their users clear them): released at every map change.
template <class T>
class Scratch final : public detail::Set<T>
{
public:
    explicit Scratch(const char* system) noexcept : detail::Set<T>(system, Kind::Scratch, MapChange) {}
};

// Results kept for their key: released at the events given (a key that can change otherwise, a setting, is part of the
// key: the cache checks it on use).
template <class T>
class Cache final : public detail::Set<T>
{
public:
    Cache(const char* system, unsigned events) noexcept : detail::Set<T>(system, Kind::Cache, events) {}
};

// The registered sets that name the event released (VR_OnClearMemory, VR_OnGameDirChanged, the model reloads).
void on(Event event);

// Their bytes: the total of each kind, and the sets that hold the most (vr_memstats, vr_limits).
struct Totals
{
    za::SizeT scratchBytes{0};
    za::SizeT cacheBytes{0};
    int scratchSets{0};
    int cacheSets{0};
};
[[nodiscard]] Totals totals();
void printLargest(int count); // "  <system> <kind> <KiB>", the largest first

} // namespace qvr::mem
