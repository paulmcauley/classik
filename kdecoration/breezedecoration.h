/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2021-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "breeze.h"

#include "breezesettings.h"
#include "colortools.h"
#include "decorationcolors.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationSettings>
#include <KSharedConfig>

#include <QPainterPath>
#include <QPalette>
#include <QVariant>
#include <QVariantAnimation>

#include <memory>

namespace KDecoration3
{
class DecorationButton;
class DecorationButtonGroup;
}

namespace Breeze
{

enum struct ButtonBackgroundType {
    Small,
    FullHeight,
};

class Decoration : public KDecoration3::Decoration
{
    Q_OBJECT

public:
    //* constructor
    explicit Decoration(QObject *parent = nullptr, const QVariantList &args = QVariantList());

    //* destructor
    virtual ~Decoration();

    //* paint
    void paint(QPainter *painter, const QRectF &repaintRegion) override;

    //* internal settings
    InternalSettingsPtr internalSettings() const
    {
        return m_internalSettings;
    }

    qreal animationsDuration() const
    {
        return m_animation->duration();
    }

    //* caption height
    int captionHeight() const;

    //*@name active state change animation
    //@{
    void setOpacity(qreal);

    qreal opacity() const
    {
        return m_opacity;
    }

    //@}

    //*@name colors
    //@{
    DecorationColors *decorationColors()
    {
        return m_decorationColors.get();
    }

    QColor titleBarColor(bool returnNonAnimatedColor = false) const;
    QColor titleBarSeparatorColor() const;
    QColor fontColor(bool returnNonAnimatedColor = false) const;
    QColor overriddenOutlineColorAnimateIn() const;
    QColor overriddenOutlineColorAnimateOut(const QColor &destinationColor);
    //@}
    //
    //*@name maximization modes
    //@{
    inline bool isMaximized() const;
    inline bool isMaximizedHorizontally() const;
    inline bool isMaximizedVertically() const;

    inline bool isLeftEdge() const;
    inline bool isRightEdge() const;
    inline bool isTopEdge() const;
    inline bool isBottomEdge() const;

    inline bool hideTitleBar() const;
    //@}

    void setThinWindowOutlineOverrideColor(const bool on, const QColor &color);

    QPainterPath *titleBarPath()
    {
        return &m_titleBarPath;
    }
    QPainterPath *windowPath()
    {
        return &m_windowPath;
    }
    qreal systemScaleFactorX11() const
    {
        return m_systemScaleFactorX11;
    }
    ButtonBackgroundType buttonBackgroundType()
    {
        return m_buttonBackgroundType;
    }
    int smallButtonPaddedSize()
    {
        return m_smallButtonPaddedSize;
    }
    int iconSize()
    {
        return m_iconSize;
    }
    int smallButtonBackgroundSize()
    {
        return m_smallButtonBackgroundSize;
    }
    qreal scaledCornerRadius()
    {
        return m_scaledCornerRadius;
    }

    KDecoration3::DecorationButtonGroup *leftButtons()
    {
        return m_leftButtons;
    }

    KDecoration3::DecorationButtonGroup *rightButtons()
    {
        return m_rightButtons;
    }

    QVariantAnimation *activeStateChangeAnimation()
    {
        return m_animation;
    }

    qreal activeStateChangeAnimationOpacity()
    {
        return m_opacity;
    }

Q_SIGNALS:
    void reconfigured();

public Q_SLOTS:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool init() override;
#else
    void init() override;
#endif

private Q_SLOTS:
    void reconfigure()
    {
        reconfigureMain(false);
    }
    void reconfigureWithNoShadowUpdate()
    {
        reconfigureMain(true);
    }
    void generateDecorationColorsOnClientPaletteUpdate(const QPalette &clientPalette);
    void generateDecorationColorsOnDecorationColorSettingsUpdate(QByteArray uuid);
    void generateDecorationColorsOnSystemColorSettingsUpdate(QByteArray uuid);
    void recalculateBorders();
    void updateOpaque();
    void updateBlur();
    void updateButtonsGeometry();
    void updateButtonsGeometryDelayed();
    void updateTitleBar();
    void updateAnimationState();
    void updateShadowOnShadedChange()
    {
        updateShadow();
    }
    void onTabletModeChanged(bool mode);

private:
    //* return the rect in which caption will be drawn
    QPair<QRect, Qt::Alignment> captionRect() const;

    void reconfigureMain(const bool noUpdateShadow = false);
    void updateDecorationColors(const QPalette &clientPalette, QByteArray uuid = "");
    void createButtons();
    void calculateWindowAndTitleBarShapes(const bool windowShapeOnly = false);
    void paintTitleBar(QPainter *painter, const QRectF &repaintRegion);
    void updateShadow(const bool forceUpdateCache = false, bool noCache = false, const bool isThinWindowOutlineOverride = false);
    std::shared_ptr<KDecoration3::DecorationShadow> createShadowObject(QColor shadowColor, const bool isThinWindowOutlineOverride = false);
    void setScaledCornerRadius();

    //*@name border size
    //@{
    int borderSize(bool bottom = false) const;
    inline bool hasBorders() const;
    inline bool hasNoBorders() const;
    inline bool hasNoSideBorders() const;
    //@}

    void setScaledTitleBarTopBottomMargins();
    void setScaledTitleBarSideMargins();
    bool isOpaqueTitleBar();
    int titleBarSeparatorHeight() const;
    qreal devicePixelRatio(QPainter *painter) const;

    //* icon + padding sizes
    void calculateIconSizes();

    //* override thin window outline colour from button colour animation update
    void updateOverrideOutlineFromButtonAnimationState();

    //* calculates and sets m_thinWindowOutline
    void setThinWindowOutlineColor();

