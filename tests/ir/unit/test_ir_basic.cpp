// path: tests/ir/unit/test_ir_basic.cpp
#include <cassert>
#include <iostream>

#include "../../../src/ir/rtl_ir.hpp"

int main() {
    using namespace sv;

    RtlDesign d;
    RtlModule m;
    m.name = "top";
    d.modules.push_back(m);

    assert(d.modules.size() == 1);
    assert(d.modules[0].name == "top");

    std::cout << "test_ir_basic: PASS\n";
    return 0;
}
