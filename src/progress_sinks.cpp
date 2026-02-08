#include "flash/progress_sinks.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace flash {

FileProgressSink::FileProgressSink(std::string path)
    : path_(std::move(path)) {}

void FileProgressSink::OnProgress(const ProgressEvent& e) {
    int comp_pct = 0;
    int ota_pct = 0;
    if (e.comp_total > 0) {
        comp_pct = static_cast<int>((e.comp_done * 100ULL) / e.comp_total);
    }
    if (e.overall_total > 0) {
        ota_pct = static_cast<int>((e.overall_done * 100ULL) / e.overall_total);
    }

    const std::string tmp_path = path_ + ".tmp";
    std::ofstream os(tmp_path, std::ios::trunc);
    if (!os.good()) return;

    os << "{"
       << "\"component\":\"" << std::string(e.component) << "\","
       << "\"component_percent\":" << comp_pct << ","
       << "\"overall_percent\":" << ota_pct
       << "}";
    os.close();

    std::rename(tmp_path.c_str(), path_.c_str());
}

} // namespace flash
