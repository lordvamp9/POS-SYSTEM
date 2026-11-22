#include "presentation/utils/TranslationManager.h"

#include <QCoreApplication>
#include <QDir>

namespace vamp9::pos::presentation {

TranslationManager& TranslationManager::instance() {
    static TranslationManager sInstance;
    return sInstance;
}

void TranslationManager::applyLanguage(AppLanguage language) {
    if (qApp && translatorInstalled_) {
        qApp->removeTranslator(&translator_);
        translatorInstalled_ = false;
    }

    const QString localeCode = (language == AppLanguage::Spanish)
        ? QStringLiteral("es")
        : QStringLiteral("en");

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("assets/translations/vamp9_pos_%1.qm").arg(localeCode)),
        QDir::current().filePath(QStringLiteral("assets/translations/vamp9_pos_%1.qm").arg(localeCode))
    };

    for (const QString& qmPath : candidates) {
        if (translator_.load(qmPath)) {
            if (qApp) {
                qApp->installTranslator(&translator_);
                translatorInstalled_ = true;
            }
            break;
        }
    }

    AppPreferences::instance().setLanguage(language);
    emit languageApplied(language);
}

AppLanguage TranslationManager::currentLanguage() const noexcept {
    return AppPreferences::instance().language();
}

QString TranslationManager::text(const QString& english, const QString& spanish) {
    return AppPreferences::trText(english, spanish);
}

} // namespace vamp9::pos::presentation
