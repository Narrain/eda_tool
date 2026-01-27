// path: src/sim/kernel.cpp
#include "kernel.hpp"

namespace sv {

void Kernel::schedule(Process proc, uint64_t delay, SchedRegion region) {
    ScheduledProcess sp;
    sp.time = cur_time_ + delay;
    sp.delta = (delay == 0) ? (cur_delta_) : 0;
    sp.region = region;
    sp.proc = std::move(proc);
    pq_.push(std::move(sp));
}

void Kernel::schedule_nba(Process proc) {
    nba_queue_.push_back(std::move(proc));
}

void Kernel::run_active_region(uint64_t target_time) {
    while (!pq_.empty()) {
        auto sp = pq_.top();
        if (sp.time != target_time) break;
        if (sp.region != SchedRegion::Active &&
            sp.region != SchedRegion::Preponed &&
            sp.region != SchedRegion::Inactive) break;

        pq_.pop();
        cur_delta_++;
        sp.proc.run(*this);
    }
}

void Kernel::run_nba_region() {
    if (nba_queue_.empty()) return;
    auto q = std::move(nba_queue_);
    nba_queue_.clear();
    for (auto &p : q) {
        p.run(*this);
    }
}

void Kernel::run(uint64_t max_time) {
    while (!pq_.empty()) {
        auto sp = pq_.top();
        if (sp.time > max_time) break;
        cur_time_ = sp.time;
        cur_delta_ = 0;

        if (vcd_) {
            vcd_->dump_time(cur_time_);
            for (const auto &kv : signals_) {
                vcd_->dump_value(kv.first, kv.second);
            }
        }

        run_active_region(cur_time_);
        run_nba_region();
    }
}

} // namespace sv
