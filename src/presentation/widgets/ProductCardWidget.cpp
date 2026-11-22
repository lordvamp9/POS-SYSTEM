#include "presentation/widgets/ProductCardWidget.h"

#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/CurrencyFormatter.h"

#include <QCoreApplication>
#include <QDir>
#include <QEnterEvent>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace vamp9::pos::presentation {

ProductCardWidget::ProductCardWidget(const CatalogProduct& product, QWidget* parent)
    : QFrame(parent)
    , product_(product) {
    setObjectName(QStringLiteral("productCard"));
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(168);
    setMaximumHeight(180);

    auto tr = &AppPreferences::trText;
    setToolTip(
        QStringLiteral("%1\nSKU: %2 · %3: %4\n%5: %6")
            .arg(product_.name)
            .arg(product_.sku)
            .arg(tr(QStringLiteral("Barcode"), QStringLiteral("Código")))
            .arg(product_.barcode)
            .arg(tr(QStringLiteral("Price"), QStringLiteral("Precio")))
            .arg(CurrencyFormatter::formatUnitPrice(product_.unitPriceClp, product_.isWeighted))
    );

    const QString resolvedPath = resolveProductImagePath();
    if (!resolvedPath.isEmpty()) {
        sourcePhoto_.load(resolvedPath);
    }

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(8.0);
    shadow->setOffset(0.0, 2.0);
    shadow->setColor(QColor(15, 23, 42, 10));
    setGraphicsEffect(shadow);

    setupUi();
    updatePhotoDisplay();
}

const CatalogProduct& ProductCardWidget::product() const noexcept {
    return product_;
}

void ProductCardWidget::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 8);
    rootLayout->setSpacing(4);

    // ------------------------------------------------------------------------
    // Top 65%: Clean White Photo Zone (8px inner padding) + Optional "kg" Badge
    // ------------------------------------------------------------------------
    auto* photoZone = new QWidget(this);
    photoZone->setObjectName(QStringLiteral("productPhotoZone"));
    photoZone->setAttribute(Qt::WA_TransparentForMouseEvents);
    photoZone->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* photoGrid = new QGridLayout(photoZone);
    photoGrid->setContentsMargins(8, 8, 8, 4);
    photoGrid->setSpacing(0);

    photoLabel_ = new QLabel(photoZone);
    photoLabel_->setObjectName(QStringLiteral("productPhotoLabel"));
    photoLabel_->setAlignment(Qt::AlignCenter);
    photoLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    photoLabel_->setMinimumHeight(92);
    photoGrid->addWidget(photoLabel_, 0, 0);

    if (product_.isWeighted) {
        auto* kgBadge = new QLabel(QStringLiteral("kg"), photoZone);
        kgBadge->setObjectName(QStringLiteral("weightBadge"));
        kgBadge->setAlignment(Qt::AlignCenter);
        photoGrid->addWidget(kgBadge, 0, 0, Qt::AlignTop | Qt::AlignRight);
    }

    rootLayout->addWidget(photoZone, 65);

    // ------------------------------------------------------------------------
    // Bottom 35%: Title (11.5px, #1F2937, max 2 lines) + Corporate Blue CLP Price
    // ------------------------------------------------------------------------
    auto* textZone = new QWidget(this);
    textZone->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* textLayout = new QVBoxLayout(textZone);
    textLayout->setContentsMargins(8, 0, 8, 2);
    textLayout->setSpacing(2);

    auto* nameLabel = new QLabel(product_.name, textZone);
    nameLabel->setObjectName(QStringLiteral("productCardName"));
    nameLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    nameLabel->setWordWrap(true);
    nameLabel->setMinimumHeight(30);
    nameLabel->setMaximumHeight(32);

    auto* priceLabel = new QLabel(
        CurrencyFormatter::formatUnitPrice(product_.unitPriceClp, product_.isWeighted),
        textZone
    );
    priceLabel->setObjectName(QStringLiteral("productCardPrice"));
    priceLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    textLayout->addWidget(nameLabel);
    textLayout->addWidget(priceLabel);

    rootLayout->addWidget(textZone, 35);
}

