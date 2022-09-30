#pragma once

#include <signal.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdint>


inline constexpr std::chrono::seconds FLUSH_INTERVAL = std::chrono::seconds(5);

enum class LogLevel
{
    DEBUG = spdlog::level::level_enum::debug,
    INFO = spdlog::level::level_enum::info,
    WARN = spdlog::level::level_enum::warn,
    ERROR = spdlog::level::level_enum::err
};

class LoggingInterface
{
public:
    LoggingInterface();

    static void flush();

    template <typename... Args>
    inline void debug(const char* fmt, const Args&... args)
    {
        spdlog::debug(fmt, args...);
    }

    template <typename... Args>
    inline void info(const char* fmt, const Args&... args)
    {
        spdlog::info(fmt, args...);
    }

    template <typename... Args>
    inline void warn(const char* fmt, const Args&... args)
    {
        spdlog::warn(fmt, args...);
    }

    template <typename... Args>
    inline void error(const char* fmt, const Args&... args)
    {
        spdlog::error(fmt, args...);
    }

    void setLevel(LogLevel level);

private:
    LogLevel _level;
};

class LoggingContext
{
public:
    static LoggingContext& instance();

    LoggingInterface& logger();

private:
    LoggingContext();
    ~LoggingContext();

    LoggingInterface _logger;
};


inline LoggingInterface& logger()
{
    return LoggingContext::instance().logger();
}
