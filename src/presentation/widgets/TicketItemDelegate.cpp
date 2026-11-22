#include "presentation/widgets/TicketItemDelegate.h"

#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/CurrencyFormatter.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace vamp9::pos::presentation {

std::int64_t TicketLineItem::lineTotalClp() const noexcept {
    return CurrencyFormatter::calculateLineTotalClp(quantity, unitPriceClp, discountPercent);
}

TicketItemDelegate::TicketItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void TicketItemDelegate::paint(
    QPainter* painter,
    const QStyleOptionViewItem& option,
    const QModelIndex& index
) const {
    if (!painter || !index.isValid()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool dark = AppPreferences::instance().isDark();
    const bool english = AppPreferences::instance().isEnglish();

    const QRect itemRect = option.rect;
    const QRect cardRect = itemRect.adjusted(4, 2, -4, -2);
    const bool isSelected = option.state.testFlag(QStyle::State_Selected);
    const bool isHovered = option.state.testFlag(QStyle::State_MouseOver);

    // 1. Row background (soft corporate blue #EFF6FF selection highlight in Light mode)
    if (isSelected) {
        QPainterPath path;
        path.addRoundedRect(cardRect, 8.0, 8.0);
        painter->fillPath(path, dark ? QColor(30, 58, 138, 128) : QColor(239, 246, 255));
        painter->setPen(QPen(dark ? QColor(59, 130, 246) : QColor(191, 219, 254), 1.0));
        painter->drawPath(path);

        QPainterPath accentBar;
        accentBar.addRoundedRect(QRectF(cardRect.left() + 1.0, cardRect.top() + 7.0, 3.0, cardRect.height() - 14.0), 1.5, 1.5);
        painter->fillPath(accentBar, dark ? QColor(96, 165, 250) : QColor(37, 99, 235));
    } else if (isHovered) {
        QPainterPath path;
        path.addRoundedRect(cardRect, 8.0, 8.0);
        painter->fillPath(path, dark ? QColor(30, 41, 59) : QColor(248, 250, 252));
    }

    if (!isSelected) {
        painter->setPen(QPen(dark ? QColor(30, 41, 59) : QColor(241, 245, 249), 1.0));
        painter->drawLine(
            cardRect.left() + 10,
            itemRect.bottom(),
            cardRect.right() - 10,
            itemRect.bottom()
        );
    }

    // 2. Extract line item values
    const QString name = index.data(TicketNameRole).toString();
    const double quantity = index.data(TicketQuantityRole).toDouble();
    const bool isWeighted = index.data(TicketIsWeightedRole).toBool();
    const std::int64_t unitPriceClp = index.data(TicketUnitPriceClpRole).toLongLong();
    const double discountPct = index.data(TicketDiscountPercentRole).toDouble();
    const std::int64_t lineTotalClp = index.data(TicketLineTotalClpRole).toLongLong();
    const QString note = index.data(TicketNoteRole).toString().trimmed();

    const QRect contentRect = cardRect.adjusted(12, 7, -12, -7);
    const int topHeight = contentRect.height() / 2 + 2;
    const QRect topRect(contentRect.left(), contentRect.top(), contentRect.width(), topHeight);
    const QRect bottomRect(contentRect.left(), contentRect.top() + topHeight, contentRect.width(), contentRect.height() - topHeight);

    // 3. Top line: Product Name (bold, left) | Subtotal CLP (bold, right)
    QFont titleFont = option.font;
    titleFont.setPixelSize(13);
    titleFont.setWeight(QFont::DemiBold);

    QFont priceFont = option.font;
    priceFont.setPixelSize(13);
    priceFont.setWeight(QFont::Bold);

    const QString totalText = CurrencyFormatter::formatClp(lineTotalClp);
    const QFontMetrics priceMetrics(priceFont);
    const int priceWidth = priceMetrics.horizontalAdvance(totalText) + 12;

    const QRect priceRect(topRect.right() - priceWidth, topRect.top(), priceWidth, topRect.height());
    const QRect nameRect(topRect.left(), topRect.top(), topRect.width() - priceWidth - 6, topRect.height());

    const QColor primaryText = dark ? QColor(248, 250, 252) : QColor(15, 23, 42);
    painter->setFont(titleFont);
    painter->setPen(primaryText);
    const QFontMetrics titleMetrics(titleFont);
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, titleMetrics.elidedText(name, Qt::ElideRight, nameRect.width()));

    painter->setFont(priceFont);
    painter->setPen(lineTotalClp < 0 ? QColor(248, 113, 113) : primaryText);
    painter->drawText(priceRect, Qt::AlignRight | Qt::AlignVCenter, totalText);

    // 4. Bottom line: quantity x unit price + discount/note tags
    QFont detailFont = option.font;
    detailFont.setPixelSize(11);
    detailFont.setWeight(QFont::Normal);
    painter->setFont(detailFont);

    QString qtyStr = CurrencyFormatter::formatQuantity(quantity, isWeighted);
    if (english && !isWeighted) {
        qtyStr.replace(QStringLiteral("Unid."), QStringLiteral("Unit"));
    }

    QString detailText = QStringLiteral("%1 x %2")
        .arg(qtyStr)
        .arg(CurrencyFormatter::formatUnitPrice(unitPriceClp, isWeighted));

    if (discountPct > 0.001) {
        detailText.append(QStringLiteral("  ·  -%1% %2")
            .arg(QString::number(discountPct, 'f', 0))
            .arg(english ? QStringLiteral("Off") : QStringLiteral("Desc.")));
    }
    if (!note.isEmpty()) {
        detailText.append(QStringLiteral("  ·  %1").arg(note));
    }

    const QColor detailColor = discountPct > 0.001
        ? (dark ? QColor(52, 211, 153) : QColor(5, 150, 105))
        : (dark ? QColor(148, 163, 184) : QColor(100, 116, 139));
    painter->setPen(detailColor);
    const QFontMetrics detailMetrics(detailFont);
    painter->drawText(bottomRect, Qt::AlignLeft | Qt::AlignVCenter, detailMetrics.elidedText(detailText, Qt::ElideRight, bottomRect.width()));

    painter->restore();
}

QSize TicketItemDelegate::sizeHint(
    const QStyleOptionViewItem& option,
    const QModelIndex& index
) const {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(0, 56);
}

} // namespace vamp9::pos::presentation
