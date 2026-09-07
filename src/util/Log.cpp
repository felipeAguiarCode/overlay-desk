#include "util/Log.h"

#include <Windows.h>

#include <share.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <system_error>

#include "util/Win32Helpers.h"

namespace overlaydesk {
namespace {

// Anything above this and the log is truncated at startup rather than appended to, so a
// long-lived install does not grow an unbounded file.
constexpr std::uintmax_t kMaxLogFileBytes = 2u * 1024u * 1024u;

std::mutex g_mutex;
std::FILE* g_file = nullptr;
std::atomic<LogLevel> g_level{LogLevel::Info};

std::string Timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto local = std::chrono::current_zone()->to_local(now);
    return std::format("{:%F %T}", std::chrono::floor<std::chrono::milliseconds>(local));
}

}  // namespace

namespace detail {

bool LogIsEnabled(LogLevel level) noexcept {
    return level >= g_level.load(std::memory_order_relaxed);
}

void LogWrite(LogLevel level, std::string_view message) {
    // std::format can throw, the callers own that. Everything below is noexcept in spirit:
    // a logging failure must never propagate into application logic.
    std::string line;
    try {
        line = std::format("[{}] [{:<5}] {}\n", Timestamp(), LogLevelName(level), message);
    } catch (...) {
        line.assign(message);
        line.push_back('\n');
    }

    OutputDebugStringW(WideFromUtf8(line).c_str());

    const std::lock_guard lock(g_mutex);
    if (g_file != nullptr) {
        std::fwrite(line.data(), 1, line.size(), g_file);
        // Flushing on every record costs little at lifecycle-event frequency and is what
        // makes the log useful after a crash.
        std::fflush(g_file);
    }
}

}  // namespace detail

void LogInitialize(const std::filesystem::path& appDataDirectory) noexcept {
    try {
        if (appDataDirectory.empty()) {
            return;
        }

        const std::filesystem::path logDirectory = appDataDirectory / L"logs";
        std::error_code ec;
        std::filesystem::create_directories(logDirectory, ec);
        if (ec) {
            return;
        }

        const std::filesystem::path logFile = logDirectory / L"overlaydesk.log";
        const std::uintmax_t existingSize = std::filesystem::file_size(logFile, ec);
        const bool truncate = ec || existingSize > kMaxLogFileBytes;

        const std::lock_guard lock(g_mutex);
        if (g_file != nullptr) {
            std::fclose(g_file);
            g_file = nullptr;
        }
        // _SH_DENYNO spelled out rather than left to the CRT default: a log has to stay
        // openable while the session it describes is still running. Readers still have to
        // ask for FileShare.ReadWrite, since this handle holds write access.
        g_file = _wfsopen(logFile.c_str(), truncate ? L"wb" : L"ab", _SH_DENYNO);
    } catch (...) {
        // Degrade to OutputDebugString only.
    }
}

void LogShutdown() noexcept {
    const std::lock_guard lock(g_mutex);
    if (g_file != nullptr) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void SetLogLevel(LogLevel level) noexcept {
    g_level.store(level, std::memory_order_relaxed);
}

LogLevel GetLogLevel() noexcept {
    return g_level.load(std::memory_order_relaxed);
}

LogLevel ParseLogLevel(std::string_view name) noexcept {
    if (name == "trace") return LogLevel::Trace;
    if (name == "debug") return LogLevel::Debug;
    if (name == "info") return LogLevel::Info;
    if (name == "warn" || name == "warning") return LogLevel::Warn;
    if (name == "error") return LogLevel::Error;
    if (name == "off" || name == "none") return LogLevel::Off;
    return LogLevel::Info;
}

std::string_view LogLevelName(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info:  return "info";
        case LogLevel::Warn:  return "warn";
        case LogLevel::Error: return "error";
        case LogLevel::Off:   return "off";
    }
    return "info";
}

}  // namespace overlaydesk
