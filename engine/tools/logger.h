#pragma once

#include "singleton.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <ostream>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace elysia::tools
{
enum class LogLevel
{
    Debug,
    Info,
    Warn,
    Error,
    Terminating
};

enum class LogFileMode
{
    Disabled,
    Append,
    NewRunFile
};

enum class ConsoleColorMode
{
    Auto,
    Always,
    Never
};

struct LoggerConfig
{
    LogLevel minimum_level{ LogLevel::Debug };
    LogFileMode file_mode{ LogFileMode::NewRunFile };
    std::string append_file_name{ "Elysia.log" };
    bool console_enabled = true;
    ConsoleColorMode console_color_mode{ ConsoleColorMode::Auto };
};

class Logger : public Singleton<Logger>
{
    friend class Singleton<Logger>;

public:
    [[nodiscard]] bool configure(const LoggerConfig& config) noexcept;
    void initialize_console() noexcept;
    void initialize_file() noexcept;
    void initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool is_initialized() const noexcept;
    [[nodiscard]] std::optional<std::filesystem::path> active_file_path() const noexcept;

    void log(LogLevel level,std::string_view category,std::string_view message,
        std::source_location location = std::source_location::current()) noexcept;
    void debug(std::string_view category,std::string_view message,
        std::source_location location = std::source_location::current()) noexcept;
    void info(std::string_view category,std::string_view message,
        std::source_location location = std::source_location::current()) noexcept;
    void warn(std::string_view category,std::string_view message,
        std::source_location location = std::source_location::current()) noexcept;
    void error(std::string_view category,std::string_view message,
        std::source_location location = std::source_location::current()) noexcept;
    void terminating(std::string_view category,std::string_view message,
        std::source_location location = std::source_location::current()) noexcept;

    template <typename Writer>
    void log_stream(LogLevel level,std::string_view category,Writer&& writer,
        std::source_location location = std::source_location::current()) noexcept
    {
        try
        {
            std::ostringstream message;
            std::forward<Writer>(writer)(message);
            log(level,category,message.str(),location);
        }
        catch (...)
        {
            log(level,category,"Log message formatting failed",location);
        }
    }

private:
    Logger() = default;

    [[nodiscard]] bool should_log(LogLevel level) const noexcept;
    [[nodiscard]] std::filesystem::path new_run_file_path() const;
    [[nodiscard]] std::string format_line(LogLevel level,std::string_view category,
        std::string_view message,const std::source_location& location) const;
    void disable_file_sink() noexcept;
    static void write_console_line(LogLevel level,std::string_view line,bool colors_enabled) noexcept;
    static void write_console_fallback(LogLevel level,std::string_view category,
        std::string_view message,const std::source_location& location,bool colors_enabled) noexcept;

private:
    mutable std::mutex _mutex;
    LoggerConfig _config;
    std::ofstream _file;
    std::optional<std::filesystem::path> _active_file_path;
    bool _lifecycle_started = false;
    bool _console_initialized = false;
    bool _file_initialized = false;
    bool _console_colors_enabled = false;
};
}

#define ELYSIA_LOG_STREAM(level,category,...) \
    (::elysia::tools::Logger::instance()->log_stream((level),(category), \
        [&](std::ostream& elysia_log_stream) { elysia_log_stream << __VA_ARGS__; }, \
        std::source_location::current()))

#define ELYSIA_LOG(category,...) ELYSIA_LOG_STREAM(::elysia::tools::LogLevel::Info,(category),__VA_ARGS__)
#define ELYSIA_LOG_DEBUG(category,...) ELYSIA_LOG_STREAM(::elysia::tools::LogLevel::Debug,(category),__VA_ARGS__)
#define ELYSIA_LOG_INFO(category,...) ELYSIA_LOG_STREAM(::elysia::tools::LogLevel::Info,(category),__VA_ARGS__)
#define ELYSIA_LOG_WARN(category,...) ELYSIA_LOG_STREAM(::elysia::tools::LogLevel::Warn,(category),__VA_ARGS__)
#define ELYSIA_LOG_ERROR(category,...) ELYSIA_LOG_STREAM(::elysia::tools::LogLevel::Error,(category),__VA_ARGS__)
#define ELYSIA_LOG_TERMINATING(category,...) ELYSIA_LOG_STREAM(::elysia::tools::LogLevel::Terminating,(category),__VA_ARGS__)
