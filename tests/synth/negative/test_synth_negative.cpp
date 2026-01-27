// path: tests/synth/negative/test_synth_negative.cpp
#include <cassert>
#include <iostream>

#include "../../../src/ir/rtl_ir.hpp"
#include "../../../src/synth/synth_driver.hpp"

int main() {
    using namespace sv;

    RtlDesign rd;
    RtlModule m;
    m.name = "neg";

    RtlAssign a;
    a.kind = RtlAssignKind::Continuous;
    a.lhs_name = "y";
    a.rhs.reset(); // null rhs, mapper should not crash

    m.continuous_assigns.push_back(a);
    rd.modules.push_back(m);

    SynthDriver sd(rd);
    NetlistDesign nd = sd.run();

    assert(nd.modules.size() == 1);
    std::cout << "test_synth_negative: PASS\n";
    return 0;
}
