#pragma once

#include <QColor>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QWidget>

#include <vector>

class QEnterEvent;
class QEvent;
class QGraphicsDropShadowEffect;
class QMouseEvent;
class QPaintEvent;
class QParallelAnimationGroup;
class QTimer;

namespace vamp9::pos::presentation {

enum class ToastType {
    Success,
    Error,
    Warning,
    Info
};

// Non-blocking floating glassmorphic toast card with 24px drop shadow, 4px status bar,
// crisp SVG icon, vertical slide + opacity QPropertyAnimation, and 3.2s auto-dismiss.
class ToastNotification final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal fadeProgress READ fadeProgress WRITE setFadeProgress)

public:
    ToastNotification(
        ToastType type,
        const QString& title,
        const QString& message,
        int durationMs,
        QWidget* parent
    );
    ~ToastNotification() override = default;

    [[nodiscard]] qreal fadeProgress() const noexcept;
    void setFadeProgress(qreal value);

    void presentAt(const QPoint& targetPos);
    void moveToTarget(const QPoint& targetPos);
    void dismiss();

signals:
    void closed(ToastNotification* self);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    [[nodiscard]] QColor accentColor() const noexcept;
    [[nodiscard]] QColor badgeFillColor(bool dark) const noexcept;
    [[nodiscard]] QByteArray iconSvgBytes() const;
    void updateShadowForFade();

    ToastType type_;
    QString title_;
    QString message_;
    int durationMs_{3200};
    qreal fadeProgress_{0.0};
    bool dismissing_{false};
    bool hovered_{false};

    QGraphicsDropShadowEffect* shadowEffect_{nullptr};
    QTimer* autoCloseTimer_{nullptr};
    QPointer<QParallelAnimationGroup> activeAnimation_;
    QRect closeButtonRect_;
};

// Centralized manager anchoring floating glassmorphic toasts to the bottom-right corner
// of the active MainWindow with a 24px margin and responsive resize repositioning.
class ToastManager final : public QObject {
    Q_OBJECT

public:
    static constexpr int kDefaultDurationMs = 3200;
    static constexpr int kMarginPx = 24;
    static constexpr int kStackSpacingPx = 10;
    static constexpr std::size_t kMaxVisibleToasts = 4;

    static ToastManager& instance();

    void setHostWidget(QWidget* host);
    void clearAll();

    void show(
        ToastType type,
        const QString& title,
        const QString& message,
        int durationMs = kDefaultDurationMs
    );

    static void showSuccess(const QString& title, const QString& message, int durationMs = kDefaultDurationMs);
    static void showError(const QString& title, const QString& message, int durationMs = kDefaultDurationMs);
    static void showWarning(const QString& title, const QString& message, int durationMs = kDefaultDurationMs);
    static void showInfo(const QString& title, const QString& message, int durationMs = kDefaultDurationMs);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ToastManager() = default;
    void repositionAll();
    void handleToastClosed(ToastNotification* toast);

    QPointer<QWidget> host_;
    std::vector<QPointer<ToastNotification>> activeToasts_;
};

} // namespace vamp9::pos::presentation
