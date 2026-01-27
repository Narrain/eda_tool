#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "../frontend/ast.hpp"
#include "../frontend/lexer.hpp"
#include "../frontend/parser.hpp"
#include "../frontend/symbol_table.hpp"
#include "../frontend/elab.hpp"
#include "../ir/ir_builder.hpp"
#include "../synth/synth_driver.hpp"
#include "../sim/kernel.hpp"
#include "../sim/vcd_writer.hpp"
#include "../coverage/coverage.hpp"
#include "../sva/sva_engine.hpp"

int main(int argc, char **argv) {
    using namespace sv;

    if (argc < 2) {
        std::cerr << "Usage: svtool <verilog-file>\n";
        return 1;
    }

    std::string filename = argv[1];

    // -----------------------------
    // Read source file
    // -----------------------------
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Error: cannot open " << filename << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string source = buffer.str();

    // -----------------------------
    // Frontend: lex + parse
    // -----------------------------
    Lexer lex(filename, source);
    std::vector<Token> tokens;
    try {
        tokens = lex.lex();
    } catch (const std::exception &e) {
        std::cerr << "Lex error: " << e.what() << "\n";
        return 1;
    }

    Parser parser(tokens);
    std::unique_ptr<Design> design;
    try {
        design = parser.parseDesign();
    } catch (const std::exception &e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    }

    if (!design) {
        std::cerr << "No design parsed.\n";
        return 1;
    }

    // -----------------------------
    // Elaboration
    // -----------------------------
    SymbolTable symtab;
    symtab.build(*design);

    Elaborator elab(*design, symtab);
    ElaboratedDesign ed;
    try {
        ed = elab.elaborate();
    } catch (const std::exception &e) {
        std::cerr << "Elab error: " << e.what() << "\n";
        return 1;
    }

    // -----------------------------
    // IR build
    // -----------------------------
    IRBuilder irb(*design, ed, symtab);
    RtlDesign rd = irb.build();

    // -----------------------------
    // Synthesis
    // -----------------------------
    SynthDriver sd(rd);
    NetlistDesign nd = sd.run();
    (void)nd; // not printed yet

    // -----------------------------
    // Simulation + VCD
    // -----------------------------
    Kernel k;
    k.load_design(&rd);

    VcdWriter vcd("wave.vcd");
    if (vcd.good()) {
        k.set_vcd(&vcd);
    }

    // -----------------------------
    // Coverage
    // -----------------------------
    CoverageDB cov;
    auto &cp = cov.coverpoint("top_dummy");
    cp.sample(0);

    // -----------------------------
    // SVA
    // -----------------------------
    SvaEngine sva;
    sva.add_property(SvaProperty("always_true", [](const Kernel &) {
        return true;
    }));

    // Run a small simulation window
    k.run(10);

    bool sva_ok = sva.check_all(k);

    // -----------------------------
    // Coverage summary
    // -----------------------------
    std::cout << "Coverage:\n";
    for (const auto &kv : cov.all()) {
        const auto &cp_ref = kv.second;
        std::cout << "  coverpoint " << cp_ref.name()
                  << " total=" << cp_ref.total() << "\n";
    }

    std::cout << "SVA: " << (sva_ok ? "PASS" : "FAIL") << "\n";
    std::cout << "VCD written to wave.vcd (if VCD file opened successfully)\n";

    return sva_ok ? 0 : 2;
}
