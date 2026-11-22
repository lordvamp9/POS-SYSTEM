#pragma once

#include <QFrame>
#include <QString>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;

namespace vamp9::pos::presentation {

class ScaleWidget final : public QFrame {
    Q_OBJECT

public:
    explicit ScaleWidget(QWidget* parent = nullptr);
    ~ScaleWidget() override = default;

    [[nodiscard]] double currentWeightKg() const noexcept;
    void setWeightKg(double weightKg);
    void resetWeight();
    void retranslateUi();

signals:
    void weightChanged(double weightKg);

private:
    void handleWeighOrTareClicked();

    QLabel* prefixLabel_{nullptr};
    QLabel* valueLabel_{nullptr};
    QPushButton* actionBtn_{nullptr};
    double weightKg_{0.0};
    int simStep_{0};
};

class TopBar final : public QFrame {
    Q_OBJECT

public:
    explicit TopBar(
        const QString& cashierInitial = QStringLiteral("C"),
        QWidget* parent = nullptr
    );
    ~TopBar() override = default;

    void setActiveTab(int index);
    void setCheckoutToolsVisible(bool visible);
    void resetScaleWeight();
    void retranslateUi();
    void refreshThemeVisuals();

signals:
    void navigationRequested(int pageIndex);
    void scaleWeightChanged(double weightKg);
    void searchTextChanged(const QString& query);
    void searchSubmitted(const QString& query);

private:
    void setupUi(const QString& cashierInitial);
    void updateGearIcon();

    QButtonGroup* navGroup_{nullptr};
    QPushButton* posTab_{nullptr};
    QPushButton* invTab_{nullptr};
    QPushButton* repTab_{nullptr};
    QPushButton* settingsGearBtn_{nullptr};
    ScaleWidget* scaleWidget_{nullptr};
    QLineEdit* searchInput_{nullptr};
    QLabel* cashierLabel_{nullptr};
};

} // namespace vamp9::pos::presentation
