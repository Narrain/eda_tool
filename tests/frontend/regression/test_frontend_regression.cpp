// path: tests/frontend/regression/test_frontend_regression.cpp
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "../../../src/frontend/ast.hpp"

namespace sv {
    enum class TokenKind;
    struct Token;
    class Lexer;
    class Parser;
}

int main() {
    using namespace sv;

    // This regression test is a placeholder for real parser+lexer integration
    // Once the full lexer/parser are wired with headers, this test can be extended
    // to parse a real module string and validate the AST.
    auto design = std::make_unique<Design>();
    auto mod = std::make_unique<ModuleDecl>();
    mod->name = "regression_top";
    design->modules.push_back(std::move(mod));

    assert(design->modules.size() == 1);
    assert(design->modules[0]->name == "regression_top");

    std::cout << "test_frontend_regression: PASS\n";
    return 0;
}
