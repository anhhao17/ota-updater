#define _FILE_OFFSET_BITS 64

#include "util/logger.hpp"
#include "ota/ota_installer.hpp"
#include "ota/progress_sinks.hpp"
#include "system/signals.hpp"

#include <getopt.h>
#include <string>

namespace {

struct CliOptions {
    std::string input_path;
    std::string progress_file;
    bool verbose = false;
    bool show_help = false;
};

void PrintUsage(const char* argv0) {
    flash::LogError("Usage: %s -i <ota.tar | -> [-v] [--progress-file <path>]", argv0);
}

bool ParseCliOptions(int argc, char** argv, CliOptions& out) {
    static option long_opts[] = {
        {"input", required_argument, nullptr, 'i'},
        {"progress-file", required_argument, nullptr, 'p'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    int idx = 0;
    int c;
    while ((c = getopt_long(argc, argv, "hi:p:v", long_opts, &idx)) != -1) {
        switch (c) {
            case 'h':
                out.show_help = true;
                return true;
            case 'i':
                out.input_path = optarg;
                break;
            case 'p':
                out.progress_file = optarg;
                break;
            case 'v':
                out.verbose = true;
                break;
            default:
                return false;
        }
    }

    return !out.input_path.empty();
}
} // namespace

int main(int argc, char** argv) {
    flash::InstallSignalHandlers();
    flash::Logger::Instance().SetLevel(flash::LogLevel::Info);

    CliOptions options;
    if (!ParseCliOptions(argc, argv, options)) {
        PrintUsage(argv[0]);
        return 2;
    }
    if (options.show_help) {
        PrintUsage(argv[0]);
        return 0;
    }
    if (options.verbose) {
        flash::Logger::Instance().SetLevel(flash::LogLevel::Debug);
    }

    flash::OtaInstaller installer;
    std::unique_ptr<flash::IProgress> progress_sink;
    if (!options.progress_file.empty()) {
        progress_sink = std::make_unique<flash::FileProgressSink>(options.progress_file);
    } else {
        progress_sink = std::make_unique<flash::ConsoleProgressSink>();
    }
    installer.SetProgressSink(progress_sink.get());
    auto r = installer.Run(options.input_path);
    if (!r.is_ok()) {
        flash::LogError("%s", r.message().c_str());
        return 1;
    }
    return 0;
}
