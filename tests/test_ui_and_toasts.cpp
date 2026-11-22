#include "application/CartService.h"
#include "infrastructure/hardware/MockReceiptPrinter.h"
#include "infrastructure/logging/Logger.h"
#include "infrastructure/persistence/DatabaseManager.h"
#include "infrastructure/persistence/SqliteProductRepository.h"
#include "presentation/MainWindow.h"
#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/TranslationManager.h"
#include "presentation/widgets/ToastManager.h"
#include "presentation/widgets/TopBar.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest/QtTest>

#include <filesystem>
#include <memory>

namespace {

using namespace vamp9::pos::presentation;

void loadStylesheetForTest(const QString& qssFileName) {
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("assets/styles/") + qssFileName),
        QDir::current().filePath(QStringLiteral("assets/styles/") + qssFileName)
    };
    for (const QString& path : candidates) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            qApp->setStyleSheet(in.readAll());
            return;
        }
    }
}

std::filesystem::path findMigrationsDir() {
    const std::filesystem::path appDir = QCoreApplication::applicationDirPath().toStdString();
    const auto nextToBin = appDir / "database" / "migrations";
    if (std::filesystem::exists(nextToBin)) {
        return nextToBin;
    }
    const auto parentOfTests = appDir.parent_path() / "database" / "migrations";
    if (std::filesystem::exists(parentOfTests)) {
        return parentOfTests;
    }
#ifdef VAMP9_SOURCE_DIR
    const auto fromSource = std::filesystem::path(VAMP9_SOURCE_DIR) / "database" / "migrations";
    if (std::filesystem::exists(fromSource)) {
        return fromSource;
    }
#endif
    return std::filesystem::current_path() / "database" / "migrations";
}

