#include "presentation/widgets/ToastManager.h"

#include "presentation/utils/AppPreferences.h"

#include <QEasingCurve>
#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSvgRenderer>
#include <QTimer>

#include <algorithm>

namespace vamp9::pos::presentation {

namespace {

constexpr int kToastWidthPx = 376;
constexpr int kSlideOffsetPx = 12;

} // namespace

// ============================================================================
// ToastNotification
// ============================================================================
ToastNotification::ToastNotification(
    ToastType type,
    const QString& title,
    const QString& message,
    int durationMs,
    QWidget* parent
)
    : QWidget(parent)
    , type_(type)
    , title_(title)
    , message_(message)
    , durationMs_(std::max(1200, durationMs)) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);

    // Measure text height dynamically so multi-line messages never clip
    QFont msgFont(QStringLiteral("Segoe UI"));
    msgFont.setPixelSize(12);
    const QFontMetrics msgFm(msgFont);
    const int textAvailWidth = kToastWidthPx - 4 - 14 - 34 - 12 - 28;
    const QRect msgBounds = msgFm.boundingRect(
        QRect(0, 0, textAvailWidth, 120),
        Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
        message_
    );
    const int calculatedHeight = std::clamp(38 + msgBounds.height() + 14, 72, 114);
    setFixedSize(kToastWidthPx, calculatedHeight);

    closeButtonRect_ = QRect(width() - 28, 10, 18, 18);

    shadowEffect_ = new QGraphicsDropShadowEffect(this);
    shadowEffect_->setBlurRadius(24.0);
    shadowEffect_->setOffset(0.0, 6.0);
    setGraphicsEffect(shadowEffect_);
    updateShadowForFade();

    autoCloseTimer_ = new QTimer(this);
    autoCloseTimer_->setSingleShot(true);
    autoCloseTimer_->setInterval(durationMs_);
    connect(autoCloseTimer_, &QTimer::timeout, this, &ToastNotification::dismiss);
}

qreal ToastNotification::fadeProgress() const noexcept {
    return fadeProgress_;
}

void ToastNotification::setFadeProgress(qreal value) {
    fadeProgress_ = std::clamp(value, 0.0, 1.0);
    updateShadowForFade();
    update();
}

void ToastNotification::presentAt(const QPoint& targetPos) {
    const QPoint startPos(targetPos.x(), targetPos.y() + kSlideOffsetPx);
    move(startPos);
    show();
    raise();

    auto* group = new QParallelAnimationGroup(this);

    auto* posAnim = new QPropertyAnimation(this, "pos", group);
    posAnim->setDuration(240);
    posAnim->setStartValue(startPos);
    posAnim->setEndValue(targetPos);
    posAnim->setEasingCurve(QEasingCurve::OutCubic);

    auto* fadeAnim = new QPropertyAnimation(this, "fadeProgress", group);
    fadeAnim->setDuration(220);
    fadeAnim->setStartValue(0.0);
    fadeAnim->setEndValue(1.0);
    fadeAnim->setEasingCurve(QEasingCurve::OutCubic);

    group->addAnimation(posAnim);
    group->addAnimation(fadeAnim);
    activeAnimation_ = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);

    if (autoCloseTimer_) {
        autoCloseTimer_->start();
    }
}

