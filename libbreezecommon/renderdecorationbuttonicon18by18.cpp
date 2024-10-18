/*
 * SPDX-FileCopyrightText: 2022-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "renderdecorationbuttonicon18by18.h"
#include <QGraphicsItem>
#include <QPainterPathStroker>

namespace Breeze
{

RenderDecorationButtonIcon18By18::RenderDecorationButtonIcon18By18(QPainter *painter,
                                                                   const bool fromKstyle,
                                                                   const bool boldButtonIcons,
                                                                   const qreal devicePixelRatio,
                                                                   const QPointF &deviceOffsetFromZeroReference,
                                                                   const bool forceEvenSquares)
    : RenderDecorationButtonIcon(painter, fromKstyle, boldButtonIcons, devicePixelRatio, deviceOffsetFromZeroReference, forceEvenSquares)
{
}

/* most base methods here are KDE's default Breeze/Oxygen style -- override with other styles */
void RenderDecorationButtonIcon18By18::renderCloseIcon()
{
    QVector<QPointF> line1{QPointF(5, 5), QPointF(13, 13)};
    QVector<QPointF> line2{QPointF(13, 5), QPointF(5, 13)};

    if (m_strokeToFilledPath) {
        QPainterPath path1, path2;
        path1.addPolygon(line1);
        path2.addPolygon(line2);
        QPainterPathStroker stroker(m_painter->pen());
        path1 = stroker.createStroke(path1);
        path2 = stroker.createStroke(path2);
        m_painter->setBrush(m_painter->pen().color());
        m_painter->setPen(Qt::NoPen);
        m_painter->drawPath(path1);
        m_painter->drawPath(path2);
    } else {
        m_painter->drawPolyline(line1);
        m_painter->drawPolyline(line2);
    }
}

