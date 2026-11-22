#include "infrastructure/persistence/SqliteProductRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace vamp9::pos::infrastructure {

namespace {

[[nodiscard]] domain::Product mapRowToProduct(const QSqlQuery& query) {
    domain::Product product;
    product.id = query.value(QStringLiteral("id")).toLongLong();
    product.sku = query.value(QStringLiteral("sku")).toString().toStdString();
    product.barcode = query.value(QStringLiteral("barcode")).toString().toStdString();
    product.name = query.value(QStringLiteral("name")).toString().toStdString();
    product.category = query.value(QStringLiteral("category")).toString().toStdString();
    product.unitPriceCents = query.value(QStringLiteral("unit_price_cents")).toLongLong();
    product.taxRateBps = query.value(QStringLiteral("tax_rate_bps")).toInt();
    product.stockQuantity = query.value(QStringLiteral("stock_quantity")).toInt();
    product.isActive = query.value(QStringLiteral("is_active")).toInt() == 1;
    product.createdAtIso = query.value(QStringLiteral("created_at")).toString().toStdString();
    product.updatedAtIso = query.value(QStringLiteral("updated_at")).toString().toStdString();
    return product;
}

} // namespace

SqliteProductRepository::SqliteProductRepository(std::shared_ptr<DatabaseManager> dbManager)
    : dbManager_(std::move(dbManager)) {}

