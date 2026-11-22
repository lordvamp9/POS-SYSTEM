#include "presentation/utils/CurrencyFormatter.h"

#include <QtTest/QtTest>

namespace {

using vamp9::pos::presentation::CurrencyFormatter;

class TestCurrencyFormatter final : public QObject {
    Q_OBJECT

private slots:
    void formatsClpWithDotThousandsSeparator() {
        QCOMPARE(CurrencyFormatter::formatClp(0), QStringLiteral("$ 0"));
        QCOMPARE(CurrencyFormatter::formatClp(990), QStringLiteral("$ 990"));
        QCOMPARE(CurrencyFormatter::formatClp(2500), QStringLiteral("$ 2.500"));
        QCOMPARE(CurrencyFormatter::formatClp(12990), QStringLiteral("$ 12.990"));
        QCOMPARE(CurrencyFormatter::formatClp(1250990), QStringLiteral("$ 1.250.990"));
        QCOMPARE(CurrencyFormatter::formatClp(-2500), QStringLiteral("-$ 2.500"));
    }

    void formatsUnitAndWeightedQuantities() {
        QCOMPARE(CurrencyFormatter::formatQuantity(1.0, false), QStringLiteral("1,000 Unid."));
        QCOMPARE(CurrencyFormatter::formatQuantity(0.5, true), QStringLiteral("0,500 kg"));
        QCOMPARE(CurrencyFormatter::formatUnitPrice(1990, true), QStringLiteral("$ 1.990/kg"));
        QCOMPARE(CurrencyFormatter::formatUnitPrice(2500, false), QStringLiteral("$ 2.500"));
    }

    void computesExactChileanIva19Breakdown() {
        const auto b1 = CurrencyFormatter::breakdownFromGrossClp(11900);
        QCOMPARE(b1.netClp, static_cast<std::int64_t>(10000));
        QCOMPARE(b1.ivaClp, static_cast<std::int64_t>(1900));
        QCOMPARE(b1.totalClp, static_cast<std::int64_t>(11900));
        QCOMPARE(b1.netClp + b1.ivaClp, b1.totalClp);

        const auto b2 = CurrencyFormatter::breakdownFromGrossClp(2500);
        QCOMPARE(b2.netClp + b2.ivaClp, b2.totalClp);
        QCOMPARE(b2.totalClp, static_cast<std::int64_t>(2500));
    }

    void calculatesDiscountedWeightedLineTotal() {
        // 0.500 kg x $ 1.990/kg = $ 995
        QCOMPARE(CurrencyFormatter::calculateLineTotalClp(0.5, 1990, 0.0), static_cast<std::int64_t>(995));
        // 2 Unid. x $ 2.500 with 10% discount = $ 4.500
        QCOMPARE(CurrencyFormatter::calculateLineTotalClp(2.0, 2500, 10.0), static_cast<std::int64_t>(4500));
    }
};

} // namespace

QTEST_GUILESS_MAIN(TestCurrencyFormatter)
#include "test_currency_formatter.moc"
