// path: src/coverage/coverage.hpp
#ifndef __COVERAGE_HPP__
#define __COVERAGE_HPP__

#include <string>
#include <unordered_map>
#include <cstdint>

namespace sv {

class Coverpoint {
public:
    explicit Coverpoint(const std::string &name) : name_(name) {}

    void sample(uint64_t bin) { bins_[bin]++; total_++; }

    const std::string &name() const { return name_; }
    uint64_t total() const { return total_; }
    const std::unordered_map<uint64_t, uint64_t> &bins() const { return bins_; }

private:
    std::string name_;
    uint64_t total_ = 0;
    std::unordered_map<uint64_t, uint64_t> bins_;
};

class CoverageDB {
public:
    Coverpoint &coverpoint(const std::string &name) {
        auto it = cps_.find(name);
        if (it == cps_.end()) {
            it = cps_.emplace(name, Coverpoint(name)).first;
        }
        return it->second;
    }

    const std::unordered_map<std::string, Coverpoint> &all() const { return cps_; }

private:
    std::unordered_map<std::string, Coverpoint> cps_;
};

} // namespace sv

#endif // __COVERAGE_HPP__
