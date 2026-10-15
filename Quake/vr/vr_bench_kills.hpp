// Fixed storage for diagnostic kill events; no heap allocation or I/O on a death path.
#pragma once

namespace qvr::bench
{
struct KillEvent
{
    int frame{0};
    int hostFrame{0};
    int victim{0};
    int hitKind{0};
    double seconds{0.0};
    char victimClass[64]{};
    char weapon[64]{};
};

struct KillEvents
{
    static constexpr int capacity = 32;
    KillEvent events[capacity]{};
    int count{0};
    int dropped{0};

    static void copyLabel(char (&out)[64], const char* text)
    {
        int n = 0;
        if(text)
        {
            while(text[n] && n < 63)
            {
                out[n] = text[n];
                ++n;
            }
        }
        out[n] = '\0';
    }

    void clear()
    {
        count = dropped = 0;
    }

    void add(int frame, int hostFrame, double seconds, int victim, const char* victimClass,
        const char* weapon, int hitKind)
    {
        if(count == capacity)
        {
            ++dropped;
            return;
        }
        KillEvent& e = events[count++];
        e.frame = frame;
        e.hostFrame = hostFrame;
        e.seconds = seconds;
        e.victim = victim;
        e.hitKind = hitKind;
        copyLabel(e.victimClass, victimClass);
        copyLabel(e.weapon, weapon);
    }
};
} // namespace qvr::bench
