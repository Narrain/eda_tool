// path: tests/sim/unit/test_sim_kernel_basic.cpp
#include <cassert>
#include <iostream>

#include "../../../src/sim/kernel.hpp"
#include "../../../src/ir/rtl_ir.hpp"

int main() {
    using namespace sv;

    Kernel k;
    RtlDesign d;
    RtlModule m;
    m.name = "top";
    d.modules.push_back(m);
    k.load_design(&d);

    bool ran_p1 = false;
    bool ran_p2 = false;

    Process p1([&](Kernel &kk) {
        ran_p1 = true;
        Value v = Value::from_uint(1, 1);
        kk.set_signal("a", v);
    }, SchedRegion::Active);

    Process p2([&](Kernel &kk) {
        ran_p2 = true;
        const Value *v = kk.get_signal("a");
        assert(v != nullptr);
        assert(v->to_string() == "1");
    }, SchedRegion::Active);

    k.schedule(std::move(p1), 0, SchedRegion::Active);
    k.schedule(std::move(p2), 1, SchedRegion::Active);

    k.run(10);

    assert(ran_p1);
    assert(ran_p2);

    std::cout << "test_sim_kernel_basic: PASS\n";
    return 0;
}