    void setGlobalLookAndFeelOptions(QString lookAndFeelPackageName);

    static KSharedConfig::Ptr s_kdeGlobalConfig;
    InternalSettingsPtr m_internalSettings;
    KDecoration3::DecorationButtonGroup *m_leftButtons = nullptr;
    KDecoration3::DecorationButtonGroup *m_rightButtons = nullptr;

    //* Whether the paint() method is active
    bool m_painting = false;

    //* Object to return decoration palette colours
    std::unique_ptr<DecorationColors> m_decorationColors;

    //* active state change animation
    QVariantAnimation *m_animation;
    //* shadow animation
    QVariantAnimation *m_shadowAnimation;
    //*window outline animation when "Colourize with highlighted button'a colour ticked"
    QVariantAnimation *m_overrideOutlineFromButtonAnimation;

    //* active state change animation opacity
    qreal m_opacity = 0;
    //* shadow change animation opacity
    qreal m_shadowOpacity = 0;
    //* overridden thin window outline change animation progress
    qreal m_overrideOutlineAnimationProgress = 0;

    //* frame corner radius, scaled according to smallspacing
    qreal m_scaledCornerRadius = 3.0;

    bool m_tabletMode = false;

    //* titleBar top margin, scaled according to smallspacing
    int m_scaledTitleBarTopMargin = 1;
    //* titleBar bottom margin, scaled according to smallspacing
    int m_scaledTitleBarBottomMargin = 1;
    //* integrated rounded rectangle bottom padding, scaled according to smallspacing
    qreal m_scaledIntegratedRoundedRectangleBottomPadding = 0;

    //* titleBar side margins, scaled according to smallspacing
    int m_scaledTitleBarLeftMargin = 1;
    int m_scaledTitleBarRightMargin = 1;

    //* Rectangular area of titlebar without clipped corners
    QRect m_titleRect;

    //* Exact titlebar path, with clipped rounded corners
    QPainterPath m_titleBarPath = QPainterPath();
    //* Exact window path, with clipped rounded corners
    QPainterPath m_windowPath = QPainterPath();

    qreal m_systemScaleFactorX11 = 1.0;

    ButtonBackgroundType m_buttonBackgroundType = ButtonBackgroundType::Small;
    int m_smallButtonPaddedSize = 20;
    int m_iconSize = 18;
    int m_smallButtonBackgroundSize = 18;

    bool m_colorSchemeHasHeaderColor = true;
    bool m_toolsAreaWillBeDrawn = true;

    //*the actual thin window outline colour to output
    QColor m_thinWindowOutline = QColor();
    //*colour to override thin window outline with, set from decoration button
    QColor m_thinWindowOutlineOverride = QColor();
    //*buffered existing thin window outline colours in case the above override colour is set (needed for animations)
    QColor m_originalThinWindowOutlineActivePreOverride = QColor();
    QColor m_originalThinWindowOutlineInactivePreOverride = QColor();
    //*flag to animate out an overridden thin window outline
    bool m_animateOutOverriddenThinWindowOutline = false;
};

bool Decoration::hasBorders() const
{
    if (m_internalSettings && m_internalSettings->exceptionBorder()) {
        return m_internalSettings->borderSize() > InternalSettings::EnumBorderSize::NoSides;
    } else {
        return settings()->borderSize() > KDecoration3::BorderSize::NoSides;
    }
}

bool Decoration::hasNoBorders() const
{
    if (m_internalSettings && m_internalSettings->exceptionBorder()) {
        return m_internalSettings->borderSize() == InternalSettings::EnumBorderSize::None;
    } else {
        return settings()->borderSize() == KDecoration3::BorderSize::None;
    }
}

bool Decoration::hasNoSideBorders() const
{
    if (m_internalSettings && m_internalSettings->exceptionBorder()) {
        return m_internalSettings->borderSize() == InternalSettings::EnumBorderSize::NoSides;
    } else {
        return settings()->borderSize() == KDecoration3::BorderSize::NoSides;
    }
}

bool Decoration::isMaximized() const
{
    auto c = window();
    return c->isMaximized() && !m_internalSettings->drawBorderOnMaximizedWindows();
}

bool Decoration::isMaximizedHorizontally() const
{
    auto c = window();
    return c->isMaximizedHorizontally() && !m_internalSettings->drawBorderOnMaximizedWindows();
}

bool Decoration::isMaximizedVertically() const
{
    auto c = window();
    return c->isMaximizedVertically() && !m_internalSettings->drawBorderOnMaximizedWindows();
}

bool Decoration::isLeftEdge() const
{
    auto c = window();
    return (c->isMaximizedHorizontally() || c->adjacentScreenEdges().testFlag(Qt::LeftEdge)) && !m_internalSettings->drawBorderOnMaximizedWindows();
}

bool Decoration::isRightEdge() const
{
    auto c = window();

    return (c->isMaximizedHorizontally() || c->adjacentScreenEdges().testFlag(Qt::RightEdge)) && !m_internalSettings->drawBorderOnMaximizedWindows();
}

bool Decoration::isTopEdge() const
{
    auto c = window();

    return (c->isMaximizedVertically() || c->adjacentScreenEdges().testFlag(Qt::TopEdge)) && !m_internalSettings->drawBorderOnMaximizedWindows();
}

bool Decoration::isBottomEdge() const
{
    auto c = window();

    return (c->isMaximizedVertically() || c->adjacentScreenEdges().testFlag(Qt::BottomEdge)) && !m_internalSettings->drawBorderOnMaximizedWindows();
}

bool Decoration::hideTitleBar() const
{
    auto c = window();
    return m_internalSettings->hideTitleBar() && !c->isShaded();
}

} // end Breeze namespace
