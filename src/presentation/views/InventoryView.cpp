#include "presentation/views/InventoryView.h"

#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/CurrencyFormatter.h"
#include "presentation/widgets/ToastManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace vamp9::pos::presentation {

namespace {

QString sanitizeFileStem(const QString& raw) {
    QString out;
    out.reserve(raw.size());
    for (QChar ch : raw.trimmed().toLower()) {
        if (ch.isLetterOrNumber()) {
            out.append(ch);
        } else if (ch == QLatin1Char('-') || ch == QLatin1Char('_')) {
            out.append(QLatin1Char('_'));
        }
    }
    return out.isEmpty() ? QStringLiteral("producto") : out;
}

bool saveProductImagePng(const QString& sourcePath, const QString& destFileName) {
    QImage src(sourcePath);
    if (src.isNull()) {
        return false;
    }

    QImage canvas(400, 400, QImage::Format_ARGB32);
    canvas.fill(Qt::white);

    const QImage scaled = src.scaled(360, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage((400 - scaled.width()) / 2, (400 - scaled.height()) / 2, scaled);
    p.end();

    const QString userProductsDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("products"));
    QDir().mkpath(userProductsDir);
    return canvas.save(QDir(userProductsDir).filePath(destFileName), "PNG");
}

} // namespace

InventoryView::InventoryView(
    std::shared_ptr<domain::IProductRepository> productRepository,
    QWidget* parent
)
    : QWidget(parent)
    , productRepository_(std::move(productRepository)) {
    setupUi();
    retranslateUi();
    reloadProducts();
}

void InventoryView::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto* panel = new QFrame(this);
    panel->setProperty("surface", QStringLiteral("panel"));
    auto* shadow = new QGraphicsDropShadowEffect(panel);
    shadow->setBlurRadius(12.0);
    shadow->setOffset(0.0, 2.0);
    shadow->setColor(QColor(15, 23, 42, 14));
    panel->setGraphicsEffect(shadow);

    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(18, 16, 18, 16);
    panelLayout->setSpacing(14);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(10);

    searchInput_ = new QLineEdit(panel);
    searchInput_->setClearButtonEnabled(true);

    refreshBtn_ = new QPushButton(panel);
    refreshBtn_->setProperty("variant", QStringLiteral("glass"));
    refreshBtn_->setCursor(Qt::PointingHandCursor);

    newProductBtn_ = new QPushButton(panel);
    newProductBtn_->setProperty("variant", QStringLiteral("primary"));
    newProductBtn_->setCursor(Qt::PointingHandCursor);

    toolbar->addWidget(searchInput_, 1);
    toolbar->addWidget(refreshBtn_);
    toolbar->addWidget(newProductBtn_);
    panelLayout->addLayout(toolbar);

    productsTable_ = new QTableWidget(0, 7, panel);
    productsTable_->horizontalHeader()->setVisible(true);
    productsTable_->horizontalHeader()->setStretchLastSection(false);
    productsTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    productsTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    productsTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    productsTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    productsTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    productsTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    productsTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    productsTable_->verticalHeader()->setVisible(false);
    productsTable_->setAlternatingRowColors(true);
    productsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    productsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    productsTable_->setShowGrid(true);

    panelLayout->addWidget(productsTable_, 1);
    rootLayout->addWidget(panel, 1);

    connect(refreshBtn_, &QPushButton::clicked, this, &InventoryView::reloadProducts);
    connect(newProductBtn_, &QPushButton::clicked, this, &InventoryView::handleAddNewProduct);
    connect(searchInput_, &QLineEdit::textChanged, this, &InventoryView::renderTableRows);
}

