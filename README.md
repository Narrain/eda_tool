# Verilog Toolchain (Frontend → IR → Synth → Sim → DPI/UVM → Coverage/SVA)

A compact, modular, end‑to‑end Verilog toolchain implemented in modern C++.  
The system parses a synthesizable subset of Verilog, elaborates it, lowers it to an RTL IR, synthesizes it into a gate‑level netlist, simulates it with an event‑driven kernel, supports DPI/UVM‑style interaction, and provides coverage + SVA checking.

This project is structured into **six phases**, each building on the previous one.

---

## ✨ Features

### **Phase 1 — Frontend**
- Lexer (tokenizer)
- Recursive‑descent parser
- AST construction
- Symbol table
- Elaboration engine

### **Phase 2 — RTL IR**
- RTL expression tree
- Continuous assigns
- Always blocks (blocking & non‑blocking)
- Instance lowering
- Deep‑copyable IR nodes

### **Phase 3 — Event‑Driven Simulator**
- 4‑state logic (`0,1,X,Z`)
- Delta‑cycle scheduler
- NBA region
- VCD waveform dumping
- Signal database

### **Phase 4 — Synthesis**
- RTL → gate‑level lowering
- AND/OR/XOR/NOT/BUF mapping
- Netlist IR
- Synth driver

### **Phase 5 — DPI + UVM‑Lite**
- C‑ABI DPI shim
- Sequencer + driver + environment
- DUT interaction via DPI
- UVM‑style stimulus/response loop

### **Phase 6 — Coverage + SVA + CLI**
- Coverpoints + bins
- SVA property engine
- Command‑line tool (`svtool`)
- Full pipeline execution
- VCD output

---

## 📁 Directory Structure



---

## 🔧 Build Instructions

### **1. Configure**
```bash
mkdir build
cd build
cmake ..
make -j

## Run All test
ctest

#Run Toolchain
svtool <verilog-file>
./svtool ../verilog/and_or_mux.v

## This performs:

1. Lexing
2. Parsing
3. Elaboration
4. RTL IR generation
5. Synthesis
6. Simulation (10 time units)
7. Coverage sampling
8. SVA property checking
9. VCD waveform dump → wave.vcd