#include "application/CartService.h"

#include <algorithm>
#include <utility>

namespace vamp9::pos::application {

CartService::CartService(std::shared_ptr<domain::IProductRepository> productRepository)
    : productRepository_(std::move(productRepository)) {}

domain::Result<void> CartService::addProduct(const domain::Product& product, std::int32_t quantity) {
    if (quantity <= 0) {
        return domain::Result<void>::fail(
            domain::ErrorCode::InvalidArgument,
            "Quantity must be greater than zero."
        );
    }

    // Scaffolding implementation: append or merge line item.
    // Complex pricing tiers, tax-inclusive rules, and promotions will be added in future iterations.
    auto it = std::find_if(items_.begin(), items_.end(), [&product](const domain::SaleItem& item) {
        return item.productId == product.id && product.id != 0;
    });

    if (it != items_.end()) {
        it->quantity += quantity;
        const domain::MoneyCents netLine = it->unitPriceCents * it->quantity;
        it->taxAmountCents = (netLine * it->taxRateBps) / 10000;
        it->lineTotalCents = netLine + it->taxAmountCents;
        return domain::Result<void>::ok();
    }

    domain::SaleItem line;
    line.productId = product.id;
    line.skuSnapshot = product.sku;
    line.productNameSnapshot = product.name;
    line.quantity = quantity;
    line.unitPriceCents = product.unitPriceCents;
    line.taxRateBps = product.taxRateBps;

    const domain::MoneyCents netLine = line.unitPriceCents * line.quantity;
    line.taxAmountCents = (netLine * line.taxRateBps) / 10000;
    line.lineTotalCents = netLine + line.taxAmountCents;

    items_.push_back(std::move(line));
    return domain::Result<void>::ok();
}

domain::Result<void> CartService::addByBarcode(std::string_view barcode, std::int32_t quantity) {
    if (!productRepository_) {
        return domain::Result<void>::fail(
            domain::ErrorCode::InvalidArgument,
            "Product repository is not configured."
        );
    }

    auto lookup = productRepository_->findByBarcode(barcode);
    if (!lookup) {
        return domain::Result<void>::fail(lookup.error());
    }

    if (!lookup.value().has_value()) {
        return domain::Result<void>::fail(
            domain::ErrorCode::NotFound,
            "No product found for the scanned barcode."
        );
    }

    return addProduct(*lookup.value(), quantity);
}

domain::Result<void> CartService::removeProduct(std::int64_t productId) {
    const auto oldSize = items_.size();
    std::erase_if(items_, [productId](const domain::SaleItem& item) {
        return item.productId == productId;
    });

    if (items_.size() == oldSize) {
        return domain::Result<void>::fail(
            domain::ErrorCode::NotFound,
            "Product is not present in the active cart."
        );
    }

    return domain::Result<void>::ok();
}

void CartService::clear() noexcept {
    items_.clear();
}

const std::vector<domain::SaleItem>& CartService::items() const noexcept {
    return items_;
}

bool CartService::isEmpty() const noexcept {
    return items_.empty();
}

CartTotals CartService::calculateTotals() const noexcept {
    CartTotals totals;
    for (const auto& item : items_) {
        totals.subtotalCents += (item.unitPriceCents * item.quantity);
        totals.taxTotalCents += item.taxAmountCents;
    }
    totals.grandTotalCents = totals.subtotalCents + totals.taxTotalCents - totals.discountTotalCents;
    return totals;
}

} // namespace vamp9::pos::application
