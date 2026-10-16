#include "../../../Quake/vr/vr_bench_kills.hpp"
#include <cassert>
#include <cstring>

int main()
{
    qvr::bench::KillEvents queue;
    queue.add(12, 123, 0.125, 17, "vr_dummy", "Super Shotgun", 5);
    assert(queue.count == 1 && queue.dropped == 0);
    const auto& e = queue.events[0];
    assert(e.frame == 12 && e.hostFrame == 123 && e.seconds == 0.125 && e.victim == 17 && e.hitKind == 5);
    assert(std::strcmp(e.victimClass, "vr_dummy") == 0 && std::strcmp(e.weapon, "Super Shotgun") == 0);
    char longLabel[100];
    std::memset(longLabel, 'x', 99);
    longLabel[99] = '\0';
    queue.add(13, 124, 0.150, 18, nullptr, longLabel, 5);
    assert(queue.events[1].victimClass[0] == '\0');
    assert(std::strlen(queue.events[1].weapon) == 63);
    for(int i = queue.count; i < queue.capacity + 3; ++i)
        queue.add(i, i, 1.0, i, "monster_army", "shotgun", 5);
    assert(queue.count == queue.capacity && queue.dropped == 3);
    assert(std::strcmp(queue.events[0].weapon, "Super Shotgun") == 0);
    queue.clear();
    assert(queue.count == 0 && queue.dropped == 0);
    queue.add(0, 200, 0.0, 17, "monster_army", nullptr, 0);
    assert(queue.count == 1 && queue.events[0].weapon[0] == '\0' && queue.events[0].frame == 0);
}
