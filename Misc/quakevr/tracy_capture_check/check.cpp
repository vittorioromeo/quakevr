// Headless validation using official Tracy v0.14.1 reader/server; test-only dependencies.
#include "TracyWorker.hpp"
#include "TracyFileRead.hpp"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <set>
#include <unordered_map>
#include <vector>
int main(int argc, char** argv)
{
    if(argc < 2 || argc > 3) { std::fprintf(stderr, "Usage: qvr_tracy_capture_check capture.tracy [--stage2]\n"); return 2; }
    try
    {
        std::unique_ptr<tracy::FileRead> file(tracy::FileRead::Open(argv[1]));
        if(!file) return 2;
        tracy::Worker worker(*file);
        unsigned errors = 0;
        uint64_t zones = 0, truncated = 0, stackZones = 0, qcEvents = 0;
        std::set<std::string> qcNames, annotations, pools;
        std::unordered_map<std::string, uint64_t> names;
        std::function<void(const tracy::ZoneEvent&, const tracy::ZoneEvent*)> zone;
        std::function<void(const tracy::Vector<tracy::short_ptr<tracy::ZoneEvent>>&, const tracy::ZoneEvent*)> timeline;
        zone = [&](const auto& z, const auto* parent) {
            ++zones;
            const auto& source = worker.GetSourceLocation(z.SrcLoc());
            ++names[worker.GetString(source.name.active ? source.name : source.function)];
            const char* name = worker.GetString(source.name.active ? source.name : source.function);
            if(worker.HasZoneExtra(z))
            {
                const auto& extra = worker.GetZoneExtra(z);
                if(extra.callstack.Val()) ++stackZones;
                if(extra.text.Active())
                {
                    const std::string text = worker.GetString(extra.text);
                    if(text == "QC function" || text == "QC builtin") { ++qcEvents; qcNames.insert(name); }
                    if((std::string(name) == "Capture context" || std::string(name) == "QC VM") && annotations.insert(text).second)
                        std::printf("annotation,%s,%s\n", name, text.c_str());
                }
            }
            if(!z.IsEndValid()) ++truncated;
            else if(z.End() < z.Start()) ++errors;
            if(parent && (z.Start() < parent->Start() || (parent->IsEndValid() && z.IsEndValid() && z.End() > parent->End()))) ++errors;
            if(z.Child() >= 0) timeline(worker.GetZoneChildren(z.Child()), &z);
        };
        timeline = [&](const auto& vec, const auto* parent) {
            if(vec.is_magic())
                for(const auto& z : reinterpret_cast<const tracy::Vector<tracy::ZoneEvent>&>(vec)) zone(z, parent);
            else for(const auto& z : vec) zone(*z, parent);
        };
        for(const auto* thread : worker.GetThreadData())
        {
            std::printf("thread,%s,%llu\n", worker.GetThreadName(thread->id), static_cast<unsigned long long>(thread->count));
            timeline(thread->timeline, nullptr);
        }
        for(const auto& [name, count] : names) std::printf("zone,%s,%llu\n", name.c_str(), static_cast<unsigned long long>(count));
        for(const auto& [name, memory] : worker.GetMemNameMap())
        {
            const std::string pool = name ? worker.GetString(name) : "default";
            if(!memory->data.empty()) pools.insert(pool);
            struct Event { int64_t time; uint64_t ptr, bytes; bool alloc; };
            std::vector<Event> events;
            for(const auto& e : memory->data)
            {
                events.push_back({e.TimeAlloc(), e.Ptr(), e.Size(), true});
                if(e.TimeFree() >= 0) events.push_back({e.TimeFree(), e.Ptr(), e.Size(), false});
            }
            std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
            if(!events.empty()) std::printf("memory_first_time,%s,%lld\n", pool.c_str(), static_cast<long long>(events.front().time));
            std::unordered_map<uint64_t, uint64_t> live;
            unsigned invalid = 0;
            for(const auto& e : events)
            {
                if(e.alloc) { if(!live.emplace(e.ptr, e.bytes).second) ++invalid; }
                else if(live.erase(e.ptr) != 1) ++invalid;
            }
            uint64_t bytes = 0;
            for(const auto& [ptr, size] : live) bytes += size;
            errors += invalid;
            std::printf("memory,%s,allocs=%zu,frees=%zu,live=%zu,bytes=%llu,invalid=%u\n", name ? worker.GetString(name) : "default",
                memory->data.size(), memory->frees.size(), live.size(), static_cast<unsigned long long>(bytes), invalid);
            // Match reconstructed allocation/discard/free events to independently reconciled engine checkpoints.
            const std::string checkpointName = "Tracy " + pool + " captured bytes";
            for(const auto* plot : worker.GetPlots())
                if(worker.GetString(plot->name) == checkpointName)
                {
                    size_t at = 0, checkpoints = 0;
                    int64_t reconstructed = 0;
                    unsigned mismatches = 0;
                    for(const auto& point : plot->data)
                    {
                        while(at < events.size() && events[at].time <= point.time.Val())
                        {
                            reconstructed += events[at].alloc ? static_cast<int64_t>(events[at].bytes) : -static_cast<int64_t>(events[at].bytes);
                            ++at;
                        }
                        ++checkpoints;
                        if(static_cast<double>(reconstructed) != point.val)
                        {
                            if(mismatches < 2) std::printf("checkpoint_mismatch,%s,time=%lld,events=%zu,reconstructed=%lld,plotted=%.0f\n", pool.c_str(),
                                static_cast<long long>(point.time.Val()), at, static_cast<long long>(reconstructed), point.val);
                            ++mismatches;
                        }
                    }
                    errors += mismatches;
                    std::printf("checkpoints,%s,count=%zu,mismatches=%u\n", pool.c_str(), checkpoints, mismatches);
                }
        }
        for(const auto* plot : worker.GetPlots())
        {
            std::printf("plot,%s,%zu\n", worker.GetString(plot->name), plot->data.size());
            if(std::string(worker.GetString(plot->name)) == "Tracy pool mismatches")
                for(const auto& point : plot->data) if(point.val != 0) ++errors;
        }
        if(argc == 3 && std::string(argv[2]) == "--stage2")
        {
            for(const char* name : {"QC VM", "Job execute", "Job wait or claim", "Map load", "Model preparation", "Texture preparation", "Texture upload", "File load", "Image decode"})
                if(!names.contains(name)) { std::printf("missing,zone,%s\n", name); ++errors; }
            for(const char* pool : {"Zone", "Hunk", "Cache"}) if(!pools.contains(pool)) { std::printf("missing,pool,%s\n", pool); ++errors; }
            if(!qcEvents || !worker.GetGpuZoneCount() || worker.GetPlots().empty()) ++errors;
        }
        std::printf("summary,cpu=%llu,gpu=%llu,truncated=%llu,named_qc_events=%llu,named_qc_functions=%zu,stack_zones=%llu,stack_payloads=%llu,statistical_samples=%llu,context_switches=%llu,errors=%u\n", static_cast<unsigned long long>(zones),
            static_cast<unsigned long long>(worker.GetGpuZoneCount()), static_cast<unsigned long long>(truncated),
            static_cast<unsigned long long>(qcEvents), qcNames.size(), static_cast<unsigned long long>(stackZones),
            static_cast<unsigned long long>(worker.GetCallstackPayloadCount()), static_cast<unsigned long long>(worker.GetCallstackSampleCount()),
            static_cast<unsigned long long>(worker.GetContextSwitchCount()), errors);
        return errors ? 1 : 0;
    }
    catch(const std::exception& e) { std::fprintf(stderr, "Decode failed: %s\n", e.what()); return 2; }
}
