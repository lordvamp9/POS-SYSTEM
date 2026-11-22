#pragma once

#include <QString>
#include <QWidget>

#include <cstdint>
#include <vector>

class QLabel;
class QPushButton;
class QTableWidget;

namespace vamp9::pos::presentation {

struct SaleReportEntry {
    QString receiptNumber;
    QString documentType;  // "Boleta" / "Factura"
    QString timestamp;     // "dd/MM/yyyy HH:mm"
    QString paymentMethod; // "Efectivo" / "Débito / Crédito" / "Transferencia"
    int itemCount{0};
    std::int64_t netClp{0};
    std::int64_t ivaClp{0};
    std::int64_t totalClp{0};
};

class ReportsView final : public QWidget {
    Q_OBJECT

public:
    explicit ReportsView(QWidget* parent = nullptr);
    ~ReportsView() override = default;

public slots:
    void recordSale(const SaleReportEntry& entry);
    void retranslateUi();

private:
    void setupUi();
    void seedInitialShiftSales();
    void refreshDisplay();
    void handleExportCsv();
    void handleExportPdf();

    std::vector<SaleReportEntry> sales_;

    QLabel* totalSalesLabel_{nullptr};
    QLabel* ticketsCountLabel_{nullptr};
    QLabel* netTotalLabel_{nullptr};
    QLabel* ivaTotalLabel_{nullptr};
    QLabel* totalSalesKpi_{nullptr};
    QLabel* ticketsCountKpi_{nullptr};
    QLabel* netTotalKpi_{nullptr};
    QLabel* ivaTotalKpi_{nullptr};
    QLabel* reportTitleLabel_{nullptr};
    QPushButton* exportCsvBtn_{nullptr};
    QPushButton* exportPdfBtn_{nullptr};
    QTableWidget* salesTable_{nullptr};
};

} // namespace vamp9::pos::presentation
