#pragma once

#include <QMetaType>
#include <QString>
#include <QStyledItemDelegate>

#include <cstdint>

namespace vamp9::pos::presentation {

struct TicketLineItem {
    std::int64_t productId{0};
    QString sku;
    QString barcode;
    QString name;
    QString category;
    double quantity{1.0};
    bool isWeighted{false};
    std::int64_t unitPriceClp{0};
    double discountPercent{0.0};
    QString note;

    [[nodiscard]] std::int64_t lineTotalClp() const noexcept;
};

enum TicketItemRole : int {
    TicketNameRole = Qt::UserRole + 100,
    TicketQuantityRole,
    TicketIsWeightedRole,
    TicketUnitPriceClpRole,
    TicketDiscountPercentRole,
    TicketLineTotalClpRole,
    TicketNoteRole
};

// Custom high-density supermarket POS ticket delegate.
// Renders two lines per row:
//   Top line:    Product name (bold, left)  |  CLP subtotal "$ 2.500" (bold, right)
//   Bottom line: "1,000 Unid. x $ 2.500" or "0,500 kg x $ 1.990/kg" + optional discount/note tags
// Selected row renders with a soft translucent sky/mint highlight and left accent bar.
class TicketItemDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit TicketItemDelegate(QObject* parent = nullptr);
    ~TicketItemDelegate() override = default;

    void paint(
        QPainter* painter,
        const QStyleOptionViewItem& option,
        const QModelIndex& index
    ) const override;

    [[nodiscard]] QSize sizeHint(
        const QStyleOptionViewItem& option,
        const QModelIndex& index
    ) const override;
};

} // namespace vamp9::pos::presentation

Q_DECLARE_METATYPE(vamp9::pos::presentation::TicketLineItem)
