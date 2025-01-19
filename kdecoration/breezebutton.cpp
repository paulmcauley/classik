/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2021-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */
#include "breezebutton.h"
#include "breeze.h"
#include "colortools.h"
#include "geometrytools.h"
#include "renderdecorationbuttonicon.h"
#include "systemicontheme.h"

#include <KColorScheme>
#include <KColorUtils>
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/DecorationButtonGroup>
#include <KIconLoader>
#include <KWindowSystem>

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace Breeze
{

using KDecoration3::ColorGroup;
using KDecoration3::ColorRole;

//__________________________________________________________________
Button::Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent)
    : DecorationButton(type, decoration, parent)
    , m_d(qobject_cast<Decoration *>(decoration))
    , m_animation(new QVariantAnimation(this))
    , m_isGtkCsdButton(false)
{
    auto c = decoration->window();

    // setup animation
    // It is important start and end value are of the same type, hence 0.0 and not just 0
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setOpacity(value.toReal());
    });

    // detect the kde-gtk-config-daemon
    // kde-gtk-config has a kded6 module which renders the buttons to svgs for gtk
    if (QCoreApplication::applicationName() == QStringLiteral("kded6")) {
        m_isGtkCsdButton = true;
    }

    // setup default geometry
    int smallButtonPaddedSize = decoration->smallButtonPaddedSize();
    int iconSize = decoration->iconSize();
    int smallButtonBackgroundSize = decoration->smallButtonBackgroundSize();

    setGeometry(QRect(0, 0, smallButtonPaddedSize, smallButtonPaddedSize));
    setSmallButtonPaddedSize(QSize(smallButtonPaddedSize, smallButtonPaddedSize));
    setIconSize(QSize(iconSize, iconSize));
    setBackgroundVisibleSize((QSizeF(smallButtonBackgroundSize, smallButtonBackgroundSize)));

    // connections
    connect(c, SIGNAL(iconChanged(QIcon)), this, SLOT(update()));
    connect(decoration, &Decoration::reconfigured, this, &Button::reconfigure);
    connect(this, &KDecoration3::DecorationButton::hoveredChanged, this, &Button::updateAnimationState);
    connect(this, &KDecoration3::DecorationButton::hoveredChanged, this, &Button::updateThinWindowOutlineWithButtonColor);
    connect(this, &KDecoration3::DecorationButton::pressedChanged, this, &Button::updateThinWindowOutlineWithButtonColor);

    reconfigure();
}

//__________________________________________________________________
Button::Button(QObject *parent, const QVariantList &args)
    : Button(args.at(0).value<KDecoration3::DecorationButtonType>(), args.at(1).value<Decoration *>(), parent)
{
    m_standAlone = true;
    //! small button size must return to !valid because it was altered from the default constructor,
    //! in Standalone mode the button is not using the decoration metrics but its geometry
    m_smallButtonPaddedSize = QSize(-1, -1);
}

//__________________________________________________________________
Button *Button::create(KDecoration3::DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent)
{
    if (auto d = qobject_cast<Decoration *>(decoration)) {
        auto c = d->window();

        Button *b = new Button(type, d, parent);
        switch (type) {
        case KDecoration3::DecorationButtonType::Close:
            b->setVisible(c->isCloseable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::closeableChanged, b, &Breeze::Button::setVisible);
            break;

        case KDecoration3::DecorationButtonType::Maximize:
            b->setVisible(c->isMaximizeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::maximizeableChanged, b, &Breeze::Button::setVisible);
            break;

        case KDecoration3::DecorationButtonType::Minimize:
            b->setVisible(c->isMinimizeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::minimizeableChanged, b, &Breeze::Button::setVisible);
            break;

        case KDecoration3::DecorationButtonType::ContextHelp:
            b->setVisible(c->providesContextHelp());
            QObject::connect(c, &KDecoration3::DecoratedWindow::providesContextHelpChanged, b, &Breeze::Button::setVisible);
            break;

        case KDecoration3::DecorationButtonType::Shade:
            b->setVisible(c->isShadeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::shadeableChanged, b, &Breeze::Button::setVisible);
            break;

        case KDecoration3::DecorationButtonType::Menu:
            QObject::connect(c, &KDecoration3::DecoratedWindow::iconChanged, b, [b]() {
                b->update();
            });
            break;

        default:
            break;
        }

        return b;
    }

    return nullptr;
}

//__________________________________________________________________
void Button::paint(QPainter *painter, const QRectF &repaintRegion)
{
    if (!geometry().intersects(repaintRegion)) {
        return;
    }
    if (!m_d) {
        return;
    }
    auto c = m_d->window();

    m_buttonPalette =
        m_d->decorationColors()->buttonPalette(static_cast<DecorationButtonType>(type())); // this is in paint() in-case caching type on m_buttonPalette changes
    if (!m_buttonPalette) { // this is the case when a spacer button
        return;
    }
    m_titlebarTextPinnedInversion = titlebarTextPinnedInversion();

    setDevicePixelRatio(painter);
    setShouldDrawBoldButtonIcons();
    m_renderSystemIcon = m_d->internalSettings()->buttonIconStyle() == InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme && isSystemIconAvailable();
    setStandardScaledPenWidth();

    m_backgroundColor = this->backgroundColor(m_isGtkCsdButton);
    m_foregroundColor = this->foregroundColor(m_isGtkCsdButton);
    m_outlineColor = this->outlineColor(m_isGtkCsdButton);
    // (the active-state animation breaks over states in m_isGtkCsdButton generation)

    if (!m_smallButtonPaddedSize.isValid() || isStandAlone()) {
        m_smallButtonPaddedSize = geometry().size().toSize();
        int iconWidth = qRound(qreal(m_smallButtonPaddedSize.width()) * 0.9);
        setIconSize(QSize(iconWidth, iconWidth));
        setBackgroundVisibleSize(QSizeF(iconWidth, iconWidth));
    }

    painter->save();

    // menu button (with application icon)
    if (type() == KDecoration3::DecorationButtonType::Menu) {
        // draw a background only with Full-sized background shapes;
        // for standalone/GTK we draw small buttons so can't draw menu
        if (m_d->buttonBackgroundType() == ButtonBackgroundType::FullHeight && !(isStandAlone() || m_isGtkCsdButton))
            paintFullHeightButtonBackground(painter);

        // translate from icon offset -- translates to the edge of smallButtonPaddedSize
        painter->translate(m_iconOffset);

        // translate to draw icon in the centre of smallButtonPaddedWidth (smallButtonPaddedWidth has additional padding)
        qreal iconTranslationOffset = (m_smallButtonPaddedSize.width() - m_iconSize.width()) / 2;
        painter->translate(iconTranslationOffset, iconTranslationOffset);

        const QRectF iconRect(geometry().topLeft(), m_iconSize);

        KIconLoader *iconLoader = KIconLoader::global();
        const QPalette originalPalette = iconLoader->customPalette();
        QPalette palette = c->palette();
        palette.setColor(QPalette::WindowText, m_foregroundColor);

        iconLoader->setCustomPalette(palette);
        c->icon().paint(painter, iconRect.toRect());
        if (originalPalette == QPalette()) {
            iconLoader->resetPalette();
        } else {
            iconLoader->setCustomPalette(originalPalette);
        }

    } else {
        drawIcon(painter);
    }

    painter->restore();
}

