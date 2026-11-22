#pragma once

#include "presentation/utils/AppPreferences.h"

#include <QObject>
#include <QString>
#include <QTranslator>

namespace vamp9::pos::presentation {

// Coordinates live EN/ES translation switching using Qt Linguist (.qm) files when available,
// seamlessly backed by deterministic bilingual string resolution via AppPreferences.
class TranslationManager final : public QObject {
    Q_OBJECT

public:
    static TranslationManager& instance();

    void applyLanguage(AppLanguage language);
    [[nodiscard]] AppLanguage currentLanguage() const noexcept;

    [[nodiscard]] static QString text(const QString& english, const QString& spanish);

signals:
    void languageApplied(AppLanguage language);

private:
    TranslationManager() = default;

    QTranslator translator_;
    bool translatorInstalled_{false};
};

} // namespace vamp9::pos::presentation
