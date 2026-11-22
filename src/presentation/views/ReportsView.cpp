#include "presentation/views/ReportsView.h"

#include "presentation/utils/AppPreferences.h"
#include "presentation/utils/CurrencyFormatter.h"
#include "presentation/widgets/ToastManager.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPushButton>
#include <QTableWidget>
#include <QTextDocument>
#include <QTextStream>
#include <QVBoxLayout>

namespace vamp9::pos::presentation {

namespace {

void applyCardShadow(QWidget* widget) {
    auto* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(12.0);
    shadow->setOffset(0.0, 2.0);
    shadow->setColor(QColor(15, 23, 42, 14));
    widget->setGraphicsEffect(shadow);
}

QFrame* createKpiCard(QLabel*& labelOut, QLabel*& valueOut, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setProperty("glass", QStringLiteral("card"));
    applyCardShadow(card);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(6);

    labelOut = new QLabel(card);
    labelOut->setObjectName(QStringLiteral("kpiLabel"));

    valueOut = new QLabel(QStringLiteral("$ 0"), card);
    valueOut->setObjectName(QStringLiteral("kpiValue"));

    layout->addWidget(labelOut);
    layout->addWidget(valueOut);
    return card;
}

QString escapeCsvField(const QString& field) {
    QString out = field;
    out.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    if (out.contains(QLatin1Char(';')) || out.contains(QLatin1Char('"')) || out.contains(QLatin1Char('\n'))) {
        return QStringLiteral("\"%1\"").arg(out);
    }
    return out;
}

} // namespace

ReportsView::ReportsView(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    seedInitialShiftSales();
    retranslateUi();
    refreshDisplay();
}

void ReportsView::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(14);

    auto* kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(14);

    kpiRow->addWidget(createKpiCard(totalSalesLabel_, totalSalesKpi_, this), 1);
    kpiRow->addWidget(createKpiCard(ticketsCountLabel_, ticketsCountKpi_, this), 1);
    kpiRow->addWidget(createKpiCard(netTotalLabel_, netTotalKpi_, this), 1);
    kpiRow->addWidget(createKpiCard(ivaTotalLabel_, ivaTotalKpi_, this), 1);

    rootLayout->addLayout(kpiRow);

    auto* reportPanel = new QFrame(this);
    reportPanel->setProperty("surface", QStringLiteral("panel"));
    applyCardShadow(reportPanel);

    auto* panelLayout = new QVBoxLayout(reportPanel);
    panelLayout->setContentsMargins(20, 18, 20, 18);
    panelLayout->setSpacing(14);

    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(10);

    reportTitleLabel_ = new QLabel(reportPanel);
    reportTitleLabel_->setObjectName(QStringLiteral("sectionTitle"));

    exportCsvBtn_ = new QPushButton(reportPanel);
    exportCsvBtn_->setProperty("variant", QStringLiteral("glass"));
    exportCsvBtn_->setCursor(Qt::PointingHandCursor);

    exportPdfBtn_ = new QPushButton(reportPanel);
    exportPdfBtn_->setProperty("variant", QStringLiteral("primary"));
    exportPdfBtn_->setCursor(Qt::PointingHandCursor);

    headerRow->addWidget(reportTitleLabel_);
    headerRow->addStretch();
    headerRow->addWidget(exportCsvBtn_);
    headerRow->addWidget(exportPdfBtn_);
    panelLayout->addLayout(headerRow);

    salesTable_ = new QTableWidget(0, 8, reportPanel);
    salesTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    salesTable_->verticalHeader()->setVisible(false);
    salesTable_->setAlternatingRowColors(true);
    salesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    salesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    panelLayout->addWidget(salesTable_, 1);
    rootLayout->addWidget(reportPanel, 1);

    connect(exportCsvBtn_, &QPushButton::clicked, this, &ReportsView::handleExportCsv);
    connect(exportPdfBtn_, &QPushButton::clicked, this, &ReportsView::handleExportPdf);
}