//__________________________________________________________________
void Button::drawIcon(QPainter *painter) const
{
    if (!m_d)
        return;

    painter->setRenderHints(QPainter::Antialiasing);

    // for standalone/GTK we draw small buttons so don't do anything
    if (!(isStandAlone() || m_isGtkCsdButton)) {
        // draw a background only with Full-sized Rectangle button shape;
        if (m_d->buttonBackgroundType() == ButtonBackgroundType::FullHeight)
            paintFullHeightButtonBackground(painter);
    }

    QPointF deviceOffsetDecorationTopLeftToIconTopLeft;
    QPointF topLeftPaddedButtonDeviceGeometry = painter->deviceTransform().map(geometry().topLeft());

    // get top-left geometry relative to the decoration top-left as is is what kwin snaps to a whole pixel since Plasma 5.27
    //(on button hover sometimes the painter gives geometry relative to the button rather than to titlebar, so this is also why this is necessary)
    QPointF decorationTopLeftDeviceGeometry = painter->deviceTransform().map(QRectF(m_d->rect()).topLeft());
    deviceOffsetDecorationTopLeftToIconTopLeft = topLeftPaddedButtonDeviceGeometry - decorationTopLeftDeviceGeometry;

    painter->translate(geometry().topLeft());

    // translate from icon offset -- translates to the edge of smallButtonPaddedWidth
    painter->translate(m_iconOffset);
    deviceOffsetDecorationTopLeftToIconTopLeft += (m_iconOffset * painter->device()->devicePixelRatioF());

    const qreal smallButtonPaddedWidth(m_smallButtonPaddedSize.width());

    if (m_isGtkCsdButton) {
        if (smallButtonPaddedWidth > 20) { // outlines appear thin so scale them proportionally
            m_standardScaledNonCosmeticPenWidth = PenWidth::Symbol * smallButtonPaddedWidth / 20;
        } else {
            m_standardScaledNonCosmeticPenWidth = PenWidth::Symbol * qMax((qreal)1.0, 20 / smallButtonPaddedWidth);
        }
    }

    if (m_d->buttonBackgroundType() == ButtonBackgroundType::Small || isStandAlone() || m_isGtkCsdButton)
        paintSmallSizedButtonBackground(painter);

    if (!m_foregroundColor.isValid())
        return;

    // render the actual icon
    qreal iconWidth(m_iconSize.width());

    // translate to draw icon in the centre of smallButtonPaddedWidth (smallButtonPaddedWidth has additional padding)
    qreal iconTranslationOffset = (smallButtonPaddedWidth - iconWidth) / 2;
    painter->translate(iconTranslationOffset, iconTranslationOffset);
    deviceOffsetDecorationTopLeftToIconTopLeft += (QPointF(iconTranslationOffset, iconTranslationOffset) * painter->device()->devicePixelRatioF());

    // setup painter
    QPen pen(m_foregroundColor);

    // cannot use a scaled cosmetic pen if GTK CSD as kde-gtk-config generates svg icons.
    if (m_isGtkCsdButton) {
        pen.setWidthF(PenWidth::Symbol * qMax((qreal)1.0, 18 / iconWidth));
    } else {
        pen.setWidthF(m_standardScaledCosmeticPenWidth);
        pen.setCosmetic(true);
    }
    painter->setPen(pen);

    if (m_renderSystemIcon) {
        auto c = m_d->window();
        QString systemIconName;
        systemIconName = isChecked() ? m_systemIconCheckedName : m_systemIconName;
        SystemIconTheme iconRenderer(painter,
                                     iconWidth,
                                     systemIconName,
                                     m_d->internalSettings(),
                                     m_d->internalSettings()->forceColorizeSystemIcons() ? QPalette() : c->palette());
        iconRenderer.renderIcon();
    } else {
        // at loDPI backgrounds are even, therefore need an even icon in such circumstances for correct centring
        bool forceEvenSquares = (m_isGtkCsdButton || isStandAlone()
                                 || (m_devicePixelRatio <= 1.001
                                     && (m_d->buttonBackgroundType() == ButtonBackgroundType::Small
                                         || m_d->internalSettings()->iconSize() < InternalSettings::EnumIconSize::IconLargeMedium)));
        auto [iconRenderer, localRenderingWidth] = RenderDecorationButtonIcon::factory(m_d->internalSettings(),
                                                                                       painter,
                                                                                       false,
                                                                                       m_boldButtonIcons,
                                                                                       m_devicePixelRatio,
                                                                                       deviceOffsetDecorationTopLeftToIconTopLeft,
                                                                                       forceEvenSquares);

        qreal scaleFactor = iconWidth / localRenderingWidth;
        /*
        scale painter so that all further rendering is preformed inside QRect( 0, 0, localRenderingWidth, localRenderingWidth )
        */
        painter->scale(scaleFactor, scaleFactor);

        iconRenderer->renderIcon(static_cast<DecorationButtonType>(type()), isChecked());
    }
}

