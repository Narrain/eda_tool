// path: tests/frontend/unit/test_frontend_basic.cpp
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

#include "../../../src/frontend/ast.hpp"

namespace sv {
    // Reuse lexer and parser declarations
    enum class TokenKind;
    struct Token;
}

int main() {
    using namespace sv;

    // Very basic sanity: construct a small AST manually
    auto design = std::make_unique<Design>();
    auto mod = std::make_unique<ModuleDecl>();
    mod->name = "top";
    design->modules.push_back(std::move(mod));

    assert(design->modules.size() == 1);
    assert(design->modules[0]->name == "top");

    std::cout << "test_frontend_basic: PASS\n";
    return 0;
}
