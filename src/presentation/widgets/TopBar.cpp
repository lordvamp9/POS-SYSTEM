#include "presentation/widgets/TopBar.h"

#include "presentation/utils/AppPreferences.h"

#include <QAction>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QSvgRenderer>

#include <algorithm>
#include <array>

namespace vamp9::pos::presentation {

namespace {

QPixmap renderSvgPixmap(const QByteArray& svg, const QSize& logicalSize) {
    QPixmap pix(logicalSize * 2);
    pix.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, QRectF(0.0, 0.0, pix.width(), pix.height()));
    painter.end();
    pix.setDevicePixelRatio(2.0);
    return pix;
}

QIcon buildGearIcon(const QString& strokeColorHex) {
    // Classic clean 8-tooth mechanical gear / cog SVG icon
    const QByteArray svg = QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
        "stroke='%1' stroke-width='1.9' stroke-linecap='round' stroke-linejoin='round'>"
        "<circle cx='12' cy='12' r='3.2'/>"
        "<path d='M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06"
        "a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09"
        "A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06"
        "a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09"
        "A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06"
        "a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09"
        "a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06"
        "a1.65 1.65 0 0 0-.33 1.82V9c0 .69.4 1.32 1.03 1.6.16.07.33.11.51.11H21a2 2 0 1 1 0 4h-.09"
        "a1.65 1.65 0 0 0-1.51 1z'/>"
        "</svg>"
    ).arg(strokeColorHex).toUtf8();
    return QIcon(renderSvgPixmap(svg, QSize(18, 18)));
}

QIcon buildSearchIcon(const QString& strokeColorHex) {
    const QByteArray svg = QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
        "stroke='%1' stroke-width='2.1' stroke-linecap='round' stroke-linejoin='round'>"
        "<circle cx='11' cy='11' r='7'/>"
        "<line x1='20' y1='20' x2='16.2' y2='16.2'/>"
        "</svg>"
    ).arg(strokeColorHex).toUtf8();
    return QIcon(renderSvgPixmap(svg, QSize(15, 15)));
}

QPixmap loadBrandIcon(const QSize& size) {
    const QStringList candidatePaths = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/assets/icons/vamp9_brand.png"),
        QStringLiteral("assets/icons/vamp9_brand.png")
    };

    for (const QString& path : candidatePaths) {
        if (QFile::exists(path)) {
            QPixmap loaded(path);
            if (!loaded.isNull()) {
                QPixmap scaled = loaded.scaled(size * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                scaled.setDevicePixelRatio(2.0);
                return scaled;
            }
        }
    }

    const QByteArray svg =
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 36 36'>"
        "<defs><linearGradient id='g' x1='0' y1='0' x2='1' y2='1'>"
        "<stop offset='0%' stop-color='#2563EB'/><stop offset='100%' stop-color='#4338CA'/>"
        "</linearGradient></defs>"
        "<rect x='2' y='2' width='32' height='32' rx='8' fill='url(#g)'/>"
        "<path d='M10 12 L18 24 L26 12' fill='none' stroke='#FFFFFF' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'/>"
        "</svg>";
    return renderSvgPixmap(svg, size);
}

} // namespace

