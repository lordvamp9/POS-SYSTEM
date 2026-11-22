#include "application/CartService.h"
#include "application/SyncWorker.h"
#include "infrastructure/hardware/MockReceiptPrinter.h"
#include "infrastructure/logging/Logger.h"
#include "infrastructure/persistence/DatabaseManager.h"
#include "infrastructure/persistence/SqliteProductRepository.h"
#include "presentation/MainWindow.h"
#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/TranslationManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextStream>

#include <chrono>
#include <filesystem>
#include <memory>

namespace {

[[nodiscard]] std::filesystem::path resolveRuntimePath(const std::filesystem::path& relativePath) {
    const std::filesystem::path appDir = QCoreApplication::applicationDirPath().toStdString();
    const std::filesystem::path nextToBinary = appDir / relativePath;
    if (std::filesystem::exists(nextToBinary)) {
        return nextToBinary;
    }
    return std::filesystem::current_path() / relativePath;
}

[[nodiscard]] std::filesystem::path resolveWritableDataRoot() {
    const QString standardDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!standardDir.isEmpty()) {
        QDir().mkpath(standardDir);
        return std::filesystem::path(standardDir.toStdString());
    }
    return std::filesystem::path(QCoreApplication::applicationDirPath().toStdString());
}

void applyStylesheet(QApplication& app, vamp9::pos::infrastructure::Logger& logger) {
    const bool isDark = vamp9::pos::presentation::AppPreferences::instance().isDark();
    const std::string primaryFile = isDark ? "dark.qss" : "main.qss";
    auto qssPath = resolveRuntimePath(std::filesystem::path("assets") / "styles" / primaryFile);
    if (!std::filesystem::exists(qssPath)) {
        qssPath = resolveRuntimePath(std::filesystem::path("assets") / "styles" / "main.qss");
    }

    QFile file(QString::fromStdString(qssPath.string()));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logger.warn("Unable to load QSS stylesheet from: " + qssPath.string());
        return;
    }

    QTextStream in(&file);
    app.setStyleSheet(in.readAll());
    logger.info("Loaded retail QSS theme from: " + qssPath.string());
}

} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough
    );

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("vamp9"));
    QCoreApplication::setApplicationName(QStringLiteral("vamp9 POS"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    // Initialize bilingual translator before instantiating views
    vamp9::pos::presentation::TranslationManager::instance().applyLanguage(
        vamp9::pos::presentation::AppPreferences::instance().language()
    );

    const auto brandIconPath = resolveRuntimePath(std::filesystem::path("assets") / "icons" / "vamp9_brand.png");
    if (std::filesystem::exists(brandIconPath)) {
        app.setWindowIcon(QIcon(QString::fromStdString(brandIconPath.string())));
    }

    const std::filesystem::path writableRoot = resolveWritableDataRoot();

    // 1. Local File Logger (user writable AppData directory for UAC-safe Program Files installs)
    auto logger = std::make_shared<vamp9::pos::infrastructure::Logger>(
        writableRoot / "logs" / "vamp9_pos.log"
    );
    logger->info("Bootstrapping vamp9 POS v1.0.0 (Offline-First C++20 / Qt6)...");

    // 2. Local SQLite3 Database (WAL mode + Idempotent SQL Migrations)
    auto dbManager = std::make_shared<vamp9::pos::infrastructure::DatabaseManager>(
        writableRoot / "data" / "vamp9_pos.db"
    );
    const auto migrationsDir = resolveRuntimePath(std::filesystem::path("database") / "migrations");
    if (const auto initRes = dbManager->initialize(migrationsDir); !initRes) {
        logger->error("Database initialization failed: " + initRes.error().message);
        QMessageBox::critical(
            nullptr,
            QStringLiteral("Database Initialization Error"),
            QString::fromStdString(initRes.error().message)
        );
        return 1;
    }
    logger->info("SQLite initialized with journal_mode=" + dbManager->activeJournalMode());

    // 3. Composition Root (Dependency Injection)
    auto productRepository = std::make_shared<vamp9::pos::infrastructure::SqliteProductRepository>(dbManager);
    auto receiptPrinter = std::make_shared<vamp9::pos::infrastructure::MockReceiptPrinter>(logger);
    auto cartService = std::make_shared<vamp9::pos::application::CartService>(productRepository);

    // 4. Offline-First Outbox Background Worker (RAII clean shutdown)
    auto syncWorker = std::make_unique<vamp9::pos::application::SyncWorker>(
        std::chrono::seconds(15),
        [logger](const std::string& msg) { logger->info(msg); }
    );
    syncWorker->start();

    // 5. Presentation Layer (Commercial Retail UI)
    applyStylesheet(app, *logger);

    const auto printerStatus = receiptPrinter->status();
    vamp9::pos::presentation::MainWindow::RuntimeStatus runtimeStatus{
        .journalMode = dbManager->activeJournalMode(),
        .printerName = printerStatus.deviceName,
        .printerOnline = printerStatus.isOnline,
        .syncWorkerRunning = syncWorker->isRunning()
    };

    vamp9::pos::presentation::MainWindow mainWindow(
        cartService,
        productRepository,
        receiptPrinter,
        runtimeStatus
    );
    mainWindow.show();

    const int exitCode = app.exec();

    syncWorker->stop();
    logger->info("Application exited cleanly with code " + std::to_string(exitCode));
    return exitCode;
}
