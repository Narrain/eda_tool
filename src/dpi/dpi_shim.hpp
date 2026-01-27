// path: src/dpi/dpi_shim.hpp
#ifndef __DPI_SHIM_HPP__
#define __DPI_SHIM_HPP__

#include <cstdint>
#include <string>

#include "../sim/kernel.hpp"
#include "../ir/rtl_ir.hpp"

namespace sv {

class DpiShim {
public:
    DpiShim(Kernel &kernel, RtlDesign &design)
        : kernel_(kernel), design_(design) {
        kernel_.load_design(&design_);
    }

    void set_signal(const std::string &name, uint64_t value, std::size_t width);
    uint64_t get_signal(const std::string &name) const;

    void run(uint64_t max_time);

private:
    Kernel &kernel_;
    RtlDesign &design_;
};

} // namespace sv

extern "C" {

// C-friendly API surface
void dpi_set_signal(sv::DpiShim *shim,
                    const char *name,
                    uint64_t value,
                    std::size_t width);

uint64_t dpi_get_signal(sv::DpiShim *shim,
                        const char *name);

void dpi_run(sv::DpiShim *shim,
             uint64_t max_time);

}

#endif // __DPI_SHIM_HPP__
