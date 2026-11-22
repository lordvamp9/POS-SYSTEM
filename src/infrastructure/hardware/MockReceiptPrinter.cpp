#include "infrastructure/hardware/MockReceiptPrinter.h"

#include <utility>

namespace vamp9::pos::infrastructure {

MockReceiptPrinter::MockReceiptPrinter(std::shared_ptr<Logger> logger, std::string deviceName)
    : logger_(std::move(logger))
    , deviceName_(std::move(deviceName)) {}

domain::PrinterStatus MockReceiptPrinter::status() const {
    return domain::PrinterStatus{
        .isOnline = true,
        .hasPaper = true,
        .deviceName = deviceName_
    };
}

domain::Result<void> MockReceiptPrinter::printReceipt(const domain::Sale& sale) {
    if (logger_) {
        logger_->info(
            "MockReceiptPrinter: Printing receipt #" + sale.receiptNumber +
            " | items=" + std::to_string(sale.items.size()) +
            " | grandTotalCents=" + std::to_string(sale.grandTotalCents)
        );
    }
    return domain::Result<void>::ok();
}

domain::Result<void> MockReceiptPrinter::openCashDrawer() {
    if (logger_) {
        logger_->info("MockReceiptPrinter: Cash drawer pulse command sent (simulated).");
    }
    return domain::Result<void>::ok();
}

} // namespace vamp9::pos::infrastructure