//__________________________________________________________________
QColor Button::foregroundColor(const bool getNonAnimatedColor) const
{
    if (!m_d)
        return QColor();

    auto c = m_d->window();
    const bool active = c->isActive();

    // return a variant of normal, hover and press colours, depending on state
    if (isPressed()) {
        return foregroundPressActiveStateAnimated(active, getNonAnimatedColor);
    } else if (isChecked()
               && (type() == KDecoration3::DecorationButtonType::KeepBelow || type() == KDecoration3::DecorationButtonType::KeepAbove
                   || type() == KDecoration3::DecorationButtonType::Shade
                   || (type() == KDecoration3::DecorationButtonType::OnAllDesktops && !m_titlebarTextPinnedInversion))) {
        if (m_d->internalSettings()->buttonStateChecked(active) == InternalSettings::EnumButtonStateChecked::Hover) {
            return foregroundHoverActiveStateAnimated(active, getNonAnimatedColor);
        } else {
            return foregroundPressActiveStateAnimated(active, getNonAnimatedColor);
        }
    } else if (m_animation->state() == QAbstractAnimation::Running && !getNonAnimatedColor) { // button hover animation
        QColor foregroundNormal = foregroundNormalActiveStateAnimated(active, getNonAnimatedColor);
        QColor foregroundHover = foregroundHoverActiveStateAnimated(active, getNonAnimatedColor);
        if (foregroundNormal.isValid() && foregroundHover.isValid()) {
            return KColorUtils::mix(foregroundNormal, foregroundHover, m_opacity);
        } else if (foregroundHover.isValid()) {
            return ColorTools::alphaMix(foregroundHover, m_opacity);
        } else
            return QColor();
    } else if (isHovered()) {
        return foregroundHoverActiveStateAnimated(active, getNonAnimatedColor);
    } else {
        return foregroundNormalActiveStateAnimated(active, getNonAnimatedColor);
    }
}

QColor Button::foregroundNormalActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->foregroundNormal.isValid() && m_buttonPalette->inactive()->foregroundNormal.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->foregroundNormal,
                                    m_buttonPalette->active()->foregroundNormal,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->foregroundNormal.isValid() && !m_buttonPalette->inactive()->foregroundNormal.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->foregroundNormal, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->foregroundNormal.isValid() && m_buttonPalette->inactive()->foregroundNormal.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->foregroundNormal, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else {
            return QColor();
        }
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->foregroundNormal;
    }
}

QColor Button::foregroundHoverActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->foregroundHover.isValid() && m_buttonPalette->inactive()->foregroundHover.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->foregroundHover,
                                    m_buttonPalette->active()->foregroundHover,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->foregroundHover.isValid() && !m_buttonPalette->inactive()->foregroundHover.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->foregroundHover, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->foregroundHover.isValid() && m_buttonPalette->inactive()->foregroundHover.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->foregroundHover, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else {
            return QColor();
        }
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->foregroundHover;
    }
}

QColor Button::foregroundPressActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->foregroundPress.isValid() && m_buttonPalette->inactive()->foregroundPress.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->foregroundPress,
                                    m_buttonPalette->active()->foregroundPress,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->foregroundPress.isValid() && !m_buttonPalette->inactive()->foregroundPress.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->foregroundPress, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->foregroundPress.isValid() && m_buttonPalette->inactive()->foregroundPress.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->foregroundPress, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else
            return QColor();
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->foregroundPress;
    }
}

//__________________________________________________________________
QColor Button::backgroundColor(const bool getNonAnimatedColor) const
{
    if (!m_d) {
        return QColor();
    }

    auto c = m_d->window();
    const bool active = c->isActive();

    // return a variant of normal, hover and press colours, depending on state
    if (isPressed()) {
        return backgroundPressActiveStateAnimated(active, getNonAnimatedColor);
    } else if (isChecked()
               && (type() == KDecoration3::DecorationButtonType::KeepBelow || type() == KDecoration3::DecorationButtonType::KeepAbove
                   || type() == KDecoration3::DecorationButtonType::Shade
                   || (type() == KDecoration3::DecorationButtonType::OnAllDesktops && !m_titlebarTextPinnedInversion))) {
        if (m_d->internalSettings()->buttonStateChecked(active) == InternalSettings::EnumButtonStateChecked::Hover) {
            return backgroundHoverActiveStateAnimated(active, getNonAnimatedColor);
        } else {
            return backgroundPressActiveStateAnimated(active, getNonAnimatedColor);
        }
    } else if (m_animation->state() == QAbstractAnimation::Running && !getNonAnimatedColor) { // button hover animation
        QColor backgroundNormal = backgroundNormalActiveStateAnimated(active, getNonAnimatedColor);
        QColor backgroundHover = backgroundHoverActiveStateAnimated(active, getNonAnimatedColor);
        if (backgroundNormal.isValid() && backgroundHover.isValid()) {
            return KColorUtils::mix(backgroundNormal, backgroundHover, m_opacity);
        } else if (backgroundHover.isValid()) {
            return ColorTools::alphaMix(backgroundHover, m_opacity);
        } else
            return QColor();
    } else if (isHovered()) {
        return backgroundHoverActiveStateAnimated(active, getNonAnimatedColor);
    } else {
        return backgroundNormalActiveStateAnimated(active, getNonAnimatedColor);
    }
}

QColor Button::backgroundNormalActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->backgroundNormal.isValid() && m_buttonPalette->inactive()->backgroundNormal.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->backgroundNormal,
                                    m_buttonPalette->active()->backgroundNormal,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->backgroundNormal.isValid() && !m_buttonPalette->inactive()->backgroundNormal.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->backgroundNormal, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->backgroundNormal.isValid() && m_buttonPalette->inactive()->backgroundNormal.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->backgroundNormal, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else {
            return QColor();
        }
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->backgroundNormal;
    }
}

QColor Button::backgroundHoverActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->backgroundHover.isValid() && m_buttonPalette->inactive()->backgroundHover.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->backgroundHover,
                                    m_buttonPalette->active()->backgroundHover,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->backgroundHover.isValid() && !m_buttonPalette->inactive()->backgroundHover.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->backgroundHover, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->backgroundHover.isValid() && m_buttonPalette->inactive()->backgroundHover.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->backgroundHover, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else {
            return QColor();
        }
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->backgroundHover;
    }
}

QColor Button::backgroundPressActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->backgroundPress.isValid() && m_buttonPalette->inactive()->backgroundPress.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->backgroundPress,
                                    m_buttonPalette->active()->backgroundPress,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->backgroundPress.isValid() && !m_buttonPalette->inactive()->backgroundPress.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->backgroundPress, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->backgroundPress.isValid() && m_buttonPalette->inactive()->backgroundPress.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->backgroundPress, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else
            return QColor();
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->backgroundPress;
    }
}

