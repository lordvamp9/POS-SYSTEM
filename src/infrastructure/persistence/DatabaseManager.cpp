#include "infrastructure/persistence/DatabaseManager.h"

#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

#include <algorithm>
#include <utility>
#include <vector>

namespace vamp9::pos::infrastructure {

namespace {

[[nodiscard]] std::vector<QString> splitSqlStatements(const QString& sqlScript) {
    std::vector<QString> statements;
    QString current;
    bool inSingleQuote = false;

    const QStringList lines = sqlScript.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("--"))) {
            continue;
        }

        for (const QChar ch : rawLine) {
            if (ch == QLatin1Char('\'')) {
                inSingleQuote = !inSingleQuote;
            }
            if (ch == QLatin1Char(';') && !inSingleQuote) {
                const QString stmt = current.trimmed();
                if (!stmt.isEmpty()) {
                    statements.push_back(stmt);
                }
                current.clear();
            } else {
                current.append(ch);
            }
        }
        current.append(QLatin1Char('\n'));
    }

    const QString trailing = current.trimmed();
    if (!trailing.isEmpty()) {
        statements.push_back(trailing);
    }

    return statements;
}

} // namespace

DatabaseManager::DatabaseManager(std::filesystem::path databasePath, QString connectionName)
    : databasePath_(std::move(databasePath))
    , connectionName_(std::move(connectionName)) {}

DatabaseManager::~DatabaseManager() {
    if (QSqlDatabase::contains(connectionName_)) {
        {
            QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(connectionName_);
    }
    isOpen_ = false;
}

domain::Result<void> DatabaseManager::initialize(const std::filesystem::path& migrationsDir) {
    std::error_code fsError;
    if (databasePath_.has_parent_path()) {
        std::filesystem::create_directories(databasePath_.parent_path(), fsError);
        if (fsError) {
            return domain::Result<void>::fail(
                domain::ErrorCode::DatabaseError,
                "Failed to create database directory: " + fsError.message()
            );
        }
    }

    QSqlDatabase db = QSqlDatabase::contains(connectionName_)
        ? QSqlDatabase::database(connectionName_)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);

    db.setDatabaseName(QString::fromStdString(databasePath_.string()));

    if (!db.open()) {
        return domain::Result<void>::fail(
            domain::ErrorCode::DatabaseError,
            "Unable to open SQLite database: " + db.lastError().text().toStdString()
        );
    }

    isOpen_ = true;

    if (auto pragmaRes = configurePragmas(db); !pragmaRes) {
        return pragmaRes;
    }

    if (!migrationsDir.empty()) {
        if (auto migRes = applyMigrations(db, migrationsDir); !migRes) {
            return migRes;
        }
    }

    return domain::Result<void>::ok();
}

bool DatabaseManager::isOpen() const noexcept {
    return isOpen_;
}

QSqlDatabase DatabaseManager::connection() const {
    return QSqlDatabase::database(connectionName_, false);
}

const QString& DatabaseManager::connectionName() const noexcept {
    return connectionName_;
}

const std::string& DatabaseManager::activeJournalMode() const noexcept {
    return activeJournalMode_;
}

domain::Result<void> DatabaseManager::configurePragmas(QSqlDatabase& db) {
    QSqlQuery query(db);

    // Enable WAL mode for safe concurrent reads/writes and crash recovery.
    if (!query.exec(QStringLiteral("PRAGMA journal_mode = WAL;"))) {
        return domain::Result<void>::fail(
            domain::ErrorCode::DatabaseError,
            "Failed to enable WAL mode: " + query.lastError().text().toStdString()
        );
    }
    if (query.next()) {
        activeJournalMode_ = query.value(0).toString().toStdString();
    }

    const std::vector<QString> pragmas = {
        QStringLiteral("PRAGMA synchronous = FULL;"),
        QStringLiteral("PRAGMA foreign_keys = ON;"),
        QStringLiteral("PRAGMA busy_timeout = 5000;"),
        QStringLiteral("PRAGMA temp_store = MEMORY;")
    };

    for (const auto& pragma : pragmas) {
        if (!query.exec(pragma)) {
            return domain::Result<void>::fail(
                domain::ErrorCode::DatabaseError,
                "Failed to execute pragma (" + pragma.toStdString() + "): " +
                    query.lastError().text().toStdString()
            );
        }
    }

    return domain::Result<void>::ok();
}

