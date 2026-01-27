// path: src/uvm/uvm_env.hpp
#ifndef __UVM_ENV_HPP__
#define __UVM_ENV_HPP__

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include "../sim/kernel.hpp"
#include "../dpi/dpi_shim.hpp"

namespace sv {

// Extremely small UVM-like façade for driving DUT via DpiShim.

class UvmSequenceItem {
public:
    uint64_t data = 0;
};

class UvmSequencer {
public:
    void add_item(const UvmSequenceItem &item) {
        items_.push_back(item);
    }

    bool next_item(UvmSequenceItem &out) {
        if (idx_ >= items_.size()) return false;
        out = items_[idx_++];
        return true;
    }

private:
    std::vector<UvmSequenceItem> items_;
    std::size_t idx_ = 0;
};

class UvmDriver {
public:
    UvmDriver(DpiShim &shim,
              const std::string &in_name,
              const std::string &out_name,
              std::size_t width)
        : shim_(shim), in_name_(in_name), out_name_(out_name), width_(width) {}

    void run(UvmSequencer &seq, uint64_t step_time) {
        UvmSequenceItem item;
        while (seq.next_item(item)) {
            shim_.set_signal(in_name_, item.data, width_);
            shim_.run(step_time);
            uint64_t resp = shim_.get_signal(out_name_);
            last_response_ = resp;
        }
    }

    uint64_t last_response() const { return last_response_; }

private:
    DpiShim &shim_;
    std::string in_name_;
    std::string out_name_;
    std::size_t width_;
    uint64_t last_response_ = 0;
};

class UvmEnv {
public:
    UvmEnv(Kernel &kernel, RtlDesign &design,
           const std::string &in_name,
           const std::string &out_name,
           std::size_t width)
        : kernel_(kernel),
          shim_(kernel_, design),
          driver_(shim_, in_name, out_name, width) {}

    UvmSequencer &sequencer() { return sequencer_; }
    UvmDriver &driver() { return driver_; }

private:
    Kernel &kernel_;
    DpiShim shim_;
    UvmSequencer sequencer_;
    UvmDriver driver_;
};

} // namespace sv

#endif // __UVM_ENV_HPP__
