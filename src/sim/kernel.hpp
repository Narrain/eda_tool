// path: src/sim/kernel.hpp
#ifndef __KERNEL_HPP__
#define __KERNEL_HPP__

#include <cstdint>
#include <queue>
#include <vector>
#include <functional>
#include <unordered_map>
#include <string>

#include "process.hpp"
#include "value.hpp"
#include "../ir/rtl_ir.hpp"
#include "vcd_writer.hpp"

namespace sv {

// Simple event-driven kernel with time + delta cycles and NBA region.

struct ScheduledProcess {
    uint64_t time;
    uint64_t delta;
    SchedRegion region;
    Process proc;

    bool operator<(const ScheduledProcess &o) const {
        // priority_queue is max-heap; invert ordering
        if (time != o.time) return time > o.time;
        if (delta != o.delta) return delta > o.delta;
        return static_cast<int>(region) > static_cast<int>(o.region);
    }
};

class Kernel {
public:
    Kernel() = default;

    void load_design(const RtlDesign *design) { design_ = design; }

    void set_vcd(VcdWriter *vcd) { vcd_ = vcd; }

    void set_signal(const std::string &name, const Value &v) {
        signals_[name] = v;
    }

    const Value *get_signal(const std::string &name) const {
        auto it = signals_.find(name);
        if (it == signals_.end()) return nullptr;
        return &it->second;
    }

    void schedule(Process proc, uint64_t delay, SchedRegion region);
    void schedule_nba(Process proc);

    void run(uint64_t max_time);

    uint64_t time() const { return cur_time_; }
    uint64_t delta() const { return cur_delta_; }

private:
    const RtlDesign *design_ = nullptr;
    VcdWriter *vcd_ = nullptr;

    uint64_t cur_time_ = 0;
    uint64_t cur_delta_ = 0;

    std::priority_queue<ScheduledProcess> pq_;
    std::vector<Process> nba_queue_;

    std::unordered_map<std::string, Value> signals_;

    void run_active_region(uint64_t target_time);
    void run_nba_region();
};

} // namespace sv

#endif // __KERNEL_HPP__
