#include "fanshim/logger.hpp"

#include <execinfo.h>
#include <signal.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>


inline constexpr std::string_view IDENTIFIER = "fanshim";
inline constexpr std::string_view SYSLOG_LOGGER_NAME = "syslog";
inline constexpr std::string_view LOG_PATTERN = "[%Y.%m.%d %H:%M:%S.%e] (%L): %v";
inline constexpr std::string_view LOG_LEVEL_ENVIRONMENT_VARIABLE = "SHIM_LOG_LEVEL";
inline constexpr size_t BACKTRACE_SIZE = 4;


void logSignal(int32_t signum, siginfo_t* info, void* context)
{
    void* buffer[BACKTRACE_SIZE];
    spdlog::critical("Signal: {} (code:{}, address:{})", signum, info->si_code, info->si_addr);

    int32_t symbol_count = backtrace(buffer, BACKTRACE_SIZE);
    if (symbol_count > 0) {
        char** symbols = backtrace_symbols(buffer, symbol_count);
        if (symbols) {
            for (int32_t i = 0; i < symbol_count; i++) {
                spdlog::critical(symbols[i]);
            }
            free(symbols);
        }
    }

    LoggingInterface::flush();
    spdlog::shutdown();
    abort();
}

LoggingInterface::LoggingInterface() : _level(LogLevel::WARN)
{
    char* level = getenv(LOG_LEVEL_ENVIRONMENT_VARIABLE.data());
    if (level) {
        try {
            _level = static_cast<LogLevel>(std::stoi(level));
        }
        catch (const std::invalid_argument& ia) {
            fprintf(stderr, "Failed to read log level from environment: %s", ia.what());
            _level = LogLevel::WARN;
        }
    }

    struct sigaction action;
    std::vector<int32_t> signals = {SIGILL, SIGSEGV};

    for (const auto& signal : signals) {
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        action.sa_sigaction = &logSignal;
        sigaction(signal, &action, NULL);
    }

    spdlog::set_pattern(LOG_PATTERN.data(), spdlog::pattern_time_type::local);
    spdlog::set_level(static_cast<spdlog::level::level_enum>(_level));
    spdlog::flush_every(FLUSH_INTERVAL);

    auto syslog_sink = spdlog::syslog_logger_mt(SYSLOG_LOGGER_NAME.data(), IDENTIFIER.data(), LOG_PID);
    syslog_sink->set_pattern(LOG_PATTERN.data(), spdlog::pattern_time_type::local);
    spdlog::set_default_logger(syslog_sink);
}

void LoggingInterface::flush()
{
    spdlog::default_logger()->flush();
}

void LoggingInterface::setLevel(LogLevel level)
{
    _level = level;
    spdlog::set_level(static_cast<spdlog::level::level_enum>(_level));
}

LoggingContext& LoggingContext::instance()
{
    static LoggingContext context;
    return context;
}

LoggingInterface& LoggingContext::logger()
{
    return _logger;
}

LoggingContext::LoggingContext() : _logger()
{}

LoggingContext::~LoggingContext()
{
    _logger.flush();
}
