#include "logger.h"

#include "../io/path/path_manager.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#if defined(_WIN32)
#include <io.h>
#include <process.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <sstream>

namespace elysia::tools
{
namespace
{
[[nodiscard]] int level_rank(LogLevel level) noexcept
{
    return static_cast<int>(level);
}

[[nodiscard]] const char* level_name(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Terminating: return "TERMINATING";
    }
    return "UNKNOWN";
}

[[nodiscard]] std::string_view console_color_code(LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Debug: return "\x1b[90m";
    case LogLevel::Info: return "\x1b[36m";
    case LogLevel::Warn: return "\x1b[33m";
    case LogLevel::Error: return "\x1b[31m";
    case LogLevel::Terminating: return "\x1b[38;5;88m";
    }
    return {};
}

[[nodiscard]] bool standard_error_is_terminal() noexcept
{
#if defined(_WIN32)
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

#if defined(_WIN32)
[[nodiscard]] bool enable_virtual_terminal_processing() noexcept
{
    const HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    if (!handle || handle == INVALID_HANDLE_VALUE)
        return false;

    DWORD mode = 0;
    if (!GetConsoleMode(handle,&mode))
        return false;
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
        return true;
    return SetConsoleMode(handle,mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}
#endif

[[nodiscard]] bool resolve_console_colors(ConsoleColorMode mode) noexcept
{
    if (mode == ConsoleColorMode::Never)
        return false;

#if defined(_WIN32)
    if (mode == ConsoleColorMode::Always)
    {
        (void)enable_virtual_terminal_processing();
        return true;
    }
#else
    if (mode == ConsoleColorMode::Always)
        return true;
#endif

    if (std::getenv("NO_COLOR") != nullptr || !standard_error_is_terminal())
        return false;

#if defined(_WIN32)
    return enable_virtual_terminal_processing();
#else
    return true;
#endif
}

[[nodiscard]] std::tm local_time(std::time_t time)
{
    std::tm result{};
#if defined(_WIN32)
    localtime_s(&result,&time);
#else
    localtime_r(&time,&result);
#endif
    return result;
}

[[nodiscard]] int process_id() noexcept
{
#if defined(_WIN32)
    return _getpid();
#else
    return static_cast<int>(getpid());
#endif
}

[[nodiscard]] std::string_view source_file_name(const std::source_location& location) noexcept
{
    const char* file_name = location.file_name();
    if (!file_name)
        return {};

    const char* short_name = file_name;
    for (const char* cursor = file_name; *cursor; ++cursor)
    {
        if (*cursor == '/' || *cursor == '\\')
            short_name = cursor + 1;
    }
    return short_name;
}
}

bool Logger::configure(const LoggerConfig& config) noexcept
{
    try
    {
        if (config.file_mode == LogFileMode::Append)
        {
            const std::filesystem::path append_name(config.append_file_name);
            if (append_name.empty() || append_name.is_absolute() || append_name.has_parent_path())
                return false;
        }
        std::lock_guard lock(_mutex);
        if (_lifecycle_started)
            return false;
        _config = config;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Logger::initialize() noexcept
{
    initialize_console();
    initialize_file();
}

void Logger::initialize_console() noexcept
{
    try
    {
        std::lock_guard lock(_mutex);
        if (_console_initialized)
            return;
        _lifecycle_started = true;
        _console_colors_enabled = _config.console_enabled
            && resolve_console_colors(_config.console_color_mode);
        _console_initialized = true;
    }
    catch (...)
    {
        _console_colors_enabled = false;
    }
}

void Logger::initialize_file() noexcept
{
    try
    {
        std::lock_guard lock(_mutex);
        _lifecycle_started = true;
        if (_file_initialized)
            return;
        if (_config.file_mode == LogFileMode::Disabled)
        {
            _file_initialized = true;
            return;
        }

        _active_file_path.reset();

        const auto* path_manager = elysia::io::PathManager::instance();
        if (!path_manager || !path_manager->is_initialized())
            return;

        const std::filesystem::path path = _config.file_mode == LogFileMode::Append
            ? path_manager->logs() / _config.append_file_name
            : new_run_file_path();
        const std::ios::openmode mode = _config.file_mode == LogFileMode::Append
            ? std::ios::out | std::ios::app
            : std::ios::out;
        _file.clear();
        _file.open(path,mode);
        if (!_file.is_open())
        {
            _file.clear();
            return;
        }
        _active_file_path = path;
        _file_initialized = true;
    }
    catch (...)
    {
        disable_file_sink();
    }
}

void Logger::shutdown() noexcept
{
    try
    {
        std::lock_guard lock(_mutex);
        if (_file.is_open())
        {
            _file.flush();
            _file.close();
        }
        _active_file_path.reset();
        _lifecycle_started = false;
        _console_initialized = false;
        _file_initialized = false;
        _console_colors_enabled = false;
    }
    catch (...)
    {
        disable_file_sink();
        _lifecycle_started = false;
        _console_initialized = false;
        _file_initialized = false;
        _console_colors_enabled = false;
    }
}

bool Logger::is_initialized() const noexcept
{
    try
    {
        std::lock_guard lock(_mutex);
        return _lifecycle_started;
    }
    catch (...)
    {
        return false;
    }
}

std::optional<std::filesystem::path> Logger::active_file_path() const noexcept
{
    try
    {
        std::lock_guard lock(_mutex);
        return _active_file_path;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void Logger::log(LogLevel level,std::string_view category,std::string_view message,
    std::source_location location) noexcept
{
    try
    {
        std::lock_guard lock(_mutex);
        if (!should_log(level))
            return;
        if (!_file.is_open() && !_config.console_enabled)
            return;
        const std::string line = format_line(level,category,message,location);
        if (_file.is_open())
        {
            _file << line << '\n';
            if (!_file.good())
            {
                disable_file_sink();
            }
            else if (level_rank(level) >= level_rank(LogLevel::Warn))
            {
                _file.flush();
                if (!_file.good())
                    disable_file_sink();
            }
        }
        if (_config.console_enabled)
            write_console_line(level,line,_console_colors_enabled);
    }
    catch (...)
    {
        disable_file_sink();
        if (_config.console_enabled)
            write_console_fallback(level,category,message,location,_console_colors_enabled);
    }
}

void Logger::debug(std::string_view category,std::string_view message,std::source_location location) noexcept
{
    log(LogLevel::Debug,category,message,location);
}

void Logger::info(std::string_view category,std::string_view message,std::source_location location) noexcept
{
    log(LogLevel::Info,category,message,location);
}

void Logger::warn(std::string_view category,std::string_view message,std::source_location location) noexcept
{
    log(LogLevel::Warn,category,message,location);
}

void Logger::error(std::string_view category,std::string_view message,std::source_location location) noexcept
{
    log(LogLevel::Error,category,message,location);
}

void Logger::terminating(std::string_view category,std::string_view message,
    std::source_location location) noexcept
{
    log(LogLevel::Terminating,category,message,location);
}

bool Logger::should_log(LogLevel level) const noexcept
{
    return level_rank(level) >= level_rank(_config.minimum_level);
}

std::filesystem::path Logger::new_run_file_path() const
{
    const auto* path_manager = elysia::io::PathManager::instance();
    const std::time_t now = std::time(nullptr);
    std::ostringstream stem;
    const std::tm timestamp = local_time(now);
    stem << "Elysia-" << std::put_time(&timestamp,"%Y%m%d-%H%M%S") << '-' << process_id();
    const std::filesystem::path logs = path_manager->logs();
    for (unsigned int index = 0;; ++index)
    {
        const std::filesystem::path candidate = logs / (stem.str()
            + (index == 0 ? "" : "-" + std::to_string(index)) + ".log");
        if (!std::filesystem::exists(candidate))
            return candidate;
    }
}

std::string Logger::format_line(LogLevel level,std::string_view category,std::string_view message,
    const std::source_location& location) const
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    const std::tm timestamp = local_time(time);
    std::ostringstream output;
    output << std::put_time(&timestamp,"%Y-%m-%d %H:%M:%S")
        << " [" << level_name(level) << "]"
        << " [" << category << "]"
        << " (" << source_file_name(location) << ':' << location.line() << ") " << message;
    return output.str();
}

void Logger::disable_file_sink() noexcept
{
    try
    {
        if (_file.is_open())
            _file.close();
        _file.clear();
        _active_file_path.reset();
        _file_initialized = false;
    }
    catch (...)
    {
    }
}

void Logger::write_console_line(LogLevel level,std::string_view line,bool colors_enabled) noexcept
{
    try
    {
        const std::string_view name = level_name(level);
        const size_t name_position = line.find(name);
        const bool has_level_tag = name_position > 0
            && name_position + name.size() < line.size()
            && line[name_position - 1] == '['
            && line[name_position + name.size()] == ']';
        if (!colors_enabled || !has_level_tag)
        {
            std::clog.write(line.data(),static_cast<std::streamsize>(line.size()));
            std::clog.put('\n');
            std::clog.flush();
            return;
        }

        const size_t tag_start = name_position - 1;
        const size_t tag_end = name_position + name.size() + 1;
        std::clog.write(line.data(),static_cast<std::streamsize>(tag_start));
        std::clog << console_color_code(level);
        std::clog.write(line.data() + tag_start,static_cast<std::streamsize>(tag_end - tag_start));
        std::clog << "\x1b[0m";
        std::clog.write(line.data() + tag_end,static_cast<std::streamsize>(line.size() - tag_end));
        std::clog.put('\n');
        std::clog.flush();
    }
    catch (...)
    {
    }
}

void Logger::write_console_fallback(LogLevel level,std::string_view category,std::string_view message,
    const std::source_location& location,bool colors_enabled) noexcept
{
    try
    {
        const std::string_view source_file = source_file_name(location);
        if (colors_enabled)
            std::clog << console_color_code(level);
        std::clog << '[' << level_name(level) << ']';
        if (colors_enabled)
            std::clog << "\x1b[0m";
        std::clog << " [" << category << "] ("
            << source_file << ':' << location.line() << ") " << message << '\n';
        std::clog.flush();
    }
    catch (...)
    {
    }
}
}
