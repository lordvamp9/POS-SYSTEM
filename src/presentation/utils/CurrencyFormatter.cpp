#include "presentation/utils/CurrencyFormatter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace vamp9::pos::presentation {

QString CurrencyFormatter::formatClp(std::int64_t amountClp) {
    const bool negative = amountClp < 0;
    const std::uint64_t absVal = negative
        ? static_cast<std::uint64_t>(-(amountClp + 1)) + 1ULL
        : static_cast<std::uint64_t>(amountClp);

    const QString rawDigits = QString::number(absVal);
    QString grouped;
    grouped.reserve(rawDigits.size() + rawDigits.size() / 3);

    const qsizetype len = rawDigits.size();
    for (qsizetype i = 0; i < len; ++i) {
        if (i > 0 && ((len - i) % 3) == 0) {
            grouped.append(QLatin1Char('.'));
        }
        grouped.append(rawDigits.at(i));
    }

    if (negative) {
        return QStringLiteral("-$ %1").arg(grouped);
    }
    return QStringLiteral("$ %1").arg(grouped);
}

QString CurrencyFormatter::formatUnitPrice(std::int64_t unitPriceClp, bool isWeighted) {
    if (isWeighted) {
        return QStringLiteral("%1/kg").arg(formatClp(unitPriceClp));
    }
    return formatClp(unitPriceClp);
}

QString CurrencyFormatter::formatQuantity(double quantity, bool isWeighted) {
    QString num = QString::number(quantity, 'f', 3);
    num.replace(QLatin1Char('.'), QLatin1Char(','));
    return isWeighted
        ? QStringLiteral("%1 kg").arg(num)
        : QStringLiteral("%1 Unid.").arg(num);
}

QString CurrencyFormatter::formatScaleWeight(double weightKg) {
    const double clamped = std::max(0.0, weightKg);
    return QStringLiteral("%1 kg").arg(QString::number(clamped, 'f', 3));
}

ClpTaxBreakdown CurrencyFormatter::breakdownFromGrossClp(
    std::int64_t grossClp,
    double ivaRate
) noexcept {
    ClpTaxBreakdown result;
    result.totalClp = grossClp;
    if (grossClp == 0 || ivaRate <= 0.0) {
        result.netClp = grossClp;
        result.ivaClp = 0;
        return result;
    }

    const double divisor = 1.0 + ivaRate;
    result.netClp = static_cast<std::int64_t>(std::llround(static_cast<double>(grossClp) / divisor));
    result.ivaClp = result.totalClp - result.netClp;
    return result;
}

std::int64_t CurrencyFormatter::calculateLineTotalClp(
    double quantity,
    std::int64_t unitPriceClp,
    double discountPercent
) noexcept {
    const double clampedDiscount = std::clamp(discountPercent, 0.0, 100.0);
    const double gross = quantity * static_cast<double>(unitPriceClp);
    const double factor = 1.0 - (clampedDiscount / 100.0);
    return static_cast<std::int64_t>(std::llround(gross * factor));
}

std::int64_t CurrencyFormatter::centsToClp(std::int64_t cents) noexcept {
    return cents / 100;
}

std::int64_t CurrencyFormatter::clpToCents(std::int64_t clp) noexcept {
    return clp * 100;
}

} // namespace vamp9::pos::presentation
