#pragma once

#include "domain/entities/Product.h"

#include <QSqlDatabase>
#include <QString>

#include <filesystem>
#include <string>

namespace vamp9::pos::infrastructure {

// RAII SQLite3 connection manager using QtSql, configured for WAL mode and crash resilience.
class DatabaseManager {
public:
    explicit DatabaseManager(
        std::filesystem::path databasePath,
        QString connectionName = QStringLiteral("vamp9_pos_main")
    );
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    DatabaseManager(DatabaseManager&&) = delete;
    DatabaseManager& operator=(DatabaseManager&&) = delete;

    [[nodiscard]] domain::Result<void> initialize(const std::filesystem::path& migrationsDir);
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] QSqlDatabase connection() const;
    [[nodiscard]] const QString& connectionName() const noexcept;
    [[nodiscard]] const std::string& activeJournalMode() const noexcept;

private:
    [[nodiscard]] domain::Result<void> configurePragmas(QSqlDatabase& db);
    [[nodiscard]] domain::Result<void> applyMigrations(
        QSqlDatabase& db,
        const std::filesystem::path& migrationsDir
    );

    std::filesystem::path databasePath_;
    QString connectionName_;
    std::string activeJournalMode_{"unknown"};
    bool isOpen_{false};
};

} // namespace vamp9::pos::infrastructure