void RenderDecorationButtonIcon18By18::renderMaximizeIcon()
{
    // up arrow
    QVector<QPointF> line{QPointF(4, 11), QPointF(9, 6), QPointF(14, 11)};

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

void RenderDecorationButtonIcon18By18::renderRestoreIcon()
{
    QPen pen = m_painter->pen();
    pen.setJoinStyle(Qt::RoundJoin);
    m_painter->setPen(pen);

    QVector<QPointF> diamond;
    if (m_fromKstyle) { // slightly smaller diamond
        // diamond / floating kite
        diamond = QVector<QPointF>{QPointF(4.5, 9), QPointF(9, 4.5), QPointF(13.5, 9), QPointF(9, 13.5)};

    } else {
        // diamond / floating kite
        diamond = QVector<QPointF>{QPointF(4, 9), QPointF(9, 4), QPointF(14, 9), QPointF(9, 14)};
    }

    if (m_strokeToFilledPath) {
        QPainterPath path;
        path.addPolygon(diamond);
        path.closeSubpath();
        QPainterPathStroker stroker(m_painter->pen());
        path = stroker.createStroke(path);
        m_painter->setBrush(m_painter->pen().color());
        m_painter->setPen(Qt::NoPen);
        m_painter->drawPath(path);
    } else {
        m_painter->drawConvexPolygon(diamond);
    }
}

void RenderDecorationButtonIcon18By18::renderMinimizeIcon()
{
    // down arrow
    QVector<QPointF> line{QPointF(4, 7), QPointF(9, 12), QPointF(14, 7)};

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

void RenderDecorationButtonIcon18By18::renderPinnedOnAllDesktopsIcon()
{
    QPen pen = m_painter->pen();
    QColor color = pen.color();
    color.setAlphaF(color.alphaF() * 0.8);
    m_painter->setBrush(color);
    m_painter->setPen(Qt::NoPen);

    QPainterPath outerRing;
    outerRing.addEllipse(QRectF(3, 3, 12, 12));

    QPainterPath innerDot;
    innerDot.addEllipse(QRectF(8, 8, 2, 2));

    outerRing = outerRing.subtracted(innerDot);

    m_painter->drawPath(outerRing);
}

void RenderDecorationButtonIcon18By18::renderPinOnAllDesktopsIcon()
{
    QPen pen = m_painter->pen();
    QColor color = pen.color();
    color.setAlphaF(color.alphaF() * 0.8);
    m_painter->setBrush(color);
    m_painter->setPen(Qt::NoPen);
    m_painter->drawConvexPolygon(QVector<QPointF>{QPointF(6.5, 8.5), QPointF(12, 3), QPointF(15, 6), QPointF(9.5, 11.5)});

    pen.setColor(color);
    m_painter->setPen(pen);
    QVector<QPointF> line1{QPointF(5.5, 7.5), QPointF(10.5, 12.5)};
    QVector<QPointF> line2{QPointF(8, 10), QPointF(4.5, 13.5)};

    if (m_strokeToFilledPath) {
        QPainterPath path1, path2;
        path1.addPolygon(line1);
        path2.addPolygon(line2);
        QPainterPathStroker stroker(m_painter->pen());
        path1 = stroker.createStroke(path1);
        path2 = stroker.createStroke(path2);
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(pen.color());
        m_painter->drawPath(path1);
        m_painter->drawPath(path2);
    } else {
        m_painter->drawPolyline(line1);
        m_painter->drawPolyline(line2);
    }
}

void RenderDecorationButtonIcon18By18::renderShadeIcon()
{
    QPen pen = m_painter->pen();
    if (m_totalScalingFactor < 1.3)
        pen.setCapStyle(Qt::FlatCap); // prevents lobsided arrow at loDPI

    bool isOddPenWidth = true;
    if (!m_fromKstyle) {
        qreal roundedBoldPenWidth = 1;
        if (m_boldButtonIcons) {
            // thicker pen in titlebar
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1.3);
        } else {
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1);
        }
        pen.setWidthF(roundedBoldPenWidth);
        m_painter->setPen(pen);
    }

    QVector<QPointF> line1, line2;

    if (isOddPenWidth) {
        line1 = {snapToNearestPixel(QPointF(4, 4.5), SnapPixel::ToWhole, SnapPixel::ToHalf),
                 snapToNearestPixel(QPointF(14, 4.5), SnapPixel::ToWhole, SnapPixel::ToHalf)};
        line2 = {QPointF(4, 13), QPointF(9, 8), QPointF(14, 13)};
    } else {
        line1 = {snapToNearestPixel(QPointF(4, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole),
                 snapToNearestPixel(QPointF(14, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole)};
        line2 = {QPointF(4, 13), QPointF(9, 8), QPointF(14, 13)};
    }

    if (m_strokeToFilledPath) {
        QPainterPath path1, path2;
        path1.addPolygon(line1);
        path2.addPolygon(line2);
        QPainterPathStroker stroker(m_painter->pen());
        path1 = stroker.createStroke(path1);
        path2 = stroker.createStroke(path2);
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(pen.color());
        m_painter->drawPath(path1);
        m_painter->drawPath(path2);
    } else {
        m_painter->drawPolyline(line1);
        m_painter->drawPolyline(line2);
    }
}

void RenderDecorationButtonIcon18By18::renderUnShadeIcon()
{
    QPen pen = m_painter->pen();

    if (m_totalScalingFactor < 1.3)
        pen.setCapStyle(Qt::FlatCap); // prevents lobsided arrow at loDPI

    bool isOddPenWidth = true;
    if (!m_fromKstyle) {
        qreal roundedBoldPenWidth = 1;
        if (m_boldButtonIcons) {
            // thicker pen in titlebar
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1.3);
        } else {
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1);
        }
        pen.setWidthF(roundedBoldPenWidth);
        m_painter->setPen(pen);
    }

    QVector<QPointF> line1, line2;

    if (isOddPenWidth) {
        line1 = {snapToNearestPixel(QPointF(4, 4.5), SnapPixel::ToWhole, SnapPixel::ToHalf),
                 snapToNearestPixel(QPointF(14, 4.5), SnapPixel::ToWhole, SnapPixel::ToHalf)};
        line2 = {QPointF(4, 8), QPointF(9, 13), QPointF(14, 8)};
    } else {
        line1 = {snapToNearestPixel(QPointF(4, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole),
                 snapToNearestPixel(QPointF(14, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole)};
        line2 = {QPointF(4, 8), QPointF(9, 13), QPointF(14, 8)};
    }

    if (m_strokeToFilledPath) {
        QPainterPath path1, path2;
        path1.addPolygon(line1);
        path2.addPolygon(line2);
        QPainterPathStroker stroker(m_painter->pen());
        path1 = stroker.createStroke(path1);
        path2 = stroker.createStroke(path2);
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(pen.color());
        m_painter->drawPath(path1);
        m_painter->drawPath(path2);
    } else {
        m_painter->drawPolyline(line1);
        m_painter->drawPolyline(line2);
    }
}

void RenderDecorationButtonIcon18By18::renderKeepBehindIcon()
{
    QVector<QPointF> line1, line2;

    // two down arrows
    line1 = {QPointF(4, 5), QPointF(9, 10), QPointF(14, 5)};
    line2 = {QPointF(4, 9), QPointF(9, 14), QPointF(14, 9)};

    if (m_strokeToFilledPath) {
        QPainterPath path1, path2;
        path1.addPolygon(line1);
        path2.addPolygon(line2);
        QPainterPathStroker stroker(m_painter->pen());
        path1 = stroker.createStroke(path1);
        path2 = stroker.createStroke(path2);
        m_painter->setBrush(m_painter->pen().color());
        m_painter->setPen(Qt::NoPen);
        m_painter->drawPath(path1);
        m_painter->drawPath(path2);
    } else {
        m_painter->drawPolyline(line1);
        m_painter->drawPolyline(line2);
    }
}

void RenderDecorationButtonIcon18By18::renderKeepInFrontIcon()
{
    QVector<QPointF> line1, line2;

    // two up arrows
    line1 = {QPointF(4, 9), QPointF(9, 4), QPointF(14, 9)};
    line2 = {QPointF(4, 13), QPointF(9, 8), QPointF(14, 13)};

    if (m_strokeToFilledPath) {
        QPainterPath path1, path2;
        path1.addPolygon(line1);
        path2.addPolygon(line2);
        QPainterPathStroker stroker(m_painter->pen());
        path1 = stroker.createStroke(path1);
        path2 = stroker.createStroke(path2);
        m_painter->setBrush(m_painter->pen().color());
        m_painter->setPen(Qt::NoPen);
        m_painter->drawPath(path1);
        m_painter->drawPath(path2);
    } else {
        m_painter->drawPolyline(line1);
        m_painter->drawPolyline(line2);
    }
}

void RenderDecorationButtonIcon18By18::renderApplicationMenuIcon()
{
    bool isOddPenWidth = true;

    QPen pen = m_painter->pen();
    pen.setCapStyle(Qt::PenCapStyle::FlatCap);

    qreal roundedBoldPenWidth = 1;
    qreal boldingFactor = m_boldButtonIcons ? m_squareMaximizeBoldPenWidthFactor : 1;
    isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, boldingFactor);

    pen.setWidthF(roundedBoldPenWidth);
    m_painter->setPen(pen);
    m_painter->setBrush(Qt::NoBrush);

    std::unique_ptr<QGraphicsScene> appMenu = std::unique_ptr<QGraphicsScene>(new QGraphicsScene(0, 0, 18, 18));

    QGraphicsItemGroup *appMenuGroup = new QGraphicsItemGroup;

    appMenu->addItem(appMenuGroup);
    QGraphicsPathItem *top = new QGraphicsPathItem;
    QGraphicsPathItem *middle = new QGraphicsPathItem;
    QGraphicsPathItem *bottom = new QGraphicsPathItem;
    appMenuGroup->addToGroup(top);
    appMenuGroup->addToGroup(middle);
    appMenuGroup->addToGroup(bottom);

    if (isOddPenWidth) {
        QPointF leftTop = snapToNearestPixel(QPointF(3.5, 4.5), SnapPixel::ToWhole, SnapPixel::ToHalf);
        QPointF leftMiddle = snapToNearestPixel(QPointF(3.5, 8.5), SnapPixel::ToWhole, SnapPixel::ToHalf);
        QPointF leftBottom = snapToNearestPixel(QPointF(3.5, 12.5), SnapPixel::ToWhole, SnapPixel::ToHalf);
        QPointF rightTop = snapToNearestPixel(QPointF(14.5, 4.5), SnapPixel::ToWhole, SnapPixel::ToHalf);

        qreal verticalSpacing = std::max(leftMiddle.y() - leftTop.y(), leftBottom.y() - leftMiddle.y());
        leftMiddle.setY(leftTop.y() + verticalSpacing);
        leftBottom.setY(leftMiddle.y() + verticalSpacing);

        qreal width = rightTop.x() - leftTop.x();

        QPainterPath topPath;
        topPath.addPolygon(QVector<QPointF>{leftTop, rightTop});
        top->setPath(topPath);
        QPainterPath middlePath;
        middlePath.addPolygon(QVector<QPointF>{leftMiddle, QPointF(leftMiddle.x() + width, leftMiddle.y())});
        middle->setPath(middlePath);
        QPainterPath bottomPath;
        bottomPath.addPolygon(QVector<QPointF>{leftBottom, QPointF(leftBottom.x() + width, leftBottom.y())});
        bottom->setPath(bottomPath);
    } else {
        QPointF leftTop = snapToNearestPixel(QPointF(3.5, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole);
        QPointF leftMiddle = snapToNearestPixel(QPointF(3.5, 8.5), SnapPixel::ToWhole, SnapPixel::ToWhole);
        QPointF leftBottom = snapToNearestPixel(QPointF(3.5, 12.5), SnapPixel::ToWhole, SnapPixel::ToWhole);
        QPointF rightTop = snapToNearestPixel(QPointF(14.5, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole);

        qreal verticalSpacing = std::max(leftMiddle.y() - leftTop.y(), leftBottom.y() - leftMiddle.y());
        leftMiddle.setY(leftTop.y() + verticalSpacing);
        leftBottom.setY(leftMiddle.y() + verticalSpacing);

        qreal width = rightTop.x() - leftTop.x();

        QPainterPath topPath;
        topPath.addPolygon(QVector<QPointF>{leftTop, rightTop});
        top->setPath(topPath);
        QPainterPath middlePath;
        middlePath.addPolygon(QVector<QPointF>{leftMiddle, QPointF(leftMiddle.x() + width, leftMiddle.y())});
        middle->setPath(middlePath);
        QPainterPath bottomPath;
        bottomPath.addPolygon(QVector<QPointF>{leftBottom, QPointF(leftBottom.x() + width, leftBottom.y())});
        bottom->setPath(bottomPath);
    }

    top->setPen(pen);
    middle->setPen(pen);
    bottom->setPen(pen);

    // centre -- centre the result, then snap centred position to a pixel boundary
    QPointF centerTranslate =
        QPointF(9, 9) - appMenuGroup->childrenBoundingRect().center(); // must use childrenBoundingRect as boundingRect does not update unless you add an item
    if (centerTranslate != QPointF(0, 0)) {
        QPointF centrePixelRealignmentOffset;

        // realign to pixel grid after centering -- use P1 of top to sample
        centrePixelRealignmentOffset =
            snapToNearestPixel(top->path().elementAt(0) + centerTranslate, SnapPixel::ToWhole, isOddPenWidth ? SnapPixel::ToHalf : SnapPixel::ToWhole)
            - top->path().elementAt(0);
        QTransform transformToAlignedCenter;
        transformToAlignedCenter.translate(centrePixelRealignmentOffset.x(), centrePixelRealignmentOffset.y());
        appMenuGroup->setTransform(transformToAlignedCenter);
    }

    if (m_strokeToFilledPath) {
        QPainterPathStroker stroker(pen);
        top->setPath(stroker.createStroke(top->path()));
        middle->setPath(stroker.createStroke(middle->path()));
        bottom->setPath(stroker.createStroke(bottom->path()));

        top->setBrush(pen.color());
        middle->setBrush(pen.color());
        bottom->setBrush(pen.color());

        top->setPen(Qt::NoPen);
        middle->setPen(Qt::NoPen);
        bottom->setPen(Qt::NoPen);
    }

    appMenu->render(m_painter, QRectF(0, 0, 18, 18), QRectF(0, 0, 18, 18));
}

void RenderDecorationButtonIcon18By18::renderContextHelpIcon()
{
    QPainterPath path;
    path.moveTo(5, 6);
    path.arcTo(QRectF(5, 3.5, 8, 5), 180, -180);
    path.cubicTo(QPointF(12.5, 9.5), QPointF(9, 7.5), QPointF(9, 11.5));

    QPainterPath dot;
    dot.addRect(QRectF(9, 15, 0.5, 0.5));

    m_painter->drawPath(path);
    m_painter->drawPath(dot);
    // NB: m_strokeToFilledPath not added as this is not currently used
}

void RenderDecorationButtonIcon18By18::renderCloseIconAtSquareMaximizeSize()
{
    // first determine the size of the maximize icon so the close icon can match in size
    auto [maximizeRect, maximizePenWidth] = renderSquareMaximizeIcon(true);
    QPen pen = m_painter->pen();

    pen.setWidthF(maximizePenWidth);

    m_painter->setPen(pen);

    QVector<QPointF> line1{maximizeRect.topLeft(), maximizeRect.bottomRight()};
    QVector<QPointF> line2{maximizeRect.topRight(), maximizeRect.bottomLeft()};

    if (m_strokeToFilledPath) {
        QPainterPath path1, path2;
        path1.addPolygon(line1);
        path2.addPolygon(line2);
        QPainterPathStroker stroker(m_painter->pen());
        path1 = stroker.createStroke(path1);
        path2 = stroker.createStroke(path2);
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(pen.color());
        m_painter->drawPath(path1);
        m_painter->drawPath(path2);
    } else {
        m_painter->drawPolyline(line1);
        m_painter->drawPolyline(line2);
    }
}

std::pair<QRectF, qreal> RenderDecorationButtonIcon18By18::renderSquareMaximizeIcon(bool returnSizeOnly, qreal cornerRelativePercent)
{
    if (returnSizeOnly)
        m_painter->save(); // needed so doesn't interfere when called from renderCloseIconAtSquareMaximizeSize etc.

    QPen pen = m_painter->pen();

    bool isOddPenWidth = true;
    if (!m_fromKstyle) {
        qreal roundedBoldPenWidth = 1;
        if (m_boldButtonIcons) {
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, m_squareMaximizeBoldPenWidthFactor);
        } else {
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1);
        }
        pen.setWidthF(roundedBoldPenWidth);
    }

    QRectF rect(QPointF(4.5, 4.5), QPointF(13.5, 13.5));
    qreal adjustmentOffset = 0;
    qreal halfDevicePixelInLocal = convertDevicePixelsToLocal(0.5);
    constexpr int maxIterations = 5;
    int i = 0;
    qreal designedWidthDevicePixels = 9 * m_totalScalingFactor;
    do {
        if (isOddPenWidth) {
            rect = QRectF(snapToNearestPixel(rect.topLeft(), SnapPixel::ToHalf, SnapPixel::ToHalf),
                          snapToNearestPixel(rect.bottomRight(), SnapPixel::ToHalf, SnapPixel::ToHalf));
        } else {
            rect = QRectF(snapToNearestPixel(rect.topLeft(), SnapPixel::ToWhole, SnapPixel::ToWhole),
                          snapToNearestPixel(rect.bottomRight(), SnapPixel::ToWhole, SnapPixel::ToWhole));
        }

        qreal maxSide =
            qMax(rect.width(), rect.height()); // ensure width and height are the same -- they are sometimes not on display scales which are not a factor of 0.5
        rect.setTopLeft(QPointF(0, 0));
        rect.setBottomRight(QPointF(maxSide, maxSide));

        qreal rectWidthDevicePixels = rect.width() * m_totalScalingFactor;
        bool increaseSize = false;
        if (m_forceEvenSquares) {
            // if m_forceEvenSquares and total device square width is not even
            if (qRound(rectWidthDevicePixels + penWidthToDevice(pen)) % 2 != 0) {
                increaseSize = true;
            }
        } else {
            // if size is still smaller than linear to original design, increase again
            if (rectWidthDevicePixels < (designedWidthDevicePixels - 0.001)) { // 0.001 as sometimes there are floating point errors
                increaseSize = true;
            }
        }

        if (increaseSize) {
            adjustmentOffset = halfDevicePixelInLocal;
            rect.adjust(-adjustmentOffset, -adjustmentOffset, adjustmentOffset, adjustmentOffset);
        } else {
            adjustmentOffset = 0;
        }
        i++;
    } while (adjustmentOffset && i < maxIterations);

    // centre
    QPointF centerTranslate = QPointF(9, 9) - rect.center();
    if (centerTranslate != QPointF(0, 0)) {
        QPointF centrePixelRealignmentOffset;

        // realign to pixel grid after centring -- use top-left of rect to sample
        if (isOddPenWidth) {
            centrePixelRealignmentOffset = snapToNearestPixel(rect.topLeft() + centerTranslate, SnapPixel::ToHalf, SnapPixel::ToHalf) - rect.topLeft();
        } else {
            centrePixelRealignmentOffset = snapToNearestPixel(rect.topLeft() + centerTranslate, SnapPixel::ToWhole, SnapPixel::ToWhole) - rect.topLeft();
        }

        rect.moveTopLeft(rect.topLeft() + centrePixelRealignmentOffset);
    }

    if (!returnSizeOnly) {
        // make excessively thick pen widths translucent to balance with other buttons

        qreal opacity = straightLineOpacity();
        QColor penColor = pen.color();
        penColor.setAlphaF(penColor.alphaF() * opacity);
        pen.setColor(penColor);

        // convert strokes to paths needed as otherwise GTK apps fill the square in generated icons
        if (m_strokeToFilledPath) {
            QPainterPath path;
            path.addRoundedRect(rect, cornerRelativePercent, cornerRelativePercent, Qt::RelativeSize);
            QPainterPathStroker stroker(pen);
            path = stroker.createStroke(path);
            m_painter->setBrush(pen.color());
            m_painter->setPen(Qt::NoPen);
            m_painter->drawPath(path);
        } else {
            m_painter->setPen(pen);
            m_painter->drawRoundedRect(rect, cornerRelativePercent, cornerRelativePercent, Qt::RelativeSize);
        }
    } else
        m_painter->restore();

    return {rect, pen.widthF()};
}

void RenderDecorationButtonIcon18By18::renderOverlappingWindowsIcon(qreal cornerRelativePercent)
{
    // first determine the size of the maximize icon so the restore icon can align with it
    auto [maximizeRect, maximizePenWidth] = renderSquareMaximizeIcon(true);
    Q_UNUSED(maximizePenWidth);

    QPen pen = m_painter->pen();

    pen.setJoinStyle(Qt::BevelJoin);
    pen.setCapStyle(Qt::FlatCap);

    qreal roundedBoldPenWidth = 1;
    bool isOddPenWidth = true;
    if (m_boldButtonIcons) {
        isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, m_overlappingWindowsBoldPenWidthFactor);
    } else {
        isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1);
    }

    // thicker pen in titlebar
    pen.setWidthF(roundedBoldPenWidth);
    qreal penWidth18By18 = penWidthToLocal(pen);
    qreal halfPenWidth18By18 = penWidth18By18 / 2;

    qreal singleDevicePixelin18By18 = convertDevicePixelsToLocal(1);
    // this is to calculate the offset to move the two rectangles further from each other onto an aligned pixel
    // they can be moved apart as the line thickness increases -- this prevents blurriness when the lines are drawn too close together
    qreal shiftOffsetX = singleDevicePixelin18By18;
    qreal shiftOffsetY = singleDevicePixelin18By18;
    bool shiftX = true;
    bool shiftY = true;

    QGraphicsPathItem *backgroundPathItem = nullptr;

    std::unique_ptr<QGraphicsScene> overlappingWindows = std::unique_ptr<QGraphicsScene>(new QGraphicsScene(0, 0, 18, 18));

    QGraphicsItemGroup *overlappingWindowsGroup = new QGraphicsItemGroup;

    overlappingWindows->addItem(overlappingWindowsGroup);

    qreal designedWidthDevicePixels = convertLocalPixelsToDevice(7);
    qreal adjustmentOffset = 0;
    qreal halfDevicePixelInLocal = convertDevicePixelsToLocal(0.5);
    int maxIterations = 5;
    int i = 0;
    // foreground square, iterate until big enough
    QRectF foregroundSquare(QPointF(4.5, 6.5), QPointF(11.5, 13.5));
    do {
        if (isOddPenWidth) {
            foregroundSquare = QRectF(snapToNearestPixel(foregroundSquare.topLeft(), SnapPixel::ToHalf, SnapPixel::ToHalf),
                                      snapToNearestPixel(foregroundSquare.bottomRight(), SnapPixel::ToHalf, SnapPixel::ToHalf));
        } else {
            foregroundSquare = QRectF(snapToNearestPixel(foregroundSquare.topLeft(), SnapPixel::ToWhole, SnapPixel::ToWhole),
                                      snapToNearestPixel(foregroundSquare.bottomRight(), SnapPixel::ToWhole, SnapPixel::ToWhole));
        }

        qreal maxSide =
            qMax(foregroundSquare.width(),
                 foregroundSquare.height()); // ensure width and height are the same -- they are sometimes not on display scales which are not a factor of 0.5
        foregroundSquare.setTopLeft(QPointF(0, 0));
        foregroundSquare.setBottomRight(QPointF(maxSide, maxSide));

        qreal rectWidthDevicePixels = foregroundSquare.width() * m_totalScalingFactor;
        bool increaseSize = false;
        // if size is still smaller than linear to original design, increase again
        if (rectWidthDevicePixels < (designedWidthDevicePixels - 0.001)) { // 0.001 as sometimes there are floating point errors
            increaseSize = true;
        }

        if (increaseSize) {
            adjustmentOffset = halfDevicePixelInLocal;
            foregroundSquare.adjust(-adjustmentOffset, -adjustmentOffset, adjustmentOffset, adjustmentOffset);
        } else {
            adjustmentOffset = 0;
        }
        i++;
    } while (adjustmentOffset && i < maxIterations);

    QPainterPath foregroundPath;
    foregroundPath.addRect(foregroundSquare);
    QGraphicsPathItem *foregroundPathItem = new QGraphicsPathItem(foregroundPath);
    overlappingWindowsGroup->addToGroup(foregroundPathItem);
    // set no pen to make all dimension calculations simpler
    foregroundPathItem->setPen(Qt::PenStyle::NoPen);
    maxIterations = 6 * qRound(m_devicePixelRatio); // would rather have based this on m_totalScalingFactor, but for some strange reason this causes kwin to
                                                    // crash in some circunstances

    // calculate the geometry of the background square, iterate until an appropriate separation from foreground square achieved
    for (int i = 0; (shiftX || shiftY) && (i < maxIterations); i++) {
        calculateBackgroundSquareGeometry(shiftOffsetX, shiftOffsetY, overlappingWindowsGroup, foregroundPathItem, backgroundPathItem, halfPenWidth18By18);

        if (!(overlappingWindows && overlappingWindowsGroup && foregroundPathItem && backgroundPathItem))
            return;

        qreal distanceBetweenSquaresX = backgroundPathItem->path().elementAt(3).x - backgroundPathItem->path().elementAt(4).x;
        qreal distanceBetweenSquaresY = backgroundPathItem->path().elementAt(0).y - backgroundPathItem->path().elementAt(1).y;

        // if distance between squares < pen width (factoring in that the background square does not join the foreground at the foreground's centre-point)
        if (((distanceBetweenSquaresX / penWidth18By18) < 1.5 - 0.01) || (m_boldButtonIcons && distanceBetweenSquaresX < 2 - 0.01)
            || (!m_boldButtonIcons
                && distanceBetweenSquaresX < 1.5 - 0.01) // 0.01 is because sometimes there are floating point errors, and also due to PenWidth::Symbol
        ) {
            shiftX = true;
            shiftOffsetX += singleDevicePixelin18By18;
        } else {
            shiftX = false;
        }

        // if distance between squares < pen width (factoring in that the background square does not join the foreground at the foreground's centre-point)
        if (((distanceBetweenSquaresY / penWidth18By18) < 1.5 - 0.01) || (m_boldButtonIcons && distanceBetweenSquaresY < 2 - 0.01)
            || (!m_boldButtonIcons
                && distanceBetweenSquaresY < 1.5 - 0.01) // 0.01 is because sometimes there are floating point errors, and also due to PenWidth::Symbol
        ) {
            shiftY = true;
            shiftOffsetY += singleDevicePixelin18By18;
        } else {
            shiftY = false;
        }
    }

    // centre -- centre the result, then snap centred position to a pixel boundary
    QPointF centerTranslate = QPointF(9, maximizeRect.center().y()) - overlappingWindowsGroup->boundingRect().center();
    if (centerTranslate != QPointF(0, 0)) {
        QPointF centrePixelRealignmentOffset;

        // realign to pixel grid after centering -- use top-left of foregroundRect to sample
        centrePixelRealignmentOffset = snapToNearestPixel(foregroundPathItem->boundingRect().topLeft() + centerTranslate,
                                                          isOddPenWidth ? SnapPixel::ToHalf : SnapPixel::ToWhole,
                                                          isOddPenWidth ? SnapPixel::ToHalf : SnapPixel::ToWhole)
            - foregroundPathItem->boundingRect().topLeft();
        QTransform transformToAlignedCenter;
        transformToAlignedCenter.translate(centrePixelRealignmentOffset.x(), centrePixelRealignmentOffset.y());
        overlappingWindowsGroup->setTransform(transformToAlignedCenter);
    }

    // make excessively thick pen widths translucent to balance with other buttons
    qreal opacity = straightLineOpacity();
    QColor penColor = pen.color();
    penColor.setAlphaF(penColor.alphaF() * opacity);
    pen.setColor(penColor);

    if (cornerRelativePercent > -0.001) {
        pen.setCapStyle(Qt::PenCapStyle::RoundCap);
        QPainterPath roundedForegroundPath;
        roundedForegroundPath.addRoundedRect(foregroundPathItem->path().boundingRect(),
                                             cornerRelativePercent,
                                             cornerRelativePercent,
                                             Qt::SizeMode::RelativeSize);
        foregroundPathItem->setPath(roundedForegroundPath);

        QPainterPath roundedBackgroundPath;
        qreal cornerRadiusForeground = foregroundPathItem->path().boundingRect().width() * cornerRelativePercent / 100.0f;
        qreal cornerRadiusBackground = cornerRadiusForeground + penWidthToLocal(pen);
        qreal doubleCornerRadiusBackground = cornerRadiusBackground * 2;

        roundedBackgroundPath.moveTo(backgroundPathItem->path().boundingRect().topLeft());
        roundedBackgroundPath.lineTo(backgroundPathItem->path().boundingRect().topRight() - QPointF(cornerRadiusBackground, 0));
        roundedBackgroundPath.arcMoveTo(backgroundPathItem->path().boundingRect().topRight().x() - doubleCornerRadiusBackground,
                                        backgroundPathItem->path().boundingRect().topRight().y(),
                                        doubleCornerRadiusBackground,
                                        doubleCornerRadiusBackground,
                                        90);
        roundedBackgroundPath.arcTo(backgroundPathItem->path().boundingRect().topRight().x() - doubleCornerRadiusBackground,
                                    backgroundPathItem->path().boundingRect().topRight().y(),
                                    doubleCornerRadiusBackground,
                                    doubleCornerRadiusBackground,
                                    90,
                                    -90);
        roundedBackgroundPath.lineTo(backgroundPathItem->path().boundingRect().bottomRight());

        backgroundPathItem->setPath(roundedBackgroundPath);
    }

    if (m_strokeToFilledPath) {
        QPainterPathStroker stroker(pen);
        foregroundPathItem->setPath(stroker.createStroke(foregroundPathItem->path()));
        backgroundPathItem->setPath(stroker.createStroke(backgroundPathItem->path()));

        foregroundPathItem->setBrush(pen.color());
        backgroundPathItem->setBrush(pen.color());
    } else {
        // set the pen widths in all items -- do this at this point as not to have non-cosmetic pen widths in boundingRect().width() calculations above
        foregroundPathItem->setPen(pen);
        backgroundPathItem->setPen(pen);
    }
    // paint
    overlappingWindows->render(m_painter, QRectF(0, 0, 18, 18), QRectF(0, 0, 18, 18));
}

