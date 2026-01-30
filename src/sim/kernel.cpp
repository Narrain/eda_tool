#include "kernel.hpp"

#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace sv {

// -------------------------
// Scheduling
// -------------------------

void Kernel::schedule(Process proc, uint64_t delay, SchedRegion region) {
    ScheduledProcess sp;
    sp.time = cur_time_ + delay;
    sp.delta = (delay == 0) ? cur_delta_ : 0;
    sp.region = region;
    sp.proc = std::move(proc);
    pq_.push(std::move(sp));
}

void Kernel::schedule_nba(Process proc) {
    nba_queue_.push_back(std::move(proc));
}

void Kernel::run_active_region(uint64_t target_time) {
    while (!pq_.empty()) {
        const ScheduledProcess &sp = pq_.top();

        if (sp.time != target_time)
            break;

        if (sp.region != SchedRegion::Active &&
            sp.region != SchedRegion::Preponed &&
            sp.region != SchedRegion::Inactive)
            break;

        ScheduledProcess cur = sp;
        pq_.pop();

        std::cerr << "[KERNEL] run proc at t=" << cur.time
                  << " delta=" << cur.delta
                  << " region=" << int(cur.region) << "\n";

        cur_delta_++;
        cur.proc.run(*this);
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
    bool unlimited = (max_time == 0);

    // Safety net: if for any reason nothing was scheduled yet,
    // schedule all RTL processes at time 0 in their natural region.
    if (pq_.empty()) {
        for (auto &p : rtl_processes_) {
            schedule(p, 0, p.region());
        }
    }

    while (!pq_.empty()) {
        const ScheduledProcess &sp = pq_.top();

        if (!unlimited && sp.time > max_time)
            break;

        cur_time_ = sp.time;
        cur_delta_ = 0;

        // Dump VCD at start of this time
        if (vcd_) {
            vcd_->dump_time(cur_time_);
            for (const auto &kv : signals_) {
                vcd_->dump_value(kv.first, kv.second);
            }
        }

        // Run all Active/Preponed/Inactive at this time
        run_active_region(cur_time_);

        // Run NBA region
        run_nba_region();

        // Dump VCD again after all updates at this time
        if (vcd_) {
            vcd_->dump_time(cur_time_);
            for (const auto &kv : signals_) {
                vcd_->dump_value(kv.first, kv.second);
            }
        }
    }
}

// -------------------------
// Design load
// -------------------------

void Kernel::load_design(const RtlDesign *design) {
    design_ = design;

    signals_.clear();
    rtl_processes_.clear();
    rtl_processes_.reserve(1024);
    while (!pq_.empty()) pq_.pop();
    nba_queue_.clear();
    level_watchers_.clear();
    posedge_watchers_.clear();
    negedge_watchers_.clear();

    if (!design_) return;

    init_signals_from_rtl();
    build_processes_from_rtl();

    if (vcd_) {
        auto width_from_type = [](const DataType &t) -> std::size_t {
            if (t.is_packed && t.msb >= 0 && t.lsb >= 0) {
                int w = (t.msb >= t.lsb) ? (t.msb - t.lsb + 1)
                                         : (t.lsb - t.msb + 1);
                return static_cast<std::size_t>(w);
            }
            return 1;
        };

        for (const auto &mod : design_->modules) {
            for (const auto &net : mod.nets) {
                std::size_t width = width_from_type(net.type);
                vcd_->add_signal(net.name, width);
            }
        }
        vcd_->dump_header();
    }

    for (auto &p : rtl_processes_) {
        schedule(p, 0, p.region());
    }
}

// -------------------------
// Signal init
// -------------------------

void Kernel::init_signals_from_rtl() {
    if (!design_) return;

    auto width_from_type = [](const DataType &t) -> std::size_t {
        if (t.is_packed && t.msb >= 0 && t.lsb >= 0) {
            int w = (t.msb >= t.lsb) ? (t.msb - t.lsb + 1)
                                     : (t.lsb - t.msb + 1);
            return static_cast<std::size_t>(w);
        }
        return 1;
    };

    for (const auto &mod : design_->modules) {
        for (const auto &net : mod.nets) {
            std::size_t width = width_from_type(net.type);
            signals_[net.name] = Value(width, Logic4::LX);
        }
    }
}

// -------------------------
// Build processes from RTL
// -------------------------

void Kernel::build_processes_from_rtl() {
    if (!design_) return;

    rtl_processes_.clear();

    for (const auto &mod : design_->modules) {

        // -------------------------
        // continuous assigns
        // -------------------------
        for (const auto &a : mod.continuous_assigns) {
            if (!a.rhs) continue;

            RtlExpr rhs_copy = *a.rhs;
            std::string lhs_name = a.lhs_name;

            rtl_processes_.emplace_back(
                Process(
                    [this, lhs_name, rhs_copy](Kernel &k) {
                        Value v = k.eval_expr(rhs_copy);
                        k.drive_signal(lhs_name, v, /*nba=*/false);
                    },
                    SchedRegion::Active
                )
            );

            Process *pp = &rtl_processes_.back();
            register_expr_dependencies(*a.rhs, pp);
        }

        // -------------------------
        // always / initial
        // -------------------------
        for (const auto &rp : mod.processes) {
            const RtlProcess *proc_ptr = &rp;

            rtl_processes_.emplace_back(
                Process(
                    [this, proc_ptr](Kernel &k) {
                        if (proc_ptr->first_stmt) {
                            Thread th{
                                proc_ptr->first_stmt,
                                nullptr,
                                proc_ptr,
                                proc_ptr->first_stmt
                            };
                            k.exec_stmt(th);
                        } else {
                            for (const auto &a : proc_ptr->assigns) {
                                if (!a.rhs) continue;
                                Value v = k.eval_expr(*a.rhs);
                                bool nba = (a.kind == RtlAssignKind::NonBlocking);
                                k.drive_signal(a.lhs_name, v, nba);
                            }
                        }
                    },
                    SchedRegion::Active
                )
            );

            Process *pp = &rtl_processes_.back();

            for (const auto &sig : rp.sensitivity_signals) {
                register_level_dependency(sig, pp);
            }
        }

        // -------------------------
        // gates
        // -------------------------
        for (const auto &g : mod.gates) {
            RtlGate gate_copy = g;

            rtl_processes_.emplace_back(
                Process(
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
                            for (const auto &in : gate_copy.inputs)
                                acc = logic_and(acc, get_bit(in));
                            out = acc;
                            break;
                        }
                        case RtlGateKind::Or: {
                            Logic4 acc = Logic4::L0;
                            for (const auto &in : gate_copy.inputs)
                                acc = logic_or(acc, get_bit(in));
                            out = acc;
                            break;
                        }
                        case RtlGateKind::Not:
                            out = logic_not(get_bit(gate_copy.inputs[0]));
                            break;

                        case RtlGateKind::Nand: {
                            Logic4 acc = Logic4::L1;
                            for (const auto &in : gate_copy.inputs)
                                acc = logic_and(acc, get_bit(in));
                            out = logic_not(acc);
                            break;
                        }
                        case RtlGateKind::Nor: {
                            Logic4 acc = Logic4::L0;
                            for (const auto &in : gate_copy.inputs)
                                acc = logic_or(acc, get_bit(in));
                            out = logic_not(acc);
                            break;
                        }
                        case RtlGateKind::Xor: {
                            Logic4 acc = Logic4::L0;
                            for (const auto &in : gate_copy.inputs)
                                acc = logic_xor(acc, get_bit(in));
                            out = acc;
                            break;
                        }
                        case RtlGateKind::Xnor: {
                            Logic4 acc = Logic4::L0;
                            for (const auto &in : gate_copy.inputs)
                                acc = logic_xor(acc, get_bit(in));
                            out = logic_not(acc);
                            break;
                        }
                        case RtlGateKind::Buf:
                            out = get_bit(gate_copy.inputs[0]);
                            break;
                        }

                        Value v(1);
                        v.set(0, out);
                        k.drive_signal(gate_copy.out, v, /*nba=*/false);
                    },
                    SchedRegion::Active
                )
            );

            Process *pp = &rtl_processes_.back();
            for (const auto &in : g.inputs)
                register_level_dependency(in, pp);
        }
    }
}

