// path: src/synth/synth_driver.hpp
#ifndef __SYNTH_DRIVER_HPP__
#define __SYNTH_DRIVER_HPP__

#include "../ir/rtl_ir.hpp"
#include "netlist.hpp"
#include "techmap.hpp"

namespace sv {

class SynthDriver {
public:
    explicit SynthDriver(const RtlDesign &rtl)
        : rtl_(rtl) {}

    NetlistDesign run() {
        TechMapper tm(rtl_);
        return tm.map();
    }

private:
    const RtlDesign &rtl_;
};

} // namespace sv

#endif // __SYNTH_DRIVER_HPP__
