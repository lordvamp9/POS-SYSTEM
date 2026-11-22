#pragma once

#include "domain/interfaces/IReceiptPrinter.h"
#include "infrastructure/logging/Logger.h"

#include <memory>
#include <string>

namespace vamp9::pos::infrastructure {

// Simulated ESC/POS printer for development, testing, and headless stations.
class MockReceiptPrinter final : public domain::IReceiptPrinter {
public:
    explicit MockReceiptPrinter(
        std::shared_ptr<Logger> logger,
        std::string deviceName = "Mock ESC/POS Thermal 80mm"
    );

    [[nodiscard]] domain::PrinterStatus status() const override;
    [[nodiscard]] domain::Result<void> printReceipt(const domain::Sale& sale) override;
    [[nodiscard]] domain::Result<void> openCashDrawer() override;

private:
    std::shared_ptr<Logger> logger_;
    std::string deviceName_;
};

} // namespace vamp9::pos::infrastructure
