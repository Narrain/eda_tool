#include "kernel.hpp"

#include <cstdlib>

namespace sv {

void Kernel::schedule(Process proc, uint64_t delay, SchedRegion region) {
    ScheduledProcess sp;
    sp.time = cur_time_ + delay;
    sp.delta = (delay == 0) ? (cur_delta_) : 0;
    sp.region = region;
    sp.proc = std::move(proc);
    pq_.push(std::move(sp));
}

void Kernel::schedule_nba(Process proc) {
    nba_queue_.push_back(std::move(proc));
}

void Kernel::run_active_region(uint64_t target_time) {
    while (!pq_.empty()) {
        auto sp = pq_.top();
        if (sp.time != target_time) break;
        if (sp.region != SchedRegion::Active &&
            sp.region != SchedRegion::Preponed &&
            sp.region != SchedRegion::Inactive) break;

        pq_.pop();
        cur_delta_++;
        sp.proc.run(*this);
    }
}

void Kernel::run_nba_region() {
    if (nba_queue_.empty()) return;
    auto q = std::move(nba_queue_);
    nba_queue_.clear();
    for (auto &p : q) {
        p.run(*this);
    }
}

void Kernel::run(uint64_t max_time) {
    while (!pq_.empty()) {
        auto sp = pq_.top();
        if (sp.time > max_time) break;
        cur_time_ = sp.time;
        cur_delta_ = 0;

        if (vcd_) {
            vcd_->dump_time(cur_time_);
            for (const auto &kv : signals_) {
                vcd_->dump_value(kv.first, kv.second);
            }
        }

        run_active_region(cur_time_);
        run_nba_region();

        // NEW: dump again after processes have run
        if (vcd_) {
            vcd_->dump_time(cur_time_);
            for (const auto &kv : signals_) {
                vcd_->dump_value(kv.first, kv.second);
            }
        }
    }
}



// -------------------------
// RTL wiring
// -------------------------

void Kernel::load_design(const RtlDesign *design) {
    design_ = design;
    signals_.clear();
    rtl_processes_.clear();
    while (!pq_.empty()) pq_.pop();
    nba_queue_.clear();

    if (!design_) return;

    init_signals_from_rtl();
    build_processes_from_rtl();

    // Run all RTL-derived processes once at time 0
    for (auto &p : rtl_processes_) {
        schedule(p, 0, p.region());
    }
    // Simple combinational settle at time 0:
    // run all Active-region processes a few times to propagate constants.
    for (int iter = 0; iter < 4; ++iter) {
        for (auto &p : rtl_processes_) {
            if (p.region() == SchedRegion::Active) {
                p.run(*this);
            }
        }
    }

}

void Kernel::init_signals_from_rtl() {
    if (!design_) return;

    for (const auto &mod : design_->modules) {
        for (const auto &net : mod.nets) {
            // For now, treat all nets as 1-bit until we wire a proper width helper.
            std::size_t width = 1;
            signals_[net.name] = Value(width, Logic4::LX);
            if (vcd_) {
                vcd_->add_signal(net.name, width);
            }
        }
    }

    if (vcd_) {
        vcd_->dump_header();
    }
}

void Kernel::build_processes_from_rtl() {
    if (!design_) return;

    for (const auto &mod : design_->modules) {
        // Continuous assigns as combinational processes
        for (const auto &a : mod.continuous_assigns) {
            if (!a.rhs) continue;
            RtlExpr rhs_copy = *a.rhs; // copy by value, lambda capture stays copyable
            std::string lhs_name = a.lhs_name;

            rtl_processes_.emplace_back(
                [this, lhs_name, rhs_copy](Kernel &k) {
                    Value v = k.eval_expr(rhs_copy);
                    k.drive_signal(lhs_name, v, /*nba=*/false);
                },
                SchedRegion::Active
            );
        }

        // Always processes (flattened assigns)
        for (const auto &p : mod.processes) {
            for (const auto &a : p.assigns) {
                if (!a.rhs) continue;
                RtlExpr rhs_copy = *a.rhs;
                std::string lhs_name = a.lhs_name;
                RtlAssignKind kind = a.kind;

                rtl_processes_.emplace_back(
                    [this, lhs_name, rhs_copy, kind](Kernel &k) {
                        Value v = k.eval_expr(rhs_copy);
                        bool nba = (kind == RtlAssignKind::NonBlocking);
                        k.drive_signal(lhs_name, v, nba);
                    },
                    (a.kind == RtlAssignKind::NonBlocking)
                        ? SchedRegion::NBA
                        : SchedRegion::Active
                );
            }
        }

        // Gate-level primitives (if/when populated)
        for (const auto &g : mod.gates) {
            RtlGate gate_copy = g; // copy by value

            rtl_processes_.emplace_back(
                [this, gate_copy](Kernel &k) {
                    auto get_bit = [&](const std::string &name) -> Logic4 {
                        const Value *v = k.get_signal(name);
                        if (!v || v->width() == 0) return Logic4::LX;
                        return v->get(0);
                    };

                    Logic4 out = Logic4::LX;
                    switch (gate_copy.kind) {
                    case RtlGateKind::And: {
                        Logic4 acc = Logic4::L1;
                        for (const auto &in : gate_copy.inputs) {
                            acc = logic_and(acc, get_bit(in));
                        }
                        out = acc;
                        break;
                    }
                    case RtlGateKind::Or: {
                        Logic4 acc = Logic4::L0;
                        for (const auto &in : gate_copy.inputs) {
                            acc = logic_or(acc, get_bit(in));
                        }
                        out = acc;
                        break;
                    }
                    case RtlGateKind::Not: {
                        out = logic_not(get_bit(
                            gate_copy.inputs.empty() ? std::string() : gate_copy.inputs[0]));
                        break;
                    }
                    case RtlGateKind::Nand: {
                        Logic4 acc = Logic4::L1;
                        for (const auto &in : gate_copy.inputs) {
                            acc = logic_and(acc, get_bit(in));
                        }
                        out = logic_not(acc);
                        break;
                    }
                    case RtlGateKind::Nor: {
                        Logic4 acc = Logic4::L0;
                        for (const auto &in : gate_copy.inputs) {
                            acc = logic_or(acc, get_bit(in));
                        }
                        out = logic_not(acc);
                        break;
                    }
                    case RtlGateKind::Xor: {
                        Logic4 acc = Logic4::L0;
                        for (const auto &in : gate_copy.inputs) {
                            acc = logic_xor(acc, get_bit(in));
                        }
                        out = acc;
                        break;
                    }
                    case RtlGateKind::Xnor: {
                        Logic4 acc = Logic4::L0;
                        for (const auto &in : gate_copy.inputs) {
                            acc = logic_xor(acc, get_bit(in));
                        }
                        out = logic_not(acc);
                        break;
                    }
                    case RtlGateKind::Buf: {
                        out = get_bit(
                            gate_copy.inputs.empty() ? std::string() : gate_copy.inputs[0]);
                        break;
                    }
                    }

                    Value v(1);
                    v.set(0, out);
                    k.drive_signal(gate_copy.out, v, /*nba=*/false);
                },
                SchedRegion::Active
            );
        }
    }
}

// -------------------------
// Expression evaluation
// -------------------------

Value Kernel::get_signal_value(const std::string &name, std::size_t width) {
    auto it = signals_.find(name);
    if (it == signals_.end()) {
        return Value(width, Logic4::LX);
    }
    const Value &v = it->second;
    if (v.width() == width) return v;

    Value out(width, Logic4::LX);
    std::size_t minw = (width < v.width()) ? width : v.width();
    for (std::size_t i = 0; i < minw; ++i) {
        out.set(i, v.get(i));
    }
    return out;
}

void Kernel::drive_signal(const std::string &name, const Value &v, bool nba) {
    if (nba) {
        Process p(
            [this, name, v](Kernel &k) {
                k.signals_[name] = v;
            },
            SchedRegion::NBA
        );
        schedule_nba(std::move(p));
    } else {
        signals_[name] = v;
    }
}

static uint64_t parse_simple_int_literal(const std::string &s) {
    bool is_bin = !s.empty();
    for (char c : s) {
        if (c != '0' && c != '1' && c != 'x' && c != 'X' && c != 'z' && c != 'Z') {
            is_bin = false;
            break;
        }
    }
    if (!is_bin) {
        return static_cast<uint64_t>(std::strtoull(s.c_str(), nullptr, 10));
    }
    uint64_t v = 0;
    for (char c : s) {
        v <<= 1;
        if (c == '1') v |= 1;
    }
    return v;
}

Value Kernel::eval_expr(const RtlExpr &e) {
    switch (e.kind) {

    case RtlExprKind::Ref: {
        // default width 1 for now
        return get_signal_value(e.ref_name, 1);
    }

    case RtlExprKind::Const: {
        const std::string &lit = e.const_literal;

        // Handle Verilog-style literals like 1'b0, 4'b1010, etc.
        auto pos = lit.find('\'');
        if (pos != std::string::npos && pos + 2 < lit.size()) {
            // width is lit.substr(0, pos) but we ignore it for now
            char base = std::tolower(lit[pos + 1]);
            std::string digits = lit.substr(pos + 2);

            switch (base) {
            case 'b': {
                // binary: use digits directly
                return Value::from_binary_string(digits);
            }
            case 'd': {
                uint64_t v = parse_simple_int_literal(digits);
                return Value::from_uint(32, v);
            }
            case 'h': {
                // hex: expand each hex nibble to 4 bits
                std::string bin;
                for (char c : digits) {
                    uint8_t val;
                    if (c >= '0' && c <= '9') val = c - '0';
                    else if (c >= 'a' && c <= 'f') val = 10 + (c - 'a');
                    else if (c >= 'A' && c <= 'F') val = 10 + (c - 'A');
                    else continue;
                    for (int i = 3; i >= 0; --i)
                        bin.push_back((val & (1 << i)) ? '1' : '0');
                }
                if (bin.empty())
                    return Value(1, Logic4::LX);
                return Value::from_binary_string(bin);
            }
            default:
                // unknown base → fall back to decimal
                break;
            }
        }

        // Fallback: old behavior
        bool is_bin = !lit.empty();
        for (char c : lit) {
            if (c != '0' && c != '1' && c != 'x' && c != 'X' && c != 'z' && c != 'Z') {
                is_bin = false;
                break;
            }
        }
        if (is_bin) {
            return Value::from_binary_string(lit);
        } else {
            uint64_t v = parse_simple_int_literal(lit);
            return Value::from_uint(32, v);
        }
    }


    case RtlExprKind::Unary: {
        Value op = eval_expr(*e.un_operand);
        Value out(op.width(), Logic4::LX);

        switch (e.un_op) {
        case RtlUnOp::Plus:
            return op;

        case RtlUnOp::Minus:
            return Value(op.width(), Logic4::LX); // placeholder

        case RtlUnOp::Not: {
            Logic4 acc = Logic4::L0;
            for (std::size_t i = 0; i < op.width(); ++i) {
                if (op.get(i) == Logic4::L1) {
                    acc = Logic4::L1;
                    break;
                }
            }
            Logic4 res = (acc == Logic4::L1) ? Logic4::L0 : Logic4::L1;
            Value v(1);
            v.set(0, res);
            return v;
        }

        case RtlUnOp::BitNot: {
            for (std::size_t i = 0; i < op.width(); ++i)
                out.set(i, logic_not(op.get(i)));
            return out;
        }
        }

        return out;
    }

    case RtlExprKind::Binary: {
        Value lhs = eval_expr(*e.lhs);
        Value rhs = eval_expr(*e.rhs);

        // ---- FIX: extend both sides to same width ----
        std::size_t width = std::max(lhs.width(), rhs.width());

        if (lhs.width() != width) {
            Value tmp(width, Logic4::LX);
            for (std::size_t i = 0; i < lhs.width(); ++i)
                tmp.set(i, lhs.get(i));
            lhs = std::move(tmp);
        }

        if (rhs.width() != width) {
            Value tmp(width, Logic4::LX);
            for (std::size_t i = 0; i < rhs.width(); ++i)
                tmp.set(i, rhs.get(i));
            rhs = std::move(tmp);
        }

        Value out(width, Logic4::LX);

        auto apply_bitwise = [&](auto fn) {
            for (std::size_t i = 0; i < width; ++i)
                out.set(i, fn(lhs.get(i), rhs.get(i)));
        };

        switch (e.bin_op) {
        case RtlBinOp::And:
            apply_bitwise(logic_and);
            return out;

        case RtlBinOp::Or:
            apply_bitwise(logic_or);
            return out;

        case RtlBinOp::Xor:
            apply_bitwise(logic_xor);
            return out;

        default:
            return Value(width, Logic4::LX);
        }
    }
    }

    return Value(1, Logic4::LX);
}

} // namespace sv
