#include "presentation/views/CheckoutView.h"

#include "domain/entities/Sale.h"
#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/CurrencyFormatter.h"
#include "presentation/widgets/ToastManager.h"

#include <QApplication>
#include <QButtonGroup>
#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace vamp9::pos::presentation {

namespace {

constexpr int kCatalogColumns = 5;

void applySurfaceShadow(QWidget* widget, qreal blur = 12.0, qreal yOffset = 2.0) {
    auto* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(blur);
    shadow->setOffset(0.0, yOffset);
    shadow->setColor(QColor(15, 23, 42, 14));
    widget->setGraphicsEffect(shadow);
}

QIcon buildSvgIcon(const QByteArray& svgBytes, const QSize& size = QSize(18, 18)) {
    QPixmap pix(size * 2);
    pix.fill(Qt::transparent);
    QSvgRenderer renderer(svgBytes);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0.0, 0.0, pix.width(), pix.height()));
    p.end();
    pix.setDevicePixelRatio(2.0);
    return QIcon(pix);
}

} // namespace

CheckoutView::CheckoutView(
    std::shared_ptr<application::CartService> cartService,
    std::shared_ptr<domain::IProductRepository> productRepository,
    std::shared_ptr<domain::IReceiptPrinter> receiptPrinter,
    QWidget* parent
)
    : QWidget(parent)
    , cartService_(std::move(cartService))
    , productRepository_(std::move(productRepository))
    , receiptPrinter_(std::move(receiptPrinter)) {
    initializeChileanCatalog();
    seedRepositoryIfEmpty();
    setupUi();
    retranslateUi();
    rebuildProductGrid();
    preloadSampleTicket();
}

void CheckoutView::setupUi() {
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(14);

    // Retail 38% Left (Operation & Payment) / 62% Right (Visual Touch Catalog)
    QWidget* leftPanel = buildLeftOperationPanel();
    QWidget* rightPanel = buildRightCatalogPanel();

    rootLayout->addWidget(leftPanel, 38);
    rootLayout->addWidget(rightPanel, 62);
}

QWidget* CheckoutView::buildLeftOperationPanel() {
    auto* panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("operationPanel"));
    panel->setProperty("surface", QStringLiteral("panel"));
    panel->setMinimumWidth(420);
    applySurfaceShadow(panel, 12.0, 2.0);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);

    // ------------------------------------------------------------------------
    // Header strip: Ticket title, item count, Boleta/Factura pill
    // ------------------------------------------------------------------------
    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(8);

    ticketTitleLabel_ = new QLabel(panel);
    ticketTitleLabel_->setObjectName(QStringLiteral("sectionTitle"));

    itemCountBadge_ = new QLabel(panel);
    itemCountBadge_->setProperty("pill", QStringLiteral("status-neutral"));

    docTypeBadge_ = new QLabel(panel);
    docTypeBadge_->setProperty("pill", QStringLiteral("status-ok"));

    headerRow->addWidget(ticketTitleLabel_);
    headerRow->addWidget(itemCountBadge_);
    headerRow->addStretch();
    headerRow->addWidget(docTypeBadge_);
    layout->addLayout(headerRow);

    // ------------------------------------------------------------------------
    // 1. Detailed Ticket List (QListWidget + custom 2-line TicketItemDelegate)
    // ------------------------------------------------------------------------
    ticketList_ = new QListWidget(panel);
    ticketList_->setObjectName(QStringLiteral("ticketList"));
    ticketList_->setItemDelegate(new TicketItemDelegate(ticketList_));
    ticketList_->setSelectionMode(QAbstractItemView::SingleSelection);
    ticketList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ticketList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ticketList_->setMouseTracking(true);
    ticketList_->setSpacing(2);
    layout->addWidget(ticketList_, 1);

    connect(ticketList_, &QListWidget::currentRowChanged, this, [this](int) {
        numpadBuffer_.clear();
        updateNumpadInputPreview();
    });

    // ------------------------------------------------------------------------
    // 2. Financial Summary Card (CLP without decimals: Neto, IVA 19%, TOTAL)
    // ------------------------------------------------------------------------
    auto* summaryCard = new QFrame(panel);
    summaryCard->setObjectName(QStringLiteral("summaryCard"));
    summaryCard->setProperty("glass", QStringLiteral("accent-card"));

    auto* summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(14, 10, 14, 10);
    summaryLayout->setSpacing(4);

    auto addRow = [summaryCard, summaryLayout](const QString& labelText, QLabel*& capOut, QLabel*& outLabel, const QString& labelObj, const QString& valObj) {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        capOut = new QLabel(labelText, summaryCard);
        capOut->setObjectName(labelObj);
        outLabel = new QLabel(QStringLiteral("$ 0"), summaryCard);
        outLabel->setObjectName(valObj);
        outLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(capOut);
        row->addStretch();
        row->addWidget(outLabel);
        summaryLayout->addLayout(row);
    };

    QLabel* totalCap = nullptr;
    addRow(QStringLiteral("Net"), netCaptionLabel_, netValueLabel_, QStringLiteral("summaryMutedLabel"), QStringLiteral("summarySubValue"));
    addRow(QStringLiteral("VAT (19%)"), ivaCaptionLabel_, ivaValueLabel_, QStringLiteral("summaryMutedLabel"), QStringLiteral("summarySubValue"));

    auto* divider = new QFrame(summaryCard);
    divider->setFrameShape(QFrame::HLine);
    divider->setObjectName(QStringLiteral("summaryDivider"));
    summaryLayout->addWidget(divider);

    addRow(QStringLiteral("TOTAL"), totalCap, totalValueLabel_, QStringLiteral("totalCaptionLabel"), QStringLiteral("totalAmountLabel"));

    numpadStatusLabel_ = new QLabel(QStringLiteral("Modo teclado: Ctdad"), summaryCard);
    numpadStatusLabel_->setObjectName(QStringLiteral("numpadStatusLabel"));
    numpadStatusLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    summaryLayout->addWidget(numpadStatusLabel_);

    layout->addWidget(summaryCard);

    // ------------------------------------------------------------------------
    // 3. Quick Actions Bar (2 rows x 3 buttons)
    // ------------------------------------------------------------------------
    auto* actionsGrid = new QGridLayout();
    actionsGrid->setContentsMargins(0, 0, 0, 0);
    actionsGrid->setSpacing(7);

    auto makeQuickBtn = [panel](const QString& text, const QString& variant = QStringLiteral("quick-action")) {
        auto* btn = new QPushButton(text, panel);
        btn->setProperty("variant", variant);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        return btn;
    };

    clearBtn_ = makeQuickBtn(QStringLiteral("Clear"), QStringLiteral("quick-danger"));
    refundBtn_ = makeQuickBtn(QStringLiteral("Refund"));
    refundBtn_->setCheckable(true);
    noteBtn_ = makeQuickBtn(QStringLiteral("Note / Receipt"));

    discountBtn_ = makeQuickBtn(QStringLiteral("Discount"));
    codeBtn_ = makeQuickBtn(QStringLiteral("Enter Code"));
    customerRutBtn_ = makeQuickBtn(QStringLiteral("Customer / Tax ID"));
    customerRutBtn_->setCheckable(true);

    actionsGrid->addWidget(clearBtn_, 0, 0);
    actionsGrid->addWidget(refundBtn_, 0, 1);
    actionsGrid->addWidget(noteBtn_, 0, 2);
    actionsGrid->addWidget(discountBtn_, 1, 0);
    actionsGrid->addWidget(codeBtn_, 1, 1);
    actionsGrid->addWidget(customerRutBtn_, 1, 2);

    layout->addLayout(actionsGrid);

    connect(clearBtn_, &QPushButton::clicked, this, &CheckoutView::handleClearTicket);
    connect(refundBtn_, &QPushButton::toggled, this, &CheckoutView::handleToggleRefundMode);
    connect(noteBtn_, &QPushButton::clicked, this, &CheckoutView::handleAttachNote);
    connect(discountBtn_, &QPushButton::clicked, this, &CheckoutView::handleQuickDiscount);
    connect(codeBtn_, &QPushButton::clicked, this, &CheckoutView::handleManualCodeEntry);
    connect(customerRutBtn_, &QPushButton::clicked, this, &CheckoutView::handleToggleDocumentType);

    // ------------------------------------------------------------------------
    // 4. Split Bottom Block: 30% Large Vertical "> Pago" + 70% 4x4 Touch Numpad
    // ------------------------------------------------------------------------
    auto* bottomBlock = new QHBoxLayout();
    bottomBlock->setContentsMargins(0, 0, 0, 0);
    bottomBlock->setSpacing(8);

    payBtn_ = new QPushButton(QStringLiteral("› Pago"), panel);
    payBtn_->setObjectName(QStringLiteral("payButton"));
    payBtn_->setProperty("variant", QStringLiteral("pay-cta"));
    payBtn_->setCursor(Qt::PointingHandCursor);
    payBtn_->setFocusPolicy(Qt::NoFocus);
    payBtn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    payBtn_->setMinimumHeight(196);
    bottomBlock->addWidget(payBtn_, 30);

    connect(payBtn_, &QPushButton::clicked, this, &CheckoutView::handleOpenPaymentDialog);

    auto* numpadGrid = new QGridLayout();
    numpadGrid->setContentsMargins(0, 0, 0, 0);
    numpadGrid->setSpacing(6);

    auto makeKey = [panel](const QString& text, const QString& role) {
        auto* btn = new QPushButton(text, panel);
        btn->setProperty("numpad", role);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        btn->setMinimumHeight(44);
        return btn;
    };

    const QString digitLabels[4][3] = {
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")},
        {QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6")},
        {QStringLiteral("7"), QStringLiteral("8"), QStringLiteral("9")},
        {QStringLiteral("+/-"), QStringLiteral("0"), QStringLiteral(",")}
    };

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 3; ++c) {
            const QString label = digitLabels[r][c];
            auto* keyBtn = makeKey(label, (label == QStringLiteral("+/-")) ? QStringLiteral("util") : QStringLiteral("digit"));
            numpadGrid->addWidget(keyBtn, r, c);

            if (label == QStringLiteral("+/-")) {
                connect(keyBtn, &QPushButton::clicked, this, &CheckoutView::handleNumpadSignToggle);
            } else {
                connect(keyBtn, &QPushButton::clicked, this, [this, label]() {
                    handleNumpadDigit(label);
                });
            }
        }
    }

    numpadModeGroup_ = new QButtonGroup(this);
    numpadModeGroup_->setExclusive(true);

    qtyModeBtn_ = makeKey(QStringLiteral("Ctdad"), QStringLiteral("mode"));
    qtyModeBtn_->setCheckable(true);
    qtyModeBtn_->setChecked(true);

    discModeBtn_ = makeKey(QStringLiteral("% Desc."), QStringLiteral("mode"));
    discModeBtn_->setCheckable(true);

    priceModeBtn_ = makeKey(QStringLiteral("Precio"), QStringLiteral("mode"));
    priceModeBtn_->setCheckable(true);

    backspaceBtn_ = makeKey(QStringLiteral("⌫"), QStringLiteral("backspace"));

    numpadModeGroup_->addButton(qtyModeBtn_, static_cast<int>(NumpadMode::Quantity));
    numpadModeGroup_->addButton(discModeBtn_, static_cast<int>(NumpadMode::Discount));
    numpadModeGroup_->addButton(priceModeBtn_, static_cast<int>(NumpadMode::Price));

    numpadGrid->addWidget(qtyModeBtn_, 0, 3);
    numpadGrid->addWidget(discModeBtn_, 1, 3);
    numpadGrid->addWidget(priceModeBtn_, 2, 3);
    numpadGrid->addWidget(backspaceBtn_, 3, 3);

    connect(qtyModeBtn_, &QPushButton::clicked, this, [this]() { setNumpadMode(NumpadMode::Quantity); });
    connect(discModeBtn_, &QPushButton::clicked, this, [this]() { setNumpadMode(NumpadMode::Discount); });
    connect(priceModeBtn_, &QPushButton::clicked, this, [this]() { setNumpadMode(NumpadMode::Price); });
    connect(backspaceBtn_, &QPushButton::clicked, this, &CheckoutView::handleNumpadBackspace);

    for (int col = 0; col < 4; ++col) {
        numpadGrid->setColumnStretch(col, col == 3 ? 12 : 10);
    }

    auto* numpadWrapper = new QWidget(panel);
    numpadWrapper->setLayout(numpadGrid);
    bottomBlock->addWidget(numpadWrapper, 70);

    layout->addLayout(bottomBlock);
    return panel;
}