void InventoryView::retranslateUi() {
    auto tr = &AppPreferences::trText;
    if (searchInput_) {
        searchInput_->setPlaceholderText(tr(
            QStringLiteral("Filter inventory by SKU, barcode, name or category..."),
            QStringLiteral("Filtrar inventario por SKU, código de barras, nombre o categoría...")
        ));
    }
    if (refreshBtn_) {
        refreshBtn_->setText(tr(QStringLiteral("Refresh"), QStringLiteral("Actualizar")));
    }
    if (newProductBtn_) {
        newProductBtn_->setText(tr(QStringLiteral("+ New Product"), QStringLiteral("+ Nuevo Producto")));
    }
    if (productsTable_) {
        productsTable_->setHorizontalHeaderLabels({
            QStringLiteral("SKU"),
            tr(QStringLiteral("Barcode"), QStringLiteral("Código de Barras")),
            tr(QStringLiteral("Product"), QStringLiteral("Producto")),
            tr(QStringLiteral("Category"), QStringLiteral("Categoría")),
            tr(QStringLiteral("Price (CLP)"), QStringLiteral("Precio (CLP)")),
            tr(QStringLiteral("VAT"), QStringLiteral("IVA")),
            tr(QStringLiteral("Stock"), QStringLiteral("Stock"))
        });
    }
}

void InventoryView::reloadProducts() {
    if (!productRepository_ || !productsTable_) {
        return;
    }

    const auto result = productRepository_->listActive();
    if (!result) {
        return;
    }

    cachedProducts_ = result.value();
    renderTableRows(searchInput_ ? searchInput_->text() : QString());
}

void InventoryView::renderTableRows(const QString& filterText) {
    if (!productsTable_) {
        return;
    }

    const QString query = filterText.trimmed().toLower();
    std::vector<const domain::Product*> filtered;
    filtered.reserve(cachedProducts_.size());

    for (const auto& p : cachedProducts_) {
        if (!query.isEmpty()) {
            const QString sku = QString::fromStdString(p.sku).toLower();
            const QString barcode = QString::fromStdString(p.barcode).toLower();
            const QString name = QString::fromStdString(p.name).toLower();
            const QString cat = QString::fromStdString(p.category).toLower();
            if (!sku.contains(query) && !barcode.contains(query)
                && !name.contains(query) && !cat.contains(query)) {
                continue;
            }
        }
        filtered.push_back(&p);
    }

    productsTable_->setRowCount(static_cast<int>(filtered.size()));
    for (int row = 0; row < static_cast<int>(filtered.size()); ++row) {
        const auto& p = *filtered[static_cast<std::size_t>(row)];
        productsTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(p.sku)));
        productsTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(p.barcode)));
        productsTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(p.name)));
        productsTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(p.category)));
        productsTable_->setItem(
            row,
            4,
            new QTableWidgetItem(CurrencyFormatter::formatClp(CurrencyFormatter::centsToClp(p.unitPriceCents)))
        );
        productsTable_->setItem(
            row,
            5,
            new QTableWidgetItem(QStringLiteral("%1%").arg(static_cast<double>(p.taxRateBps) / 100.0, 0, 'f', 0))
        );
        productsTable_->setItem(row, 6, new QTableWidgetItem(QStringLiteral("%1 u.").arg(p.stockQuantity)));
    }
}

