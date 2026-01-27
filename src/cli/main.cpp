// path: src/cli/main.cpp
#include <iostream>
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
    // Minimal stub: no real file IO, just demonstrate pipeline wiring.

    Design design;
    auto mod = std::make_unique<ModuleDecl>();
    mod->name = "top";
    design.modules.push_back(std::move(mod));

    SymbolTable symtab;
    symtab.build(design);

    Elaborator elab(design, symtab);
    auto ed = elab.elaborate();

    IRBuilder irb(design, ed, symtab);
    RtlDesign rd = irb.build();

    SynthDriver sd(rd);
    NetlistDesign nd = sd.run();
    (void)nd;

    Kernel k;
    k.load_design(&rd);

    VcdWriter vcd("wave.vcd");
    k.set_vcd(&vcd);

    CoverageDB cov;
    auto &cp = cov.coverpoint("dummy");
    cp.sample(0);

    SvaEngine sva;
    sva.add_property(SvaProperty("always_true", [](const Kernel &) { return true; }));

    k.run(10);
    bool ok = sva.check_all(k);

    std::cout << "Simulation finished, SVA: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 2;
}