QWidget* CheckoutView::buildRightCatalogPanel() {
    auto* panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("catalogPanel"));
    panel->setProperty("surface", QStringLiteral("panel"));
    applySurfaceShadow(panel, 12.0, 2.0);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    // ------------------------------------------------------------------------
    // 1. Category Navigation Bar (Home Breadcrumb + Category Pill Tabs)
    // ------------------------------------------------------------------------
    auto* navBar = new QFrame(panel);
    navBar->setObjectName(QStringLiteral("categoryNavBar"));
    auto* navLayout = new QHBoxLayout(navBar);
    navLayout->setContentsMargins(6, 4, 6, 4);
    navLayout->setSpacing(6);

    const QByteArray homeSvg =
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
        "stroke='#1D4ED8' stroke-width='2.2' stroke-linecap='round' stroke-linejoin='round'>"
        "<path d='M3 10.5L12 3l9 7.5'/><path d='M5 9.5V20a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V9.5'/>"
        "</svg>";

    homeBreadcrumbBtn_ = new QPushButton(navBar);
    homeBreadcrumbBtn_->setObjectName(QStringLiteral("homeBreadcrumbBtn"));
    homeBreadcrumbBtn_->setIcon(buildSvgIcon(homeSvg, QSize(16, 16)));
    homeBreadcrumbBtn_->setIconSize(QSize(16, 16));
    homeBreadcrumbBtn_->setCursor(Qt::PointingHandCursor);
    homeBreadcrumbBtn_->setFocusPolicy(Qt::NoFocus);

    auto* chevron = new QLabel(QStringLiteral("›"), navBar);
    chevron->setObjectName(QStringLiteral("breadcrumbChevron"));

    navLayout->addWidget(homeBreadcrumbBtn_);
    navLayout->addWidget(chevron);

    categoryGroup_ = new QButtonGroup(this);
    categoryGroup_->setExclusive(true);

    const QStringList categories = {
        QStringLiteral("Todos"),
        QStringLiteral("Abarrotes"),
        QStringLiteral("Panadería y Granel"),
        QStringLiteral("Lácteos y Fiambres"),
        QStringLiteral("Frutas y Verduras"),
        QStringLiteral("Bebidas")
    };

    QPushButton* allBtn = nullptr;
    categoryButtons_.clear();
    for (int i = 0; i < categories.size(); ++i) {
        const QString& cat = categories.at(i);
        auto* tabBtn = new QPushButton(cat, navBar);
        tabBtn->setProperty("categoryPill", true);
        tabBtn->setCheckable(true);
        tabBtn->setCursor(Qt::PointingHandCursor);
        tabBtn->setFocusPolicy(Qt::NoFocus);
        if (i == 0) {
            tabBtn->setChecked(true);
            allBtn = tabBtn;
        }
        categoryGroup_->addButton(tabBtn, i);
        categoryButtons_.push_back(tabBtn);
        navLayout->addWidget(tabBtn);

        connect(tabBtn, &QPushButton::clicked, this, [this, cat]() {
            activeCategory_ = cat;
            rebuildProductGrid();
        });
    }

    connect(homeBreadcrumbBtn_, &QPushButton::clicked, this, [this, allBtn]() {
        activeCategory_ = QStringLiteral("Todos");
        searchQuery_.clear();
        if (allBtn) {
            allBtn->setChecked(true);
        }
        rebuildProductGrid();
    });

    navLayout->addStretch();

    catalogCountLabel_ = new QLabel(QStringLiteral("15 productos"), navBar);
    catalogCountLabel_->setObjectName(QStringLiteral("mutedText"));
    navLayout->addWidget(catalogCountLabel_);

    layout->addWidget(navBar);

    // ------------------------------------------------------------------------
    // 2. Scrollable 5-Column Visual Product Grid
    // ------------------------------------------------------------------------
    auto* scrollArea = new QScrollArea(panel);
    scrollArea->setObjectName(QStringLiteral("catalogScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    gridContainer_ = new QWidget(scrollArea);
    gridContainer_->setObjectName(QStringLiteral("catalogGridContainer"));

    productGrid_ = new QGridLayout(gridContainer_);
    productGrid_->setContentsMargins(4, 4, 8, 8);
    productGrid_->setHorizontalSpacing(10);
    productGrid_->setVerticalSpacing(10);
    productGrid_->setAlignment(Qt::AlignTop);

    for (int col = 0; col < kCatalogColumns; ++col) {
        productGrid_->setColumnStretch(col, 1);
    }

    scrollArea->setWidget(gridContainer_);
    layout->addWidget(scrollArea, 1);

    return panel;
}

void CheckoutView::initializeChileanCatalog() {
    catalog_ = {
        {1,  QStringLiteral("AB-001"), QStringLiteral("7801610001014"), QStringLiteral("Aceite Maravilla 1L"),    QStringLiteral("Abarrotes"),          2390, false, QStringLiteral("aceite.png")},
        {2,  QStringLiteral("AB-002"), QStringLiteral("7802900002011"), QStringLiteral("Arroz Grado 1 1kg"),      QStringLiteral("Abarrotes"),          1590, false, QStringLiteral("arroz.png")},
        {3,  QStringLiteral("AB-003"), QStringLiteral("7802950003018"), QStringLiteral("Café Tradicional 170g"),  QStringLiteral("Abarrotes"),          5490, false, QStringLiteral("cafe.png")},
        {4,  QStringLiteral("AB-004"), QStringLiteral("7802575004015"), QStringLiteral("Fideos Espagueti N°5"),   QStringLiteral("Abarrotes"),          1190, false, QStringLiteral("fideos.png")},
        {5,  QStringLiteral("AB-005"), QStringLiteral("7801505005012"), QStringLiteral("Azúcar Granulada 1kg"),   QStringLiteral("Abarrotes"),          1390, false, QStringLiteral("azucar.png")},

        {6,  QStringLiteral("PG-001"), QStringLiteral("2000001000019"), QStringLiteral("Marraqueta Granel"),      QStringLiteral("Panadería y Granel"), 1990, true,  QStringLiteral("marraqueta.png")},
        {7,  QStringLiteral("PG-002"), QStringLiteral("2000002000016"), QStringLiteral("Pan Hallulla Granel"),    QStringLiteral("Panadería y Granel"), 2190, true,  QStringLiteral("hallulla.png")},
        {8,  QStringLiteral("PG-003"), QStringLiteral("2000003000013"), QStringLiteral("Dobladitas Artesanales"), QStringLiteral("Panadería y Granel"), 2690, true,  QStringLiteral("dobladitas.png")},

        {9,  QStringLiteral("LF-001"), QStringLiteral("7802920009019"), QStringLiteral("Leche Entera 1L"),        QStringLiteral("Lácteos y Fiambres"), 1250, false, QStringLiteral("leche.png")},
        {10, QStringLiteral("LF-002"), QStringLiteral("7802920010015"), QStringLiteral("Queso Gauda Laminado"),   QStringLiteral("Lácteos y Fiambres"), 3490, false, QStringLiteral("queso.png")},
        {11, QStringLiteral("LF-003"), QStringLiteral("7804610011012"), QStringLiteral("Jamón Colonial 250g"),    QStringLiteral("Lácteos y Fiambres"), 2890, false, QStringLiteral("jamon.png")},

        {12, QStringLiteral("FV-001"), QStringLiteral("2000012000015"), QStringLiteral("Palta Hass Chilena"),     QStringLiteral("Frutas y Verduras"),  4990, true,  QStringLiteral("palta.png")},
        {13, QStringLiteral("FV-002"), QStringLiteral("2000013000012"), QStringLiteral("Tomate Larga Vida"),      QStringLiteral("Frutas y Verduras"),  1690, true,  QStringLiteral("tomate.png")},

        {14, QStringLiteral("BB-001"), QStringLiteral("7801610014014"), QStringLiteral("Bebida Cola 1,5L"),       QStringLiteral("Bebidas"),            1890, false, QStringLiteral("bebida.png")},
        {15, QStringLiteral("BB-002"), QStringLiteral("7801620015011"), QStringLiteral("Agua Mineral 1,6L"),      QStringLiteral("Bebidas"),            1150, false, QStringLiteral("agua.png")}
    };
}

void CheckoutView::preloadSampleTicket() {
    // Preload 3 realistic supermarket items so the terminal looks alive on first launch:
    // Aceite Maravilla 1L (1 Unid.), Marraqueta Granel (0,850 kg), Palta Hass Chilena (0,500 kg).
    if (catalog_.size() < 12) {
        refreshFinancialSummary();
        return;
    }

    addProductToTicket(catalog_[0], 1.0, false);   // Aceite Maravilla 1L
    addProductToTicket(catalog_[5], 0.850, false); // Marraqueta Granel 0,850 kg
    addProductToTicket(catalog_[11], 0.500, false); // Palta Hass Chilena 0,500 kg
}

void CheckoutView::seedRepositoryIfEmpty() {
    if (!productRepository_) {
        return;
    }

    const auto existing = productRepository_->listActive();
    if (!existing || !existing.value().empty()) {
        return;
    }

    for (const auto& item : catalog_) {
        domain::Product p;
        p.sku = item.sku.toStdString();
        p.barcode = item.barcode.toStdString();
        p.name = item.name.toStdString();
        p.category = item.category.toStdString();
        p.unitPriceCents = CurrencyFormatter::clpToCents(item.unitPriceClp);
        p.taxRateBps = 1900; // Chile 19% IVA
        p.stockQuantity = 100;
        p.isActive = true;
        (void)productRepository_->save(p);
    }
}

void CheckoutView::rebuildProductGrid() {
    if (!productGrid_ || !gridContainer_) {
        return;
    }

    while (QLayoutItem* item = productGrid_->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const QString queryLower = searchQuery_.trimmed().toLower();
    int visibleCount = 0;

    for (const auto& prod : catalog_) {
        const bool categoryMatch = (activeCategory_ == QStringLiteral("Todos"))
            || (prod.category.compare(activeCategory_, Qt::CaseInsensitive) == 0);
        if (!categoryMatch) {
            continue;
        }

        if (!queryLower.isEmpty()) {
            const bool textMatch = prod.name.toLower().contains(queryLower)
                || prod.sku.toLower().contains(queryLower)
                || prod.barcode.toLower().contains(queryLower)
                || prod.category.toLower().contains(queryLower);
            if (!textMatch) {
                continue;
            }
        }

        auto* card = new ProductCardWidget(prod, gridContainer_);
        connect(card, &ProductCardWidget::clicked, this, [this](const CatalogProduct& clickedProd) {
            addProductToTicket(clickedProd);
        });

        const int row = visibleCount / kCatalogColumns;
        const int col = visibleCount % kCatalogColumns;
        productGrid_->addWidget(card, row, col);
        ++visibleCount;
    }

    if (visibleCount == 0) {
        auto tr = &AppPreferences::trText;
        auto* emptyLbl = new QLabel(
            tr(QStringLiteral("No products match the current filter."),
               QStringLiteral("No se encontraron productos para el filtro actual.")),
            gridContainer_
        );
        emptyLbl->setObjectName(QStringLiteral("mutedText"));
        emptyLbl->setAlignment(Qt::AlignCenter);
        emptyLbl->setMinimumHeight(140);
        productGrid_->addWidget(emptyLbl, 0, 0, 1, kCatalogColumns);
    }

    if (catalogCountLabel_) {
        auto tr = &AppPreferences::trText;
        catalogCountLabel_->setText(
            visibleCount == 1
                ? tr(QStringLiteral("1 product"), QStringLiteral("1 producto"))
                : tr(QStringLiteral("%1 products"), QStringLiteral("%1 productos")).arg(visibleCount)
        );
    }
}

bool CheckoutView::addProductToTicket(const CatalogProduct& product, double explicitQty, bool playSound) {
    auto tr = &AppPreferences::trText;

    double qtyToAdd = explicitQty;
    if (std::abs(qtyToAdd) < 0.0001) {
        if (product.isWeighted) {
            qtyToAdd = (currentScaleKg_ > 0.0001) ? currentScaleKg_ : 0.500;
        } else {
            qtyToAdd = 1.0;
        }
    }

    if (refundMode_ && qtyToAdd > 0.0) {
        qtyToAdd = -qtyToAdd;
    }

    // Stock verification against local SQLite repository (skip during sample preload or refunds)
    if (playSound && qtyToAdd > 0.0 && productRepository_) {
        const auto bySku = productRepository_->findBySku(product.sku.toStdString());
        if (bySku && bySku.value().has_value()) {
            const int availableStock = bySku.value()->stockQuantity;
            double alreadyInCart = 0.0;
            for (const auto& existingLine : ticketLines_) {
                if (existingLine.sku.compare(product.sku, Qt::CaseInsensitive) == 0 && existingLine.quantity > 0.0) {
                    alreadyInCart += existingLine.quantity;
                }
            }
            if (availableStock <= 0 || (alreadyInCart + qtyToAdd) > static_cast<double>(availableStock) + 0.0001) {
                ToastManager::showWarning(
                    tr(QStringLiteral("Insufficient Stock"), QStringLiteral("Stock Insuficiente")),
                    tr(QStringLiteral("Insufficient stock for %1 (available: %2)."),
                       QStringLiteral("Stock insuficiente para %1 (disponible: %2)."))
                        .arg(product.name)
                        .arg(availableStock)
                );
                return false;
            }
        }
    }

    int targetIndex = -1;
    // Merge unit items with identical price/discount/sign; keep weighted or noted lines separate if needed
    for (int i = 0; i < static_cast<int>(ticketLines_.size()); ++i) {
        auto& line = ticketLines_[static_cast<std::size_t>(i)];
        const bool sameSign = (line.quantity >= 0.0) == (qtyToAdd >= 0.0);
        if (!product.isWeighted && line.productId == product.id && line.unitPriceClp == product.unitPriceClp
            && std::abs(line.discountPercent) < 0.001 && line.note.isEmpty() && sameSign) {
            line.quantity += qtyToAdd;
            targetIndex = i;
            break;
        }
    }

    if (targetIndex < 0) {
        TicketLineItem line;
        line.productId = product.id;
        line.sku = product.sku;
        line.barcode = product.barcode;
        line.name = product.name;
        line.category = product.category;
        line.quantity = qtyToAdd;
        line.isWeighted = product.isWeighted;
        line.unitPriceClp = product.unitPriceClp;
        line.discountPercent = 0.0;
        ticketLines_.push_back(std::move(line));
        targetIndex = static_cast<int>(ticketLines_.size()) - 1;
    }

    if (product.isWeighted && currentScaleKg_ > 0.0001) {
        emit scaleResetRequested();
    }

    if (playSound) {
        playPosAddBeep();
    }

    numpadBuffer_.clear();
    refreshTicketList(targetIndex);
    refreshFinancialSummary();
    return true;
}

void CheckoutView::refreshTicketList(int selectRow) {
    if (!ticketList_) {
        return;
    }

    const int previousRow = ticketList_->currentRow();
    ticketList_->blockSignals(true);
    ticketList_->clear();

    for (const auto& line : ticketLines_) {
        auto* item = new QListWidgetItem(ticketList_);
        item->setData(TicketNameRole, line.name);
        item->setData(TicketQuantityRole, line.quantity);
        item->setData(TicketIsWeightedRole, line.isWeighted);
        item->setData(TicketUnitPriceClpRole, static_cast<qlonglong>(line.unitPriceClp));
        item->setData(TicketDiscountPercentRole, line.discountPercent);
        item->setData(TicketLineTotalClpRole, static_cast<qlonglong>(line.lineTotalClp()));
        item->setData(TicketNoteRole, line.note);
        ticketList_->addItem(item);
    }

    int target = selectRow;
    if (target < 0) {
        target = previousRow;
    }
    if (target >= ticketList_->count()) {
        target = ticketList_->count() - 1;
    }
    if (target >= 0) {
        ticketList_->setCurrentRow(target);
        ticketList_->scrollToItem(ticketList_->item(target), QAbstractItemView::EnsureVisible);
    }

    ticketList_->blockSignals(false);
    updateNumpadInputPreview();
}

void CheckoutView::refreshFinancialSummary() {
    std::int64_t grossTotalClp = 0;
    for (const auto& line : ticketLines_) {
        grossTotalClp += line.lineTotalClp();
    }

    const auto breakdown = CurrencyFormatter::breakdownFromGrossClp(grossTotalClp);

    if (netValueLabel_) {
        netValueLabel_->setText(CurrencyFormatter::formatClp(breakdown.netClp));
    }
    if (ivaValueLabel_) {
        ivaValueLabel_->setText(CurrencyFormatter::formatClp(breakdown.ivaClp));
    }
    if (totalValueLabel_) {
        totalValueLabel_->setText(CurrencyFormatter::formatClp(breakdown.totalClp));
    }
    auto tr = &AppPreferences::trText;
    if (itemCountBadge_) {
        const auto count = ticketLines_.size();
        itemCountBadge_->setText(
            count == 1
                ? tr(QStringLiteral("1 item"), QStringLiteral("1 línea"))
                : tr(QStringLiteral("%1 items"), QStringLiteral("%1 líneas")).arg(count)
        );
    }
    if (docTypeBadge_) {
        if (invoiceMode_) {
            const QString suffix = customerRut_.isEmpty() ? QString() : QStringLiteral(" · %1").arg(customerRut_);
            docTypeBadge_->setText(
                QStringLiteral("%1%2").arg(tr(QStringLiteral("INVOICE"), QStringLiteral("FACTURA")), suffix)
            );
        } else {
            docTypeBadge_->setText(tr(QStringLiteral("RECEIPT"), QStringLiteral("BOLETA")));
        }
    }
}

void CheckoutView::updateNumpadInputPreview() {
    if (!numpadStatusLabel_) {
        return;
    }

    auto tr = &AppPreferences::trText;
    QString modeText;
    switch (numpadMode_) {
        case NumpadMode::Quantity:
            modeText = tr(QStringLiteral("Qty"), QStringLiteral("Ctdad"));
            break;
        case NumpadMode::Discount:
            modeText = tr(QStringLiteral("% Disc."), QStringLiteral("% Desc."));
            break;
        case NumpadMode::Price:
            modeText = tr(QStringLiteral("Price CLP"), QStringLiteral("Precio CLP"));
            break;
    }

    if (!numpadBuffer_.isEmpty()) {
        numpadStatusLabel_->setText(
            tr(QStringLiteral("Input [%1]: %2"), QStringLiteral("Entrada [%1]: %2")).arg(modeText, numpadBuffer_)
        );
    } else if (refundMode_) {
        numpadStatusLabel_->setText(
            tr(QStringLiteral("REFUND MODE ACTIVE · Keypad: %1"), QStringLiteral("MODO REEMBOLSO ACTIVO · Teclado: %1")).arg(modeText)
        );
    } else if (!ticketNote_.isEmpty()) {
        numpadStatusLabel_->setText(
            tr(QStringLiteral("Receipt note: %1"), QStringLiteral("Nota ticket: %1")).arg(ticketNote_)
        );
    } else {
        numpadStatusLabel_->setText(
            tr(QStringLiteral("Keypad mode: %1"), QStringLiteral("Modo teclado: %1")).arg(modeText)
        );
    }
}

void CheckoutView::retranslateUi() {
    auto tr = &AppPreferences::trText;

    if (ticketTitleLabel_) {
        ticketTitleLabel_->setText(tr(QStringLiteral("Sale Details"), QStringLiteral("Detalle de Venta")));
    }
    if (netCaptionLabel_) {
        netCaptionLabel_->setText(tr(QStringLiteral("Net"), QStringLiteral("Neto")));
    }
    if (ivaCaptionLabel_) {
        ivaCaptionLabel_->setText(tr(QStringLiteral("VAT (19%)"), QStringLiteral("IVA (19%)")));
    }
    if (clearBtn_) {
        clearBtn_->setText(tr(QStringLiteral("Clear"), QStringLiteral("Limpiar")));
    }
    if (refundBtn_) {
        refundBtn_->setText(tr(QStringLiteral("Refund"), QStringLiteral("Reembolso")));
    }
    if (noteBtn_) {
        noteBtn_->setText(tr(QStringLiteral("Note / Receipt"), QStringLiteral("Nota / Boleta")));
    }
    if (discountBtn_) {
        discountBtn_->setText(tr(QStringLiteral("Discount"), QStringLiteral("Descuento")));
    }
    if (codeBtn_) {
        codeBtn_->setText(tr(QStringLiteral("Enter Code"), QStringLiteral("Ingresar Código")));
    }
    if (customerRutBtn_) {
        customerRutBtn_->setText(
            invoiceMode_
                ? tr(QStringLiteral("Invoice / Tax ID"), QStringLiteral("Factura / RUT"))
                : tr(QStringLiteral("Customer / Tax ID"), QStringLiteral("Cliente / RUT"))
        );
    }
    if (payBtn_) {
        payBtn_->setText(tr(QStringLiteral("› Pay"), QStringLiteral("› Pago")));
    }
    if (qtyModeBtn_) {
        qtyModeBtn_->setText(tr(QStringLiteral("Qty"), QStringLiteral("Ctdad")));
    }
    if (discModeBtn_) {
        discModeBtn_->setText(tr(QStringLiteral("% Disc."), QStringLiteral("% Desc.")));
    }
    if (priceModeBtn_) {
        priceModeBtn_->setText(tr(QStringLiteral("Price"), QStringLiteral("Precio")));
    }
    if (backspaceBtn_) {
        backspaceBtn_->setToolTip(tr(
            QStringLiteral("Delete digit or remove selected line"),
            QStringLiteral("Borrar dígito o eliminar línea seleccionada")
        ));
    }
    if (homeBreadcrumbBtn_) {
        homeBreadcrumbBtn_->setToolTip(tr(
            QStringLiteral("Show all categories"),
            QStringLiteral("Volver a todas las categorías")
        ));
    }

    const QStringList enCats = {
        QStringLiteral("All"),
        QStringLiteral("Groceries"),
        QStringLiteral("Bakery && Bulk"),
        QStringLiteral("Dairy && Deli"),
        QStringLiteral("Produce"),
        QStringLiteral("Beverages")
    };
    const QStringList esCats = {
        QStringLiteral("Todos"),
        QStringLiteral("Abarrotes"),
        QStringLiteral("Panadería y Granel"),
        QStringLiteral("Lácteos y Fiambres"),
        QStringLiteral("Frutas y Verduras"),
        QStringLiteral("Bebidas")
    };

    for (std::size_t i = 0; i < categoryButtons_.size() && i < static_cast<std::size_t>(enCats.size()); ++i) {
        categoryButtons_[i]->setText(tr(enCats[static_cast<qsizetype>(i)], esCats[static_cast<qsizetype>(i)]));
    }

    refreshFinancialSummary();
    updateNumpadInputPreview();
    if (ticketList_) {
        ticketList_->viewport()->update();
    }
    rebuildProductGrid();
}

void CheckoutView::refreshThemeVisuals() {
    if (ticketList_) {
        ticketList_->viewport()->update();
    }
}

void CheckoutView::handleBarcodeScanned(const QString& code) {
    auto tr = &AppPreferences::trText;
    const QString cleaned = code.trimmed();
    if (cleaned.isEmpty()) {
        return;
    }

    // 1. Lookup in active visual catalog (barcode or SKU)
    for (const auto& prod : catalog_) {
        if (prod.barcode.compare(cleaned, Qt::CaseInsensitive) == 0
            || prod.sku.compare(cleaned, Qt::CaseInsensitive) == 0) {
            if (addProductToTicket(prod)) {
                ToastManager::showSuccess(
                    tr(QStringLiteral("Product Added"), QStringLiteral("Producto Agregado")),
                    tr(QStringLiteral("%1 added to ticket (%2)"), QStringLiteral("%1 agregado al ticket (%2)"))
                        .arg(prod.name, CurrencyFormatter::formatUnitPrice(prod.unitPriceClp, prod.isWeighted))
                );
            }
            return;
        }
    }

    // 2. Fallback lookup in SQLite IProductRepository
    if (productRepository_) {
        const auto byBarcode = productRepository_->findByBarcode(cleaned.toStdString());
        std::optional<domain::Product> found;
        if (byBarcode && byBarcode.value().has_value()) {
            found = byBarcode.value();
        } else {
            const auto bySku = productRepository_->findBySku(cleaned.toStdString());
            if (bySku && bySku.value().has_value()) {
                found = bySku.value();
            }
        }

        if (found.has_value()) {
            CatalogProduct dynamicProd;
            dynamicProd.id = found->id;
            dynamicProd.sku = QString::fromStdString(found->sku);
            dynamicProd.barcode = QString::fromStdString(found->barcode);
            dynamicProd.name = QString::fromStdString(found->name);
            dynamicProd.category = QString::fromStdString(found->category);
            dynamicProd.unitPriceClp = CurrencyFormatter::centsToClp(found->unitPriceCents);
            dynamicProd.isWeighted = false;
            if (addProductToTicket(dynamicProd)) {
                ToastManager::showSuccess(
                    tr(QStringLiteral("Product Added"), QStringLiteral("Producto Agregado")),
                    tr(QStringLiteral("%1 added to ticket (%2)"), QStringLiteral("%1 agregado al ticket (%2)"))
                        .arg(dynamicProd.name, CurrencyFormatter::formatUnitPrice(dynamicProd.unitPriceClp, false))
                );
            }
            return;
        }
    }

    // 3. Not found: non-blocking floating coral error toast (sound reserved strictly for adding items)
    ToastManager::showError(
        tr(QStringLiteral("Unregistered Barcode"), QStringLiteral("Código de barras no registrado")),
        tr(QStringLiteral("No product matches code \"%1\"."), QStringLiteral("No existe producto registrado para \"%1\".")).arg(cleaned)
    );
}

void CheckoutView::setScaleWeightKg(double weightKg) {
    currentScaleKg_ = std::max(0.0, weightKg);
}

void CheckoutView::setSearchFilter(const QString& query) {
    searchQuery_ = query;
    rebuildProductGrid();
}

void CheckoutView::addCatalogProduct(const CatalogProduct& product) {
    catalog_.push_back(product);
    rebuildProductGrid();
}

void CheckoutView::submitSearchQuery(const QString& query) {
    auto tr = &AppPreferences::trText;
    const QString cleaned = query.trimmed();
    if (cleaned.isEmpty()) {
        return;
    }

    // Exact match on barcode or SKU takes immediate priority
    for (const auto& prod : catalog_) {
        if (prod.barcode.compare(cleaned, Qt::CaseInsensitive) == 0
            || prod.sku.compare(cleaned, Qt::CaseInsensitive) == 0) {
            handleBarcodeScanned(cleaned);
            return;
        }
    }

    // If exactly one product matches the search text, add it immediately
    const QString lower = cleaned.toLower();
    std::vector<const CatalogProduct*> matches;
    for (const auto& prod : catalog_) {
        if (prod.name.toLower().contains(lower) || prod.sku.toLower().contains(lower)) {
            matches.push_back(&prod);
        }
    }

    if (matches.size() == 1) {
        if (addProductToTicket(*matches.front())) {
            ToastManager::showSuccess(
                tr(QStringLiteral("Product Added"), QStringLiteral("Producto Agregado")),
                tr(QStringLiteral("%1 added to ticket"), QStringLiteral("%1 agregado al ticket")).arg(matches.front()->name)
            );
        }
        return;
    }

    // Otherwise delegate to repository / not found handler
    handleBarcodeScanned(cleaned);
}

void CheckoutView::handleClearTicket() {
    auto tr = &AppPreferences::trText;
    ticketLines_.clear();
    ticketNote_.clear();
    numpadBuffer_.clear();
    if (cartService_) {
        cartService_->clear();
    }
    refreshTicketList();
    refreshFinancialSummary();
    ToastManager::showInfo(
        tr(QStringLiteral("Ticket Cleared"), QStringLiteral("Ticket Limpiado")),
        tr(QStringLiteral("Ready for next customer transaction."), QStringLiteral("Listo para iniciar una nueva venta."))
    );
}

void CheckoutView::handleToggleRefundMode(bool checked) {
    auto tr = &AppPreferences::trText;
    refundMode_ = checked;
    updateNumpadInputPreview();
    if (refundMode_) {
        ToastManager::showWarning(
            tr(QStringLiteral("Refund Mode Active"), QStringLiteral("Modo Reembolso Activo")),
            tr(QStringLiteral("Newly added items will be subtracted from the ticket total."),
               QStringLiteral("Los nuevos ítems se restarán del total del ticket."))
        );
    }
}

void CheckoutView::handleAttachNote() {
    auto tr = &AppPreferences::trText;
    const int row = ticketList_ ? ticketList_->currentRow() : -1;
    const bool hasSelectedLine = (row >= 0 && row < static_cast<int>(ticketLines_.size()));

    bool ok = false;
    const QString prompt = hasSelectedLine
        ? tr(QStringLiteral("Note for \"%1\":"), QStringLiteral("Nota para \"%1\":")).arg(ticketLines_[static_cast<std::size_t>(row)].name)
        : tr(QStringLiteral("General note for Receipt / Ticket:"), QStringLiteral("Nota general para la Boleta / Ticket:"));
    const QString currentText = hasSelectedLine
        ? ticketLines_[static_cast<std::size_t>(row)].note
        : ticketNote_;

    const QString text = QInputDialog::getText(
        this,
        tr(QStringLiteral("Note / Receipt"), QStringLiteral("Nota / Boleta")),
        prompt,
        QLineEdit::Normal,
        currentText,
        &ok
    );

    if (!ok) {
        return;
    }

    if (hasSelectedLine) {
        ticketLines_[static_cast<std::size_t>(row)].note = text.trimmed();
        refreshTicketList(row);
    } else {
        ticketNote_ = text.trimmed();
        updateNumpadInputPreview();
    }
}

void CheckoutView::handleQuickDiscount() {
    auto tr = &AppPreferences::trText;
    if (discModeBtn_) {
        discModeBtn_->setChecked(true);
    }
    setNumpadMode(NumpadMode::Discount);

    const int row = ticketList_ ? ticketList_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(ticketLines_.size())) {
        ToastManager::showWarning(
            tr(QStringLiteral("No Item Selected"), QStringLiteral("Sin Ítem Seleccionado")),
            tr(QStringLiteral("Select a ticket item to apply a discount."), QStringLiteral("Seleccione un producto del ticket para aplicar descuento."))
        );
        return;
    }

    bool ok = false;
    const double currentDisc = ticketLines_[static_cast<std::size_t>(row)].discountPercent;
    const double pct = QInputDialog::getDouble(
        this,
        tr(QStringLiteral("Line Discount"), QStringLiteral("Descuento en Línea")),
        tr(QStringLiteral("Discount percentage (%) for \"%1\":"), QStringLiteral("Porcentaje de descuento (%) para \"%1\":"))
            .arg(ticketLines_[static_cast<std::size_t>(row)].name),
        currentDisc,
        0.0,
        100.0,
        0,
        &ok
    );

    if (ok) {
        ticketLines_[static_cast<std::size_t>(row)].discountPercent = pct;
        numpadBuffer_.clear();
        refreshTicketList(row);
        refreshFinancialSummary();
    }
}

void CheckoutView::handleManualCodeEntry() {
    auto tr = &AppPreferences::trText;
    bool ok = false;
    const QString code = QInputDialog::getText(
        this,
        tr(QStringLiteral("Enter Barcode / SKU"), QStringLiteral("Ingresar Código de Barras / SKU")),
        tr(QStringLiteral("EAN-13 Barcode or SKU (e.g. 7801610001014, 2000001000019, AB-001):"), QStringLiteral("Código EAN-13 o SKU (ej: 7801610001014, 2000001000019, AB-001):")),
        QLineEdit::Normal,
        QString(),
        &ok
    );

    if (ok && !code.trimmed().isEmpty()) {
        handleBarcodeScanned(code.trimmed());
    }
}

void CheckoutView::handleToggleDocumentType() {
    auto tr = &AppPreferences::trText;
    if (!invoiceMode_) {
        bool ok = false;
        const QString rut = QInputDialog::getText(
            this,
            tr(QStringLiteral("Customer / Electronic Invoice"), QStringLiteral("Cliente / Factura Electrónica")),
            tr(QStringLiteral("Enter Customer Tax ID / RUT (e.g. 76.123.456-7) or leave empty for Invoice:"), QStringLiteral("Ingrese RUT del cliente (ej. 76.123.456-7) o deje vacío para cambiar a Factura:")),
            QLineEdit::Normal,
            customerRut_,
            &ok
        );
        if (!ok) {
            if (customerRutBtn_) {
                customerRutBtn_->setChecked(false);
            }
            return;
        }
        invoiceMode_ = true;
        customerRut_ = rut.trimmed();
        if (customerRutBtn_) {
            customerRutBtn_->setChecked(true);
            customerRutBtn_->setText(tr(QStringLiteral("Invoice / Tax ID"), QStringLiteral("Factura / RUT")));
        }
    } else {
        invoiceMode_ = false;
        customerRut_.clear();
        if (customerRutBtn_) {
            customerRutBtn_->setChecked(false);
            customerRutBtn_->setText(tr(QStringLiteral("Customer / Tax ID"), QStringLiteral("Cliente / RUT")));
        }
    }
    refreshFinancialSummary();
}

void CheckoutView::handleOpenPaymentDialog() {
    auto tr = &AppPreferences::trText;
    if (ticketLines_.empty()) {
        ToastManager::showWarning(
            tr(QStringLiteral("Empty Ticket"), QStringLiteral("Ticket Vacío")),
            tr(QStringLiteral("Add at least one item before proceeding to payment."), QStringLiteral("Agrega al menos un producto antes de cobrar."))
        );
        return;
    }

    std::int64_t grossTotalClp = 0;
    for (const auto& line : ticketLines_) {
        grossTotalClp += line.lineTotalClp();
    }
    const auto breakdown = CurrencyFormatter::breakdownFromGrossClp(grossTotalClp);

    QDialog dlg(this);
    dlg.setWindowTitle(tr(QStringLiteral("Payment — vamp9 POS"), QStringLiteral("Cobro — vamp9 POS")));
    dlg.setMinimumWidth(440);
    dlg.setModal(true);

    auto* dlgLayout = new QVBoxLayout(&dlg);
    dlgLayout->setContentsMargins(22, 20, 22, 20);
    dlgLayout->setSpacing(14);

    auto* docHeader = new QLabel(
        invoiceMode_
            ? QStringLiteral("%1 %2").arg(tr(QStringLiteral("ELECTRONIC INVOICE"), QStringLiteral("FACTURA ELECTRÓNICA")), customerRut_)
            : tr(QStringLiteral("ELECTRONIC RECEIPT"), QStringLiteral("BOLETA ELECTRÓNICA")),
        &dlg
    );
    docHeader->setObjectName(QStringLiteral("sectionTitle"));
    dlgLayout->addWidget(docHeader);

    auto* totalBox = new QFrame(&dlg);
    totalBox->setProperty("glass", QStringLiteral("accent-card"));
    auto* totalBoxLayout = new QVBoxLayout(totalBox);
    totalBoxLayout->setContentsMargins(16, 12, 16, 12);
    totalBoxLayout->setSpacing(4);

    auto* totalLbl = new QLabel(CurrencyFormatter::formatClp(breakdown.totalClp), totalBox);
    totalLbl->setObjectName(QStringLiteral("totalAmountLabel"));
    totalLbl->setAlignment(Qt::AlignCenter);

    auto* taxSubLbl = new QLabel(
        QStringLiteral("%1: %2   |   %3: %4")
            .arg(tr(QStringLiteral("Net"), QStringLiteral("Neto")))
            .arg(CurrencyFormatter::formatClp(breakdown.netClp))
            .arg(tr(QStringLiteral("VAT (19%)"), QStringLiteral("IVA (19%)")))
            .arg(CurrencyFormatter::formatClp(breakdown.ivaClp)),
        totalBox
    );
    taxSubLbl->setObjectName(QStringLiteral("mutedText"));
    taxSubLbl->setAlignment(Qt::AlignCenter);

    totalBoxLayout->addWidget(totalLbl);
    totalBoxLayout->addWidget(taxSubLbl);
    dlgLayout->addWidget(totalBox);

    auto* methodRow = new QHBoxLayout();
    methodRow->setSpacing(8);
    auto* methodGroup = new QButtonGroup(&dlg);
    methodGroup->setExclusive(true);

    auto* cashBtn = new QPushButton(tr(QStringLiteral("Cash"), QStringLiteral("Efectivo")), &dlg);
    auto* cardBtn = new QPushButton(tr(QStringLiteral("Debit / Credit"), QStringLiteral("Débito / Crédito")), &dlg);
    auto* transferBtn = new QPushButton(tr(QStringLiteral("Transfer"), QStringLiteral("Transferencia")), &dlg);
    cashBtn->setCheckable(true);
    cardBtn->setCheckable(true);
    transferBtn->setCheckable(true);
    cashBtn->setChecked(true);
    cashBtn->setProperty("categoryPill", true);
    cardBtn->setProperty("categoryPill", true);
    transferBtn->setProperty("categoryPill", true);

    methodGroup->addButton(cashBtn, static_cast<int>(domain::PaymentMethod::Cash));
    methodGroup->addButton(cardBtn, static_cast<int>(domain::PaymentMethod::Card));
    methodGroup->addButton(transferBtn, static_cast<int>(domain::PaymentMethod::Transfer));

    methodRow->addWidget(cashBtn, 1);
    methodRow->addWidget(cardBtn, 1);
    methodRow->addWidget(transferBtn, 1);
    dlgLayout->addLayout(methodRow);

    auto* tenderedInput = new QLineEdit(QString::number(std::max<std::int64_t>(0, breakdown.totalClp)), &dlg);
    tenderedInput->setPlaceholderText(tr(QStringLiteral("Amount tendered (CLP)..."), QStringLiteral("Monto recibido en CLP...")));
    dlgLayout->addWidget(tenderedInput);

    auto* billsRow = new QHBoxLayout();
    billsRow->setSpacing(6);
    const std::int64_t quickBills[] = {5000, 10000, 20000};
    for (std::int64_t bill : quickBills) {
        auto* billBtn = new QPushButton(CurrencyFormatter::formatClp(bill), &dlg);
        billBtn->setProperty("variant", QStringLiteral("quick-action"));
        connect(billBtn, &QPushButton::clicked, &dlg, [tenderedInput, bill]() {
            tenderedInput->setText(QString::number(bill));
        });
        billsRow->addWidget(billBtn);
    }
    auto* exactBtn = new QPushButton(tr(QStringLiteral("Exact"), QStringLiteral("Exacto")), &dlg);
    exactBtn->setProperty("variant", QStringLiteral("quick-action"));
    connect(exactBtn, &QPushButton::clicked, &dlg, [tenderedInput, total = breakdown.totalClp]() {
        tenderedInput->setText(QString::number(std::max<std::int64_t>(0, total)));
    });
    billsRow->addWidget(exactBtn);
    dlgLayout->addLayout(billsRow);

    auto* changeLabel = new QLabel(&dlg);
    changeLabel->setObjectName(QStringLiteral("sectionTitle"));
    changeLabel->setAlignment(Qt::AlignRight);
    dlgLayout->addWidget(changeLabel);

    auto updateChange = [tenderedInput, changeLabel, total = breakdown.totalClp, tr]() {
        bool ok = false;
        const std::int64_t paid = tenderedInput->text().trimmed().toLongLong(&ok);
        const std::int64_t change = ok ? (paid - total) : 0;
        changeLabel->setText(
            change >= 0
                ? QStringLiteral("%1: %2").arg(tr(QStringLiteral("Change"), QStringLiteral("Vuelto")), CurrencyFormatter::formatClp(change))
                : QStringLiteral("%1: %2").arg(tr(QStringLiteral("Remaining"), QStringLiteral("Falta")), CurrencyFormatter::formatClp(-change))
        );
    };
    connect(tenderedInput, &QLineEdit::textChanged, &dlg, updateChange);
    updateChange();

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    auto* cancelBtn = new QPushButton(tr(QStringLiteral("Cancel"), QStringLiteral("Cancelar")), &dlg);
    cancelBtn->setProperty("variant", QStringLiteral("glass"));
    auto* confirmBtn = new QPushButton(tr(QStringLiteral("Confirm && Issue"), QStringLiteral("Confirmar y Emitir")), &dlg);
    confirmBtn->setProperty("variant", QStringLiteral("primary"));

    btnRow->addWidget(cancelBtn, 1);
    btnRow->addWidget(confirmBtn, 2);
    dlgLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(confirmBtn, &QPushButton::clicked, &dlg, [&]() {
        const auto selectedMethod = static_cast<domain::PaymentMethod>(methodGroup->checkedId());
        if (selectedMethod == domain::PaymentMethod::Cash && breakdown.totalClp > 0) {
            bool okPaid = false;
            const std::int64_t paidVal = tenderedInput->text().trimmed().toLongLong(&okPaid);
            if (!okPaid || paidVal < breakdown.totalClp) {
                ToastManager::showWarning(
                    tr(QStringLiteral("Insufficient Payment"), QStringLiteral("Monto Insuficiente")),
                    tr(QStringLiteral("Amount tendered is lower than the ticket total."), QStringLiteral("El monto recibido es menor al total a pagar."))
                );
                return;
            }
        }
        dlg.accept();
    });

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    bool okTendered = false;
    const std::int64_t paidAmount = tenderedInput->text().trimmed().toLongLong(&okTendered);
    const std::int64_t changeClp = (okTendered && paidAmount > breakdown.totalClp)
        ? (paidAmount - breakdown.totalClp)
        : 0;

    static int sReceiptSeq = 1042;
    const QString receiptNum = QStringLiteral("%1-%2")
        .arg(invoiceMode_ ? QStringLiteral("FAC") : QStringLiteral("BOL"))
        .arg(sReceiptSeq++, 4, 10, QLatin1Char('0'));

    domain::Sale sale;
    sale.receiptNumber = receiptNum.toStdString();
    sale.subtotalCents = CurrencyFormatter::clpToCents(breakdown.netClp);
    sale.taxTotalCents = CurrencyFormatter::clpToCents(breakdown.ivaClp);
    sale.grandTotalCents = CurrencyFormatter::clpToCents(breakdown.totalClp);
    sale.paymentMethod = static_cast<domain::PaymentMethod>(methodGroup->checkedId());
    sale.status = domain::SaleStatus::Completed;

    if (receiptPrinter_) {
        (void)receiptPrinter_->printReceipt(sale);
        if (sale.paymentMethod == domain::PaymentMethod::Cash) {
            (void)receiptPrinter_->openCashDrawer();
        }
    }

    // Update stock in SQLite repository for completed sale
    if (productRepository_) {
        for (const auto& line : ticketLines_) {
            const auto bySku = productRepository_->findBySku(line.sku.toStdString());
            if (bySku && bySku.value().has_value()) {
                const int qtyDelta = -static_cast<int>(std::llround(line.quantity));
                if (qtyDelta != 0) {
                    (void)productRepository_->updateStock(bySku.value()->id, qtyDelta);
                }
            }
        }
    }

    QString methodStr = tr(QStringLiteral("Cash"), QStringLiteral("Efectivo"));
    if (sale.paymentMethod == domain::PaymentMethod::Card) {
        methodStr = tr(QStringLiteral("Card"), QStringLiteral("Débito / Crédito"));
    } else if (sale.paymentMethod == domain::PaymentMethod::Transfer) {
        methodStr = tr(QStringLiteral("Transfer"), QStringLiteral("Transferencia"));
    }

    SaleReportEntry reportEntry;
    reportEntry.receiptNumber = receiptNum;
    reportEntry.documentType = invoiceMode_
        ? tr(QStringLiteral("Invoice"), QStringLiteral("Factura"))
        : tr(QStringLiteral("Receipt"), QStringLiteral("Boleta"));
    reportEntry.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"));
    reportEntry.paymentMethod = methodStr;
    reportEntry.itemCount = static_cast<int>(ticketLines_.size());
    reportEntry.netClp = breakdown.netClp;
    reportEntry.ivaClp = breakdown.ivaClp;
    reportEntry.totalClp = breakdown.totalClp;
    emit saleCompleted(reportEntry);

    const QString toastTitle = tr(QStringLiteral("Sale Approved"), QStringLiteral("Venta Aprobada"));
    const QString toastBody = (sale.paymentMethod == domain::PaymentMethod::Cash && changeClp > 0)
        ? tr(QStringLiteral("Sale #%1 approved — Change: %2"),
             QStringLiteral("Venta #%1 aprobada — Vuelto: %2"))
              .arg(receiptNum, CurrencyFormatter::formatClp(changeClp))
        : tr(QStringLiteral("Sale #%1 approved — Total: %2 (%3)"),
             QStringLiteral("Venta #%1 aprobada — Total: %2 (%3)"))
              .arg(receiptNum, CurrencyFormatter::formatClp(breakdown.totalClp), methodStr);

    ticketLines_.clear();
    ticketNote_.clear();
    numpadBuffer_.clear();
    if (refundBtn_ && refundBtn_->isChecked()) {
        refundBtn_->setChecked(false);
    }
    refreshTicketList();
    refreshFinancialSummary();
    ToastManager::showSuccess(toastTitle, toastBody);
}

void CheckoutView::setNumpadMode(NumpadMode mode) {
    numpadMode_ = mode;
    numpadBuffer_.clear();
    updateNumpadInputPreview();
}

void CheckoutView::handleNumpadDigit(const QString& token) {
    auto tr = &AppPreferences::trText;
    const int row = ticketList_ ? ticketList_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(ticketLines_.size())) {
        ToastManager::showWarning(
            tr(QStringLiteral("No Item Selected"), QStringLiteral("Sin Ítem Seleccionado")),
            tr(QStringLiteral("Select a ticket row to edit quantity, discount, or price."),
               QStringLiteral("Seleccione un ítem del ticket para editar cantidad, descuento o precio."))
        );
        return;
    }

    if (token == QStringLiteral(",")) {
        if (numpadMode_ == NumpadMode::Price) {
            return; // CLP has no decimals
        }
        if (numpadBuffer_.contains(QLatin1Char(','))) {
            return;
        }
        if (numpadBuffer_.isEmpty()) {
            numpadBuffer_ = QStringLiteral("0,");
        } else {
            numpadBuffer_.append(QLatin1Char(','));
        }
    } else {
        if (numpadBuffer_.size() >= 9) {
            return;
        }
        numpadBuffer_.append(token);
    }

    applyNumpadBufferToSelectedRow();
}

