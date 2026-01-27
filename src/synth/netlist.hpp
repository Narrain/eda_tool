// path: src/synth/netlist.hpp
#ifndef __NETLIST_HPP__
#define __NETLIST_HPP__

#include <string>
#include <vector>

#include "../sim/value.hpp"
#include "../ir/rtl_ir.hpp"

namespace sv {

enum class GateKind {
    And,
    Or,
    Xor,
    Not,
    Buf
};

struct NetlistNet {
    std::string name;
    std::size_t width = 1;
};

struct NetlistGate {
    GateKind kind;
    std::string output;
    std::vector<std::string> inputs;
};

struct NetlistModule {
    std::string name;
    std::vector<NetlistNet> nets;
    std::vector<NetlistGate> gates;
};

struct NetlistDesign {
    std::vector<NetlistModule> modules;
};

} // namespace sv

#endif // __NETLIST_HPP__
