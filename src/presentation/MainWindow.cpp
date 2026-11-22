#include "presentation/MainWindow.h"

#include "presentation/hardware/BarcodeScannerHandler.h"
#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/TranslationManager.h"
#include "presentation/views/CheckoutView.h"
#include "presentation/views/InventoryView.h"
#include "presentation/views/ReportsView.h"
#include "presentation/views/SettingsView.h"
#include "presentation/widgets/ToastManager.h"
#include "presentation/widgets/TopBar.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <utility>

namespace vamp9::pos::presentation {

MainWindow::MainWindow(
    std::shared_ptr<application::CartService> cartService,
    std::shared_ptr<domain::IProductRepository> productRepository,
    std::shared_ptr<domain::IReceiptPrinter> receiptPrinter,
    RuntimeStatus runtimeStatus,
    QWidget* parent
)
    : QMainWindow(parent)
    , cartService_(std::move(cartService))
    , productRepository_(std::move(productRepository))
    , receiptPrinter_(std::move(receiptPrinter)) {
    Q_UNUSED(runtimeStatus);

    setMinimumSize(1280, 760);
    resize(1440, 860);

    setupUi();
    ToastManager::instance().setHostWidget(this);
    retranslateAll();

    auto& prefs = AppPreferences::instance();
    connect(&prefs, &AppPreferences::themeChanged, this, &MainWindow::applyTheme);
    connect(&prefs, &AppPreferences::languageChanged, this, [this](AppLanguage) {
        retranslateAll();
    });

    // Global USB laser barcode scanner interceptor (HID Keyboard Wedge < 40 ms)
    scannerHandler_ = new BarcodeScannerHandler(this, 40, 4, this);
    if (qApp) {
        qApp->installEventFilter(scannerHandler_);
        // Global borderless fullscreen shortcuts (F11 and Alt+Enter) installed on top
        qApp->installEventFilter(this);
    }

    connect(scannerHandler_, &BarcodeScannerHandler::barcodeScanned, this, [this](const QString& code) {
        if (viewStack_ && viewStack_->currentIndex() != 0) {
            navigateTo(0);
            if (topBar_) {
                topBar_->setActiveTab(0);
            }
        }
        if (checkoutView_) {
            checkoutView_->handleBarcodeScanned(code);
        }
    });
}

MainWindow::~MainWindow() {
    ToastManager::instance().setHostWidget(nullptr);
    if (qApp) {
        qApp->removeEventFilter(this);
        if (scannerHandler_) {
            qApp->removeEventFilter(scannerHandler_);
        }
    }
}

void MainWindow::toggleFullScreenMode() {
    auto tr = &AppPreferences::trText;
    if (isFullScreen()) {
        if (wasMaximizedBeforeFullscreen_) {
            showMaximized();
        } else {
            showNormal();
        }
        if (settingsView_) {
            settingsView_->setFullscreenState(false);
        }
        ToastManager::showInfo(
            tr(QStringLiteral("Windowed Mode"), QStringLiteral("Modo Ventana")),
            tr(QStringLiteral("Press F11 or Alt + Enter to return to fullscreen."),
               QStringLiteral("Presiona F11 o Alt + Enter para volver a pantalla completa."))
        );
    } else {
        wasMaximizedBeforeFullscreen_ = isMaximized();
        showFullScreen();
        if (settingsView_) {
            settingsView_->setFullscreenState(true);
        }
        ToastManager::showInfo(
            tr(QStringLiteral("Borderless Fullscreen"), QStringLiteral("Pantalla Completa Sin Bordes")),
            tr(QStringLiteral("Press F11 or Alt + Enter at any time to exit."),
               QStringLiteral("Presiona F11 o Alt + Enter en cualquier momento para salir."))
        );
    }
}

void MainWindow::paintEvent(QPaintEvent* event) {
    QMainWindow::paintEvent(event);

    QPainter painter(this);
    const QColor canvasColor = AppPreferences::instance().isDark()
        ? QColor(11, 17, 32)   // #0B1120 Dark slate canvas
        : QColor(241, 245, 249); // #F1F5F9 Commercial cool gray canvas
    painter.fillRect(rect(), canvasColor);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched);

