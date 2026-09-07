#pragma once

// ARCHITECTURE.md section 9: log lifecycle events only. Nothing here may be called from
// the per-frame path - CaptureSession::OnFrameArrived and Renderer::Render must stay
// silent, otherwise a 60 Hz session turns into a 60 Hz disk writer.

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace overlaydesk {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Off = 5,
};

namespace detail {
void LogWrite(LogLevel level, std::string_view message);
bool LogIsEnabled(LogLevel level) noexcept;
}  // namespace detail

// Opens <appDataDirectory>/logs/overlaydesk.log. Never throws: if the log file cannot be
// opened we degrade to OutputDebugString only, because losing logging must not stop the
// application from starting.
void LogInitialize(const std::filesystem::path& appDataDirectory) noexcept;
void LogShutdown() noexcept;

void SetLogLevel(LogLevel level) noexcept;
LogLevel GetLogLevel() noexcept;

// Parses "trace" / "debug" / "info" / "warn" / "error" / "off"; returns Info on anything
// unrecognised so a bad settings file cannot silence the log.
LogLevel ParseLogLevel(std::string_view name) noexcept;
std::string_view LogLevelName(LogLevel level) noexcept;

template <typename... Args>
void LogTrace(std::format_string<Args...> fmt, Args&&... args) {
    if (detail::LogIsEnabled(LogLevel::Trace)) {
        detail::LogWrite(LogLevel::Trace, std::format(fmt, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void LogDebug(std::format_string<Args...> fmt, Args&&... args) {
    if (detail::LogIsEnabled(LogLevel::Debug)) {
        detail::LogWrite(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void LogInfo(std::format_string<Args...> fmt, Args&&... args) {
    if (detail::LogIsEnabled(LogLevel::Info)) {
        detail::LogWrite(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void LogWarn(std::format_string<Args...> fmt, Args&&... args) {
    if (detail::LogIsEnabled(LogLevel::Warn)) {
        detail::LogWrite(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
    }
}

template <typename... Args>
void LogError(std::format_string<Args...> fmt, Args&&... args) {
    if (detail::LogIsEnabled(LogLevel::Error)) {
        detail::LogWrite(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
    }
}

}  // namespace overlaydesk
