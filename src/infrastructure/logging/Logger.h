#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace vamp9::pos::infrastructure {

enum class LogLevel {
    Info,
    Warn,
    Error
};

// Thread-safe RAII local file logger with INFO, WARN, and ERROR severity levels.
class Logger {
public:
    explicit Logger(std::filesystem::path logFilePath);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void log(LogLevel level, std::string_view message);
    void info(std::string_view message);
    void warn(std::string_view message);
    void error(std::string_view message);

    [[nodiscard]] bool isReady() const noexcept;

private:
    [[nodiscard]] static const char* levelToString(LogLevel level) noexcept;
    [[nodiscard]] static std::string currentUtcTimestamp();

    std::filesystem::path logFilePath_;
    std::ofstream stream_;
    mutable std::mutex mutex_;
};

} // namespace vamp9::pos::infrastructure