void RenderDecorationButtonIcon18By18::calculateBackgroundSquareGeometry(const qreal shiftOffsetX,
                                                                         const qreal shiftOffsetY,
                                                                         QGraphicsItemGroup *overlappingWindowsGroup,
                                                                         QGraphicsPathItem *foregroundSquareItem,
                                                                         QGraphicsPathItem *&backgroundSquareItem,
                                                                         qreal halfPenWidthLocal)
{
    QRectF foregroundSquareBoundingRect = foregroundSquareItem->boundingRect();
    qreal diameter = foregroundSquareBoundingRect.width();

    // background square
    QVector<QPointF> background;
    background.resize(5);

    background[1] = foregroundSquareBoundingRect.topLeft();
    background[1].setX(background[1].x() + shiftOffsetX);
    background[0].setX(background[1].x());
    background[0].setY(foregroundSquareBoundingRect.top() - halfPenWidthLocal);
    background[1].setY(foregroundSquareBoundingRect.top() - shiftOffsetY);

    background[2].setX(background[1].x() + diameter);
    background[2].setY(background[1].y());

    background[3].setX(background[2].x());
    background[3].setY(background[2].y() + diameter);

    background[4].setX(foregroundSquareBoundingRect.right() + halfPenWidthLocal);
    background[4].setY(background[3].y());

    QPainterPath backgroundSquarePath;
    backgroundSquarePath.addPolygon(background);
    backgroundSquareItem = new QGraphicsPathItem(backgroundSquarePath);

    overlappingWindowsGroup->addToGroup(backgroundSquareItem);

    // set no pen to make all dimension calculations simpler
    backgroundSquareItem->setPen(Qt::PenStyle::NoPen);
}