void ReportsView::retranslateUi() {
    auto tr = &AppPreferences::trText;
    if (totalSalesLabel_) {
        totalSalesLabel_->setText(tr(QStringLiteral("TODAY'S SALES"), QStringLiteral("VENTAS DEL DÍA")));
    }
    if (ticketsCountLabel_) {
        ticketsCountLabel_->setText(tr(QStringLiteral("DOCUMENTS ISSUED"), QStringLiteral("DOCUMENTOS EMITIDOS")));
    }
    if (netTotalLabel_) {
        netTotalLabel_->setText(tr(QStringLiteral("ACCUMULATED NET"), QStringLiteral("NETO ACUMULADO")));
    }
    if (ivaTotalLabel_) {
        ivaTotalLabel_->setText(tr(QStringLiteral("VAT (19%)"), QStringLiteral("IVA (19%)")));
    }
    if (reportTitleLabel_) {
        reportTitleLabel_->setText(tr(
            QStringLiteral("Sales Summary & Shift Close — Register 01"),
            QStringLiteral("Resumen de Ventas y Cierre de Turno — Caja 01")
        ));
    }
    if (exportCsvBtn_) {
        exportCsvBtn_->setText(tr(QStringLiteral("Export CSV"), QStringLiteral("Exportar CSV")));
    }
    if (exportPdfBtn_) {
        exportPdfBtn_->setText(tr(QStringLiteral("Export PDF"), QStringLiteral("Exportar PDF")));
    }
    if (salesTable_) {
        salesTable_->setHorizontalHeaderLabels({
            tr(QStringLiteral("Document #"), QStringLiteral("N° Documento")),
            tr(QStringLiteral("Type"), QStringLiteral("Tipo")),
            tr(QStringLiteral("Date / Time"), QStringLiteral("Fecha / Hora")),
            tr(QStringLiteral("Payment Method"), QStringLiteral("Medio de Pago")),
            tr(QStringLiteral("Items"), QStringLiteral("Ítems")),
            tr(QStringLiteral("Net"), QStringLiteral("Neto")),
            tr(QStringLiteral("VAT (19%)"), QStringLiteral("IVA (19%)")),
            QStringLiteral("Total")
        });
    }
}

void ReportsView::seedInitialShiftSales() {
    const QString today = QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy"));

    auto makeEntry = [&](const QString& num, const QString& docType, const QString& timeStr,
                         const QString& method, int items, std::int64_t totalClp) {
        const auto bd = CurrencyFormatter::breakdownFromGrossClp(totalClp);
        SaleReportEntry e;
        e.receiptNumber = num;
        e.documentType = docType;
        e.timestamp = QStringLiteral("%1 %2").arg(today, timeStr);
        e.paymentMethod = method;
        e.itemCount = items;
        e.netClp = bd.netClp;
        e.ivaClp = bd.ivaClp;
        e.totalClp = bd.totalClp;
        return e;
    };

    sales_ = {
        makeEntry(QStringLiteral("BOL-0001"), QStringLiteral("Receipt"), QStringLiteral("09:14"), QStringLiteral("Card"),     4, 12870),
        makeEntry(QStringLiteral("BOL-0002"), QStringLiteral("Receipt"), QStringLiteral("09:42"), QStringLiteral("Cash"),     3,  6580),
        makeEntry(QStringLiteral("FAC-0003"), QStringLiteral("Invoice"), QStringLiteral("10:18"), QStringLiteral("Transfer"), 6, 21450)
    };
}

void ReportsView::recordSale(const SaleReportEntry& entry) {
    sales_.push_back(entry);
    refreshDisplay();
}

void ReportsView::refreshDisplay() {
    std::int64_t totalGross = 0;
    std::int64_t totalNet = 0;
    std::int64_t totalIva = 0;

    for (const auto& s : sales_) {
        totalGross += s.totalClp;
        totalNet += s.netClp;
        totalIva += s.ivaClp;
    }

    if (totalSalesKpi_) {
        totalSalesKpi_->setText(CurrencyFormatter::formatClp(totalGross));
    }
    if (ticketsCountKpi_) {
        ticketsCountKpi_->setText(QString::number(sales_.size()));
    }
    if (netTotalKpi_) {
        netTotalKpi_->setText(CurrencyFormatter::formatClp(totalNet));
    }
    if (ivaTotalKpi_) {
        ivaTotalKpi_->setText(CurrencyFormatter::formatClp(totalIva));
    }

    if (!salesTable_) {
        return;
    }

    salesTable_->setRowCount(static_cast<int>(sales_.size()));
    for (int row = 0; row < static_cast<int>(sales_.size()); ++row) {
        const auto& s = sales_[static_cast<std::size_t>(row)];
        salesTable_->setItem(row, 0, new QTableWidgetItem(s.receiptNumber));
        salesTable_->setItem(row, 1, new QTableWidgetItem(s.documentType));
        salesTable_->setItem(row, 2, new QTableWidgetItem(s.timestamp));
        salesTable_->setItem(row, 3, new QTableWidgetItem(s.paymentMethod));
        salesTable_->setItem(row, 4, new QTableWidgetItem(QString::number(s.itemCount)));
        salesTable_->setItem(row, 5, new QTableWidgetItem(CurrencyFormatter::formatClp(s.netClp)));
        salesTable_->setItem(row, 6, new QTableWidgetItem(CurrencyFormatter::formatClp(s.ivaClp)));
        salesTable_->setItem(row, 7, new QTableWidgetItem(CurrencyFormatter::formatClp(s.totalClp)));
    }
}