    if (event && event->type() == QEvent::KeyPress) {
        QWidget* activeWin = QApplication::activeWindow();
        if (activeWin == this) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (!keyEvent->isAutoRepeat()) {
                const int key = keyEvent->key();
                const Qt::KeyboardModifiers mods = keyEvent->modifiers();

                const bool isF11 = (key == Qt::Key_F11) && (mods == Qt::NoModifier);
                const bool isAltEnter = mods.testFlag(Qt::AltModifier)
                    && (key == Qt::Key_Return || key == Qt::Key_Enter);

                if (isF11 || isAltEnter) {
                    toggleFullScreenMode();
                    return true;
                }
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralCanvas"));
    setCentralWidget(central);

    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    topBar_ = new TopBar(QStringLiteral("C"), central);
    rootLayout->addWidget(topBar_);

    auto* bodyWrapper = new QWidget(central);
    bodyWrapper->setObjectName(QStringLiteral("workspaceContainer"));
    auto* bodyLayout = new QVBoxLayout(bodyWrapper);
    bodyLayout->setContentsMargins(14, 14, 14, 14);
    bodyLayout->setSpacing(0);

    viewStack_ = new QStackedWidget(bodyWrapper);
    checkoutView_ = new CheckoutView(cartService_, productRepository_, receiptPrinter_, viewStack_);
    inventoryView_ = new InventoryView(productRepository_, viewStack_);
    reportsView_ = new ReportsView(viewStack_);
    settingsView_ = new SettingsView(viewStack_);

    viewStack_->addWidget(checkoutView_);
    viewStack_->addWidget(inventoryView_);
    viewStack_->addWidget(reportsView_);
    viewStack_->addWidget(settingsView_);

    bodyLayout->addWidget(viewStack_, 1);
    rootLayout->addWidget(bodyWrapper, 1);

    connect(topBar_, &TopBar::navigationRequested, this, &MainWindow::navigateTo);
    connect(topBar_, &TopBar::scaleWeightChanged, checkoutView_, &CheckoutView::setScaleWeightKg);
    connect(topBar_, &TopBar::searchTextChanged, checkoutView_, &CheckoutView::setSearchFilter);
    connect(topBar_, &TopBar::searchSubmitted, checkoutView_, &CheckoutView::submitSearchQuery);

    connect(checkoutView_, &CheckoutView::scaleResetRequested, topBar_, &TopBar::resetScaleWeight);
    connect(checkoutView_, &CheckoutView::saleCompleted, reportsView_, &ReportsView::recordSale);
    connect(inventoryView_, &InventoryView::productCreated, checkoutView_, &CheckoutView::addCatalogProduct);
    connect(settingsView_, &SettingsView::fullscreenToggleRequested, this, &MainWindow::toggleFullScreenMode);
}

void MainWindow::navigateTo(int index) {
    if (!viewStack_ || index < 0 || index >= viewStack_->count()) {
        return;
    }
    viewStack_->setCurrentIndex(index);
    if (topBar_) {
        topBar_->setCheckoutToolsVisible(index == 0);
    }
    if (index == 1 && inventoryView_) {
        inventoryView_->reloadProducts();
    }
    if (index == 3 && settingsView_) {
        settingsView_->setFullscreenState(isFullScreen());
    }
}

void MainWindow::applyTheme(AppTheme theme) {
    const QString fileName = (theme == AppTheme::Dark)
        ? QStringLiteral("dark.qss")
        : QStringLiteral("main.qss");

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("assets/styles/") + fileName),
        QDir::current().filePath(QStringLiteral("assets/styles/") + fileName)
    };

    for (const QString& path : candidates) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            if (qApp) {
                qApp->setStyleSheet(in.readAll());
            }
            break;
        }
    }

    update();
    if (topBar_) {
        topBar_->refreshThemeVisuals();
    }
    if (checkoutView_) {
        checkoutView_->refreshThemeVisuals();
    }
}

void MainWindow::retranslateAll() {
    setWindowTitle(TranslationManager::text(
        QStringLiteral("vamp9 POS — Checkout Terminal"),
        QStringLiteral("vamp9 POS — Terminal de Caja")
    ));
    if (topBar_) {
        topBar_->retranslateUi();
    }
    if (checkoutView_) {
        checkoutView_->retranslateUi();
    }
    if (inventoryView_) {
        inventoryView_->retranslateUi();
    }
    if (reportsView_) {
        reportsView_->retranslateUi();
    }
    if (settingsView_) {
        settingsView_->retranslateUi();
    }
}

} // namespace vamp9::pos::presentation
