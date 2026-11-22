#include "presentation/views/SettingsView.h"

#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/TranslationManager.h"
#include "presentation/widgets/ToastManager.h"

#include <QButtonGroup>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace vamp9::pos::presentation {

SettingsView::SettingsView(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    syncSelectionsFromPreferences();
    retranslateUi();

    auto& prefs = AppPreferences::instance();
    connect(&prefs, &AppPreferences::themeChanged, this, [this](AppTheme) {
        syncSelectionsFromPreferences();
    });
    connect(&prefs, &AppPreferences::languageChanged, this, [this](AppLanguage) {
        syncSelectionsFromPreferences();
        retranslateUi();
    });
}

void SettingsView::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 12, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("settingsCard"));
    card->setProperty("surface", QStringLiteral("panel"));
    card->setMinimumWidth(560);
    card->setMaximumWidth(660);

    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(14.0);
    shadow->setOffset(0.0, 2.0);
    shadow->setColor(QColor(15, 23, 42, 16));
    card->setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(28, 26, 28, 28);
    layout->setSpacing(20);

    // Header
    auto* headerBox = new QVBoxLayout();
    headerBox->setSpacing(4);
    titleLabel_ = new QLabel(card);
    titleLabel_->setObjectName(QStringLiteral("settingsTitle"));
    subtitleLabel_ = new QLabel(card);
    subtitleLabel_->setObjectName(QStringLiteral("mutedText"));
    headerBox->addWidget(titleLabel_);
    headerBox->addWidget(subtitleLabel_);
    layout->addLayout(headerBox);

    auto* sep1 = new QFrame(card);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName(QStringLiteral("summaryDivider"));
    layout->addWidget(sep1);

    // ------------------------------------------------------------------------
    // 1. Theme Selection (Light Mode / Dark Mode)
    // ------------------------------------------------------------------------
    auto* themeSection = new QVBoxLayout();
    themeSection->setSpacing(10);

    themeHeadingLabel_ = new QLabel(card);
    themeHeadingLabel_->setObjectName(QStringLiteral("sectionTitle"));
    themeDescLabel_ = new QLabel(card);
    themeDescLabel_->setObjectName(QStringLiteral("mutedText"));

    auto* themeRow = new QHBoxLayout();
    themeRow->setSpacing(12);

    themeGroup_ = new QButtonGroup(this);
    themeGroup_->setExclusive(true);

    lightThemeBtn_ = new QPushButton(card);
    lightThemeBtn_->setProperty("settingChoice", true);
    lightThemeBtn_->setCheckable(true);
    lightThemeBtn_->setCursor(Qt::PointingHandCursor);
    lightThemeBtn_->setMinimumHeight(50);

    darkThemeBtn_ = new QPushButton(card);
    darkThemeBtn_->setProperty("settingChoice", true);
    darkThemeBtn_->setCheckable(true);
    darkThemeBtn_->setCursor(Qt::PointingHandCursor);
    darkThemeBtn_->setMinimumHeight(50);

    themeGroup_->addButton(lightThemeBtn_, static_cast<int>(AppTheme::Light));
    themeGroup_->addButton(darkThemeBtn_, static_cast<int>(AppTheme::Dark));

    themeRow->addWidget(lightThemeBtn_, 1);
    themeRow->addWidget(darkThemeBtn_, 1);

    themeSection->addWidget(themeHeadingLabel_);
    themeSection->addWidget(themeDescLabel_);
    themeSection->addLayout(themeRow);
    layout->addLayout(themeSection);

    auto* sep2 = new QFrame(card);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setObjectName(QStringLiteral("summaryDivider"));
    layout->addWidget(sep2);

    // ------------------------------------------------------------------------
    // 2. Language Selection (English [Primary] / Spanish)
    // ------------------------------------------------------------------------
    auto* langSection = new QVBoxLayout();
    langSection->setSpacing(10);

    langHeadingLabel_ = new QLabel(card);
    langHeadingLabel_->setObjectName(QStringLiteral("sectionTitle"));
    langDescLabel_ = new QLabel(card);
    langDescLabel_->setObjectName(QStringLiteral("mutedText"));

    auto* langRow = new QHBoxLayout();
    langRow->setSpacing(12);

    langGroup_ = new QButtonGroup(this);
    langGroup_->setExclusive(true);

    englishBtn_ = new QPushButton(QStringLiteral("English"), card);
    englishBtn_->setProperty("settingChoice", true);
    englishBtn_->setCheckable(true);
    englishBtn_->setCursor(Qt::PointingHandCursor);
    englishBtn_->setMinimumHeight(50);

    spanishBtn_ = new QPushButton(QStringLiteral("Español"), card);
    spanishBtn_->setProperty("settingChoice", true);
    spanishBtn_->setCheckable(true);
    spanishBtn_->setCursor(Qt::PointingHandCursor);
    spanishBtn_->setMinimumHeight(50);

    langGroup_->addButton(englishBtn_, static_cast<int>(AppLanguage::English));
    langGroup_->addButton(spanishBtn_, static_cast<int>(AppLanguage::Spanish));

    langRow->addWidget(englishBtn_, 1);
    langRow->addWidget(spanishBtn_, 1);

    langSection->addWidget(langHeadingLabel_);
    langSection->addWidget(langDescLabel_);
    langSection->addLayout(langRow);
    layout->addLayout(langSection);

    auto* sep3 = new QFrame(card);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setObjectName(QStringLiteral("summaryDivider"));
    layout->addWidget(sep3);

    // ------------------------------------------------------------------------
    // 3. Display & Borderless Fullscreen Mode (F11 / Alt + Enter)
    // ------------------------------------------------------------------------
    auto* displaySection = new QVBoxLayout();
    displaySection->setSpacing(10);

    displayHeadingLabel_ = new QLabel(card);
    displayHeadingLabel_->setObjectName(QStringLiteral("sectionTitle"));
    displayDescLabel_ = new QLabel(card);
    displayDescLabel_->setObjectName(QStringLiteral("mutedText"));

    fullscreenBtn_ = new QPushButton(card);
    fullscreenBtn_->setProperty("settingChoice", true);
    fullscreenBtn_->setCheckable(true);
    fullscreenBtn_->setCursor(Qt::PointingHandCursor);
    fullscreenBtn_->setMinimumHeight(48);

    displaySection->addWidget(displayHeadingLabel_);
    displaySection->addWidget(displayDescLabel_);
    displaySection->addWidget(fullscreenBtn_);
    layout->addLayout(displaySection);

    outerLayout->addWidget(card);

    connect(lightThemeBtn_, &QPushButton::clicked, this, []() {
        AppPreferences::instance().setTheme(AppTheme::Light);
        ToastManager::showInfo(
            TranslationManager::text(QStringLiteral("Appearance Updated"), QStringLiteral("Tema Actualizado")),
            TranslationManager::text(QStringLiteral("Switched to Light Mode."), QStringLiteral("Modo Claro activado."))
        );
    });
    connect(darkThemeBtn_, &QPushButton::clicked, this, []() {
        AppPreferences::instance().setTheme(AppTheme::Dark);
        ToastManager::showInfo(
            TranslationManager::text(QStringLiteral("Appearance Updated"), QStringLiteral("Tema Actualizado")),
            TranslationManager::text(QStringLiteral("Switched to Dark Mode."), QStringLiteral("Modo Oscuro activado."))
        );
    });
    connect(englishBtn_, &QPushButton::clicked, this, []() {
        TranslationManager::instance().applyLanguage(AppLanguage::English);
        ToastManager::showInfo(
            QStringLiteral("Language Updated"),
            QStringLiteral("Interface switched to English.")
        );
    });
    connect(spanishBtn_, &QPushButton::clicked, this, []() {
        TranslationManager::instance().applyLanguage(AppLanguage::Spanish);
        ToastManager::showInfo(
            QStringLiteral("Idioma Actualizado"),
            QStringLiteral("Interfaz cambiada a Español.")
        );
    });
    connect(fullscreenBtn_, &QPushButton::clicked, this, &SettingsView::fullscreenToggleRequested);
}

