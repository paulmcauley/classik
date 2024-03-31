/*
 * SPDX-FileCopyrightText: 2021 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "styletraditional.h"

namespace Breeze
{

void RenderStyleTraditional18By18::renderCloseIcon()
{
    renderCloseIconAtSquareMaximizeSize();
}

void RenderStyleTraditional18By18::renderMaximizeIcon()
{
    renderSquareMaximizeIcon(false);
}

void RenderStyleTraditional18By18::renderRestoreIcon()
{
    renderOverlappingWindowsIcon();
}

void RenderStyleTraditional18By18::renderMinimizeIcon()
{
    // first determine the size of the maximize icon so the minimize icon can align with it
    auto [maximizeRect, maximizePenWidth] = renderSquareMaximizeIcon(true);
    Q_UNUSED(maximizePenWidth);
    QPen pen = m_painter->pen();
    bool isOddPenWidth = true;

    if (!m_fromKstyle) {
        qreal roundedBoldPenWidth;
        if (m_boldButtonIcons) {
            // thicker pen in titlebar
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, m_squareMaximizeBoldPenWidthFactor);
        } else
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1);
        pen.setWidthF(roundedBoldPenWidth);
    }

    // make excessively thick pen widths translucent to balance with other buttons
    qreal opacity = straightLineOpacity();
    QColor penColor = pen.color();
    penColor.setAlphaF(penColor.alphaF() * opacity);
    pen.setColor(penColor);

    m_painter->setPen(pen);

    QVector<QPointF> line;
    // horizontal line
    // original y position in design was 12.5 -- this is often too high
    if (isOddPenWidth) {
        line = {snapToNearestPixel(QPointF(4.5, maximizeRect.bottom()), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Up, ThresholdRound::Down),
                snapToNearestPixel(QPointF(13.5, maximizeRect.bottom()), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Up, ThresholdRound::Down)};

    } else {
        line = {snapToNearestPixel(QPointF(4.5, maximizeRect.bottom()), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Up, ThresholdRound::Down),
                snapToNearestPixel(QPointF(13.5, maximizeRect.bottom()), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Up, ThresholdRound::Down)};
    }

    if (m_strokeToFilledPath) {
        QPainterPath path;
        path.addPolygon(line);
        QPainterPathStroker stroker(m_painter->pen());
        path = stroker.createStroke(path);
        m_painter->setBrush(m_painter->pen().color());
        m_painter->setPen(Qt::NoPen);
        m_painter->drawPath(path);
    } else {
        m_painter->drawPolyline(line);
    }
}

// For consistency with breeze icon set
void RenderStyleTraditional18By18::renderKeepBehindIcon()
{
    renderKeepBehindIconAsFromBreezeIcons();
}

void RenderStyleTraditional18By18::renderKeepInFrontIcon()
{
    renderKeepInFrontIconAsFromBreezeIcons();
}

void RenderStyleTraditional18By18::renderContextHelpIcon()
{
    renderRounderAndBolderContextHelpIcon();
}

}
