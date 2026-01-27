// path: tests/coverage/unit/test_coverage_basic.cpp
#include <cassert>
#include <iostream>

#include "../../../src/coverage/coverage.hpp"

int main() {
    using namespace sv;

    CoverageDB db;
    auto &cp = db.coverpoint("cp1");
    cp.sample(0);
    cp.sample(1);
    cp.sample(1);

    assert(cp.total() == 3);
    auto &bins = cp.bins();
    assert(bins.at(0) == 1);
    assert(bins.at(1) == 2);

    std::cout << "test_coverage_basic: PASS\n";
    return 0;
}
