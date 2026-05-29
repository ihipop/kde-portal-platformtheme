// SPDX-License-Identifier: LGPL-2.0-or-later
//
// Minimal local copy/adaptation of KDE KGuiAddons color math from
// kguiaddons/src/colors/kcolorutils.cpp and kcolorspaces.cpp.

#include "kde_color_utils.h"

#include <cmath>

#include <QtGlobal>

namespace KdePortal::ColorUtils {
namespace {

qreal normalize(qreal value)
{
    if (std::isnan(value))
        return 0.0;
    return value < 1.0 ? (value > 0.0 ? value : 0.0) : 1.0;
}

qreal wrap(qreal value, qreal divisor = 1.0)
{
    const qreal result = std::fmod(value, divisor);
    return result < 0.0 ? divisor + result : (result > 0.0 ? result : 0.0);
}

qreal mixReal(qreal first, qreal second, qreal amount)
{
    return first + (second - first) * amount;
}

constexpr qreal hcyLumaRed = 0.2126;
constexpr qreal hcyLumaGreen = 0.7152;
constexpr qreal hcyLumaBlue = 0.0722;

struct HcyColor {
    explicit HcyColor(const QColor &color)
    {
        const qreal red = gamma(color.redF());
        const qreal green = gamma(color.greenF());
        const qreal blue = gamma(color.blueF());
        a = color.alphaF();

        y = lumaFromGamma(red, green, blue);

        const qreal max = qMax(qMax(red, green), blue);
        const qreal min = qMin(qMin(red, green), blue);
        const qreal delta = 6.0 * (max - min);
        if (min == max) {
            h = 0.0;
        } else if (red == max) {
            h = (green - blue) / delta;
        } else if (green == max) {
            h = (blue - red) / delta + (1.0 / 3.0);
        } else {
            h = (red - green) / delta + (2.0 / 3.0);
        }

        if (red == green && green == blue)
            c = 0.0;
        else
            c = qMax((y - min) / y, (max - y) / (1.0 - y));
    }

    HcyColor(qreal hue, qreal chroma, qreal luma, qreal alpha = 1.0)
        : h(hue)
        , c(chroma)
        , y(luma)
        , a(alpha)
    {
    }

    QColor qColor() const
    {
        const qreal hue = wrap(h);
        const qreal chroma = normalize(c);
        const qreal luma = normalize(y);
        const qreal hueSegment = hue * 6.0;

        qreal th = 0.0;
        qreal tm = 0.0;
        if (hueSegment < 1.0) {
            th = hueSegment;
            tm = hcyLumaRed + hcyLumaGreen * th;
        } else if (hueSegment < 2.0) {
            th = 2.0 - hueSegment;
            tm = hcyLumaGreen + hcyLumaRed * th;
        } else if (hueSegment < 3.0) {
            th = hueSegment - 2.0;
            tm = hcyLumaGreen + hcyLumaBlue * th;
        } else if (hueSegment < 4.0) {
            th = 4.0 - hueSegment;
            tm = hcyLumaBlue + hcyLumaGreen * th;
        } else if (hueSegment < 5.0) {
            th = hueSegment - 4.0;
            tm = hcyLumaBlue + hcyLumaRed * th;
        } else {
            th = 6.0 - hueSegment;
            tm = hcyLumaRed + hcyLumaBlue * th;
        }

        qreal tn = 0.0;
        qreal to = 0.0;
        qreal tp = 0.0;
        if (tm >= luma) {
            tp = luma + luma * chroma * (1.0 - tm) / tm;
            to = luma + luma * chroma * (th - tm) / tm;
            tn = luma - luma * chroma;
        } else {
            tp = luma + (1.0 - luma) * chroma;
            to = luma + (1.0 - luma) * chroma * (th - tm) / (1.0 - tm);
            tn = luma - (1.0 - luma) * chroma * tm / (1.0 - tm);
        }

        if (hueSegment < 1.0)
            return QColor::fromRgbF(igamma(tp), igamma(to), igamma(tn), a);
        if (hueSegment < 2.0)
            return QColor::fromRgbF(igamma(to), igamma(tp), igamma(tn), a);
        if (hueSegment < 3.0)
            return QColor::fromRgbF(igamma(tn), igamma(tp), igamma(to), a);
        if (hueSegment < 4.0)
            return QColor::fromRgbF(igamma(tn), igamma(to), igamma(tp), a);
        if (hueSegment < 5.0)
            return QColor::fromRgbF(igamma(to), igamma(tn), igamma(tp), a);
        return QColor::fromRgbF(igamma(tp), igamma(tn), igamma(to), a);
    }

