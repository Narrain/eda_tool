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
        std::cerr << "Usage: svtool [--vcd=FILE] [--max=N] <verilog-file>\n";
        return 1;
    }

    // -----------------------------
    // CLI parsing
    // -----------------------------
    std::string vcd_filename = "";
    uint64_t max_time = 0;   // 0 = unlimited
    std::string verilog_file = "";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg.rfind("--vcd=", 0) == 0) {
            vcd_filename = arg.substr(6);
        }
        else if (arg.rfind("--max=", 0) == 0) {
            max_time = std::stoull(arg.substr(6));
        }
        else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
        else {
            verilog_file = arg;
        }
    }

    if (verilog_file.empty()) {
        std::cerr << "Error: no Verilog file provided.\n";
        return 1;
    }

    // -----------------------------
    // Read source file
    // -----------------------------
    std::ifstream in(verilog_file);
    if (!in) {
        std::cerr << "Error: cannot open " << verilog_file << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string source = buffer.str();

    // -----------------------------
    // Frontend: lex + parse
    // -----------------------------
    Lexer lex(verilog_file, source);
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
    RtlDesign rd = irb.build();   // this one is for simulation

    for (const auto &m : rd.modules) {
        std::cout << "MODULE " << m.name << "\n";
        for (const auto &p : m.processes) {
            std::cout << "  PROCESS kind=" << int(p.kind)
                      << " assigns=" << p.assigns.size()
                      << " first_stmt=" << (p.first_stmt ? "YES" : "NO")
                      << "\n";
        }
    }

    // -----------------------------
    // Synthesis on a separate copy
    // -----------------------------
    RtlDesign rd_for_synth = rd;   // deep copy, keeps first_stmt mapping
    SynthDriver sd(rd_for_synth);
    NetlistDesign nd = sd.run();
    (void)nd;

    // -----------------------------
    // Simulation + VCD
    // -----------------------------
    Kernel k;

    VcdWriter vcd(vcd_filename);
    if (!vcd_filename.empty()) {
        if (vcd.good()) {
            k.set_vcd(&vcd);
        } else {
            std::cerr << "Warning: cannot open VCD file '"
                      << vcd_filename << "'\n";
        }
    }

    // Let kernel initialize signals and VCD header
    k.load_design(&rd);

    // Explicitly schedule all RTL processes at time 0,
    // driving their procedural bodies directly.
    for (const auto &mod : rd.modules) {
        for (const auto &rp : mod.processes) {
            const RtlProcess *proc_ptr = &rp;

            Process proc(
                [proc_ptr](Kernel &kk) {
                    if (proc_ptr->first_stmt) {
                        Thread th{
                            proc_ptr->first_stmt,
                            nullptr,
                            proc_ptr,
                            proc_ptr->first_stmt
                        };
                        kk.exec_stmt(th);
                    }
                    // No fallback: we only care about procedural bodies here.
                },
                SchedRegion::Active
            );

            k.schedule(std::move(proc), 0, SchedRegion::Active);
        }
    }

    // Run simulation
    k.run(max_time);

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

    if (!vcd_filename.empty())
        std::cout << "VCD written to " << vcd_filename << "\n";

    return sva_ok ? 0 : 2;
}
