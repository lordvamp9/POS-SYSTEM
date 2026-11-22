#pragma once

#include "domain/entities/Product.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vamp9::pos::domain {

enum class PaymentMethod {
    Cash,
    Card,
    Transfer,
    Mixed
};

enum class SaleStatus {
    Open,
    Completed,
    Voided,
    Refunded
};

enum class SyncStatus {
    Pending,
    Synced,
    Failed
};

struct SaleItem {
    std::int64_t id{0};
    std::int64_t productId{0};
    std::string skuSnapshot;
    std::string productNameSnapshot;
    std::int32_t quantity{1};
    MoneyCents unitPriceCents{0};
    TaxRateBps taxRateBps{0};
    MoneyCents taxAmountCents{0};
    MoneyCents lineTotalCents{0};
};

struct Sale {
    std::int64_t id{0};
    std::string receiptNumber;
    std::optional<std::int64_t> cashRegisterId;
    std::vector<SaleItem> items;
    MoneyCents subtotalCents{0};
    MoneyCents taxTotalCents{0};
    MoneyCents discountTotalCents{0};
    MoneyCents grandTotalCents{0};
    PaymentMethod paymentMethod{PaymentMethod::Cash};
    SaleStatus status{SaleStatus::Open};
    SyncStatus syncStatus{SyncStatus::Pending};
    std::string createdAtIso;
};

} // namespace vamp9::pos::domain