domain::Result<std::optional<domain::Product>> SqliteProductRepository::findById(std::int64_t id) const {
    if (!dbManager_ || !dbManager_->isOpen()) {
        return domain::Result<std::optional<domain::Product>>::fail(
            domain::ErrorCode::DatabaseError,
            "Database connection is not open."
        );
    }

    QSqlQuery query(dbManager_->connection());
    query.prepare(QStringLiteral(
        "SELECT id, sku, barcode, name, category, unit_price_cents, tax_rate_bps, "
        "stock_quantity, is_active, created_at, updated_at "
        "FROM products WHERE id = :id LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":id"), static_cast<qlonglong>(id));

    if (!query.exec()) {
        return domain::Result<std::optional<domain::Product>>::fail(
            domain::ErrorCode::DatabaseError,
            query.lastError().text().toStdString()
        );
    }

    if (!query.next()) {
        return domain::Result<std::optional<domain::Product>>::ok(std::nullopt);
    }

    return domain::Result<std::optional<domain::Product>>::ok(mapRowToProduct(query));
}

domain::Result<std::optional<domain::Product>> SqliteProductRepository::findByBarcode(std::string_view barcode) const {
    if (!dbManager_ || !dbManager_->isOpen()) {
        return domain::Result<std::optional<domain::Product>>::fail(
            domain::ErrorCode::DatabaseError,
            "Database connection is not open."
        );
    }

    QSqlQuery query(dbManager_->connection());
    query.prepare(QStringLiteral(
        "SELECT id, sku, barcode, name, category, unit_price_cents, tax_rate_bps, "
        "stock_quantity, is_active, created_at, updated_at "
        "FROM products WHERE barcode = :barcode AND is_active = 1 LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":barcode"), QString::fromUtf8(barcode.data(), static_cast<qsizetype>(barcode.size())));

    if (!query.exec()) {
        return domain::Result<std::optional<domain::Product>>::fail(
            domain::ErrorCode::DatabaseError,
            query.lastError().text().toStdString()
        );
    }

    if (!query.next()) {
        return domain::Result<std::optional<domain::Product>>::ok(std::nullopt);
    }

    return domain::Result<std::optional<domain::Product>>::ok(mapRowToProduct(query));
}

domain::Result<std::optional<domain::Product>> SqliteProductRepository::findBySku(std::string_view sku) const {
    if (!dbManager_ || !dbManager_->isOpen()) {
        return domain::Result<std::optional<domain::Product>>::fail(
            domain::ErrorCode::DatabaseError,
            "Database connection is not open."
        );
    }

    QSqlQuery query(dbManager_->connection());
    query.prepare(QStringLiteral(
        "SELECT id, sku, barcode, name, category, unit_price_cents, tax_rate_bps, "
        "stock_quantity, is_active, created_at, updated_at "
        "FROM products WHERE sku = :sku LIMIT 1;"
    ));
    query.bindValue(QStringLiteral(":sku"), QString::fromUtf8(sku.data(), static_cast<qsizetype>(sku.size())));

    if (!query.exec()) {
        return domain::Result<std::optional<domain::Product>>::fail(
            domain::ErrorCode::DatabaseError,
            query.lastError().text().toStdString()
        );
    }

    if (!query.next()) {
        return domain::Result<std::optional<domain::Product>>::ok(std::nullopt);
    }

    return domain::Result<std::optional<domain::Product>>::ok(mapRowToProduct(query));
}

domain::Result<std::vector<domain::Product>> SqliteProductRepository::listActive() const {
    if (!dbManager_ || !dbManager_->isOpen()) {
        return domain::Result<std::vector<domain::Product>>::fail(
            domain::ErrorCode::DatabaseError,
            "Database connection is not open."
        );
    }

    QSqlQuery query(dbManager_->connection());
    if (!query.exec(QStringLiteral(
            "SELECT id, sku, barcode, name, category, unit_price_cents, tax_rate_bps, "
            "stock_quantity, is_active, created_at, updated_at "
            "FROM products WHERE is_active = 1 ORDER BY name ASC;"))) {
        return domain::Result<std::vector<domain::Product>>::fail(
            domain::ErrorCode::DatabaseError,
            query.lastError().text().toStdString()
        );
    }

    std::vector<domain::Product> results;
    while (query.next()) {
        results.push_back(mapRowToProduct(query));
    }
    return domain::Result<std::vector<domain::Product>>::ok(std::move(results));
}

domain::Result<std::int64_t> SqliteProductRepository::save(const domain::Product& product) {
    if (!dbManager_ || !dbManager_->isOpen()) {
        return domain::Result<std::int64_t>::fail(
            domain::ErrorCode::DatabaseError,
            "Database connection is not open."
        );
    }

    QSqlQuery query(dbManager_->connection());
    query.prepare(QStringLiteral(
        "INSERT INTO products (sku, barcode, name, category, unit_price_cents, tax_rate_bps, stock_quantity, is_active) "
        "VALUES (:sku, :barcode, :name, :category, :unit_price_cents, :tax_rate_bps, :stock_quantity, :is_active);"
    ));
    query.bindValue(QStringLiteral(":sku"), QString::fromStdString(product.sku));
    query.bindValue(QStringLiteral(":barcode"), product.barcode.empty() ? QVariant() : QString::fromStdString(product.barcode));
    query.bindValue(QStringLiteral(":name"), QString::fromStdString(product.name));
    query.bindValue(QStringLiteral(":category"), QString::fromStdString(product.category));
    query.bindValue(QStringLiteral(":unit_price_cents"), static_cast<qlonglong>(product.unitPriceCents));
    query.bindValue(QStringLiteral(":tax_rate_bps"), product.taxRateBps);
    query.bindValue(QStringLiteral(":stock_quantity"), product.stockQuantity);
    query.bindValue(QStringLiteral(":is_active"), product.isActive ? 1 : 0);

    if (!query.exec()) {
        return domain::Result<std::int64_t>::fail(
            domain::ErrorCode::DatabaseError,
            query.lastError().text().toStdString()
        );
    }

    return domain::Result<std::int64_t>::ok(query.lastInsertId().toLongLong());
}

domain::Result<void> SqliteProductRepository::updateStock(std::int64_t productId, std::int32_t deltaQuantity) {
    if (!dbManager_ || !dbManager_->isOpen()) {
        return domain::Result<void>::fail(
            domain::ErrorCode::DatabaseError,
            "Database connection is not open."
        );
    }

    QSqlQuery query(dbManager_->connection());
    query.prepare(QStringLiteral(
        "UPDATE products "
        "SET stock_quantity = stock_quantity + :delta, "
        "    updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') "
        "WHERE id = :id;"
    ));
    query.bindValue(QStringLiteral(":delta"), deltaQuantity);
    query.bindValue(QStringLiteral(":id"), static_cast<qlonglong>(productId));

    if (!query.exec()) {
        return domain::Result<void>::fail(
            domain::ErrorCode::DatabaseError,
            query.lastError().text().toStdString()
        );
    }

    if (query.numRowsAffected() == 0) {
        return domain::Result<void>::fail(
            domain::ErrorCode::NotFound,
            "Product not found for stock update."
        );
    }

    return domain::Result<void>::ok();
}

} // namespace vamp9::pos::infrastructure
