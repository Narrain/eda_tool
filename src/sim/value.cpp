// path: src/sim/value.cpp
#include "value.hpp"

namespace sv {

static char logic4_to_char(Logic4 v) {
    switch (v) {
    case Logic4::L0: return '0';
    case Logic4::L1: return '1';
    case Logic4::LX: return 'x';
    case Logic4::LZ: return 'z';
    }
    return 'x';
}

static Logic4 char_to_logic4(char c) {
    switch (c) {
    case '0': return Logic4::L0;
    case '1': return Logic4::L1;
    case 'x':
    case 'X': return Logic4::LX;
    case 'z':
    case 'Z': return Logic4::LZ;
    default:  return Logic4::LX;
    }
}

std::string Value::to_string() const {
    std::string s;
    s.reserve(bits_.size());
    for (auto it = bits_.rbegin(); it != bits_.rend(); ++it) {
        s.push_back(logic4_to_char(*it));
    }
    return s;
}

Value Value::from_binary_string(const std::string &s) {
    Value v;
    v.bits_.resize(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[s.size() - 1 - i];
        v.bits_[i] = char_to_logic4(c);
    }
    return v;
}

Value Value::from_uint(std::size_t width, uint64_t x) {
    Value v(width, Logic4::L0);
    for (std::size_t i = 0; i < width; ++i) {
        v.set(i, (x & (1ull << i)) ? Logic4::L1 : Logic4::L0);
    }
    return v;
}

Logic4 logic_and(Logic4 a, Logic4 b) {
    if (a == Logic4::L0 || b == Logic4::L0) return Logic4::L0;
    if (a == Logic4::L1 && b == Logic4::L1) return Logic4::L1;
    if (a == Logic4::LZ || b == Logic4::LZ) return Logic4::LX;
    return Logic4::LX;
}

Logic4 logic_or(Logic4 a, Logic4 b) {
    if (a == Logic4::L1 || b == Logic4::L1) return Logic4::L1;
    if (a == Logic4::L0 && b == Logic4::L0) return Logic4::L0;
    if (a == Logic4::LZ || b == Logic4::LZ) return Logic4::LX;
    return Logic4::LX;
}

Logic4 logic_xor(Logic4 a, Logic4 b) {
    if (a == Logic4::LX || b == Logic4::LX ||
        a == Logic4::LZ || b == Logic4::LZ) return Logic4::LX;
    return (a == b) ? Logic4::L0 : Logic4::L1;
}

Logic4 logic_not(Logic4 a) {
    if (a == Logic4::L0) return Logic4::L1;
    if (a == Logic4::L1) return Logic4::L0;
    return Logic4::LX;
}

} // namespace sv