// Returns a colour if an outline is to be drawn around the button
QColor Button::outlineColor(const bool getNonAnimatedColor) const
{
    if (!m_d)
        return QColor();

    auto c = m_d->window();
    const bool active = c->isActive();

    // return a variant of normal, hover and press colours, depending on state
    if (isPressed()) {
        return outlinePressActiveStateAnimated(active, getNonAnimatedColor);
    } else if (isChecked()
               && (type() == KDecoration3::DecorationButtonType::KeepBelow || type() == KDecoration3::DecorationButtonType::KeepAbove
                   || type() == KDecoration3::DecorationButtonType::Shade
                   || (type() == KDecoration3::DecorationButtonType::OnAllDesktops && !m_titlebarTextPinnedInversion))) {
        if (m_d->internalSettings()->buttonStateChecked(active) == InternalSettings::EnumButtonStateChecked::Hover) {
            return outlineHoverActiveStateAnimated(active, getNonAnimatedColor);
        } else {
            return outlinePressActiveStateAnimated(active, getNonAnimatedColor);
        }
    } else if (m_animation->state() == QAbstractAnimation::Running && !getNonAnimatedColor) { // button hover animation
        QColor outlineHover = outlineHoverActiveStateAnimated(active, getNonAnimatedColor);
        QColor outlineNormal = outlineNormalActiveStateAnimated(active, getNonAnimatedColor);
        if (outlineNormal.isValid() && outlineHover.isValid()) {
            return KColorUtils::mix(outlineNormal, outlineHover, m_opacity);
        } else if (outlineHover.isValid()) {
            return ColorTools::alphaMix(outlineHover, m_opacity);
        } else
            return QColor();
    } else if (isHovered()) {
        return outlineHoverActiveStateAnimated(active, getNonAnimatedColor);
    } else {
        return outlineNormalActiveStateAnimated(active, getNonAnimatedColor);
    }
}

QColor Button::outlineNormalActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->outlineNormal.isValid() && m_buttonPalette->inactive()->outlineNormal.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->outlineNormal,
                                    m_buttonPalette->active()->outlineNormal,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->outlineNormal.isValid() && !m_buttonPalette->inactive()->outlineNormal.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->outlineNormal, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->outlineNormal.isValid() && m_buttonPalette->inactive()->outlineNormal.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->outlineNormal, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else {
            return QColor();
        }
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->outlineNormal;
    }
}

QColor Button::outlineHoverActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->outlineHover.isValid() && m_buttonPalette->inactive()->outlineHover.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->outlineHover,
                                    m_buttonPalette->active()->outlineHover,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->outlineHover.isValid() && !m_buttonPalette->inactive()->outlineHover.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->outlineHover, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->outlineHover.isValid() && m_buttonPalette->inactive()->outlineHover.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->outlineHover, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else {
            return QColor();
        }
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->outlineHover;
    }
}

QColor Button::outlinePressActiveStateAnimated(const bool active, const bool getNonAnimatedColor) const
{
    if (!getNonAnimatedColor && m_d->activeStateChangeAnimation()->state() == QAbstractAnimation::Running) {
        if (m_buttonPalette->active()->outlinePress.isValid() && m_buttonPalette->inactive()->outlinePress.isValid()) {
            return KColorUtils::mix(m_buttonPalette->inactive()->outlinePress,
                                    m_buttonPalette->active()->outlinePress,
                                    m_d->activeStateChangeAnimationOpacity());
        } else if (m_buttonPalette->active()->outlinePress.isValid() && !m_buttonPalette->inactive()->outlinePress.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->active()->outlinePress, m_d->activeStateChangeAnimationOpacity());
        } else if (!m_buttonPalette->active()->outlinePress.isValid() && m_buttonPalette->inactive()->outlinePress.isValid()) {
            return ColorTools::alphaMix(m_buttonPalette->inactive()->outlinePress, (1.0 - m_d->activeStateChangeAnimationOpacity()));
        } else
            return QColor();
    } else {
        const DecorationButtonPaletteGroup *group = active ? m_buttonPalette->active() : m_buttonPalette->inactive();
        return group->outlinePress;
    }
}

bool Button::titlebarTextPinnedInversion() const
{
    if (!m_d)
        return false;
    auto c = m_d->window();
    bool active = c->isActive();

    return type() == KDecoration3::DecorationButtonType::OnAllDesktops
        && m_d->internalSettings()->buttonIconStyle() != InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme
        && (m_d->internalSettings()->buttonBackgroundOpacity(active) > 50 && m_d->internalSettings()->buttonIconOpacity(active) > 50
            && (((m_d->internalSettings()->buttonBackgroundColors(active) == InternalSettings::EnumButtonBackgroundColors::TitleBarText
                  || m_d->internalSettings()->buttonBackgroundColors(active) == InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose)
                 && (m_d->internalSettings()->buttonIconColors(active) == InternalSettings::EnumButtonIconColors::TitleBarText
                     || m_d->internalSettings()->buttonIconColors(active) == InternalSettings::EnumButtonIconColors::TitleBarTextNegativeClose))
                || ((m_d->internalSettings()->buttonBackgroundColors(active) == InternalSettings::EnumButtonBackgroundColors::Accent
                     || m_d->internalSettings()->buttonBackgroundColors(active) == InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose
                     || m_d->internalSettings()->buttonBackgroundColors(active) == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights)
                    && (m_d->internalSettings()->buttonIconColors(active) == InternalSettings::EnumButtonIconColors::Accent
                        || m_d->internalSettings()->buttonIconColors(active) == InternalSettings::EnumButtonIconColors::AccentNegativeClose
                        || m_d->internalSettings()->buttonIconColors(active) == InternalSettings::EnumButtonIconColors::AccentTrafficLights)))

                )
        && !m_d->internalSettings()->showBackgroundNormally(active); // inversion occuring for compatibility with breeze's circular pin on all desktops icon
}

//________________________________________________________________
void Button::reconfigure()
{
    if (!m_d)
        return;

    // animation
    m_animation->setDuration(m_d->animationsDuration());

    // set m_systemIconName and m_systemIconCheckedName if a system icon theme is set
    if (m_d->internalSettings()->buttonIconStyle() == InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme) {
        SystemIconTheme::systemIconNames(static_cast<DecorationButtonType>(type()), m_systemIconName, m_systemIconCheckedName);
    }
}

