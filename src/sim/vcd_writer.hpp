// path: src/sim/vcd_writer.hpp
#ifndef __VCD_WRITER_HPP__
#define __VCD_WRITER_HPP__

#include <string>
#include <unordered_map>
#include <fstream>
#include "value.hpp"

namespace sv {

class VcdWriter {
public:
    explicit VcdWriter(const std::string &path)
        : path_(path), out_(path) {}

    bool good() const { return out_.good(); }

    void add_signal(const std::string &name, std::size_t width);
    void dump_header();
    void dump_time(uint64_t time);
    void dump_value(const std::string &name, const Value &v);

private:
    std::string path_;
    std::ofstream out_;
    std::unordered_map<std::string, std::string> id_map_;
    uint64_t next_id_ = 0;

    std::string make_id();
};

} // namespace sv

#endif // __VCD_WRITER_HPP__
