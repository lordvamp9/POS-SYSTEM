#pragma once

#include <QString>

#include <cstdint>

namespace vamp9::pos::presentation {

struct ClpTaxBreakdown {
    std::int64_t netClp{0};
    std::int64_t ivaClp{0};
    std::int64_t totalClp{0};
};

// Utility formatter and deterministic integer calculator for Chilean Pesos (CLP).
// Chilean retail shelf prices include 19% IVA and carry no fractional decimals.
class CurrencyFormatter final {
public:
    static constexpr double kChileIvaRate = 0.19;

    // Formats integer CLP with dot thousands separator, e.g. 12990 -> "$ 12.990", -2500 -> "-$ 2.500".
    [[nodiscard]] static QString formatClp(std::int64_t amountClp);

    // Formats unit price including "/kg" suffix for bulk/weighted goods.
    [[nodiscard]] static QString formatUnitPrice(std::int64_t unitPriceClp, bool isWeighted);

    // Formats quantity using Chilean decimal comma with 3 decimal places, e.g. "1,000 Unid." or "0,500 kg".
    [[nodiscard]] static QString formatQuantity(double quantity, bool isWeighted);

    // Formats digital scale readout, e.g. "0.000 kg".
    [[nodiscard]] static QString formatScaleWeight(double weightKg);

    // Computes Neto + IVA (19%) from an IVA-inclusive gross CLP amount such that netClp + ivaClp == totalClp.
    [[nodiscard]] static ClpTaxBreakdown breakdownFromGrossClp(
        std::int64_t grossClp,
        double ivaRate = kChileIvaRate
    ) noexcept;

    // Computes net line total in CLP after quantity and percentage discount (0..100).
    [[nodiscard]] static std::int64_t calculateLineTotalClp(
        double quantity,
        std::int64_t unitPriceClp,
        double discountPercent = 0.0
    ) noexcept;

    // Converts domain MoneyCents (100 cents = 1 CLP unit) to integer CLP and vice versa.
    [[nodiscard]] static std::int64_t centsToClp(std::int64_t cents) noexcept;
    [[nodiscard]] static std::int64_t clpToCents(std::int64_t clp) noexcept;
};

} // namespace vamp9::pos::presentation
