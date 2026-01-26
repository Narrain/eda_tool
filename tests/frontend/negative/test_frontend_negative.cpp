// path: tests/frontend/negative/test_frontend_negative.cpp
#include <cassert>
#include <iostream>

int main() {
    // For now, this negative test simply asserts that the test harness runs.
    // When the parser interface is fully exposed via headers, this file will
    // be extended to feed invalid code and assert that errors are thrown.
    std::cout << "test_frontend_negative: PASS (no-op placeholder)\n";
    return 0;
}
