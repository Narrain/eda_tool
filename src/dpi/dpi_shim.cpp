// path: src/dpi/dpi_shim.cpp
#include "dpi_shim.hpp"

namespace sv {

void DpiShim::set_signal(const std::string &name, uint64_t value, std::size_t width) {
    Value v = Value::from_uint(width, value);
    kernel_.set_signal(name, v);
}

uint64_t DpiShim::get_signal(const std::string &name) const {
    const Value *v = kernel_.get_signal(name);
    if (!v) return 0;
    uint64_t x = 0;
    for (std::size_t i = 0; i < v->width() && i < 64; ++i) {
        if (v->get(i) == Logic4::L1) {
            x |= (1ull << i);
        }
    }
    return x;
}

void DpiShim::run(uint64_t max_time) {
    kernel_.run(max_time);
}

} // namespace sv

extern "C" {

void dpi_set_signal(sv::DpiShim *shim,
                    const char *name,
                    uint64_t value,
                    std::size_t width) {
    if (!shim || !name) return;
    shim->set_signal(name, value, width);
}

uint64_t dpi_get_signal(sv::DpiShim *shim,
                        const char *name) {
    if (!shim || !name) return 0;
    return shim->get_signal(name);
}

void dpi_run(sv::DpiShim *shim,
             uint64_t max_time) {
    if (!shim) return;
    shim->run(max_time);
}

}
