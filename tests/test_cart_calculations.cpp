#include "application/CartService.h"
#include "domain/entities/Product.h"

#include <QtTest/QtTest>

namespace {

class TestCartCalculations final : public QObject {
    Q_OBJECT

private slots:
    void emptyCartHasZeroTotals() {
        vamp9::pos::application::CartService cart(nullptr);
        QVERIFY(cart.isEmpty());

        const auto totals = cart.calculateTotals();
        QCOMPARE(totals.subtotalCents, static_cast<vamp9::pos::domain::MoneyCents>(0));
        QCOMPARE(totals.taxTotalCents, static_cast<vamp9::pos::domain::MoneyCents>(0));
        QCOMPARE(totals.grandTotalCents, static_cast<vamp9::pos::domain::MoneyCents>(0));
    }

    void placeholderSingleItemTaxAndTotal() {
        vamp9::pos::application::CartService cart(nullptr);

        vamp9::pos::domain::Product sample;
        sample.id = 1;
        sample.sku = "SKU-001";
        sample.name = "Sample Item";
        sample.unitPriceCents = 10000; // $100.00
        sample.taxRateBps = 2100;      // 21.00%

        const auto addResult = cart.addProduct(sample, 2);
        QVERIFY(addResult.hasValue());

        const auto totals = cart.calculateTotals();
        QCOMPARE(totals.subtotalCents, static_cast<vamp9::pos::domain::MoneyCents>(20000));
        QCOMPARE(totals.taxTotalCents, static_cast<vamp9::pos::domain::MoneyCents>(4200));
        QCOMPARE(totals.grandTotalCents, static_cast<vamp9::pos::domain::MoneyCents>(24200));

        cart.clear();
        QVERIFY(cart.isEmpty());
    }
};

} // namespace

QTEST_GUILESS_MAIN(TestCartCalculations)
#include "test_cart_calculations.moc"
