// path: src/sim/process.hpp
#ifndef __PROCESS_HPP__
#define __PROCESS_HPP__

#include <functional>
#include <cstdint>

namespace sv {

class Kernel;

// A simple process abstraction: a callback that runs in a scheduling region.
enum class SchedRegion {
    Preponed,
    Active,
    Inactive,
    NBA,
    Postponed
};

using ProcessFunc = std::function<void(Kernel &)>;

class Process {
public:
    Process() = default;
    Process(ProcessFunc fn, SchedRegion region)
        : fn_(std::move(fn)), region_(region) {}

    void run(Kernel &k) {
        if (fn_) fn_(k);
    }

    SchedRegion region() const { return region_; }

private:
    ProcessFunc fn_;
    SchedRegion region_{SchedRegion::Active};
};

} // namespace sv

#endif // __PROCESS_HPP__
