#pragma once

#include <QObject>
#include <QString>

namespace vamp9::pos::presentation {

enum class AppTheme {
    Light,
    Dark
};

enum class AppLanguage {
    English,
    Spanish
};

// Centralized persistent UI preferences (Light/Dark theme + English/Spanish i18n).
// English is the primary default language.
class AppPreferences final : public QObject {
    Q_OBJECT

public:
    static AppPreferences& instance();

    [[nodiscard]] AppTheme theme() const noexcept;
    [[nodiscard]] bool isDark() const noexcept;
    void setTheme(AppTheme theme);

    [[nodiscard]] AppLanguage language() const noexcept;
    [[nodiscard]] bool isEnglish() const noexcept;
    void setLanguage(AppLanguage language);

    // Lightweight inline translation helper (English primary, Spanish secondary).
    [[nodiscard]] static QString trText(const QString& english, const QString& spanish);

signals:
    void themeChanged(AppTheme theme);
    void languageChanged(AppLanguage language);

private:
    AppPreferences();
    void loadFromDisk();
    void saveToDisk() const;

    AppTheme theme_{AppTheme::Light};
    AppLanguage language_{AppLanguage::English};
};

} // namespace vamp9::pos::presentation
