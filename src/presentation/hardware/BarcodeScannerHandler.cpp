#include "presentation/hardware/BarcodeScannerHandler.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QWidget>

namespace vamp9::pos::presentation {

BarcodeScannerHandler::BarcodeScannerHandler(
    QWidget* targetWindow,
    qint64 maxInterKeyMs,
    int minBarcodeLength,
    QObject* parent
)
    : QObject(parent)
    , targetWindow_(targetWindow)
    , maxInterKeyMs_(maxInterKeyMs)
    , minBarcodeLength_(minBarcodeLength) {
    inactivityTimer_.setSingleShot(true);
    inactivityTimer_.setInterval(static_cast<int>(maxInterKeyMs_ * 3));
    connect(&inactivityTimer_, &QTimer::timeout, this, &BarcodeScannerHandler::resetBuffer);
}

void BarcodeScannerHandler::setEnabled(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled_) {
        resetBuffer();
    }
}

bool BarcodeScannerHandler::isEnabled() const noexcept {
    return enabled_;
}

qint64 BarcodeScannerHandler::maxInterKeyMs() const noexcept {
    return maxInterKeyMs_;
}

int BarcodeScannerHandler::minBarcodeLength() const noexcept {
    return minBarcodeLength_;
}

void BarcodeScannerHandler::simulateScan(const QString& code) {
    const QString cleaned = code.trimmed();
    if (cleaned.size() >= minBarcodeLength_) {
        emit barcodeScanned(cleaned);
    }
}

bool BarcodeScannerHandler::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched);

    if (!enabled_ || !event || event->type() != QEvent::KeyPress) {
        return false;
    }

    // Only intercept keystrokes belonging to the active POS window (ignore modal dialogs if needed).
    QWidget* activeWin = QApplication::activeWindow();
    if (targetWindow_ && activeWin && activeWin != targetWindow_) {
        return false;
    }

    auto* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->isAutoRepeat()) {
        return false;
    }

    const int key = keyEvent->key();
    if (isModifierOnlyKey(key)) {
        return false;
    }

    // Allow standard Ctrl/Alt keyboard shortcuts (Ctrl+C, Alt+F4, etc.) to pass normally.
    const Qt::KeyboardModifiers mods = keyEvent->modifiers();
    if (mods.testFlag(Qt::ControlModifier) || mods.testFlag(Qt::AltModifier) || mods.testFlag(Qt::MetaModifier)) {
        resetBuffer();
        return false;
    }

    const bool hasElapsed = keyTimer_.isValid();
    const qint64 elapsedMs = hasElapsed ? keyTimer_.elapsed() : (maxInterKeyMs_ + 1);
    const bool isFastKeystroke = hasElapsed && (elapsedMs < maxInterKeyMs_);

    // 1. Handle scanner terminator (Return / Enter)
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (m_scanBuffer.size() >= minBarcodeLength_ && (inScanBurst_ || isFastKeystroke)) {
            const QString scannedCode = m_scanBuffer.trimmed();
            resetBuffer();
            if (scannedCode.size() >= minBarcodeLength_) {
                emit barcodeScanned(scannedCode);
                return true;
            }
        }
        resetBuffer();
        return false;
    }

    // 2. Inspect printable character payload
    const QString text = keyEvent->text();
    if (text.size() != 1 || !isAcceptableBarcodeChar(text.at(0))) {
        resetBuffer();
        return false;
    }

    const QChar ch = text.at(0);
    QWidget* focusWidget = QApplication::focusWidget();
    const bool focusedOnText = isTextInputWidget(focusWidget);

    if (isFastKeystroke && !m_scanBuffer.isEmpty()) {
        // Confirmed rapid HID burst (< 40 ms): capture and swallow the keystroke.
        if (!inScanBurst_ && firstCharPassedToTextInput_ && pendingTextInput_) {
            // Remove the single leading character that slipped into the focused QLineEdit
            // before we knew this was a scanner burst rather than human typing.
            if (auto* lineEdit = qobject_cast<QLineEdit*>(pendingTextInput_.data())) {
                if (lineEdit->text().endsWith(m_scanBuffer.at(0))) {
                    lineEdit->backspace();
                }
            }
            firstCharPassedToTextInput_ = false;
            pendingTextInput_.clear();
        }

        inScanBurst_ = true;
        m_scanBuffer.append(ch);
        keyTimer_.restart();
        inactivityTimer_.start();
        return true;
    }

    // Slow arrival (> 40 ms) or first character of a potential scan:
    // Start a fresh candidate buffer and measure arrival time of the next key.
    m_scanBuffer.clear();
    m_scanBuffer.append(ch);
    inScanBurst_ = false;
    keyTimer_.restart();
    inactivityTimer_.start();

    if (focusedOnText) {
        // Let the first character reach the QLineEdit so normal human typing feels instantaneous.
        // If a second key arrives within < 40 ms, we will retroactively clean it up and lock the burst.
        firstCharPassedToTextInput_ = true;
        pendingTextInput_ = focusWidget;
        return false;
    }

    // No text input has focus: intercept so rapid scanner characters never trigger button mnemonics.
    firstCharPassedToTextInput_ = false;
    pendingTextInput_.clear();
    return true;
}

void BarcodeScannerHandler::resetBuffer() {
    m_scanBuffer.clear();
    inScanBurst_ = false;
    firstCharPassedToTextInput_ = false;
    pendingTextInput_.clear();
    keyTimer_.invalidate();
    inactivityTimer_.stop();
}

bool BarcodeScannerHandler::isTextInputWidget(const QWidget* widget) noexcept {
    if (!widget) {
        return false;
    }
    return qobject_cast<const QLineEdit*>(widget) != nullptr
        || qobject_cast<const QTextEdit*>(widget) != nullptr
        || qobject_cast<const QPlainTextEdit*>(widget) != nullptr
        || qobject_cast<const QAbstractSpinBox*>(widget) != nullptr;
}

bool BarcodeScannerHandler::isModifierOnlyKey(int key) noexcept {
    return key == Qt::Key_Shift
        || key == Qt::Key_Control
        || key == Qt::Key_Alt
        || key == Qt::Key_AltGr
        || key == Qt::Key_Meta
        || key == Qt::Key_CapsLock
        || key == Qt::Key_NumLock;
}

bool BarcodeScannerHandler::isAcceptableBarcodeChar(QChar ch) noexcept {
    return ch.isLetterOrNumber()
        || ch == QLatin1Char('-')
        || ch == QLatin1Char('_')
        || ch == QLatin1Char('.')
        || ch == QLatin1Char('/');
}

} // namespace vamp9::pos::presentation
