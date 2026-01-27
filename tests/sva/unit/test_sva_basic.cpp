// path: tests/sva/unit/test_sva_basic.cpp
#include <cassert>
#include <iostream>

#include "../../../src/sim/kernel.hpp"
#include "../../../src/sva/sva_engine.hpp"

int main() {
    using namespace sv;

    Kernel k;
    SvaEngine sva;

    bool called = false;
    sva.add_property(SvaProperty("p1", [&](const Kernel &) {
        called = true;
        return true;
    }));

    bool ok = sva.check_all(k);
    assert(ok);
    assert(called);

    std::cout << "test_sva_basic: PASS\n";
    return 0;
}
