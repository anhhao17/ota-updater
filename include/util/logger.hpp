#pragma once

#include <cstdarg>
#include <string>

namespace flash {

enum class LogLevel : int {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
    None  = 4,
};

class Logger {
public:
    static Logger& Instance();

    void SetLevel(LogLevel lvl);
    LogLevel Level() const;

    // printf-style logging
    void Log(LogLevel lvl, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
    void VLog(LogLevel lvl, const char* fmt, va_list ap);
    void LogWithSource(LogLevel lvl,
                       const char* file,
                       int line,
                       const char* fmt,
                       ...) __attribute__((format(printf, 5, 6)));
    void VLogWithSource(LogLevel lvl,
                        const char* file,
                        int line,
                        const char* fmt,
                        va_list ap);

private:
    Logger() = default;
};

#define LogDebug(...) ::flash::Logger::Instance().LogWithSource(::flash::LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)
#define LogInfo(...)  ::flash::Logger::Instance().LogWithSource(::flash::LogLevel::Info,  __FILE__, __LINE__, __VA_ARGS__)
#define LogWarn(...)  ::flash::Logger::Instance().LogWithSource(::flash::LogLevel::Warn,  __FILE__, __LINE__, __VA_ARGS__)
#define LogError(...) ::flash::Logger::Instance().LogWithSource(::flash::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)

} // namespace flash
