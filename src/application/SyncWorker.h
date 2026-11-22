#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

namespace vamp9::pos::application {

// Background worker stub implementing the Offline-First Outbox synchronization loop.
// Uses C++20 std::jthread + std::stop_token for safe RAII shutdown without dangling threads.
class SyncWorker {
public:
    using LogCallback = std::function<void(const std::string&)>;

    explicit SyncWorker(
        std::chrono::milliseconds pollInterval = std::chrono::seconds(15),
        LogCallback logCallback = {}
    );
    ~SyncWorker();

    SyncWorker(const SyncWorker&) = delete;
    SyncWorker& operator=(const SyncWorker&) = delete;
    SyncWorker(SyncWorker&&) = delete;
    SyncWorker& operator=(SyncWorker&&) = delete;

    void start();
    void stop();
    void triggerSyncNow();

    [[nodiscard]] bool isRunning() const noexcept;

private:
    void runLoop(std::stop_token stopToken);
    void processOutboxBatch();

    std::chrono::milliseconds pollInterval_;
    LogCallback logCallback_;
    std::atomic<bool> running_{false};
    std::mutex wakeMutex_;
    std::condition_variable_any wakeCv_;
    std::jthread workerThread_;
};

} // namespace vamp9::pos::application
