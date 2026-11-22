#pragma once

#include "domain/entities/Sale.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace vamp9::pos::domain {

class ISaleRepository {
public:
    virtual ~ISaleRepository() = default;

    [[nodiscard]] virtual Result<std::int64_t> save(const Sale& sale) = 0;
    [[nodiscard]] virtual Result<std::optional<Sale>> findById(std::int64_t id) const = 0;
    [[nodiscard]] virtual Result<std::optional<Sale>> findByReceiptNumber(std::string_view receiptNumber) const = 0;
    [[nodiscard]] virtual Result<std::vector<Sale>> listPendingSync(std::size_t limit) const = 0;
    [[nodiscard]] virtual Result<void> markAsSynced(std::int64_t saleId) = 0;
};

} // namespace vamp9::pos::domain
