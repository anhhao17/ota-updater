#include "ota/progress_sinks.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace flash {

namespace {
bool g_progress_line_active = false;
} // namespace

FileProgressSink::FileProgressSink(std::string path) : path_(std::move(path)) {}

void FileProgressSink::OnProgress(const ProgressEvent& e) {
    int comp_pct = 0;
    int ota_pct = 0;
    if (e.comp_total > 0) {
        comp_pct = static_cast<int>((e.comp_done * 100ULL) / e.comp_total);
        if (comp_pct > 100)
            comp_pct = 100;
    }
    if (e.overall_total > 0) {
        ota_pct = static_cast<int>((e.overall_done * 100ULL) / e.overall_total);
        if (ota_pct > 100)
            ota_pct = 100;
    }

    const std::string tmp_path = path_ + ".tmp";
    std::ofstream os(tmp_path, std::ios::trunc);
    if (!os.good())
        return;

    os << "{"
       << "\"component\":\"" << std::string(e.component) << "\","
       << "\"component_percent\":" << comp_pct << ","
       << "\"overall_percent\":" << ota_pct << "}";
    os.close();

    std::rename(tmp_path.c_str(), path_.c_str());
}

void ConsoleProgressSink::OnProgress(const ProgressEvent& e) {
    int comp_pct = 0;
    int ota_pct = 0;
    if (e.comp_total > 0) {
        comp_pct = static_cast<int>((e.comp_done * 100ULL) / e.comp_total);
        if (comp_pct > 100)
            comp_pct = 100;
    }
    if (e.overall_total > 0) {
        ota_pct = static_cast<int>((e.overall_done * 100ULL) / e.overall_total);
        if (ota_pct > 100)
            ota_pct = 100;
    }

    static std::string last_component;
    static bool component_finished = false;
    const std::string cur_component(e.component);
    if (cur_component != last_component) {
        component_finished = false;
        last_component = cur_component;
    }

    if (e.overall_total > 0) {
        std::fprintf(stderr,
                     "\r[%.*s] %3d%% | OTA %3d%%",
                     (int)e.component.size(),
                     e.component.data(),
                     comp_pct,
                     ota_pct);
    } else {
        std::fprintf(stderr,
                     "\r[%.*s] %3d%% | OTA --",
                     (int)e.component.size(),
                     e.component.data(),
                     comp_pct);
    }
    std::fflush(stderr);
    g_progress_line_active = true;

    if (comp_pct >= 100 && !component_finished) {
        std::fprintf(stderr, "\n");
        component_finished = true;
        g_progress_line_active = false;
    }

    if (e.overall_total > 0 && e.overall_done >= e.overall_total) {
        std::fprintf(stderr, "\n");
        g_progress_line_active = false;
    }
}

bool IsProgressLineActive() { return g_progress_line_active; }

void ClearProgressLine() {
    if (g_progress_line_active) {
        std::fprintf(stderr, "\n");
        g_progress_line_active = false;
    }
}

} // namespace flash
