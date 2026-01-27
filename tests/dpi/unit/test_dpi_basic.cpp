// path: tests/dpi/unit/test_dpi_basic.cpp
#include <cassert>
#include <iostream>

#include "../../../src/sim/kernel.hpp"
#include "../../../src/ir/rtl_ir.hpp"
#include "../../../src/dpi/dpi_shim.hpp"

int main() {
    using namespace sv;

    Kernel k;
    RtlDesign rd;
    RtlModule m;
    m.name = "top";
    rd.modules.push_back(m);

    DpiShim shim(k, rd);

    shim.set_signal("a", 1, 1);
    shim.run(5);
    uint64_t v = shim.get_signal("a");
    assert(v == 1);

    std::cout << "test_dpi_basic: PASS\n";
    return 0;
}