void ReportsView::handleExportCsv() {
    auto tr = &AppPreferences::trText;
    const QString defaultName = QStringLiteral("vamp9_sales_report_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr(QStringLiteral("Export Sales Report to CSV"), QStringLiteral("Exportar Reporte de Ventas a CSV")),
        defaultName,
        tr(QStringLiteral("CSV File (*.csv)"), QStringLiteral("Archivo CSV (*.csv)"))
    );
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ToastManager::showError(
            tr(QStringLiteral("CSV Export Failed"), QStringLiteral("Error al exportar CSV")),
            tr(QStringLiteral("Unable to write file: %1"), QStringLiteral("No se pudo escribir el archivo: %1")).arg(QFileInfo(filePath).fileName())
        );
        return;
    }

    file.write("\xEF\xBB\xBF");

    QTextStream out(&file);
    out << tr(
        QStringLiteral("Document #;Type;Date / Time;Payment Method;Items;Net CLP;VAT 19% CLP;Total CLP\n"),
        QStringLiteral("N° Documento;Tipo;Fecha / Hora;Medio de Pago;Ítems;Neto CLP;IVA 19% CLP;Total CLP\n")
    );

    std::int64_t sumNet = 0;
    std::int64_t sumIva = 0;
    std::int64_t sumTotal = 0;
    int sumItems = 0;

    for (const auto& s : sales_) {
        out << escapeCsvField(s.receiptNumber) << ';'
            << escapeCsvField(s.documentType) << ';'
            << escapeCsvField(s.timestamp) << ';'
            << escapeCsvField(s.paymentMethod) << ';'
            << s.itemCount << ';'
            << s.netClp << ';'
            << s.ivaClp << ';'
            << s.totalClp << '\n';
        sumItems += s.itemCount;
        sumNet += s.netClp;
        sumIva += s.ivaClp;
        sumTotal += s.totalClp;
    }

    out << "TOTAL;;;;" << sumItems << ';' << sumNet << ';' << sumIva << ';' << sumTotal << '\n';
    file.close();

    ToastManager::showSuccess(
        tr(QStringLiteral("CSV Exported"), QStringLiteral("CSV Exportado")),
        tr(QStringLiteral("Saved %1 successfully."), QStringLiteral("Archivo %1 guardado correctamente.")).arg(QFileInfo(filePath).fileName())
    );
}

