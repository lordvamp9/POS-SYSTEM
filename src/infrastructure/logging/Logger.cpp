#include "infrastructure/logging/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace vamp9::pos::infrastructure {

Logger::Logger(std::filesystem::path logFilePath)
    : logFilePath_(std::move(logFilePath)) {
    std::error_code ec;
    if (logFilePath_.has_parent_path()) {
        std::filesystem::create_directories(logFilePath_.parent_path(), ec);
    }
    stream_.open(logFilePath_, std::ios::out | std::ios::app);
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_.is_open()) {
        stream_.flush();
        stream_.close();
    }
}

void Logger::log(LogLevel level, std::string_view message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!stream_.is_open()) {
        return;
    }

    stream_ << '[' << currentUtcTimestamp() << "] "
            << '[' << levelToString(level) << "] "
            << message << '\n';
    stream_.flush();
}

void Logger::info(std::string_view message) {
    log(LogLevel::Info, message);
}

void Logger::warn(std::string_view message) {
    log(LogLevel::Warn, message);
}

void Logger::error(std::string_view message) {
    log(LogLevel::Error, message);
}

bool Logger::isReady() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return stream_.is_open();
}

const char* Logger::levelToString(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

std::string Logger::currentUtcTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm utcTm{};
#ifdef _WIN32
    gmtime_s(&utcTm, &nowTime);
#else
    gmtime_r(&nowTime, &utcTm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&utcTm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace vamp9::pos::infrastructure