void SettingsView::setFullscreenState(bool fullscreen) {
    isFullscreen_ = fullscreen;
    if (fullscreenBtn_) {
        fullscreenBtn_->setChecked(isFullscreen_);
    }
    retranslateUi();
}

void SettingsView::syncSelectionsFromPreferences() {
    const auto& prefs = AppPreferences::instance();
    if (lightThemeBtn_ && darkThemeBtn_) {
        lightThemeBtn_->setChecked(!prefs.isDark());
        darkThemeBtn_->setChecked(prefs.isDark());
    }
    if (englishBtn_ && spanishBtn_) {
        englishBtn_->setChecked(prefs.isEnglish());
        spanishBtn_->setChecked(!prefs.isEnglish());
    }
}

void SettingsView::retranslateUi() {
    auto tr = &TranslationManager::text;
    if (titleLabel_) {
        titleLabel_->setText(tr(QStringLiteral("Settings"), QStringLiteral("Configuración")));
    }
    if (subtitleLabel_) {
        subtitleLabel_->setText(tr(
            QStringLiteral("Customize terminal appearance, display mode, and interface language."),
            QStringLiteral("Personaliza el tema visual, modo de pantalla e idioma de la interfaz.")
        ));
    }
    if (themeHeadingLabel_) {
        themeHeadingLabel_->setText(tr(QStringLiteral("Appearance"), QStringLiteral("Tema Visual")));
    }
    if (themeDescLabel_) {
        themeDescLabel_->setText(tr(
            QStringLiteral("Choose between Light and Dark mode for your checkout terminal."),
            QStringLiteral("Elige entre modo claro y modo oscuro para la terminal de caja.")
        ));
    }
    if (lightThemeBtn_) {
        lightThemeBtn_->setText(tr(QStringLiteral("Light Mode"), QStringLiteral("Modo Claro")));
    }
    if (darkThemeBtn_) {
        darkThemeBtn_->setText(tr(QStringLiteral("Dark Mode"), QStringLiteral("Modo Oscuro")));
    }
    if (langHeadingLabel_) {
        langHeadingLabel_->setText(tr(QStringLiteral("Language"), QStringLiteral("Idioma")));
    }
    if (langDescLabel_) {
        langDescLabel_->setText(tr(
            QStringLiteral("Switch interface language dynamically in real time (English / Español)."),
            QStringLiteral("Cambia dinámicamente el idioma de la aplicación en tiempo real (English / Español).")
        ));
    }
    if (displayHeadingLabel_) {
        displayHeadingLabel_->setText(tr(QStringLiteral("Display Mode"), QStringLiteral("Modo de Pantalla")));
    }
    if (displayDescLabel_) {
        displayDescLabel_->setText(tr(
            QStringLiteral("Toggle borderless kiosk fullscreen anytime using F11 or Alt + Enter."),
            QStringLiteral("Alterna pantalla completa sin bordes en cualquier momento usando F11 o Alt + Enter.")
        ));
    }
    if (fullscreenBtn_) {
        fullscreenBtn_->setText(
            isFullscreen_
                ? tr(QStringLiteral("Exit Borderless Fullscreen (F11 / Alt + Enter)"),
                     QStringLiteral("Salir de Pantalla Completa (F11 / Alt + Enter)"))
                : tr(QStringLiteral("Enter Borderless Fullscreen (F11 / Alt + Enter)"),
                     QStringLiteral("Activar Pantalla Completa Sin Bordes (F11 / Alt + Enter)"))
        );
    }
}

} // namespace vamp9::pos::presentation
