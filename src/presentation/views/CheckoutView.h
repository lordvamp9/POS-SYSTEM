#pragma once

#include "application/CartService.h"
#include "domain/interfaces/IProductRepository.h"
#include "domain/interfaces/IReceiptPrinter.h"
#include "presentation/views/ReportsView.h"
#include "presentation/widgets/ProductCardWidget.h"
#include "presentation/widgets/TicketItemDelegate.h"

#include <QString>
#include <QWidget>

#include <memory>
#include <vector>

class QButtonGroup;
class QGridLayout;
class QLabel;
class QListWidget;
class QPushButton;

namespace vamp9::pos::presentation {

// High-traffic Chilean supermarket POS checkout screen (38% Operation/Payment | 62% Visual Catalog).
class CheckoutView final : public QWidget {
    Q_OBJECT

public:
    enum class NumpadMode {
        Quantity,
        Discount,
        Price
    };

    CheckoutView(
        std::shared_ptr<application::CartService> cartService,
        std::shared_ptr<domain::IProductRepository> productRepository,
        std::shared_ptr<domain::IReceiptPrinter> receiptPrinter,
        QWidget* parent = nullptr
    );
    ~CheckoutView() override = default;

public slots:
    void handleBarcodeScanned(const QString& code);
    void setScaleWeightKg(double weightKg);
    void setSearchFilter(const QString& query);
    void submitSearchQuery(const QString& query);
    void addCatalogProduct(const CatalogProduct& product);
    void retranslateUi();
    void refreshThemeVisuals();

signals:
    void scaleResetRequested();
    void saleCompleted(const SaleReportEntry& entry);

private:
    void setupUi();
    QWidget* buildLeftOperationPanel();
    QWidget* buildRightCatalogPanel();

    void initializeChileanCatalog();
    void preloadSampleTicket();
    void seedRepositoryIfEmpty();
    void rebuildProductGrid();

    bool addProductToTicket(const CatalogProduct& product, double explicitQty = 0.0, bool playSound = true);
    void refreshTicketList(int selectRow = -1);
    void refreshFinancialSummary();
    void updateNumpadInputPreview();

    // Quick actions
    void handleClearTicket();
    void handleToggleRefundMode(bool checked);
    void handleAttachNote();
    void handleQuickDiscount();
    void handleManualCodeEntry();
    void handleToggleDocumentType();
    void handleOpenPaymentDialog();

    // Numpad handlers
    void setNumpadMode(NumpadMode mode);
    void handleNumpadDigit(const QString& token);
    void handleNumpadSignToggle();
    void handleNumpadBackspace();
    void applyNumpadBufferToSelectedRow();

    static void playPosAddBeep();

    std::shared_ptr<application::CartService> cartService_;
    std::shared_ptr<domain::IProductRepository> productRepository_;
    std::shared_ptr<domain::IReceiptPrinter> receiptPrinter_;

    std::vector<CatalogProduct> catalog_;
    std::vector<TicketLineItem> ticketLines_;

    QString activeCategory_{QStringLiteral("Todos")};
    QString searchQuery_;
    double currentScaleKg_{0.0};
    bool refundMode_{false};
    bool invoiceMode_{false}; // false = Boleta, true = Factura
    QString customerRut_;
    QString ticketNote_;

    NumpadMode numpadMode_{NumpadMode::Quantity};
    QString numpadBuffer_;

    // Left operation widgets
    QLabel* ticketTitleLabel_{nullptr};
    QListWidget* ticketList_{nullptr};
    QLabel* itemCountBadge_{nullptr};
    QLabel* docTypeBadge_{nullptr};
    QLabel* numpadStatusLabel_{nullptr};
    QLabel* netCaptionLabel_{nullptr};
    QLabel* ivaCaptionLabel_{nullptr};
    QLabel* netValueLabel_{nullptr};
    QLabel* ivaValueLabel_{nullptr};
    QLabel* totalValueLabel_{nullptr};
    QPushButton* clearBtn_{nullptr};
    QPushButton* refundBtn_{nullptr};
    QPushButton* noteBtn_{nullptr};
    QPushButton* discountBtn_{nullptr};
    QPushButton* codeBtn_{nullptr};
    QPushButton* customerRutBtn_{nullptr};
    QPushButton* payBtn_{nullptr};

    QButtonGroup* numpadModeGroup_{nullptr};
    QPushButton* qtyModeBtn_{nullptr};
    QPushButton* discModeBtn_{nullptr};
    QPushButton* priceModeBtn_{nullptr};
    QPushButton* backspaceBtn_{nullptr};

    // Right catalog widgets
    QPushButton* homeBreadcrumbBtn_{nullptr};
    QButtonGroup* categoryGroup_{nullptr};
    std::vector<QPushButton*> categoryButtons_;
    QWidget* gridContainer_{nullptr};
    QGridLayout* productGrid_{nullptr};
    QLabel* catalogCountLabel_{nullptr};
};

} // namespace vamp9::pos::presentation
