/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2021 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "breezedecoration.h"
#include "decorationbuttoncolors.h"
#include <KDecoration3/DecorationButton>

#include <QHash>
#include <QImage>

class QVariantAnimation;

namespace Breeze
{

class Button : public KDecoration3::DecorationButton
{
    Q_OBJECT

public:
    //* constructor
    explicit Button(QObject *parent, const QVariantList &args);

    //* destructor
    virtual ~Button() = default;

    //* button creation
    static Button *create(KDecoration3::DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent);

    //* render
    void paint(QPainter *painter, const QRectF &repaintRegion) override;

    //* standalone buttons
    bool isStandAlone() const
    {
        return m_standAlone;
    }

    //*set position of buttons which are both enabled and visible
    void setLeftButtonVisible(bool value = true)
    {
        m_leftButtonVisible = value;
    }

    void setRightButtonVisible(bool value = true)
    {
        m_rightButtonVisible = value;
    }

    void setLeftmostLeftVisible(bool value = true)
    {
        m_leftmostLeftVisible = value;
    }

    bool isVisibleAfterMenu()
    {
        return m_visibleAfterMenu;
    }

    void setVisibleAfterMenu(bool value = true)
    {
        m_visibleAfterMenu = value;
    }

    bool isVisibleAfterSpacer()
    {
        return m_visibleAfterSpacer;
    }

    void setVisibleAfterSpacer(bool value = true)
    {
        m_visibleAfterSpacer = value;
    }

    void setRightmostLeftVisible(bool value = true)
    {
        m_rightmostLeftVisible = value;
    }

    void setLeftmostRightVisible(bool value = true)
    {
        m_leftmostRightVisible = value;
    }

    void setRightmostRightVisible(bool value = true)
    {
        m_rightmostRightVisible = value;
    }

    bool isVisibleBeforeMenu()
    {
        return m_visibleBeforeMenu;
    }

    void setVisibleBeforeMenu(bool value = true)
    {
        m_visibleBeforeMenu = value;
    }

    bool isVisibleBeforeSpacer()
    {
        return m_visibleBeforeSpacer;
    }

    void setVisibleBeforeSpacer(bool value = true)
    {
        m_visibleBeforeSpacer = value;
    }

    //* offset for drawing icon
    void setIconOffset(const QPointF &value)
    {
        m_iconOffset = value;
    }

    //* horizontal offset, for rendering icon
    void setHorizontalIconOffset(qreal value)
    {
        m_iconOffset.setX(value);
    }

    //* vertical offset, for rendering icon
    void setVerticalIconOffset(qreal value)
    {
        m_iconOffset.setY(value);
    }

    //* offset for drawing full-sized background -- used for the far left button
    void setFullHeightVisibleBackgroundOffset(const QPointF &value)
    {
        m_fullHeightVisibleBackgroundOffset = value;
    }

    //* geometry for rendering a visible button background- can differ from button geometry
    void setBackgroundVisibleSize(const QSizeF &value)
    {
        m_backgroundVisibleSize = value;
    }

    //* set small button padded size
    void setSmallButtonPaddedSize(const QSize &value)
    {
        m_smallButtonPaddedSize = value;
    }

    //* set icon size
    void setIconSize(const QSize &value)
    {
        m_iconSize = value;
    }

    //*@name active state change animation
    //@{
    void setOpacity(qreal value)
    {
        if (m_opacity == value) {
            return;
        }
        m_opacity = value;
        update();
    }

    qreal opacity() const
    {
        return m_opacity;
    }

    //@}

private Q_SLOTS:

    //* apply configuration changes
    void reconfigure();

    //* animation state
    void updateAnimationState(bool);

    //* get colour and trigger same in thin window outline
    void updateThinWindowOutlineWithButtonColor(bool);

private:
    //* private constructor
    explicit Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent = nullptr);

    //* draw button icon
    void drawIcon(QPainter *) const;

    //*@name colors
    //@{
    QColor backgroundColor(const bool getNonAnimatedColor = false) const;
    QColor foregroundColor(const bool getNonAnimatedColor = false) const;
    QColor outlineColor(const bool getNonAnimatedColor = false) const;
    //@}

    //* gets base button state colors with the active change state animation considered
    QColor foregroundNormalActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;
    QColor foregroundHoverActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;
    QColor foregroundPressActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;
    QColor backgroundNormalActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;
    QColor backgroundHoverActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;
    QColor backgroundPressActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;
    QColor outlineNormalActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;
    QColor outlineHoverActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;
    QColor outlinePressActiveStateAnimated(const bool active, const bool getNonAnimatedColor = false) const;

    //* sets m_systemIconName and m_systemIconCheckedName
    void configureSystemIcons();

    bool isSystemIconAvailable() const;

    void setDevicePixelRatio(QPainter *painter);
    void setShouldDrawBoldButtonIcons();
    void setStandardScaledPenWidth();

    /**
     * @brief Paint the button background for the Full-sized Rectangle button shape;
     * @param painter Current QPainter object.
     */
    void paintFullHeightButtonBackground(QPainter *painter) const;

    /**
     * @brief Paint the button background for the small circle button shape as originally done in Breeze
     *        Also paints the small square or small rounded square shapes
     * @param painter Current QPainter object
     */
    void paintSmallSizedButtonBackground(QPainter *painter) const;

    //* Whether to invert the pinned-on-all-desktops icon like in Breeze
    bool titlebarTextPinnedInversion() const;

    //* Pointer to the decoration
    Decoration *m_d;

    //* these are the actual colors to be outputted (including for animations)
    QColor m_backgroundColor;
    QColor m_foregroundColor;
    QColor m_outlineColor;

    bool m_standAlone = false;

    bool m_leftButtonVisible = false;
    bool m_rightButtonVisible = false;
    bool m_leftmostLeftVisible = false;
    bool m_visibleAfterMenu = false;
    bool m_visibleAfterSpacer = false;
    bool m_rightmostLeftVisible = false;
    bool m_leftmostRightVisible = false;
    bool m_rightmostRightVisible = false;
    bool m_visibleBeforeMenu = false;
    bool m_visibleBeforeSpacer = false;

    //* active state change animation
    QVariantAnimation *m_animation;

    //* icon offset (for rendering)
    mutable QPointF m_iconOffset;

    //* offset for rendering a large or Full-sized background
    QPointF m_fullHeightVisibleBackgroundOffset = QPointF();

    //* geometry for rendering a Full-sized button
    QSizeF m_backgroundVisibleSize = QSizeF();

    //* small button size (icon + padding)
    QSize m_smallButtonPaddedSize = QSize();

    //* icon size
    QSize m_iconSize = QSize();

    //* active state change opacity
    qreal m_opacity = 0;

    DecorationButtonPalette *m_buttonPalette = nullptr;
    bool m_renderSystemIcon;
    QString m_systemIconName;
    QString m_systemIconCheckedName;
    bool m_isGtkCsdButton;
    qreal m_devicePixelRatio = 1.0;
    bool m_boldButtonIcons;
    qreal m_standardScaledCosmeticPenWidth = 1.0;
    mutable qreal m_standardScaledNonCosmeticPenWidth = 1.0;
    bool m_titlebarTextPinnedInversion = false;
};

} // namespace
