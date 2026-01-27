// path: src/synth/techmap.cpp
#include "techmap.hpp"

namespace sv {

NetlistDesign TechMapper::map() {
    NetlistDesign nd;
    for (const auto &m : rtl_.modules) {
        nd.modules.push_back(map_module(m));
    }
    return nd;
}

NetlistModule TechMapper::map_module(const RtlModule &m) {
    NetlistModule nm;
    nm.name = m.name;

    for (const auto &n : m.nets) {
        ensure_net(nm, n.name, 1);
    }

    for (const auto &a : m.continuous_assigns) {
        lower_assign(nm, a);
    }
    for (const auto &p : m.processes) {
        for (const auto &a : p.assigns) {
            lower_assign(nm, a);
        }
    }

    return nm;
}

void TechMapper::ensure_net(NetlistModule &nm, const std::string &name, std::size_t width) {
    for (auto &n : nm.nets) {
        if (n.name == name) return;
    }
    NetlistNet nn;
    nn.name = name;
    nn.width = width;
    nm.nets.push_back(nn);
}

std::string TechMapper::lower_expr(NetlistModule &nm, const RtlExpr &e) {
    switch (e.kind) {
    case RtlExprKind::Ref:
        ensure_net(nm, e.ref_name, 1);
        return e.ref_name;
    case RtlExprKind::Const: {
        std::string cname = "const_" + e.const_literal;
        ensure_net(nm, cname, 1);
        return cname;
    }
    case RtlExprKind::Unary: {
        std::string in = lower_expr(nm, *e.un_operand);
        std::string out = "u_" + in;
        ensure_net(nm, out, 1);
        NetlistGate g;
        g.kind = (e.un_op == RtlUnOp::Not) ? GateKind::Not : GateKind::Buf;
        g.output = out;
        g.inputs.push_back(in);
        nm.gates.push_back(std::move(g));
        return out;
    }
    case RtlExprKind::Binary: {
        std::string a = lower_expr(nm, *e.lhs);
        std::string b = lower_expr(nm, *e.rhs);
        std::string out = "g_" + a + "_" + b;
        ensure_net(nm, out, 1);
        NetlistGate g;
        switch (e.bin_op) {
        case RtlBinOp::And: g.kind = GateKind::And; break;
        case RtlBinOp::Or: g.kind = GateKind::Or; break;
        case RtlBinOp::Xor: g.kind = GateKind::Xor; break;
        default: g.kind = GateKind::Buf; break;
        }
        g.output = out;
        g.inputs.push_back(a);
        g.inputs.push_back(b);
        nm.gates.push_back(std::move(g));
        return out;
    }
    }
    return "<undef>";
}

void TechMapper::lower_assign(NetlistModule &nm, const RtlAssign &a) {
    std::string rhs_net = a.rhs ? lower_expr(nm, *a.rhs) : "<undef>";
    ensure_net(nm, a.lhs_name, 1);
    NetlistGate g;
    g.kind = GateKind::Buf;
    g.output = a.lhs_name;
    g.inputs.push_back(rhs_net);
    nm.gates.push_back(std::move(g));
}

} // namespace sv