void ToastNotification::moveToTarget(const QPoint& targetPos) {
    if (dismissing_) {
        return;
    }
    if (activeAnimation_ && activeAnimation_->state() == QAbstractAnimation::Running) {
        activeAnimation_->stop();
        setFadeProgress(1.0);
    }

    auto* anim = new QPropertyAnimation(this, "pos", this);
    anim->setDuration(180);
    anim->setStartValue(pos());
    anim->setEndValue(targetPos);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ToastNotification::dismiss() {
    if (dismissing_) {
        return;
    }
    dismissing_ = true;

    if (autoCloseTimer_) {
        autoCloseTimer_->stop();
    }
    if (activeAnimation_ && activeAnimation_->state() == QAbstractAnimation::Running) {
        activeAnimation_->stop();
    }

    const QPoint startPos = pos();
    const QPoint endPos(startPos.x(), startPos.y() + kSlideOffsetPx);

    auto* group = new QParallelAnimationGroup(this);

    auto* posAnim = new QPropertyAnimation(this, "pos", group);
    posAnim->setDuration(200);
    posAnim->setStartValue(startPos);
    posAnim->setEndValue(endPos);
    posAnim->setEasingCurve(QEasingCurve::InCubic);

    auto* fadeAnim = new QPropertyAnimation(this, "fadeProgress", group);
    fadeAnim->setDuration(200);
    fadeAnim->setStartValue(fadeProgress_);
    fadeAnim->setEndValue(0.0);
    fadeAnim->setEasingCurve(QEasingCurve::InCubic);

    group->addAnimation(posAnim);
    group->addAnimation(fadeAnim);

    connect(group, &QParallelAnimationGroup::finished, this, [this]() {
        emit closed(this);
        deleteLater();
    });

    activeAnimation_ = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void ToastNotification::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setOpacity(fadeProgress_);

    const bool dark = AppPreferences::instance().isDark();
    const QRectF cardRect(0.5, 0.5, static_cast<qreal>(width()) - 1.0, static_cast<qreal>(height()) - 1.0);

    QPainterPath cardPath;
    cardPath.addRoundedRect(cardRect, 12.0, 12.0);

    // 1. Glassmorphic frosted background: rgba(255,255,255,0.92) in light mode
    const QColor glassFill = dark
        ? QColor(30, 41, 59, 240)
        : QColor(255, 255, 255, 235);
    painter.fillPath(cardPath, glassFill);

    // 2. 4px status accent bar neatly clipped to the left rounded edge
    painter.save();
    painter.setClipPath(cardPath);
    painter.fillRect(QRectF(0.0, 0.0, 4.5, static_cast<qreal>(height())), accentColor());
    painter.restore();

    // 3. Outer subtle border + illuminated inner glass rim rgba(255,255,255,0.6)
    painter.setPen(QPen(dark ? QColor(71, 85, 105, 190) : QColor(226, 232, 240, 220), 1.0));
    painter.drawPath(cardPath);

    QPainterPath innerGlowPath;
    innerGlowPath.addRoundedRect(cardRect.adjusted(1.0, 1.0, -1.0, -1.0), 11.0, 11.0);
    painter.setPen(QPen(dark ? QColor(255, 255, 255, 36) : QColor(255, 255, 255, 153), 1.0));
    painter.drawPath(innerGlowPath);

    // 4. Status circular icon badge + crisp vector SVG icon
    const int iconContainerSize = 32;
    const int iconX = 16;
    const int iconY = (height() - iconContainerSize) / 2;
    const QRectF badgeRect(iconX, iconY, iconContainerSize, iconContainerSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(badgeFillColor(dark));
    painter.drawEllipse(badgeRect);

    QSvgRenderer svgRenderer(iconSvgBytes());
    const QRectF svgRect(iconX + 7.0, iconY + 7.0, 18.0, 18.0);
    svgRenderer.render(&painter, svgRect);

    // 5. Title + wrapped detail message
    const int textLeft = iconX + iconContainerSize + 12;
    const int textRightMargin = 32;
    const int textWidth = width() - textLeft - textRightMargin;

    QFont titleFont(QStringLiteral("Segoe UI"));
    titleFont.setPixelSize(13);
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    painter.setPen(dark ? QColor(248, 250, 252) : QColor(15, 23, 42));

    const QFontMetrics titleFm(titleFont);
    const QRect titleRect(textLeft, 12, textWidth, 20);
    painter.drawText(
        titleRect,
        Qt::AlignLeft | Qt::AlignVCenter,
        titleFm.elidedText(title_, Qt::ElideRight, textWidth)
    );

    QFont msgFont(QStringLiteral("Segoe UI"));
    msgFont.setPixelSize(12);
    msgFont.setWeight(QFont::Normal);
    painter.setFont(msgFont);
    painter.setPen(dark ? QColor(203, 213, 225) : QColor(71, 85, 105));

    const QRect msgRect(textLeft, 34, textWidth, height() - 44);
    painter.drawText(msgRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, message_);

    // 6. Subtle dismiss button ("x")
    const QColor closeColor = hovered_
        ? (dark ? QColor(226, 232, 240) : QColor(51, 65, 85))
        : (dark ? QColor(148, 163, 184) : QColor(148, 163, 184));
    painter.setPen(QPen(closeColor, 1.6, Qt::SolidLine, Qt::RoundCap));
    const QRectF cr = closeButtonRect_.adjusted(5, 5, -5, -5);
    painter.drawLine(cr.topLeft(), cr.bottomRight());
    painter.drawLine(cr.topRight(), cr.bottomLeft());
}

void ToastNotification::mousePressEvent(QMouseEvent* event) {
    if (event && event->button() == Qt::LeftButton) {
        dismiss();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ToastNotification::enterEvent(QEnterEvent* event) {
    hovered_ = true;
    if (autoCloseTimer_ && autoCloseTimer_->isActive()) {
        autoCloseTimer_->stop();
    }
    update();
    QWidget::enterEvent(event);
}

void ToastNotification::leaveEvent(QEvent* event) {
    hovered_ = false;
    if (!dismissing_ && autoCloseTimer_) {
        autoCloseTimer_->start(1400);
    }
    update();
    QWidget::leaveEvent(event);
}

QColor ToastNotification::accentColor() const noexcept {
    switch (type_) {
        case ToastType::Success: return QColor(16, 185, 129);  // #10B981 Emerald
        case ToastType::Error:   return QColor(239, 68, 68);   // #EF4444 Coral
        case ToastType::Warning: return QColor(245, 158, 11);  // #F59E0B Amber
        case ToastType::Info:    return QColor(37, 99, 235);   // #2563EB Corporate Blue
    }
    return QColor(37, 99, 235);
}

QColor ToastNotification::badgeFillColor(bool dark) const noexcept {
    if (dark) {
        switch (type_) {
            case ToastType::Success: return QColor(16, 185, 129, 38);
            case ToastType::Error:   return QColor(239, 68, 68, 38);
            case ToastType::Warning: return QColor(245, 158, 11, 38);
            case ToastType::Info:    return QColor(37, 99, 235, 44);
        }
    }
    switch (type_) {
        case ToastType::Success: return QColor(236, 253, 245); // #ECFDF5
        case ToastType::Error:   return QColor(254, 242, 242); // #FEF2F2
        case ToastType::Warning: return QColor(255, 251, 235); // #FFFBEB
        case ToastType::Info:    return QColor(239, 246, 255); // #EFF6FF
    }
    return QColor(239, 246, 255);
}

QByteArray ToastNotification::iconSvgBytes() const {
    switch (type_) {
        case ToastType::Success:
            return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
                   "stroke='#10B981' stroke-width='2.4' stroke-linecap='round' stroke-linejoin='round'>"
                   "<path d='M20 6L9 17l-5-5'/></svg>";
        case ToastType::Error:
            return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
                   "stroke='#EF4444' stroke-width='2.3' stroke-linecap='round' stroke-linejoin='round'>"
                   "<circle cx='12' cy='12' r='9'/><line x1='15' y1='9' x2='9' y2='15'/>"
                   "<line x1='9' y1='9' x2='15' y2='15'/></svg>";
        case ToastType::Warning:
            return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
                   "stroke='#F59E0B' stroke-width='2.2' stroke-linecap='round' stroke-linejoin='round'>"
                   "<path d='M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z'/>"
                   "<line x1='12' y1='9' x2='12' y2='13'/><line x1='12' y1='17' x2='12.01' y2='17'/></svg>";
        case ToastType::Info:
            return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
                   "stroke='#2563EB' stroke-width='2.2' stroke-linecap='round' stroke-linejoin='round'>"
                   "<circle cx='12' cy='12' r='9'/><line x1='12' y1='16' x2='12' y2='12'/>"
                   "<line x1='12' y1='8' x2='12.01' y2='8'/></svg>";
    }
    return {};
}

void ToastNotification::updateShadowForFade() {
    if (!shadowEffect_) {
        return;
    }
    const bool dark = AppPreferences::instance().isDark();
    const int baseAlpha = dark ? 115 : 44;
    const int alpha = static_cast<int>(std::lround(static_cast<double>(baseAlpha) * fadeProgress_));
    shadowEffect_->setColor(dark ? QColor(2, 6, 23, alpha) : QColor(15, 23, 42, alpha));
}

// ============================================================================
// ToastManager
// ============================================================================
ToastManager& ToastManager::instance() {
    static ToastManager sInstance;
    return sInstance;
}

void ToastManager::setHostWidget(QWidget* host) {
    if (host_ == host) {
        return;
    }
    if (host_) {
        host_->removeEventFilter(this);
    }
    clearAll();
    host_ = host;
    if (host_) {
        host_->installEventFilter(this);
    }
}

void ToastManager::clearAll() {
    auto snapshot = activeToasts_;
    activeToasts_.clear();
    for (const auto& ptr : snapshot) {
        if (ptr) {
            ptr->deleteLater();
        }
    }
}

void ToastManager::show(
    ToastType type,
    const QString& title,
    const QString& message,
    int durationMs
) {
    if (!host_) {
        return;
    }

    // Purge any stale pointers
    activeToasts_.erase(
        std::remove_if(activeToasts_.begin(), activeToasts_.end(), [](const QPointer<ToastNotification>& p) {
            return p.isNull();
        }),
        activeToasts_.end()
    );

    // Keep stack clean and unobstructed
    while (activeToasts_.size() >= kMaxVisibleToasts) {
        if (auto oldest = activeToasts_.front()) {
            activeToasts_.erase(activeToasts_.begin());
            oldest->dismiss();
        } else {
            activeToasts_.erase(activeToasts_.begin());
        }
    }

    auto* toast = new ToastNotification(type, title, message, durationMs, host_);
    connect(toast, &ToastNotification::closed, this, &ToastManager::handleToastClosed);
    activeToasts_.push_back(toast);

    // Anchor newest toast at the bottom-right corner with 24px margin, pushing older toasts upward
    const int x = std::max(kMarginPx, host_->width() - toast->width() - kMarginPx);
    const int y = std::max(kMarginPx, host_->height() - toast->height() - kMarginPx);

    repositionAll();
    toast->presentAt(QPoint(x, y));
}

void ToastManager::showSuccess(const QString& title, const QString& message, int durationMs) {
    instance().show(ToastType::Success, title, message, durationMs);
}

void ToastManager::showError(const QString& title, const QString& message, int durationMs) {
    instance().show(ToastType::Error, title, message, durationMs);
}

void ToastManager::showWarning(const QString& title, const QString& message, int durationMs) {
    instance().show(ToastType::Warning, title, message, durationMs);
}

void ToastManager::showInfo(const QString& title, const QString& message, int durationMs) {
    instance().show(ToastType::Info, title, message, durationMs);
}

bool ToastManager::eventFilter(QObject* watched, QEvent* event) {
    if (watched == host_ && event && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        repositionAll();
    }
    return QObject::eventFilter(watched, event);
}

void ToastManager::repositionAll() {
    if (!host_) {
        return;
    }

    int bottomCursor = host_->height() - kMarginPx;
    for (auto it = activeToasts_.rbegin(); it != activeToasts_.rend(); ++it) {
        ToastNotification* toast = it->data();
        if (!toast) {
            continue;
        }
        const int x = std::max(kMarginPx, host_->width() - toast->width() - kMarginPx);
        const int y = std::max(kMarginPx, bottomCursor - toast->height());
        if (toast->isVisible()) {
            toast->moveToTarget(QPoint(x, y));
            toast->raise();
        }
        bottomCursor = y - kStackSpacingPx;
    }
}

void ToastManager::handleToastClosed(ToastNotification* toast) {
    activeToasts_.erase(
        std::remove_if(activeToasts_.begin(), activeToasts_.end(), [toast](const QPointer<ToastNotification>& p) {
            return p.isNull() || p.data() == toast;
        }),
        activeToasts_.end()
    );
    repositionAll();
}

} // namespace vamp9::pos::presentation
