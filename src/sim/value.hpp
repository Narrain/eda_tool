// path: src/sim/value.hpp
#ifndef __VALUE_HPP__
#define __VALUE_HPP__

#include <cstdint>
#include <string>
#include <vector>

namespace sv {

// 4-state logic: 0,1,X,Z
enum class Logic4 : uint8_t {
    L0 = 0,
    L1 = 1,
    LX = 2,
    LZ = 3
};

class Value {
public:
    Value() = default;
    explicit Value(std::size_t width, Logic4 init = Logic4::LX)
        : bits_(width, init) {}

    std::size_t width() const { return bits_.size(); }

    Logic4 get(std::size_t idx) const { return bits_.at(idx); }
    void set(std::size_t idx, Logic4 v) { bits_.at(idx) = v; }

    void resize(std::size_t w, Logic4 init = Logic4::LX) {
        bits_.assign(w, init);
    }

    std::string to_string() const;
    static Value from_binary_string(const std::string &s);
    static Value from_uint(std::size_t width, uint64_t v);

private:
    std::vector<Logic4> bits_;
};

Logic4 logic_and(Logic4 a, Logic4 b);
Logic4 logic_or(Logic4 a, Logic4 b);
Logic4 logic_xor(Logic4 a, Logic4 b);
Logic4 logic_not(Logic4 a);

} // namespace sv

#endif // __VALUE_HPP__