//__________________________________________________________________
void Button::updateAnimationState(bool hovered)
{
    if (!(m_d && m_d->animationsDuration() > 0)) {
        return;
    }

    m_animation->setDirection(hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
    if (m_animation->state() != QAbstractAnimation::Running) {
        m_animation->start();
    }
}

void Button::updateThinWindowOutlineWithButtonColor(bool on)
{
    if (!m_d || !m_d->internalSettings()->colorizeThinWindowOutlineWithButton() || isStandAlone())
        return;

    QColor color = QColor();
    if (on) {
        m_buttonPalette =
            m_d->decorationColors()->buttonPalette(static_cast<DecorationButtonType>(type())); // this is here in-case caching type on m_buttonPalette changes
        if (!m_buttonPalette) {
            return;
        }
        m_titlebarTextPinnedInversion = titlebarTextPinnedInversion();
        color = this->outlineColor(true); // generate colour again in non-animated state
        if (!color.isValid())
            color = this->backgroundColor(true); // use a background colour if outline colour not valid
        m_d->setThinWindowOutlineOverrideColor(on, color); // generate colour again in non-animated state
    } else {
        if (!isHovered() && isPressed())
            return; // don't remove the window outline highlight if the button is still pressed

        // Check if any other button is hovered/pressed.
        // This is to prevent glitches when you directly mouse over one button to another and the second button does not trigger on.
        // In the case where another button is hovered/pressed do not send an off flag.
        const auto decButtons = m_d->leftButtons()->buttons() + m_d->rightButtons()->buttons();
        for (KDecoration3::DecorationButton *decButton : decButtons) {
            Button *button = static_cast<Button *>(decButton);

            if (button != this && (button->isHovered() || button->isPressed())) {
                return;
            }
        }

        m_d->setThinWindowOutlineOverrideColor(on, color);
    }
}

void Button::paintFullHeightButtonBackground(QPainter *painter) const
{
    if (!m_backgroundColor.isValid() && !m_outlineColor.isValid())
        return;
    if (!m_d)
        return;

    painter->save();
    painter->translate(m_fullHeightVisibleBackgroundOffset);

    qreal cornerRadius = 0;

    if (m_d->internalSettings()->buttonShape() != InternalSettings::EnumButtonShape::ShapeFullHeightRectangle) {
        if (m_d->internalSettings()->buttonCornerRadius() == InternalSettings::EnumButtonCornerRadius::Custom) {
            cornerRadius = m_d->internalSettings()->buttonCustomCornerRadius() / 2.0f * m_d->settings()->smallSpacing();
        } else {
            cornerRadius = m_d->scaledCornerRadius();
        }
        if (cornerRadius < 0.1) {
            cornerRadius = 0;
        }
    }

    QRectF backgroundBoundingRect = (QRectF(geometry().topLeft(), m_backgroundVisibleSize));
    painter->setClipRect(backgroundBoundingRect);
    QPainterPath background;
    QPainterPath outline;
    painter->setPen(Qt::NoPen);

    bool drawOutlineUsingPath = false;

    qreal penWidth = PenWidth::Symbol;
    qreal geometryShrinkOffsetHorizontal = PenWidth::Symbol * 1.5;
    if (KWindowSystem::isPlatformX11()) {
        penWidth *= m_devicePixelRatio;
        geometryShrinkOffsetHorizontal *= m_devicePixelRatio;
    }

    if (m_outlineColor.isValid()) {
        qreal geometryShrinkOffsetVertical = geometryShrinkOffsetHorizontal;

        if (m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeFullHeightRoundedRectangle) {
            // shrink the backgroundBoundingRect to make border more visible
            backgroundBoundingRect = QRectF(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontal,
                                                                            geometryShrinkOffsetVertical,
                                                                            -geometryShrinkOffsetHorizontal,
                                                                            -geometryShrinkOffsetVertical));
            background.addRoundedRect(backgroundBoundingRect, cornerRadius, cornerRadius);

        } else if (m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangle) {
            QPainterPath inner;
            qreal halfPenWidth = penWidth / 2;
            geometryShrinkOffsetHorizontal = halfPenWidth;
            geometryShrinkOffsetVertical = halfPenWidth;
            qreal geometryShrinkOffsetHorizontalOuter = geometryShrinkOffsetHorizontal - halfPenWidth;
            qreal geometryShrinkOffsetHorizontalInner = geometryShrinkOffsetHorizontal + halfPenWidth;
            qreal geometryShrinkOffsetVerticalOuter = geometryShrinkOffsetVertical - halfPenWidth;
            qreal geometryShrinkOffsetVerticalInner = geometryShrinkOffsetVertical + halfPenWidth;

            qreal outerCornerRadius = cornerRadius + halfPenWidth;
            qreal innerCornerRadius = qMax(0.0, cornerRadius - halfPenWidth);

            drawOutlineUsingPath = true;

            if (m_rightmostRightVisible && !m_d->internalSettings()->titleBarRightMargin()) { // right-most-right
                outline = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(0, 0, 0, -geometryShrinkOffsetVerticalOuter),
                                                     CornerBottomLeft,
                                                     outerCornerRadius);
                inner = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(penWidth, 0, 0, -geometryShrinkOffsetVerticalInner),
                                                   CornerBottomLeft,
                                                   innerCornerRadius);
                background = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(halfPenWidth, 0, 0, -geometryShrinkOffsetVertical),
                                                        CornerBottomLeft,
                                                        cornerRadius);
            } else if (m_leftmostLeftVisible && !m_d->internalSettings()->titleBarLeftMargin()) { // left-most-left
                outline = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(0, 0, 0, -geometryShrinkOffsetVerticalOuter),
                                                     CornerBottomRight,
                                                     outerCornerRadius);
                inner = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(0, 0, -penWidth, -geometryShrinkOffsetVerticalInner),
                                                   CornerBottomRight,
                                                   innerCornerRadius);
                background = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(0, 0, -halfPenWidth, -geometryShrinkOffsetVertical),
                                                        CornerBottomRight,
                                                        cornerRadius);
            } else {
                outline = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalOuter,
                                                                                     0,
                                                                                     -geometryShrinkOffsetHorizontalOuter,
                                                                                     -geometryShrinkOffsetVerticalOuter),
                                                     CornersBottom,
                                                     outerCornerRadius);
                inner = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalInner,
                                                                                   0,
                                                                                   -geometryShrinkOffsetHorizontalInner,
                                                                                   -geometryShrinkOffsetVerticalInner),
                                                   CornersBottom,
                                                   innerCornerRadius);
                background = GeometryTools::roundedPath(
                    backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontal, 0, -geometryShrinkOffsetHorizontal, -geometryShrinkOffsetVertical),
                    CornersBottom,
                    cornerRadius);
            }

            outline = outline.subtracted(inner);
        } else if (m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
            if (type() != KDecoration3::DecorationButtonType::Menu) {
                QPainterPath inner;
                qreal halfPenWidth = penWidth / 2;
                geometryShrinkOffsetHorizontal = halfPenWidth;
                geometryShrinkOffsetVertical = halfPenWidth;
                qreal geometryShrinkOffsetHorizontalOuter = geometryShrinkOffsetHorizontal - halfPenWidth;
                qreal geometryShrinkOffsetHorizontalInner = geometryShrinkOffsetHorizontal + halfPenWidth;

                qreal geometryShrinkOffsetHorizontalMiddle;
                qreal geometryShrinkOffsetHorizontalMiddleOuter;
                qreal geometryShrinkOffsetHorizontalMiddleInner;
                if ((m_leftButtonVisible && m_d->internalSettings()->fullHeightButtonSpacingLeft() == 0)
                    || (m_rightButtonVisible && m_d->internalSettings()->fullHeightButtonSpacingRight() == 0)) {
                    geometryShrinkOffsetHorizontalMiddle = 0;
                    geometryShrinkOffsetHorizontalMiddleOuter = -halfPenWidth;
                    geometryShrinkOffsetHorizontalMiddleInner = halfPenWidth;
                } else {
                    geometryShrinkOffsetHorizontalMiddle = geometryShrinkOffsetHorizontal;
                    geometryShrinkOffsetHorizontalMiddleOuter = geometryShrinkOffsetHorizontalOuter;
                    geometryShrinkOffsetHorizontalMiddleInner = geometryShrinkOffsetHorizontalInner;
                }

                qreal geometryShrinkOffsetVerticalOuter = geometryShrinkOffsetVertical - halfPenWidth;
                qreal geometryShrinkOffsetVerticalInner = geometryShrinkOffsetVertical + halfPenWidth;

                qreal outerCornerRadius = cornerRadius + halfPenWidth;
                qreal innerCornerRadius = qMax(0.0, cornerRadius - halfPenWidth);

                drawOutlineUsingPath = true;

                bool visibleAfterSpacer = m_visibleAfterMenu || m_visibleAfterSpacer;
                bool visibleBeforeSpacer = m_visibleBeforeMenu || m_visibleBeforeSpacer;

                if (((m_leftmostLeftVisible && m_d->internalSettings()->titleBarLeftMargin()) && m_rightmostLeftVisible)
                    || (visibleAfterSpacer && (m_rightmostRightVisible || m_rightmostLeftVisible))
                    || ((m_rightmostRightVisible && m_d->internalSettings()->titleBarRightMargin()) && m_leftmostRightVisible)
                    || (visibleBeforeSpacer && (m_leftmostRightVisible || m_leftmostLeftVisible)) || (visibleBeforeSpacer && visibleAfterSpacer)) {
                    outline = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalOuter,
                                                                                         0,
                                                                                         -geometryShrinkOffsetHorizontalOuter,
                                                                                         -geometryShrinkOffsetVerticalOuter),
                                                         CornersBottom,
                                                         outerCornerRadius);
                    inner = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalInner,
                                                                                       0,
                                                                                       -geometryShrinkOffsetHorizontalInner,
                                                                                       -geometryShrinkOffsetVerticalInner),
                                                       CornersBottom,
                                                       innerCornerRadius);
                    background = GeometryTools::roundedPath(
                        backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontal, 0, -geometryShrinkOffsetHorizontal, -geometryShrinkOffsetVertical),
                        CornersBottom,
                        cornerRadius);
                } else if (m_leftmostLeftVisible && !m_d->internalSettings()->titleBarLeftMargin() && m_rightmostLeftVisible) {
                    outline = GeometryTools::roundedPath(
                        backgroundBoundingRect.adjusted(0, 0, -geometryShrinkOffsetHorizontalOuter, -geometryShrinkOffsetVerticalOuter),
                        CornerBottomRight,
                        outerCornerRadius);
                    inner = GeometryTools::roundedPath(
                        backgroundBoundingRect.adjusted(0, 0, -geometryShrinkOffsetHorizontalInner, -geometryShrinkOffsetVerticalInner),
                        CornerBottomRight,
                        innerCornerRadius);
                    background =
                        GeometryTools::roundedPath(backgroundBoundingRect.adjusted(0, 0, -geometryShrinkOffsetHorizontal, -geometryShrinkOffsetVertical),
                                                   CornerBottomRight,
                                                   cornerRadius);
                } else if (m_rightmostRightVisible && !m_d->internalSettings()->titleBarRightMargin() && m_leftmostRightVisible) {
                    outline = GeometryTools::roundedPath(
                        backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalOuter, 0, 0, -geometryShrinkOffsetVerticalOuter),
                        CornerBottomLeft,
                        outerCornerRadius);
                    inner = GeometryTools::roundedPath(
                        backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalInner, 0, 0, -geometryShrinkOffsetVerticalInner),
                        CornerBottomLeft,
                        innerCornerRadius);
                    background =
                        GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontal, 0, 0, -geometryShrinkOffsetVertical),
                                                   CornerBottomLeft,
                                                   cornerRadius);
                } else if (m_leftmostLeftVisible && !m_d->internalSettings()->titleBarLeftMargin()) {
                    outline.addRect(backgroundBoundingRect.adjusted(0, 0, -geometryShrinkOffsetHorizontalMiddleOuter, -geometryShrinkOffsetVerticalOuter));
                    inner.addRect(backgroundBoundingRect.adjusted(0, 0, -geometryShrinkOffsetHorizontalMiddleInner, -geometryShrinkOffsetVerticalInner));
                    background.addRect(backgroundBoundingRect.adjusted(0, 0, -geometryShrinkOffsetHorizontalMiddle, -geometryShrinkOffsetVertical));
                } else if (m_rightmostRightVisible && !m_d->internalSettings()->titleBarRightMargin()) {
                    outline.addRect(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddleOuter, 0, 0, -geometryShrinkOffsetVerticalOuter));
                    inner.addRect(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddleInner, 0, 0, -geometryShrinkOffsetVerticalInner));
                    background.addRect(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddle, 0, 0, -geometryShrinkOffsetVertical));
                } else if ((m_rightmostRightVisible && m_d->internalSettings()->titleBarRightMargin()) || visibleBeforeSpacer || m_rightmostLeftVisible) {
                    outline = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddleOuter,
                                                                                         0,
                                                                                         -geometryShrinkOffsetHorizontalOuter,
                                                                                         -geometryShrinkOffsetVerticalOuter),
                                                         CornerBottomRight,
                                                         outerCornerRadius);
                    inner = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddleInner,
                                                                                       0,
                                                                                       -geometryShrinkOffsetHorizontalInner,
                                                                                       -geometryShrinkOffsetVerticalInner),
                                                       CornerBottomRight,
                                                       innerCornerRadius);
                    background = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddle,
                                                                                            0,
                                                                                            -geometryShrinkOffsetHorizontal,
                                                                                            -geometryShrinkOffsetVertical),
                                                            CornerBottomRight,
                                                            cornerRadius);
                } else if ((m_leftmostLeftVisible && m_d->internalSettings()->titleBarLeftMargin()) || visibleAfterSpacer || m_leftmostRightVisible) {
                    outline = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalOuter,
                                                                                         0,
                                                                                         -geometryShrinkOffsetHorizontalMiddleOuter,
                                                                                         -geometryShrinkOffsetVerticalOuter),
                                                         CornerBottomLeft,
                                                         outerCornerRadius);
                    inner = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalInner,
                                                                                       0,
                                                                                       -geometryShrinkOffsetHorizontalMiddleInner,
                                                                                       -geometryShrinkOffsetVerticalInner),
                                                       CornerBottomLeft,
                                                       innerCornerRadius);
                    background = GeometryTools::roundedPath(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontal,
                                                                                            0,
                                                                                            -geometryShrinkOffsetHorizontalMiddle,
                                                                                            -geometryShrinkOffsetVertical),
                                                            CornerBottomLeft,
                                                            cornerRadius);
                } else {
                    outline.addRect(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddleOuter,
                                                                    0,
                                                                    -geometryShrinkOffsetHorizontalMiddleOuter,
                                                                    -geometryShrinkOffsetVerticalOuter));
                    inner.addRect(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddleInner,
                                                                  0,
                                                                  -geometryShrinkOffsetHorizontalMiddleInner,
                                                                  -geometryShrinkOffsetVerticalInner));
                    background.addRect(backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontalMiddle,
                                                                       0,
                                                                       -geometryShrinkOffsetHorizontalMiddle,
                                                                       -geometryShrinkOffsetVertical));
                }

                outline = outline.subtracted(inner);
            }
        } else { // plain rectangle

            // shrink the backgroundBoundingRect to make border more visible
            backgroundBoundingRect = backgroundBoundingRect.adjusted(geometryShrinkOffsetHorizontal,
                                                                     geometryShrinkOffsetVertical,
                                                                     -geometryShrinkOffsetHorizontal,
                                                                     -geometryShrinkOffsetVertical);
            background.addRect(backgroundBoundingRect);
        }

    } else { // non-shrunk background without outline
        painter->setPen(Qt::NoPen);
        if (m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeFullHeightRoundedRectangle) {
            background.addRoundedRect(backgroundBoundingRect, cornerRadius, cornerRadius);

        } else if (m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangle) {
            if (m_rightmostRightVisible && !m_d->internalSettings()->titleBarRightMargin()) { // right-most-right
                background = GeometryTools::roundedPath(backgroundBoundingRect, CornerBottomLeft, cornerRadius);
            } else if (m_leftmostLeftVisible && !m_d->internalSettings()->titleBarLeftMargin()) { // left-most-left
                background = GeometryTools::roundedPath(backgroundBoundingRect, CornerBottomRight, cornerRadius);
            } else {
                background = GeometryTools::roundedPath(backgroundBoundingRect, CornersBottom, cornerRadius);
            }
        } else if (m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
            if (type() != KDecoration3::DecorationButtonType::Menu) {
                painter->setPen(Qt::NoPen);
                bool visibleAfterSpacer = m_visibleAfterMenu || m_visibleAfterSpacer;
                bool visibleBeforeSpacer = m_visibleBeforeMenu || m_visibleBeforeSpacer;

                if (((m_leftmostLeftVisible && m_d->internalSettings()->titleBarLeftMargin()) && m_rightmostLeftVisible)
                    || (visibleAfterSpacer && (m_rightmostRightVisible || m_rightmostLeftVisible))
                    || ((m_rightmostRightVisible && m_d->internalSettings()->titleBarRightMargin()) && m_leftmostRightVisible)
                    || (visibleBeforeSpacer && (m_leftmostRightVisible || m_leftmostLeftVisible)) || (visibleBeforeSpacer && visibleAfterSpacer)) {
                    background = GeometryTools::roundedPath(backgroundBoundingRect, CornersBottom, cornerRadius);
                } else if (m_leftmostLeftVisible && !m_d->internalSettings()->titleBarLeftMargin() && m_rightmostLeftVisible) {
                    background = GeometryTools::roundedPath(backgroundBoundingRect, CornerBottomRight, cornerRadius);
                } else if (m_rightmostRightVisible && !m_d->internalSettings()->titleBarRightMargin() && m_leftmostRightVisible) {
                    background = GeometryTools::roundedPath(backgroundBoundingRect, CornerBottomLeft, cornerRadius);
                } else if (m_leftmostLeftVisible && !m_d->internalSettings()->titleBarLeftMargin()) {
                    background.addRect(backgroundBoundingRect);
                } else if (m_rightmostRightVisible && !m_d->internalSettings()->titleBarRightMargin()) {
                    background.addRect(backgroundBoundingRect);
                } else if ((m_rightmostRightVisible && m_d->internalSettings()->titleBarRightMargin()) || visibleBeforeSpacer || m_rightmostLeftVisible) {
                    background = GeometryTools::roundedPath(backgroundBoundingRect, CornerBottomRight, cornerRadius);
                } else if ((m_leftmostLeftVisible && m_d->internalSettings()->titleBarLeftMargin()) || visibleAfterSpacer || m_leftmostRightVisible) {
                    background = GeometryTools::roundedPath(backgroundBoundingRect, CornerBottomLeft, cornerRadius);
                } else {
                    background.addRect(backgroundBoundingRect);
                }
            }
        } else { // plain rectangle
            background.addRect(backgroundBoundingRect);
        }
    }

    // clip the rounded corners using the windowPath
    if (!m_d->isMaximized() && (!(!m_backgroundColor.isValid() && m_outlineColor.isValid() && drawOutlineUsingPath)))
        background = background.intersected(*(m_d->windowPath()));

    if (m_outlineColor.isValid() && !drawOutlineUsingPath) {
        QPen pen(m_outlineColor);
        pen.setWidthF(m_standardScaledCosmeticPenWidth);
        pen.setCosmetic(true);
        painter->setPen(pen);
    }
    if (m_backgroundColor.isValid()) {
        painter->setBrush(m_backgroundColor);
        painter->drawPath(background);
    } else if (m_outlineColor.isValid() && !drawOutlineUsingPath) {
        painter->drawPath(background);
    }

    if (m_outlineColor.isValid() && drawOutlineUsingPath) {
        // clip the rounded corners using the windowPath
        if (!m_d->isMaximized())
            outline = outline.intersected(*(m_d->windowPath()));
        painter->setBrush(m_outlineColor);
        painter->drawPath(outline);
    }

    painter->restore();
}