void RenderDecorationButtonIcon18By18::renderTinySquareMinimizeIcon()
{
    // first determine the size of the maximize icon so the minimize icon can align with it
    auto [maximizeRect, maximizePenWidth] = renderSquareMaximizeIcon(true);

    bool isOddPenWidth = true;

    QPen pen = m_painter->pen();
    if (m_boldButtonIcons) {
        QColor penColor = pen.color();
        QColor brushColor = penColor;
        brushColor.setAlphaF(brushColor.alphaF() * 0.45);
        penColor.setAlphaF(penColor.alphaF() * straightLineOpacity() * 0.95);
        pen.setColor(penColor);

        pen.setJoinStyle(Qt::BevelJoin);
        m_painter->setBrush(brushColor);

        isOddPenWidth = qRound(maximizePenWidth) % 2 != 0;
        pen.setWidthF(maximizePenWidth);

        m_painter->setPen(pen);
    } else { // in fine mode the dense minimize button appears bolder than the others so reduce its opacity to compensate
        QColor penColor = pen.color();
        QColor brushColor = penColor;
        brushColor.setAlphaF(brushColor.alphaF() * 0.35);
        penColor.setAlphaF(penColor.alphaF() * straightLineOpacity() * 0.9);
        pen.setColor(penColor);

        pen.setJoinStyle(Qt::BevelJoin);
        m_painter->setBrush(brushColor);

        isOddPenWidth = qRound(maximizePenWidth) % 2 != 0;
        pen.setWidthF(maximizePenWidth);
        m_painter->setPen(pen);
    }

    // tiny filled square
    QRectF rect(QPointF(7.5, 7.5), QPointF(10.5, 10.5));
    qreal adjustmentOffset = 0;
    constexpr int maxIterations = 5;
    int i = 0;
    qreal designedWidthDevicePixels = 3 * m_totalScalingFactor;
    qreal halfDevicePixelInLocal = convertDevicePixelsToLocal(0.5);
    do {
        if (isOddPenWidth) {
            rect = QRectF(snapToNearestPixel(rect.topLeft(), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Up, ThresholdRound::Down),
                          snapToNearestPixel(rect.bottomRight(), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Up, ThresholdRound::Down));
        } else {
            rect = QRectF(snapToNearestPixel(rect.topLeft(), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Up, ThresholdRound::Down),
                          snapToNearestPixel(rect.bottomRight(), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Up, ThresholdRound::Down));
        }
        qreal maxSide =
            qMax(rect.width(), rect.height()); // ensure width and height are the same -- they are sometimes not on display scales which are not a factor of 0.5
        rect.setTopLeft(QPointF(0, 0));
        rect.setBottomRight(QPointF(maxSide, maxSide));

        qreal rectWidthDevicePixels = rect.width() * m_totalScalingFactor;
        bool increaseSize = false;
        if (m_forceEvenSquares) {
            // if m_forceEvenSquares and total device square width is not even
            if (qRound(rectWidthDevicePixels + penWidthToDevice(pen)) % 2 != 0) {
                increaseSize = true;
            }
        } else {
            // if size is still smaller than linear to original design, increase again
            if (rectWidthDevicePixels < (designedWidthDevicePixels - 0.001)) { // 0.001 as sometimes there are floating point errors
                increaseSize = true;
            }
        }

        if (increaseSize) {
            adjustmentOffset = halfDevicePixelInLocal;
            rect.adjust(-adjustmentOffset, -adjustmentOffset, adjustmentOffset, adjustmentOffset);
        } else {
            adjustmentOffset = 0;
        }
        i++;
    } while (adjustmentOffset && i < maxIterations);

    // centre
    QPointF centerTranslate = QPointF(9, maximizeRect.center().y()) - rect.center();
    if (centerTranslate != QPointF(0, 0)) {
        QPointF centrePixelRealignmentOffset;

        // realign to pixel grid after centring -- use top-left of rect to sample
        if (isOddPenWidth) {
            centrePixelRealignmentOffset = snapToNearestPixel(rect.topLeft() + centerTranslate, SnapPixel::ToHalf, SnapPixel::ToHalf) - rect.topLeft();
        } else {
            centrePixelRealignmentOffset = snapToNearestPixel(rect.topLeft() + centerTranslate, SnapPixel::ToWhole, SnapPixel::ToWhole) - rect.topLeft();
        }
        m_painter->translate(centrePixelRealignmentOffset);
    }

    if (m_strokeToFilledPath) {
        m_painter->setPen(Qt::NoPen);
        m_painter->drawRect(rect);
        QPainterPath stroke;
        stroke.addRect(rect);
        QPainterPathStroker stroker(pen);
        stroke = stroker.createStroke(stroke);
        m_painter->setBrush(pen.color());
        m_painter->drawPath(stroke);
    } else {
        m_painter->drawRect(rect);
    }
}

