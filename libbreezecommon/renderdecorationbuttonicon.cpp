/*
 * SPDX-FileCopyrightText: 2022-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "renderdecorationbuttonicon.h"
#include "stylekite.h"
#include "styleklassy.h"
#include "styleoxygen.h"
#include "styleredmond10.h"
#include "styleredmond11.h"
#include "styletraditional.h"
#include "systemicontheme.h"
#include <algorithm>
#include <cmath>

namespace Breeze
{
using KDecoration2::DecorationButtonType;

std::pair<std::unique_ptr<RenderDecorationButtonIcon>, int> RenderDecorationButtonIcon::factory(const QSharedPointer<Breeze::InternalSettings> internalSettings,
                                                                                                QPainter *painter,
                                                                                                const bool fromKstyle,
                                                                                                const bool boldButtonIcons,
                                                                                                const qreal devicePixelRatio,
                                                                                                const QPointF &deviceOffsetFromZeroReference,
                                                                                                const bool forceEvenSquares)
{
    switch (internalSettings->buttonIconStyle()) {
    case InternalSettings::EnumButtonIconStyle::StyleKlassy:
    default:
        return {
            std::make_unique<RenderStyleKlassy18By18>(painter, fromKstyle, boldButtonIcons, devicePixelRatio, deviceOffsetFromZeroReference, forceEvenSquares),
            18};

    case InternalSettings::EnumButtonIconStyle::StyleKite:
        return {
            std::make_unique<RenderStyleKite18By18>(painter, fromKstyle, boldButtonIcons, devicePixelRatio, deviceOffsetFromZeroReference, forceEvenSquares),
            18};
    case InternalSettings::EnumButtonIconStyle::StyleOxygen:
        return {
            std::make_unique<RenderStyleOxygen18By18>(painter, fromKstyle, boldButtonIcons, devicePixelRatio, deviceOffsetFromZeroReference, forceEvenSquares),
            18};
    case InternalSettings::EnumButtonIconStyle::StyleTraditional:
        return {std::make_unique<RenderStyleTraditional18By18>(painter,
                                                               fromKstyle,
                                                               boldButtonIcons,
                                                               devicePixelRatio,
                                                               deviceOffsetFromZeroReference,
                                                               forceEvenSquares),
                18};
    case InternalSettings::EnumButtonIconStyle::StyleRedmond10:
        return {std::make_unique<RenderStyleRedmond1018By18>(painter,
                                                             fromKstyle,
                                                             boldButtonIcons,
                                                             devicePixelRatio,
                                                             deviceOffsetFromZeroReference,
                                                             forceEvenSquares),
                18};
    case InternalSettings::EnumButtonIconStyle::StyleRedmond11:
        return {std::make_unique<RenderStyleRedmond1118By18>(painter,
                                                             fromKstyle,
                                                             boldButtonIcons,
                                                             devicePixelRatio,
                                                             deviceOffsetFromZeroReference,
                                                             forceEvenSquares),
                18};
    }
}

RenderDecorationButtonIcon::RenderDecorationButtonIcon(QPainter *painter,
                                                       const bool fromKstyle,
                                                       const bool boldButtonIcons,
                                                       const qreal devicePixelRatio,
                                                       const QPointF &deviceOffsetFromZeroReference,
                                                       const bool forceEvenSquares)
    : m_painter(painter)
    , m_fromKstyle(fromKstyle)
    , m_boldButtonIcons(boldButtonIcons)
    , m_devicePixelRatio(devicePixelRatio)
    , m_deviceOffsetFromZeroReference(deviceOffsetFromZeroReference)
    , m_forceEvenSquares(forceEvenSquares)
{
}

RenderDecorationButtonIcon::~RenderDecorationButtonIcon()
{
}

void RenderDecorationButtonIcon::initPainter()
{
    QPen pen = m_painter->pen();

    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    m_painter->setPen(pen);
    m_painter->setBrush(Qt::NoBrush);

    m_totalScalingFactor = m_painter->deviceTransform().m22();
}

void RenderDecorationButtonIcon::renderIcon(KDecoration2::DecorationButtonType type, bool checked)
{
    m_painter->save();
    initPainter();

    switch (type) {
    case DecorationButtonType::Close:
        renderCloseIcon();
        break;

    case DecorationButtonType::Maximize:
        if (checked) {
            renderRestoreIcon();
        } else {
            renderMaximizeIcon();
        }
        break;

    case DecorationButtonType::Minimize:
        renderMinimizeIcon();
        break;

    case DecorationButtonType::OnAllDesktops:
        if (checked) {
            renderPinnedOnAllDesktopsIcon();
        } else {
            renderPinOnAllDesktopsIcon();
        }
        break;

    case DecorationButtonType::Shade:
        if (checked) {
            renderUnShadeIcon();
        } else {
            renderShadeIcon();
        }
        break;

    case DecorationButtonType::KeepBelow:
        renderKeepBehindIcon();
        break;

    case DecorationButtonType::KeepAbove:
        renderKeepInFrontIcon();
        break;

    case DecorationButtonType::ApplicationMenu:
        renderApplicationMenuIcon();
        break;

    case DecorationButtonType::ContextHelp:
        renderContextHelpIcon();
        break;

    default:
        break;
    }

    m_painter->restore();
}

bool RenderDecorationButtonIcon::roundedPenWidthIsOdd(const QPen &pen, qreal &outputRoundedPenWidth, const qreal boldingFactor)
{
    outputRoundedPenWidth = qRound(penWidthToDevice(pen) * boldingFactor) * PenWidth::Symbol;
    bool isOdd(int(outputRoundedPenWidth) % 2 != 0);
    if (!pen.isCosmetic()) {
        outputRoundedPenWidth = outputRoundedPenWidth / m_totalScalingFactor;
    }
    return (isOdd);
}

qreal RenderDecorationButtonIcon::convertDevicePixelsToLocal(const qreal devicePixels)
{
    return (devicePixels / m_totalScalingFactor);
}

qreal RenderDecorationButtonIcon::convertLocalPixelsToDevice(const qreal localPixels)
{
    return (localPixels * m_totalScalingFactor);
}

void RenderDecorationButtonIcon::translatePainterForAliasedPainting(const bool penWidthOdd)
{
    // see https://doc.qt.io/qt-6/coordsys.html for aliased painting co-ordinates
    if (penWidthOdd) {
        m_painter->translate(QPointF(-0.5, -0.5));
    }
}

qreal RenderDecorationButtonIcon::roundCoordToHalf(qreal coord, const ThresholdRound roundAtZero)
{
    qreal coordIntegralPart, coordFractionalPart;
    static constexpr qreal zeroLimit = 0.0001;

    coordFractionalPart = abs(modf(coord, &coordIntegralPart));

    if (coordFractionalPart > (1 - zeroLimit)) { // at 1 -- same as zero
        coordFractionalPart = 0;

        if (roundAtZero == ThresholdRound::Down) {
            coord = coord - (0.5 + coordFractionalPart);
        } else {
            coord = coord + (0.5 - coordFractionalPart);
        }
    } else if (coordFractionalPart > 0.5) {
        coord = coord - (coordFractionalPart - 0.5);
    } else if (coordFractionalPart < 0.5) {
        if (coordFractionalPart < zeroLimit) { // 0
            coordFractionalPart = 0;

            if (roundAtZero == ThresholdRound::Down) {
                coord = coord - (0.5 + coordFractionalPart);
            } else {
                coord = coord + (0.5 - coordFractionalPart);
            }
        } else {
            coord = coord + (0.5 - coordFractionalPart);
        }
    }

    return coord;
}

qreal RenderDecorationButtonIcon::roundCoordToWhole(qreal coord, const ThresholdRound roundAtHalf)
{
    qreal coordIntegralPart, coordFractionalPart;
    static constexpr qreal halfLimit = 0.0001;

    coordFractionalPart = abs(modf(coord, &coordIntegralPart));

    if (coordFractionalPart > (0.5 + halfLimit) || coordFractionalPart < (0.5 - halfLimit)) {
        coord = round(coord);
    } else {
        if (roundAtHalf == ThresholdRound::Down) {
            coord = floor(coord);
        } else {
            coord = ceil(coord);
        }
    }

    return coord;
}

QPointF RenderDecorationButtonIcon::snapToNearestPixel(const QPointF pointLocal)
{
    qreal coordIntegralPart, coordFractionalPart;
    static constexpr qreal halfLimit = 0.0001;
    SnapPixel snapX;
    SnapPixel snapY;

    coordFractionalPart = abs(modf(pointLocal.x(), &coordIntegralPart));
    if (coordFractionalPart < (0.5 + halfLimit) && coordFractionalPart > (0.5 - halfLimit)) { // if around 0.5 snap to a half-pixel
        snapX = m_isOddPenWidth ? SnapPixel::ToHalf : SnapPixel::ToWhole;
    } else {
        snapX = m_isOddPenWidth ? SnapPixel::ToWhole : SnapPixel::ToHalf;
    }

    coordFractionalPart = abs(modf(pointLocal.y(), &coordIntegralPart));
    if (coordFractionalPart < (0.5 + halfLimit) && coordFractionalPart > (0.5 - halfLimit)) { // if around 0.5 snap to a half-pixel
        snapY = m_isOddPenWidth ? SnapPixel::ToHalf : SnapPixel::ToWhole;
    } else {
        snapY = m_isOddPenWidth ? SnapPixel::ToWhole : SnapPixel::ToHalf;
    }

    return (snapToNearestPixel(pointLocal, snapX, snapY));
}

QPointF RenderDecorationButtonIcon::snapToNearestPixel(QPointF pointLocal,
                                                       const SnapPixel snapX,
                                                       const SnapPixel snapY,
                                                       const ThresholdRound roundAtThresholdX,
                                                       const ThresholdRound roundAtThresholdY)
{
    pointLocal *= m_totalScalingFactor;

    // the top-left of the decoration is used as the reference-point at which the pixel is most whole, as this point is snapped to a whole pixel
    pointLocal += m_deviceOffsetFromZeroReference;

    if (snapX == SnapPixel::ToHalf) {
        pointLocal.setX(roundCoordToHalf(pointLocal.x(), roundAtThresholdX));
    } else {
        pointLocal.setX(roundCoordToWhole(pointLocal.x(), roundAtThresholdX));
    }

    if (snapY == SnapPixel::ToHalf) {
        pointLocal.setY(roundCoordToHalf(pointLocal.y(), roundAtThresholdY));
    } else {
        pointLocal.setY(roundCoordToWhole(pointLocal.y(), roundAtThresholdY));
    }

    pointLocal -= m_deviceOffsetFromZeroReference;
    return (pointLocal / m_totalScalingFactor);
}

qreal RenderDecorationButtonIcon::penWidthToLocal(const QPen &pen)
{
    if (pen.isCosmetic()) {
        return convertDevicePixelsToLocal(pen.widthF());
    } else {
        return pen.widthF();
    }
}

qreal RenderDecorationButtonIcon::penWidthToDevice(const QPen &pen)
{
    if (pen.isCosmetic()) {
        return pen.widthF();
    } else {
        return convertLocalPixelsToDevice(pen.widthF());
    }
}

qreal RenderDecorationButtonIcon::straightLineOpacity()
{
    if (m_devicePixelRatio < 1.2)
        return 0.9;
    else
        return 1;
}
}