// -------------------------
// Expression evaluation helpers
// -------------------------

Value Kernel::get_signal_value(const std::string &name, std::size_t width) {
    auto it = signals_.find(name);
    if (it == signals_.end()) {
        return Value(width, Logic4::LX);
    }
    const Value &v = it->second;
    if (v.width() == width) return v;

    Value out(width, Logic4::LX);
    std::size_t minw = std::min(width, v.width());
    for (std::size_t i = 0; i < minw; ++i) {
        out.set(i, v.get(i));
    }
    return out;
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

static uint64_t value_to_uint(const Value &v) {
    uint64_t res = 0;
    std::size_t w = std::min<std::size_t>(v.width(), 64);
    for (std::size_t i = 0; i < w; ++i) {
        res |= (v.get(i) == Logic4::L1 ? (uint64_t(1) << i) : 0);
    }
    return res;
}

Value Kernel::eval_expr(const RtlExpr &e) {
    switch (e.kind) {

    case RtlExprKind::Ref: {
        return get_signal_value(e.ref_name, 1);
    }

    case RtlExprKind::Const: {
        const std::string &lit = e.const_literal;

        auto pos = lit.find('\'');
        if (pos != std::string::npos && pos + 2 < lit.size()) {
            char base = std::tolower(lit[pos + 1]);
            std::string digits = lit.substr(pos + 2);

            switch (base) {
            case 'b':
                return Value::from_binary_string(digits);
            case 'd': {
                uint64_t v = parse_simple_int_literal(digits);
                return Value::from_uint(32, v);
            }
            case 'h': {
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
                break;
            }
        }

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

        case RtlUnOp::Minus: {
            uint64_t u = value_to_uint(op);
            uint64_t r = static_cast<uint64_t>(-static_cast<int64_t>(u));
            return Value::from_uint(op.width(), r);
        }

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

        std::size_t width = std::max(lhs.width(), rhs.width());
        if (width == 0) width = 1;

        auto extend = [width](Value v) {
            if (v.width() == width) return v;
            Value tmp(width, Logic4::LX);
            std::size_t minw = std::min(width, v.width());
            for (std::size_t i = 0; i < minw; ++i)
                tmp.set(i, v.get(i));
            return tmp;
        };

        lhs = extend(lhs);
        rhs = extend(rhs);

        Value out(width, Logic4::LX);

        auto apply_bitwise = [&](auto fn) {
            for (std::size_t i = 0; i < width; ++i)
                out.set(i, fn(lhs.get(i), rhs.get(i)));
        };

        uint64_t ul = value_to_uint(lhs);
        uint64_t ur = value_to_uint(rhs);

        switch (e.bin_op) {
        case RtlBinOp::Add:
            return Value::from_uint(width, ul + ur);
        case RtlBinOp::Sub:
            return Value::from_uint(width, ul - ur);
        case RtlBinOp::Mul:
            return Value::from_uint(width, ul * ur);
        case RtlBinOp::Div:
            return Value::from_uint(width, ur ? (ul / ur) : 0);
        case RtlBinOp::Mod:
            return Value::from_uint(width, ur ? (ul % ur) : 0);

        case RtlBinOp::And:
            apply_bitwise(logic_and);
            return out;
        case RtlBinOp::Or:
            apply_bitwise(logic_or);
            return out;
        case RtlBinOp::Xor:
            apply_bitwise(logic_xor);
            return out;

        case RtlBinOp::LogicalAnd: {
            bool res = (ul != 0) && (ur != 0);
            Value v(1);
            v.set(0, res ? Logic4::L1 : Logic4::L0);
            return v;
        }
        case RtlBinOp::LogicalOr: {
            bool res = (ul != 0) || (ur != 0);
            Value v(1);
            v.set(0, res ? Logic4::L1 : Logic4::L0);
            return v;
        }

        case RtlBinOp::Eq:
        case RtlBinOp::CaseEq: {
            bool res = (ul == ur);
            Value v(1);
            v.set(0, res ? Logic4::L1 : Logic4::L0);
            return v;
        }
        case RtlBinOp::Neq:
        case RtlBinOp::CaseNeq: {
            bool res = (ul != ur);
            Value v(1);
            v.set(0, res ? Logic4::L1 : Logic4::L0);
            return v;
        }
        case RtlBinOp::Lt: {
            bool res = (static_cast<int64_t>(ul) < static_cast<int64_t>(ur));
            Value v(1);
            v.set(0, res ? Logic4::L1 : Logic4::L0);
            return v;
        }
        case RtlBinOp::Gt: {
            bool res = (static_cast<int64_t>(ul) > static_cast<int64_t>(ur));
            Value v(1);
            v.set(0, res ? Logic4::L1 : Logic4::L0);
            return v;
        }
        case RtlBinOp::Le: {
            bool res = (static_cast<int64_t>(ul) <= static_cast<int64_t>(ur));
            Value v(1);
            v.set(0, res ? Logic4::L1 : Logic4::L0);
            return v;
        }
        case RtlBinOp::Ge: {
            bool res = (static_cast<int64_t>(ul) >= static_cast<int64_t>(ur));
            Value v(1);
            v.set(0, res ? Logic4::L1 : Logic4::L0);
            return v;
        }

        case RtlBinOp::Shl:
            return Value::from_uint(width, ul << (ur & 63));
        case RtlBinOp::Shr:
        case RtlBinOp::Ashr:
            return Value::from_uint(width, ul >> (ur & 63));
        case RtlBinOp::Ashl:
            return Value::from_uint(width, ul << (ur & 63));
        }

        return Value(width, Logic4::LX);
    }
    }

    return Value(1, Logic4::LX);
}

// -------------------------
// Driving signals
// -------------------------

void Kernel::drive_signal(const std::string &name, const Value &v, bool nba) {
    if (nba) {
        Process p(
            [this, name, v](Kernel &k) {
                k.signals_[name] = v;
            },
            SchedRegion::NBA
        );
        schedule_nba(std::move(p));
        return;
    }

    Logic4 old_bit = Logic4::LX;
    auto it_old = signals_.find(name);
    if (it_old != signals_.end() && it_old->second.width() > 0) {
        old_bit = it_old->second.get(0);
    }

    signals_[name] = v;

    Logic4 new_bit = Logic4::LX;
    if (v.width() > 0) {
        new_bit = v.get(0);
    }

    bool is_posedge = (old_bit == Logic4::L0 && new_bit == Logic4::L1);
    bool is_negedge = (old_bit == Logic4::L1 && new_bit == Logic4::L0);

    auto it_lvl = level_watchers_.find(name);
    if (it_lvl != level_watchers_.end()) {
        for (Process *pp : it_lvl->second) {
            if (!pp) continue;
            schedule(*pp, 0, pp->region());
        }
    }

    if (is_posedge) {
        auto it_pe = posedge_watchers_.find(name);
        if (it_pe != posedge_watchers_.end()) {
            for (Process *pp : it_pe->second) {
                if (!pp) continue;
                schedule(*pp, 0, pp->region());
            }
        }
    }

    if (is_negedge) {
        auto it_ne = negedge_watchers_.find(name);
        if (it_ne != negedge_watchers_.end()) {
            for (Process *pp : it_ne->second) {
                if (!pp) continue;
                schedule(*pp, 0, pp->region());
            }
        }
    }
}

// -------------------------
// Dependency registration
// -------------------------

void Kernel::register_level_dependency(const std::string &sig, Process *p) {
    if (sig.empty() || !p) return;
    level_watchers_[sig].push_back(p);
}

void Kernel::register_posedge_dependency(const std::string &sig, Process *p) {
    if (sig.empty() || !p) return;
    posedge_watchers_[sig].push_back(p);
}

void Kernel::register_negedge_dependency(const std::string &sig, Process *p) {
    if (sig.empty() || !p) return;
    negedge_watchers_[sig].push_back(p);
}

void Kernel::register_expr_dependencies(const RtlExpr &e, Process *p) {
    switch (e.kind) {
    case RtlExprKind::Ref:
        register_level_dependency(e.ref_name, p);
        break;

    case RtlExprKind::Unary:
        if (e.un_operand)
            register_expr_dependencies(*e.un_operand, p);
        break;

    case RtlExprKind::Binary:
        if (e.lhs)
            register_expr_dependencies(*e.lhs, p);
        if (e.rhs)
            register_expr_dependencies(*e.rhs, p);
        break;

    default:
        break;
    }
}

// -------------------------
// Procedural execution
// -------------------------

void Kernel::exec_stmt(Thread &th) {
    const RtlStmt* s = th.stmt;

    while (s) {

        switch (s->kind) {

        case RtlStmtKind::BlockingAssign: {
            if (s->rhs) {
                Value v = eval_expr(*s->rhs);
                drive_signal(s->lhs_name, v, /*nba=*/false);
            }
            s = s->next;
            break;
        }

        case RtlStmtKind::NonBlockingAssign: {
            if (s->rhs) {
                Value v = eval_expr(*s->rhs);
                drive_signal(s->lhs_name, v, /*nba=*/true);
            }
            s = s->next;
            break;
        }

        case RtlStmtKind::Delay: {
            uint64_t d = 0;
            if (s->delay_expr) {
                Value dv = eval_expr(*s->delay_expr);
                d = value_to_uint(dv);
            }

            // Build continuation thread, preserving owner + entry
            Thread cont;
            cont.stmt  = s->delay_stmt;
            cont.next  = s->next;
            cont.owner = th.owner;
            cont.entry = th.entry;

            schedule(
                Process(
                    [this, cont](Kernel &k) mutable {
                        Thread t = cont;
                        k.exec_stmt(t);
                    },
                    SchedRegion::Active
                ),
                d,
                SchedRegion::Active
            );
            return;
        }

        case RtlStmtKind::Finish:
            std::exit(0);
        }
    }

    // If we reached the end of the statement chain and this is an
    // 'always' process, reschedule it from its entry point.
    if (th.owner && th.owner->kind == RtlProcessKind::Always && th.entry) {
        Thread restart;
        restart.stmt  = th.entry;
        restart.next  = nullptr;
        restart.owner = th.owner;
        restart.entry = th.entry;

        schedule(
            Process(
                [this, restart](Kernel &k) mutable {
                    Thread t = restart;
                    k.exec_stmt(t);
                },
                SchedRegion::Active
            ),
            0,
            SchedRegion::Active
        );
    }

}


} // namespace sv