QString ProductCardWidget::resolveProductImagePath() const {
    if (product_.imageFile.isEmpty()) {
        return {};
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString userDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QStringList candidates = {
        QDir(userDataDir).filePath(QStringLiteral("products/") + product_.imageFile),
        QDir(appDir).filePath(QStringLiteral("assets/products/") + product_.imageFile),
        QDir::current().filePath(QStringLiteral("assets/products/") + product_.imageFile),
        QStringLiteral(":/products/") + product_.imageFile
    };

    for (const QString& path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    return {};
}

void ProductCardWidget::updatePhotoDisplay() {
    if (!photoLabel_) {
        return;
    }

    const int availW = std::max(84, photoLabel_->width() > 16 ? photoLabel_->width() - 4 : 112);
    const int availH = std::max(78, photoLabel_->height() > 16 ? photoLabel_->height() - 4 : 92);
    const QSize targetSize(availW, availH);

    if (!sourcePhoto_.isNull()) {
        const qreal dpr = devicePixelRatioF() > 0.0 ? devicePixelRatioF() : 1.0;
        QPixmap scaled = sourcePhoto_.scaled(
            targetSize * dpr,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        scaled.setDevicePixelRatio(dpr);
        photoLabel_->setPixmap(scaled);
    } else {
        photoLabel_->setPixmap(renderPlaceholderPixmap(targetSize));
    }
}

QPixmap ProductCardWidget::renderPlaceholderPixmap(const QSize& logicalSize) const {
    const qreal dpr = 2.0;
    QPixmap pix(logicalSize * dpr);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(dpr, dpr);

    const QRectF box(1.0, 1.0, logicalSize.width() - 2.0, logicalSize.height() - 2.0);
    QPainterPath framePath;
    framePath.addRoundedRect(box, 8.0, 8.0);

    // Clean light-gray photographic placeholder (no loud pastel circles)
    painter.fillPath(framePath, QColor(248, 250, 252)); // #F8FAFC
    QPen dashPen(QColor(226, 232, 240), 1.0);           // #E2E8F0
    painter.setPen(dashPen);
    painter.drawPath(framePath);

    // Minimalist camera/image wireframe outline in soft slate gray
    const qreal cx = box.center().x();
    const qreal cy = box.center().y() - 8.0;
    const QRectF iconRect(cx - 14.0, cy - 10.0, 28.0, 20.0);
    painter.setPen(QPen(QColor(148, 163, 184), 1.4)); // #94A3B8
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(iconRect, 3.5, 3.5);
    painter.drawEllipse(QPointF(cx - 6.0, cy - 3.5), 2.2, 2.2);

    QPainterPath hills;
    hills.moveTo(iconRect.left() + 3.0, iconRect.bottom() - 3.0);
    hills.lineTo(iconRect.left() + 10.0, iconRect.bottom() - 9.5);
    hills.lineTo(iconRect.left() + 15.5, iconRect.bottom() - 5.0);
    hills.lineTo(iconRect.left() + 20.5, iconRect.bottom() - 10.5);
    hills.lineTo(iconRect.right() - 3.0, iconRect.bottom() - 3.0);
    painter.drawPath(hills);

    // Expected filename label (e.g. "aceite.png") so developer knows exact drop-in asset name
    QFont labelFont(QStringLiteral("Segoe UI"));
    labelFont.setPixelSize(10);
    labelFont.setWeight(QFont::Medium);
    painter.setFont(labelFont);
    painter.setPen(QColor(148, 163, 184));

    const QRectF textRect(box.left() + 6.0, box.bottom() - 22.0, box.width() - 12.0, 16.0);
    const QFontMetrics fm(labelFont);
    const QString fileLabel = fm.elidedText(
        product_.imageFile.isEmpty() ? QStringLiteral("sin_imagen.png") : product_.imageFile,
        Qt::ElideMiddle,
        static_cast<int>(textRect.width())
    );
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, fileLabel);

    painter.end();
    pix.setDevicePixelRatio(dpr);
    return pix;
}

void ProductCardWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        pressed_ = true;
        setProperty("pressed", true);
        style()->unpolish(this);
        style()->polish(this);
    }
    QFrame::mousePressEvent(event);
}

void ProductCardWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (pressed_ && event->button() == Qt::LeftButton) {
        pressed_ = false;
        setProperty("pressed", false);
        style()->unpolish(this);
        style()->polish(this);
        if (rect().contains(event->pos())) {
            emit clicked(product_);
        }
    }
    QFrame::mouseReleaseEvent(event);
}

void ProductCardWidget::enterEvent(QEnterEvent* event) {
    if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect())) {
        shadow->setBlurRadius(18.0);
        shadow->setOffset(0.0, 5.0);
        shadow->setColor(QColor(15, 23, 42, 26));
    }
    QFrame::enterEvent(event);
}

void ProductCardWidget::leaveEvent(QEvent* event) {
    pressed_ = false;
    setProperty("pressed", false);
    style()->unpolish(this);
    style()->polish(this);
    if (auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect())) {
        shadow->setBlurRadius(8.0);
        shadow->setOffset(0.0, 2.0);
        shadow->setColor(QColor(15, 23, 42, 10));
    }
    QFrame::leaveEvent(event);
}

void ProductCardWidget::resizeEvent(QResizeEvent* event) {
    QFrame::resizeEvent(event);
    updatePhotoDisplay();
}

} // namespace vamp9::pos::presentation
