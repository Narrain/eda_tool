// path: src/synth/techmap.hpp
#ifndef __TECHMAP_HPP__
#define __TECHMAP_HPP__

#include "netlist.hpp"
#include "../ir/rtl_ir.hpp"

namespace sv {

class TechMapper {
public:
    explicit TechMapper(const RtlDesign &rtl)
        : rtl_(rtl) {}

    NetlistDesign map();

private:
    const RtlDesign &rtl_;

    NetlistModule map_module(const RtlModule &m);
    void ensure_net(NetlistModule &nm, const std::string &name, std::size_t width = 1);
    std::string lower_expr(NetlistModule &nm, const RtlExpr &e);
    void lower_assign(NetlistModule &nm, const RtlAssign &a);
};

} // namespace sv

#endif // __TECHMAP_HPP__
