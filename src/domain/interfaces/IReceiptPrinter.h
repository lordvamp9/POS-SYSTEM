#pragma once

#include "domain/entities/Sale.h"

#include <string>

namespace vamp9::pos::domain {

struct PrinterStatus {
    bool isOnline{false};
    bool hasPaper{true};
    std::string deviceName;
};

// Hardware abstraction for ESC/POS thermal receipt printers (or simulated mocks).
class IReceiptPrinter {
public:
    virtual ~IReceiptPrinter() = default;

    [[nodiscard]] virtual PrinterStatus status() const = 0;
    [[nodiscard]] virtual Result<void> printReceipt(const Sale& sale) = 0;
    [[nodiscard]] virtual Result<void> openCashDrawer() = 0;
};

} // namespace vamp9::pos::domain
