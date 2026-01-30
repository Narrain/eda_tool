#ifndef __VCD_WRITER_HPP__
#define __VCD_WRITER_HPP__

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "value.hpp"

namespace sv {

class VcdWriter {
public:
    explicit VcdWriter(const std::string &filename)
        : filename_(filename)
    {
        if (!filename_.empty()) {
            out_.open(filename_, std::ios::out | std::ios::trunc);
        }
    }

    ~VcdWriter() {
        if (out_.is_open()) {
            out_.flush();
            out_.close();
        }
    }

    bool good() const {
        return !filename_.empty() && out_.good();
    }

    void add_signal(const std::string &name, std::size_t width) {
        if (!good()) return;
        if (name_to_id_.count(name)) return;

        std::string id = make_id(id_counter_++);
        name_to_id_[name] = id;
        signals_.push_back({name, id, width});
    }

    void dump_header() {
        if (!good() || header_written_) return;

        out_ << "$date\n";
        out_ << "    today\n";
        out_ << "$end\n";
        out_ << "$version\n";
        out_ << "    svtool\n";
        out_ << "$end\n";
        out_ << "$timescale 1ns $end\n";

        out_ << "$scope module top $end\n";
        for (const auto &s : signals_) {
            out_ << "$var wire " << s.width << " " << s.id
                 << " " << s.name << " $end\n";
        }
        out_ << "$upscope $end\n";
        out_ << "$enddefinitions $end\n";

        header_written_ = true;
        out_.flush();
    }

    void dump_time(uint64_t t) {
        if (!good() || !header_written_) return;
        out_ << "#" << t << "\n";
        out_.flush();
    }

    void dump_value(const std::string &name, const Value &v) {
        if (!good() || !header_written_) return;

        auto it = name_to_id_.find(name);
        if (it == name_to_id_.end()) return;

        const std::string &id = it->second;
        std::string bits;

        std::size_t w = v.width();
        if (w == 0) {
            bits = "x";
        } else {
            bits.resize(w);
            for (std::size_t i = 0; i < w; ++i) {
                Logic4 b = v.get(w - 1 - i); // MSB first
                char c = 'x';
                switch (b) {
                case Logic4::L0: c = '0'; break;
                case Logic4::L1: c = '1'; break;
                case Logic4::LX: c = 'x'; break;
                case Logic4::LZ: c = 'z'; break;
                }
                bits[i] = c;
            }
        }

        out_ << "b" << bits << " " << id << "\n";
        out_.flush();
    }

private:
    struct SigInfo {
        std::string name;
        std::string id;
        std::size_t width;
    };

    std::string filename_;
    std::ofstream out_;

    bool header_written_ = false;

    std::unordered_map<std::string, std::string> name_to_id_;
    std::vector<SigInfo> signals_;
    std::size_t id_counter_ = 0;

    static std::string make_id(std::size_t n) {
        // Simple printable ID generator: '!'..'~'
        std::string s;
        do {
            char c = static_cast<char>('!' + (n % 94));
            s.push_back(c);
            n /= 94;
        } while (n > 0);
        return s;
    }
};

} // namespace sv

#endif // __VCD_WRITER_HPP__
