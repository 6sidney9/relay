#pragma once

#include <chrono>
#include <iostream>
#include <string>

namespace relay::log {

enum class Level { Info, Warn, Error };

inline const char* level_name(Level l) {
    switch (l) {
        case Level::Info:
            return "info";
        case Level::Warn:
            return "warn";
        case Level::Error:
            return "error";
    }
    return "?";
}

inline void write(Level level, const std::string& msg) {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const auto t = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    std::cerr << buf << " [" << level_name(level) << "] " << msg << '\n';
}

inline void info(const std::string& msg) { write(Level::Info, msg); }
inline void warn(const std::string& msg) { write(Level::Warn, msg); }
inline void error(const std::string& msg) { write(Level::Error, msg); }

}  // namespace relay::log
