#include "fanshim/logger.hpp"

#include <execinfo.h>
#include <signal.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>


inline constexpr std::string_view IDENTIFIER = "fanshim";
inline constexpr std::string_view LOG_FILE = "/var/log/devices/fanshim.log";
inline constexpr std::string_view LOG_PATTERN = "[%Y.%m.%d %H:%M:%S.%e] (%L): %v";
inline constexpr std::string_view LOG_LEVEL_ENVIRONMENT_VARIABLE = "SHIM_LOG_LEVEL";
inline constexpr size_t BACKTRACE_SIZE = 4;
inline constexpr size_t FILE_SIZE_MB = 1 * 1024 * 1024;
inline constexpr size_t MAX_LOG_FILES = 3;


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

    std::shared_ptr<spdlog::logger> logger;

    try {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(LOG_FILE.data(), FILE_SIZE_MB, MAX_LOG_FILES);
        spdlog::sinks_init_list sinks = {file_sink};
        logger = std::make_shared<spdlog::logger>(IDENTIFIER.data(), sinks);
    }
    catch (const spdlog::spdlog_ex& ex) {
        fprintf(stderr, "Log to file failed: %s\n", ex.what());
        abort();
    }

    spdlog::set_default_logger(logger);
    spdlog::set_pattern(LOG_PATTERN.data(), spdlog::pattern_time_type::local);
    spdlog::set_level(static_cast<spdlog::level::level_enum>(_level));
    spdlog::flush_every(FLUSH_INTERVAL);
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
