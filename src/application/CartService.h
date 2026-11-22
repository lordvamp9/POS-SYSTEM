#pragma once

#include "domain/entities/Product.h"
#include "domain/entities/Sale.h"
#include "domain/interfaces/IProductRepository.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace vamp9::pos::application {

struct CartTotals {
    domain::MoneyCents subtotalCents{0};
    domain::MoneyCents taxTotalCents{0};
    domain::MoneyCents discountTotalCents{0};
    domain::MoneyCents grandTotalCents{0};
};

// Manages the active POS ticket/cart state and deterministic integer tax calculations.
// Intentionally minimal scaffolding: ready to expand with pricing rules, promotions & checkout.
class CartService {
public:
    explicit CartService(std::shared_ptr<domain::IProductRepository> productRepository);

    [[nodiscard]] domain::Result<void> addProduct(const domain::Product& product, std::int32_t quantity = 1);
    [[nodiscard]] domain::Result<void> addByBarcode(std::string_view barcode, std::int32_t quantity = 1);
    [[nodiscard]] domain::Result<void> removeProduct(std::int64_t productId);
    void clear() noexcept;

    [[nodiscard]] const std::vector<domain::SaleItem>& items() const noexcept;
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] CartTotals calculateTotals() const noexcept;

private:
    std::shared_ptr<domain::IProductRepository> productRepository_;
    std::vector<domain::SaleItem> items_;
};

} // namespace vamp9::pos::application
