#pragma once

#include "application/CartService.h"
#include "domain/interfaces/IProductRepository.h"
#include "domain/interfaces/IReceiptPrinter.h"
#include "presentation/utils/AppPreferences.h"

#include <QMainWindow>

#include <memory>
#include <string>

class QEvent;
class QPaintEvent;
class QStackedWidget;

namespace vamp9::pos::presentation {

class BarcodeScannerHandler;
class CheckoutView;
class InventoryView;
class ReportsView;
class SettingsView;
class TopBar;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    struct RuntimeStatus {
        std::string journalMode{"wal"};
        std::string printerName{"Mock ESC/POS"};
        bool printerOnline{true};
        bool syncWorkerRunning{false};
    };

    MainWindow(
        std::shared_ptr<application::CartService> cartService,
        std::shared_ptr<domain::IProductRepository> productRepository,
        std::shared_ptr<domain::IReceiptPrinter> receiptPrinter,
        RuntimeStatus runtimeStatus,
        QWidget* parent = nullptr
    );
    ~MainWindow() override;

    void toggleFullScreenMode();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void navigateTo(int index);
    void applyTheme(AppTheme theme);
    void retranslateAll();

    std::shared_ptr<application::CartService> cartService_;
    std::shared_ptr<domain::IProductRepository> productRepository_;
    std::shared_ptr<domain::IReceiptPrinter> receiptPrinter_;

    BarcodeScannerHandler* scannerHandler_{nullptr};
    TopBar* topBar_{nullptr};
    QStackedWidget* viewStack_{nullptr};

    CheckoutView* checkoutView_{nullptr};
    InventoryView* inventoryView_{nullptr};
    ReportsView* reportsView_{nullptr};
    SettingsView* settingsView_{nullptr};

    bool wasMaximizedBeforeFullscreen_{false};
};

} // namespace vamp9::pos::presentation