void ReportsView::handleExportPdf() {
    auto tr = &AppPreferences::trText;
    const QString defaultName = QStringLiteral("vamp9_sales_report_%1.pdf")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr(QStringLiteral("Export Sales Report to PDF"), QStringLiteral("Exportar Reporte de Ventas a PDF")),
        defaultName,
        tr(QStringLiteral("PDF Document (*.pdf)"), QStringLiteral("Documento PDF (*.pdf)"))
    );
    if (filePath.isEmpty()) {
        return;
    }

    std::int64_t sumNet = 0;
    std::int64_t sumIva = 0;
    std::int64_t sumTotal = 0;
    for (const auto& s : sales_) {
        sumNet += s.netClp;
        sumIva += s.ivaClp;
        sumTotal += s.totalClp;
    }

    QString rowsHtml;
    for (const auto& s : sales_) {
        rowsHtml += QStringLiteral(
            "<tr>"
            "<td>%1</td><td>%2</td><td>%3</td><td>%4</td>"
            "<td align='center'>%5</td>"
            "<td align='right'>%6</td><td align='right'>%7</td>"
            "<td align='right'><b>%8</b></td>"
            "</tr>"
        )
            .arg(s.receiptNumber.toHtmlEscaped())
            .arg(s.documentType.toHtmlEscaped())
            .arg(s.timestamp.toHtmlEscaped())
            .arg(s.paymentMethod.toHtmlEscaped())
            .arg(s.itemCount)
            .arg(CurrencyFormatter::formatClp(s.netClp).toHtmlEscaped())
            .arg(CurrencyFormatter::formatClp(s.ivaClp).toHtmlEscaped())
            .arg(CurrencyFormatter::formatClp(s.totalClp).toHtmlEscaped());
    }

    const QString generatedAt = QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"));
    const QString html = QStringLiteral(
        "<html><head><style>"
        "body { font-family: 'Segoe UI', Arial, sans-serif; color: #0F172A; font-size: 10pt; }"
        "h1 { color: #1D4ED8; font-size: 18pt; margin-bottom: 2px; }"
        ".meta { color: #64748B; font-size: 9pt; margin-bottom: 16px; }"
        ".kpi-table { width: 100%%; border-collapse: separate; border-spacing: 8px; margin-bottom: 16px; }"
        ".kpi-cell { background: #F8FAFC; border: 1px solid #E2E8F0; padding: 10px; }"
        ".kpi-label { font-size: 8pt; color: #64748B; font-weight: bold; }"
        ".kpi-val { font-size: 14pt; color: #0F172A; font-weight: bold; margin-top: 4px; }"
        ".data-table { width: 100%%; border-collapse: collapse; margin-top: 8px; }"
        ".data-table th { background: #EFF6FF; color: #1E3A8A; border-bottom: 2px solid #2563EB; padding: 7px 6px; text-align: left; font-size: 9pt; }"
        ".data-table td { border-bottom: 1px solid #E2E8F0; padding: 6px; font-size: 9pt; }"
        ".total-row td { background: #F1F5F9; font-weight: bold; border-top: 2px solid #94A3B8; padding: 8px 6px; }"
        "</style></head><body>"
        "<h1>vamp9 POS — %1</h1>"
        "<div class='meta'>%2 &nbsp;|&nbsp; %3</div>"
        "<table class='kpi-table'><tr>"
        "<td class='kpi-cell'><div class='kpi-label'>%4</div><div class='kpi-val'>%8</div></td>"
        "<td class='kpi-cell'><div class='kpi-label'>%5</div><div class='kpi-val'>%9</div></td>"
        "<td class='kpi-cell'><div class='kpi-label'>%6</div><div class='kpi-val'>%10</div></td>"
        "<td class='kpi-cell'><div class='kpi-label'>%7</div><div class='kpi-val'>%11</div></td>"
        "</tr></table>"
        "<table class='data-table'>"
        "<thead><tr>"
        "<th>Doc #</th><th>%12</th><th>%13</th><th>%14</th>"
        "<th>%15</th><th>%6</th><th>%7</th><th>Total</th>"
        "</tr></thead><tbody>%16"
        "<tr class='total-row'>"
        "<td colspan='5'>TOTAL</td>"
        "<td align='right'>%10</td><td align='right'>%11</td><td align='right'>%8</td>"
        "</tr></tbody></table>"
        "</body></html>"
    )
        .arg(tr(QStringLiteral("Sales Report"), QStringLiteral("Reporte de Ventas")))
        .arg(tr(QStringLiteral("Register 01 · AM Shift"), QStringLiteral("Caja 01 · Turno AM")))
        .arg(generatedAt)
        .arg(tr(QStringLiteral("TODAY'S SALES"), QStringLiteral("VENTAS DEL DÍA")))
        .arg(tr(QStringLiteral("DOCUMENTS"), QStringLiteral("DOCUMENTOS")))
        .arg(tr(QStringLiteral("NET"), QStringLiteral("NETO")))
        .arg(tr(QStringLiteral("VAT (19%)"), QStringLiteral("IVA (19%)")))
        .arg(CurrencyFormatter::formatClp(sumTotal).toHtmlEscaped())
        .arg(sales_.size())
        .arg(CurrencyFormatter::formatClp(sumNet).toHtmlEscaped())
        .arg(CurrencyFormatter::formatClp(sumIva).toHtmlEscaped())
        .arg(tr(QStringLiteral("Type"), QStringLiteral("Tipo")))
        .arg(tr(QStringLiteral("Date / Time"), QStringLiteral("Fecha / Hora")))
        .arg(tr(QStringLiteral("Payment"), QStringLiteral("Medio de Pago")))
        .arg(tr(QStringLiteral("Items"), QStringLiteral("Ítems")))
        .arg(rowsHtml);

    QPdfWriter pdfWriter(filePath);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageOrientation(QPageLayout::Portrait);
    pdfWriter.setPageMargins(QMarginsF(14.0, 14.0, 14.0, 14.0), QPageLayout::Millimeter);
    pdfWriter.setResolution(150);

    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&pdfWriter);

    ToastManager::showSuccess(
        tr(QStringLiteral("PDF Exported"), QStringLiteral("PDF Exportado")),
        tr(QStringLiteral("Saved %1 successfully."), QStringLiteral("Archivo %1 guardado correctamente.")).arg(QFileInfo(filePath).fileName())
    );
}

} // namespace vamp9::pos::presentation