// ============================================================================
// ScaleWidget — Crisp 3-decimal tabular digital retail scale readout
// ============================================================================
ScaleWidget::ScaleWidget(QWidget* parent)
    : QFrame(parent) {
    setObjectName(QStringLiteral("scaleCapsule"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 4, 6, 4);
    layout->setSpacing(6);

    prefixLabel_ = new QLabel(this);
    prefixLabel_->setObjectName(QStringLiteral("scalePrefixLabel"));

    valueLabel_ = new QLabel(QStringLiteral("0.000"), this);
    valueLabel_->setObjectName(QStringLiteral("scaleValueLabel"));
    valueLabel_->setMinimumWidth(62);
    valueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* unitLabel = new QLabel(QStringLiteral("kg"), this);
    unitLabel->setObjectName(QStringLiteral("scaleUnitLabel"));

    actionBtn_ = new QPushButton(this);
    actionBtn_->setObjectName(QStringLiteral("scaleActionBtn"));
    actionBtn_->setCursor(Qt::PointingHandCursor);
    actionBtn_->setFocusPolicy(Qt::NoFocus);

    layout->addWidget(prefixLabel_);
    layout->addWidget(valueLabel_);
    layout->addWidget(unitLabel);
    layout->addSpacing(4);
    layout->addWidget(actionBtn_);

    retranslateUi();
    connect(actionBtn_, &QPushButton::clicked, this, &ScaleWidget::handleWeighOrTareClicked);
}

double ScaleWidget::currentWeightKg() const noexcept {
    return weightKg_;
}

void ScaleWidget::setWeightKg(double weightKg) {
    weightKg_ = std::max(0.0, weightKg);
    if (valueLabel_) {
        valueLabel_->setText(QString::number(weightKg_, 'f', 3));
        valueLabel_->setProperty("hasWeight", weightKg_ > 0.0005);
        valueLabel_->style()->unpolish(valueLabel_);
        valueLabel_->style()->polish(valueLabel_);
    }
    retranslateUi();
}

void ScaleWidget::resetWeight() {
    simStep_ = 0;
    setWeightKg(0.0);
    emit weightChanged(0.0);
}

void ScaleWidget::retranslateUi() {
    auto tr = &AppPreferences::trText;
    if (prefixLabel_) {
        prefixLabel_->setText(tr(QStringLiteral("Weight:"), QStringLiteral("Peso:")));
    }
    if (actionBtn_) {
        actionBtn_->setText(
            weightKg_ > 0.0005
                ? tr(QStringLiteral("Tare"), QStringLiteral("Tarar"))
                : tr(QStringLiteral("Weigh"), QStringLiteral("Pesar"))
        );
        actionBtn_->setToolTip(tr(
            QStringLiteral("Weigh bulk item or tare scale back to zero"),
            QStringLiteral("Pesar producto a granel o tarar balanza a cero")
        ));
    }
}

void ScaleWidget::handleWeighOrTareClicked() {
    if (weightKg_ > 0.0005) {
        setWeightKg(0.0);
        emit weightChanged(0.0);
        return;
    }

    static constexpr std::array<double, 5> kRealisticWeightsKg = {0.450, 0.850, 0.500, 1.250, 0.625};
    const double nextWeight = kRealisticWeightsKg[static_cast<std::size_t>(simStep_ % static_cast<int>(kRealisticWeightsKg.size()))];
    ++simStep_;
    setWeightKg(nextWeight);
    emit weightChanged(weightKg_);
}

// ============================================================================
// TopBar — Commercial NCR / GK Software / Odoo POS Minimalist Header
// ============================================================================
TopBar::TopBar(const QString& cashierInitial, QWidget* parent)
    : QFrame(parent) {
    setObjectName(QStringLiteral("topBar"));
    setFixedHeight(58);
    setupUi(cashierInitial);
    retranslateUi();
    updateGearIcon();
}

void TopBar::setupUi(const QString& cashierInitial) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(12);

    // ------------------------------------------------------------------------
    // Left Zone: Custom Logo + Borderless Tabs ONLY for Checkout / Inventory / Reports
    // ------------------------------------------------------------------------
    auto* logoIcon = new QLabel(this);
    logoIcon->setPixmap(loadBrandIcon(QSize(30, 30)));
    logoIcon->setFixedSize(30, 30);

    auto* brandTitle = new QLabel(QStringLiteral("vamp9 POS"), this);
    brandTitle->setObjectName(QStringLiteral("brandTitle"));

    layout->addWidget(logoIcon);
    layout->addWidget(brandTitle);
    layout->addSpacing(10);

    auto* separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("topBarDivider"));
    separator->setFrameShape(QFrame::VLine);
    separator->setFixedHeight(22);
    layout->addWidget(separator);
    layout->addSpacing(2);

    navGroup_ = new QButtonGroup(this);
    navGroup_->setExclusive(true);

    auto makeNavTab = [this](int index) {
        auto* btn = new QPushButton(this);
        btn->setProperty("topTab", true);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setFixedHeight(56);
        navGroup_->addButton(btn, index);
        return btn;
    };

    posTab_ = makeNavTab(0);
    invTab_ = makeNavTab(1);
    repTab_ = makeNavTab(2);
    posTab_->setChecked(true);

    layout->addWidget(posTab_);
    layout->addWidget(invTab_);
    layout->addWidget(repTab_);

    // ------------------------------------------------------------------------
    // Center Zone: Bright 0.000 kg Scale Display + Quick SKU/Product Search
    // (Zero technical badges, zero embedded status alerts)
    // ------------------------------------------------------------------------
    layout->addStretch(1);

    scaleWidget_ = new ScaleWidget(this);
    layout->addWidget(scaleWidget_);
    connect(scaleWidget_, &ScaleWidget::weightChanged, this, &TopBar::scaleWeightChanged);

    searchInput_ = new QLineEdit(this);
    searchInput_->setObjectName(QStringLiteral("topSearchInput"));
    searchInput_->setClearButtonEnabled(true);
    searchInput_->setFocusPolicy(Qt::ClickFocus);
    searchInput_->setMinimumWidth(275);
    searchInput_->setMaximumWidth(350);
    searchInput_->addAction(buildSearchIcon(QStringLiteral("#94A3B8")), QLineEdit::LeadingPosition);
    layout->addWidget(searchInput_);

    connect(searchInput_, &QLineEdit::textChanged, this, &TopBar::searchTextChanged);
    connect(searchInput_, &QLineEdit::returnPressed, this, [this]() {
        if (!searchInput_) {
            return;
        }
        const QString q = searchInput_->text().trimmed();
        if (!q.isEmpty()) {
            emit searchSubmitted(q);
            searchInput_->clear();
        }
    });

    layout->addStretch(1);

    // ------------------------------------------------------------------------
    // Right Zone: Compact Terminal Profile + Classic SVG Gear Settings Button
    // ------------------------------------------------------------------------
    auto* profileContainer = new QWidget(this);
    auto* profileLayout = new QHBoxLayout(profileContainer);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    profileLayout->setSpacing(9);

    auto* avatarBadge = new QLabel(cashierInitial, profileContainer);
    avatarBadge->setObjectName(QStringLiteral("cashierAvatar"));
    avatarBadge->setFixedSize(26, 26);
    avatarBadge->setAlignment(Qt::AlignCenter);

    cashierLabel_ = new QLabel(profileContainer);
    cashierLabel_->setObjectName(QStringLiteral("cashierNameLabel"));

    profileLayout->addWidget(avatarBadge);
    profileLayout->addWidget(cashierLabel_);
    layout->addWidget(profileContainer);

    auto* rightDivider = new QFrame(this);
    rightDivider->setObjectName(QStringLiteral("topBarDivider"));
    rightDivider->setFrameShape(QFrame::VLine);
    rightDivider->setFixedHeight(20);
    layout->addWidget(rightDivider);

    // Subtle 34x34px Settings Gear Icon button wired to QStackedWidget index 3
    settingsGearBtn_ = new QPushButton(this);
    settingsGearBtn_->setObjectName(QStringLiteral("settingsGearBtn"));
    settingsGearBtn_->setCheckable(true);
    settingsGearBtn_->setFixedSize(34, 34);
    settingsGearBtn_->setIconSize(QSize(18, 18));
    settingsGearBtn_->setCursor(Qt::PointingHandCursor);
    settingsGearBtn_->setFocusPolicy(Qt::NoFocus);
    navGroup_->addButton(settingsGearBtn_, 3);
    layout->addWidget(settingsGearBtn_);

    connect(navGroup_, &QButtonGroup::idClicked, this, [this](int index) {
        updateGearIcon();
        emit navigationRequested(index);
    });
}

