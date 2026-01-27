// path: tests/synth/regression/test_synth_regression.cpp
#include <cassert>
#include <iostream>

#include "../../../src/ir/rtl_ir.hpp"
#include "../../../src/synth/synth_driver.hpp"

int main() {
    using namespace sv;

    RtlDesign rd;
    RtlModule m;
    m.name = "alu";

    auto make_bin = [](const std::string &lhs_name,
                       const std::string &a_name,
                       const std::string &b_name,
                       RtlBinOp op) {
        RtlAssign a;
        a.kind = RtlAssignKind::Continuous;
        a.lhs_name = lhs_name;
        auto a1 = std::make_unique<RtlExpr>(RtlExprKind::Ref);
        a1->ref_name = a_name;
        auto b1 = std::make_unique<RtlExpr>(RtlExprKind::Ref);
        b1->ref_name = b_name;
        auto bin = std::make_unique<RtlExpr>(RtlExprKind::Binary);
        bin->bin_op = op;
        bin->lhs = std::move(a1);
        bin->rhs = std::move(b1);
        a.rhs = std::move(bin);
        return a;
    };

    m.continuous_assigns.push_back(make_bin("y_and", "a", "b", RtlBinOp::And));
    m.continuous_assigns.push_back(make_bin("y_or", "a", "b", RtlBinOp::Or));
    m.continuous_assigns.push_back(make_bin("y_xor", "a", "b", RtlBinOp::Xor));

    rd.modules.push_back(m);

    SynthDriver sd(rd);
    NetlistDesign nd = sd.run();

    assert(nd.modules.size() == 1);
    const auto &nm = nd.modules[0];
    int and_cnt = 0, or_cnt = 0, xor_cnt = 0;
    for (const auto &g : nm.gates) {
        if (g.kind == GateKind::And) and_cnt++;
        if (g.kind == GateKind::Or) or_cnt++;
        if (g.kind == GateKind::Xor) xor_cnt++;
    }
    assert(and_cnt == 1);
    assert(or_cnt == 1);
    assert(xor_cnt == 1);

    std::cout << "test_synth_regression: PASS\n";
    return 0;
}
