#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class QWidget;

namespace vamp9::pos::presentation {

// Production HID Keyboard-Wedge USB laser barcode scanner interceptor.
// Measures inter-key arrival latency via QElapsedTimer:
//   - Rapid bursts (< 40 ms between characters) are captured into m_scanBuffer
//     and intercepted (return true) so stray digits never trigger buttons or corrupt inputs.
//   - Enter/Return with >= 4 buffered characters emits barcodeScanned(code).
//   - Normal human typing (> 40 ms) passes through untouched (return false).
class BarcodeScannerHandler final : public QObject {
    Q_OBJECT

public:
    static constexpr qint64 kDefaultMaxInterKeyMs = 40;
    static constexpr int kDefaultMinBarcodeLength = 4;

    explicit BarcodeScannerHandler(
        QWidget* targetWindow,
        qint64 maxInterKeyMs = kDefaultMaxInterKeyMs,
        int minBarcodeLength = kDefaultMinBarcodeLength,
        QObject* parent = nullptr
    );
    ~BarcodeScannerHandler() override = default;

    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;

    [[nodiscard]] qint64 maxInterKeyMs() const noexcept;
    [[nodiscard]] int minBarcodeLength() const noexcept;

    // Programmatic helper for diagnostics or testing.
    void simulateScan(const QString& code);

signals:
    void barcodeScanned(const QString& code);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void resetBuffer();
    [[nodiscard]] static bool isTextInputWidget(const QWidget* widget) noexcept;
    [[nodiscard]] static bool isModifierOnlyKey(int key) noexcept;
    [[nodiscard]] static bool isAcceptableBarcodeChar(QChar ch) noexcept;

    QPointer<QWidget> targetWindow_;
    qint64 maxInterKeyMs_{kDefaultMaxInterKeyMs};
    int minBarcodeLength_{kDefaultMinBarcodeLength};
    bool enabled_{true};

    QElapsedTimer keyTimer_;
    QTimer inactivityTimer_;
    QString m_scanBuffer;
    bool inScanBurst_{false};
    bool firstCharPassedToTextInput_{false};
    QPointer<QWidget> pendingTextInput_;
};

} // namespace vamp9::pos::presentation
