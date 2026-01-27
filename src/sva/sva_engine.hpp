// path: src/sva/sva_engine.hpp
#ifndef __SVA_ENGINE_HPP__
#define __SVA_ENGINE_HPP__

#include <string>
#include <vector>
#include <functional>

#include "../sim/kernel.hpp"
#include "../sim/value.hpp"

namespace sv {

class SvaProperty {
public:
    using CheckFn = std::function<bool(const Kernel &)>;

    SvaProperty(std::string name, CheckFn fn)
        : name_(std::move(name)), fn_(std::move(fn)) {}

    const std::string &name() const { return name_; }

    bool evaluate(const Kernel &k) const {
        if (!fn_) return true;
        return fn_(k);
    }

private:
    std::string name_;
    CheckFn fn_;
};

class SvaEngine {
public:
    void add_property(const SvaProperty &p) {
        props_.push_back(p);
    }

    bool check_all(const Kernel &k) const {
        for (const auto &p : props_) {
            if (!p.evaluate(k)) return false;
        }
        return true;
    }

private:
    std::vector<SvaProperty> props_;
};

} // namespace sv

#endif // __SVA_ENGINE_HPP__
