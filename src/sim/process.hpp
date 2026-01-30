#ifndef __PROCESS_HPP__
#define __PROCESS_HPP__

#include <functional>

namespace sv {

class Kernel;

// Scheduling regions for processes
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
