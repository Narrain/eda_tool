// path: tests/synth/unit/test_synth_basic.cpp
#include <cassert>
#include <iostream>

#include "../../../src/ir/rtl_ir.hpp"
#include "../../../src/synth/synth_driver.hpp"

int main() {
    using namespace sv;

    RtlDesign rd;
    RtlModule m;
    m.name = "top";

    RtlAssign a;
    a.kind = RtlAssignKind::Continuous;
    a.lhs_name = "y";
    auto lhs = std::make_unique<RtlExpr>(RtlExprKind::Ref);
    lhs->ref_name = "a";
    auto rhs = std::make_unique<RtlExpr>(RtlExprKind::Ref);
    rhs->ref_name = "b";
    auto bin = std::make_unique<RtlExpr>(RtlExprKind::Binary);
    bin->bin_op = RtlBinOp::And;
    bin->lhs = std::move(lhs);
    bin->rhs = std::move(rhs);
    a.rhs = std::move(bin);

    m.continuous_assigns.push_back(a);
    rd.modules.push_back(m);

    SynthDriver sd(rd);
    NetlistDesign nd = sd.run();

    assert(nd.modules.size() == 1);
    const auto &nm = nd.modules[0];
    assert(nm.name == "top");
    bool found_y = false;
    bool found_gate = false;
    for (const auto &n : nm.nets) {
        if (n.name == "y") found_y = true;
    }
    for (const auto &g : nm.gates) {
        if (g.kind == GateKind::And) found_gate = true;
    }
    assert(found_y);
    assert(found_gate);

    std::cout << "test_synth_basic: PASS\n";
    return 0;
}