void CheckoutView::handleNumpadSignToggle() {
    const int row = ticketList_ ? ticketList_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(ticketLines_.size())) {
        if (refundBtn_) {
            refundBtn_->toggle();
        }
        return;
    }

    auto& line = ticketLines_[static_cast<std::size_t>(row)];
    line.quantity = -line.quantity;
    refreshTicketList(row);
    refreshFinancialSummary();
}

void CheckoutView::handleNumpadBackspace() {
    const int row = ticketList_ ? ticketList_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(ticketLines_.size())) {
        return;
    }

    if (!numpadBuffer_.isEmpty()) {
        numpadBuffer_.chop(1);
        if (!numpadBuffer_.isEmpty()) {
            applyNumpadBufferToSelectedRow();
        } else {
            updateNumpadInputPreview();
        }
        return;
    }

    // Empty buffer: remove selected line item cleanly
    ticketLines_.erase(ticketLines_.begin() + row);
    const int nextRow = std::min(row, static_cast<int>(ticketLines_.size()) - 1);
    refreshTicketList(nextRow);
    refreshFinancialSummary();
}

void CheckoutView::applyNumpadBufferToSelectedRow() {
    const int row = ticketList_ ? ticketList_->currentRow() : -1;
    if (row < 0 || row >= static_cast<int>(ticketLines_.size())) {
        return;
    }

    QString normalized = numpadBuffer_;
    normalized.replace(QLatin1Char(','), QLatin1Char('.'));

    bool ok = false;
    const double val = normalized.toDouble(&ok);
    if (!ok) {
        updateNumpadInputPreview();
        return;
    }

    auto& line = ticketLines_[static_cast<std::size_t>(row)];
    switch (numpadMode_) {
        case NumpadMode::Quantity: {
            if (val > 0.0) {
                const double sign = (line.quantity < 0.0) ? -1.0 : 1.0;
                line.quantity = sign * val;
            }
            break;
        }
        case NumpadMode::Discount: {
            line.discountPercent = std::clamp(val, 0.0, 100.0);
            break;
        }
        case NumpadMode::Price: {
            line.unitPriceClp = std::max<std::int64_t>(0, static_cast<std::int64_t>(std::llround(val)));
            break;
        }
    }

    refreshTicketList(row);
    refreshFinancialSummary();
}

