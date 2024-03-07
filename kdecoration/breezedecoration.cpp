/*
 * SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2018 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
 * SPDX-FileCopyrightText: 2021-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "breezedecoration.h"

#if KLASSY_DECORATION_DEBUG_MODE
#include "setqdebug_logging.h"
#endif

#include "breezeboxshadowrenderer.h"
#include "breezebutton.h"
#include "breezesettingsprovider.h"
#include "config/breezeconfigwidget.h"
#include "dbusmessages.h"
#include "dbusupdatenotifier.h"
#include "geometrytools.h"

#include <KDecoration2/DecorationButtonGroup>
#include <KDecoration2/DecorationShadow>

#include <KColorUtils>
#include <KConfigGroup>
#include <KPluginFactory>
#include <KWindowSystem>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPainter>
#include <QTextStream>
#include <QTimer>

#include <cmath>
#include <mutex>

K_PLUGIN_FACTORY_WITH_JSON(BreezeDecoFactory, "breeze.json", registerPlugin<Breeze::Decoration>(); registerPlugin<Breeze::Button>();
                           registerPlugin<Breeze::ConfigWidget>();)

namespace
{
struct ShadowParams {
    ShadowParams()
        : offset(QPoint(0, 0))
        , radius(0)
        , opacity(0)
    {
    }

    ShadowParams(const QPoint &offset, int radius, qreal opacity)
        : offset(offset)
        , radius(radius)
        , opacity(opacity)
    {
    }

    QPoint offset;
    int radius;
    qreal opacity;
};

struct CompositeShadowParams {
    CompositeShadowParams() = default;

    CompositeShadowParams(const QPoint &offset, const ShadowParams &shadow1, const ShadowParams &shadow2)
        : offset(offset)
        , shadow1(shadow1)
        , shadow2(shadow2)
    {
    }

    bool isNone() const
    {
        return qMax(shadow1.radius, shadow2.radius) == 0;
    }

    QPoint offset;
    ShadowParams shadow1;
    ShadowParams shadow2;
};

const CompositeShadowParams s_shadowParams[] = {
    // None
    CompositeShadowParams( // hacked in by PAM with small values except with opacity 0; this is to allow a thin window outline to be drawn without a shadow
        QPoint(0, 4),
        ShadowParams(QPoint(0, 0), 16, 0),
        ShadowParams(QPoint(0, -2), 8, 0)),
    // Small
    CompositeShadowParams(QPoint(0, 4), ShadowParams(QPoint(0, 0), 16, 1), ShadowParams(QPoint(0, -2), 8, 0.4)),
    // Medium
    CompositeShadowParams(QPoint(0, 8), ShadowParams(QPoint(0, 0), 32, 0.9), ShadowParams(QPoint(0, -4), 16, 0.3)),
    // Large
    CompositeShadowParams(QPoint(0, 12), ShadowParams(QPoint(0, 0), 48, 0.8), ShadowParams(QPoint(0, -6), 24, 0.2)),
    // Very large
    CompositeShadowParams(QPoint(0, 16), ShadowParams(QPoint(0, 0), 64, 0.7), ShadowParams(QPoint(0, -8), 32, 0.1)),
};

inline CompositeShadowParams lookupShadowParams(int size)
{
    switch (size) {
    case Breeze::InternalSettings::EnumShadowSize::ShadowNone:
        return s_shadowParams[0];
    case Breeze::InternalSettings::EnumShadowSize::ShadowSmall:
        return s_shadowParams[1];
    case Breeze::InternalSettings::EnumShadowSize::ShadowMedium:
        return s_shadowParams[2];
    case Breeze::InternalSettings::EnumShadowSize::ShadowLarge:
        return s_shadowParams[3];
    case Breeze::InternalSettings::EnumShadowSize::ShadowVeryLarge:
        return s_shadowParams[4];
    default:
        // Fallback to the Large size.
        return s_shadowParams[3];
    }
}
}

namespace Breeze
{

KSharedConfig::Ptr Decoration::s_kdeGlobalConfig = KSharedConfig::Ptr();

using KDecoration2::ColorGroup;
using KDecoration2::ColorRole;

static std::mutex g_setGlobalLookAndFeelOptionsMutex;

// cached shadow values
static int g_sDecoCount = 0;
static int g_shadowSizeEnum = InternalSettings::EnumShadowSize::ShadowLarge;
static int g_shadowStrength = 255;
static QColor g_shadowColor = Qt::black;
static qreal g_cornerRadius = 3;
static qreal g_systemScaleFactor = 1;
static bool g_hasNoBorders = true;
static bool g_roundBottomCornersWhenNoBorders = false;
static int g_thinWindowOutlineStyleActive = 0;
static int g_thinWindowOutlineStyleInactive = 0;
static QColor g_thinWindowOutlineColorActive = Qt::black;
static QColor g_thinWindowOutlineColorInactive = Qt::black;
static qreal g_thinWindowOutlineThickness = 1;
static QSharedPointer<KDecoration2::DecorationShadow> g_sShadow;
static QSharedPointer<KDecoration2::DecorationShadow> g_sShadowInactive;

//________________________________________________________________
Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration2::Decoration(parent, args)
    , m_animation(new QVariantAnimation(this))
    , m_shadowAnimation(new QVariantAnimation(this))
    , m_overrideOutlineFromButtonAnimation(new QVariantAnimation(this))

{
#if KLASSY_DECORATION_DEBUG_MODE
    setDebugOutput(KLASSY_QDEBUG_OUTPUT_PATH_RELATIVE_HOME);
#endif
    if (!s_kdeGlobalConfig) {
        s_kdeGlobalConfig = KSharedConfig::openConfig();
    }
    g_sDecoCount++;
}

//________________________________________________________________
Decoration::~Decoration()
{
    g_sDecoCount--;
    if (g_sDecoCount == 0) {
        // last deco destroyed, clean up shadow
        g_sShadow.clear();
    }
}

//________________________________________________________________
void Decoration::setOpacity(qreal value)
{
    if (m_opacity == value) {
        return;
    }
    m_opacity = value;
    update();
}

//________________________________________________________________
QColor Decoration::titleBarColor(bool returnNonAnimatedColor) const
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);
    if (hideTitleBar() && !m_internalSettings->useTitleBarColorForAllBorders())
        return c->color(ColorGroup::Inactive, ColorRole::TitleBar);

    QColor activeTitleBarColor = m_decorationColors->active()->titleBarBase;
    QColor inactiveTitlebarColor = m_decorationColors->inactive()->titleBarBase;
    if (m_internalSettings->opaqueTitleBar() || (m_internalSettings->opaqueMaximizedTitleBars() && c->isMaximized())) {
        activeTitleBarColor.setAlpha(255);
        inactiveTitlebarColor.setAlpha(255);
    }

    // do not animate titlebar if there is a tools area/header area as it causes glitches
    if (!m_toolsAreaWillBeDrawn && (m_animation->state() == QAbstractAnimation::Running) && !returnNonAnimatedColor) {
        return KColorUtils::mix(inactiveTitlebarColor, activeTitleBarColor, m_opacity);
    } else {
        return c->isActive() ? activeTitleBarColor : inactiveTitlebarColor;
    }
}

//________________________________________________________________
QColor Decoration::titleBarSeparatorColor() const
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);
    if (!m_internalSettings->drawTitleBarSeparator())
        return QColor();
    if (m_animation->state() == QAbstractAnimation::Running) {
        QColor color(m_decorationColors->active()->buttonFocus);
        color.setAlpha(color.alpha() * m_opacity);
        return color;
    } else if (c->isActive())
        return m_decorationColors->active()->buttonFocus;
    else
        return QColor();
}

QColor Decoration::overriddenOutlineColorAnimateIn() const
{
    QColor color = m_thinWindowOutlineOverride;
    if (m_overrideOutlineFromButtonAnimation->state() == QAbstractAnimation::Running) {
        auto c = client().toStrongRef();
        Q_ASSERT(c);
        QColor originalColor;
        c->isActive() ? originalColor = m_originalThinWindowOutlineActivePreOverride : originalColor = m_originalThinWindowOutlineInactivePreOverride;

        if (originalColor.isValid())
            return KColorUtils::mix(originalColor, color, m_overrideOutlineAnimationProgress);
        else {
            color.setAlphaF(color.alphaF() * m_overrideOutlineAnimationProgress);
            return color;
        }
    } else
        return color;
}

QColor Decoration::overriddenOutlineColorAnimateOut(const QColor &destinationColor)
{
    if (m_overrideOutlineFromButtonAnimation->state() == QAbstractAnimation::Running) {
        auto c = client().toStrongRef();
        Q_ASSERT(c);
        QColor originalColor;
        c->isActive() ? originalColor = m_originalThinWindowOutlineActivePreOverride : originalColor = m_originalThinWindowOutlineInactivePreOverride;

        if (originalColor.isValid() && destinationColor.isValid()) {
            if (m_overrideOutlineAnimationProgress == 1)
                m_animateOutOverriddenThinWindowOutline = false;
            return KColorUtils::mix(originalColor, destinationColor, m_overrideOutlineAnimationProgress);
        } else if (originalColor.isValid()) {
            QColor color = originalColor;
            color.setAlphaF(originalColor.alphaF() * (1.0 - m_overrideOutlineAnimationProgress));
            if (m_overrideOutlineAnimationProgress == 1)
                m_animateOutOverriddenThinWindowOutline = false;
            return color;
        } else {
            if (m_overrideOutlineAnimationProgress == 1)
                m_animateOutOverriddenThinWindowOutline = false;
            return QColor();
        }
    } else {
        m_animateOutOverriddenThinWindowOutline = false;
        return destinationColor;
    }
}

//________________________________________________________________
QColor Decoration::fontColor(bool returnNonAnimatedColor) const
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    if (m_animation->state() == QAbstractAnimation::Running && !returnNonAnimatedColor) {
        return KColorUtils::mix(m_decorationColors->inactive()->titleBarText, m_decorationColors->active()->titleBarText);
    } else {
        return c->isActive() ? m_decorationColors->active()->titleBarText : m_decorationColors->inactive()->titleBarText;
    }
}

//________________________________________________________________
void Decoration::init()
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    reconfigureMain(true);
    
    // active state change animation
    // It is important start and end value are of the same type, hence 0.0 and not just 0
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    // Linear to have the same easing as Breeze animations
    m_animation->setEasingCurve(QEasingCurve::Linear);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setOpacity(value.toReal());
    });

    m_shadowAnimation->setStartValue(0.0);
    m_shadowAnimation->setEndValue(1.0);
    m_shadowAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_shadowAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_shadowOpacity = value.toReal();
        if (m_shadowAnimation->state() == QAbstractAnimation::Running)
            updateShadow();
    });

    m_overrideOutlineFromButtonAnimation->setStartValue(0.0);
    m_overrideOutlineFromButtonAnimation->setEndValue(1.0);
    m_overrideOutlineFromButtonAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    connect(m_overrideOutlineFromButtonAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_overrideOutlineAnimationProgress = value.toReal();
        if (m_overrideOutlineFromButtonAnimation->state() == QAbstractAnimation::Running)
            updateShadow(false, true, true);
    });

    // use DBus connection to update on Klassy configuration change
    auto dbus = QDBusConnection::sessionBus();

    dbus.connect(QString(),
                 QStringLiteral("/KGlobalSettings"),
                 QStringLiteral("org.kde.KGlobalSettings"),
                 QStringLiteral("notifyChange"),
                 this,
                 SLOT(reconfigure()));

    // Implement tablet mode DBus connection
    dbus.connect(QStringLiteral("org.kde.KWin"),
                 QStringLiteral("/org/kde/KWin"),
                 QStringLiteral("org.kde.KWin.TabletModeManager"),
                 QStringLiteral("tabletModeChanged"),
                 QStringLiteral("b"),
                 this,
                 SLOT(onTabletModeChanged(bool)));

    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                  QStringLiteral("/org/kde/KWin"),
                                                  QStringLiteral("org.freedesktop.DBus.Properties"),
                                                  QStringLiteral("Get"));
    message.setArguments({QStringLiteral("org.kde.KWin.TabletModeManager"), QStringLiteral("tabletMode")});
    auto call = new QDBusPendingCallWatcher(dbus.asyncCall(message), this);
    connect(call, &QDBusPendingCallWatcher::finished, this, [this, call]() {
        QDBusPendingReply<QVariant> reply = *call;
        if (!reply.isError()) {
            onTabletModeChanged(reply.value().toBool());
        }

        call->deleteLater();
    });

    updateTitleBar();
    auto s = settings();
    connect(s.data(), &KDecoration2::DecorationSettings::borderSizeChanged, this, &Decoration::recalculateBorders);
    connect(s.data(), &KDecoration2::DecorationSettings::borderSizeChanged, this, &Decoration::updateBlur); // for the case when a border with transparency

    // a change in font might cause the borders to change
    connect(s.data(), &KDecoration2::DecorationSettings::fontChanged, this, &Decoration::recalculateBorders);
    connect(s.data(), &KDecoration2::DecorationSettings::fontChanged, this, &Decoration::updateBlur); // for the case when a border with transparency
    connect(s.data(), &KDecoration2::DecorationSettings::spacingChanged, this, &Decoration::recalculateBorders);
    connect(s.data(), &KDecoration2::DecorationSettings::spacingChanged, this, &Decoration::updateBlur); // for the case when a border with transparency

    // color cache update
    // The slot will only update if the UUID has changed, hence preventing unnecessary multiple colour cache updates
    connect(&g_dBusUpdateNotifier, &DBusUpdateNotifier::decorationSettingsUpdate, this, &Decoration::generateDecorationColorsOnDecorationColorSettingsUpdate);
    connect(&g_dBusUpdateNotifier, &DBusUpdateNotifier::systemColorSchemeUpdate, this, &Decoration::generateDecorationColorsOnSystemColorSettingsUpdate);
    connect(&g_dBusUpdateNotifier, &DBusUpdateNotifier::systemIconsUpdate, this, [this]() {
        if (m_internalSettings->buttonIconStyle() == InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme) {
            Q_EMIT reconfigured(); // this will trigger Button::reconfigure
        }
    });
    connect(c.data(), &KDecoration2::DecoratedClient::paletteChanged, this, &Decoration::generateDecorationColorsOnClientPaletteUpdate);

    // buttons
    connect(s.data(), &KDecoration2::DecorationSettings::spacingChanged, this, &Decoration::updateButtonsGeometryDelayed);
    connect(s.data(), &KDecoration2::DecorationSettings::decorationButtonsLeftChanged, this, &Decoration::updateButtonsGeometryDelayed);
    connect(s.data(), &KDecoration2::DecorationSettings::decorationButtonsRightChanged, this, &Decoration::updateButtonsGeometryDelayed);

    // full reconfiguration
    connect(s.data(), &KDecoration2::DecorationSettings::reconfigured, this, &Decoration::reconfigure);
    connect(s.data(), &KDecoration2::DecorationSettings::reconfigured, this, &Decoration::updateButtonsGeometryDelayed);

    connect(c.data(), &KDecoration2::DecoratedClient::adjacentScreenEdgesChanged, this, &Decoration::recalculateBorders);
    connect(c.data(), &KDecoration2::DecoratedClient::maximizedHorizontallyChanged, this, &Decoration::recalculateBorders);
    connect(c.data(), &KDecoration2::DecoratedClient::maximizedVerticallyChanged, this, &Decoration::recalculateBorders);
    connect(c.data(), &KDecoration2::DecoratedClient::shadedChanged, this, &Decoration::recalculateBorders);
    connect(c.data(), &KDecoration2::DecoratedClient::shadedChanged, this, &Decoration::updateShadowOnShadedChange);
    connect(c.data(), &KDecoration2::DecoratedClient::captionChanged, this, [this]() {
        // update the caption area
        update(titleBar());
    });

    connect(c.data(), &KDecoration2::DecoratedClient::activeChanged, this, &Decoration::updateAnimationState);
    connect(c.data(), &KDecoration2::DecoratedClient::activeChanged, this, &Decoration::updateOpaque);
    connect(c.data(), &KDecoration2::DecoratedClient::activeChanged, this, &Decoration::updateBlur);
    connect(c.data(), &KDecoration2::DecoratedClient::widthChanged, this, &Decoration::updateTitleBar);
    connect(c.data(), &KDecoration2::DecoratedClient::sizeChanged, this, &Decoration::updateBlur);

    connect(c.data(), &KDecoration2::DecoratedClient::maximizedChanged, this, &Decoration::updateTitleBar);
    connect(c.data(), &KDecoration2::DecoratedClient::maximizedChanged, this, &Decoration::updateOpaque);

    connect(c.data(), &KDecoration2::DecoratedClient::widthChanged, this, &Decoration::updateButtonsGeometry);
    connect(c.data(), &KDecoration2::DecoratedClient::maximizedChanged, this, &Decoration::updateButtonsGeometry);
    connect(c.data(), &KDecoration2::DecoratedClient::adjacentScreenEdgesChanged, this, &Decoration::updateButtonsGeometry);
    connect(c.data(), &KDecoration2::DecoratedClient::shadedChanged, this, &Decoration::updateButtonsGeometry);

    createButtons();
    updateShadow();
}

//________________________________________________________________
void Decoration::updateTitleBar()
{
    auto s = settings();
    auto c = client().toStrongRef();
    Q_ASSERT(c);
    // TODO: check whether isMaximized() should really be split into isMaximizedHorizontally() and isMaximizedVertically()
    const bool maximized = isMaximized();
    int width, height, x, y;
    setScaledTitleBarTopBottomMargins();
    setScaledTitleBarSideMargins();

    // prevents resize handles appearing in button at top window edge for large full-height buttons
    if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight && !(m_internalSettings->drawBorderOnMaximizedWindows() && c->isMaximizedVertically())) {
        width = maximized ? c->width() : c->width() - m_scaledTitleBarLeftMargin - m_scaledTitleBarRightMargin;
        height = borderTop();
        x = maximized ? 0 : m_scaledTitleBarLeftMargin;
        y = 0;

    } else {
        // for smaller circular buttons increase the resizable area
        width = maximized ? c->width() : c->width() - m_scaledTitleBarLeftMargin - m_scaledTitleBarRightMargin;
        height = maximized ? borderTop() : borderTop() - m_scaledTitleBarTopMargin;
        x = maximized ? 0 : m_scaledTitleBarLeftMargin;
        y = maximized ? 0 : m_scaledTitleBarTopMargin;
    }

    setTitleBar(QRect(x, y, width, height));
}

// For Titlebar active state and shadow animations only
void Decoration::updateAnimationState()
{
    if (m_shadowAnimation->duration() > 0) {
        auto c = client().toStrongRef();
        Q_ASSERT(c);
        m_shadowAnimation->setDirection(c->isActive() ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        m_shadowAnimation->setEasingCurve(c->isActive() ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
        if (m_shadowAnimation->state() != QAbstractAnimation::Running) {
            m_shadowAnimation->start();
        }

    } else {
        updateShadow();
    }

    if (m_animation->duration() > 0) {
        auto c = client().toStrongRef();
        Q_ASSERT(c);
        m_animation->setDirection(c->isActive() ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        if (m_animation->state() != QAbstractAnimation::Running) {
            m_animation->start();
        }

    } else {
        update();
    }
}

// For overriding thin window outline with button colour
void Decoration::updateOverrideOutlineFromButtonAnimationState()
{
    if (m_overrideOutlineFromButtonAnimation->duration() > 0) {
        m_overrideOutlineFromButtonAnimation->setDirection(QAbstractAnimation::Forward);
        m_overrideOutlineFromButtonAnimation->setEasingCurve(QEasingCurve::InOutQuad);
        if (m_overrideOutlineFromButtonAnimation->state() != QAbstractAnimation::Running)
            m_overrideOutlineFromButtonAnimation->start();

    } else {
        updateShadow(false, true, true);
    }
}

//________________________________________________________________
int Decoration::borderSize(bool bottom) const
{
    const int baseSize = settings()->smallSpacing();
    if (m_internalSettings && (m_internalSettings->exceptionBorder())) {
        switch (m_internalSettings->borderSize()) {
        case InternalSettings::EnumBorderSize::None:
            return 0;
        case InternalSettings::EnumBorderSize::NoSides:
            return bottom ? qMax(4, baseSize) : 0;
        default:
        case InternalSettings::EnumBorderSize::Tiny:
            return bottom ? qMax(4, baseSize) : baseSize;
        case InternalSettings::EnumBorderSize::Normal:
            return baseSize * 2;
        case InternalSettings::EnumBorderSize::Large:
            return baseSize * 3;
        case InternalSettings::EnumBorderSize::VeryLarge:
            return baseSize * 4;
        case InternalSettings::EnumBorderSize::Huge:
            return baseSize * 5;
        case InternalSettings::EnumBorderSize::VeryHuge:
            return baseSize * 6;
        case InternalSettings::EnumBorderSize::Oversized:
            return baseSize * 10;
        }

    } else {
        switch (settings()->borderSize()) {
        case KDecoration2::BorderSize::None:
            return 0;
        case KDecoration2::BorderSize::NoSides:
            return bottom ? qMax(4, baseSize) : 0;
        default:
        case KDecoration2::BorderSize::Tiny:
            return bottom ? qMax(4, baseSize) : baseSize;
        case KDecoration2::BorderSize::Normal:
            return baseSize * 2;
        case KDecoration2::BorderSize::Large:
            return baseSize * 3;
        case KDecoration2::BorderSize::VeryLarge:
            return baseSize * 4;
        case KDecoration2::BorderSize::Huge:
            return baseSize * 5;
        case KDecoration2::BorderSize::VeryHuge:
            return baseSize * 6;
        case KDecoration2::BorderSize::Oversized:
            return baseSize * 10;
        }
    }
}

//________________________________________________________________
void Decoration::reconfigureMain(const bool noUpdateShadow)
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    SettingsProvider::self()->reconfigure();
    m_internalSettings = SettingsProvider::self()->internalSettings(this);

    QPalette clientPalette = c->palette();
    updateDecorationColors(clientPalette);

    s_kdeGlobalConfig->reparseConfiguration();
    if (KWindowSystem::isPlatformX11()) {
        // loads system ScaleFactor from ~/.config/kdeglobals
        const KConfigGroup cgKScreen(s_kdeGlobalConfig, QStringLiteral("KScreen"));
        m_systemScaleFactorX11 = cgKScreen.readEntry("ScaleFactor", 1.0f);
    }

    setScaledCornerRadius();
    setScaledTitleBarTopBottomMargins();
    setScaledTitleBarSideMargins();

    if (m_internalSettings->buttonShape() == InternalSettings::EnumButtonShape::ShapeFullHeightRectangle
        || m_internalSettings->buttonShape() == InternalSettings::EnumButtonShape::ShapeFullHeightRoundedRectangle
        || m_internalSettings->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangle
        || m_internalSettings->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped)
        m_buttonBackgroundType = ButtonBackgroundType::FullHeight;
    else
        m_buttonBackgroundType = ButtonBackgroundType::Small;

    calculateButtonHeights();

    const KConfigGroup cg(s_kdeGlobalConfig, QStringLiteral("KDE"));

    setGlobalLookAndFeelOptions(cg.readEntry("LookAndFeelPackage"));

    m_colorSchemeHasHeaderColor = KColorScheme::isColorSetSupported(s_kdeGlobalConfig, KColorScheme::Header);

    // m_toolsAreaWillBeDrawn = ( m_colorSchemeHasHeaderColor && ( settings()->borderSize() == KDecoration2::BorderSize::None || settings()->borderSize() ==
    // KDecoration2::BorderSize::NoSides ) );
    m_toolsAreaWillBeDrawn = (m_colorSchemeHasHeaderColor);

    // animation
    if (m_internalSettings->animationsEnabled()) {
        qreal animationsDurationFactorRelativeSystem = 1;
        if (m_internalSettings->animationsSpeedRelativeSystem() < 0)
            animationsDurationFactorRelativeSystem = (-m_internalSettings->animationsSpeedRelativeSystem() + 2) / 2.0f;
        else if (m_internalSettings->animationsSpeedRelativeSystem() > 0)
            animationsDurationFactorRelativeSystem = 1 / ((m_internalSettings->animationsSpeedRelativeSystem() + 2) / 2.0f);
        m_animation->setDuration(cg.readEntry("AnimationDurationFactor", 1.0f) * 150.0f * animationsDurationFactorRelativeSystem);
        m_shadowAnimation->setDuration(m_animation->duration());
        m_overrideOutlineFromButtonAnimation->setDuration(m_animation->duration());
    } else {
        m_animation->setDuration(0);
        m_shadowAnimation->setDuration(0);
        m_overrideOutlineFromButtonAnimation->setDuration(0);
    }

    // borders
    recalculateBorders();

    updateOpaque();
    updateBlur();

    // shadow
    if (!noUpdateShadow)
        this->updateShadow();

    Q_EMIT reconfigured();
}

void Decoration::updateDecorationColors(const QPalette &clientPalette, QByteArray uuid)
{
    QPalette systemPalette = KColorScheme::createApplicationPalette(s_kdeGlobalConfig);
    bool clientSpecificPalette = false;
    if (clientPalette != systemPalette) { // Some applications can set a Window Colour Scheme, meaning the client palette and system palette differ
        clientSpecificPalette = true;
    }

    // The preset exception may modify the decoration colours by having a different translucentButtonBackgroundsOpacity, so in this case we don't want to
    // cache the decoration colours as it may corrupt the colours for normal non-exception decoration windows
    bool noCache = m_internalSettings->property("noCacheException").toBool() || clientSpecificPalette;

    if (noCache) {
        if (!m_decorationColors || m_decorationColors->isCachedPalette()) {
            m_decorationColors = std::make_unique<DecorationColors>(false);
        }
    } else {
        if (!m_decorationColors || !m_decorationColors->isCachedPalette()) {
            m_decorationColors = std::make_unique<DecorationColors>(true);
        }
    }

    QPalette palette = clientSpecificPalette ? clientPalette : systemPalette;
    bool generateColors = false;

    if (!m_decorationColors->areColorsGenerated()) {
        generateColors = true;
    } else {
        if (!uuid.isEmpty()
            && (noCache
                || (!noCache && uuid != m_decorationColors->settingsUpdateUuid()))) { // case from generateDecorationColorsOnDecorationSettingsPaletteUpdate()
            generateColors = true;
        }

        // TODO: palette may not be a reliable indicator of the entire colour scheme - get an update to KDecoration2::DecoratedClient to read QString
        // m_colorScheme instead
        if (!generateColors && palette != *m_decorationColors->basePalette()) {
            generateColors = true;
        }
    }

    if (generateColors) {
        auto c = client().toStrongRef();
        Q_ASSERT(c);

        QColor activeTitleBarBase = c->color(ColorGroup::Active, ColorRole::TitleBar);
        QColor inactiveTitlebarBase = c->color(ColorGroup::Inactive, ColorRole::TitleBar);
        QColor activeTitleBarText = c->color(ColorGroup::Active, ColorRole::Foreground);
        QColor inactiveTitleBarText = c->color(ColorGroup::Inactive, ColorRole::Foreground);

        m_decorationColors->generateDecorationAndButtonColors(palette,
                                                              m_internalSettings,
                                                              activeTitleBarText,
                                                              activeTitleBarBase,
                                                              inactiveTitleBarText,
                                                              inactiveTitlebarBase,
                                                              uuid); // update the decoration colors
    }
}

void Decoration::generateDecorationColorsOnClientPaletteUpdate(const QPalette &clientPalette)
{
    SettingsProvider::self()->reconfigure();
    m_internalSettings = SettingsProvider::self()->internalSettings(this);
    s_kdeGlobalConfig->reparseConfiguration();

    updateDecorationColors(clientPalette);
    reconfigure();
}

void Decoration::generateDecorationColorsOnDecorationColorSettingsUpdate(QByteArray uuid)
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);
    QPalette clientPalette = c->palette();

    SettingsProvider::self()->reconfigure();
    m_internalSettings = SettingsProvider::self()->internalSettings(this);
    s_kdeGlobalConfig->reparseConfiguration();

    updateDecorationColors(clientPalette, uuid);
}

void Decoration::generateDecorationColorsOnSystemColorSettingsUpdate(QByteArray uuid)
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);
    QPalette clientPalette = c->palette();

    SettingsProvider::self()->reconfigure();
    m_internalSettings = SettingsProvider::self()->internalSettings(this);
    s_kdeGlobalConfig->reparseConfiguration();

    updateDecorationColors(clientPalette, uuid);
    reconfigure();
}

void Decoration::setGlobalLookAndFeelOptions(QString lookAndFeelPackageName)
{
    if (lookAndFeelPackageName == m_internalSettings->lookAndFeelSet()) {
        return;
    }

    // only allow one thread at a time to set the look-and-feel options
    std::unique_lock<std::mutex> lock(g_setGlobalLookAndFeelOptionsMutex, std::try_to_lock);
    if (lock.owns_lock()) {
        m_internalSettings->setLookAndFeelSet(lookAndFeelPackageName);
        m_internalSettings->save();

        QString gtkThemeName = KWindowSystem::isPlatformWayland() ? QStringLiteral("Adwaita") : QStringLiteral("Breeze");

        // associate the look-and-feel package with a Klassy window decoration preset
        const QHash<QString, QString> lfPackagePresetNames{
            {QStringLiteral("org.kde.klassydarkleftpanel.desktop"), QStringLiteral("Klassy")},
            {QStringLiteral("org.kde.klassylightleftpanel.desktop"), QStringLiteral("Klassy")},
            {QStringLiteral("org.kde.klassytwilightleftpanel.desktop"), QStringLiteral("Klassy Twilight")},
            {QStringLiteral("org.kde.klassydarkbottompanel.desktop"), QStringLiteral("Klassy - bottom panel")},
            {QStringLiteral("org.kde.klassylightbottompanel.desktop"), QStringLiteral("Klassy - bottom panel")},
            {QStringLiteral("org.kde.klassytwilightbottompanel.desktop"), QStringLiteral("Klassy Twilight - bottom panel")},
        };

        auto presetNameIt = lfPackagePresetNames.find(lookAndFeelPackageName);
        if (presetNameIt != lfPackagePresetNames.end()) { // if matching look-and-feel-package, load the associated Klassy window decoration preset
            system("klassy-settings -w \"" + presetNameIt.value().toUtf8() + "\" &");
            DBusMessages::setGtkTheme(gtkThemeName);
        }
    }
}

//________________________________________________________________
void Decoration::recalculateBorders()
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);
    auto s = settings();

    // setScaledTitleBarTopBottomMargins();

    // left, right and bottom borders
    const int left = isLeftEdge() ? 0 : borderSize();
    const int right = isRightEdge() ? 0 : borderSize();
    const int bottom = (c->isShaded() || isBottomEdge()) ? 0 : borderSize(true);

    int top = 0;
    if (hideTitleBar()) {
        top = bottom;
    } else {
        QFontMetrics fm(s->font());
        top += qMax(fm.height(), m_smallButtonPaddedHeight);

        // padding below
        top += titleBarSeparatorHeight();
        top += (m_scaledTitleBarTopMargin + m_scaledTitleBarBottomMargin);
    }

    setBorders(QMargins(left, top, right, bottom));

    // extended sizes
    const int extSize = s->largeSpacing();
    int extLeft = 0;
    int extRight = 0;
    int extBottom = 0;
    int extTop = 0;

    // Add extended resize handles for Full-sized Rectangle highlight as they cannot overlap with larger full-sized buttons
    if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight) {
        if (!isMaximizedVertically())
            extTop = extSize;
    }

    if (hasNoBorders()) {
        if (!isMaximizedHorizontally()) {
            extLeft = extSize;
            extRight = extSize;
        }
        if (!isMaximizedVertically()) {
            extBottom = extSize;
        }

    } else if (!isMaximizedHorizontally()) {
        if (hasNoSideBorders()) {
            extLeft = extSize;
            extRight = extSize;
        } else {
            if (m_internalSettings->titleBarLeftMargin() == 0)
                extLeft = extSize;
            if (m_internalSettings->titleBarRightMargin() == 0)
                extRight = extSize;
        }
    }

    setResizeOnlyBorders(QMargins(extLeft, extTop, extRight, extBottom));
}

//________________________________________________________________
void Decoration::createButtons()
{
    m_leftButtons = new KDecoration2::DecorationButtonGroup(KDecoration2::DecorationButtonGroup::Position::Left, this, &Button::create);
    m_rightButtons = new KDecoration2::DecorationButtonGroup(KDecoration2::DecorationButtonGroup::Position::Right, this, &Button::create);
    updateButtonsGeometry();
}

//________________________________________________________________
void Decoration::updateButtonsGeometryDelayed()
{
    QTimer::singleShot(0, this, &Decoration::updateButtonsGeometry);
}

//________________________________________________________________
void Decoration::updateButtonsGeometry()
{
    const auto s = settings();
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    setScaledTitleBarSideMargins();

    // adjust button position
    qreal bHeightNormal;
    qreal bWidth = 0;
    const int &smallButtonPaddedWidth = m_smallButtonPaddedHeight;
    qreal verticalIconOffsetNormal = 0;
    qreal bHeightMenuGrouped = 0; // used only for the menu button with Integrated rounded rectangle, grouped
    qreal verticalIconOffsetMenuGrouped = 0; // used only for the menu button with Integrated rounded rectangle, grouped
    qreal horizontalIconOffsetLeftButtons = 0;
    qreal horizontalIconOffsetLeftFullHeightClose = 0;
    qreal horizontalIconOffsetRightButtons = 0;
    qreal horizontalIconOffsetRightFullHeightClose = 0;
    int buttonTopMargin = m_scaledTitleBarTopMargin;
    int buttonSpacingLeft = 0;
    int buttonSpacingRight = 0;
    int titleBarSeparatorHeight = this->titleBarSeparatorHeight();

    if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight) {
        bHeightNormal = borderTop();
        bHeightNormal = qMax(bHeightNormal - titleBarSeparatorHeight, 0.0);
        if (internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangle
            || internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
            if (internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
                bHeightMenuGrouped = bHeightNormal;
            }
            bHeightNormal = qMax(bHeightNormal - m_scaledIntegratedRoundedRectangleBottomPadding, 0.0);
            qreal shrinkOffsetWithOutline = 0;
            if (m_internalSettings->showOutlineNormally(true) || m_internalSettings->showOutlineOnHover(true) || m_internalSettings->showOutlineOnPress(true)
                || m_internalSettings->showOutlineNormally(false) || m_internalSettings->showOutlineOnHover(false)
                || m_internalSettings->showOutlineOnPress(false)) {
                shrinkOffsetWithOutline = PenWidth::Symbol / 2;
                if (KWindowSystem::isPlatformX11()) {
                    shrinkOffsetWithOutline *= m_systemScaleFactorX11;
                }
            }
            verticalIconOffsetNormal = buttonTopMargin + (captionHeight() - m_smallButtonPaddedHeight) / 2 - m_scaledIntegratedRoundedRectangleBottomPadding / 2
                - shrinkOffsetWithOutline;
            if (internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
                verticalIconOffsetMenuGrouped = buttonTopMargin + (captionHeight() - m_smallButtonPaddedHeight) / 2;
            }
        } else {
            verticalIconOffsetNormal = buttonTopMargin + (captionHeight() - m_smallButtonPaddedHeight) / 2;
        }

        buttonSpacingLeft = s->smallSpacing() * m_internalSettings->fullHeightButtonSpacingLeft();
        buttonSpacingRight = s->smallSpacing() * m_internalSettings->fullHeightButtonSpacingRight();
    } else {
        bHeightNormal = captionHeight() + (isTopEdge() ? buttonTopMargin : 0);
        verticalIconOffsetNormal = (isTopEdge() ? buttonTopMargin : 0) + qreal(captionHeight() - m_smallButtonPaddedHeight) / 2;

        buttonSpacingLeft = s->smallSpacing() * m_internalSettings->buttonSpacingLeft();
        buttonSpacingRight = s->smallSpacing() * m_internalSettings->buttonSpacingRight();
    }

    int leftmostLeftVisibleIndex = -1;
    int rightmostLeftVisibleIndex = -1;
    int menuButtonVisibleIndexLeft = -1;
    bool visibleAfterMenuSetLeft = false;
    int numLeftButtons = m_leftButtons->buttons().count();

    for (int i = 0; i < numLeftButtons; i++) {
        Button *button = static_cast<Button *>(m_leftButtons->buttons()[i].data());

        qreal bHeight = bHeightNormal;
        qreal verticalIconOffset = verticalIconOffsetNormal;
        if (button->type() == KDecoration2::DecorationButtonType::Menu) {
            if (button->isVisible() && button->isEnabled()) {
                menuButtonVisibleIndexLeft = i;
            }
            if (internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
                bHeight = bHeightMenuGrouped;
                verticalIconOffset = verticalIconOffsetMenuGrouped;
            }
        }

        if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight) {
            if (button->type() == KDecoration2::DecorationButtonType::Close) {
                qreal bWidthMargin = s->smallSpacing() * m_internalSettings->fullHeightButtonWidthMarginLeft()
                    * m_internalSettings->closeFullHeightButtonWidthMarginRelative() / 100.0f;
                bWidth = m_smallButtonPaddedHeight + bWidthMargin;
                horizontalIconOffsetLeftFullHeightClose = bWidthMargin / 2;
            } else {
                qreal bWidthMargin = s->smallSpacing() * m_internalSettings->fullHeightButtonWidthMarginLeft();
                bWidth = m_smallButtonPaddedHeight + bWidthMargin;
                horizontalIconOffsetLeftButtons = bWidthMargin / 2;
            }
            button->setBackgroundVisibleSize(QSizeF(bWidth, bHeight));
        } else {
            bWidth = m_smallButtonPaddedHeight;
            horizontalIconOffsetLeftButtons = 0;
            button->setBackgroundVisibleSize(QSizeF(m_smallButtonBackgroundHeight, m_smallButtonBackgroundHeight));
        }

        button->setGeometry(QRectF(QPoint(0, 0), QSizeF(bWidth, bHeight)));
        if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight && button->type() == KDecoration2::DecorationButtonType::Close) {
            button->setIconOffset(QPointF(horizontalIconOffsetLeftFullHeightClose, verticalIconOffset));
        } else {
            button->setIconOffset(QPointF(horizontalIconOffsetLeftButtons, verticalIconOffset));
        }
        button->setSmallButtonPaddedSize(QSize(smallButtonPaddedWidth, smallButtonPaddedWidth));
        button->setIconSize(QSize(m_iconHeight, m_iconHeight));

        // determine leftmost left visible and rightmostLeftVisible
        if (button->isVisible() && button->isEnabled()) {
            button->setLeftButtonVisible(true);
            button->setRightButtonVisible(false);
            button->setLeftmostLeftVisible(false);
            button->setVisibleAfterMenu(false);
            button->setVisibleBeforeMenu(false);
            button->setRightmostLeftVisible(false);

            if (leftmostLeftVisibleIndex == -1) {
                leftmostLeftVisibleIndex = i;
                button->setLeftmostLeftVisible();
            } else if (menuButtonVisibleIndexLeft != -1 && !visibleAfterMenuSetLeft) {
                if (i > menuButtonVisibleIndexLeft) {
                    button->setVisibleAfterMenu(true);
                    visibleAfterMenuSetLeft = true;
                }
            }
            rightmostLeftVisibleIndex = i;
        }
    }

    if (rightmostLeftVisibleIndex != -1) {
        static_cast<Button *>(m_leftButtons->buttons()[rightmostLeftVisibleIndex].data())->setRightmostLeftVisible();

        if (menuButtonVisibleIndexLeft != -1) {
            for (int i = menuButtonVisibleIndexLeft - 1; i >= 0; i--) {
                Button *button = static_cast<Button *>(m_leftButtons->buttons()[i].data());
                if (button->isEnabled() && button->isVisible()) {
                    button->setVisibleBeforeMenu();
                    break;
                }
            }
        }
    }

    int leftmostRightVisibleIndex = -1;
    int rightmostRightVisibleIndex = -1;
    int menuButtonVisibleIndexRight = -1;
    bool visibleAfterMenuSetRight = false;
    int numRightButtons = m_rightButtons->buttons().count();

    for (int i = 0; i < numRightButtons; i++) {
        Button *button = static_cast<Button *>(m_rightButtons->buttons()[i].data());

        qreal bHeight = bHeightNormal;
        qreal verticalIconOffset = verticalIconOffsetNormal;

        if (button->type() == KDecoration2::DecorationButtonType::Menu) {
            if (button->isVisible() && button->isEnabled()) {
                menuButtonVisibleIndexRight = i;
            }
            if (internalSettings()->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
                bHeight = bHeightMenuGrouped;
                verticalIconOffset = verticalIconOffsetMenuGrouped;
            }
        }

        if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight) {
            if (button->type() == KDecoration2::DecorationButtonType::Close) {
                qreal bWidthMargin = s->smallSpacing() * m_internalSettings->fullHeightButtonWidthMarginRight()
                    * m_internalSettings->closeFullHeightButtonWidthMarginRelative() / 100.0f;
                bWidth = m_smallButtonPaddedHeight + bWidthMargin;
                horizontalIconOffsetRightFullHeightClose = bWidthMargin / 2;
            } else {
                qreal bWidthMargin = s->smallSpacing() * m_internalSettings->fullHeightButtonWidthMarginRight();
                bWidth = m_smallButtonPaddedHeight + bWidthMargin;
                horizontalIconOffsetRightButtons = bWidthMargin / 2;
            }
            button->setBackgroundVisibleSize(QSizeF(bWidth, bHeight));
        } else {
            bWidth = m_smallButtonPaddedHeight;
            horizontalIconOffsetRightButtons = 0;
            button->setBackgroundVisibleSize(QSizeF(m_smallButtonBackgroundHeight, m_smallButtonBackgroundHeight));
        }

        button->setGeometry(QRectF(QPoint(0, 0), QSizeF(bWidth, bHeight)));
        if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight && button->type() == KDecoration2::DecorationButtonType::Close) {
            button->setIconOffset(QPointF(horizontalIconOffsetRightFullHeightClose, verticalIconOffset));
        } else {
            button->setIconOffset(QPointF(horizontalIconOffsetRightButtons, verticalIconOffset));
        }
        button->setSmallButtonPaddedSize(QSize(smallButtonPaddedWidth, smallButtonPaddedWidth));
        button->setIconSize(QSize(m_iconHeight, m_iconHeight));

        // determine leftmost right visible and rightmostRightVisible
        if (button->isVisible() && button->isEnabled()) {
            button->setRightButtonVisible(true);
            button->setLeftButtonVisible(false);
            button->setLeftmostRightVisible(false);
            button->setVisibleAfterMenu(false);
            button->setVisibleBeforeMenu(false);
            button->setRightmostRightVisible(false);

            if (leftmostRightVisibleIndex == -1) {
                leftmostRightVisibleIndex = i;
                button->setLeftmostRightVisible();
            } else if (menuButtonVisibleIndexRight != -1 && !visibleAfterMenuSetRight) {
                if (i > menuButtonVisibleIndexRight) {
                    button->setVisibleAfterMenu(true);
                    visibleAfterMenuSetRight = true;
                }
            }
            rightmostRightVisibleIndex = i;
        }
    }

    if (rightmostRightVisibleIndex != -1) {
        static_cast<Button *>(m_rightButtons->buttons()[rightmostRightVisibleIndex].data())->setRightmostRightVisible();

        if (menuButtonVisibleIndexRight != -1) {
            for (int i = menuButtonVisibleIndexRight - 1; i >= 0; i--) {
                Button *button = static_cast<Button *>(m_rightButtons->buttons()[i].data());
                if (button->isEnabled() && button->isVisible()) {
                    button->setVisibleBeforeMenu();
                    break;
                }
            }
        }
    }

    // left buttons
    if (!m_leftButtons->buttons().isEmpty() && leftmostLeftVisibleIndex != -1) {
        // spacing
        m_leftButtons->setSpacing(buttonSpacingLeft);

        // padding
        int vPadding;
        if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight)
            vPadding = 0;
        else
            vPadding = isTopEdge() ? 0 : buttonTopMargin;
        const int hPadding = m_scaledTitleBarLeftMargin;

        auto firstButton = static_cast<Button *>(m_leftButtons->buttons()[leftmostLeftVisibleIndex].data());
        firstButton->setFlag(Button::FlagFirstInList);
        if (isLeftEdge()) {
            // add offsets on the side buttons, to preserve padding, but satisfy Fitts law
            firstButton->setGeometry(QRectF(QPoint(0, 0), QSizeF(firstButton->geometry().width() + hPadding, firstButton->geometry().height())));

            if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight && firstButton->type() == KDecoration2::DecorationButtonType::Close) {
                firstButton->setHorizontalIconOffset(horizontalIconOffsetLeftFullHeightClose + hPadding);
            } else {
                firstButton->setHorizontalIconOffset(horizontalIconOffsetLeftButtons + hPadding);
            }
            firstButton->setFullHeightVisibleBackgroundOffset(QPointF(hPadding, 0));

            m_leftButtons->setPos(QPointF(0, vPadding));

        } else {
            m_leftButtons->setPos(QPointF(hPadding + borderLeft(), vPadding));
            firstButton->setFullHeightVisibleBackgroundOffset(QPointF(0, 0));
        }
    }

    // right buttons
    if (!m_rightButtons->buttons().isEmpty() && rightmostRightVisibleIndex != -1) {
        // spacing
        m_rightButtons->setSpacing(buttonSpacingRight);

        // padding
        int vPadding;
        if (m_buttonBackgroundType == ButtonBackgroundType::FullHeight)
            vPadding = 0;
        else
            vPadding = isTopEdge() ? 0 : buttonTopMargin;
        const int hPadding = m_scaledTitleBarRightMargin;

        auto lastButton = static_cast<Button *>(m_rightButtons->buttons()[rightmostRightVisibleIndex].data());
        lastButton->setFlag(Button::FlagLastInList);
        if (isRightEdge()) {
            lastButton->setGeometry(QRectF(QPoint(0, 0), QSizeF(lastButton->geometry().width() + hPadding, lastButton->geometry().height())));

            m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width(), vPadding));

        } else {
            m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width() - hPadding - borderRight(), vPadding));
        }
    }

    update();
}

//________________________________________________________________
void Decoration::paint(QPainter *painter, const QRect &repaintRegion)
{
    m_painting = true;

    // TODO: optimize based on repaintRegion
    auto c = client().toStrongRef();
    Q_ASSERT(c);
    auto s = settings();

    calculateWindowAndTitleBarShapes();

    // paint background
    if (!c->isShaded()) {
        painter->fillRect(rect(), Qt::transparent);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);

        QColor windowBorderColor;
        if (m_internalSettings->useTitleBarColorForAllBorders()) {
            windowBorderColor = titleBarColor();
        } else
            windowBorderColor = c->color(c->isActive() ? ColorGroup::Active : ColorGroup::Inactive, ColorRole::Frame);

        painter->setBrush(windowBorderColor);

        QPainterPath clipRect;
        // use clipRect for clipping away the top part
        if (!hideTitleBar()) {
            clipRect.addRect(0, borderTop(), size().width(), size().height() - borderTop());
            // clip off the titlebar and draw bottom part
            QPainterPath windowPathMinusTitleBar = m_windowPath.intersected(clipRect);
            painter->drawPath(windowPathMinusTitleBar);
        } else {
            painter->drawPath(m_windowPath);
        }

        painter->restore();
    }

    if (!hideTitleBar()) {
        paintTitleBar(painter, repaintRegion);
    }

    if (hasBorders() && !s->isAlphaChannelSupported()) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(c->isActive() ? c->color(ColorGroup::Active, ColorRole::TitleBar) : c->color(ColorGroup::Inactive, ColorRole::Foreground));

        painter->drawRect(rect().adjusted(0, 0, -1, -1));
        painter->restore();
    }

    m_painting = false;
}

void Decoration::calculateWindowAndTitleBarShapes(const bool windowShapeOnly)
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);
    auto s = settings();

    if (!windowShapeOnly || c->isShaded()) {
        // set titleBar geometry and path
        m_titleRect = QRect(QPoint(0, 0), QSize(size().width(), borderTop()));
        m_titleBarPath.clear(); // clear the path for subsequent calls to this function
        if (isMaximized() || !s->isAlphaChannelSupported()) {
            m_titleBarPath.addRect(m_titleRect);
        } else if (c->isShaded()) {
            m_titleBarPath.addRoundedRect(m_titleRect, m_scaledCornerRadius, m_scaledCornerRadius);
        } else {
            m_titleBarPath = GeometryTools::roundedPath(m_titleRect, CornersTop, m_scaledCornerRadius);
        }
    }

    // set windowPath
    m_windowPath.clear(); // clear the path for subsequent calls to this function
    if (!c->isShaded()) {
        if (s->isAlphaChannelSupported() && !isMaximized()) {
            if (hasNoBorders() && !m_internalSettings->roundBottomCornersWhenNoBorders()) { // round at top, square at bottom
                m_windowPath = GeometryTools::roundedPath(rect(), CornersTop, m_scaledCornerRadius);
            } else {
                m_windowPath.addRoundedRect(rect(), m_scaledCornerRadius, m_scaledCornerRadius);
            }
        } else // maximized / no alpha
            m_windowPath.addRect(rect());

    } else { // shaded
        m_windowPath = m_titleBarPath;
    }
}

//________________________________________________________________
void Decoration::paintTitleBar(QPainter *painter, const QRect &repaintRegion)
{
    const auto c = client().toStrongRef();
    Q_ASSERT(c);

    if (!m_titleRect.intersects(repaintRegion)) {
        return;
    }

    painter->save();
    painter->setPen(Qt::NoPen);

    QColor titleBarColor(this->titleBarColor());

    // render a linear gradient on title area
    if (c->isActive() && m_internalSettings->drawBackgroundGradient()) {
        QLinearGradient gradient(0, 0, 0, m_titleRect.height());
        gradient.setColorAt(0.0, titleBarColor.lighter(120));
        gradient.setColorAt(0.8, titleBarColor);
        painter->setBrush(gradient);

    } else {
        painter->setBrush(titleBarColor);
    }

    auto s = settings();

    painter->drawPath(m_titleBarPath);

    // draw titlebar separator
    const QColor titleBarSeparatorColor(this->titleBarSeparatorColor());
    int separatorHeight;
    if ((separatorHeight = titleBarSeparatorHeight()) && titleBarSeparatorColor.isValid()) {
        // outline
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setBrush(Qt::NoBrush);
        QPen p(titleBarSeparatorColor);
        p.setWidthF(qRound(devicePixelRatio(painter)));
        p.setCosmetic(true);
        p.setCapStyle(Qt::FlatCap);
        painter->setPen(p);

        QRectF titleRectF(m_titleRect); // use a QRectF because QRects have quirks when getting their corner positions
        qreal separatorYCoOrd = qreal(titleRectF.bottom()) - qreal(separatorHeight) / 2;
        if (m_internalSettings->useTitleBarColorForAllBorders()) {
            painter->drawLine(QPointF(titleRectF.bottomLeft().x() + borderLeft(), separatorYCoOrd),
                              QPointF(titleRectF.bottomRight().x() - borderRight(), separatorYCoOrd));
        } else {
            painter->drawLine(QPointF(titleRectF.bottomLeft().x(), separatorYCoOrd), QPointF(titleRectF.bottomRight().x(), separatorYCoOrd));
        }
    }

    painter->restore();

    // draw caption
    painter->setFont(s->font());
    painter->setPen(fontColor());
    const auto cR = captionRect();
    const QString caption = painter->fontMetrics().elidedText(c->caption(), Qt::ElideMiddle, cR.first.width());
    painter->drawText(cR.first, cR.second | Qt::TextSingleLine, caption);

    // draw all buttons
    m_leftButtons->paint(painter, repaintRegion);
    m_rightButtons->paint(painter, repaintRegion);
}

// outputs the icon height + padding to make a small button, the actual icon height, and the background height to make a small button
void Decoration::calculateButtonHeights()
{
    int baseSize = settings()->gridUnit(); // 10 on Wayland
    int basePaddingSize = settings()->smallSpacing(); // 2 on Wayland

    if (m_tabletMode)
        baseSize *= 2;

    if (m_internalSettings->buttonIconStyle() == InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme) {
        switch (m_internalSettings->systemIconSize()) {
        case InternalSettings::EnumSystemIconSize::SystemIcon8: // 10, 8 on Wayland
            break;
        case InternalSettings::EnumSystemIconSize::SystemIcon12: // 14, 12 on Wayland
            baseSize *= 1.4;
            break;
        case InternalSettings::EnumSystemIconSize::SystemIcon14: // 16, 14 on Wayland
            baseSize *= 1.6;
            break;
        default:
        case InternalSettings::EnumSystemIconSize::SystemIcon16: // 18, 16 on Wayland
            baseSize *= 1.8;
            break;
        case InternalSettings::EnumSystemIconSize::SystemIcon18: // 20, 18 on Wayland
            baseSize *= 2;
            break;
        case InternalSettings::EnumSystemIconSize::SystemIcon20: // 22, 20 on Wayland
            baseSize *= 2.2;
            break;
        case InternalSettings::EnumSystemIconSize::SystemIcon22: // 24, 22 on Wayland
            baseSize *= 2.4;
            break;
        case InternalSettings::EnumSystemIconSize::SystemIcon24: // 26, 24 on Wayland
            baseSize *= 2.6;
            break;
        case InternalSettings::EnumSystemIconSize::SystemIcon32: // 36, 32 on Wayland
            baseSize *= 3.6;
            basePaddingSize *= 2;
            break;
        case InternalSettings::EnumSystemIconSize::SystemIcon48: // 52, 48 on Wayland
            baseSize *= 5.2;
            basePaddingSize *= 2;
            break;
        }
    } else {
        switch (m_internalSettings->iconSize()) {
        case InternalSettings::EnumIconSize::IconTiny: // 10, 8 on Wayland
            break;
        case InternalSettings::EnumIconSize::IconVerySmall: // 14, 12 on Wayland
            baseSize *= 1.4;
            break;
        case InternalSettings::EnumIconSize::IconSmall: // 16, 14 on Wayland
            baseSize *= 1.6;
            break;
        case InternalSettings::EnumIconSize::IconSmallMedium: // 18, 16 on Wayland
            baseSize *= 1.8;
            break;
        default:
        case InternalSettings::EnumIconSize::IconMedium: // 20, 18 on Wayland
            baseSize *= 2;
            break;
        case InternalSettings::EnumIconSize::IconLargeMedium: // 22, 20 on Wayland
            baseSize *= 2.2;
            break;
        case InternalSettings::EnumIconSize::IconLarge: // 24, 22 on Wayland
            baseSize *= 2.4;
            break;
        case InternalSettings::EnumIconSize::IconVeryLarge: // 26, 24 on Wayland
            baseSize *= 2.6;
            break;
        case InternalSettings::EnumIconSize::IconGiant: // 36, 32 on Wayland
            baseSize *= 3.6;
            basePaddingSize *= 2;
            break;
        case InternalSettings::EnumIconSize::IconHumongous: // 52, 48 on Wayland
            baseSize *= 5.2;
            basePaddingSize *= 2;
            break;
        }
    }

    m_smallButtonPaddedHeight = baseSize;
    m_iconHeight = baseSize - basePaddingSize;

    if (m_buttonBackgroundType == ButtonBackgroundType::Small) {
        qreal smallBackgroundScaleFactor = qreal(m_internalSettings->scaleBackgroundPercent()) / 100;

        m_smallButtonPaddedHeight = qRound(m_smallButtonPaddedHeight * smallBackgroundScaleFactor);

        m_smallButtonBackgroundHeight = qRound(m_iconHeight * smallBackgroundScaleFactor);
    }

    if (m_iconHeight % 2 == 1) // if an odd value make even
        m_iconHeight += 1;

    if (m_smallButtonPaddedHeight % 2 == 1) // if an odd value make even
        m_smallButtonPaddedHeight += 1;

    if (m_smallButtonBackgroundHeight % 2 == 1) // if an odd value make even
        m_smallButtonBackgroundHeight += 1;
}

void Decoration::onTabletModeChanged(bool mode)
{
    m_tabletMode = mode;
    calculateButtonHeights();
    recalculateBorders();
    updateButtonsGeometry();
}

//________________________________________________________________
int Decoration::captionHeight() const
{
    return hideTitleBar() ? borderTop() : borderTop() - m_scaledTitleBarTopMargin - m_scaledTitleBarBottomMargin - titleBarSeparatorHeight();
}

//________________________________________________________________
QPair<QRect, Qt::Alignment> Decoration::captionRect() const
{
    if (hideTitleBar()) {
        return qMakePair(QRect(), Qt::AlignCenter);
    } else {
        auto c = client().toStrongRef();
        Q_ASSERT(c);

        int padding = m_internalSettings->titleSidePadding() * settings()->smallSpacing();

        const int leftOffset = m_leftButtons->buttons().isEmpty() ? padding : m_leftButtons->geometry().x() + m_leftButtons->geometry().width() + padding;

        const int rightOffset = m_rightButtons->buttons().isEmpty() ? padding : size().width() - m_rightButtons->geometry().x() + padding;

        const int yOffset = m_scaledTitleBarTopMargin;
        const QRect maxRect(leftOffset, yOffset, size().width() - leftOffset - rightOffset, captionHeight());

        switch (m_internalSettings->titleAlignment()) {
        case InternalSettings::EnumTitleAlignment::AlignLeft:
            return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignLeft);

        case InternalSettings::EnumTitleAlignment::AlignRight:
            return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignRight);

        case InternalSettings::EnumTitleAlignment::AlignCenter:
            return qMakePair(maxRect, Qt::AlignCenter);

        default:
        case InternalSettings::EnumTitleAlignment::AlignCenterFullWidth: {
            // full caption rect
            const QRect fullRect = QRect(0, yOffset, size().width(), captionHeight());
            QRect boundingRect(settings()->fontMetrics().boundingRect(c->caption()).toRect());

            // text bounding rect
            boundingRect.setTop(yOffset);
            boundingRect.setHeight(captionHeight());
            boundingRect.moveLeft((size().width() - boundingRect.width()) / 2);

            if (boundingRect.left() < leftOffset) {
                return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignLeft);
            } else if (boundingRect.right() > size().width() - rightOffset) {
                return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignRight);
            } else {
                return qMakePair(fullRect, Qt::AlignCenter);
            }
        }
        }
    }
}

//________________________________________________________________
void Decoration::updateShadow(const bool forceUpdateCache, bool noCache, const bool isThinWindowOutlineOverride)
{
    auto s = settings();
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    // if the decoration is painting, abandon setting the shadow.
    // Setting the shadow at the same time as paint() being executed causes a EGL_BAD_SURFACE error and a SEGFAULT from Plasma 5.26 onwards.
    if (m_painting) {
        qWarning("Klassy: paint() occurring at same time as shadow creation for \"%s\" - abandoning setting shadow to prevent EGL_BAD_SURFACE.",
                 c->caption().toLatin1().data());
        return;
    }

    // The preset exception may modify the shadow, so in this case there is a "noCache" property set - we don't want to cache the exception shadow as it may
    // corrupt the shadow cache for normal non-exception decoration windows For shaded windows the shadow/outline has a potentially different shape so do not
    // use the shadow cache when shaded
    if (m_internalSettings->property("noCacheException").toBool() || c->isShaded()) {
        noCache = true;
    }

    // Animated case, no cached shadow object
    if ((m_shadowAnimation->state() == QAbstractAnimation::Running) && (m_shadowOpacity != 0.0) && (m_shadowOpacity != 1.0)) {
        QColor shadowColor = KColorUtils::mix(m_decorationColors->inactive()->shadow, m_decorationColors->active()->shadow, m_shadowOpacity);
        setThinWindowOutlineColor();
        setShadow(createShadowObject(shadowColor, isThinWindowOutlineOverride));
        return;
    }
    setThinWindowOutlineColor();

    // TODO: Potentially make the kdecoration configwidget more intelligent and send a dbus signal which is aware of whether to update the shadow or not, so
    // there is less processing here
    // check if cached settings have changed, if so replace them with new settings values and regenerate the shadow cache
    if (!noCache
        && (

            forceUpdateCache || g_shadowSizeEnum != m_internalSettings->shadowSize() || g_shadowStrength != m_internalSettings->shadowStrength()
            || g_shadowColor != m_internalSettings->shadowColor() || !(qAbs(g_cornerRadius - m_scaledCornerRadius) < 0.001)
            || !(qAbs(g_systemScaleFactor - m_systemScaleFactorX11) < 0.001) || g_hasNoBorders != hasNoBorders()
            || g_roundBottomCornersWhenNoBorders != m_internalSettings->roundBottomCornersWhenNoBorders()
            || g_thinWindowOutlineStyleActive != m_internalSettings->thinWindowOutlineStyle(true)
            || g_thinWindowOutlineStyleInactive != m_internalSettings->thinWindowOutlineStyle(false)
            || (c->isActive() ? g_thinWindowOutlineColorActive != m_thinWindowOutline : g_thinWindowOutlineColorInactive != m_thinWindowOutline)
            || g_thinWindowOutlineThickness != m_internalSettings->thinWindowOutlineThickness())) {
        g_sShadow.clear();
        g_sShadowInactive.clear();
        g_shadowSizeEnum = m_internalSettings->shadowSize();
        g_shadowStrength = m_internalSettings->shadowStrength();
        g_shadowColor = m_internalSettings->shadowColor();
        g_cornerRadius = m_scaledCornerRadius;
        g_systemScaleFactor = m_systemScaleFactorX11;
        g_hasNoBorders = hasNoBorders();
        g_roundBottomCornersWhenNoBorders = m_internalSettings->roundBottomCornersWhenNoBorders();
        g_thinWindowOutlineStyleActive = m_internalSettings->thinWindowOutlineStyle(true);
        g_thinWindowOutlineStyleInactive = m_internalSettings->thinWindowOutlineStyle(false);
        c->isActive() ? g_thinWindowOutlineColorActive = m_thinWindowOutline : g_thinWindowOutlineColorInactive = m_thinWindowOutline;
        g_thinWindowOutlineThickness = m_internalSettings->thinWindowOutlineThickness();
    }

    QSharedPointer<KDecoration2::DecorationShadow> nonCachedShadow;
    QSharedPointer<KDecoration2::DecorationShadow> *shadow = nullptr;

    if (noCache)
        shadow = &nonCachedShadow;
    else // use the already cached shadow
        shadow = (c->isActive()) ? &g_sShadow : &g_sShadowInactive;

    if (!(*shadow)) { // only recreate the shadow if necessary
        QColor shadowColor = c->isActive() ? m_decorationColors->active()->shadow : m_decorationColors->inactive()->shadow;
        *shadow = createShadowObject(shadowColor, isThinWindowOutlineOverride);
    }

    setShadow(*shadow);
}

//________________________________________________________________
QSharedPointer<KDecoration2::DecorationShadow> Decoration::createShadowObject(QColor shadowColor, const bool isThinWindowOutlineOverride)
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    // determine when a window outline does not need to be drawn (even when set to none, sometimes needs to be drawn if there is an animation)
    bool windowOutlineNone =
        ((m_internalSettings->thinWindowOutlineStyle(true) == InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineNone
          && m_internalSettings->thinWindowOutlineStyle(false) == InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineNone)
         || (m_animation->state() != QAbstractAnimation::Running
             && ((c->isActive() && m_internalSettings->thinWindowOutlineStyle(true) == InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineNone)
                 || (!c->isActive() && m_internalSettings->thinWindowOutlineStyle(false) == InternalSettings::EnumThinWindowOutlineStyle::WindowOutlineNone))));

    if (m_internalSettings->shadowSize() == InternalSettings::EnumShadowSize::ShadowNone && windowOutlineNone && !isThinWindowOutlineOverride) {
        return nullptr;
    }

    const CompositeShadowParams params = lookupShadowParams(m_internalSettings->shadowSize());

    const QSize boxSize =
        BoxShadowRenderer::calculateMinimumBoxSize(params.shadow1.radius).expandedTo(BoxShadowRenderer::calculateMinimumBoxSize(params.shadow2.radius));

    BoxShadowRenderer shadowRenderer;

    shadowRenderer.setBorderRadius(m_scaledCornerRadius + 0.5);
    shadowRenderer.setBoxSize(boxSize);
    shadowRenderer.addShadow(params.shadow1.offset, params.shadow1.radius, ColorTools::alphaMix(shadowColor, params.shadow1.opacity));
    shadowRenderer.addShadow(params.shadow2.offset, params.shadow2.radius, ColorTools::alphaMix(shadowColor, params.shadow2.opacity));

    QImage shadowTexture = shadowRenderer.render();

    QPainter painter(&shadowTexture);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect outerRect = shadowTexture.rect();

    QRect boxRect(QPoint(0, 0), boxSize);
    boxRect.moveCenter(outerRect.center());

    // Mask out inner rect.
    const QMargins padding = QMargins(boxRect.left() - outerRect.left() - Metrics::Decoration_Shadow_Overlap - params.offset.x(),
                                      boxRect.top() - outerRect.top() - Metrics::Decoration_Shadow_Overlap - params.offset.y(),
                                      outerRect.right() - boxRect.right() - Metrics::Decoration_Shadow_Overlap + params.offset.x(),
                                      outerRect.bottom() - boxRect.bottom() - Metrics::Decoration_Shadow_Overlap + params.offset.y());
    const QRectF innerRect = outerRect - padding;

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);

    QPainterPath roundedRectMask;
    if (hasNoBorders() && !m_internalSettings->roundBottomCornersWhenNoBorders() && !c->isShaded()) {
        roundedRectMask = GeometryTools::roundedPath(innerRect, CornersTop, m_scaledCornerRadius + 0.5);
    } else {
        roundedRectMask.addRoundedRect(innerRect, m_scaledCornerRadius + 0.5, m_scaledCornerRadius + 0.5);
    }

    painter.drawPath(roundedRectMask);

    // Draw Thin window outline
    if (!windowOutlineNone || isThinWindowOutlineOverride) {
        if (m_thinWindowOutline.isValid()) {
            QPen p;
            p.setColor(m_thinWindowOutline);
            // use a miter join rather than the default bevel join to get sharp corners at low radii
            if (m_internalSettings->cornerRadius() < 0.2)
                p.setJoinStyle(Qt::MiterJoin);

            qreal outlinePenWidth = m_internalSettings->thinWindowOutlineThickness();

            // the overlap between the thin window outline and behind the window in unscaled pixels.
            // This is necessary for the thin window outline to sit flush with the window on Wayland,
            // and also makes sure that the anti-aliasing blends properly between the window and thin window outline
            qreal outlineOverlap = 0.5;

            // scale outline
            // We can't get the DPR for Wayland from KDecoration/KWin but can work around this as Wayland will auto-scale if you don't use a cosmetic pen. On
            // X11 this does not happen but we can use the system-set scaling value directly.
            if (KWindowSystem::isPlatformX11()) {
                outlinePenWidth *= m_systemScaleFactorX11;
                outlineOverlap *= m_systemScaleFactorX11;
            }

            qreal outlineAdjustment = outlinePenWidth / 2 - outlineOverlap;
            QRectF outlineRect;
            outlineRect =
                innerRect.adjusted(-outlineAdjustment,
                                   -outlineAdjustment,
                                   outlineAdjustment,
                                   outlineAdjustment); // make thin window outline rect larger so most is outside the window, except for a 0.5px scaled overlap

            p.setWidthF(outlinePenWidth);
            painter.setPen(p);
            painter.setBrush(Qt::NoBrush);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

            QPainterPath outlinePath;
            qreal cornerRadius;

            if (m_internalSettings->cornerRadius() < 0.2)
                cornerRadius = m_scaledCornerRadius; // give a square corner for when corner radius is 0
            else
                cornerRadius = m_scaledCornerRadius + outlineAdjustment; // else round corner slightly more to account for pen width

            if (hasNoBorders() && !m_internalSettings->roundBottomCornersWhenNoBorders() && !c->isShaded()) {
                outlinePath = GeometryTools::roundedPath(outlineRect, CornersTop, cornerRadius);
            } else {
                outlinePath.addRoundedRect(outlineRect, cornerRadius, cornerRadius);
            }

            painter.drawPath(outlinePath);
        }
    }
    painter.end();

    auto ret = QSharedPointer<KDecoration2::DecorationShadow>::create();
    ret->setPadding(padding);
    ret->setInnerShadowRect(QRect(outerRect.center(), QSize(1, 1)));
    ret->setShadow(shadowTexture);
    return ret;
}

void Decoration::setThinWindowOutlineOverrideColor(const bool on, const QColor &color)
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    if (on) {
        if (!c->isMaximized()) {
            // draw a thin window outline with this override colour
            m_thinWindowOutlineOverride = color;
            updateOverrideOutlineFromButtonAnimationState();
        }
    } else {
        if (!c->isMaximized()) {
            // reset the thin window outline
            m_thinWindowOutlineOverride = QColor();
            m_animateOutOverriddenThinWindowOutline = true;
            updateOverrideOutlineFromButtonAnimationState();
        }
    }
}

void Decoration::setThinWindowOutlineColor()
{
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    if (m_thinWindowOutlineOverride.isValid()) {
        m_thinWindowOutline = overriddenOutlineColorAnimateIn();
    } else { // normal case, not an override

        QColor thinWindowOutlineActiveFinal = m_decorationColors->active()->windowOutline;
        QColor thinWindowOutlineInactiveFinal = m_decorationColors->inactive()->windowOutline;
        ;

        // get blended colour if animated
        if (m_animation->state() == QAbstractAnimation::Running) {
            // deal with animation cases where there is an invalid colour (WindowOutlineNone)
            if (!(thinWindowOutlineActiveFinal.isValid() && thinWindowOutlineInactiveFinal.isValid())) {
                if (!thinWindowOutlineInactiveFinal.isValid() && thinWindowOutlineActiveFinal.isValid()) {
                    m_thinWindowOutline = ColorTools::alphaMix(thinWindowOutlineActiveFinal, m_opacity);
                } else if (thinWindowOutlineInactiveFinal.isValid() && !thinWindowOutlineActiveFinal.isValid()) {
                    m_thinWindowOutline = ColorTools::alphaMix(thinWindowOutlineInactiveFinal, (1.0 - m_opacity));
                }
            } else { // standard animated case with both valid colours
                m_thinWindowOutline = KColorUtils::mix(thinWindowOutlineInactiveFinal, thinWindowOutlineActiveFinal, m_opacity);
            }
        } else { // normal non-animated final colour
            m_thinWindowOutline = c->isActive() ? thinWindowOutlineActiveFinal : thinWindowOutlineInactiveFinal;
        }
    }

    // deal with override colours ("Colourize with highlighted button's colour")
    if (m_animateOutOverriddenThinWindowOutline)
        m_thinWindowOutline = overriddenOutlineColorAnimateOut(m_thinWindowOutline);

    // the existing thin window outline colour is stored in-case it is overridden in the future and needed by an animation
    if (!m_thinWindowOutlineOverride.isValid()) { // non-override
        c->isActive() ? m_originalThinWindowOutlineActivePreOverride = m_thinWindowOutline
                      : m_originalThinWindowOutlineInactivePreOverride = m_thinWindowOutline;
    } else if ((m_overrideOutlineFromButtonAnimation->state() == QAbstractAnimation::Running) && m_overrideOutlineAnimationProgress == 1) {
        // only buffer the override colour once it has finished animating -- used for the override out animation, and when mouse moves from one overrride
        // colour to another
        c->isActive() ? m_originalThinWindowOutlineActivePreOverride = m_thinWindowOutline
                      : m_originalThinWindowOutlineInactivePreOverride = m_thinWindowOutline;
    }
}

void Decoration::setScaledTitleBarTopBottomMargins()
{
    // access client
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    qreal topMargin = m_internalSettings->titleBarTopMargin();
    qreal bottomMargin = m_internalSettings->titleBarBottomMargin();
    if (m_internalSettings->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangle
        || m_internalSettings->buttonShape() == InternalSettings::EnumButtonShape::ShapeIntegratedRoundedRectangleGrouped) {
        m_scaledIntegratedRoundedRectangleBottomPadding = m_internalSettings->integratedRoundedRectangleBottomPadding() * settings()->smallSpacing();
    } else {
        m_scaledIntegratedRoundedRectangleBottomPadding = 0;
    }
    if (c->isMaximized()) {
        qreal maximizedScaleFactor = qreal(m_internalSettings->percentMaximizedTopBottomMargins()) / 100;
        topMargin *= maximizedScaleFactor;
        bottomMargin *= maximizedScaleFactor;
    }

    m_scaledTitleBarTopMargin = qRound(settings()->smallSpacing() * topMargin);
    m_scaledTitleBarBottomMargin = qRound(settings()->smallSpacing() * bottomMargin);
}

void Decoration::setScaledTitleBarSideMargins()
{
    m_scaledTitleBarLeftMargin = int(qreal(m_internalSettings->titleBarLeftMargin()) * qreal(settings()->smallSpacing()));
    m_scaledTitleBarRightMargin = int(qreal(m_internalSettings->titleBarRightMargin()) * qreal(settings()->smallSpacing()));

    // subtract any added borders from the side margin so the user doesn't need to adjust the side margins when changing border size
    // this makes the side margin relative to the border edge rather than the titlebar edge
    if (!isMaximizedHorizontally()) {
        int borderSize = this->borderSize(false);
        m_scaledTitleBarLeftMargin -= borderSize;
        m_scaledTitleBarRightMargin -= borderSize;
    }
}

void Decoration::setScaledCornerRadius()
{
    m_scaledCornerRadius = m_internalSettings->cornerRadius() * settings()->smallSpacing();
}

void Decoration::updateOpaque()
{
    // access client
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    if (isOpaqueTitleBar()) { // opaque titlebar colours
        if (c->isMaximized())
            setOpaque(true);
        else
            setOpaque(false);
    } else { // transparent titlebar colours
        setOpaque(false);
    }
}

void Decoration::updateBlur()
{
    // disable blur if the titlebar is opaque
    if (isOpaqueTitleBar()) { // opaque titlebar colours
        setBlurRegion(QRegion());
    } else { // transparent titlebar colours
        if (m_internalSettings->blurTransparentTitleBars()) { // enable blur
            calculateWindowAndTitleBarShapes(true); // refreshes m_windowPath
            setBlurRegion(QRegion(m_windowPath.toFillPolygon().toPolygon()));
        } else
            setBlurRegion(QRegion());
    }
}

bool Decoration::isOpaqueTitleBar()
{
    return (titleBarColor(true).alpha() == 255);
}

int Decoration::titleBarSeparatorHeight() const
{
    // access client
    auto c = client().toStrongRef();
    Q_ASSERT(c);

    if (m_internalSettings->drawTitleBarSeparator() && !c->isShaded() && !m_toolsAreaWillBeDrawn) {
        qreal height = 1;
        if (KWindowSystem::isPlatformX11())
            height = m_systemScaleFactorX11;
        return qRound(height);
    } else
        return 0;
}

qreal Decoration::devicePixelRatio(QPainter *painter) const
{
    // determine DPR
    qreal dpr = painter->device()->devicePixelRatioF();

    // on X11 Kwin just returns 1.0 for the DPR instead of the correct value, so use the scaling setting directly
    if (KWindowSystem::isPlatformX11())
        dpr = systemScaleFactorX11();
    return dpr;
}

} // namespace

#include "breezedecoration.moc"