domain::Result<void> DatabaseManager::applyMigrations(
    QSqlDatabase& db,
    const std::filesystem::path& migrationsDir
) {
    QSqlQuery query(db);
    const QString createTableSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "version TEXT PRIMARY KEY, "
        "applied_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))"
        ");"
    );

    if (!query.exec(createTableSql)) {
        return domain::Result<void>::fail(
            domain::ErrorCode::DatabaseError,
            "Failed to create schema_migrations table: " + query.lastError().text().toStdString()
        );
    }

    if (!std::filesystem::exists(migrationsDir)) {
        return domain::Result<void>::fail(
            domain::ErrorCode::NotFound,
            "Migrations directory does not exist: " + migrationsDir.string()
        );
    }

    std::vector<std::filesystem::path> migrationFiles;
    for (const auto& entry : std::filesystem::directory_iterator(migrationsDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sql") {
            migrationFiles.push_back(entry.path());
        }
    }
    std::sort(migrationFiles.begin(), migrationFiles.end());

    for (const auto& filePath : migrationFiles) {
        const QString version = QString::fromStdString(filePath.filename().string());

        QSqlQuery checkQuery(db);
        checkQuery.prepare(QStringLiteral("SELECT 1 FROM schema_migrations WHERE version = :version;"));
        checkQuery.bindValue(QStringLiteral(":version"), version);
        if (!checkQuery.exec()) {
            return domain::Result<void>::fail(
                domain::ErrorCode::DatabaseError,
                "Migration check failed: " + checkQuery.lastError().text().toStdString()
            );
        }
        if (checkQuery.next()) {
            continue;
        }

        QFile sqlFile(QString::fromStdString(filePath.string()));
        if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return domain::Result<void>::fail(
                domain::ErrorCode::DatabaseError,
                "Unable to read migration file: " + filePath.string()
            );
        }

        QTextStream in(&sqlFile);
        const QString script = in.readAll();
        const auto statements = splitSqlStatements(script);

        if (!db.transaction()) {
            return domain::Result<void>::fail(
                domain::ErrorCode::DatabaseError,
                "Unable to start transaction for migration " + version.toStdString()
            );
        }

        for (const auto& stmt : statements) {
            QSqlQuery execQuery(db);
            if (!execQuery.exec(stmt)) {
                const std::string err = execQuery.lastError().text().toStdString();
                db.rollback();
                return domain::Result<void>::fail(
                    domain::ErrorCode::DatabaseError,
                    "Migration " + version.toStdString() + " failed: " + err
                );
            }
        }

        QSqlQuery recordQuery(db);
        recordQuery.prepare(QStringLiteral("INSERT INTO schema_migrations (version) VALUES (:version);"));
        recordQuery.bindValue(QStringLiteral(":version"), version);
        if (!recordQuery.exec()) {
            const std::string err = recordQuery.lastError().text().toStdString();
            db.rollback();
            return domain::Result<void>::fail(
                domain::ErrorCode::DatabaseError,
                "Failed to record migration " + version.toStdString() + ": " + err
            );
        }

        if (!db.commit()) {
            const std::string err = db.lastError().text().toStdString();
            db.rollback();
            return domain::Result<void>::fail(
                domain::ErrorCode::DatabaseError,
                "Failed to commit migration " + version.toStdString() + ": " + err
            );
        }
    }

    return domain::Result<void>::ok();
}

} // namespace vamp9::pos::infrastructure
