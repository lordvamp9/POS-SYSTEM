#pragma once

#include <QFrame>
#include <QPixmap>
#include <QString>

#include <cstdint>

class QEnterEvent;
class QEvent;
class QLabel;
class QMouseEvent;
class QResizeEvent;

namespace vamp9::pos::presentation {

struct CatalogProduct {
    std::int64_t id{0};
    QString sku;
    QString barcode;
    QString name;
    QString category;
    std::int64_t unitPriceClp{0};
    bool isWeighted{false};
    QString imageFile; // Filename inside assets/products/ (e.g. "aceite.png")
};

// Commercial supermarket product card (Odoo POS / GK Software style).
// Pure white surface (#FFFFFF), crisp #E5E7EB border, 65% real product photo zone
// dynamically loaded from assets/products/ (with clean photographic placeholder if missing),
// discrete "kg" badge for bulk items, and 35% typography zone with CLP pricing.
class ProductCardWidget final : public QFrame {
    Q_OBJECT

public:
    explicit ProductCardWidget(const CatalogProduct& product, QWidget* parent = nullptr);
    ~ProductCardWidget() override = default;

    [[nodiscard]] const CatalogProduct& product() const noexcept;

signals:
    void clicked(const CatalogProduct& product);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void updatePhotoDisplay();
    [[nodiscard]] QString resolveProductImagePath() const;
    [[nodiscard]] QPixmap renderPlaceholderPixmap(const QSize& logicalSize) const;

    CatalogProduct product_;
    QPixmap sourcePhoto_;
    QLabel* photoLabel_{nullptr};
    bool pressed_{false};
};

} // namespace vamp9::pos::presentation