void Button::paintSmallSizedButtonBackground(QPainter *painter) const
{
    if (!m_backgroundColor.isValid() && (!m_outlineColor.isValid()))
        return;
    if (!m_d)
        return;

    painter->save();

    qreal translationOffset = (m_smallButtonPaddedSize.width() - m_backgroundVisibleSize.width()) / 2;
    painter->translate(translationOffset, translationOffset);
    qreal geometryEnlargeOffset = 0;
    qreal backgroundSize = m_backgroundVisibleSize.width();

    qreal penWidth = m_isGtkCsdButton ? m_standardScaledNonCosmeticPenWidth : PenWidth::Symbol;
    if (KWindowSystem::isPlatformX11()) {
        penWidth *= m_devicePixelRatio;
    }

    if (m_outlineColor.isValid()) {
        QPen pen(m_outlineColor);
        if (m_isGtkCsdButton) { // kde-gtk-config GTK CSD button generator does not work properly with cosmetic pens
            pen.setWidthF(penWidth);
            pen.setCosmetic(false);
        } else { // standard case
            pen.setWidthF(m_standardScaledCosmeticPenWidth); // this is a scaled pen width for use with drawing cosmetic pen outlines
            pen.setCosmetic(true);
        }
        painter->setPen(pen);
    } else
        painter->setPen(Qt::NoPen);
    if (m_backgroundColor.isValid())
        painter->setBrush(m_backgroundColor);
    else
        painter->setBrush(Qt::NoBrush);

    qreal cornerRadiusUnscaled = 0;
    if (m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeSmallRoundedSquare
        || m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeFullHeightRoundedRectangle // case where standalone
        || m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangle // case where standalone
        || m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
        if (m_d->internalSettings()->buttonCornerRadius() == InternalSettings::EnumButtonCornerRadius::Custom) {
            cornerRadiusUnscaled = m_d->internalSettings()->buttonCustomCornerRadius();
        } else {
            cornerRadiusUnscaled = m_d->internalSettings()->windowCornerRadius();
        }
    }

    if (cornerRadiusUnscaled < 0.1 && m_d->internalSettings()->buttonShape() != InternalSettings::EnumButtonShape::ShapeSmallCircle) {
        if (m_outlineColor.isValid())
            geometryEnlargeOffset = penWidth / 2;
        painter->drawRect(QRectF(0 - geometryEnlargeOffset,
                                 0 - geometryEnlargeOffset,
                                 backgroundSize + geometryEnlargeOffset * 2,
                                 backgroundSize + geometryEnlargeOffset * 2));
    } else if (m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeSmallRoundedSquare
               || m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeFullHeightRoundedRectangle // case where standalone
               || m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangle // case where standalone
               || m_d->internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped // case where standalone
    ) {
        qreal cornerRadiusScaled = cornerRadiusUnscaled / 2.0f * m_d->settings()->smallSpacing();

        if (m_outlineColor.isValid())
            geometryEnlargeOffset = penWidth / 2;
        painter->drawRoundedRect(QRectF(0 - geometryEnlargeOffset,
                                        0 - geometryEnlargeOffset,
                                        backgroundSize + geometryEnlargeOffset * 2,
                                        backgroundSize + geometryEnlargeOffset * 2),
                                 cornerRadiusScaled,
                                 cornerRadiusScaled);
    } else {
        painter->drawEllipse(QRectF(0 - geometryEnlargeOffset,
                                    0 - geometryEnlargeOffset,
                                    backgroundSize + geometryEnlargeOffset * 2,
                                    backgroundSize + geometryEnlargeOffset * 2));
    }

    painter->restore();
}