void TopBar::setActiveTab(int index) {
    if (navGroup_) {
        if (auto* btn = navGroup_->button(index)) {
            btn->setChecked(true);
        }
    }
    updateGearIcon();
}

void TopBar::setCheckoutToolsVisible(bool visible) {
    if (scaleWidget_) {
        scaleWidget_->setVisible(visible);
    }
    if (searchInput_) {
        searchInput_->setVisible(visible);
    }
}

void TopBar::resetScaleWeight() {
    if (scaleWidget_) {
        scaleWidget_->resetWeight();
    }
}

void TopBar::retranslateUi() {
    auto tr = &AppPreferences::trText;
    if (posTab_) {
        posTab_->setText(tr(QStringLiteral("Checkout"), QStringLiteral("Caja")));
    }
    if (invTab_) {
        invTab_->setText(tr(QStringLiteral("Inventory"), QStringLiteral("Inventario")));
    }
    if (repTab_) {
        repTab_->setText(tr(QStringLiteral("Reports"), QStringLiteral("Reportes")));
    }
    if (settingsGearBtn_) {
        settingsGearBtn_->setToolTip(tr(
            QStringLiteral("Settings (Appearance, Language & Display)"),
            QStringLiteral("Configuración (Tema, Idioma y Pantalla)")
        ));
    }
    if (searchInput_) {
        searchInput_->setPlaceholderText(tr(
            QStringLiteral("Search product, SKU or barcode..."),
            QStringLiteral("Buscar producto, SKU o código...")
        ));
    }
    if (cashierLabel_) {
        cashierLabel_->setText(tr(
            QStringLiteral("Register 01 · AM Shift"),
            QStringLiteral("Caja 01 · Turno AM")
        ));
    }
    if (scaleWidget_) {
        scaleWidget_->retranslateUi();
    }
}

void TopBar::refreshThemeVisuals() {
    updateGearIcon();
}

void TopBar::updateGearIcon() {
    if (!settingsGearBtn_) {
        return;
    }
    const bool dark = AppPreferences::instance().isDark();
    const bool active = settingsGearBtn_->isChecked();
    QString strokeHex;
    if (active) {
        strokeHex = dark ? QStringLiteral("#FFFFFF") : QStringLiteral("#2563EB");
    } else {
        strokeHex = dark ? QStringLiteral("#CBD5E1") : QStringLiteral("#475569");
    }
    settingsGearBtn_->setIcon(buildGearIcon(strokeHex));
}

} // namespace vamp9::pos::presentation
