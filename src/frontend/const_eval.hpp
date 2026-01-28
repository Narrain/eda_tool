#ifndef __CONST_EVAL_HPP__
#define __CONST_EVAL_HPP__

#include "ast.hpp"
#include <cstdint>
#include <stdexcept>

namespace sv {

struct ConstValue {
    bool valid = false;
    int64_t value = 0;
};

class ConstEval {
public:
    ConstValue eval(const Expression &e) {
        switch (e.kind) {
        case ExprKind::Number: {
            ConstValue cv;
            cv.valid = true;
            cv.value = parse_int(e.literal);
            return cv;
        }
        case ExprKind::Unary: {
            if (!e.unary_operand) return {};
            ConstValue op = eval(*e.unary_operand);
            if (!op.valid) return {};
            ConstValue cv;
            cv.valid = true;
            switch (e.unary_op) {
            case UnaryOp::Plus:  cv.value = +op.value; break;
            case UnaryOp::Minus: cv.value = -op.value; break;
            case UnaryOp::LogicalNot: cv.value = !op.value; break;
            case UnaryOp::BitNot: cv.value = ~op.value; break;
            }
            return cv;
        }
        case ExprKind::Binary: {
            if (!e.lhs || !e.rhs) return {};
            ConstValue l = eval(*e.lhs);
            ConstValue r = eval(*e.rhs);
            if (!l.valid || !r.valid) return {};
            ConstValue cv;
            cv.valid = true;
            switch (e.binary_op) {
            case BinaryOp::Add:  cv.value = l.value + r.value; break;
            case BinaryOp::Sub:  cv.value = l.value - r.value; break;
            case BinaryOp::Mul:  cv.value = l.value * r.value; break;
            case BinaryOp::Div:  cv.value = (r.value ? l.value / r.value : 0); break;
            case BinaryOp::Mod:  cv.value = (r.value ? l.value % r.value : 0); break;

            case BinaryOp::BitAnd: cv.value = l.value & r.value; break;
            case BinaryOp::BitOr:  cv.value = l.value | r.value; break;
            case BinaryOp::BitXor: cv.value = l.value ^ r.value; break;

            case BinaryOp::LogicalAnd: cv.value = (l.value != 0) && (r.value != 0); break;
            case BinaryOp::LogicalOr:  cv.value = (l.value != 0) || (r.value != 0); break;

            case BinaryOp::Eq:  cv.value = (l.value == r.value); break;
            case BinaryOp::Neq: cv.value = (l.value != r.value); break;
            case BinaryOp::Lt:  cv.value = (l.value <  r.value); break;
            case BinaryOp::Gt:  cv.value = (l.value >  r.value); break;
            case BinaryOp::Le:  cv.value = (l.value <= r.value); break;
            case BinaryOp::Ge:  cv.value = (l.value >= r.value); break;

            case BinaryOp::Shl:  cv.value = (l.value << (r.value & 63)); break;
            case BinaryOp::Shr:
            case BinaryOp::Ashr: cv.value = (l.value >> (r.value & 63)); break;
            case BinaryOp::Ashl: cv.value = (l.value << (r.value & 63)); break;

            default:
                cv.valid = false;
                break;
            }
            return cv;
        }
        case ExprKind::Ternary: {
            if (!e.cond || !e.then_expr || !e.else_expr) return {};
            ConstValue c = eval(*e.cond);
            if (!c.valid) return {};
            return c.value ? eval(*e.then_expr) : eval(*e.else_expr);
        }
        default:
            return {};
        }
    }

private:
    static int64_t parse_int(const std::string &s) {
        // very simple: decimal only for now
        return static_cast<int64_t>(std::strtoll(s.c_str(), nullptr, 10));
    }
};

} // namespace sv

#endif // __CONST_EVAL_HPP__
