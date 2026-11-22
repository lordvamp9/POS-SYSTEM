#include "presentation/utils/AppPreferences.h"

#include <QSettings>

namespace vamp9::pos::presentation {

AppPreferences& AppPreferences::instance() {
    static AppPreferences sInstance;
    return sInstance;
}

AppPreferences::AppPreferences() {
    loadFromDisk();
}

AppTheme AppPreferences::theme() const noexcept {
    return theme_;
}

bool AppPreferences::isDark() const noexcept {
    return theme_ == AppTheme::Dark;
}

void AppPreferences::setTheme(AppTheme theme) {
    if (theme_ == theme) {
        return;
    }
    theme_ = theme;
    saveToDisk();
    emit themeChanged(theme_);
}

AppLanguage AppPreferences::language() const noexcept {
    return language_;
}

bool AppPreferences::isEnglish() const noexcept {
    return language_ == AppLanguage::English;
}

void AppPreferences::setLanguage(AppLanguage language) {
    if (language_ == language) {
        return;
    }
    language_ = language;
    saveToDisk();
    emit languageChanged(language_);
}

QString AppPreferences::trText(const QString& english, const QString& spanish) {
    return instance().isEnglish() ? english : spanish;
}

void AppPreferences::loadFromDisk() {
    QSettings settings(QStringLiteral("vamp9"), QStringLiteral("vamp9POS"));
    const QString themeStr = settings.value(QStringLiteral("ui/theme"), QStringLiteral("light")).toString();
    theme_ = (themeStr.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0)
        ? AppTheme::Dark
        : AppTheme::Light;

    // English is the primary default language when no preference is saved yet.
    const QString langStr = settings.value(QStringLiteral("ui/language"), QStringLiteral("en")).toString();
    language_ = (langStr.compare(QStringLiteral("es"), Qt::CaseInsensitive) == 0)
        ? AppLanguage::Spanish
        : AppLanguage::English;
}

void AppPreferences::saveToDisk() const {
    QSettings settings(QStringLiteral("vamp9"), QStringLiteral("vamp9POS"));
    settings.setValue(
        QStringLiteral("ui/theme"),
        theme_ == AppTheme::Dark ? QStringLiteral("dark") : QStringLiteral("light")
    );
    settings.setValue(
        QStringLiteral("ui/language"),
        language_ == AppLanguage::Spanish ? QStringLiteral("es") : QStringLiteral("en")
    );
}

} // namespace vamp9::pos::presentation
