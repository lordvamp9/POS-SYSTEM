#pragma once

#include "domain/entities/Product.h"
#include "domain/interfaces/IProductRepository.h"
#include "presentation/widgets/ProductCardWidget.h"

#include <QWidget>

#include <memory>
#include <vector>

class QLineEdit;
class QPushButton;
class QTableWidget;

namespace vamp9::pos::presentation {

class InventoryView final : public QWidget {
    Q_OBJECT

public:
    explicit InventoryView(
        std::shared_ptr<domain::IProductRepository> productRepository,
        QWidget* parent = nullptr
    );
    ~InventoryView() override = default;

public slots:
    void reloadProducts();
    void retranslateUi();

signals:
    void productCreated(const CatalogProduct& product);

private:
    void setupUi();
    void renderTableRows(const QString& filterText);
    void handleAddNewProduct();

    std::shared_ptr<domain::IProductRepository> productRepository_;
    std::vector<domain::Product> cachedProducts_;

    QLineEdit* searchInput_{nullptr};
    QPushButton* refreshBtn_{nullptr};
    QPushButton* newProductBtn_{nullptr};
    QTableWidget* productsTable_{nullptr};
};

} // namespace vamp9::pos::presentation
