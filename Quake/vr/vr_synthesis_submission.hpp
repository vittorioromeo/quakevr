// CPU-only freshness contract shared by native GL and GL->Vulkan submission.
#pragma once
#include <cstdint>

namespace qvr::synthesis
{
class SubmissionContent
{
public:
    struct Eye
    {
        std::uint32_t motionIndex{0}, depthIndex{0};
        bool acquired{false}, completed{false}, ready{false};
    };
    void begin()
    {
        if(++serial == 0) ++serial;
        eyes[0] = eyes[1] = {};
    }
    [[nodiscard]] std::uint64_t frameId() const { return serial; }
    [[nodiscard]] bool acquire(int eye, std::uint32_t motion, std::uint32_t depth)
    {
        if(eye < 0 || eye > 1 || serial == 0 || eyes[eye].acquired) return false;
        eyes[eye] = {motion, depth, true, false, false};
        return true;
    }
    [[nodiscard]] bool complete(int eye, std::uint64_t contentFrame,
        bool valid, bool motionReleased, bool depthReleased)
    {
        if(eye < 0 || eye > 1) return false;
        auto& e = eyes[eye];
        const bool fresh = e.acquired && !e.completed && contentFrame == serial;
        e.completed = true;
        e.ready = fresh && valid && motionReleased && depthReleased;
        return e.ready;
    }
    [[nodiscard]] bool ready() const { return eyes[0].ready && eyes[1].ready; }
    [[nodiscard]] const Eye& eye(int index) const { return eyes[index]; }
private:
    std::uint64_t serial{0};
    Eye eyes[2];
};
}