void Button::setDevicePixelRatio(QPainter *painter)
{
    if (!m_d)
        return;
    // determine DPR
    m_devicePixelRatio = painter->device()->devicePixelRatioF();

    // on X11 Kwin just returns 1.0 for the DPR instead of the correct value, so use the scaling setting directly
    if (KWindowSystem::isPlatformX11())
        m_devicePixelRatio = m_d->systemScaleFactorX11();
    if (m_isGtkCsdButton)
        m_devicePixelRatio = 1.0;
}

void Button::setStandardScaledPenWidth()
{
    m_standardScaledCosmeticPenWidth = PenWidth::Symbol;
    m_standardScaledCosmeticPenWidth *= m_devicePixelRatio; // this is assuming you are going to use setCosmetic(true) for pen sizes
}

void Button::setShouldDrawBoldButtonIcons()
{
    if (!m_d)
        return;

    m_boldButtonIcons = false;

    if (!m_isGtkCsdButton) {
        switch (m_d->internalSettings()->boldButtonIcons()) {
        default:
            break;
        case InternalSettings::EnumBoldButtonIcons::BoldIconsHiDpiOnly:
            // If HiDPI system scaling use bold icons
            if (m_devicePixelRatio > 1.2)
                m_boldButtonIcons = true;
            break;
        case InternalSettings::EnumBoldButtonIcons::BoldIconsBold:
            m_boldButtonIcons = true;
            break;
        case InternalSettings::EnumBoldButtonIcons::BoldIconsFine:
            break;
        }
    }
}

bool Button::isSystemIconAvailable() const
{
    if (isChecked()) {
        if (m_systemIconCheckedName.isEmpty())
            return false;
        else
            return true;
    } else {
        if (m_systemIconName.isEmpty())
            return false;
        else
            return true;
    }
}

} // namespace
