#pragma once

#include <QWidget>

class QButtonGroup;
class QLabel;
class QPushButton;

namespace vamp9::pos::presentation {

// Commercial settings view: Theme (Light / Dark), Language (English / Spanish), and Display Mode (F11 / Alt+Enter).
class SettingsView final : public QWidget {
    Q_OBJECT

public:
    explicit SettingsView(QWidget* parent = nullptr);
    ~SettingsView() override = default;

public slots:
    void retranslateUi();
    void setFullscreenState(bool fullscreen);

signals:
    void fullscreenToggleRequested();

private:
    void setupUi();
    void syncSelectionsFromPreferences();

    QLabel* titleLabel_{nullptr};
    QLabel* subtitleLabel_{nullptr};

    QLabel* themeHeadingLabel_{nullptr};
    QLabel* themeDescLabel_{nullptr};
    QPushButton* lightThemeBtn_{nullptr};
    QPushButton* darkThemeBtn_{nullptr};
    QButtonGroup* themeGroup_{nullptr};

    QLabel* langHeadingLabel_{nullptr};
    QLabel* langDescLabel_{nullptr};
    QPushButton* englishBtn_{nullptr};
    QPushButton* spanishBtn_{nullptr};
    QButtonGroup* langGroup_{nullptr};

    QLabel* displayHeadingLabel_{nullptr};
    QLabel* displayDescLabel_{nullptr};
    QPushButton* fullscreenBtn_{nullptr};
    bool isFullscreen_{false};
};

} // namespace vamp9::pos::presentation
