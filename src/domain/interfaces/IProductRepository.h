#pragma once

#include "domain/entities/Product.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace vamp9::pos::domain {

class IProductRepository {
public:
    virtual ~IProductRepository() = default;

    [[nodiscard]] virtual Result<std::optional<Product>> findById(std::int64_t id) const = 0;
    [[nodiscard]] virtual Result<std::optional<Product>> findByBarcode(std::string_view barcode) const = 0;
    [[nodiscard]] virtual Result<std::optional<Product>> findBySku(std::string_view sku) const = 0;
    [[nodiscard]] virtual Result<std::vector<Product>> listActive() const = 0;

    [[nodiscard]] virtual Result<std::int64_t> save(const Product& product) = 0;
    [[nodiscard]] virtual Result<void> updateStock(std::int64_t productId, std::int32_t deltaQuantity) = 0;
};

} // namespace vamp9::pos::domain
