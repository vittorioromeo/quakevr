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
//         std::vector<int> nearby;         // holds near a hand (nearbyHolds)
//         std::vector<edict_t*> movers;    // (findHolds)
//         auto members() { return std::tie(nearby, movers); }
//     };
//     mem::Scratch<ClimbScratch> scratch{"climb"};
//
//     void findHolds(...)
//     {
//         std::vector<int>& nearby = scratch.nearby; // cleared by its user: it keeps its capacity between calls
//         nearby.clear();

#include <array>
#include <cstddef>
#include <string>
#include <tuple>
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
[[nodiscard]] std::size_t heldBytes(const T&);
[[nodiscard]] inline std::size_t heldBytes(const std::string& s);
template <class A, class B>
[[nodiscard]] std::size_t heldBytes(const std::pair<A, B>& p);
template <class T, class Alloc>
[[nodiscard]] std::size_t heldBytes(const std::vector<T, Alloc>& v);
template <class Alloc>
[[nodiscard]] std::size_t heldBytes(const std::vector<bool, Alloc>& v);
template <class T, std::size_t N>
[[nodiscard]] std::size_t heldBytes(const std::array<T, N>& a);
template <class T, std::size_t N>
[[nodiscard]] std::size_t heldBytes(const T (&a)[N]);
template <class K, class V, class H, class E, class A>
[[nodiscard]] std::size_t heldBytes(const std::unordered_map<K, V, H, E, A>& m);
template <class K, class H, class E, class A>
[[nodiscard]] std::size_t heldBytes(const std::unordered_set<K, H, E, A>& s);

template <class T>
std::size_t heldBytes(const T&)
{
    return 0; // a value that holds no heap memory
}

inline std::size_t heldBytes(const std::string& s)
{
    return s.capacity() > std::string{}.capacity() ? s.capacity() + 1 : 0; // (within the small-string buffer: none)
}

template <class A, class B>
std::size_t heldBytes(const std::pair<A, B>& p)
{
    return heldBytes(p.first) + heldBytes(p.second);
}

template <class T, class Alloc>
std::size_t heldBytes(const std::vector<T, Alloc>& v)
{
    std::size_t n = v.capacity() * sizeof(T);
    for(const T& e : v)
    {
        n += heldBytes(e);
    }
    return n;
}

template <class Alloc>
std::size_t heldBytes(const std::vector<bool, Alloc>& v)
{
    return (v.capacity() + 7) / 8;
}

template <class T, std::size_t N>
std::size_t heldBytes(const std::array<T, N>& a)
{
    std::size_t n = 0;
    for(const T& e : a)
    {
        n += heldBytes(e);
    }
    return n;
}

template <class T, std::size_t N>
std::size_t heldBytes(const T (&a)[N])
{
    std::size_t n = 0;
    for(const T& e : a)
    {
        n += heldBytes(e);
    }
    return n;
}

// A node container: its buckets, and each node's value and two links (an estimate: the library's nodes are about that).
template <class K, class V, class H, class E, class A>
std::size_t heldBytes(const std::unordered_map<K, V, H, E, A>& m)
{
    std::size_t n = m.bucket_count() * sizeof(void*) + m.size() * (sizeof(std::pair<const K, V>) + 2 * sizeof(void*));
    for(const auto& [k, v] : m)
    {
        n += heldBytes(k) + heldBytes(v);
    }
    return n;
}

template <class K, class H, class E, class A>
std::size_t heldBytes(const std::unordered_set<K, H, E, A>& s)
{
    std::size_t n = s.bucket_count() * sizeof(void*) + s.size() * (sizeof(K) + 2 * sizeof(void*));
    for(const K& k : s)
    {
        n += heldBytes(k);
    }
    return n;
}

// Emptied, its memory given back.
template <class T>
void release(T& c)
{
    T empty{};
    std::swap(c, empty);
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
    [[nodiscard]] virtual std::size_t bytes() = 0;
    virtual void release() = 0;

    [[nodiscard]] static Registration* first();
    [[nodiscard]] Registration* next() const { return next_; }

private:
    const char* system_;
    Kind kind_;
    unsigned events_;
    Registration* next_;
};

namespace detail
{

template <class Tuple>
struct ListedSize;

template <class... M>
struct ListedSize<std::tuple<M&...>>
{
    static constexpr std::size_t value = (sizeof(M) + ... + 0);
};

// A set: T's members, as its members() lists them (every member listed: checked by their sizes adding up to T's).
template <class T>
class Set : public T, public Registration
{
    static constexpr std::size_t listed = ListedSize<decltype(std::declval<T&>().members())>::value;
    static_assert(listed <= sizeof(T) && sizeof(T) - listed < alignof(T),
        "a scratch or cache struct lists every member in members()");

public:
    Set(const char* system, Kind kind, unsigned events) noexcept : Registration(system, kind, events) {}

    [[nodiscard]] std::size_t bytes() override
    {
        return std::apply([](auto&... m) { return (heldBytes(m) + ... + std::size_t{0}); }, T::members());
    }

    void release() override
    {
        std::apply([](auto&... m) { (mem::release(m), ...); }, T::members());
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
    std::size_t scratchBytes{0};
    std::size_t cacheBytes{0};
    int scratchSets{0};
    int cacheSets{0};
};
[[nodiscard]] Totals totals();
void printLargest(int count); // "  <system> <kind> <KiB>", the largest first

} // namespace qvr::mem
