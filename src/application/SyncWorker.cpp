#include "application/SyncWorker.h"

#include <utility>

namespace vamp9::pos::application {

SyncWorker::SyncWorker(std::chrono::milliseconds pollInterval, LogCallback logCallback)
    : pollInterval_(pollInterval)
    , logCallback_(std::move(logCallback)) {}

SyncWorker::~SyncWorker() {
    stop();
}

void SyncWorker::start() {
    if (running_.exchange(true)) {
        return;
    }

    if (logCallback_) {
        logCallback_("SyncWorker started (Offline-First Outbox stub).");
    }

    workerThread_ = std::jthread([this](std::stop_token stopToken) {
        runLoop(std::move(stopToken));
    });
}

void SyncWorker::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (workerThread_.joinable()) {
        workerThread_.request_stop();
        wakeCv_.notify_all();
        workerThread_.join();
    }

    if (logCallback_) {
        logCallback_("SyncWorker stopped cleanly.");
    }
}

void SyncWorker::triggerSyncNow() {
    wakeCv_.notify_one();
}

bool SyncWorker::isRunning() const noexcept {
    return running_.load();
}

void SyncWorker::runLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        processOutboxBatch();

        std::unique_lock<std::mutex> lock(wakeMutex_);
        wakeCv_.wait_for(lock, stopToken, pollInterval_, [&stopToken]() {
            return stopToken.stop_requested();
        });
    }
}

void SyncWorker::processOutboxBatch() {
    // Stub for future Outbox pattern implementation:
    // 1. Read pending events from `sync_queue` where next_attempt_at <= now.
    // 2. Transmit idempotent payloads to upstream ERP/Cloud API with exponential backoff.
    // 3. Mark rows SYNCED or increment retry_count / move to DEAD_LETTER.
}

} // namespace vamp9::pos::application
