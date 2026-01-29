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

struct Thread {
    const RtlStmt* stmt;     // current statement
    const RtlStmt* next;     // next statement after delay
};

class Kernel {
public:
    Kernel() = default;

    void load_design(const RtlDesign *design);

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
    void exec_stmt(Thread &th);

private:
    const RtlDesign *design_ = nullptr;
    VcdWriter *vcd_ = nullptr;

    uint64_t cur_time_ = 0;
    uint64_t cur_delta_ = 0;

    std::priority_queue<ScheduledProcess> pq_;
    std::vector<Process> nba_queue_;

    // Signal storage: name -> 4-state value
    std::unordered_map<std::string, Value> signals_;

    // Internal: processes we build from RTL (to keep lambdas alive)
    std::vector<Process> rtl_processes_;

    // Level-sensitive watchers: @a, @(a or b), @(*), always_comb
    std::unordered_map<std::string, std::vector<Process*>> level_watchers_;

    // Edge-sensitive watchers: @(posedge clk), @(negedge rst)
    std::unordered_map<std::string, std::vector<Process*>> posedge_watchers_;
    std::unordered_map<std::string, std::vector<Process*>> negedge_watchers_;

    void run_active_region(uint64_t target_time);
    void run_nba_region();

    // ---- RTL wiring helpers ----
    void init_signals_from_rtl();
    void build_processes_from_rtl();

    // Expression evaluation
    Value eval_expr(const RtlExpr &e);
    uint64_t eval_delay(const RtlExpr &e);

    // Helpers to read/write signals
    Value get_signal_value(const std::string &name, std::size_t width);
    void drive_signal(const std::string &name, const Value &v, bool nba);

    // Dependency registration
    void register_level_dependency(const std::string &sig, Process *p);
    void register_posedge_dependency(const std::string &sig, Process *p);
    void register_negedge_dependency(const std::string &sig, Process *p);
    void register_expr_dependencies(const RtlExpr &e, Process *p);
};

} // namespace sv

#endif // __KERNEL_HPP__