void InventoryView::handleAddNewProduct() {
    if (!productRepository_) {
        return;
    }

    auto tr = &AppPreferences::trText;

    QDialog dlg(this);
    dlg.setWindowTitle(tr(QStringLiteral("New Product — vamp9 POS"), QStringLiteral("Nuevo Producto — vamp9 POS")));
    dlg.setMinimumWidth(460);
    dlg.setModal(true);

    auto* mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setContentsMargins(22, 20, 22, 20);
    mainLayout->setSpacing(14);

    auto* titleLbl = new QLabel(
        tr(QStringLiteral("Add New Product to Catalog"), QStringLiteral("Agregar Nuevo Producto al Catálogo")),
        &dlg
    );
    titleLbl->setObjectName(QStringLiteral("sectionTitle"));
    mainLayout->addWidget(titleLbl);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText(tr(QStringLiteral("e.g. Natural Yogurt 1L"), QStringLiteral("Ej: Yogurt Natural 1L")));

    const QString suggestedSku = QStringLiteral("PR-%1").arg(cachedProducts_.size() + 1, 3, 10, QLatin1Char('0'));
    auto* skuEdit = new QLineEdit(suggestedSku, &dlg);
    skuEdit->setPlaceholderText(QStringLiteral("AB-016"));

    auto* barcodeEdit = new QLineEdit(&dlg);
    barcodeEdit->setPlaceholderText(tr(QStringLiteral("Optional — e.g. 7801234567890"), QStringLiteral("Opcional — Ej: 7801234567890")));

    auto* categoryCombo = new QComboBox(&dlg);
    categoryCombo->addItems({
        QStringLiteral("Abarrotes"),
        QStringLiteral("Panadería y Granel"),
        QStringLiteral("Lácteos y Fiambres"),
        QStringLiteral("Frutas y Verduras"),
        QStringLiteral("Bebidas")
    });
    categoryCombo->setEditable(true);
    categoryCombo->setMinimumHeight(38);

    auto* priceSpin = new QSpinBox(&dlg);
    priceSpin->setRange(1, 9999999);
    priceSpin->setSingleStep(100);
    priceSpin->setValue(1990);
    priceSpin->setPrefix(QStringLiteral("$ "));
    priceSpin->setMinimumHeight(38);

    auto* taxSpin = new QDoubleSpinBox(&dlg);
    taxSpin->setRange(0.0, 100.0);
    taxSpin->setDecimals(0);
    taxSpin->setValue(19.0);
    taxSpin->setSuffix(QStringLiteral(" %"));
    taxSpin->setMinimumHeight(38);

    auto* stockSpin = new QSpinBox(&dlg);
    stockSpin->setRange(0, 999999);
    stockSpin->setValue(50);
    stockSpin->setMinimumHeight(38);

    auto* weightedCheck = new QCheckBox(
        tr(QStringLiteral("Bulk item (sold by kg)"), QStringLiteral("Producto a granel (se vende por kg)")),
        &dlg
    );

    QString selectedPhotoPath;
    auto* photoRow = new QHBoxLayout();
    photoRow->setSpacing(8);
    auto* photoPathEdit = new QLineEdit(&dlg);
    photoPathEdit->setReadOnly(true);
    photoPathEdit->setPlaceholderText(tr(QStringLiteral("No image selected (optional)"), QStringLiteral("Sin imagen seleccionada (opcional)")));
    auto* browsePhotoBtn = new QPushButton(tr(QStringLiteral("Choose photo..."), QStringLiteral("Elegir foto...")), &dlg);
    browsePhotoBtn->setProperty("variant", QStringLiteral("glass"));
    photoRow->addWidget(photoPathEdit, 1);
    photoRow->addWidget(browsePhotoBtn);

    connect(browsePhotoBtn, &QPushButton::clicked, &dlg, [&dlg, &selectedPhotoPath, photoPathEdit, tr]() {
        const QString file = QFileDialog::getOpenFileName(
            &dlg,
            tr(QStringLiteral("Select Product Photo"), QStringLiteral("Seleccionar Fotografía del Producto")),
            QString(),
            tr(QStringLiteral("Images (*.png *.jpg *.jpeg *.webp)"), QStringLiteral("Imágenes (*.png *.jpg *.jpeg *.webp)"))
        );
        if (!file.isEmpty()) {
            selectedPhotoPath = file;
            photoPathEdit->setText(QFileInfo(file).fileName());
        }
    });

    form->addRow(tr(QStringLiteral("Name *:"), QStringLiteral("Nombre *:")), nameEdit);
    form->addRow(QStringLiteral("SKU *:"), skuEdit);
    form->addRow(tr(QStringLiteral("Barcode:"), QStringLiteral("Código de Barras:")), barcodeEdit);
    form->addRow(tr(QStringLiteral("Category:"), QStringLiteral("Categoría:")), categoryCombo);
    form->addRow(tr(QStringLiteral("Price (CLP) *:"), QStringLiteral("Precio (CLP) *:")), priceSpin);
    form->addRow(tr(QStringLiteral("VAT:"), QStringLiteral("IVA:")), taxSpin);
    form->addRow(tr(QStringLiteral("Initial stock:"), QStringLiteral("Stock inicial:")), stockSpin);
    form->addRow(tr(QStringLiteral("Sold by weight:"), QStringLiteral("Venta por peso:")), weightedCheck);
    form->addRow(tr(QStringLiteral("Photo:"), QStringLiteral("Fotografía:")), photoRow);

    mainLayout->addLayout(form);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);
    auto* cancelBtn = new QPushButton(tr(QStringLiteral("Cancel"), QStringLiteral("Cancelar")), &dlg);
    cancelBtn->setProperty("variant", QStringLiteral("glass"));
    auto* saveBtn = new QPushButton(tr(QStringLiteral("Save Product"), QStringLiteral("Guardar Producto")), &dlg);
    saveBtn->setProperty("variant", QStringLiteral("primary"));
    btnRow->addWidget(cancelBtn, 1);
    btnRow->addWidget(saveBtn, 2);
    mainLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, &dlg, [&]() {
        if (nameEdit->text().trimmed().isEmpty() || skuEdit->text().trimmed().isEmpty()) {
            ToastManager::showWarning(
                tr(QStringLiteral("Incomplete Data"), QStringLiteral("Datos Incompletos")),
                tr(QStringLiteral("Please enter at least the product Name and SKU."), QStringLiteral("Debes ingresar al menos el Nombre y el SKU del producto."))
            );
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    domain::Product newProd;
    newProd.name = nameEdit->text().trimmed().toStdString();
    newProd.sku = skuEdit->text().trimmed().toStdString();
    newProd.barcode = barcodeEdit->text().trimmed().toStdString();
    newProd.category = categoryCombo->currentText().trimmed().toStdString();
    newProd.unitPriceCents = CurrencyFormatter::clpToCents(priceSpin->value());
    newProd.taxRateBps = static_cast<domain::TaxRateBps>(std::llround(taxSpin->value() * 100.0));
    newProd.stockQuantity = stockSpin->value();
    newProd.isActive = true;

    const auto saveRes = productRepository_->save(newProd);
    if (!saveRes) {
        ToastManager::showError(
            tr(QStringLiteral("Save Failed"), QStringLiteral("No se pudo guardar")),
            tr(QStringLiteral("Could not save product (ensure SKU and barcode are unique)."),
               QStringLiteral("Error al guardar en base de datos (verifica que SKU y código sean únicos)."))
        );
        return;
    }

    const QString imageFileName = sanitizeFileStem(skuEdit->text()) + QStringLiteral(".png");
    if (!selectedPhotoPath.isEmpty()) {
        (void)saveProductImagePng(selectedPhotoPath, imageFileName);
    }

    CatalogProduct catalogItem;
    catalogItem.id = saveRes.value();
    catalogItem.sku = skuEdit->text().trimmed();
    catalogItem.barcode = barcodeEdit->text().trimmed();
    catalogItem.name = nameEdit->text().trimmed();
    catalogItem.category = categoryCombo->currentText().trimmed();
    catalogItem.unitPriceClp = priceSpin->value();
    catalogItem.isWeighted = weightedCheck->isChecked();
    catalogItem.imageFile = imageFileName;

    reloadProducts();
    emit productCreated(catalogItem);
    ToastManager::showSuccess(
        tr(QStringLiteral("Product Saved"), QStringLiteral("Producto Guardado")),
        tr(QStringLiteral("\"%1\" has been added to inventory and catalog."),
           QStringLiteral("\"%1\" se agregó al inventario y al catálogo.")).arg(catalogItem.name)
    );
}

} // namespace vamp9::pos::presentation