void CheckoutView::playPosAddBeep() {
#ifdef _WIN32
    // Synthesize once: crisp 85ms 2500Hz classic supermarket POS barcode-scanner beep (16-bit PCM WAV)
    static const std::vector<std::uint8_t> kPosBeepWav = []() {
        constexpr std::uint32_t kSampleRate = 44100;
        constexpr double kDurationSec = 0.085;
        constexpr double kFreqHz = 2500.0;
        constexpr double kTwoPi = 6.28318530717958647692;
        constexpr double kAmplitude = 0.42 * 32767.0;

        const auto numSamples = static_cast<std::uint32_t>(kSampleRate * kDurationSec);
        const std::uint32_t dataBytes = numSamples * sizeof(std::int16_t);
        const std::uint32_t riffSize = 36 + dataBytes;

        std::vector<std::uint8_t> buf;
        buf.reserve(44 + dataBytes);

        auto writeU16 = [&buf](std::uint16_t v) {
            buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
            buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        };
        auto writeU32 = [&buf](std::uint32_t v) {
            buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
            buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
            buf.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
            buf.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
        };
        auto writeFourCC = [&buf](const char tag[4]) {
            for (int i = 0; i < 4; ++i) {
                buf.push_back(static_cast<std::uint8_t>(tag[i]));
            }
        };

        writeFourCC("RIFF");
        writeU32(riffSize);
        writeFourCC("WAVE");
        writeFourCC("fmt ");
        writeU32(16);                     // PCM chunk size
        writeU16(1);                      // PCM format
        writeU16(1);                      // Mono
        writeU32(kSampleRate);
        writeU32(kSampleRate * 2);        // Byte rate
        writeU16(2);                      // Block align
        writeU16(16);                     // Bits per sample
        writeFourCC("data");
        writeU32(dataBytes);

        const std::uint32_t fadeInSamples = static_cast<std::uint32_t>(kSampleRate * 0.004);
        const std::uint32_t fadeOutSamples = static_cast<std::uint32_t>(kSampleRate * 0.014);

        for (std::uint32_t i = 0; i < numSamples; ++i) {
            double env = 1.0;
            if (i < fadeInSamples) {
                env = static_cast<double>(i) / static_cast<double>(fadeInSamples);
            } else if (i > numSamples - fadeOutSamples) {
                env = static_cast<double>(numSamples - i) / static_cast<double>(fadeOutSamples);
            }
            const double t = static_cast<double>(i) / static_cast<double>(kSampleRate);
            const double sampleVal = std::sin(kTwoPi * kFreqHz * t) * kAmplitude * env;
            const auto pcm = static_cast<std::int16_t>(std::lround(sampleVal));
            writeU16(static_cast<std::uint16_t>(pcm));
        }

        return buf;
    }();

    PlaySoundW(
        reinterpret_cast<LPCWSTR>(kPosBeepWav.data()),
        nullptr,
        SND_MEMORY | SND_ASYNC | SND_NODEFAULT
    );
#else
    QApplication::beep();
#endif
}

} // namespace vamp9::pos::presentation
