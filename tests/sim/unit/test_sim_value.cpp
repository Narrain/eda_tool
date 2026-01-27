// path: tests/sim/unit/test_sim_value.cpp
#include <cassert>
#include <iostream>

#include "../../../src/sim/value.hpp"

int main() {
    using namespace sv;

    Value v = Value::from_uint(4, 0b1010);
    assert(v.width() == 4);
    assert(v.to_string() == "1010");

    Value x = Value::from_binary_string("10xz");
    assert(x.width() == 4);
    assert(x.to_string() == "10xz");

    assert(logic_and(Logic4::L1, Logic4::L1) == Logic4::L1);
    assert(logic_or(Logic4::L0, Logic4::L0) == Logic4::L0);
    assert(logic_not(Logic4::L0) == Logic4::L1);

    std::cout << "test_sim_value: PASS\n";
    return 0;
}
