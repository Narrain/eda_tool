// path: tests/uvm/unit/test_uvm_smoke.cpp
#include <cassert>
#include <iostream>

#include "../../../src/sim/kernel.hpp"
#include "../../../src/ir/rtl_ir.hpp"
#include "../../../src/dpi/dpi_shim.hpp"
#include "../../../src/uvm/uvm_env.hpp"

int main() {
    using namespace sv;

    Kernel k;
    RtlDesign rd;
    RtlModule m;
    m.name = "top";
    rd.modules.push_back(m);

    UvmEnv env(k, rd, "in", "out", 8);

    auto &seq = env.sequencer();
    UvmSequenceItem item1;
    item1.data = 0x12;
    UvmSequenceItem item2;
    item2.data = 0x34;
    seq.add_item(item1);
    seq.add_item(item2);

    auto &drv = env.driver();
    drv.run(seq, 10);

    uint64_t last = drv.last_response();
    (void)last; // we only check that nothing crashed

    std::cout << "test_uvm_smoke: PASS\n";
    return 0;
}
