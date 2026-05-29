// SPDX-License-Identifier: LGPL-2.0-or-later
//
// Declarations for the local KDE KGuiAddons color utility subset.

#pragma once

#include <QColor>

namespace KdePortal::ColorUtils {

qreal luma(const QColor &color);
QColor lighten(const QColor &color, qreal lumaAmount, qreal chromaInverseGain = 1.0);
QColor darken(const QColor &color, qreal lumaAmount, qreal chromaGain = 1.0);
QColor shade(const QColor &color, qreal lumaAmount, qreal chromaAmount = 0.0);
QColor mix(const QColor &first, const QColor &second, qreal amount);
QColor tint(const QColor &base, const QColor &color, qreal amount);

} // namespace KdePortal::ColorUtils
