#pragma once

#include "domain/interfaces/IProductRepository.h"
#include "infrastructure/persistence/DatabaseManager.h"

#include <memory>

namespace vamp9::pos::infrastructure {

// Concrete SQLite3 repository implementing domain::IProductRepository.
class SqliteProductRepository final : public domain::IProductRepository {
public:
    explicit SqliteProductRepository(std::shared_ptr<DatabaseManager> dbManager);

    [[nodiscard]] domain::Result<std::optional<domain::Product>> findById(std::int64_t id) const override;
    [[nodiscard]] domain::Result<std::optional<domain::Product>> findByBarcode(std::string_view barcode) const override;
    [[nodiscard]] domain::Result<std::optional<domain::Product>> findBySku(std::string_view sku) const override;
    [[nodiscard]] domain::Result<std::vector<domain::Product>> listActive() const override;

    [[nodiscard]] domain::Result<std::int64_t> save(const domain::Product& product) override;
    [[nodiscard]] domain::Result<void> updateStock(std::int64_t productId, std::int32_t deltaQuantity) override;

private:
    std::shared_ptr<DatabaseManager> dbManager_;
};

} // namespace vamp9::pos::infrastructure
