#include "util/logger.hpp"
#include "ota/progress_sinks.hpp"

#include <cstring>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace flash {

namespace {
std::mutex g_mu;
LogLevel g_level = LogLevel::Info;

const char* ToStr(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default:              return "LOG";
    }
}

void FormatTimestamp(char* buf, size_t buf_len) {
    if (buf_len == 0) return;
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    if (localtime_r(&now, &tm) == nullptr) {
        buf[0] = '\0';
        return;
    }
    std::strftime(buf, buf_len, "%Y-%m-%d %H:%M:%S", &tm);
}

const char* BaseName(const char* file) {
    if (!file || *file == '\0') return nullptr;
    const char* slash = std::strrchr(file, '/');
    const char* backslash = std::strrchr(file, '\\');
    const char* base = slash;
    if (!base || (backslash && backslash > base)) {
        base = backslash;
    }
    return base ? (base + 1) : file;
}
} // namespace

Logger& Logger::Instance() {
    static Logger inst;
    return inst;
}

void Logger::SetLevel(LogLevel lvl) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_level = lvl;
}

LogLevel Logger::Level() const {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_level;
}

void Logger::Log(LogLevel lvl, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    VLogWithSource(lvl, nullptr, 0, fmt, ap);
    va_end(ap);
}

void Logger::VLog(LogLevel lvl, const char* fmt, va_list ap) {
    VLogWithSource(lvl, nullptr, 0, fmt, ap);
}

void Logger::LogWithSource(LogLevel lvl,
                           const char* file,
                           int line,
                           const char* fmt,
                           ...) {
    va_list ap;
    va_start(ap, fmt);
    VLogWithSource(lvl, file, line, fmt, ap);
    va_end(ap);
}

void Logger::VLogWithSource(LogLevel lvl,
                            const char* file,
                            int line,
                            const char* fmt,
                            va_list ap) {
    LogLevel cur;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        cur = g_level;
    }

    if (lvl < cur) return;

    // Print to stderr (typical for tools)
    std::lock_guard<std::mutex> lk(g_mu);
    if (IsProgressLineActive()) {
        ClearProgressLine();
    }
    char ts[32]{};
    FormatTimestamp(ts, sizeof(ts));
    if (ts[0] != '\0') {
        std::fprintf(stderr, "[%s] [%s] ", ts, ToStr(lvl));
    } else {
        std::fprintf(stderr, "[%s] ", ToStr(lvl));
    }
    const char* base = BaseName(file);
    if (base && line > 0) {
        std::fprintf(stderr, "[%s:%d] ", base, line);
    }
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
}

} // namespace flash