// For consistency with breeze icon set
void RenderDecorationButtonIcon18By18::renderKeepBehindIconAsFromBreezeIcons()
{
    QPen pen = m_painter->pen();

    bool flatCapArrowHead = false;
    if (m_totalScalingFactor < 1.3) {
        pen.setCapStyle(Qt::FlatCap); // prevents blurry lobsided arrow at loDPI
        flatCapArrowHead = true;
    }

    bool isOddPenWidth = true;
    if (!m_fromKstyle) {
        qreal roundedBoldPenWidth = 1;
        if (m_boldButtonIcons) {
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1.2);
        } else {
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1);
        }

        // thicker pen in titlebar
        pen.setWidthF(roundedBoldPenWidth);
    }
    m_painter->setPen(pen);

    QVector<QPointF> top, middle, bottom, arrowBody, arrowHead;
    if (isOddPenWidth) {
        // horizontal lines
        QPointF leftBottom = snapToNearestPixel(QPointF(4.5, 14.5), SnapPixel::ToHalf, SnapPixel::ToHalf);
        QPointF rightBottom = snapToNearestPixel(QPointF(13.5, 14.5), SnapPixel::ToHalf, SnapPixel::ToHalf);
        QPointF leftMiddle = snapToNearestPixel(QPointF(9.5, 10.5), SnapPixel::ToHalf, SnapPixel::ToHalf);
        QPointF rightMiddle = snapToNearestPixel(QPointF(13.5, 10.5), SnapPixel::ToHalf, SnapPixel::ToHalf);
        QPointF leftTop = snapToNearestPixel(QPointF(9.5, 6.5), SnapPixel::ToHalf, SnapPixel::ToHalf);
        QPointF rightTop = snapToNearestPixel(QPointF(13.5, 6.5), SnapPixel::ToHalf, SnapPixel::ToHalf);

        // define relative spaces between lines evenly; to correct for any distortion
        qreal verticalSpacing = std::max(leftMiddle.y() - leftTop.y(), leftBottom.y() - leftMiddle.y());
        rightBottom.setY(leftBottom.y());
        rightMiddle.setX(rightTop.x());
        leftMiddle.setY(leftBottom.y() - verticalSpacing);
        rightMiddle.setY(leftMiddle.y());
        rightTop.setX(rightBottom.x());
        leftTop.setX(leftMiddle.x());
        rightTop.setY(rightMiddle.y() - verticalSpacing);
        leftTop.setY(rightTop.y());

        top = {leftBottom, rightBottom};
        middle = {leftMiddle, rightMiddle};
        bottom = {leftTop, rightTop};

        // arrow
        arrowBody = {snapToNearestPixel(QPointF(4.5, 4.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down),
                     snapToNearestPixel(QPointF(4.5, 12.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down)};

        arrowHead = {snapToNearestPixel(QPointF(2.5, 10.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down),
                     snapToNearestPixel(QPointF(4.5, 12.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down),
                     snapToNearestPixel(QPointF(6.5, 10.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down)};

        if (flatCapArrowHead)
            arrowHead[0] = snapToNearestPixel(QPointF(2, 10), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down);
        qreal arrowSlopeWidth = arrowHead[1].x() - arrowHead[0].x();
        arrowHead[2] = QPointF(arrowHead[1].x() + arrowSlopeWidth,
                               arrowHead[0].y()); // set the other side of arrowhead in relative co-ordinates in case pixel-snapping is uneven either side

    } else {
        // horizontal lines
        QPointF leftBottom = snapToNearestPixel(QPointF(4.5, 14.5), SnapPixel::ToWhole, SnapPixel::ToWhole);
        QPointF rightBottom = snapToNearestPixel(QPointF(13.5, 14.5), SnapPixel::ToWhole, SnapPixel::ToWhole);
        QPointF leftMiddle = snapToNearestPixel(QPointF(9.5, 10.5), SnapPixel::ToWhole, SnapPixel::ToWhole);
        QPointF rightMiddle = snapToNearestPixel(QPointF(13.5, 10.5), SnapPixel::ToWhole, SnapPixel::ToWhole);
        QPointF leftTop = snapToNearestPixel(QPointF(9.5, 6.5), SnapPixel::ToWhole, SnapPixel::ToWhole);
        QPointF rightTop = snapToNearestPixel(QPointF(13.5, 6.5), SnapPixel::ToWhole, SnapPixel::ToWhole);

        // define relative spaces between lines evenly; to correct for any distortion
        qreal verticalSpacing = std::max(leftMiddle.y() - leftTop.y(), leftBottom.y() - leftMiddle.y());
        rightBottom.setY(leftBottom.y());
        rightMiddle.setX(rightTop.x());
        leftMiddle.setY(leftBottom.y() - verticalSpacing);
        rightMiddle.setY(leftMiddle.y());
        rightTop.setX(rightBottom.x());
        leftTop.setX(leftMiddle.x());
        rightTop.setY(rightMiddle.y() - verticalSpacing);
        leftTop.setY(rightTop.y());

        top = {leftBottom, rightBottom};
        middle = {leftMiddle, rightMiddle};
        bottom = {leftTop, rightTop};

        // arrow
        arrowBody = {snapToNearestPixel(QPointF(4.5, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down),
                     snapToNearestPixel(QPointF(4.5, 12.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down)};

        arrowHead = {snapToNearestPixel(QPointF(2.5, 10.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down),
                     snapToNearestPixel(QPointF(4.5, 12.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down),
                     snapToNearestPixel(QPointF(6.5, 10.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down)};

        if (flatCapArrowHead)
            arrowHead[0] = snapToNearestPixel(QPointF(2, 10), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down);
        qreal arrowSlopeWidth = arrowHead[1].x() - arrowHead[0].x();
        arrowHead[2] = QPointF(arrowHead[1].x() + arrowSlopeWidth,
                               arrowHead[0].y()); // set the other side of arrowhead in relative co-ordinates in case pixel-snapping is uneven either side
    }

    if (m_strokeToFilledPath) {
        QPainterPath topPath, middlePath, bottomPath, arrowBodyPath, arrowHeadPath;
        QPainterPathStroker stroker(pen);
        topPath.addPolygon(top);
        middlePath.addPolygon(middle);
        bottomPath.addPolygon(bottom);
        arrowBodyPath.addPolygon(arrowBody);
        arrowHeadPath.addPolygon(arrowHead);

        topPath = stroker.createStroke(topPath);
        middlePath = stroker.createStroke(middlePath);
        bottomPath = stroker.createStroke(bottomPath);
        arrowBodyPath = stroker.createStroke(arrowBodyPath);
        arrowHeadPath = stroker.createStroke(arrowHeadPath);

        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(pen.color());

        m_painter->drawPath(topPath);
        m_painter->drawPath(middlePath);
        m_painter->drawPath(bottomPath);
        m_painter->drawPath(arrowBodyPath);
        m_painter->drawPath(arrowHeadPath);
    } else {
        m_painter->drawPolyline(top);
        m_painter->drawPolyline(middle);
        m_painter->drawPolyline(bottom);
        m_painter->drawPolyline(arrowBody);
        m_painter->drawPolyline(arrowHead);
    }
}

void RenderDecorationButtonIcon18By18::renderKeepInFrontIconAsFromBreezeIcons()
{
    QPen pen = m_painter->pen();

    bool flatCapArrowHead = false;
    if (m_totalScalingFactor < 1.3) {
        pen.setCapStyle(Qt::FlatCap); // prevents blurry lobsided arrow at loDPI
        flatCapArrowHead = true;
    }

    bool isOddPenWidth = true;
    if (!m_fromKstyle) {
        qreal roundedBoldPenWidth = 1;
        if (m_boldButtonIcons) {
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1.2);
        } else {
            isOddPenWidth = roundedPenWidthIsOdd(pen, roundedBoldPenWidth, 1);
        }

        // thicker pen in titlebar
        pen.setWidthF(roundedBoldPenWidth);
    }
    m_painter->setPen(pen);

    QVector<QPointF> top, middle, bottom, arrowBody, arrowHead;
    if (isOddPenWidth) {
        // horizontal lines
        QPointF leftTop = snapToNearestPixel(QPointF(4.5, 4.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down);
        QPointF rightTop = snapToNearestPixel(QPointF(13.5, 4.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down);
        QPointF leftMiddle = snapToNearestPixel(QPointF(4.5, 8.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down);
        QPointF rightMiddle = snapToNearestPixel(QPointF(8.5, 8.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down);
        QPointF leftBottom = snapToNearestPixel(QPointF(4.5, 12.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down);
        QPointF rightBottom = snapToNearestPixel(QPointF(8.5, 12.5), SnapPixel::ToHalf, SnapPixel::ToHalf, ThresholdRound::Down, ThresholdRound::Down);

        // define relative spaces between lines evenly; to correct for any distortion
        qreal verticalSpacing = std::max(leftMiddle.y() - leftTop.y(), leftBottom.y() - leftMiddle.y());
        rightTop.setY(leftTop.y());
        leftMiddle.setX(leftTop.x());
        leftMiddle.setY(leftTop.y() + verticalSpacing);
        rightMiddle.setY(leftMiddle.y());
        leftBottom.setX(leftTop.x());
        rightBottom.setX(rightMiddle.x());
        leftBottom.setY(leftMiddle.y() + verticalSpacing);
        rightBottom.setY(leftBottom.y());

        top = {leftBottom, rightBottom};
        middle = {leftMiddle, rightMiddle};
        bottom = {leftTop, rightTop};

        // arrow
        arrowBody = {snapToNearestPixel(QPointF(13.5, 6.5), SnapPixel::ToHalf, SnapPixel::ToHalf),
                     snapToNearestPixel(QPointF(13.5, 14.5), SnapPixel::ToHalf, SnapPixel::ToHalf)};

        arrowHead = {snapToNearestPixel(QPointF(11.5, 8.5), SnapPixel::ToHalf, SnapPixel::ToHalf),
                     snapToNearestPixel(QPointF(13.5, 6.5), SnapPixel::ToHalf, SnapPixel::ToHalf),
                     snapToNearestPixel(QPointF(15.5, 8.5), SnapPixel::ToHalf, SnapPixel::ToHalf)};
        if (flatCapArrowHead)
            arrowHead[2] = snapToNearestPixel(QPointF(16, 9), SnapPixel::ToWhole, SnapPixel::ToWhole);
        qreal arrowSlopeWidth = arrowHead[2].x() - arrowHead[1].x();
        arrowHead[0] = QPointF(arrowHead[1].x() - arrowSlopeWidth,
                               arrowHead[2].y()); // set the other side of arrowhead in relative co-ordinates in case pixel-snapping is uneven either side
    } else {
        // horizontal lines
        QPointF leftTop = snapToNearestPixel(QPointF(4.5, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down);
        QPointF rightTop = snapToNearestPixel(QPointF(13.5, 4.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down);
        QPointF leftMiddle = snapToNearestPixel(QPointF(4.5, 8.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down);
        QPointF rightMiddle = snapToNearestPixel(QPointF(8.5, 8.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down);
        QPointF leftBottom = snapToNearestPixel(QPointF(4.5, 12.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down);
        QPointF rightBottom = snapToNearestPixel(QPointF(8.5, 12.5), SnapPixel::ToWhole, SnapPixel::ToWhole, ThresholdRound::Down, ThresholdRound::Down);

        // define relative spaces between lines evenly; to correct for any distortion
        qreal verticalSpacing = std::max(leftMiddle.y() - leftTop.y(), leftBottom.y() - leftMiddle.y());
        rightTop.setY(leftTop.y());
        leftMiddle.setX(leftTop.x());
        leftMiddle.setY(leftTop.y() + verticalSpacing);
        rightMiddle.setY(leftMiddle.y());
        leftBottom.setX(leftTop.x());
        rightBottom.setX(rightMiddle.x());
        leftBottom.setY(leftMiddle.y() + verticalSpacing);
        rightBottom.setY(leftBottom.y());

        top = {leftBottom, rightBottom};
        middle = {leftMiddle, rightMiddle};
        bottom = {leftTop, rightTop};

        // arrow
        arrowBody = {snapToNearestPixel(QPointF(13.5, 6.5), SnapPixel::ToWhole, SnapPixel::ToWhole),
                     snapToNearestPixel(QPointF(13.5, 14.5), SnapPixel::ToWhole, SnapPixel::ToWhole)};

        arrowHead = {snapToNearestPixel(QPointF(11.5, 8.5), SnapPixel::ToWhole, SnapPixel::ToWhole),
                     snapToNearestPixel(QPointF(13.5, 6.5), SnapPixel::ToWhole, SnapPixel::ToWhole),
                     snapToNearestPixel(QPointF(15.5, 8.5), SnapPixel::ToWhole, SnapPixel::ToWhole)};
        if (flatCapArrowHead)
            arrowHead[2] = snapToNearestPixel(QPointF(16, 9), SnapPixel::ToHalf, SnapPixel::ToHalf);
        qreal arrowSlopeWidth = arrowHead[2].x() - arrowHead[1].x();
        arrowHead[0] = QPointF(arrowHead[1].x() - arrowSlopeWidth,
                               arrowHead[2].y()); // set the other side of arrowhead in relative co-ordinates in case pixel-snapping is uneven either side
    }

    if (m_strokeToFilledPath) {
        QPainterPath topPath, middlePath, bottomPath, arrowBodyPath, arrowHeadPath;
        QPainterPathStroker stroker(pen);
        topPath.addPolygon(top);
        middlePath.addPolygon(middle);
        bottomPath.addPolygon(bottom);
        arrowBodyPath.addPolygon(arrowBody);
        arrowHeadPath.addPolygon(arrowHead);

        topPath = stroker.createStroke(topPath);
        middlePath = stroker.createStroke(middlePath);
        bottomPath = stroker.createStroke(bottomPath);
        arrowBodyPath = stroker.createStroke(arrowBodyPath);
        arrowHeadPath = stroker.createStroke(arrowHeadPath);

        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(pen.color());

        m_painter->drawPath(topPath);
        m_painter->drawPath(middlePath);
        m_painter->drawPath(bottomPath);
        m_painter->drawPath(arrowBodyPath);
        m_painter->drawPath(arrowHeadPath);
    } else {
        m_painter->drawPolyline(top);
        m_painter->drawPolyline(middle);
        m_painter->drawPolyline(bottom);
        m_painter->drawPolyline(arrowBody);
        m_painter->drawPolyline(arrowHead);
    }
}

void RenderDecorationButtonIcon18By18::renderRounderAndBolderContextHelpIcon()
{
    QPen pen = m_painter->pen();

    if ((!m_fromKstyle) && m_boldButtonIcons) {
        // thicker pen in titlebar
        pen.setWidthF(pen.widthF() * 1.6);
    }

    pen.setJoinStyle(Qt::RoundJoin);
    m_painter->setPen(pen);

    // main body of question mark
    QPainterPath path;
    path.arcMoveTo(QRectF(6.5, 3.5, 5.5, 5), 150);
    path.arcTo(QRectF(6.5, 3.5, 5.5, 5), 150, -150);
    path.cubicTo(QPointF(12, 9.5), QPointF(9, 7.5), QPointF(9, 11.5));
    if (m_strokeToFilledPath) {
        QPainterPathStroker stroker(m_painter->pen());
        path = stroker.createStroke(path);
        ;
        m_painter->setPen(Qt::NoPen);
        m_painter->setBrush(pen.color());
    }
    m_painter->drawPath(path);

    // dot of question mark
    m_painter->setPen(Qt::NoPen);
    m_painter->setBrush(pen.color());
    if ((!m_fromKstyle) && m_boldButtonIcons)
        m_painter->drawEllipse(QRectF(8, 14, 2, 2));
    else
        m_painter->drawEllipse(QRectF(8.25, 14.25, 1.5, 1.5));
}

}
