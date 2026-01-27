// path: tests/sim/regression/test_sim_regression.cpp
#include <cassert>
#include <iostream>

#include "../../../src/sim/kernel.hpp"

int main() {
    using namespace sv;

    Kernel k;

    int nba_count = 0;
    int active_count = 0;

    Process active_proc([&](Kernel &kk) {
        active_count++;
        Process nba_proc([&](Kernel &kk2) {
            nba_count++;
            Value v = Value::from_uint(1, 1);
            kk2.set_signal("q", v);
        }, SchedRegion::NBA);
        kk.schedule_nba(std::move(nba_proc));
    }, SchedRegion::Active);

    k.schedule(std::move(active_proc), 0, SchedRegion::Active);
    k.run(5);

    assert(active_count == 1);
    assert(nba_count == 1);
    const Value *q = k.get_signal("q");
    assert(q != nullptr);
    assert(q->to_string() == "1");

    std::cout << "test_sim_regression: PASS\n";
    return 0;
}