    static qreal luma(const QColor &color)
    {
        return lumaFromGamma(gamma(color.redF()), gamma(color.greenF()), gamma(color.blueF()));
    }

    static qreal gamma(qreal value)
    {
        return std::pow(normalize(value), 2.2);
    }

    static qreal igamma(qreal value)
    {
        return std::pow(normalize(value), 1.0 / 2.2);
    }

    static qreal lumaFromGamma(qreal red, qreal green, qreal blue)
    {
        return red * hcyLumaRed + green * hcyLumaGreen + blue * hcyLumaBlue;
    }

    qreal h = 0.0;
    qreal c = 0.0;
    qreal y = 0.0;
    qreal a = 1.0;
};

qreal contrastRatioForLuma(qreal first, qreal second)
{
    return first > second ? (first + 0.05) / (second + 0.05) : (second + 0.05) / (first + 0.05);
}

HcyColor tintHelper(const QColor &base, qreal baseLuma, const QColor &color, qreal amount)
{
    HcyColor result(mix(base, color, std::pow(amount, 0.3)));
    result.y = mixReal(baseLuma, result.y, amount);
    return result;
}

qreal tintHelperLuma(const QColor &base, qreal baseLuma, const QColor &color, qreal amount)
{
    qreal result = luma(mix(base, color, std::pow(amount, 0.3)));
    return mixReal(baseLuma, result, amount);
}

} // namespace

qreal luma(const QColor &color)
{
    return HcyColor::luma(color);
}

QColor lighten(const QColor &color, qreal lumaAmount, qreal chromaInverseGain)
{
    HcyColor hcy(color);
    hcy.y = 1.0 - normalize((1.0 - hcy.y) * (1.0 - lumaAmount));
    hcy.c = 1.0 - normalize((1.0 - hcy.c) * chromaInverseGain);
    return hcy.qColor();
}

QColor darken(const QColor &color, qreal lumaAmount, qreal chromaGain)
{
    HcyColor hcy(color);
    hcy.y = normalize(hcy.y * (1.0 - lumaAmount));
    hcy.c = normalize(hcy.c * chromaGain);
    return hcy.qColor();
}

QColor shade(const QColor &color, qreal lumaAmount, qreal chromaAmount)
{
    HcyColor hcy(color);
    hcy.y = normalize(hcy.y + lumaAmount);
    hcy.c = normalize(hcy.c + chromaAmount);
    return hcy.qColor();
}

QColor mix(const QColor &first, const QColor &second, qreal amount)
{
    if (amount <= 0.0)
        return first;
    if (amount >= 1.0)
        return second;
    if (std::isnan(amount))
        return first;

    const qreal alpha = mixReal(first.alphaF(), second.alphaF(), amount);
    if (alpha <= 0.0)
        return Qt::transparent;

    const qreal red = qBound<qreal>(0.0, mixReal(first.redF() * first.alphaF(), second.redF() * second.alphaF(), amount), 1.0) / alpha;
    const qreal green = qBound<qreal>(0.0, mixReal(first.greenF() * first.alphaF(), second.greenF() * second.alphaF(), amount), 1.0) / alpha;
    const qreal blue = qBound<qreal>(0.0, mixReal(first.blueF() * first.alphaF(), second.blueF() * second.alphaF(), amount), 1.0) / alpha;
    return QColor::fromRgbF(red, green, blue, alpha);
}

QColor tint(const QColor &base, const QColor &color, qreal amount)
{
    if (amount <= 0.0)
        return base;
    if (amount >= 1.0)
        return color;
    if (std::isnan(amount))
        return base;

    const qreal baseLuma = luma(base);
    const qreal initialRatio = contrastRatioForLuma(baseLuma, luma(color));
    const qreal targetRatio = 1.0 + ((initialRatio + 1.0) * amount * amount * amount);
    qreal upper = 1.0;
    qreal lower = 0.0;
    qreal adjustedAmount = 0.5;
    for (int i = 12; i; --i) {
        adjustedAmount = 0.5 * (lower + upper);
        const qreal resultLuma = tintHelperLuma(base, baseLuma, color, adjustedAmount);
        const qreal adjustedRatio = contrastRatioForLuma(baseLuma, resultLuma);
        if (adjustedRatio > targetRatio)
            upper = adjustedAmount;
        else
            lower = adjustedAmount;
    }

    return tintHelper(base, baseLuma, color, adjustedAmount).qColor();
}

} // namespace KdePortal::ColorUtils
