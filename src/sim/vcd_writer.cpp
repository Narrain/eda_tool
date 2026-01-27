// path: src/sim/vcd_writer.cpp
#include "vcd_writer.hpp"

namespace sv {

std::string VcdWriter::make_id() {
    std::string s;
    uint64_t x = next_id_++;
    do {
        char c = '!' + (x % 94); // printable ASCII 33-126
        s.push_back(c);
        x /= 94;
    } while (x);
    return s;
}

void VcdWriter::add_signal(const std::string &name, std::size_t width) {
    if (!out_) return;
    if (id_map_.count(name)) return;
    std::string id = make_id();
    id_map_[name] = id;
    out_ << "$var wire " << width << " " << id << " " << name << " $end\n";
}

void VcdWriter::dump_header() {
    if (!out_) return;
    out_ << "$date\n  today\n$end\n";
    out_ << "$version\n  svtool\n$end\n";
    out_ << "$timescale 1ns $end\n";
    out_ << "$scope module top $end\n";
    for (auto &kv : id_map_) {
        // already emitted in add_signal
        (void)kv;
    }
    out_ << "$upscope $end\n";
    out_ << "$enddefinitions $end\n";
}

void VcdWriter::dump_time(uint64_t time) {
    if (!out_) return;
    out_ << "#" << time << "\n";
}

void VcdWriter::dump_value(const std::string &name, const Value &v) {
    if (!out_) return;
    auto it = id_map_.find(name);
    if (it == id_map_.end()) return;
    std::string bits = v.to_string();
    out_ << "b" << bits << " " << it->second << "\n";
}

} // namespace sv