class TestUiAndToasts final : public QObject {
    Q_OBJECT

private slots:
    void verifiesCommercialTopBarToastsBilingualAndOptionalScreenshots() {
#ifdef VAMP9_SOURCE_DIR
        QDir::setCurrent(QStringLiteral(VAMP9_SOURCE_DIR));
#endif
        const AppTheme savedTheme = AppPreferences::instance().theme();
        const AppLanguage savedLang = AppPreferences::instance().language();

        AppPreferences::instance().setTheme(AppTheme::Light);
        TranslationManager::instance().applyLanguage(AppLanguage::English);
        loadStylesheetForTest(QStringLiteral("main.qss"));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const std::filesystem::path root = tempDir.path().toStdString();

        auto logger = std::make_shared<vamp9::pos::infrastructure::Logger>(root / "test.log");
        auto db = std::make_shared<vamp9::pos::infrastructure::DatabaseManager>(
            root / "test_pos.db",
            QStringLiteral("test_ui_conn")
        );
        const auto initRes = db->initialize(findMigrationsDir());
        QVERIFY2(initRes.hasValue(), initRes ? "" : initRes.error().message.c_str());

        auto repo = std::make_shared<vamp9::pos::infrastructure::SqliteProductRepository>(db);
        auto printer = std::make_shared<vamp9::pos::infrastructure::MockReceiptPrinter>(logger);
        auto cart = std::make_shared<vamp9::pos::application::CartService>(repo);

        MainWindow::RuntimeStatus status;
        MainWindow window(cart, repo, printer, status);
        window.resize(1440, 860);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window, 3000));
        QTest::qWait(200);

        // 1. Verify TopBar cleanliness: 3-decimal scale, gear icon button, zero scanToast
        auto* scaleVal = window.findChild<QLabel*>(QStringLiteral("scaleValueLabel"));
        QVERIFY(scaleVal != nullptr);
        QCOMPARE(scaleVal->text(), QStringLiteral("0.000"));

        auto* oldScanToast = window.findChild<QLabel*>(QStringLiteral("scanToast"));
        QVERIFY(oldScanToast == nullptr);

        auto* gearBtn = window.findChild<QPushButton*>(QStringLiteral("settingsGearBtn"));
        QVERIFY(gearBtn != nullptr);
        QCOMPARE(gearBtn->size(), QSize(34, 34));
        QVERIFY(gearBtn->isCheckable());

        // Verify clicking gear icon opens Settings view (index 3)
        auto* stack = window.findChild<QStackedWidget*>();
        QVERIFY(stack != nullptr);
        QCOMPARE(stack->currentIndex(), 0);

        // Capture clean screenshots when requested via environment variable
        const QString captureDir = qEnvironmentVariable("VAMP9_CAPTURE_SCREENSHOTS_DIR");
        if (!captureDir.isEmpty()) {
            QDir().mkpath(captureDir);

            // Screenshot 1: Checkout view (Light mode, 3 preloaded items, real product photos)
            QTest::qWait(250);
            window.grab().save(QDir(captureDir).filePath(QStringLiteral("checkout.png")), "PNG");

            // Screenshot 2: Checkout with live glassmorphic Emerald Toast Notification
            ToastManager::showSuccess(
                QStringLiteral("Sale Approved"),
                QStringLiteral("Sale #BOL-1042 approved — Change: $ 2.500"),
                5000
            );
            QTest::qWait(350);
            window.grab().save(QDir(captureDir).filePath(QStringLiteral("toast_notification.png")), "PNG");
            ToastManager::instance().clearAll();
            QTest::qWait(100);

            // Screenshot 3: Inventory view with visible localized headers and 15 SQLite products
            auto* topBar = window.findChild<TopBar*>();
            QVERIFY(topBar != nullptr);
            auto topButtons = topBar->findChildren<QPushButton*>();
            for (auto* btn : topButtons) {
                if (btn->property("topTab").toBool() && btn->text() == QStringLiteral("Inventory")) {
                    btn->click();
                    break;
                }
            }
            QTest::qWait(250);
            window.grab().save(QDir(captureDir).filePath(QStringLiteral("inventory.png")), "PNG");

            // Screenshot 4: Reports & Shift Close view
            for (auto* btn : topButtons) {
                if (btn->property("topTab").toBool() && btn->text() == QStringLiteral("Reports")) {
                    btn->click();
                    break;
                }
            }
            QTest::qWait(250);
            window.grab().save(QDir(captureDir).filePath(QStringLiteral("reports.png")), "PNG");

            // Screenshot 5: Settings view via Gear Icon (Dark mode showcase)
            gearBtn->click();
            AppPreferences::instance().setTheme(AppTheme::Dark);
            QTest::qWait(250);
            window.grab().save(QDir(captureDir).filePath(QStringLiteral("settings_dark.png")), "PNG");

            // Return to Light mode & Checkout for remaining assertions
            AppPreferences::instance().setTheme(AppTheme::Light);
            for (auto* btn : topButtons) {
                if (btn->property("topTab").toBool() && btn->text() == QStringLiteral("Checkout")) {
                    btn->click();
                    break;
                }
            }
            QTest::qWait(150);
        }

        // 2. Verify Settings gear button opens SettingsView (index 3)
        gearBtn->click();
        QCOMPARE(stack->currentIndex(), 3);

        // 3. Verify live bilingual switching (English -> Spanish -> English)
        TranslationManager::instance().applyLanguage(AppLanguage::Spanish);
        QTest::qWait(60);
        auto* scalePrefix = window.findChild<QLabel*>(QStringLiteral("scalePrefixLabel"));
        QVERIFY(scalePrefix != nullptr);
        QCOMPARE(scalePrefix->text(), QStringLiteral("Peso:"));
        QVERIFY(window.windowTitle().contains(QStringLiteral("Terminal de Caja")));

        TranslationManager::instance().applyLanguage(AppLanguage::English);
        QTest::qWait(60);
        QCOMPARE(scalePrefix->text(), QStringLiteral("Weight:"));

        // 4. Verify ToastNotification widget creation, 24px shadow blur, and bottom-right 24px anchor
        ToastManager::showError(
            QStringLiteral("Unregistered Barcode"),
            QStringLiteral("No product matches code \"9999999999999\"."),
            3200
        );
        QTest::qWait(300);
        auto* toast = window.findChild<ToastNotification*>();
        QVERIFY(toast != nullptr);
        auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(toast->graphicsEffect());
        QVERIFY(shadow != nullptr);
        QCOMPARE(shadow->blurRadius(), 24.0);
        QCOMPARE(window.width() - (toast->x() + toast->width()), ToastManager::kMarginPx);

        // 5. Verify borderless fullscreen toggle (F11 / programmatic)
        QVERIFY(!window.isFullScreen());
        window.toggleFullScreenMode();
        QTest::qWait(150);
        QVERIFY(window.isFullScreen());
        window.toggleFullScreenMode();
        QTest::qWait(150);
        QVERIFY(!window.isFullScreen());

        ToastManager::instance().clearAll();
        window.close();

        // Restore original user preferences
        AppPreferences::instance().setTheme(savedTheme);
        TranslationManager::instance().applyLanguage(savedLang);
    }
};

} // namespace

QTEST_MAIN(TestUiAndToasts)
#include "test_ui_and_toasts.moc"
