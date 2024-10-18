/* SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2016 The Qt Company Ltd.
 * SPDX-FileCopyrightText: 2021 Noah Davis <noahadvs@gmail.com>
 * SPDX-FileCopyrightText: 2023 ivan tkachenko <me@ratijas.tk>
 * SPDX-FileCopyrightText: 2021-2024 Paul A McAuley <kde@paulmcauley.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "breezestyle.h"

#include "breezeanimations.h"
#include "breezeblurhelper.h"
#include "breezeframeshadow.h"
#include "breezemdiwindowshadow.h"
#include "breezemetrics.h"
#include "breezemnemonics.h"
#include "breezepropertynames.h"
#include "breezeshadowhelper.h"
#include "breezesplitterproxy.h"
#include "breezestyleconfigdata.h"
#include "breezetoolsareamanager.h"
#include "breezewidgetexplorer.h"
#include "breezewindowmanager.h"
#include "decorationcolors.h"

#include <KColorUtils>
#include <KIconLoader>
#include <KWindowEffects>
#include <kguiaddons_version.h>

#include <QApplication>
#include <QBitmap>
#include <QCheckBox>
#include <QComboBox>
#include <QDial>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGraphicsProxyWidget>
#include <QGraphicsView>
#include <QGroupBox>
#include <QItemDelegate>
#include <QLineEdit>
#include <QMainWindow>
#include <QMdiArea>
#include <QMenu>
#include <QMetaEnum>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSplitterHandle>
#include <QStackedLayout>
#include <QTextEdit>
#include <QToolBar>
#include <QToolBox>
#include <QToolButton>
#include <QTreeView>
#include <QWidgetAction>
#include <memory>

#if HAVE_QTDBUS
#include "dbusupdatenotifier.h"
#include <QDBusConnection>
#endif

#if BREEZE_HAVE_QTQUICK
#include <KCoreAddons>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <Kirigami/Platform/TabletModeWatcher>
using TabletModeWatcher = Kirigami::Platform::TabletModeWatcher;
#else
#if __has_include(<Kirigami/TabletModeWatcher>)
// the namespaced include is new in KF 5.91
#include <Kirigami/TabletModeWatcher>
#else
#include <TabletModeWatcher>
#endif
using TabletModeWatcher = Kirigami::TabletModeWatcher;
#endif
#include <QQuickWindow>
#endif

#include "breeze_logging.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
constexpr auto HighlightColor = QPalette::Accent;
#else
constexpr auto HighlightColor = QPalette::Highlight;
#endif

namespace BreezePrivate
{

class PainterStateSaver
{
public:
    PainterStateSaver(QPainter *painter)
        : m_painter(painter)
    {
        m_painter->save();
    }

    ~PainterStateSaver()
    {
        m_painter->restore();
    }

private:
    QPainter *const m_painter;
};

// needed to keep track of tabbars when being dragged
class TabBarData : public QObject
{
public:
    //* constructor
    explicit TabBarData()
        : QObject()
    {
    }

    //* assign target tabBar
    void lock(const QWidget *widget)
    {
        _tabBar = widget;
    }

    //* true if tabbar is locked
    bool isLocked(const QWidget *widget) const
    {
        return _tabBar && _tabBar.data() == widget;
    }

    //* release
    void release()
    {
        _tabBar.clear();
    }

private:
    //* pointer to target tabBar
    Breeze::WeakPointer<const QWidget> _tabBar;
};

//* needed to have spacing added to items in combobox
class ComboBoxItemDelegate : public QItemDelegate
{
public:
    //* constructor
    explicit ComboBoxItemDelegate(QAbstractItemView *parent)
        : QItemDelegate(parent)
        , _proxy(parent->itemDelegate())
        , _itemMargin(Breeze::Metrics::ItemView_ItemMarginWidth)
    {
    }

    //* paint
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        painter->setRenderHints(QPainter::Antialiasing);
        // if the app sets an item delegate that isn't the default, use its drawing...
        if (_proxy && _proxy->metaObject()->className() != QStringLiteral("QComboBoxDelegate")) {
            _proxy.data()->paint(painter, option, index);
            return;
        }

        // otherwise we draw the selected/highlighted background ourselves.
        if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
            using namespace Breeze;

            auto c = option.palette.brush((option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled, HighlightColor).color();

            painter->setPen(c);
            c.setAlphaF(c.alphaF() * 0.3);
            painter->setBrush(c);
            auto radius = Metrics::Frame_FrameRadius - (0.5 * PenWidth::Frame);
            painter->drawRoundedRect(QRectF(option.rect).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        }

        // and ask the base class to do everything else for us besides the selected/highlighted part which we just did
        auto opt = option;
        opt.showDecorationSelected = false;
        opt.state &= ~QStyle::State_Selected;

        QItemDelegate::paint(painter, opt, index);
    }

    //* size hint for index
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        // get size from either proxy or parent class
        auto size(_proxy ? _proxy.data()->sizeHint(option, index) : QItemDelegate::sizeHint(option, index));

        // adjust and return
        if (size.isValid()) {
            size.rheight() += _itemMargin * 2;
        }
        return size;
    }

private:
    //* proxy
    Breeze::WeakPointer<QAbstractItemDelegate> _proxy;

    //* margin
    int _itemMargin;
};

//_______________________________________________________________
bool isProgressBarHorizontal(const QStyleOptionProgressBar *option)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return option && (option->state & QStyle::State_Horizontal);
#else
    return option && ((option->state & QStyle::State_Horizontal) || option->orientation == Qt::Horizontal);
#endif
}

enum class ToolButtonMenuArrowStyle {
    None,
    InlineLarge,
    InlineSmall,
    SubControl,
};

ToolButtonMenuArrowStyle toolButtonMenuArrowStyle(const QStyleOption *option)
{
    const auto toolButtonOption = qstyleoption_cast<const QStyleOptionToolButton *>(option);
    if (!toolButtonOption) {
        return ToolButtonMenuArrowStyle::None;
    }

    const bool hasPopupMenu(toolButtonOption->features & QStyleOptionToolButton::HasMenu
                            && toolButtonOption->features & QStyleOptionToolButton::MenuButtonPopup);
    const bool hasInlineIndicator(toolButtonOption->features & QStyleOptionToolButton::HasMenu && !hasPopupMenu);
    const bool hasDelayedMenu(hasInlineIndicator && toolButtonOption->features & QStyleOptionToolButton::PopupDelay);

    const bool hasIcon = !toolButtonOption->icon.isNull() || (toolButtonOption->features & QStyleOptionToolButton::Arrow);
    const bool iconOnly = toolButtonOption->toolButtonStyle == Qt::ToolButtonIconOnly || (toolButtonOption->text.isEmpty() && hasIcon);

    if (hasPopupMenu) {
        return ToolButtonMenuArrowStyle::SubControl;
    }

    if (hasDelayedMenu) {
        return ToolButtonMenuArrowStyle::InlineSmall;
    }

    if (hasInlineIndicator && !iconOnly) {
        return ToolButtonMenuArrowStyle::InlineLarge;
    }

    return ToolButtonMenuArrowStyle::None;
}

}

namespace Breeze
{

//______________________________________________________________
Style::Style()
    : _helper(std::make_shared<Helper>(StyleConfigData::self()->sharedConfig()))
    , _shadowHelper(std::make_unique<ShadowHelper>(_helper))
    , _animations(std::make_unique<Animations>())
    , _mnemonics(std::make_unique<Mnemonics>())
    , _blurHelper(std::make_unique<BlurHelper>(_helper))
    , _windowManager(std::make_unique<WindowManager>())
    , _frameShadowFactory(std::make_unique<FrameShadowFactory>())
    , _mdiWindowShadowFactory(std::make_unique<MdiWindowShadowFactory>())
    , _splitterFactory(std::make_unique<SplitterFactory>())
    , _toolsAreaManager(std::make_unique<ToolsAreaManager>(_helper))
    , _widgetExplorer(std::make_unique<WidgetExplorer>())
    , _tabBarData(std::make_unique<BreezePrivate::TabBarData>())
#if BREEZE_HAVE_KSTYLE
    , SH_ArgbDndWindow(newStyleHint(QStringLiteral("SH_ArgbDndWindow")))
    , CE_CapacityBar(newControlElement(QStringLiteral("CE_CapacityBar")))
#endif
{
#if HAVE_QTDBUS
    // use DBus connection to update on breeze configuration change
    auto dbus = QDBusConnection::sessionBus();
    dbus.connect(QString(),
                 QStringLiteral("/KlassyStyle"),
                 QStringLiteral("org.kde.Klassy.Style"),
                 QStringLiteral("reparseConfiguration"),
                 this,
                 SLOT(configurationChanged()));

    // not needed
    /*dbus.connect(QString(),
                 QStringLiteral("/KlassyDecoration"),
                 QStringLiteral("org.kde.Klassy.Style"),
                 QStringLiteral("reparseConfiguration"),
                 this,
                 SLOT(configurationChanged()));
    */

    dbus.connect(QString(),
                 QStringLiteral("/KGlobalSettings"),
                 QStringLiteral("org.kde.KGlobalSettings"),
                 QStringLiteral("notifyChange"),
                 this,
                 SLOT(configurationChanged()));

    connect(&g_dBusUpdateNotifier, &DBusUpdateNotifier::decorationSettingsUpdate, this, &Style::generateDecorationColorsOnDecorationColorSettingsUpdate);

    // dbus.connect(QString(), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reloadConfig"), this, SLOT(configurationChanged()));
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    qApp->installEventFilter(this);
#else
    connect(qApp, &QApplication::paletteChanged, this, &Style::loadConfiguration);
#endif

    // call the slot directly; this initial call will set up things that also
    // need to be reset when the system palette changes
    loadConfiguration();

#if HAVE_QTDBUS
    connect(&g_dBusUpdateNotifier,
            &DBusUpdateNotifier::systemIconsUpdate,
            this,
            [this]() { // call this after loadConfiguration() as _helper->decorationConfig() needs to be initialized properly first
                if (_helper->decorationConfig()->buttonIconStyle() == InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme) {
                    loadConfiguration();
                }
            });
#endif
}

//______________________________________________________________
Style::~Style()
{
}

//______________________________________________________________
void Style::polish(QWidget *widget)
{
    if (!widget) {
        return;
    }

    // register widget to animations
    _animations->registerWidget(widget);
    _windowManager->registerWidget(widget);
    _frameShadowFactory->registerWidget(widget, _helper);
    _mdiWindowShadowFactory->registerWidget(widget);
    _shadowHelper->registerWidget(widget);
    _splitterFactory->registerWidget(widget);
    _toolsAreaManager->registerWidget(widget);

    // enable mouse over effects for all necessary widgets
    if (qobject_cast<QAbstractItemView *>(widget) || qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QCheckBox *>(widget)
        || qobject_cast<QComboBox *>(widget) || qobject_cast<QDial *>(widget) || qobject_cast<QLineEdit *>(widget) || qobject_cast<QPushButton *>(widget)
        || qobject_cast<QRadioButton *>(widget) || qobject_cast<QScrollBar *>(widget) || qobject_cast<QSlider *>(widget)
        || qobject_cast<QSplitterHandle *>(widget) || qobject_cast<QTabBar *>(widget) || qobject_cast<QTextEdit *>(widget)
        || qobject_cast<QToolButton *>(widget) || widget->inherits("KTextEditor::View")) {
        widget->setAttribute(Qt::WA_Hover);
    }

    // enforce translucency for drag and drop window
    if (widget->testAttribute(Qt::WA_X11NetWmWindowTypeDND) && _helper->compositingActive()) {
        widget->setAttribute(Qt::WA_TranslucentBackground);
        widget->clearMask();
    }

    // scrollarea polishing is somewhat complex. It is moved to a dedicated method
    polishScrollArea(qobject_cast<QAbstractScrollArea *>(widget));

    if (auto itemView = qobject_cast<QAbstractItemView *>(widget)) {
        // enable mouse over effects in the viewport of the itemview
        itemView->viewport()->setAttribute(Qt::WA_Hover);

    } else if (auto groupBox = qobject_cast<QGroupBox *>(widget)) {
        // checkable group boxes
        if (groupBox->isCheckable()) {
            groupBox->setAttribute(Qt::WA_Hover);
        }

    } else if (qobject_cast<QAbstractButton *>(widget) && qobject_cast<QDockWidget *>(widget->parent())) {
        widget->setAttribute(Qt::WA_Hover);

    } else if (qobject_cast<QAbstractButton *>(widget) && qobject_cast<QToolBox *>(widget->parent())) {
        widget->setAttribute(Qt::WA_Hover);
#if KGUIADDONS_VERSION < QT_VERSION_CHECK(6, 4, 0)
    } else if (qobject_cast<QFrame *>(widget) && widget->parent() && widget->parent()->inherits("KTitleWidget")) {
        // Using available KGuiAddons version as reference, assuming KF6 modules all same version
        // With KWidgetsAddons >= 6.4 the child QFrame is gone and all children default to sutoFillBackground == false.
        widget->setAutoFillBackground(false);
#endif
    }

    if (qobject_cast<QScrollBar *>(widget)) {
        // remove opaque painting for scrollbars
        widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
        addEventFilter(widget);

    } else if (widget->parent() && widget->parent()->inherits("QComboBoxListView")) {
        widget->setAutoFillBackground(false);

    } else if (widget->inherits("KTextEditor::View")) {
        addEventFilter(widget);

    } else if (auto toolButton = qobject_cast<QToolButton *>(widget)) {
        if (toolButton->autoRaise()) {
            // for flat toolbuttons, adjust foreground and background role accordingly
            widget->setBackgroundRole(QPalette::NoRole);
            widget->setForegroundRole(QPalette::WindowText);
        }

        if (widget->parentWidget() && widget->parentWidget()->parentWidget() && widget->parentWidget()->parentWidget()->inherits("Gwenview::SideBarGroup")) {
            widget->setProperty(PropertyNames::toolButtonAlignment, Qt::AlignLeft);
        }

    } else if (qobject_cast<QDockWidget *>(widget)) {
        // add event filter on dock widgets
        // and alter palette
        widget->setAutoFillBackground(false);
        widget->setContentsMargins({});
        addEventFilter(widget);

    } else if (qobject_cast<QMdiSubWindow *>(widget)) {
        widget->setAutoFillBackground(false);
        addEventFilter(widget);

    } else if (qobject_cast<QToolBox *>(widget)) {
        widget->setBackgroundRole(QPalette::NoRole);
        widget->setAutoFillBackground(false);

    } else if (widget->parentWidget() && widget->parentWidget()->parentWidget()
               && qobject_cast<QToolBox *>(widget->parentWidget()->parentWidget()->parentWidget())) {
        widget->setBackgroundRole(QPalette::NoRole);
        widget->setAutoFillBackground(false);
        widget->parentWidget()->setAutoFillBackground(false);

    } else if (qobject_cast<QMenu *>(widget)) {
        setTranslucentBackground(widget);

        if (_helper->hasAlphaChannel(widget) && StyleConfigData::menuOpacity() < 100) {
            _blurHelper->registerWidget(widget->window());
        }

    } else if (qobject_cast<QCommandLinkButton *>(widget)) {
        addEventFilter(widget);

    } else if (auto comboBox = qobject_cast<QComboBox *>(widget)) {
        if (!hasParent(widget, "QWebView")) {
            auto itemView(comboBox->view());
            if (itemView && itemView->itemDelegate() && itemView->itemDelegate()->inherits("QComboBoxDelegate")) {
                itemView->setItemDelegate(new BreezePrivate::ComboBoxItemDelegate(itemView));
            }
        }

    } else if (widget->inherits("QComboBoxPrivateContainer")) {
        addEventFilter(widget);
        setTranslucentBackground(widget);

    } else if (widget->inherits("QTipLabel")) {
        setTranslucentBackground(widget);

    } else if (widget->inherits("KMultiTabBar")) {
        enum class Position {
            Left,
            Right,
            Top,
            Bottom,
        };

        const Position position = static_cast<Position>(widget->property("position").toInt());
        const auto splitterWidth = Metrics::Splitter_SplitterWidth;

        int left = 0, right = 0;
        if ((position == Position::Left && widget->layoutDirection() == Qt::LeftToRight)
            || (position == Position::Right && widget->layoutDirection() == Qt::RightToLeft)) {
            right += splitterWidth;
        } else if ((position == Position::Right && widget->layoutDirection() == Qt::LeftToRight)
                   || (position == Position::Left && widget->layoutDirection() == Qt::RightToLeft)) {
            left += splitterWidth;
        }
        widget->setContentsMargins(left, splitterWidth, right, splitterWidth);

    } else if (qobject_cast<QMainWindow *>(widget)) {
        widget->setAttribute(Qt::WA_StyledBackground);
        addEventFilter(widget);
    } else if (qobject_cast<QDialog *>(widget)) {
        widget->setAttribute(Qt::WA_StyledBackground);
        addEventFilter(widget);
    } else if (qobject_cast<QDialogButtonBox *>(widget)) {
        addEventFilter(widget);
    } else if (auto pushButton = qobject_cast<QPushButton *>(widget)) {
        QDialog *dialog = nullptr;
        auto p = pushButton->parentWidget();
        while (p && !p->isWindow()) {
            p = p->parentWidget();
            if (auto d = qobject_cast<QDialog *>(p)) {
                dialog = d;
            }
        }
        // Internally, QPushButton::autoDefault can be explicitly on,
        // explicitly off, or automatic (enabled if in a QDialog).
        // If autoDefault is explicitly on and not in a dialog,
        // or on/automatic in a dialog and has a QDialogButtonBox parent,
        // explicitly enable autoDefault, else explicitly disable autoDefault.
        bool autoDefaultNoDialog = pushButton->autoDefault() && !dialog;
        bool autoDefaultInDialog = pushButton->autoDefault() && dialog;
        auto dialogButtonBox = qobject_cast<QDialogButtonBox *>(pushButton->parent());
        pushButton->setAutoDefault(autoDefaultNoDialog || (autoDefaultInDialog && dialogButtonBox));
    }
    if (_toolsAreaManager->hasHeaderColors()) {
        // style TitleWidget and Search KPageView to look the same as KDE System Settings
        if (widget->objectName() == QLatin1String("KPageView::TitleWidget")) {
            widget->setAutoFillBackground(true);
            widget->setPalette(_toolsAreaManager->palette());
            addEventFilter(widget);
        } else if (widget->objectName() == QLatin1String("KPageView::Search")) {
            widget->setBackgroundRole(QPalette::Window);
            widget->setPalette(_toolsAreaManager->palette());
            addEventFilter(widget);
        }
    }

    // base class polishing
    ParentStyleClass::polish(widget);
}

//______________________________________________________________
void Style::polish(QApplication *application)
{
    _toolsAreaManager->registerApplication(application);
}

//______________________________________________________________
void Style::polishScrollArea(QAbstractScrollArea *scrollArea)
{
    // check argument
    if (!scrollArea) {
        return;
    }

    // enable mouse over effect in sunken scrollareas that support focus
    if (scrollArea->frameShadow() == QFrame::Sunken && scrollArea->focusPolicy() & Qt::StrongFocus) {
        scrollArea->setAttribute(Qt::WA_Hover);
    }

    if (scrollArea->viewport() && scrollArea->inherits("KItemListContainer") && scrollArea->frameShape() == QFrame::NoFrame) {
        scrollArea->viewport()->setBackgroundRole(QPalette::Window);
        scrollArea->viewport()->setForegroundRole(QPalette::WindowText);
    }

    // add event filter, to make sure proper background is rendered behind scrollbars
    addEventFilter(scrollArea);

    // force side panels as flat, on option
    if (scrollArea->inherits("KDEPrivate::KPageListView") || scrollArea->inherits("KDEPrivate::KPageTreeView")) {
        scrollArea->setProperty(PropertyNames::sidePanelView, true);
    }

    // for all side view panels, unbold font (design choice)
    if (scrollArea->property(PropertyNames::sidePanelView).toBool()) {
        // upbold list font
        auto font(scrollArea->font());
        font.setBold(false);
        scrollArea->setFont(font);
    }

    // disable autofill background for flat (== NoFrame) scrollareas, with QPalette::Window as a background
    // this fixes flat scrollareas placed in a tinted widget, such as groupboxes, tabwidgets or framed dock-widgets
    if (!(scrollArea->frameShape() == QFrame::NoFrame || scrollArea->backgroundRole() == QPalette::Window)) {
        return;
    }

    // get viewport and check background role
    auto viewport(scrollArea->viewport());
    if (!(viewport && viewport->backgroundRole() == QPalette::Window)) {
        return;
    }

    // change viewport autoFill background.
    // do the same for all children if the background role is QPalette::Window
    viewport->setAutoFillBackground(false);
    const QList<QWidget *> children(viewport->findChildren<QWidget *>());
    for (QWidget *child : children) {
        if (child->parent() == viewport && child->backgroundRole() == QPalette::Window) {
            child->setAutoFillBackground(false);
        }
    }

    /*
    QTreeView animates expanding/collapsing branches. It paints them into a
    temp pixmap whose background is unconditionally filled with the palette's
    *base* color which is usually different from the window's color
    cf. QTreeViewPrivate::renderTreeToPixmapForAnimation()
    */
    if (auto treeView = qobject_cast<QTreeView *>(scrollArea)) {
        if (treeView->isAnimated()) {
            QPalette pal(treeView->palette());
            pal.setColor(QPalette::Active, QPalette::Base, treeView->palette().color(treeView->backgroundRole()));
            treeView->setPalette(pal);
        }
    }
}

//_______________________________________________________________
void Style::unpolish(QWidget *widget)
{
    // register widget to animations
    _animations->unregisterWidget(widget);
    _frameShadowFactory->unregisterWidget(widget);
    _mdiWindowShadowFactory->unregisterWidget(widget);
    _shadowHelper->unregisterWidget(widget);
    _windowManager->unregisterWidget(widget);
    _splitterFactory->unregisterWidget(widget);
    _blurHelper->unregisterWidget(widget);
    _toolsAreaManager->unregisterWidget(widget);

    // remove event filter
    if (qobject_cast<QAbstractScrollArea *>(widget) || qobject_cast<QDockWidget *>(widget) || qobject_cast<QMdiSubWindow *>(widget)
        || widget->inherits("QComboBoxPrivateContainer")) {
        widget->removeEventFilter(this);
    }

    ParentStyleClass::unpolish(widget);
}

/// Find direct parent layout as the parentWidget()->layout() might not be
/// the direct parent layout but just a parent layout.
QLayout *findParentLayout(const QWidget *widget)
{
    if (!widget->parentWidget()) {
        return nullptr;
    }

    auto layout = widget->parentWidget()->layout();
    if (!layout) {
        return nullptr;
    }

    if (layout->indexOf(const_cast<QWidget *>(widget)) > -1) {
        return layout;
    }

    QList<QObject *> children = layout->children();

    while (!children.isEmpty()) {
        layout = qobject_cast<QLayout *>(children.takeFirst());
        if (!layout) {
            continue;
        }

        if (layout->indexOf(const_cast<QWidget *>(widget)) > -1) {
            return layout;
        }
        children += layout->children();
    }

    return nullptr;
}

//______________________________________________________________
int Style::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    // handle special cases
    switch (metric) {
    case PM_MenuHMargin:
    case PM_MenuVMargin:
        return Metrics::MenuItem_HighlightGap;

    // small icon size
    case PM_SmallIconSize: {
        auto iconSize = ParentStyleClass::pixelMetric(metric, option, widget);
        if (!isTabletMode()) {
            return iconSize;
        }

        // in tablet mode, we try to figure out the next size and use it
        // see bug 455513
        auto metaEnum = QMetaEnum::fromType<KIconLoader::StdSizes>();
        for (int i = 0; i + 1 < metaEnum.keyCount(); ++i) {
            if (iconSize == metaEnum.value(i)) {
                return metaEnum.value(i + 1);
            }
        }

        // size is either too large or unknown, just increase it by 50%
        return iconSize * 3 / 2;
    }

    // frame width
    case PM_DefaultFrameWidth: {
        const auto isControl = isQtQuickControl(option, widget);
        if (!widget && !isControl) {
            return 0;
        }
        if (qobject_cast<const QMenu *>(widget)) {
            return Metrics::Menu_FrameWidth;
        }
        if (qobject_cast<const QLineEdit *>(widget)) {
            return Metrics::LineEdit_FrameWidth;
        } else if (isControl) {
            const QString &elementType = option->styleObject->property("elementType").toString();
            if (elementType == QLatin1String("edit") || elementType == QLatin1String("spinbox")) {
                return Metrics::LineEdit_FrameWidth;

            } else if (elementType == QLatin1String("combobox")) {
                return Metrics::ComboBox_FrameWidth;
            }

            return Metrics::Frame_FrameWidth;
        }

        const auto forceFrame = widget->property(PropertyNames::forceFrame);
        if (forceFrame.isValid() && !forceFrame.toBool()) {
            return 0;
        }
        if ((forceFrame.isValid() && forceFrame.toBool()) || widget->property(PropertyNames::bordersSides).isValid()) {
            return Metrics::Frame_FrameWidth;
        }

        if (qobject_cast<const QAbstractScrollArea *>(widget)) {
            auto layout = findParentLayout(widget);

            if (!layout) {
                if (widget->parentWidget() && widget->parentWidget()->layout()) {
                    layout = widget->parentWidget()->layout();
                }
            }

            if (layout) {
                if (layout->inherits("QDockWidgetLayout") || layout->inherits("QMainWindowLayout") || qobject_cast<const QStackedLayout *>(layout)) {
                    return 0;
                }

                if (auto grid = qobject_cast<const QGridLayout *>(layout)) {
                    if (grid->horizontalSpacing() > 0 || grid->verticalSpacing() > 0) {
                        return Metrics::Frame_FrameWidth;
                    }
                }

                // Add frame when scroll area is in a layout with more than an item and the
                // layout has some spacing.
                if (layout->spacing() > 0 && layout->count() > 1) {
                    return Metrics::Frame_FrameWidth;
                }
            }
        }

        if (qobject_cast<const QTabWidget *>(widget)) {
            return Metrics::Frame_FrameWidth;
        }

        // fallback
        return 0;
    }

    case PM_ComboBoxFrameWidth: {
        const auto comboBoxOption(qstyleoption_cast<const QStyleOptionComboBox *>(option));
        return comboBoxOption && comboBoxOption->editable ? Metrics::LineEdit_FrameWidth : Metrics::ComboBox_FrameWidth;
    }

    case PM_SpinBoxFrameWidth:
        return Metrics::SpinBox_FrameWidth;
    case PM_ToolBarFrameWidth:
        return Metrics::ToolBar_FrameWidth;
    case PM_ToolTipLabelFrameWidth:
        return Metrics::ToolTip_FrameWidth;

    case PM_FocusFrameVMargin:
    case PM_FocusFrameHMargin:
        return 2;

    // layout
    case PM_LayoutLeftMargin:
    case PM_LayoutTopMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutBottomMargin: {
        /*
         * use either Child margin or TopLevel margin,
         * depending on  widget type
         */
        if ((option && (option->state & QStyle::State_Window)) || (widget && widget->isWindow())) {
            return Metrics::Layout_TopLevelMarginWidth;

        } else if (widget && widget->inherits("KPageView")) {
            return 0;

        } else {
            return Metrics::Layout_ChildMarginWidth;
        }
    }

    case PM_LayoutHorizontalSpacing:
        return Metrics::Layout_DefaultSpacing;
    case PM_LayoutVerticalSpacing:
        return Metrics::Layout_DefaultSpacing;

    // buttons
    case PM_ButtonMargin: {
        // needs special case for kcalc buttons, to prevent the application to set too small margins
        if (widget && widget->inherits("KCalcButton")) {
            return Metrics::Button_MarginWidth + 4;
        } else {
            return Metrics::Button_MarginWidth;
        }
    }

    case PM_ButtonDefaultIndicator:
        return 0;
    case PM_ButtonShiftHorizontal:
        return 0;
    case PM_ButtonShiftVertical:
        return 0;

    // menubars
    case PM_MenuBarPanelWidth:
        return 0;
    case PM_MenuBarHMargin:
        return 0;
    case PM_MenuBarVMargin:
        return 0;
    case PM_MenuBarItemSpacing:
        return 0;
    case PM_MenuDesktopFrameWidth:
        return 0;

    // menu buttons
    case PM_MenuButtonIndicator:
        return Metrics::MenuButton_IndicatorWidth;

    // toolbars
    case PM_ToolBarHandleExtent:
        return Metrics::ToolBar_HandleExtent;
    case PM_ToolBarSeparatorExtent:
        return Metrics::ToolBar_SeparatorWidth;
    case PM_ToolBarExtensionExtent:
        return pixelMetric(PM_SmallIconSize, option, widget) + 2 * Metrics::ToolButton_MarginWidth;

    case PM_ToolBarItemMargin:
        return Metrics::ToolBar_ItemMargin;
    case PM_ToolBarItemSpacing:
        return Metrics::ToolBar_ItemSpacing;

    // tabbars
    case PM_TabBarTabShiftVertical:
        return 0;
    case PM_TabBarTabShiftHorizontal:
        return 0;
    case PM_TabBarTabOverlap:
        return Metrics::TabBar_TabOverlap;
    case PM_TabBarBaseOverlap:
        return Metrics::TabBar_BaseOverlap;
    case PM_TabBarTabHSpace:
        return 2 * Metrics::TabBar_TabMarginWidth;
    case PM_TabBarTabVSpace:
        return 2 * Metrics::TabBar_TabMarginHeight;
    case PM_TabCloseIndicatorWidth:
    case PM_TabCloseIndicatorHeight:
        return pixelMetric(PM_SmallIconSize, option, widget);

    // scrollbars
    case PM_ScrollBarExtent:
        return Metrics::ScrollBar_Extend;
    case PM_ScrollBarSliderMin:
        return Metrics::ScrollBar_MinSliderHeight;

    // title bar
    case PM_TitleBarHeight:
        return 2 * Metrics::TitleBar_MarginWidth + pixelMetric(PM_SmallIconSize, option, widget);

    // sliders
    case PM_SliderThickness:
        return Metrics::Slider_ControlThickness;
    case PM_SliderControlThickness:
        return Metrics::Slider_ControlThickness;
    case PM_SliderLength:
        return Metrics::Slider_ControlThickness;

    // checkboxes and radio buttons
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return Metrics::CheckBox_Size;

    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return Metrics::CheckBox_ItemSpacing;

    // list headers
    case PM_HeaderMarkSize:
        return Metrics::Header_ArrowSize;
    case PM_HeaderMargin:
        return Metrics::Header_MarginWidth;

    // dock widget
    // return 0 here, since frame is handled directly in polish
    case PM_DockWidgetFrameWidth:
        return 0;
    case PM_DockWidgetTitleMargin:
        return Metrics::Frame_FrameWidth;
    case PM_DockWidgetTitleBarButtonMargin:
        return Metrics::ToolButton_MarginWidth;

    case PM_SplitterWidth:
        return Metrics::Splitter_SplitterWidth;
    case PM_DockWidgetSeparatorExtent:
        return Metrics::Splitter_SplitterWidth;

    // fallback
    default:
        return ParentStyleClass::pixelMetric(metric, option, widget);
    }
}

//______________________________________________________________
int Style::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    switch (hint) {
    case SH_RubberBand_Mask: {
        if (auto mask = qstyleoption_cast<QStyleHintReturnMask *>(returnData)) {
            mask->region = option->rect;

            /*
             * need to check on widget before removing inner region
             * in order to still preserve rubberband in MainWindow and QGraphicsView
             * in QMainWindow because it looks better
             * in QGraphicsView because the painting fails completely otherwise
             */
            if (widget
                && (qobject_cast<const QAbstractItemView *>(widget->parent()) || qobject_cast<const QGraphicsView *>(widget->parent())
                    || qobject_cast<const QMainWindow *>(widget->parent()))) {
                return true;
            }

            // also check if widget's parent is some itemView viewport
            if (widget && widget->parent() && qobject_cast<const QAbstractItemView *>(widget->parent()->parent())
                && static_cast<const QAbstractItemView *>(widget->parent()->parent())->viewport() == widget->parent()) {
                return true;
            }

            // mask out center
            mask->region -= insideMargin(option->rect, 1);

            return true;
        }
        return false;
    }

    case SH_ComboBox_ListMouseTracking:
        return true;
    case SH_MenuBar_MouseTracking:
        return true;
    case SH_Menu_Scrollable:
        return true;
    case SH_Menu_MouseTracking:
        return true;
    case SH_Menu_SubMenuPopupDelay:
        return 150;
    case SH_Menu_SloppySubMenus:
        return true;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    case SH_Widget_Animate:
        return StyleConfigData::animationsEnabled();
#endif
    case SH_Menu_SupportsSections:
        return true;
    case SH_Widget_Animation_Duration:
        return StyleConfigData::animationsEnabled() ? StyleConfigData::animationsDuration() : 0;

    case SH_DialogButtonBox_ButtonsHaveIcons:
        return true;

    case SH_GroupBox_TextLabelVerticalAlignment:
        return Qt::AlignVCenter;
    case SH_TabBar_Alignment:
        return StyleConfigData::tabBarDrawCenteredTabs() ? Qt::AlignCenter : Qt::AlignLeft;
    case SH_ToolBox_SelectedPageTitleBold:
        return false;
    case SH_ScrollBar_MiddleClickAbsolutePosition:
        return true;
    case SH_ScrollView_FrameOnlyAroundContents:
        return false;
    case SH_FormLayoutFormAlignment:
        return Qt::AlignLeft | Qt::AlignTop;
    case SH_FormLayoutLabelAlignment:
        return Qt::AlignRight;
    case SH_FormLayoutFieldGrowthPolicy:
        return QFormLayout::ExpandingFieldsGrow;
    case SH_FormLayoutWrapPolicy:
        return QFormLayout::DontWrapRows;
    case SH_MessageBox_TextInteractionFlags:
        return Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse;
    case SH_ProgressDialog_CenterCancelButton:
        return false;
    case SH_MessageBox_CenterButtons:
        return false;

    case SH_FocusFrame_AboveWidget:
        return true;
    case SH_FocusFrame_Mask:
        return false;

    case SH_RequestSoftwareInputPanel:
        return RSIP_OnMouseClick;
    case SH_TitleBar_NoBorder:
        return true;
    case SH_DockWidget_ButtonsHaveFrame:
        return false;
    default:
        return ParentStyleClass::styleHint(hint, option, widget, returnData);
    }
}

//______________________________________________________________
QRect Style::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
    switch (element) {
    case SE_PushButtonContents:
        return pushButtonContentsRect(option, widget);
    case SE_CheckBoxContents:
        return checkBoxContentsRect(option, widget);
    case SE_RadioButtonContents:
        return checkBoxContentsRect(option, widget);
    case SE_LineEditContents:
        return lineEditContentsRect(option, widget);
    case SE_ProgressBarGroove:
        return progressBarGrooveRect(option, widget);
    case SE_ProgressBarContents:
        return progressBarContentsRect(option, widget);
    case SE_ProgressBarLabel:
        return progressBarLabelRect(option, widget);
    case SE_FrameContents:
        return frameContentsRect(option, widget);
    case SE_HeaderArrow:
        return headerArrowRect(option, widget);
    case SE_HeaderLabel:
        return headerLabelRect(option, widget);
    case SE_TabBarTabLeftButton:
        return tabBarTabLeftButtonRect(option, widget);
    case SE_TabBarTabRightButton:
        return tabBarTabRightButtonRect(option, widget);
    case SE_TabWidgetTabBar:
        return tabWidgetTabBarRect(option, widget);
    case SE_TabWidgetTabContents:
        return tabWidgetTabContentsRect(option, widget);
    case SE_TabWidgetTabPane:
        return tabWidgetTabPaneRect(option, widget);
    case SE_TabWidgetLeftCorner:
        return tabWidgetCornerRect(SE_TabWidgetLeftCorner, option, widget);
    case SE_TabWidgetRightCorner:
        return tabWidgetCornerRect(SE_TabWidgetRightCorner, option, widget);
    case SE_ToolBoxTabContents:
        return toolBoxTabContentsRect(option, widget);

    // fallback
    default:
        return ParentStyleClass::subElementRect(element, option, widget);
    }
}

//______________________________________________________________
QRect Style::subControlRect(ComplexControl element, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    switch (element) {
    case CC_GroupBox:
        return groupBoxSubControlRect(option, subControl, widget);
    case CC_ToolButton:
        return toolButtonSubControlRect(option, subControl, widget);
    case CC_ComboBox:
        return comboBoxSubControlRect(option, subControl, widget);
    case CC_SpinBox:
        return spinBoxSubControlRect(option, subControl, widget);
    case CC_ScrollBar:
        return scrollBarSubControlRect(option, subControl, widget);
    case CC_Dial:
        return dialSubControlRect(option, subControl, widget);
    case CC_Slider:
        return sliderSubControlRect(option, subControl, widget);

    // fallback
    default:
        return ParentStyleClass::subControlRect(element, option, subControl, widget);
    }
}

//______________________________________________________________
QSize Style::sizeFromContents(ContentsType element, const QStyleOption *option, const QSize &size, const QWidget *widget) const
{
    switch (element) {
    case CT_CheckBox:
        return checkBoxSizeFromContents(option, size, widget);
    case CT_RadioButton:
        return checkBoxSizeFromContents(option, size, widget);
    case CT_LineEdit:
        return lineEditSizeFromContents(option, size, widget);
    case CT_ComboBox:
        return comboBoxSizeFromContents(option, size, widget);
    case CT_SpinBox:
        return spinBoxSizeFromContents(option, size, widget);
    case CT_Slider:
        return sliderSizeFromContents(option, size, widget);
    case CT_PushButton:
        return pushButtonSizeFromContents(option, size, widget);
    case CT_ToolButton:
        return toolButtonSizeFromContents(option, size, widget);
    case CT_MenuBar:
        return defaultSizeFromContents(option, size, widget);
    case CT_MenuBarItem:
        return menuBarItemSizeFromContents(option, size, widget);
    case CT_MenuItem:
        return menuItemSizeFromContents(option, size, widget);
    case CT_ProgressBar:
        return progressBarSizeFromContents(option, size, widget);
    case CT_TabWidget:
        return tabWidgetSizeFromContents(option, size, widget);
    case CT_TabBarTab:
        return tabBarTabSizeFromContents(option, size, widget);
    case CT_HeaderSection:
        return headerSectionSizeFromContents(option, size, widget);
    case CT_ItemViewItem:
        return itemViewItemSizeFromContents(option, size, widget);

    // fallback
    default:
        return ParentStyleClass::sizeFromContents(element, option, size, widget);
    }
}

//______________________________________________________________
QStyle::SubControl Style::hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option, const QPoint &point, const QWidget *widget) const
{
    switch (control) {
    case CC_ScrollBar: {
        auto grooveRect = subControlRect(CC_ScrollBar, option, SC_ScrollBarGroove, widget);
        if (grooveRect.contains(point)) {
            // Must be either page up/page down, or just click on the slider.
            auto sliderRect = subControlRect(CC_ScrollBar, option, SC_ScrollBarSlider, widget);

            if (sliderRect.contains(point)) {
                return SC_ScrollBarSlider;
            } else if (preceeds(point, sliderRect, option)) {
                return SC_ScrollBarSubPage;
            } else {
                return SC_ScrollBarAddPage;
            }
        }

        // This is one of the up/down buttons. First, decide which one it is.
        if (preceeds(point, grooveRect, option)) {
            if (_subLineButtons == DoubleButton) {
                auto buttonRect = scrollBarInternalSubControlRect(option, SC_ScrollBarSubLine);
                return scrollBarHitTest(buttonRect, point, option);

            } else {
                return SC_ScrollBarSubLine;
            }
        }

        if (_addLineButtons == DoubleButton) {
            auto buttonRect = scrollBarInternalSubControlRect(option, SC_ScrollBarAddLine);
            return scrollBarHitTest(buttonRect, point, option);

        } else {
            return SC_ScrollBarAddLine;
        }
    }

    // fallback
    default:
        return ParentStyleClass::hitTestComplexControl(control, option, point, widget);
    }
}

//______________________________________________________________
void Style::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    StylePrimitive fcn;
    switch (element) {
    case PE_PanelButtonCommand:
        fcn = &Style::drawPanelButtonCommandPrimitive;
        break;
    case PE_PanelButtonTool:
        fcn = &Style::drawPanelButtonToolPrimitive;
        break;
    case PE_PanelScrollAreaCorner:
        fcn = &Style::drawPanelScrollAreaCornerPrimitive;
        break;
    case PE_PanelMenu:
        fcn = &Style::drawPanelMenuPrimitive;
        break;
    case PE_PanelTipLabel:
        fcn = &Style::drawPanelTipLabelPrimitive;
        break;
    case PE_PanelItemViewItem:
        fcn = &Style::drawPanelItemViewItemPrimitive;
        break;
    case PE_IndicatorCheckBox:
        fcn = &Style::drawIndicatorCheckBoxPrimitive;
        break;
    case PE_IndicatorRadioButton:
        fcn = &Style::drawIndicatorRadioButtonPrimitive;
        break;
    case PE_IndicatorButtonDropDown:
        fcn = &Style::drawIndicatorButtonDropDownPrimitive;
        break;
    case PE_IndicatorTabClose:
        fcn = &Style::drawIndicatorTabClosePrimitive;
        break;
    case PE_IndicatorTabTear:
        fcn = &Style::drawIndicatorTabTearPrimitive;
        break;
    case PE_IndicatorArrowUp:
        fcn = &Style::drawIndicatorArrowUpPrimitive;
        break;
    case PE_IndicatorArrowDown:
        fcn = &Style::drawIndicatorArrowDownPrimitive;
        break;
    case PE_IndicatorArrowLeft:
        fcn = &Style::drawIndicatorArrowLeftPrimitive;
        break;
    case PE_IndicatorArrowRight:
        fcn = &Style::drawIndicatorArrowRightPrimitive;
        break;
    case PE_IndicatorHeaderArrow:
        fcn = &Style::drawIndicatorHeaderArrowPrimitive;
        break;
    case PE_IndicatorToolBarHandle:
        fcn = &Style::drawIndicatorToolBarHandlePrimitive;
        break;
    case PE_IndicatorToolBarSeparator:
        fcn = &Style::drawIndicatorToolBarSeparatorPrimitive;
        break;
    case PE_IndicatorBranch:
        fcn = &Style::drawIndicatorBranchPrimitive;
        break;
    case PE_FrameStatusBarItem:
        fcn = &Style::emptyPrimitive;
        break;
    case PE_Frame:
        fcn = &Style::drawFramePrimitive;
        break;
    case PE_FrameLineEdit:
        fcn = &Style::drawFrameLineEditPrimitive;
        break;
    case PE_FrameMenu:
        fcn = &Style::drawFrameMenuPrimitive;
        break;
    case PE_FrameGroupBox:
        fcn = &Style::drawFrameGroupBoxPrimitive;
        break;
    case PE_FrameTabWidget:
        fcn = &Style::drawFrameTabWidgetPrimitive;
        break;
    case PE_FrameTabBarBase:
        fcn = &Style::drawFrameTabBarBasePrimitive;
        break;
    case PE_FrameWindow:
        fcn = &Style::drawFrameWindowPrimitive;
        break;
    case PE_FrameFocusRect:
        fcn = _frameFocusPrimitive;
        break;
    case PE_IndicatorDockWidgetResizeHandle:
        fcn = &Style::drawDockWidgetResizeHandlePrimitive;
        break;
    case PE_PanelStatusBar:
        fcn = &Style::drawPanelStatusBarPrimitive;
        break;
    case PE_Widget:
        fcn = &Style::drawWidgetPrimitive;
        break;

    // fallback
    default:
        break;
    }

    painter->save();

    // call function if implemented
    if (!(fcn && fcn(*this, option, painter, widget))) {
        ParentStyleClass::drawPrimitive(element, option, painter, widget);
    }

    painter->restore();
}

bool Style::drawWidgetPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // Paint functions for QMainWindow and QDialog tools areas are moved to eventFilter() to allow translucent headers to be painted early enough

    if (widget && widget->inherits("KMultiTabBar")) {
        enum class Position {
            Left,
            Right,
            Top,
            Bottom,
        };

        const Position position = static_cast<Position>(widget->property("position").toInt());
        const auto splitterWidth = Metrics::Splitter_SplitterWidth;
        QRect rect = option->rect;

        if (position == Position::Top || position == Position::Bottom) {
            return true;
        }

        if ((position == Position::Left && widget->layoutDirection() == Qt::LeftToRight)
            || (position == Position::Right && widget->layoutDirection() == Qt::RightToLeft)) {
            rect.setX(rect.width() - splitterWidth);
        }

        rect.setWidth(splitterWidth);

        const auto color(_helper->separatorColor(option->palette));
        _helper->renderSeparator(painter, rect, color, true);
    }
    return true;
}

//______________________________________________________________
void Style::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    StyleControl fcn;

#if BREEZE_HAVE_KSTYLE
    if (element == CE_CapacityBar) {
        fcn = &Style::drawProgressBarControl;

    } else
#endif
    {
        switch (element) {
        case CE_PushButtonBevel:
            fcn = &Style::drawPanelButtonCommandPrimitive;
            break;
        case CE_PushButtonLabel:
            fcn = &Style::drawPushButtonLabelControl;
            break;
        case CE_CheckBoxLabel:
            fcn = &Style::drawCheckBoxLabelControl;
            break;
        case CE_RadioButtonLabel:
            fcn = &Style::drawCheckBoxLabelControl;
            break;
        case CE_ToolButtonLabel:
            fcn = &Style::drawToolButtonLabelControl;
            break;
        case CE_ComboBoxLabel:
            fcn = &Style::drawComboBoxLabelControl;
            break;
        case CE_MenuBarEmptyArea:
            fcn = &Style::emptyControl;
            break;
        case CE_MenuBarItem:
            fcn = &Style::drawMenuBarItemControl;
            break;
        case CE_MenuItem:
            fcn = &Style::drawMenuItemControl;
            break;
        case CE_ToolBar:
            fcn = &Style::emptyControl;
            break;
        case CE_ProgressBar:
            fcn = &Style::drawProgressBarControl;
            break;
        case CE_ProgressBarContents:
            fcn = &Style::drawProgressBarContentsControl;
            break;
        case CE_ProgressBarGroove:
            fcn = &Style::drawProgressBarGrooveControl;
            break;
        case CE_ProgressBarLabel:
            fcn = &Style::drawProgressBarLabelControl;
            break;
        case CE_ScrollBarSlider:
            fcn = &Style::drawScrollBarSliderControl;
            break;
        case CE_ScrollBarAddLine:
            fcn = &Style::drawScrollBarAddLineControl;
            break;
        case CE_ScrollBarSubLine:
            fcn = &Style::drawScrollBarSubLineControl;
            break;
        case CE_ScrollBarAddPage:
            fcn = &Style::emptyControl;
            break;
        case CE_ScrollBarSubPage:
            fcn = &Style::emptyControl;
            break;
        case CE_ShapedFrame:
            fcn = &Style::drawShapedFrameControl;
            break;
        case CE_FocusFrame:
            fcn = &Style::drawFocusFrame;
            break;
        case CE_RubberBand:
            fcn = &Style::drawRubberBandControl;
            break;
        case CE_SizeGrip:
            fcn = &Style::emptyControl;
            break;
        case CE_HeaderSection:
            fcn = &Style::drawHeaderSectionControl;
            break;
        case CE_HeaderEmptyArea:
            fcn = &Style::drawHeaderEmptyAreaControl;
            break;
        case CE_TabBarTabLabel:
            fcn = &Style::drawTabBarTabLabelControl;
            break;
        case CE_TabBarTabShape:
            fcn = &Style::drawTabBarTabShapeControl;
            break;
        case CE_ToolBoxTabLabel:
            fcn = &Style::drawToolBoxTabLabelControl;
            break;
        case CE_ToolBoxTabShape:
            fcn = &Style::drawToolBoxTabShapeControl;
            break;
        case CE_DockWidgetTitle:
            fcn = &Style::drawDockWidgetTitleControl;
            break;
        case QStyle::CE_Splitter:
            fcn = &Style::drawSplitterControl;
            break;

        // fallback
        default:
            break;
        }
    }

    painter->save();

    // call function if implemented
    if (!(fcn && fcn(*this, option, painter, widget))) {
        ParentStyleClass::drawControl(element, option, painter, widget);
    }

    painter->restore();
}

//______________________________________________________________
void Style::drawComplexControl(ComplexControl element, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    StyleComplexControl fcn;
    switch (element) {
    case CC_GroupBox:
        fcn = &Style::drawGroupBoxComplexControl;
        break;
    case CC_ToolButton:
        fcn = &Style::drawToolButtonComplexControl;
        break;
    case CC_ComboBox:
        fcn = &Style::drawComboBoxComplexControl;
        break;
    case CC_SpinBox:
        fcn = &Style::drawSpinBoxComplexControl;
        break;
    case CC_Slider:
        fcn = &Style::drawSliderComplexControl;
        break;
    case CC_Dial:
        fcn = &Style::drawDialComplexControl;
        break;
    case CC_ScrollBar:
        fcn = &Style::drawScrollBarComplexControl;
        break;
    case CC_TitleBar:
        fcn = &Style::drawTitleBarComplexControl;
        break;

    // fallback
    default:
        break;
    }

    painter->save();

    // call function if implemented
    if (!(fcn && fcn(*this, option, painter, widget))) {
        ParentStyleClass::drawComplexControl(element, option, painter, widget);
    }

    painter->restore();
}

//___________________________________________________________________________________
void Style::drawItemText(QPainter *painter,
                         const QRect &rect,
                         int flags,
                         const QPalette &palette,
                         bool enabled,
                         const QString &text,
                         QPalette::ColorRole textRole) const
{
    // hide mnemonics if requested
    if (!_mnemonics->enabled() && (flags & Qt::TextShowMnemonic) && !(flags & Qt::TextHideMnemonic)) {
        flags &= ~Qt::TextShowMnemonic;
        flags |= Qt::TextHideMnemonic;
    }

    // make sure vertical alignment is defined
    // fallback on Align::VCenter if not
    if (!(flags & Qt::AlignVertical_Mask)) {
        flags |= Qt::AlignVCenter;
    }

    if (_animations->widgetEnabilityEngine().enabled()) {
        /*
         * check if painter engine is registered to WidgetEnabilityEngine, and animated
         * if yes, merge the palettes. Note: void * is used here because we only care
         * about the pointer value which is used as a key to look up a value in a map.
         */
        const void *key = painter->device();
        if (_animations->widgetEnabilityEngine().isAnimated(key, AnimationEnable)) {
            const QPalette copy(_helper->disabledPalette(palette, _animations->widgetEnabilityEngine().opacity(key, AnimationEnable)));
            return ParentStyleClass::drawItemText(painter, rect, flags, copy, enabled, text, textRole);
        }
    }

    // fallback
    return ParentStyleClass::drawItemText(painter, rect, flags, palette, enabled, text, textRole);
}

bool Style::event(QEvent *e)
{
    // Adapted from QMacStyle::event()
    if (e->type() == QEvent::FocusIn) {
        QWidget *target = nullptr;
        auto focusWidget = QApplication::focusWidget();
        if (auto graphicsView = qobject_cast<QGraphicsView *>(focusWidget)) {
            QGraphicsItem *focusItem = graphicsView->scene() ? graphicsView->scene()->focusItem() : nullptr;
            if (focusItem && focusItem->type() == QGraphicsProxyWidget::Type) {
                auto proxy = static_cast<QGraphicsProxyWidget *>(focusItem);
                if (proxy->widget()) {
                    focusWidget = proxy->widget()->focusWidget();
                }
            }
        }

        if (focusWidget) {
            auto focusEvent = static_cast<QFocusEvent *>(e);
            auto focusReason = focusEvent->reason();
            bool hasKeyboardFocusReason = focusReason == Qt::TabFocusReason || focusReason == Qt::BacktabFocusReason || focusReason == Qt::ShortcutFocusReason;
            if (hasKeyboardFocusReason) {
                auto focusProxy = focusWidget->focusProxy();
                while (focusProxy != nullptr) {
                    focusWidget = focusProxy;
                    focusProxy = focusWidget->focusProxy();
                }
                // by default we want to draw a focus frame only for the following widgets
                if (focusWidget->inherits("QLineEdit") || focusWidget->inherits("QTextEdit") || focusWidget->inherits("QAbstractSpinBox")
                    || focusWidget->inherits("QComboBox") || focusWidget->inherits("QPushButton") || focusWidget->inherits("QToolButton")
                    || focusWidget->inherits("QCheckBox") || focusWidget->inherits("QRadioButton") || focusWidget->inherits("QSlider")
                    || focusWidget->inherits("QDial") || focusWidget->inherits("QGroupBox")) {
                    target = focusWidget;
                }
            }
        }

        if (_focusFrame) {
            // sets to nullptr or a widget
            _focusFrame->setWidget(target);
        } else if (target) { // only create if there is a widget
            _focusFrame = new QFocusFrame(target);
            _focusFrame->setWidget(target);
        }
    } else if (e->type() == QEvent::FocusOut) {
        if (_focusFrame) {
            _focusFrame->setWidget(nullptr);
        }
    }
    return ParentStyleClass::event(e);
}

//_____________________________________________________________________
bool Style::eventFilter(QObject *object, QEvent *event)
{
    if (auto mw = qobject_cast<QMainWindow *>(object)) {
        return eventFilterMainWindow(mw, event);
    } else if (auto dialog = qobject_cast<QDialog *>(object)) {
        return eventFilterDialog(dialog, event);
    } else if (auto dockWidget = qobject_cast<QDockWidget *>(object)) {
        return eventFilterDockWidget(dockWidget, event);
    } else if (auto subWindow = qobject_cast<QMdiSubWindow *>(object)) {
        return eventFilterMdiSubWindow(subWindow, event);
    } else if (auto commandLinkButton = qobject_cast<QCommandLinkButton *>(object)) {
        return eventFilterCommandLinkButton(commandLinkButton, event);
    } else if (auto scrollBar = qobject_cast<QScrollBar *>(object)) {
        return eventFilterScrollBar(scrollBar, event);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    else if (object == qApp && event->type() == QEvent::ApplicationPaletteChange) {
        loadConfiguration();
    }
#endif

    if (object->isWidgetType()) {
        QWidget *widget = static_cast<QWidget *>(object);

        if (widget->objectName() == QLatin1String("KPageView::Search") || widget->objectName() == QLatin1String("KPageView::TitleWidget")) {
            return eventFilterPageViewHeader(widget, event);
        } else if (auto dialogButtonBox = qobject_cast<QDialogButtonBox *>(object)) {
            if (widget->property(PropertyNames::forceFrame).toBool() || (widget->parentWidget() && widget->parentWidget()->inherits("KPageView"))) {
                // QDialogButtonBox has no paintEvent
                return eventFilterDialogButtonBox(dialogButtonBox, event);
            }
        } else if (widget->inherits("QAbstractScrollArea") || widget->inherits("KTextEditor::View")) {
            return eventFilterScrollArea(widget, event);
        } else if (widget->inherits("QComboBoxPrivateContainer")) {
            return eventFilterComboBoxContainer(widget, event);
        }
    }

    // fallback
    return ParentStyleClass::eventFilter(object, event);
}

//____________________________________________________________________________
bool Style::eventFilterDialogButtonBox(QDialogButtonBox *widget, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        QPainter painter(widget);
        auto paintEvent = static_cast<QPaintEvent *>(event);
        painter.setClipRegion(paintEvent->region());

        // store rect and palette
        auto rect(widget->rect());
        rect.setHeight(1);
        const auto &palette(widget->palette());

        // define color and render
        const auto color(_helper->separatorColor(palette));
        _helper->renderSeparator(&painter, rect, color, false);
    }

    return false;
}

//____________________________________________________________________________
bool Style::eventFilterPageViewHeader(QWidget *widget, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        QPainter painter(widget);
        const auto &palette(_toolsAreaManager->palette());

        painter.setBrush(palette.color(QPalette::Window));
        painter.setPen(Qt::NoPen);
        painter.drawRect(widget->rect());

        // Add separator next to the search field
        if (widget->objectName() == QLatin1String("KPageView::Search")) {
            auto rect(widget->rect());
            if (widget->layoutDirection() == Qt::RightToLeft) {
                rect.setWidth(1);
            } else {
                rect.setLeft(rect.width() - 1);
            }
            rect.setHeight(rect.height() - Metrics::ToolBar_SeparatorVerticalMargin * 2);
            rect.setY(Metrics::ToolBar_SeparatorVerticalMargin);

            // define color and render
            const auto color(_helper->separatorColor(palette));
            _helper->renderSeparator(&painter, rect, color, true);
        }
    }

    return false;
}

//____________________________________________________________________________
bool Style::eventFilterScrollArea(QWidget *widget, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Paint: {
        // get scrollarea viewport
        auto scrollArea(qobject_cast<QAbstractScrollArea *>(widget));
        QWidget *viewport;
        if (!(scrollArea && (viewport = scrollArea->viewport()))) {
            break;
        }

        // get scrollarea horizontal and vertical containers
        QWidget *child(nullptr);
        QList<QWidget *> children;
        if (!widget->inherits("QComboBoxListView")) {
            if ((child = scrollArea->findChild<QWidget *>("qt_scrollarea_vcontainer")) && child->isVisible()) {
                children.append(child);
            }

            if ((child = scrollArea->findChild<QWidget *>("qt_scrollarea_hcontainer")) && child->isVisible()) {
                children.append(child);
            }
        }

        if (children.empty()) {
            break;
        }
        if (!scrollArea->styleSheet().isEmpty()) {
            break;
        }

        // make sure proper background is rendered behind the containers
        QPainter painter(scrollArea);
        painter.setClipRegion(static_cast<QPaintEvent *>(event)->region());

        painter.setPen(Qt::NoPen);

        // decide background color
        const QPalette::ColorRole role(viewport->backgroundRole());
        QColor background;
        if (role == QPalette::Window && hasAlteredBackground(viewport)) {
            background = _helper->frameBackgroundColor(viewport->palette());
        } else {
            background = viewport->palette().color(role);
        }
        painter.setBrush(background);

        // render
        for (auto *child : std::as_const(children)) {
            painter.drawRect(child->geometry());
        }

    } break;

    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove: {
        // case event
        QMouseEvent *mouseEvent(static_cast<QMouseEvent *>(event));

        // get frame framewidth
        const int frameWidth(pixelMetric(PM_DefaultFrameWidth, nullptr, widget));

        // find list of scrollbars
        QList<QScrollBar *> scrollBars;
        if (auto scrollArea = qobject_cast<QAbstractScrollArea *>(widget)) {
            if (scrollArea->horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
                scrollBars.append(scrollArea->horizontalScrollBar());
            }
            if (scrollArea->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOff) {
                scrollBars.append(scrollArea->verticalScrollBar());
            }

        } else if (widget->inherits("KTextEditor::View")) {
            scrollBars = widget->findChildren<QScrollBar *>();
        }

        // loop over found scrollbars
        for (QScrollBar *scrollBar : std::as_const(scrollBars)) {
            if (!(scrollBar && scrollBar->isVisible() && scrollBar->isEnabled())) {
                continue;
            }

            QPoint offset;
            if (scrollBar->orientation() == Qt::Horizontal) {
                offset = QPoint(0, frameWidth);
            } else {
                offset = QPoint(QApplication::isLeftToRight() ? frameWidth : -frameWidth, 0);
            }

            // map position to scrollarea
            QPoint position(scrollBar->mapFrom(widget, mouseEvent->pos() - offset));

            // check if contains
            if (!scrollBar->rect().contains(position)) {
                continue;
            }

            // copy event, send and return
            QMouseEvent copy(mouseEvent->type(),
                             position,
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
                             QCursor::pos(),
#endif
                             mouseEvent->button(),
                             mouseEvent->buttons(),
                             mouseEvent->modifiers());

            QCoreApplication::sendEvent(scrollBar, &copy);
            event->setAccepted(true);
            return true;
        }

        break;
    }

    default:
        break;
    }

    return ParentStyleClass::eventFilter(widget, event);
}

//_________________________________________________________
bool Style::eventFilterComboBoxContainer(QWidget *widget, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        QPainter painter(widget);
        auto paintEvent = static_cast<QPaintEvent *>(event);
        painter.setClipRegion(paintEvent->region());

        const auto rect(widget->rect());
        const auto &palette(widget->palette());
        const auto background(_helper->frameBackgroundColor(palette));
        const auto outline(_helper->frameOutlineColor(palette));

        const bool hasAlpha(_helper->hasAlphaChannel(widget));
        if (hasAlpha) {
            painter.setCompositionMode(QPainter::CompositionMode_Source);
        }
        _helper->renderMenuFrame(&painter, rect, background, outline, hasAlpha);
    }

    return false;
}

//____________________________________________________________________________
bool Style::eventFilterMainWindow(QMainWindow *mw, QEvent *event)
{
    if (event->type() == QEvent::Paint && mw && mw->window() == mw) {
        QPainter painter(mw);
        painter.setClipRegion(static_cast<QPaintEvent *>(event)->region());
        drawMainWindow(&painter, mw, true);
    }
    return ParentStyleClass::eventFilter(mw, event);
}

//____________________________________________________________________________
bool Style::eventFilterDialog(QDialog *dialog, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        QPainter painter(dialog);
        painter.setClipRegion(static_cast<QPaintEvent *>(event)->region());
        drawDialog(&painter, dialog, true);
    }
    return ParentStyleClass::eventFilter(dialog, event);
}

void Style::drawMainWindow(QPainter *painter, const QMainWindow *mw, const bool drawWindowBackground) const
{
    const QColor windowColor = mw->palette().color(mw->isActiveWindow() ? QPalette::Active : QPalette::Inactive, QPalette::Window);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const bool hasAlpha(_helper->hasAlphaChannel(mw));
    if (hasAlpha)
        painter->setCompositionMode(QPainter::CompositionMode_Source);
#endif
    const bool toolsAreaWithHeaderColors = _toolsAreaManager->hasHeaderColors() && _helper->shouldDrawToolsArea(mw);

    auto rect = _toolsAreaManager->toolsAreaRect(mw);
    if (rect.height() == 0) {
        if (drawWindowBackground) {
            auto bg = mw->rect();
            painter->setPen(Qt::NoPen);
            painter->setBrush(windowColor);
            painter->drawRect(bg);
        }
        if (toolsAreaWithHeaderColors) {
            drawToolsAreaSeparator(painter, mw);
        }
    } else {
        if (drawWindowBackground) {
            auto bg = mw->rect();
            auto rect = _toolsAreaManager->toolsAreaRect(mw);
            bg.setTop(rect.height() - 1);

            painter->setPen(Qt::NoPen);
            painter->setBrush(windowColor);
            painter->drawRect(bg);
        }
        drawToolsAreaBackgroundAndSeparator(painter, mw, rect, true);
    }
}

void Style::drawDialog(QPainter *painter, const QDialog *dialog, const bool drawDialogBackground) const
{
    const QColor windowColor = dialog->palette().color(dialog->isActiveWindow() ? QPalette::Active : QPalette::Inactive, QPalette::Window);
    painter->setCompositionMode(QPainter::CompositionMode_Source);

    const bool toolsAreaWithHeaderColors = _toolsAreaManager->hasHeaderColors() && _helper->shouldDrawToolsArea(dialog);
    auto vLayout = qobject_cast<QVBoxLayout *>(dialog->layout());
    if (vLayout) {
        QRect rect(0, 0, dialog->width(), 0);
        const auto color = _toolsAreaManager->palette().brush(dialog->isActiveWindow() ? QPalette::Active : QPalette::Inactive, QPalette::Window);

        if (vLayout->menuBar()) {
            rect.setHeight(rect.height() + vLayout->menuBar()->rect().height());
        }

        for (int i = 0, count = vLayout->count(); i < count; i++) {
            const auto layoutItem = vLayout->itemAt(i);
            if (layoutItem->widget() && qobject_cast<QToolBar *>(layoutItem->widget())) {
                rect.setHeight(rect.height() + layoutItem->widget()->rect().height() + vLayout->spacing());
            } else {
                break;
            }
        }

        if (rect.height() > 0) {
            // We found either a QMenuBar or a QToolBar

            // Add contentsMargins + separator
            rect.setHeight(rect.height() + 1 + vLayout->contentsMargins().top());

            if (drawDialogBackground) {
                auto bg = dialog->rect();
                bg.setTop(rect.height());

                painter->setPen(Qt::NoPen);
                painter->setBrush(windowColor);
                painter->drawRect(bg);
            }
            drawToolsAreaBackgroundAndSeparator(painter, dialog, rect, true);
        } else {
            if (drawDialogBackground) {
                auto bg = dialog->rect();
                painter->setPen(Qt::NoPen);
                painter->setBrush(windowColor);
                painter->drawRect(bg);
            }
            if (toolsAreaWithHeaderColors) {
                drawToolsAreaSeparator(painter, dialog);
            }
        }
    } else if (drawDialogBackground) {
        auto bg = dialog->rect();

        painter->setPen(Qt::NoPen);
        painter->setBrush(windowColor);
        painter->drawRect(bg);
    }
}

void Style::drawToolsAreaSeparator(QPainter *painter, const QWidget *w) const
{
    if (!(qobject_cast<const QMainWindow *>(w) || qobject_cast<const QDialog *>(w))) {
        return;
    }

    if (w->property(PropertyNames::noSeparator).toBool() || w->isFullScreen()) {
        return;
    }
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setRenderHints(QPainter::Antialiasing);
    QPen pen(_helper->separatorColor(_toolsAreaManager->palette()), PenWidth::Frame * qRound(w->devicePixelRatioF()));
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->drawLine(w->rect().topLeft() + QPointF(0, PenWidth::Frame / 2), w->rect().topRight() + QPointF(1, PenWidth::Frame / 2));
}

void Style::drawToolsAreaBackgroundAndSeparator(QPainter *painter, const QWidget *w, const QRect &rect, const bool drawBackground) const
{
    if (!(qobject_cast<const QMainWindow *>(w) || qobject_cast<const QDialog *>(w))) {
        return;
    }

    if (drawBackground) {
        QBrush color = _toolsAreaManager->palette().brush(w->isActiveWindow() ? QPalette::Active : QPalette::Inactive, QPalette::Window);

        if (_helper->decorationConfig()->applyOpacityToHeader() && color.color().alpha() < 255) {
            if ((w->isMaximized() || w->isFullScreen()) && _helper->decorationConfig()->opaqueMaximizedTitleBars()) {
                QColor colorWithoutAlpha = color.color();
                colorWithoutAlpha.setAlpha(255);
                color.setColor(colorWithoutAlpha);
            } else if (_helper->decorationConfig()->blurTransparentTitleBars()) { // apply blur to tools area
                if ((w->testAttribute(Qt::WA_WState_Created) || w->internalWinId())) {
                    // PAM: modified from breezeblurhelper.cpp -- did not use _blurHelper->registerWidget() as it doesn't allow you to specify a region, hence
                    // blurring the entire window and causing kornerbug
                    w->winId(); // force creation of the window handle
                    KWindowEffects::enableBlurBehind(w->windowHandle(), true, rect);

                    // no force update at this point like in breezeblurhelper.cpp, as already drawing next and creates an infinite loop
                }
            }
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawRect(rect);
    }

    // default Painter composition mode from previous function may be CompositionMode_Source
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setRenderHints(QPainter::Antialiasing);
    QPen pen(_helper->separatorColor(_toolsAreaManager->palette()), PenWidth::Frame * qRound(w->devicePixelRatioF()));
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->drawLine(rect.bottomLeft() + QPointF(0, 1 - PenWidth::Frame / 2), rect.bottomRight() + QPointF(1, 1 - PenWidth::Frame / 2));
}

//____________________________________________________________________________
bool Style::eventFilterDockWidget(QDockWidget *dockWidget, QEvent *event)
{
    if (event->type() == QEvent::Paint && dockWidget->isFloating()) {
        // create painter and clip
        QPainter painter(dockWidget);
        QPaintEvent *paintEvent = static_cast<QPaintEvent *>(event);
        painter.setClipRegion(paintEvent->region());

        // store palette and set colors
        const auto &palette(dockWidget->palette());
        const auto background(_helper->frameBackgroundColor(palette));
        const auto outline(_helper->frameOutlineColor(palette));

        // store rect
        const auto rect(dockWidget->rect());

        // render
        _helper->renderMenuFrame(&painter, rect, background, outline, false);
    }

    return false;
}

//____________________________________________________________________________
bool Style::eventFilterMdiSubWindow(QMdiSubWindow *subWindow, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        QPainter painter(subWindow);
        QPaintEvent *paintEvent(static_cast<QPaintEvent *>(event));
        painter.setClipRegion(paintEvent->region());

        const auto rect(subWindow->rect());
        const auto background(subWindow->palette().color(QPalette::Window));

        if (subWindow->isMaximized()) {
            // full painting
            painter.setPen(Qt::NoPen);
            painter.setBrush(background);
            painter.drawRect(rect);

        } else {
            // framed painting
            _helper->renderMenuFrame(&painter, rect, background, QColor());
        }
    }

    // continue with normal painting
    return false;
}

//____________________________________________________________________________
bool Style::eventFilterCommandLinkButton(QCommandLinkButton *button, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        // painter
        QPainter painter(button);
        painter.setClipRegion(static_cast<QPaintEvent *>(event)->region());

        const bool isFlat = false;

        // option
        QStyleOptionButton option;
        option.initFrom(button);
        option.features |= QStyleOptionButton::CommandLinkButton;
        if (isFlat) {
            option.features |= QStyleOptionButton::Flat;
        }
        option.text = QString();
        option.icon = QIcon();

        if (button->isChecked()) {
            option.state |= State_On;
        }
        if (button->isDown()) {
            option.state |= State_Sunken;
        }

        // frame
        drawControl(QStyle::CE_PushButton, &option, &painter, button);

        // offset
        const int margin(Metrics::Button_MarginWidth + Metrics::Frame_FrameWidth);
        QPoint offset(margin, margin);

        // state
        const State &state(option.state);
        const bool enabled(state & State_Enabled);

        // icon
        if (!button->icon().isNull()) {
            const auto pixmapSize(button->icon().actualSize(button->iconSize()));
            const QRect pixmapRect(QPoint(offset.x(), button->description().isEmpty() ? (button->height() - pixmapSize.height()) / 2 : offset.y()), pixmapSize);
            const qreal dpr = painter.device() ? painter.device()->devicePixelRatioF() : qApp->devicePixelRatio();
            const QPixmap pixmap(_helper->coloredIcon(button->icon(),
                                                      button->palette(),
                                                      pixmapSize,
                                                      dpr,
                                                      enabled ? QIcon::Normal : QIcon::Disabled,
                                                      button->isChecked() ? QIcon::On : QIcon::Off));
            drawItemPixmap(&painter, pixmapRect, Qt::AlignCenter, pixmap);

            offset.rx() += pixmapSize.width() + Metrics::Button_ItemSpacing;
        }

        // text rect
        QRect textRect(offset, QSize(button->size().width() - offset.x() - margin, button->size().height() - 2 * margin));
        const QPalette::ColorRole textRole = QPalette::ButtonText;
        if (!button->text().isEmpty()) {
            QFont font(button->font());
            font.setBold(true);
            painter.setFont(font);
            if (button->description().isEmpty()) {
                drawItemText(&painter, textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextHideMnemonic, button->palette(), enabled, button->text(), textRole);

            } else {
                drawItemText(&painter, textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextHideMnemonic, button->palette(), enabled, button->text(), textRole);
                textRect.setTop(textRect.top() + QFontMetrics(font).height());
            }

            painter.setFont(button->font());
        }

        if (!button->description().isEmpty()) {
            drawItemText(&painter, textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, button->palette(), enabled, button->description(), textRole);
        }

        return true;
    }

    // continue with normal painting
    return false;
}

bool Style::eventFilterScrollBar(QScrollBar *scrollBar, QEvent *event)
{
    if (shouldAutoHideArrows(scrollBar)) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

            QStyleOptionSlider opt;
            _helper->initScrollBarStyleOption(scrollBar, &opt);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QPoint click = mouseEvent->position().toPoint();
#else
            QPoint click = mouseEvent->pos();
#endif
            if (hitTestComplexControl(CC_ScrollBar, &opt, click, scrollBar) == SC_ScrollBarSlider) {
                bool horizontal = scrollBar->orientation() == Qt::Orientation::Horizontal;
                QRect sr = subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, scrollBar);
                int clickOffset = horizontal ? (click.x() - sr.x()) : (click.y() - sr.y());

                // adjust the clickOffset so that dragging over the space where the arrow resides initially results in instantaneous scrolling
                if (opt.sliderPosition <= opt.minimum && opt.sliderPosition != opt.maximum) {
                    QRect grShownArrows = scrollBarSubControlRect(&opt,
                                                                  QStyle::SC_ScrollBarGroove,
                                                                  scrollBar,
                                                                  OverrideAutoHideArrows::ShowArrow,
                                                                  OverrideAutoHideArrows::ShowArrow);
                    QRect grHiddenSubLine = scrollBarSubControlRect(&opt,
                                                                    QStyle::SC_ScrollBarGroove,
                                                                    scrollBar,
                                                                    OverrideAutoHideArrows::HideArrow,
                                                                    OverrideAutoHideArrows::ShowArrow);
                    if (horizontal) {
                        clickOffset -= (grShownArrows.left() - grHiddenSubLine.left());
                    } else {
                        clickOffset -= (grShownArrows.top() - grHiddenSubLine.top());
                    }
                } else if (opt.sliderPosition >= opt.maximum && opt.sliderPosition != opt.minimum) {
                    QRect grShownArrows = scrollBarSubControlRect(&opt,
                                                                  QStyle::SC_ScrollBarGroove,
                                                                  scrollBar,
                                                                  OverrideAutoHideArrows::ShowArrow,
                                                                  OverrideAutoHideArrows::ShowArrow);
                    QRect grHiddenAddLine = scrollBarSubControlRect(&opt,
                                                                    QStyle::SC_ScrollBarGroove,
                                                                    scrollBar,
                                                                    OverrideAutoHideArrows::ShowArrow,
                                                                    OverrideAutoHideArrows::HideArrow);
                    if (horizontal) {
                        clickOffset -= (grShownArrows.right() - grHiddenAddLine.right());
                    } else {
                        clickOffset -= (grShownArrows.bottom() - grHiddenAddLine.bottom());
                    }
                }

                scrollBar->setProperty("_klassySliderClickOffset", clickOffset);
            }
        }

        else if (event->type() == QEvent::MouseMove && scrollBar->isSliderDown()) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QPoint click = mouseEvent->position().toPoint();
#else
            QPoint click = mouseEvent->pos();
#endif
            // modified from https://github.com/qt/qtbase/blob/dev/src/widgets/widgets/qscrollbar.cpp for Klassy
            int newSliderPosition = scrollBarPixelPosToRangeValue(scrollBar,
                                                                  (scrollBar->orientation() == Qt::Orientation::Horizontal ? click.x() : click.y())
                                                                      - scrollBar->property("_klassySliderClickOffset").toInt());
            scrollBar->setSliderPosition(newSliderPosition);
            event->setAccepted(true);
            return true;

        } else if (event->type() == QEvent::MouseButtonRelease) {
            scrollBar->setProperty("_klassySliderClickOffset", QVariant());
        }
    }
    return false;
}

// modified from https://github.com/qt/qtbase/blob/dev/src/widgets/widgets/qscrollbar.cpp for Klassy
int Style::scrollBarPixelPosToRangeValue(QScrollBar *scrollBar, int pos) const
{
    QStyleOptionSlider opt;
    _helper->initScrollBarStyleOption(scrollBar, &opt);

    QRect srShownArrows =
        scrollBarSubControlRect(&opt, QStyle::SC_ScrollBarSlider, scrollBar, OverrideAutoHideArrows::ShowArrow, OverrideAutoHideArrows::ShowArrow);
    QRect grShownArrows =
        scrollBarSubControlRect(&opt, QStyle::SC_ScrollBarGroove, scrollBar, OverrideAutoHideArrows::ShowArrow, OverrideAutoHideArrows::ShowArrow);

    int sliderMin, sliderMax, sliderLength;

    if (opt.orientation == Qt::Horizontal) {
        sliderLength = srShownArrows.width();
        sliderMin = grShownArrows.x();
        sliderMax = grShownArrows.right() - sliderLength + 1;
        if (scrollBar->layoutDirection() == Qt::RightToLeft)
            opt.upsideDown = !opt.upsideDown;
    } else {
        sliderLength = srShownArrows.height();
        sliderMin = grShownArrows.y();
        sliderMax = grShownArrows.bottom() - sliderLength + 1;
    }

    return QStyle::sliderValueFromPosition(opt.minimum, opt.maximum, pos - sliderMin, sliderMax - sliderMin, opt.upsideDown);
}

//_____________________________________________________________________
void Style::configurationChanged()
{
    // reload
    StyleConfigData::self()->load();

    // reload configuration
    loadConfiguration();
}

//_____________________________________________________________________
void Style::loadGlobalAnimationSettings()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig();
    const KConfigGroup cg(config, QStringLiteral("KDE"));

    // Don't override if it isn't set by the user
    if (!cg.hasKey("AnimationDurationFactor")) {
        return;
    }

    const int animationsDuration = cg.readEntry("AnimationDurationFactor", StyleConfigData::animationsDuration() / 100.0f) * 100;
    if (animationsDuration > 0) {
        StyleConfigData::setAnimationsDuration(animationsDuration);
        StyleConfigData::setAnimationsEnabled(true);
    } else {
        StyleConfigData::setAnimationsEnabled(false);
    }
}

//_____________________________________________________________________
void Style::globalConfigurationChanged(int type, int arg)
{
    Q_UNUSED(arg);

    // 3 == SettingsChanged, which is manually redefined in
    // plasma-integration/src/platformtheme/khintssettings.h and fetched
    // from KGlobalConfig in kdelibs4support in plasma-desktop/kcms/*,
    // seems to be agreed on by everything in plasma is what sets the
    // animation duration
    if (type != 3) {
        return;
    }

    // Reload the new values
    loadGlobalAnimationSettings();

    // reinitialize engines
    _animations->setupEngines();
}

//____________________________________________________________________
QIcon Style::standardIconImplementation(StandardPixmap standardPixmap, const QStyleOption *option, const QWidget *widget) const
{
    // lookup cache
    if (_iconCache.contains(standardPixmap)) {
        return _iconCache.value(standardPixmap);
    }

    QIcon icon;
    switch (standardPixmap) {
    case SP_TitleBarNormalButton:
    case SP_TitleBarMinButton:
    case SP_TitleBarMaxButton:
    case SP_TitleBarCloseButton:
    case SP_DockWidgetCloseButton:
        icon = titleBarButtonIcon(standardPixmap, option, widget);
        break;

    case SP_ToolBarHorizontalExtensionButton:
    case SP_ToolBarVerticalExtensionButton:
        icon = toolBarExtensionIcon(standardPixmap, option, widget);
        break;

    default:
        break;
    }

    if (icon.isNull()) {
        // do not cache parent style icon, since it may change at runtime
        return ParentStyleClass::standardIcon(standardPixmap, option, widget);

    } else {
        const_cast<IconCache *>(&_iconCache)->insert(standardPixmap, icon);
        return icon;
    }
}

//_____________________________________________________________________
void Style::loadConfiguration()
{
    // load helper configuration
    _helper->loadConfig();

    _toolsAreaManager->configUpdated();

    loadGlobalAnimationSettings();

    // reinitialize engines
    _animations->setupEngines();
    _windowManager->initialize();

    // mnemonics
    _mnemonics->setMode(StyleConfigData::mnemonicsMode());

    // splitter proxy
    _splitterFactory->setEnabled(StyleConfigData::splitterProxyEnabled());

    // reset shadow tiles
    _shadowHelper->loadConfig();

    // set mdiwindow factory shadow tiles
    _mdiWindowShadowFactory->setShadowHelper(_shadowHelper.get());

    // clear icon cache
    _iconCache.clear();

    // scrollbar buttons
    switch (StyleConfigData::scrollBarAddLineButtons()) {
    case 0:
        _addLineButtons = NoButton;
        break;
    case 1:
        _addLineButtons = SingleButton;
        break;

    default:
    case 2:
        _addLineButtons = DoubleButton;
        break;
    }

    switch (StyleConfigData::scrollBarSubLineButtons()) {
    case 0:
        _subLineButtons = NoButton;
        break;
    case 1:
        _subLineButtons = SingleButton;
        break;

    default:
    case 2:
        _subLineButtons = DoubleButton;
        break;
    }

    // frame focus
    if (StyleConfigData::viewDrawFocusIndicator()) {
        _frameFocusPrimitive = &Style::drawFrameFocusRectPrimitive;
    } else {
        _frameFocusPrimitive = &Style::emptyPrimitive;
    }

    // widget explorer
    _widgetExplorer->setEnabled(StyleConfigData::widgetExplorerEnabled());
    _widgetExplorer->setDrawWidgetRects(StyleConfigData::drawWidgetRects());
}

//___________________________________________________________________________________________________________________
QRect Style::pushButtonContentsRect(const QStyleOption *option, const QWidget *) const
{
    return insideMargin(option->rect, Metrics::Frame_FrameWidth);
}

//___________________________________________________________________________________________________________________
QRect Style::checkBoxContentsRect(const QStyleOption *option, const QWidget *) const
{
    return visualRect(option, option->rect.adjusted(Metrics::CheckBox_Size + Metrics::CheckBox_ItemSpacing, 0, 0, 0));
}

//___________________________________________________________________________________________________________________
QRect Style::lineEditContentsRect(const QStyleOption *option, const QWidget *widget) const
{
    // cast option and check
    const auto frameOption(qstyleoption_cast<const QStyleOptionFrame *>(option));
    if (!frameOption) {
        return option->rect;
    }

    // check flatness
    const bool flat(frameOption->lineWidth == 0);
    if (flat) {
        return option->rect;
    }

    // copy rect and take out margins
    auto rect(option->rect);

    // take out margins if there is enough room
    const int frameWidth(pixelMetric(PM_DefaultFrameWidth, option, widget));
    if (rect.height() >= option->fontMetrics.height() + 2 * frameWidth) {
        return insideMargin(rect, frameWidth);
    } else {
        return rect;
    }
}

//___________________________________________________________________________________________________________________
QRect Style::progressBarGrooveRect(const QStyleOption *option, const QWidget *widget) const
{
    // cast option and check
    const auto progressBarOption(qstyleoption_cast<const QStyleOptionProgressBar *>(option));
    if (!progressBarOption) {
        return option->rect;
    }

    // get flags and orientation
    const bool textVisible(progressBarOption->textVisible);
    const bool busy(progressBarOption->minimum == 0 && progressBarOption->maximum == 0);
    const bool horizontal(BreezePrivate::isProgressBarHorizontal(progressBarOption));

    // copy rectangle and adjust
    auto rect(option->rect);
    const int frameWidth(pixelMetric(PM_DefaultFrameWidth, option, widget));
    if (horizontal) {
        rect = insideMargin(rect, frameWidth, 0);
    } else {
        rect = insideMargin(rect, 0, frameWidth);
    }

    if (textVisible && !busy && horizontal) {
        auto textRect(subElementRect(SE_ProgressBarLabel, option, widget));
        textRect = visualRect(option, textRect);
        rect.setRight(textRect.left() - Metrics::ProgressBar_ItemSpacing - 1);
        rect = visualRect(option, rect);
        rect = centerRect(rect, rect.width(), Metrics::ProgressBar_Thickness);

    } else if (horizontal) {
        rect = centerRect(rect, rect.width(), Metrics::ProgressBar_Thickness);

    } else {
        rect = centerRect(rect, Metrics::ProgressBar_Thickness, rect.height());
    }

    return rect;
}

//___________________________________________________________________________________________________________________
QRect Style::progressBarContentsRect(const QStyleOption *option, const QWidget *widget) const
{
    // cast option and check
    const auto progressBarOption(qstyleoption_cast<const QStyleOptionProgressBar *>(option));
    if (!progressBarOption) {
        return QRect();
    }

    // get groove rect
    const auto rect(progressBarGrooveRect(option, widget));

    // in busy mode, grooveRect is used
    const bool busy(progressBarOption->minimum == 0 && progressBarOption->maximum == 0);
    if (busy) {
        return rect;
    }

    // get orientation
    const bool horizontal(BreezePrivate::isProgressBarHorizontal(progressBarOption));

    // check inverted appearance
    bool reverse = (horizontal && (option->direction == Qt::RightToLeft)) || !horizontal;
    if (progressBarOption->invertedAppearance) {
        reverse = !reverse;
    }

    // get progress and steps
    const int progress(progressBarOption->progress - progressBarOption->minimum);
    const int steps(qMax(progressBarOption->maximum - progressBarOption->minimum, 1));

    // Calculate width fraction
    const qreal position = qreal(progress) / qreal(steps);

    // convert the pixel width
    const int indicatorSize(position * (horizontal ? rect.width() : rect.height()));

    QRect indicatorRect;
    if (horizontal) {
        indicatorRect = QRect(rect.left() + (reverse ? rect.width() - indicatorSize : 0), rect.y(), indicatorSize, rect.height());
    } else {
        indicatorRect = QRect(rect.x(), reverse ? rect.top() : (rect.bottom() - indicatorSize + 1), rect.width(), indicatorSize);
    }

    return indicatorRect;
}

//___________________________________________________________________________________________________________________
QRect Style::frameContentsRect(const QStyleOption *option, const QWidget *widget) const
{
    if (widget) {
        const auto borders = widget->property(PropertyNames::bordersSides);
        if (borders.isValid() && borders.canConvert<Qt::Edges>()) {
            const auto value = borders.value<Qt::Edges>();
            auto rect = option->rect;

            if ((value & Qt::LeftEdge && widget->layoutDirection() == Qt::LeftToRight)
                || (value & Qt::RightEdge && widget->layoutDirection() == Qt::RightToLeft)) {
                rect.adjust(1, 0, 0, 0);
            }
            if ((value & Qt::RightEdge && widget->layoutDirection() == Qt::LeftToRight)
                || (value & Qt::LeftEdge && widget->layoutDirection() == Qt::RightToLeft)) {
                rect.adjust(0, 0, -1, 0);
            }
            if (value & Qt::TopEdge) {
                rect.adjust(0, 1, 0, 0);
            }
            if (value & Qt::BottomEdge) {
                rect.adjust(0, 0, 0, -1);
            }

            return rect;
        }
    }

    if (!StyleConfigData::sidePanelDrawFrame() && qobject_cast<const QAbstractScrollArea *>(widget)
        && widget->property(PropertyNames::sidePanelView).toBool()) {
        // adjust margins for sidepanel widgets
        return option->rect.adjusted(0, 0, -1, 0);

    } else {
        // base class implementation
        return ParentStyleClass::subElementRect(SE_FrameContents, option, widget);
    }
}

//___________________________________________________________________________________________________________________
QRect Style::progressBarLabelRect(const QStyleOption *option, const QWidget *) const
{
    // cast option and check
    const auto progressBarOption(qstyleoption_cast<const QStyleOptionProgressBar *>(option));
    if (!progressBarOption) {
        return QRect();
    }

    // get flags and check
    const bool textVisible(progressBarOption->textVisible);
    const bool busy(progressBarOption->minimum == 0 && progressBarOption->maximum == 0);
    if (!textVisible || busy) {
        return QRect();
    }

    // get direction and check
    const bool horizontal(BreezePrivate::isProgressBarHorizontal(progressBarOption));
    if (!horizontal) {
        return QRect();
    }

    int textWidth = qMax(option->fontMetrics.size(_mnemonics->textFlags(), progressBarOption->text).width(),
                         option->fontMetrics.size(_mnemonics->textFlags(), QStringLiteral("100%")).width());

    auto rect(insideMargin(option->rect, Metrics::Frame_FrameWidth, 0));
    rect.setLeft(rect.right() - textWidth + 1);
    rect = visualRect(option, rect);

    return rect;
}

//___________________________________________________________________________________________________________________
QRect Style::headerArrowRect(const QStyleOption *option, const QWidget *) const
{
    // cast option and check
    const auto headerOption(qstyleoption_cast<const QStyleOptionHeader *>(option));
    if (!headerOption) {
        return option->rect;
    }

    // check if arrow is necessary
    if (headerOption->sortIndicator == QStyleOptionHeader::None) {
        return QRect();
    }

    auto arrowRect(insideMargin(option->rect, Metrics::Header_MarginWidth));
    arrowRect.setLeft(arrowRect.right() - Metrics::Header_ArrowSize + 1);

    return visualRect(option, arrowRect);
}

//___________________________________________________________________________________________________________________
QRect Style::headerLabelRect(const QStyleOption *option, const QWidget *) const
{
    // cast option and check
    const auto headerOption(qstyleoption_cast<const QStyleOptionHeader *>(option));
    if (!headerOption) {
        return option->rect;
    }

    // check if arrow is necessary
    auto labelRect(insideMargin(option->rect, Metrics::Header_MarginWidth, 0));
    if (headerOption->sortIndicator == QStyleOptionHeader::None) {
        return labelRect;
    }

    labelRect.adjust(0, 0, -Metrics::Header_ArrowSize - Metrics::Header_ItemSpacing, 0);
    return visualRect(option, labelRect);
}

//____________________________________________________________________
QRect Style::tabBarTabLeftButtonRect(const QStyleOption *option, const QWidget *) const
{
    // cast option and check
    const auto tabOption(qstyleoption_cast<const QStyleOptionTab *>(option));
    if (!tabOption || tabOption->leftButtonSize.isEmpty()) {
        return QRect();
    }

    const auto rect(option->rect);
    const QSize size(tabOption->leftButtonSize);
    QRect buttonRect(QPoint(0, 0), size);

    // vertical positioning
    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        buttonRect.moveLeft(rect.left() + Metrics::TabBar_TabMarginWidth);
        buttonRect.moveTop((rect.height() - buttonRect.height()) / 2);
        buttonRect = visualRect(option, buttonRect);
        break;

    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        buttonRect.moveTop(rect.top() + ((rect.height() - buttonRect.height()) / 2));
        buttonRect.moveLeft((rect.width() - buttonRect.width()) / 2);
        break;

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        buttonRect.moveTop(rect.top() + ((rect.height() - buttonRect.height()) / 2));
        buttonRect.moveLeft((rect.width() - buttonRect.width()) / 2);
        break;

    default:
        break;
    }

    return buttonRect;
}

//____________________________________________________________________
QRect Style::tabBarTabRightButtonRect(const QStyleOption *option, const QWidget *) const
{
    // cast option and check
    const auto tabOption(qstyleoption_cast<const QStyleOptionTab *>(option));
    if (!tabOption || tabOption->rightButtonSize.isEmpty()) {
        return QRect();
    }

    const auto rect(option->rect);
    const auto size(tabOption->rightButtonSize);
    QRect buttonRect(QPoint(0, 0), size);

    // vertical positioning
    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        buttonRect.moveRight(rect.right() - Metrics::TabBar_TabMarginWidth);
        buttonRect.moveTop((rect.height() - buttonRect.height()) / 2);
        buttonRect = visualRect(option, buttonRect);
        break;

    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        buttonRect.moveTop(rect.top() + Metrics::TabBar_TabMarginWidth);
        buttonRect.moveLeft((rect.width() - buttonRect.width()) / 2);
        break;

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        buttonRect.moveBottom(rect.bottom() - Metrics::TabBar_TabMarginWidth);
        buttonRect.moveLeft((rect.width() - buttonRect.width()) / 2);
        break;

    default:
        break;
    }

    return buttonRect;
}

//____________________________________________________________________
QRect Style::tabWidgetTabBarRect(const QStyleOption *option, const QWidget *widget) const
{
    // cast option and check
    const auto tabOption = qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option);
    if (!tabOption) {
        return ParentStyleClass::subElementRect(SE_TabWidgetTabBar, option, widget);
    }

    // do nothing if tabbar is hidden
    const QSize tabBarSize(tabOption->tabBarSize);

    auto rect(option->rect);
    QRect tabBarRect(QPoint(0, 0), tabBarSize);

    Qt::Alignment tabBarAlignment(styleHint(SH_TabBar_Alignment, option, widget));

    // horizontal positioning
    const bool verticalTabs(isVerticalTab(tabOption->shape));
    if (verticalTabs) {
        tabBarRect.setHeight(qMin(tabBarRect.height(), rect.height() - 2));
        if (tabBarAlignment == Qt::AlignCenter) {
            tabBarRect.moveTop(rect.top() + (rect.height() - tabBarRect.height()) / 2);
        } else {
            tabBarRect.moveTop(rect.top() + 1);
        }

    } else {
        // account for corner rects
        // need to re-run visualRect to remove right-to-left handling, since it is re-added on tabBarRect at the end
        const auto leftButtonRect(visualRect(option, subElementRect(SE_TabWidgetLeftCorner, option, widget)));
        const auto rightButtonRect(visualRect(option, subElementRect(SE_TabWidgetRightCorner, option, widget)));

        rect.setLeft(leftButtonRect.width());
        rect.setRight(rightButtonRect.left() - 1);

        tabBarRect.setWidth(qMin(tabBarRect.width(), rect.width() - 2));
        if (tabBarAlignment == Qt::AlignCenter) {
            tabBarRect.moveLeft(rect.left() + (rect.width() - tabBarRect.width()) / 2);
        } else {
            tabBarRect.moveLeft(rect.left() + 1);
        }

        tabBarRect = visualRect(option, tabBarRect);
    }

    // expand the tab bar towards the frame to cover the frame's border
    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        tabBarRect.moveTop(rect.top());
        tabBarRect.setBottom(tabBarRect.bottom() + 1);
        break;

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        tabBarRect.moveBottom(rect.bottom());
        tabBarRect.setTop(tabBarRect.top() - 1);
        break;

    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        tabBarRect.moveLeft(rect.left());
        tabBarRect.setRight(tabBarRect.right() + 1);
        break;

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        tabBarRect.moveRight(rect.right());
        tabBarRect.setLeft(tabBarRect.left() - 1);
        break;

    default:
        break;
    }

    return tabBarRect;
}

//____________________________________________________________________
QRect Style::tabWidgetTabContentsRect(const QStyleOption *option, const QWidget *widget) const
{
    // cast option and check
    const auto tabOption = qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option);
    if (!tabOption) {
        return option->rect;
    }

    // do nothing if tabbar is hidden
    if (tabOption->tabBarSize.isEmpty()) {
        return option->rect;
    }
    const auto rect = tabWidgetTabPaneRect(option, widget);

    const bool documentMode(tabOption->lineWidth == 0);
    if (documentMode) {
        // add margin only to the relevant side
        switch (tabOption->shape) {
        case QTabBar::RoundedNorth:
        case QTabBar::TriangularNorth:
            return rect.adjusted(0, Metrics::TabWidget_MarginWidth, 0, 0);

        case QTabBar::RoundedSouth:
        case QTabBar::TriangularSouth:
            return rect.adjusted(0, 0, 0, -Metrics::TabWidget_MarginWidth);

        case QTabBar::RoundedWest:
        case QTabBar::TriangularWest:
            return rect.adjusted(Metrics::TabWidget_MarginWidth, 0, 0, 0);

        case QTabBar::RoundedEast:
        case QTabBar::TriangularEast:
            return rect.adjusted(0, 0, -Metrics::TabWidget_MarginWidth, 0);

        default:
            return rect;
        }

    } else {
        return insideMargin(rect, Metrics::TabWidget_MarginWidth);
    }
}

//____________________________________________________________________
QRect Style::tabWidgetTabPaneRect(const QStyleOption *option, const QWidget *) const
{
    const auto tabOption = qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option);
    if (!tabOption || tabOption->tabBarSize.isEmpty()) {
        return option->rect;
    }

    const int overlap = Metrics::TabBar_BaseOverlap - 1;
    const QSize tabBarSize(tabOption->tabBarSize - QSize(overlap, overlap));

    auto rect(option->rect);
    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        rect.adjust(0, tabBarSize.height(), 0, 0);
        break;

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        rect.adjust(0, 0, 0, -tabBarSize.height());
        break;

    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        rect.adjust(tabBarSize.width(), 0, 0, 0);
        break;

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        rect.adjust(0, 0, -tabBarSize.width(), 0);
        break;

    default:
        return QRect();
    }

    return rect;
}

//____________________________________________________________________
QRect Style::tabWidgetCornerRect(SubElement element, const QStyleOption *option, const QWidget *) const
{
    // cast option and check
    const auto tabOption = qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option);
    if (!tabOption) {
        return option->rect;
    }

    // do nothing if tabbar is hidden
    const QSize tabBarSize(tabOption->tabBarSize);
    if (tabBarSize.isEmpty()) {
        return QRect();
    }

    // do nothing for vertical tabs
    const bool verticalTabs(isVerticalTab(tabOption->shape));
    if (verticalTabs) {
        return QRect();
    }

    const auto rect(option->rect);
    QRect cornerRect;
    switch (element) {
    case SE_TabWidgetLeftCorner:
        cornerRect = QRect(QPoint(0, 0), tabOption->leftCornerWidgetSize);
        cornerRect.moveLeft(rect.left());
        break;

    case SE_TabWidgetRightCorner:
        cornerRect = QRect(QPoint(0, 0), tabOption->rightCornerWidgetSize);
        cornerRect.moveRight(rect.right());
        break;

    default:
        break;
    }

    // expend height to tabBarSize, if needed, to make sure base is properly rendered
    cornerRect.setHeight(qMax(cornerRect.height(), tabBarSize.height() + 1));

    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        cornerRect.moveTop(rect.top());
        break;

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        cornerRect.moveBottom(rect.bottom());
        break;

    default:
        break;
    }

    // return cornerRect;
    cornerRect = visualRect(option, cornerRect);
    return cornerRect;
}

//____________________________________________________________________
QRect Style::toolBoxTabContentsRect(const QStyleOption *option, const QWidget *widget) const
{
    // cast option and check
    const auto toolBoxOption(qstyleoption_cast<const QStyleOptionToolBox *>(option));
    if (!toolBoxOption) {
        return option->rect;
    }

    // copy rect
    const auto &rect(option->rect);

    int contentsWidth(0);
    if (!toolBoxOption->icon.isNull()) {
        const int iconSize(pixelMetric(QStyle::PM_SmallIconSize, option, widget));
        contentsWidth += iconSize;

        if (!toolBoxOption->text.isEmpty()) {
            contentsWidth += Metrics::ToolBox_TabItemSpacing;
        }
    }

    if (!toolBoxOption->text.isEmpty()) {
        const int textWidth = toolBoxOption->fontMetrics.size(_mnemonics->textFlags(), toolBoxOption->text).width();
        contentsWidth += textWidth;
    }

    contentsWidth += 2 * Metrics::ToolBox_TabMarginWidth;
    contentsWidth = qMin(contentsWidth, rect.width());
    contentsWidth = qMax(contentsWidth, int(Metrics::ToolBox_TabMinWidth));
    return centerRect(rect, contentsWidth, rect.height());
}

//____________________________________________________________________
QRect Style::genericLayoutItemRect(const QStyleOption *option, const QWidget *) const
{
    return insideMargin(option->rect, -Metrics::Frame_FrameWidth);
}

//______________________________________________________________
QRect Style::groupBoxSubControlRect(const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    QRect rect = option->rect;
    switch (subControl) {
    case SC_GroupBoxFrame:
        return rect;

    case SC_GroupBoxContents: {
        // cast option and check
        const auto groupBoxOption = qstyleoption_cast<const QStyleOptionGroupBox *>(option);
        if (!groupBoxOption) {
            break;
        }

        // take out frame width
        rect = insideMargin(rect, Metrics::Frame_FrameWidth);

        // get state
        const bool checkable(groupBoxOption->subControls & QStyle::SC_GroupBoxCheckBox);
        const bool emptyText(groupBoxOption->text.isEmpty());

        // calculate title height
        int titleHeight(0);
        if (!emptyText) {
            titleHeight = groupBoxOption->fontMetrics.height();
        }
        if (checkable) {
            titleHeight = qMax(titleHeight, int(Metrics::CheckBox_Size));
        }

        // add margin
        if (titleHeight > 0) {
            titleHeight += 2 * Metrics::GroupBox_TitleMarginWidth;
        }

        rect.adjust(0, titleHeight, 0, 0);
        return rect;
    }

    case SC_GroupBoxCheckBox:
    case SC_GroupBoxLabel: {
        // cast option and check
        const auto groupBoxOption = qstyleoption_cast<const QStyleOptionGroupBox *>(option);
        if (!groupBoxOption) {
            break;
        }

        // take out frame width
        rect = insideMargin(rect, Metrics::Frame_FrameWidth);

        const bool emptyText(groupBoxOption->text.isEmpty());
        const bool checkable(groupBoxOption->subControls & QStyle::SC_GroupBoxCheckBox);

        QFont font = widget ? widget->font() : qApp->font("QGroupBox");
        if (groupBoxOption->features == QStyleOptionFrame::Flat && !(groupBoxOption->subControls & SC_GroupBoxCheckBox)
            && font.isCopyOf(qApp->font("QGroupBox"))) {
            font.setPointSize(font.pointSize() + 1);
            font.setBold(true);
        }
        const QFontMetrics fontMetrics(font);

        // calculate title height
        int titleHeight(0);
        int titleWidth(0);
        if (!emptyText) {
            titleHeight = qMax(titleHeight, fontMetrics.height());
            titleWidth += fontMetrics.size(_mnemonics->textFlags(), groupBoxOption->text).width();
        }

        if (checkable) {
            titleHeight = qMax(titleHeight, int(Metrics::CheckBox_Size));
            titleWidth += Metrics::CheckBox_Size;
            if (!emptyText) {
                titleWidth += Metrics::CheckBox_ItemSpacing;
            }
        }

        // adjust height
        auto titleRect(rect);
        titleRect.setHeight(titleHeight);
        titleRect.translate(0, Metrics::GroupBox_TitleMarginWidth);

        // center
        titleRect = centerRect(titleRect, titleWidth, titleHeight);

        if (subControl == SC_GroupBoxCheckBox) {
            // vertical centering
            titleRect = centerRect(titleRect, titleWidth, Metrics::CheckBox_Size);

            // horizontal positioning
            const QRect subRect(titleRect.topLeft(), QSize(Metrics::CheckBox_Size, titleRect.height()));
            return visualRect(option->direction, titleRect, subRect);

        } else {
            // vertical centering
            QFontMetrics fontMetrics = option->fontMetrics;
            titleRect = centerRect(titleRect, titleWidth, fontMetrics.height());

            // horizontal positioning
            auto subRect(titleRect);
            if (checkable) {
                subRect.adjust(Metrics::CheckBox_Size + Metrics::CheckBox_ItemSpacing, 0, 0, 0);
            }
            return visualRect(option->direction, titleRect, subRect);
        }
    }

    default:
        break;
    }

    return ParentStyleClass::subControlRect(CC_GroupBox, option, subControl, widget);
}

//___________________________________________________________________________________________________________________
QRect Style::toolButtonSubControlRect(const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    // cast option and check
    const auto toolButtonOption = qstyleoption_cast<const QStyleOptionToolButton *>(option);
    if (!toolButtonOption) {
        return ParentStyleClass::subControlRect(CC_ToolButton, option, subControl, widget);
    }

    const auto menuStyle = BreezePrivate::toolButtonMenuArrowStyle(toolButtonOption);

    // store rect
    const auto &rect(option->rect);
    const int menuButtonWidth(Metrics::MenuButton_IndicatorWidth);
    switch (subControl) {
    case SC_ToolButtonMenu: {
        // check features
        if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::None) {
            return QRect();
        }

        auto menuRect(rect);
        if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::InlineSmall) {
            QRect arrowRect(0, 0, Metrics::SmallArrowSize, Metrics::SmallArrowSize);
            arrowRect.moveBottomRight(menuRect.bottomRight() - QPoint(4, 3));
            menuRect = arrowRect;
        } else {
            menuRect.setLeft(rect.right() - menuButtonWidth + 1);
        }

        return visualRect(option, menuRect);
    }

    case SC_ToolButton: {
        if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::SubControl) {
            auto contentsRect(rect);
            contentsRect.setRight(rect.right() - menuButtonWidth);
            return visualRect(option, contentsRect);

        } else {
            return rect;
        }
    }

    default:
        return QRect();
    }
}

//___________________________________________________________________________________________________________________
QRect Style::comboBoxSubControlRect(const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    // cast option and check
    const auto comboBoxOption(qstyleoption_cast<const QStyleOptionComboBox *>(option));
    if (!comboBoxOption) {
        return ParentStyleClass::subControlRect(CC_ComboBox, option, subControl, widget);
    }

    const bool editable(comboBoxOption->editable);
    const bool flat(editable && !comboBoxOption->frame);

    // copy rect
    auto rect(option->rect);

    switch (subControl) {
    case SC_ComboBoxFrame:
        return flat ? rect : QRect();
    case SC_ComboBoxListBoxPopup:
        return rect;

    case SC_ComboBoxArrow: {
        // take out frame width
        if (!flat) {
            rect = insideMargin(rect, Metrics::Frame_FrameWidth);
        }

        QRect arrowRect(rect.right() - Metrics::MenuButton_IndicatorWidth + 1, rect.top(), Metrics::MenuButton_IndicatorWidth, rect.height());

        arrowRect = centerRect(arrowRect, Metrics::MenuButton_IndicatorWidth, Metrics::MenuButton_IndicatorWidth);
        return visualRect(option, arrowRect);
    }

    case SC_ComboBoxEditField: {
        QRect labelRect;
        const int frameWidth(pixelMetric(PM_ComboBoxFrameWidth, option, widget));
        labelRect = QRect(rect.left(), rect.top(), rect.width() - Metrics::MenuButton_IndicatorWidth, rect.height());

        // remove margins
        if (!flat && rect.height() >= option->fontMetrics.height() + 2 * frameWidth) {
            labelRect.adjust(frameWidth, frameWidth, 0, -frameWidth);
        }

        return visualRect(option, labelRect);
    }

    default:
        break;
    }

    return ParentStyleClass::subControlRect(CC_ComboBox, option, subControl, widget);
}

//___________________________________________________________________________________________________________________
QRect Style::spinBoxSubControlRect(const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    // cast option and check
    const auto spinBoxOption(qstyleoption_cast<const QStyleOptionSpinBox *>(option));
    if (!spinBoxOption) {
        return ParentStyleClass::subControlRect(CC_SpinBox, option, subControl, widget);
    }
    const bool flat(!spinBoxOption->frame);

    // copy rect
    auto rect(option->rect);

    switch (subControl) {
    case SC_SpinBoxFrame:
        return flat ? QRect() : rect;

    case SC_SpinBoxUp:
    case SC_SpinBoxDown: {
        // take out frame width
        if (!flat && rect.height() >= 2 * Metrics::Frame_FrameWidth + Metrics::SpinBox_ArrowButtonWidth) {
            rect = insideMargin(rect, Metrics::Frame_FrameWidth);
        }

        QRect arrowRect;
        arrowRect = QRect(rect.right() - Metrics::SpinBox_ArrowButtonWidth + 1, rect.top(), Metrics::SpinBox_ArrowButtonWidth, rect.height());

        const int arrowHeight(qMin(rect.height(), int(Metrics::SpinBox_ArrowButtonWidth)));
        arrowRect = centerRect(arrowRect, Metrics::SpinBox_ArrowButtonWidth, arrowHeight);
        arrowRect.setHeight(arrowHeight / 2);
        if (subControl == SC_SpinBoxDown) {
            arrowRect.translate(0, arrowHeight / 2);
        }

        return visualRect(option, arrowRect);
    }

    case SC_SpinBoxEditField: {
        const bool showButtons = spinBoxOption->buttonSymbols != QAbstractSpinBox::NoButtons;

        QRect labelRect = rect;
        if (showButtons) {
            labelRect.setRight(rect.right() - Metrics::SpinBox_ArrowButtonWidth);
        }

        // remove right side line editor margins
        const int frameWidth(pixelMetric(PM_SpinBoxFrameWidth, option, widget));
        if (!flat && labelRect.height() >= option->fontMetrics.height() + 2 * frameWidth) {
            labelRect.adjust(frameWidth, frameWidth, showButtons ? 0 : -frameWidth, -frameWidth);
        }

        return visualRect(option, labelRect);
    }

    default:
        break;
    }

    return ParentStyleClass::subControlRect(CC_SpinBox, option, subControl, widget);
}

//___________________________________________________________________________________________________________________
QRect Style::scrollBarInternalSubControlRect(const QStyleOptionComplex *option, SubControl subControl) const
{
    const auto &rect = option->rect;
    const State &state(option->state);
    const bool horizontal(state & State_Horizontal);

    switch (subControl) {
    case SC_ScrollBarSubLine: {
        int majorSize(scrollBarButtonHeight(_subLineButtons));
        if (horizontal) {
            return visualRect(option, QRect(rect.left(), rect.top(), majorSize, rect.height()));
        } else {
            return visualRect(option, QRect(rect.left(), rect.top(), rect.width(), majorSize));
        }
    }

    case SC_ScrollBarAddLine: {
        int majorSize(scrollBarButtonHeight(_addLineButtons));
        if (horizontal) {
            return visualRect(option, QRect(rect.right() - majorSize + 1, rect.top(), majorSize, rect.height()));
        } else {
            return visualRect(option, QRect(rect.left(), rect.bottom() - majorSize + 1, rect.width(), majorSize));
        }
    }

    default:
        return QRect();
    }
}

//___________________________________________________________________________________________________________________
QRect Style::scrollBarSubControlRect(const QStyleOptionComplex *option,
                                     SubControl subControl,
                                     const QWidget *widget,
                                     OverrideAutoHideArrows overrideHideSubLine,
                                     OverrideAutoHideArrows overrideHideAddLine) const
{
    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return ParentStyleClass::subControlRect(CC_ScrollBar, option, subControl, widget);
    }

    // get relevant state
    const State &state(option->state);
    const bool horizontal(state & State_Horizontal);
    const bool autoHideArrows(shouldAutoHideArrows(widget));

    switch (subControl) {
    case SC_ScrollBarSubLine:
    case SC_ScrollBarAddLine:
        return scrollBarInternalSubControlRect(option, subControl);

    case SC_ScrollBarGroove: {
        // bools marking whether to auto-hide the arrows
        bool enlargeOverSubLine = (sliderOption->minimum == sliderOption->maximum) || overrideHideSubLine == OverrideAutoHideArrows::HideArrow
            || (autoHideArrows && overrideHideSubLine != OverrideAutoHideArrows::ShowArrow && (sliderOption->sliderPosition <= (sliderOption->minimum)));
        bool enlargeOverAddLine = (sliderOption->minimum == sliderOption->maximum) || overrideHideAddLine == OverrideAutoHideArrows::HideArrow
            || (autoHideArrows && overrideHideAddLine != OverrideAutoHideArrows::ShowArrow && (sliderOption->sliderPosition >= (sliderOption->maximum)));

        QPoint topLeftCorner;
        QPoint botRightCorner;

        QRect subLineRect = visualRect(option, scrollBarInternalSubControlRect(option, SC_ScrollBarSubLine));
        QRect addLineRect = visualRect(option, scrollBarInternalSubControlRect(option, SC_ScrollBarAddLine));

        if (horizontal) {
            if (_subLineButtons == ScrollBarButtonType::NoButton) {
                topLeftCorner = QPoint(subLineRect.left() + Metrics::ScrollBar_TopBottomMargins, subLineRect.top());
            } else if (_subLineButtons == ScrollBarButtonType::SingleButton) {
                if (enlargeOverSubLine)
                    topLeftCorner = QPoint(subLineRect.left() + Metrics::ScrollBar_TopBottomMargins, subLineRect.top());
                else
                    topLeftCorner = QPoint(subLineRect.right() + 1 + StyleConfigData::scrollBarTopOneButtonSpacing(), subLineRect.top());
            } else if (_subLineButtons == ScrollBarButtonType::DoubleButton) {
                topLeftCorner = QPoint(subLineRect.right() + 1 + StyleConfigData::scrollBarTopTwoButtonSpacing(), subLineRect.top());
            }

            if (_addLineButtons == ScrollBarButtonType::NoButton) {
                botRightCorner = QPoint(addLineRect.right() + 1 - Metrics::ScrollBar_TopBottomMargins, subLineRect.bottom() + 1);
            } else if (_addLineButtons == ScrollBarButtonType::SingleButton) {
                if (enlargeOverAddLine)
                    botRightCorner = QPoint(addLineRect.right() + 1 - Metrics::ScrollBar_TopBottomMargins, subLineRect.bottom() + 1);
                else
                    botRightCorner = QPoint(addLineRect.left() - StyleConfigData::scrollBarBottomOneButtonSpacing(), subLineRect.bottom() + 1);
            } else if (_addLineButtons == ScrollBarButtonType::DoubleButton) {
                botRightCorner = QPoint(addLineRect.left() - StyleConfigData::scrollBarBottomTwoButtonSpacing(), subLineRect.bottom() + 1);
            }

        } else {
            if (_subLineButtons == ScrollBarButtonType::NoButton) {
                topLeftCorner = QPoint(subLineRect.left(), subLineRect.top() + Metrics::ScrollBar_TopBottomMargins);
            } else if (_subLineButtons == ScrollBarButtonType::SingleButton) {
                if (enlargeOverSubLine)
                    topLeftCorner = QPoint(subLineRect.left(), subLineRect.top() + Metrics::ScrollBar_TopBottomMargins);
                else
                    topLeftCorner = QPoint(subLineRect.left(), subLineRect.bottom() + 1 + StyleConfigData::scrollBarTopOneButtonSpacing());
            } else if (_subLineButtons == ScrollBarButtonType::DoubleButton) {
                topLeftCorner = QPoint(subLineRect.left(), subLineRect.bottom() + 1 + StyleConfigData::scrollBarTopTwoButtonSpacing());
            }

            if (_addLineButtons == ScrollBarButtonType::NoButton) {
                botRightCorner = QPoint(subLineRect.right() + 1, addLineRect.bottom() + 1 - Metrics::ScrollBar_TopBottomMargins);
            } else if (_addLineButtons == ScrollBarButtonType::SingleButton) {
                if (enlargeOverAddLine)
                    botRightCorner = QPoint(subLineRect.right() + 1, addLineRect.bottom() + 1 - Metrics::ScrollBar_TopBottomMargins);
                else
                    botRightCorner = QPoint(subLineRect.right() + 1, addLineRect.top() - StyleConfigData::scrollBarBottomOneButtonSpacing());
            } else if (_addLineButtons == ScrollBarButtonType::DoubleButton) {
                botRightCorner = QPoint(subLineRect.right() + 1, addLineRect.top() - StyleConfigData::scrollBarBottomTwoButtonSpacing());
            }
        }

        // define rect
        // all +1s above are due to QRect.bottomRight() not being the true bottomRight
        // bottomRight constructor is also weird in QRect so have to specify -1 for bottomRight point
        return visualRect(option, QRect(topLeftCorner, botRightCorner - QPoint(1, 1)));
    }

    case SC_ScrollBarSlider: {
        // handle RTL here to unreflect things if need be
        auto groove = visualRect(option, scrollBarSubControlRect(option, SC_ScrollBarGroove, widget, overrideHideSubLine, overrideHideAddLine));

        if (sliderOption->minimum == sliderOption->maximum) {
            return groove;
        }

        // Figure out how much room there is
        int space(horizontal ? groove.width() : groove.height());

        // Calculate the portion of this space that the slider should occupy
        int sliderSize = space * qreal(sliderOption->pageStep) / (sliderOption->maximum - sliderOption->minimum + sliderOption->pageStep);
        sliderSize = qMax(sliderSize, static_cast<int>(Metrics::ScrollBar_MinSliderHeight));
        sliderSize = qMin(sliderSize, space);

        space -= sliderSize;
        if (space <= 0) {
            return groove;
        }

        int pos = qRound(qreal(sliderOption->sliderPosition - sliderOption->minimum) / (sliderOption->maximum - sliderOption->minimum) * space);
        if (sliderOption->upsideDown)
            pos = space - pos;

        // return the slider rect, accounting for any enlargement of slider at extremities
        if (horizontal) {
            return visualRect(option, QRect(groove.left() + pos, groove.top(), sliderSize, groove.height()));

        } else {
            return visualRect(option, QRect(groove.left(), groove.top() + pos, groove.width(), sliderSize));
        }
    }

    case SC_ScrollBarSubPage: {
        // handle RTL here to unreflect things if need be
        auto slider = visualRect(option, subControlRect(CC_ScrollBar, option, SC_ScrollBarSlider, widget));
        auto groove = visualRect(option, subControlRect(CC_ScrollBar, option, SC_ScrollBarGroove, widget));

        if (horizontal) {
            return visualRect(option, QRect(groove.left(), groove.top(), slider.left() - groove.left(), groove.height()));
        } else {
            return visualRect(option, QRect(groove.left(), groove.top(), groove.width(), slider.top() - groove.top()));
        }
    }

    case SC_ScrollBarAddPage: {
        // handle RTL here to unreflect things if need be
        auto slider = visualRect(option, subControlRect(CC_ScrollBar, option, SC_ScrollBarSlider, widget));
        auto groove = visualRect(option, subControlRect(CC_ScrollBar, option, SC_ScrollBarGroove, widget));

        if (horizontal)
            return visualRect(option, QRect(slider.right() + 1, groove.top(), groove.right() - slider.right(), groove.height()));
        else
            return visualRect(option, QRect(groove.left(), slider.bottom() + 1, groove.width(), groove.bottom() - slider.bottom()));
    }

    default:
        return ParentStyleClass::subControlRect(CC_ScrollBar, option, subControl, widget);
    }
}

// there are some edge cases where extending the scrollBar slider will look wrong such as the Kate/KDevelop ScrollBar Minimap
// Also in LibreOffice, auto-hiding does not work -- LibreOffice uses a null widget
// do not auto-hide arrows with slider extension in these cases
bool Style::scrollBarAutoHideArrowsException(const QWidget *widget) const
{
    bool exception = false;

    if (widget) {
        QList<const char *> exceptionClassNames = {"KateScrollBar"};
        for (int i = 0; i < exceptionClassNames.size(); i++) {
            if (widget->inherits(exceptionClassNames[i])) {
                exception = true;
                break;
            }
        }
    }

    return exception;
}

bool Style::shouldAutoHideArrows(const QWidget *widget) const
{
    if (StyleConfigData::animationsEnabled() && StyleConfigData::scrollBarAutoHideArrows() && _subLineButtons == ScrollBarButtonType::SingleButton
        && _addLineButtons == ScrollBarButtonType::SingleButton && !scrollBarAutoHideArrowsException(widget))
        return true;
    else
        return false;
}

//___________________________________________________________________________________________________________________
QRect Style::dialSubControlRect(const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return ParentStyleClass::subControlRect(CC_Dial, option, subControl, widget);
    }

    // adjust rect to be square, and centered
    auto rect(option->rect);
    const int dimension(qMin(rect.width(), rect.height()));
    rect = centerRect(rect, dimension, dimension);

    switch (subControl) {
    case QStyle::SC_DialGroove:
        return insideMargin(rect, (Metrics::Slider_ControlThickness - Metrics::Slider_GrooveThickness) / 2);
    case QStyle::SC_DialHandle: {
        // calculate angle at which handle needs to be drawn
        const qreal angle(dialAngle(sliderOption, sliderOption->sliderPosition));

        // groove rect
        const QRectF grooveRect(insideMargin(rect, Metrics::Slider_ControlThickness / 2));
        qreal radius(grooveRect.width() / 2);

        // slider center
        QPointF center(grooveRect.center() + QPointF(radius * std::cos(angle), -radius * std::sin(angle)));

        // slider rect
        QRect handleRect(0, 0, Metrics::Slider_ControlThickness, Metrics::Slider_ControlThickness);
        handleRect.moveCenter(center.toPoint());
        return handleRect;
    }

    default:
        return ParentStyleClass::subControlRect(CC_Dial, option, subControl, widget);
    }
}

//___________________________________________________________________________________________________________________
QRect Style::sliderSubControlRect(const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return ParentStyleClass::subControlRect(CC_Slider, option, subControl, widget);
    }

    // direction
    const bool horizontal(sliderOption->orientation == Qt::Horizontal);

    // somewhat a copy-pasta of QCommonStyle::subControlRect, except we want
    // to account for our specific tick marks, and perform centering.
    auto rect(sliderRectWithoutTickMarks(sliderOption));

    switch (subControl) {
    case SC_SliderHandle: {
        QRect ret(centerRect(rect, Metrics::Slider_ControlThickness, Metrics::Slider_ControlThickness));
        constexpr int len = Metrics::Slider_ControlThickness;
        const int sliderPos = sliderPositionFromValue(sliderOption->minimum,
                                                      sliderOption->maximum,
                                                      sliderOption->sliderPosition,
                                                      (horizontal ? rect.width() : rect.height()) - len,
                                                      sliderOption->upsideDown);
        if (horizontal) {
            ret.moveLeft(rect.x() + sliderPos);
        } else {
            ret.moveTop(rect.y() + sliderPos);
        }
        ret = visualRect(option->direction, rect, ret);
        return ret;
    }

    case SC_SliderGroove: {
        auto grooveRect = insideMargin(rect, pixelMetric(PM_DefaultFrameWidth, option, widget));

        // centering
        if (horizontal) {
            grooveRect = centerRect(rect, grooveRect.width(), Metrics::Slider_GrooveThickness);
        } else {
            grooveRect = centerRect(rect, Metrics::Slider_GrooveThickness, grooveRect.height());
        }
        return grooveRect;
    }

    default:
        return ParentStyleClass::subControlRect(CC_Slider, option, subControl, widget);
    }
}

//______________________________________________________________
QSize Style::checkBoxSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *) const
{
    // get contents size
    QSize size(contentsSize);

    // add focus height
    size = expandSize(size, 0, Metrics::CheckBox_FocusMarginWidth);

    // make sure there is enough height for indicator
    size.setHeight(qMax(size.height(), int(Metrics::CheckBox_Size)));

    // Add space for the indicator
    size.rwidth() += Metrics::CheckBox_Size;

    auto buttonOption = qstyleoption_cast<const QStyleOptionButton *>(option);
    if (!buttonOption) {
        return size;
    }

    if (!buttonOption->icon.isNull()) {
        // Add space for the icon
        size.rwidth() += Metrics::CheckBox_ItemSpacing;
    }

    if (!buttonOption->text.isEmpty()) {
        // also add extra space, to leave room to the right of the label
        size.rwidth() += Metrics::CheckBox_ItemSpacing;
    }

    return size;
}

//______________________________________________________________
QSize Style::lineEditSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    // cast option and check
    const auto frameOption(qstyleoption_cast<const QStyleOptionFrame *>(option));
    if (!frameOption) {
        return contentsSize;
    }

    const bool flat(frameOption->lineWidth == 0);
    const int frameWidth(pixelMetric(PM_DefaultFrameWidth, option, widget));
    return flat ? contentsSize : expandSize(contentsSize, frameWidth);
}

//______________________________________________________________
QSize Style::comboBoxSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    // cast option and check
    const auto comboBoxOption(qstyleoption_cast<const QStyleOptionComboBox *>(option));
    if (!comboBoxOption) {
        return contentsSize;
    }

    // copy size
    QSize size(contentsSize);

    // make sure there is enough height for the button
    size.setHeight(qMax(size.height(), int(Metrics::MenuButton_IndicatorWidth)));

    // add relevant margin
    const int frameWidth(pixelMetric(PM_ComboBoxFrameWidth, option, widget));
    size = expandSize(size, frameWidth);

    // add button width and spacing
    size.rwidth() += Metrics::MenuButton_IndicatorWidth + 2;
    size.rwidth() += Metrics::Button_ItemSpacing;

    return size;
}

//______________________________________________________________
QSize Style::spinBoxSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    // cast option and check
    const auto spinBoxOption(qstyleoption_cast<const QStyleOptionSpinBox *>(option));
    if (!spinBoxOption) {
        return contentsSize;
    }

    const bool flat(!spinBoxOption->frame);

    // copy size
    QSize size(contentsSize);

    // add editor margins
    const int frameWidth(pixelMetric(PM_SpinBoxFrameWidth, option, widget));
    if (!flat) {
        size = expandSize(size, frameWidth);
    }

    // make sure there is enough height for the button
    size.setHeight(qMax(size.height(), int(Metrics::SpinBox_ArrowButtonWidth)));

    // add button width and spacing
    const bool showButtons = spinBoxOption->buttonSymbols != QAbstractSpinBox::NoButtons;
    if (showButtons) {
        size.rwidth() += Metrics::SpinBox_ArrowButtonWidth;
    }

    return size;
}

int Style::sliderTickMarksLength()
{
    const bool disableTicks(!StyleConfigData::sliderDrawTickMarks());

    /*
     * Qt adds its own tick length directly inside QSlider.
     * Take it out and replace by ours, if needed
     */
    const int tickLength(disableTicks ? 0
                                      : (Metrics::Slider_TickLength + Metrics::Slider_TickMarginWidth
                                         + (Metrics::Slider_GrooveThickness - Metrics::Slider_ControlThickness) / 2));
    constexpr int builtInTickLength(5);
    return tickLength - builtInTickLength;
}

QRect Style::sliderRectWithoutTickMarks(const QStyleOptionSlider *option)
{
    // store tick position and orientation
    const QSlider::TickPosition tickPosition(option->tickPosition);
    const bool horizontal(option->orientation == Qt::Horizontal);
    const int tick = sliderTickMarksLength();

    auto rect(option->rect);

    if (horizontal) {
        if (tickPosition & QSlider::TicksAbove) {
            rect.setTop(-tick);
        }
        if (tickPosition & QSlider::TicksBelow) {
            rect.setBottom(rect.bottom() + tick);
        }
    } else {
        if (tickPosition & QSlider::TicksAbove) {
            rect.setLeft(-tick);
        }
        if (tickPosition & QSlider::TicksBelow) {
            rect.setRight(rect.right() + tick);
        }
    }

    return rect;
}

//______________________________________________________________
QSize Style::sliderSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *) const
{
    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return contentsSize;
    }

    // store tick position and orientation
    const QSlider::TickPosition tickPosition(sliderOption->tickPosition);
    const bool horizontal(sliderOption->orientation == Qt::Horizontal);
    const int tick = sliderTickMarksLength();

    // do nothing if no ticks are requested
    if (tickPosition == QSlider::NoTicks) {
        return contentsSize;
    }

    QSize size(contentsSize);
    if (horizontal) {
        if (tickPosition & QSlider::TicksAbove) {
            size.rheight() += tick;
        }
        if (tickPosition & QSlider::TicksBelow) {
            size.rheight() += tick;
        }
    } else {
        if (tickPosition & QSlider::TicksAbove) {
            size.rwidth() += tick;
        }
        if (tickPosition & QSlider::TicksBelow) {
            size.rwidth() += tick;
        }
    }

    return size;
}

//______________________________________________________________
QSize Style::pushButtonSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    // cast option and check
    const auto buttonOption(qstyleoption_cast<const QStyleOptionButton *>(option));
    if (!buttonOption) {
        return contentsSize;
    }

    // output
    QSize size;

    // check text and icon
    const bool hasText(!buttonOption->text.isEmpty());
    const bool flat(buttonOption->features & QStyleOptionButton::Flat);
    bool hasIcon(!buttonOption->icon.isNull());

    if (!(hasText || hasIcon)) {
        /*
        no text nor icon is passed.
        assume custom button and use contentsSize as a starting point
        */
        size = contentsSize;

    } else {
        /*
        rather than trying to guess what Qt puts into its contents size calculation,
        we recompute the button size entirely, based on button option
        this ensures consistency with the rendering stage
        */

        // update has icon to honour showIconsOnPushButtons, when possible
        hasIcon &= (showIconsOnPushButtons() || flat || !hasText);

        // text
        if (hasText) {
            const int textFlags(_mnemonics->textFlags() | Qt::AlignCenter);
            size = option->fontMetrics.size(textFlags, buttonOption->text);
        }

        // icon
        if (hasIcon) {
            QSize iconSize = buttonOption->iconSize;
            if (!iconSize.isValid()) {
                iconSize = QSize(pixelMetric(PM_SmallIconSize, option, widget), pixelMetric(PM_SmallIconSize, option, widget));
            }

            size.setHeight(qMax(size.height(), iconSize.height()));
            size.rwidth() += iconSize.width();

            if (hasText) {
                size.rwidth() += Metrics::Button_ItemSpacing;
            }
        }
    }

    // menu
    const bool hasMenu(buttonOption->features & QStyleOptionButton::HasMenu);
    if (hasMenu) {
        size.rwidth() += Metrics::MenuButton_IndicatorWidth;
        if (hasText || hasIcon) {
            size.rwidth() += Metrics::Button_ItemSpacing;
        }
    }

    // expand with buttons margin
    size = expandSize(size, Metrics::Button_MarginWidth);

    // make sure buttons have a minimum width
    if (hasText) {
        size.setWidth(qMax(size.width(), int(Metrics::Button_MinWidth)));
    }

    // finally add frame margins
    return expandSize(size, Metrics::Frame_FrameWidth);
}

//______________________________________________________________
QSize Style::toolButtonSizeFromContents(const QStyleOption *option, const QSize &contentsSize, [[maybe_unused]] const QWidget *widget) const
{
    // cast option and check
    const auto toolButtonOption = qstyleoption_cast<const QStyleOptionToolButton *>(option);
    if (!toolButtonOption) {
        return contentsSize;
    }

    // copy size
    QSize size = contentsSize;

    // get relevant state flags
    const State &state(option->state);
    const bool autoRaise(state & State_AutoRaise);

    const auto menuStyle = BreezePrivate::toolButtonMenuArrowStyle(toolButtonOption);
    if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::InlineLarge) {
        size.rwidth() += Metrics::MenuButton_IndicatorWidth;
    }

    const int marginWidth(autoRaise ? Metrics::ToolButton_MarginWidth : Metrics::Button_MarginWidth + Metrics::Frame_FrameWidth);

    size = expandSize(size, marginWidth);

    return size;
}

//______________________________________________________________
QSize Style::menuBarItemSizeFromContents(const QStyleOption *, const QSize &contentsSize, const QWidget *) const
{
    return expandSize(contentsSize, Metrics::MenuBarItem_MarginWidth, Metrics::MenuBarItem_MarginHeight);
}

//______________________________________________________________
QSize Style::menuItemSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    // cast option and check
    const auto menuItemOption = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (!menuItemOption) {
        return contentsSize;
    }

    /*
     * First calculate the intrinsic size of the item.
     * this must be kept consistent with what's in drawMenuItemControl
     */
    switch (menuItemOption->menuItemType) {
    case QStyleOptionMenuItem::Normal:
    case QStyleOptionMenuItem::DefaultItem:
    case QStyleOptionMenuItem::SubMenu: {
        QString text = menuItemOption->text;
        qsizetype acceleratorSeparatorPos = text.indexOf(QLatin1Char('\t'));
        const bool hasAccelerator = acceleratorSeparatorPos >= 0;
        if (hasAccelerator) {
            text = text.left(acceleratorSeparatorPos);
        }

        QFontMetrics fm(menuItemOption->font);
        QSize size = fm.boundingRect({}, Qt::TextHideMnemonic, text).size();

        int iconWidth = 0;
        if (showIconsInMenuItems()) {
            iconWidth = isQtQuickControl(option, widget) ? qMax(pixelMetric(PM_SmallIconSize, option, widget), menuItemOption->maxIconWidth)
                                                         : menuItemOption->maxIconWidth;
        }

        int leftColumnWidth = 0;

        // add icon width
        if (iconWidth > 0) {
            leftColumnWidth += iconWidth + Metrics::MenuItem_ItemSpacing;
        }

        // add checkbox indicator width
        if (menuItemOption->menuHasCheckableItems) {
            leftColumnWidth += Metrics::CheckBox_Size + Metrics::MenuItem_ItemSpacing;
        }

        // add spacing for accelerator
        /*
         * Note:
         * The width of the accelerator itself is not included here since
         * Qt will add that on separately after obtaining the
         * sizeFromContents() for each menu item in the menu to be shown
         * ( see QMenuPrivate::calcActionRects() )
         */
        if (hasAccelerator) {
            size.rwidth() += Metrics::MenuItem_AcceleratorSpace;
        }

        // right column
        const int rightColumnWidth = Metrics::MenuButton_IndicatorWidth + Metrics::MenuItem_ItemSpacing;
        size.rwidth() += leftColumnWidth + rightColumnWidth;

        // make sure height is large enough for icon and arrow
        size.setHeight(qMax(size.height(), int(Metrics::MenuButton_IndicatorWidth)));
        size.setHeight(qMax(size.height(), int(Metrics::CheckBox_Size)));
        size.setHeight(qMax(size.height(), iconWidth));
        return expandSize(size, Metrics::MenuItem_MarginWidth, (isTabletMode() ? 2 : 1) * Metrics::MenuItem_MarginHeight);
    }

    case QStyleOptionMenuItem::Separator: {
        // contentsSize for separators in QMenuPrivate::updateActionRects() is {2,2}
        // We choose to override that.
        // Have at least 1px for separator line.
        int w = 1;
        int h = 1;

        // If the menu item is a section, add width for text
        // and make height the same as other menu items, plus extra top padding.
        if (!menuItemOption->text.isEmpty()) {
            auto font = menuItemOption->font;
            font.setBold(true);
            QFontMetrics fm(font);
            QRect textRect = fm.boundingRect({}, Qt::TextSingleLine | Qt::TextHideMnemonic, menuItemOption->text);
            w = qMax(w, textRect.width());
            h = qMax(h, fm.height());

            if (showIconsInMenuItems()) {
                int iconWidth = menuItemOption->maxIconWidth;
                if (isQtQuickControl(option, widget)) {
                    iconWidth = qMax(pixelMetric(PM_SmallIconSize, option, widget), iconWidth);
                }
                h = qMax(h, iconWidth);
            }

            if (menuItemOption->menuHasCheckableItems) {
                h = qMax(h, Metrics::CheckBox_Size);
            }

            h = qMax(h, Metrics::MenuButton_IndicatorWidth);
            h += Metrics::MenuItem_MarginHeight; // extra top padding
        }

        return {w + Metrics::MenuItem_MarginWidth * 2, h + Metrics::MenuItem_MarginHeight * 2};
    }

    // for all other cases, return input
    default:
        return contentsSize;
    }
}

//______________________________________________________________
QSize Style::progressBarSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *) const
{
    // cast option
    const auto progressBarOption(qstyleoption_cast<const QStyleOptionProgressBar *>(option));
    if (!progressBarOption) {
        return contentsSize;
    }

    const bool horizontal(BreezePrivate::isProgressBarHorizontal(progressBarOption));

    // make local copy
    QSize size(contentsSize);

    if (horizontal) {
        // check text visibility
        const bool textVisible(progressBarOption->textVisible);

        size.setWidth(qMax(size.width(), int(Metrics::ProgressBar_Thickness)));
        size.setHeight(qMax(size.height(), int(Metrics::ProgressBar_Thickness)));
        if (textVisible) {
            size.setHeight(qMax(size.height(), option->fontMetrics.height()));
        }

    } else {
        size.setHeight(qMax(size.height(), int(Metrics::ProgressBar_Thickness)));
        size.setWidth(qMax(size.width(), int(Metrics::ProgressBar_Thickness)));
    }

    return size;
}

//______________________________________________________________
QSize Style::tabWidgetSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    // cast option and check
    const auto tabOption = qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option);
    if (!tabOption) {
        return expandSize(contentsSize, Metrics::TabWidget_MarginWidth);
    }

    // try to find direct children of type QTabBar and QStackedWidget
    // this is needed in order to add TabWidget margins only if they are necessary around tabWidget content, not the tabbar
    if (!widget) {
        return expandSize(contentsSize, Metrics::TabWidget_MarginWidth);
    }
    QTabBar *tabBar = nullptr;
    QStackedWidget *stack = nullptr;
    const auto children(widget->children());
    for (auto child : children) {
        if (!tabBar) {
            tabBar = qobject_cast<QTabBar *>(child);
        }
        if (!stack) {
            stack = qobject_cast<QStackedWidget *>(child);
        }
        if (tabBar && stack) {
            break;
        }
    }

    if (!(tabBar && stack)) {
        return expandSize(contentsSize, Metrics::TabWidget_MarginWidth);
    }

    // tab orientation
    const bool verticalTabs(tabOption && isVerticalTab(tabOption->shape));
    if (verticalTabs) {
        const int tabBarHeight = tabBar->minimumSizeHint().height();
        const int stackHeight = stack->minimumSizeHint().height();
        if (contentsSize.height() == tabBarHeight && tabBarHeight + 2 * (Metrics::Frame_FrameWidth - 1) >= stackHeight + 2 * Metrics::TabWidget_MarginWidth) {
            return QSize(contentsSize.width() + 2 * Metrics::TabWidget_MarginWidth, contentsSize.height() + 2 * (Metrics::Frame_FrameWidth - 1));
        } else {
            return expandSize(contentsSize, Metrics::TabWidget_MarginWidth);
        }

    } else {
        const int tabBarWidth = tabBar->minimumSizeHint().width();
        const int stackWidth = stack->minimumSizeHint().width();
        if (contentsSize.width() == tabBarWidth && tabBarWidth + 2 * (Metrics::Frame_FrameWidth - 1) >= stackWidth + 2 * Metrics::TabWidget_MarginWidth) {
            return QSize(contentsSize.width() + 2 * (Metrics::Frame_FrameWidth - 1), contentsSize.height() + 2 * Metrics::TabWidget_MarginWidth);
        } else {
            return expandSize(contentsSize, Metrics::TabWidget_MarginWidth);
        }
    }
}

//______________________________________________________________
QSize Style::tabBarTabSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *) const
{
    const auto tabOption(qstyleoption_cast<const QStyleOptionTab *>(option));
    const bool hasText(tabOption && !tabOption->text.isEmpty());
    const bool hasIcon(tabOption && !tabOption->icon.isNull());
    const bool hasLeftButton(tabOption && !tabOption->leftButtonSize.isEmpty());
    const bool hasRightButton(tabOption && !tabOption->leftButtonSize.isEmpty());

    // calculate width increment for horizontal tabs
    int widthIncrement = 0;
    if (hasIcon && !(hasText || hasLeftButton || hasRightButton)) {
        widthIncrement -= 4;
    }
    if (hasText && hasIcon) {
        widthIncrement += Metrics::TabBar_TabItemSpacing;
    }
    if (hasLeftButton && (hasText || hasIcon)) {
        widthIncrement += Metrics::TabBar_TabItemSpacing;
    }
    if (hasRightButton && (hasText || hasIcon || hasLeftButton)) {
        widthIncrement += Metrics::TabBar_TabItemSpacing;
    }

    // add margins
    QSize size(contentsSize);

    // compare to minimum size
    const bool verticalTabs(tabOption && isVerticalTab(tabOption));
    if (verticalTabs) {
        size.rheight() += widthIncrement;
        if (hasIcon && !hasText) {
            size = size.expandedTo(QSize(Metrics::TabBar_TabMinHeight, 0));
        } else {
            size = size.expandedTo(QSize(Metrics::TabBar_TabMinHeight, Metrics::TabBar_TabMinWidth));
        }

    } else {
        size.rwidth() += widthIncrement;
        if (hasIcon && !hasText) {
            size = size.expandedTo(QSize(0, Metrics::TabBar_TabMinHeight));
        } else {
            size = size.expandedTo(QSize(Metrics::TabBar_TabMinWidth, Metrics::TabBar_TabMinHeight));
        }
    }

    return size;
}

//______________________________________________________________
QSize Style::headerSectionSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *) const
{
    // cast option and check
    const auto headerOption(qstyleoption_cast<const QStyleOptionHeader *>(option));
    if (!headerOption) {
        return contentsSize;
    }

    // get text size
    const bool horizontal(headerOption->orientation == Qt::Horizontal);
    const bool hasText(!headerOption->text.isEmpty());
    const bool hasIcon(!headerOption->icon.isNull());

    const QSize textSize(hasText ? headerOption->fontMetrics.size(0, headerOption->text) : QSize());
    const QSize iconSize(hasIcon ? QSize(22, 22) : QSize());

    // contents width
    int contentsWidth(0);
    if (hasText) {
        contentsWidth += textSize.width();
    }
    if (hasIcon) {
        contentsWidth += iconSize.width();
        if (hasText) {
            contentsWidth += Metrics::Header_ItemSpacing;
        }
    }

    // contents height
    int contentsHeight(hasText ? textSize.height() : headerOption->fontMetrics.height());
    if (hasIcon) {
        contentsHeight = qMax(contentsHeight, iconSize.height());
    }

    if (horizontal && headerOption->sortIndicator != QStyleOptionHeader::None) {
        // also add space for sort indicator
        contentsWidth += Metrics::Header_ArrowSize + Metrics::Header_ItemSpacing;
        contentsHeight = qMax(contentsHeight, int(Metrics::Header_ArrowSize));
    }

    // update contents size, add margins and return
    const QSize size(contentsSize.expandedTo(QSize(contentsWidth, contentsHeight)));
    return expandSize(size, Metrics::Header_MarginWidth);
}

//______________________________________________________________
QSize Style::itemViewItemSizeFromContents(const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    // call base class
    const QSize size(ParentStyleClass::sizeFromContents(CT_ItemViewItem, option, contentsSize, widget));
    return expandSize(size, Metrics::ItemView_ItemMarginWidth);
}

//______________________________________________________________
bool Style::drawFramePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // copy palette and rect
    const auto &palette(option->palette);
    const auto &rect(option->rect);

    // copy state
    const State &state(option->state);
    if (!(state & (State_Sunken | State_Raised))) {
        return true;
    }

    const bool isInputWidget((widget && widget->testAttribute(Qt::WA_Hover))
                             || (isQtQuickControl(option, widget) && option->styleObject->property("elementType").toString() == QStringLiteral("edit")));

    const bool enabled(state & State_Enabled);
    const bool mouseOver(enabled && isInputWidget && (state & State_MouseOver));
    const bool hasFocus(enabled && isInputWidget && (state & State_HasFocus));

    // focus takes precedence over mouse over
    _animations->inputWidgetEngine().updateState(widget, AnimationFocus, hasFocus);
    _animations->inputWidgetEngine().updateState(widget, AnimationHover, mouseOver && !hasFocus);

    // retrieve animation mode and opacity
    const AnimationMode mode(_animations->inputWidgetEngine().frameAnimationMode(widget));
    const qreal opacity(_animations->inputWidgetEngine().frameOpacity(widget));

    if (widget && widget->property(PropertyNames::bordersSides).isValid()) {
        const auto background(palette.base().color());
        const auto outline(_helper->frameOutlineColor(palette));
        _helper->renderFrameWithSides(painter, rect, background, widget->property(PropertyNames::bordersSides).value<Qt::Edges>(), outline);

        return true;
    }

    // render
    if (!StyleConfigData::sidePanelDrawFrame() && widget && widget->property(PropertyNames::sidePanelView).toBool()) {
        const auto outline(_helper->sidePanelOutlineColor(palette, hasFocus, opacity, mode));
        const bool reverseLayout(option->direction == Qt::RightToLeft);
        const Side side(reverseLayout ? SideRight : SideLeft);
        _helper->renderSidePanelFrame(painter, rect, outline, side);

    } else {
        if (_frameShadowFactory->isRegistered(widget)) {
            // update frame shadow factory
            _frameShadowFactory->updateShadowsGeometry(widget, rect);
            _frameShadowFactory->updateState(widget, hasFocus, mouseOver, opacity, mode);
        }

        const auto background(palette.base().color());
        const auto outline(_helper->frameOutlineColor(palette, mouseOver, hasFocus, opacity, mode));
        _helper->renderFrame(painter, rect, background, outline);
    }

    return true;
}

//______________________________________________________________
bool Style::drawFrameLineEditPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // copy palette and rect
    const auto &palette(option->palette);
    const auto &rect(option->rect);

    if (widget) {
        const auto borders = widget->property(PropertyNames::bordersSides);
        if (borders.isValid()) {
            const auto value = borders.value<Qt::Edges>();

            // copy state
            const State &state(option->state);
            const bool enabled(state & State_Enabled);
            const bool mouseOver(enabled && (state & State_MouseOver));
            const bool hasFocus(enabled && (state & State_HasFocus));

            // Draw background
            const auto &background = palette.color(QPalette::Base);
            painter->setPen(Qt::NoPen);
            painter->setBrush(background);
            painter->drawRect(rect);

            // Draw focus frame
            if (mouseOver || hasFocus) {
                // Retrieve animation mode and opacity
                const AnimationMode mode(_animations->inputWidgetEngine().frameAnimationMode(widget));
                const qreal opacity(_animations->inputWidgetEngine().frameOpacity(widget));

                const auto outline(hasHighlightNeutral(widget, option, mouseOver, hasFocus)
                                       ? _helper->neutralText(palette)
                                       : _helper->frameOutlineColor(palette, mouseOver, hasFocus, opacity, mode));

                auto outlineRect = rect.adjusted(0, 0, -1, -1);
                if (value & Qt::LeftEdge) {
                    outlineRect = outlineRect.adjusted(1, 0, 0, 0);
                }
                if (value & Qt::TopEdge) {
                    outlineRect = outlineRect.adjusted(0, 1, 0, 0);
                }
                if (value & Qt::RightEdge) {
                    outlineRect = outlineRect.adjusted(0, 0, -1, 0);
                }
                if (value & Qt::BottomEdge) {
                    outlineRect = outlineRect.adjusted(0, 0, 0, -1);
                }

                painter->setPen(outline);
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(outlineRect);
            }

            // Draw border
            const auto borderColor = _helper->frameOutlineColor(palette, false, false, 1, AnimationMode::AnimationNone);
            painter->setRenderHint(QPainter::Antialiasing, false);
            painter->setBrush(Qt::NoBrush);
            painter->setPen(borderColor);

            if (value & Qt::LeftEdge) {
                painter->drawLine(rect.topLeft(), rect.bottomLeft());
            }
            if (value & Qt::RightEdge) {
                painter->drawLine(rect.topRight(), rect.bottomRight());
            }
            if (value & Qt::TopEdge) {
                painter->drawLine(rect.topLeft(), rect.topRight());
            }
            if (value & Qt::BottomEdge) {
                painter->drawLine(rect.bottomLeft(), rect.bottomRight());
            }

            return true;
        }
    }

    // make sure there is enough room to render frame
    if (rect.height() < 2 * Metrics::LineEdit_FrameWidth + option->fontMetrics.height()) {
        const auto &background = palette.color(QPalette::Base);

        painter->setPen(Qt::NoPen);
        painter->setBrush(background);
        painter->drawRect(rect);
        return true;

    } else {
        // copy state
        const State &state(option->state);
        const bool enabled(state & State_Enabled);
        const bool mouseOver(enabled && (state & State_MouseOver));
        const bool hasFocus(enabled && (state & State_HasFocus));

        // focus takes precedence over mouse over
        _animations->inputWidgetEngine().updateState(widget, AnimationFocus, hasFocus);
        _animations->inputWidgetEngine().updateState(widget, AnimationHover, mouseOver && !hasFocus);

        // retrieve animation mode and opacity
        const AnimationMode mode(_animations->inputWidgetEngine().frameAnimationMode(widget));
        const qreal opacity(_animations->inputWidgetEngine().frameOpacity(widget));

        // render
        const auto &background = palette.color(QPalette::Base);
        const auto outline(hasHighlightNeutral(widget, option, mouseOver, hasFocus) ? _helper->neutralText(palette).lighter(mouseOver || hasFocus ? 150 : 100)
                                                                                    : _helper->frameOutlineColor(palette, mouseOver, hasFocus, opacity, mode));
        _helper->renderFrame(painter, rect, background, outline);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawFrameFocusRectPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // no focus indicator on buttons / scrollbars, since it is rendered elsewhere
    if (qobject_cast<const QAbstractButton *>(widget) || qobject_cast<const QScrollBar *>(widget) || qobject_cast<const QGroupBox *>(widget)) {
        return true;
    }

    // no focus indicator on ComboBox list items
    if (widget && widget->inherits("QComboBoxListView")) {
        return true;
    }

    if (option->styleObject && option->styleObject->property("elementType") == QLatin1String("button")) {
        return true;
    }

    const State &state(option->state);

    // no focus indicator on selected list items
    if ((state & State_Selected) && qobject_cast<const QAbstractItemView *>(widget)) {
        return true;
    }

    const auto rect(option->rect.adjusted(0, 0, 0, 1));
    const auto &palette(option->palette);

    if (rect.width() < 10) {
        return true;
    }

    const auto outlineColor(state & State_Selected ? palette.color(QPalette::HighlightedText) : palette.color(HighlightColor));
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(outlineColor);
    painter->drawLine(QPoint(rect.bottomLeft() - QPoint(0, 1)), QPoint(rect.bottomRight() - QPoint(0, 1)));

    return true;
}

//___________________________________________________________________________________
bool Style::drawFrameMenuPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // only draw frame for (expanded) toolbars and QtQuick controls
    // do nothing for other cases, for which frame is rendered via drawPanelMenuPrimitive
    if (qobject_cast<const QToolBar *>(widget)) {
        const auto &palette(option->palette);
        const auto background(_helper->frameBackgroundColor(palette));
        const auto outline(_helper->frameOutlineColor(palette));

        const bool hasAlpha(_helper->hasAlphaChannel(widget));
        _helper->renderMenuFrame(painter, option->rect, background, outline, hasAlpha);

    } else if (isQtQuickControl(option, widget)) {
        const auto &palette(option->palette);
        const auto background(_helper->frameBackgroundColor(palette));
        const auto outline(_helper->frameOutlineColor(palette));

        const bool hasAlpha(_helper->hasAlphaChannel(widget));
        _helper->renderMenuFrame(painter, option->rect, background, outline, hasAlpha);
    }

    return true;
}

//______________________________________________________________
bool Style::drawFrameGroupBoxPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // cast option and check
    const auto frameOption(qstyleoption_cast<const QStyleOptionFrame *>(option));
    if (!frameOption) {
        return true;
    }

    // no frame for flat groupboxes
    if (frameOption->features & QStyleOptionFrame::Flat) {
        return true;
    }

    // normal frame
    const auto &palette(option->palette);
    const auto background(_helper->frameBackgroundColor(palette));
    const auto outline(_helper->frameOutlineColor(palette));

    /*
     * need to reset painter's clip region in order to paint behind textbox label
     * (was taken out in QCommonStyle)
     */

    painter->setClipRegion(option->rect);
    _helper->renderFrame(painter, option->rect, background, outline);

    return true;
}

//___________________________________________________________________________________
bool Style::drawFrameTabWidgetPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto tabOption(qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option));
    if (!tabOption) {
        return true;
    }

    // do nothing if tabbar is hidden
    const bool isQtQuickControl(this->isQtQuickControl(option, widget));
    if (tabOption->tabBarSize.isEmpty() && !isQtQuickControl) {
        return true;
    }

    // adjust rect to handle overlaps
    auto rect(option->rect);

    const auto tabBarRect(tabOption->tabBarRect);
    const QSize tabBarSize(tabOption->tabBarSize);

    // define colors
    const auto &palette(option->palette);
    const auto background(_helper->frameBackgroundColor(palette));
    const auto outline(_helper->frameOutlineColor(palette));

    Corners corners = AllCorners;

    // adjust corners to deal with oversized tabbars
    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        if (isQtQuickControl) {
            rect.adjust(-1, -1, 1, 0);
        }
        if (tabBarSize.width() >= rect.width() - 2 * Metrics::Frame_FrameRadius) {
            corners &= ~CornersTop;
        }
        if (tabBarRect.left() < rect.left() + Metrics::Frame_FrameRadius) {
            corners &= ~CornerTopLeft;
        }
        if (tabBarRect.right() > rect.right() - Metrics::Frame_FrameRadius) {
            corners &= ~CornerTopRight;
        }
        break;

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        if (isQtQuickControl) {
            rect.adjust(-1, 0, 1, 1);
        }
        if (tabBarSize.width() >= rect.width() - 2 * Metrics::Frame_FrameRadius) {
            corners &= ~CornersBottom;
        }
        if (tabBarRect.left() < rect.left() + Metrics::Frame_FrameRadius) {
            corners &= ~CornerBottomLeft;
        }
        if (tabBarRect.right() > rect.right() - Metrics::Frame_FrameRadius) {
            corners &= ~CornerBottomRight;
        }
        break;

    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        if (isQtQuickControl) {
            rect.adjust(-1, 0, 0, 0);
        }
        if (tabBarSize.height() >= rect.height() - 2 * Metrics::Frame_FrameRadius) {
            corners &= ~CornersLeft;
        }
        if (tabBarRect.top() < rect.top() + Metrics::Frame_FrameRadius) {
            corners &= ~CornerTopLeft;
        }
        if (tabBarRect.bottom() > rect.bottom() - Metrics::Frame_FrameRadius) {
            corners &= ~CornerBottomLeft;
        }
        break;

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        if (isQtQuickControl) {
            rect.adjust(0, 0, 1, 0);
        }
        if (tabBarSize.height() >= rect.height() - 2 * Metrics::Frame_FrameRadius) {
            corners &= ~CornersRight;
        }
        if (tabBarRect.top() < rect.top() + Metrics::Frame_FrameRadius) {
            corners &= ~CornerTopRight;
        }
        if (tabBarRect.bottom() > rect.bottom() - Metrics::Frame_FrameRadius) {
            corners &= ~CornerBottomRight;
        }
        break;

    default:
        break;
    }

    _helper->renderTabWidgetFrame(painter, rect, background, outline, corners);

    return true;
}

//___________________________________________________________________________________
bool Style::drawFrameTabBarBasePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // tabbar frame used either for 'separate' tabbar, or in 'document mode'

    // cast option and check
    const auto tabOption(qstyleoption_cast<const QStyleOptionTabBarBase *>(option));
    if (!tabOption) {
        return true;
    }

    // get rect, orientation, palette
    const QRectF rect(option->rect);
    const auto outline(_helper->frameOutlineColor(option->palette));

    // setup painter
    painter->setBrush(Qt::NoBrush);
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(QPen(outline, PenWidth::Frame));

    // render
    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        painter->drawLine(rect.bottomLeft() - QPointF(1, 0), rect.bottomRight() + QPointF(1, 0));
        break;

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        painter->drawLine(rect.topLeft() - QPointF(1, 0), rect.topRight() + QPointF(1, 0));
        break;

    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        painter->drawLine(rect.topRight() - QPointF(0, 1), rect.bottomRight() + QPointF(1, 0));
        break;

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        painter->drawLine(rect.topLeft() - QPointF(0, 1), rect.bottomLeft() + QPointF(1, 0));
        break;

    default:
        break;
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawFrameWindowPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // copy rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);
    const State state(option->state);
    const bool selected(state & State_Selected);

    // render frame outline
    const auto outline(_helper->frameOutlineColor(palette, false, selected));
    _helper->renderMenuFrame(painter, rect, QColor(), outline);

    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorArrowPrimitive(ArrowOrientation orientation, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // store rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // store state
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    bool mouseOver(enabled && (state & State_MouseOver));
    bool hasFocus(enabled && (state & State_HasFocus));

    // detect special buttons
    const bool inTabBar(widget && qobject_cast<const QTabBar *>(widget->parentWidget()));
    const bool inToolButton(qstyleoption_cast<const QStyleOptionToolButton *>(option));

    // color
    QColor color;
    if (inTabBar) {
        // for tabbar arrows one uses animations to get the arrow color
        /*
         * get animation state
         * there is no need to update the engine since this was already done when rendering the frame
         */
        const AnimationMode mode(_animations->widgetStateEngine().buttonAnimationMode(widget));
        const qreal opacity(_animations->widgetStateEngine().buttonOpacity(widget));
        color = _helper->arrowColor(palette, mouseOver, hasFocus, opacity, mode);

    } else if (mouseOver && !inToolButton) {
        color = _helper->hoverColor(palette);

    } else if (inToolButton) {
        const bool flat(state & State_AutoRaise);

        // cast option
        const QStyleOptionToolButton *toolButtonOption(static_cast<const QStyleOptionToolButton *>(option));
        const auto menuStyle = BreezePrivate::toolButtonMenuArrowStyle(toolButtonOption);
        const bool sunken = state & State_Sunken;
        const bool checked = state & State_On;
        const bool arrowHover = mouseOver && (toolButtonOption->activeSubControls & SC_ToolButtonMenu);
        if (flat && menuStyle != BreezePrivate::ToolButtonMenuArrowStyle::None) {
            if (sunken && !mouseOver) {
                color = palette.color(QPalette::HighlightedText);
            } else if (checked && !mouseOver) {
                color = _helper->arrowColor(palette, QPalette::WindowText);
            } else if (checked && arrowHover) {
                // If the button is checked we have a focus color tinted background on hover
                color = palette.color(QPalette::HighlightedText);
            } else {
                // for menu arrows in flat toolbutton one uses animations to get the arrow color
                // handle arrow over animation
                _animations->toolButtonEngine().updateState(widget, AnimationHover, arrowHover);

                const bool animated(_animations->toolButtonEngine().isAnimated(widget, AnimationHover));
                const qreal opacity(_animations->toolButtonEngine().opacity(widget, AnimationHover));

                color = _helper->arrowColor(palette, arrowHover, false, opacity, animated ? AnimationHover : AnimationNone);
            }

        } else if (flat) {
            if (sunken && hasFocus && !mouseOver) {
                color = palette.color(QPalette::WindowText);
            } else {
                color = _helper->arrowColor(palette, QPalette::WindowText);
            }

        } else if (hasFocus && !mouseOver) {
            color = palette.color(QPalette::ButtonText);

        } else {
            color = _helper->arrowColor(palette, QPalette::ButtonText);
        }

    } else {
        color = _helper->arrowColor(palette, QPalette::WindowText);
    }

    // render
    _helper->renderArrow(painter, rect, color, orientation);

    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorHeaderArrowPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    const auto headerOption(qstyleoption_cast<const QStyleOptionHeader *>(option));
    const State &state(option->state);

    // arrow orientation
    ArrowOrientation orientation(ArrowNone);
    if (state & State_UpArrow || (headerOption && headerOption->sortIndicator == QStyleOptionHeader::SortUp)) {
        orientation = ArrowUp;
    } else if (state & State_DownArrow || (headerOption && headerOption->sortIndicator == QStyleOptionHeader::SortDown)) {
        orientation = ArrowDown;
    }
    if (orientation == ArrowNone) {
        return true;
    }

    // invert arrows if requested by (hidden) options
    if (StyleConfigData::viewInvertSortIndicator()) {
        orientation = (orientation == ArrowUp) ? ArrowDown : ArrowUp;
    }

    // state, rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // define color and polygon for drawing arrow
    const auto color = _helper->arrowColor(palette, QPalette::ButtonText);

    // render
    _helper->renderArrow(painter, rect, color, orientation);

    return true;
}

//______________________________________________________________
bool Style::drawPanelButtonCommandPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // button state
    bool enabled = option->state & QStyle::State_Enabled;
    bool activeFocus = option->state & QStyle::State_HasFocus;
    // Using `widget->focusProxy() == nullptr` to work around a possible Qt bug
    // where buttons that have a focusProxy still show focus.
    bool visualFocus = activeFocus && option->state & QStyle::State_KeyboardFocusChange && (widget == nullptr || widget->focusProxy() == nullptr);
    bool hovered = option->state & QStyle::State_MouseOver;
    bool down = option->state & QStyle::State_Sunken;
    bool checked = option->state & QStyle::State_On;
    bool flat = false;
    bool hasMenu = false;
    // Use to determine if this button is a default button.
    bool defaultButton = false;
    bool hasNeutralHighlight = hasHighlightNeutral(widget, option);

    const auto buttonOption = qstyleoption_cast<const QStyleOptionButton *>(option);
    if (buttonOption) {
        flat = buttonOption->features & QStyleOptionButton::Flat;
        hasMenu = buttonOption->features & QStyleOptionButton::HasMenu;
        // If autoDefault is re-enabled by undoing a change to this file and
        // we decide that the default button highlight moving around outside
        // of the QDialogButtonBox is annoying, we could add
        // ` && widget->parentWidget()->inherits("QDialogButtonBox")`.
        // The downside would be that you can't see which button is the
        // default button when a QPushButton in a QDialog outside of a
        // QDialogButtonBox is focused.
        defaultButton = buttonOption->features & QStyleOptionButton::DefaultButton;
    }

    // NOTE: Using focus animation for bg down because the pressed animation only works on press when enabled for buttons and not on release.
    _animations->widgetStateEngine().updateState(widget, AnimationFocus, down && enabled);
    // NOTE: Using hover animation for all pen animations to prevent flickering when closing the menu.
    _animations->widgetStateEngine().updateState(widget, AnimationHover, (hovered || visualFocus || down) && enabled);
    qreal bgAnimation = _animations->widgetStateEngine().opacity(widget, AnimationFocus);
    qreal penAnimation = _animations->widgetStateEngine().opacity(widget, AnimationHover);

    QHash<QByteArray, bool> stateProperties;
    stateProperties["enabled"] = enabled;
    stateProperties["visualFocus"] = visualFocus;
    stateProperties["hovered"] = hovered;
    stateProperties["down"] = down;
    stateProperties["checked"] = checked;
    stateProperties["flat"] = flat;
    stateProperties["hasMenu"] = hasMenu;
    stateProperties["defaultButton"] = defaultButton;
    stateProperties["hasNeutralHighlight"] = hasNeutralHighlight;
    stateProperties["isActiveWindow"] = widget ? widget->isActiveWindow() : true;

    _helper->renderButtonFrame(painter, option->rect, option->palette, stateProperties, bgAnimation, penAnimation);

    return true;
}

//______________________________________________________________
bool Style::drawPanelButtonToolPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // button state
    bool enabled = option->state & QStyle::State_Enabled;
    bool activeFocus = option->state & QStyle::State_HasFocus;
    bool visualFocus = activeFocus && option->state & QStyle::State_KeyboardFocusChange && (widget == nullptr || widget->focusProxy() == nullptr);
    bool hovered = option->state & QStyle::State_MouseOver;
    bool down = option->state & QStyle::State_Sunken;
    bool checked = option->state & QStyle::State_On;
    bool flat = option->state & QStyle::State_AutoRaise;
    bool hasNeutralHighlight = hasHighlightNeutral(widget, option);

    // NOTE: Using focus animation for bg down because the pressed animation only works on press when enabled for buttons and not on release.
    _animations->widgetStateEngine().updateState(widget, AnimationFocus, down && enabled);
    // NOTE: Using hover animation for all pen animations to prevent flickering when closing the menu.
    _animations->widgetStateEngine().updateState(widget, AnimationHover, (hovered || visualFocus || down) && enabled);
    qreal bgAnimation = _animations->widgetStateEngine().opacity(widget, AnimationFocus);
    qreal penAnimation = _animations->widgetStateEngine().opacity(widget, AnimationHover);

    QRect baseRect = option->rect;
    // adjust frame in case of menu
    const auto menuStyle = BreezePrivate::toolButtonMenuArrowStyle(option);
    if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::SubControl) {
        // NOTE: working around weird issue with flat toolbuttons having unusually wide rects
        const auto clipRect = baseRect.adjusted(0, 0, flat ? -Metrics::ToolButton_InlineIndicatorWidth - Metrics::ToolButton_ItemSpacing * 2 : 0, 0);
        painter->setClipRect(visualRect(option, clipRect));
        baseRect.adjust(0, 0, Metrics::Frame_FrameRadius + PenWidth::Shadow, 0);
        baseRect = visualRect(option, baseRect);
    }

    QHash<QByteArray, bool> stateProperties;
    stateProperties["enabled"] = enabled;
    stateProperties["visualFocus"] = visualFocus;
    stateProperties["hovered"] = hovered;
    stateProperties["down"] = down;
    stateProperties["checked"] = checked;
    stateProperties["flat"] = flat;
    stateProperties["hasNeutralHighlight"] = hasNeutralHighlight;
    stateProperties["isActiveWindow"] = widget ? widget->isActiveWindow() : true;

    _helper->renderButtonFrame(painter, baseRect, option->palette, stateProperties, bgAnimation, penAnimation);
    if (painter->hasClipping()) {
        painter->setClipping(false);
    }

    return true;
}

//______________________________________________________________
bool Style::drawTabBarPanelButtonToolPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // copy palette and rect
    auto rect(option->rect);

    // static_cast is safe here since check was already performed in calling function
    const QTabBar *tabBar(static_cast<QTabBar *>(widget->parentWidget()));

    // overlap.
    // subtract 1, because of the empty pixel left the tabwidget frame
    const int overlap(Metrics::TabBar_BaseOverlap - 1);

    // adjust rect based on tabbar shape
    switch (tabBar->shape()) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        rect.adjust(0, 0, 0, -overlap);
        break;

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        rect.adjust(0, overlap, 0, 0);
        break;

    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        rect.adjust(0, 0, -overlap, 0);
        break;

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        rect.adjust(overlap, 0, 0, 0);
        break;

    default:
        break;
    }

    // get the relevant palette
    const QWidget *parent(tabBar->parentWidget());
    if (qobject_cast<const QTabWidget *>(parent)) {
        parent = parent->parentWidget();
    }
    const auto &palette(parent ? parent->palette() : QApplication::palette());
    const auto color = hasAlteredBackground(parent) ? _helper->frameBackgroundColor(palette) : palette.color(QPalette::Window);

    // render flat background
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawRect(rect);

    return true;
}

//___________________________________________________________________________________
bool Style::drawPanelScrollAreaCornerPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // make sure background role matches viewport
    const QAbstractScrollArea *scrollArea;
    if ((scrollArea = qobject_cast<const QAbstractScrollArea *>(widget)) && scrollArea->viewport()) {
        // need to adjust clipRect in order not to render outside of frame
        const int frameWidth(pixelMetric(PM_DefaultFrameWidth, nullptr, scrollArea));
        painter->setClipRect(insideMargin(scrollArea->rect(), frameWidth));
        painter->setBrush(scrollArea->viewport()->palette().color(scrollArea->viewport()->backgroundRole()));
        painter->setPen(Qt::NoPen);
        painter->drawRect(option->rect);
        return true;

    } else {
        return false;
    }
}

//___________________________________________________________________________________
bool Style::drawPanelMenuPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    /*
     * do nothing if menu is embedded in another widget
     * this corresponds to having a transparent background
     */
    if (widget && !widget->isWindow()) {
        return true;
    }

    const auto &palette(option->palette);
    const bool hasAlpha(_helper->hasAlphaChannel(widget));
    const auto seamlessEdges = _helper->menuSeamlessEdges(widget);
    auto background(_helper->frameBackgroundColor(palette));
    auto outline(_helper->frameOutlineColor(palette));

    painter->save();

    if (StyleConfigData::menuOpacity() < 100) {
        if (widget && widget->isWindow()) {
            painter->setCompositionMode(QPainter::CompositionMode_Source);
        }
        background.setAlphaF(StyleConfigData::menuOpacity() / 100.0);
        outline = _helper->alphaColor(palette.color(QPalette::WindowText), 0.25);
    }

    _helper->renderMenuFrame(painter, option->rect, background, outline, hasAlpha, seamlessEdges);

    painter->restore();

    return true;
}

//___________________________________________________________________________________
bool Style::drawPanelTipLabelPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // force registration of widget
    if (widget && widget->window()) {
        _shadowHelper->registerWidget(widget->window(), true);
    }

    const auto &palette(option->palette);
    const auto &background = palette.color(QPalette::ToolTipBase);
    const auto outline(KColorUtils::mix(palette.color(QPalette::ToolTipBase), palette.color(QPalette::ToolTipText), 0.25));
    const bool hasAlpha(_helper->hasAlphaChannel(widget));

    _helper->renderMenuFrame(painter, option->rect, background, outline, hasAlpha);
    return true;
}

//___________________________________________________________________________________
bool Style::drawPanelItemViewItemPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto viewItemOption = qstyleoption_cast<const QStyleOptionViewItem *>(option);
    if (!viewItemOption) {
        return false;
    }

    // try cast widget
    const auto abstractItemView = qobject_cast<const QAbstractItemView *>(widget);

    // store palette and rect
    const auto &palette(option->palette);
    auto rect(option->rect);

    // store flags
    const State &state(option->state);
    const bool mouseOver((state & State_MouseOver) && (!abstractItemView || abstractItemView->selectionMode() != QAbstractItemView::NoSelection));
    const bool selected(state & State_Selected);
    const bool enabled(state & State_Enabled);
    const bool active(state & State_Active);

    const bool hasCustomBackground = viewItemOption->backgroundBrush.style() != Qt::NoBrush && !(state & State_Selected);
    const bool hasSolidBackground = !hasCustomBackground || viewItemOption->backgroundBrush.style() == Qt::SolidPattern;
    const bool hasAlternateBackground(viewItemOption->features & QStyleOptionViewItem::Alternate);

    // do nothing if no background is to be rendered
    if (!(mouseOver || selected || hasCustomBackground || hasAlternateBackground)) {
        return true;
    }

    // define color group
    QPalette::ColorGroup colorGroup;
    if (enabled) {
        colorGroup = active ? QPalette::Active : QPalette::Inactive;
    } else {
        colorGroup = QPalette::Disabled;
    }

    // render alternate background
    if (hasAlternateBackground) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette.brush(colorGroup, QPalette::AlternateBase));
        painter->drawRect(rect);
    }

    // stop here if no highlight is needed
    if (!(mouseOver || selected || hasCustomBackground)) {
        return true;
    }

    // render custom background
    if (hasCustomBackground && !hasSolidBackground) {
        painter->setBrushOrigin(viewItemOption->rect.topLeft());
        painter->setBrush(viewItemOption->backgroundBrush);
        painter->setPen(Qt::NoPen);
        painter->drawRect(viewItemOption->rect);
        return true;
    }

    // render selection
    // define color
    QColor color;
    if (hasCustomBackground && hasSolidBackground) {
        color = viewItemOption->backgroundBrush.color();
    } else {
        color = palette.color(colorGroup, QPalette::Highlight);
    }

    // change color to implement mouse over
    if (mouseOver && !hasCustomBackground) {
        if (!selected) {
            color.setAlphaF(0.2);
        } else {
            color = color.lighter(110);
        }
    }

    // render
    _helper->renderSelection(painter, rect, color);

    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorCheckBoxPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // copy rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // store flags
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool sunken(state & State_Sunken);
    const bool mouseOver(enabled && (state & State_MouseOver));

    // checkbox state
    CheckBoxState checkBoxState(CheckOff);
    if (state & State_NoChange) {
        checkBoxState = CheckPartial;
    } else if (state & State_On) {
        checkBoxState = CheckOn;
    }

    // animation state
    _animations->widgetStateEngine().updateState(widget, AnimationHover, mouseOver);
    _animations->widgetStateEngine().updateState(widget, AnimationPressed, checkBoxState != CheckOff);
    auto target = checkBoxState;
    if (_animations->widgetStateEngine().isAnimated(widget, AnimationPressed)) {
        checkBoxState = CheckAnimated;
    }
    const qreal animation(_animations->widgetStateEngine().opacity(widget, AnimationPressed));

    const qreal opacity(_animations->widgetStateEngine().opacity(widget, AnimationHover));

    // render
    _helper->renderCheckBoxBackground(painter, rect, palette, checkBoxState, hasHighlightNeutral(widget, option, mouseOver), sunken, animation);
    _helper
        ->renderCheckBox(painter, rect, palette, mouseOver, checkBoxState, target, hasHighlightNeutral(widget, option, mouseOver), sunken, animation, opacity);
    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorRadioButtonPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // copy rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // store flags
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool sunken(state & State_Sunken);
    const bool mouseOver(enabled && (state & State_MouseOver));

    // radio button state
    RadioButtonState radioButtonState((state & State_On) ? RadioOn : RadioOff);

    // animation state
    _animations->widgetStateEngine().updateState(widget, AnimationHover, mouseOver);
    _animations->widgetStateEngine().updateState(widget, AnimationPressed, radioButtonState != RadioOff);
    if (_animations->widgetStateEngine().isAnimated(widget, AnimationPressed)) {
        radioButtonState = RadioAnimated;
    }
    const qreal animation(_animations->widgetStateEngine().opacity(widget, AnimationPressed));

    // colors
    const qreal opacity(_animations->widgetStateEngine().opacity(widget, AnimationHover));

    // render
    _helper->renderRadioButtonBackground(painter, rect, palette, radioButtonState, hasHighlightNeutral(widget, option, mouseOver), sunken, animation);
    _helper->renderRadioButton(painter, rect, palette, mouseOver, radioButtonState, hasHighlightNeutral(widget, option, mouseOver), sunken, animation, opacity);

    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorButtonDropDownPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto complexOption(qstyleoption_cast<const QStyleOptionComplex *>(option));
    if (!complexOption || !(complexOption->subControls & SC_ToolButtonMenu)) {
        return true;
    }

    // button state
    bool enabled = option->state & QStyle::State_Enabled;
    bool activeFocus = option->state & QStyle::State_HasFocus;
    bool visualFocus = activeFocus && option->state & QStyle::State_KeyboardFocusChange && (widget == nullptr || widget->focusProxy() == nullptr);
    bool hovered = option->state & QStyle::State_MouseOver;
    bool down = option->state & QStyle::State_Sunken;
    bool checked = option->state & QStyle::State_On;
    bool flat = option->state & QStyle::State_AutoRaise;
    bool hasNeutralHighlight = hasHighlightNeutral(widget, option);

    // update animation state
    // NOTE: Using focus animation for bg down because the pressed animation only works on press when enabled for buttons and not on release.
    _animations->widgetStateEngine().updateState(widget, AnimationFocus, down && enabled);
    // NOTE: Using hover animation for all pen animations to prevent flickering when closing the menu.
    _animations->widgetStateEngine().updateState(widget, AnimationHover, (hovered || visualFocus || down) && enabled);
    qreal bgAnimation = _animations->widgetStateEngine().opacity(widget, AnimationFocus);
    qreal penAnimation = _animations->widgetStateEngine().opacity(widget, AnimationHover);

    QRect baseRect = option->rect;
    const auto clipRect = visualRect(option, baseRect);
    painter->setClipRect(clipRect);
    baseRect.adjust(-Metrics::Frame_FrameRadius - qRound(PenWidth::Shadow), 0, 0, 0);
    baseRect = visualRect(option, baseRect);

    QHash<QByteArray, bool> stateProperties;
    stateProperties["enabled"] = enabled;
    stateProperties["visualFocus"] = visualFocus;
    stateProperties["hovered"] = hovered;
    stateProperties["down"] = down;
    stateProperties["checked"] = checked;
    stateProperties["flat"] = flat;
    stateProperties["hasNeutralHighlight"] = hasNeutralHighlight;
    stateProperties["isActiveWindow"] = widget ? widget->isActiveWindow() : true;

    _helper->renderButtonFrame(painter, baseRect, option->palette, stateProperties, bgAnimation, penAnimation);

    QRectF frameRect = _helper->strokedRect(_helper->shadowedRect(baseRect));

    // also render separator
    if (!flat || activeFocus || hovered || down || checked || penAnimation != AnimationData::OpacityInvalid) {
        painter->setBrush(Qt::NoBrush);
        if (option->direction == Qt::RightToLeft) {
            QRectF separatorRect = frameRect.adjusted(0, 0, -Metrics::Frame_FrameRadius - PenWidth::Shadow, 0);
            painter->drawLine(separatorRect.topRight(), separatorRect.bottomRight());
        } else {
            QRectF separatorRect = frameRect.adjusted(Metrics::Frame_FrameRadius + PenWidth::Shadow, 0, 0, 0);
            painter->drawLine(separatorRect.topLeft(), separatorRect.bottomLeft());
        }
    }

    if (painter->hasClipping()) {
        painter->setClipping(false);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorTabClosePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // get icon and check
    QIcon icon(standardIcon(SP_TitleBarCloseButton, option, widget));
    if (icon.isNull()) {
        return false;
    }

    // store state
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool active(state & State_Raised);
    const bool sunken(state & State_Sunken);

    // decide icon mode and state
    QIcon::Mode iconMode;
    QIcon::State iconState;
    if (!enabled) {
        iconMode = QIcon::Disabled;
        iconState = QIcon::Off;

    } else {
        if (active) {
            iconMode = QIcon::Active;
        } else {
            iconMode = QIcon::Normal;
        }

        iconState = sunken ? QIcon::On : QIcon::Off;
    }

    // icon size
    const int iconWidth(pixelMetric(QStyle::PM_SmallIconSize, option, widget));
    const QSize iconSize(iconWidth, iconWidth);

    // get pixmap
    const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
    const QPixmap pixmap(_helper->coloredIcon(icon, option->palette, iconSize, dpr, iconMode, iconState));

    // render
    drawItemPixmap(painter, option->rect, Qt::AlignCenter, pixmap);
    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorTabTearPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // cast option and check
    const auto tabOption(qstyleoption_cast<const QStyleOptionTab *>(option));
    if (!tabOption) {
        return true;
    }

    // store palette and rect
    const auto &palette(option->palette);
    auto rect(option->rect);

    const bool reverseLayout(option->direction == Qt::RightToLeft);

    const auto color(_helper->alphaColor(palette.color(QPalette::WindowText), 0.2));
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(color);
    painter->setBrush(Qt::NoBrush);
    switch (tabOption->shape) {
    case QTabBar::TriangularNorth:
    case QTabBar::RoundedNorth:
        rect.adjust(0, 1, 0, 0);
        if (reverseLayout) {
            painter->drawLine(rect.topRight(), rect.bottomRight());
        } else {
            painter->drawLine(rect.topLeft(), rect.bottomLeft());
        }
        break;

    case QTabBar::TriangularSouth:
    case QTabBar::RoundedSouth:
        rect.adjust(0, 0, 0, -1);
        if (reverseLayout) {
            painter->drawLine(rect.topRight(), rect.bottomRight());
        } else {
            painter->drawLine(rect.topLeft(), rect.bottomLeft());
        }
        break;

    case QTabBar::TriangularWest:
    case QTabBar::RoundedWest:
        rect.adjust(1, 0, 0, 0);
        painter->drawLine(rect.topLeft(), rect.topRight());
        break;

    case QTabBar::TriangularEast:
    case QTabBar::RoundedEast:
        rect.adjust(0, 0, -1, 0);
        painter->drawLine(rect.topLeft(), rect.topRight());
        break;

    default:
        break;
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorToolBarHandlePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // do nothing if disabled from options
    if (!StyleConfigData::toolBarDrawItemSeparator()) {
        return true;
    }

    // store rect and palette
    auto rect(option->rect);
    const auto &palette(option->palette);

    // store state
    const State &state(option->state);
    const bool separatorIsVertical(state & State_Horizontal);

    // define color and render
    const auto color(_helper->separatorColor(palette));
    if (separatorIsVertical) {
        rect.setWidth(Metrics::ToolBar_HandleWidth);
        rect = centerRect(option->rect, rect.size());
        rect.setWidth(3);
        _helper->renderSeparator(painter, rect, color, separatorIsVertical);

        rect.translate(2, 0);
        _helper->renderSeparator(painter, rect, color, separatorIsVertical);

    } else {
        rect.setHeight(Metrics::ToolBar_HandleWidth);
        rect = centerRect(option->rect, rect.size());
        rect.setHeight(3);
        _helper->renderSeparator(painter, rect, color, separatorIsVertical);

        rect.translate(0, 2);
        _helper->renderSeparator(painter, rect, color, separatorIsVertical);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorToolBarSeparatorPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    /*
     * do nothing if disabled from options
     * also need to check if widget is a combobox, because of Qt hack using 'toolbar' separator primitive
     * for rendering separators in comboboxes
     */
    if (!(StyleConfigData::toolBarDrawItemSeparator() || qobject_cast<const QComboBox *>(widget))) {
        return true;
    }

    // store rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // store state
    const State &state(option->state);
    const bool separatorIsVertical(state & State_Horizontal);

    // define color and render
    const auto color(_helper->separatorColor(palette));
    _helper->renderSeparator(painter, rect, color, separatorIsVertical);

    return true;
}

//___________________________________________________________________________________
bool Style::drawIndicatorBranchPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // copy rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // state
    const State &state(option->state);
    const bool reverseLayout(option->direction == Qt::RightToLeft);

    // draw expander
    int expanderAdjust = 0;
    if (state & State_Children) {
        // state
        const bool expanderOpen(state & State_Open);
        const bool enabled(state & State_Enabled);
        const bool mouseOver(enabled && (state & State_MouseOver));

        // expander rect
        int expanderSize = qMin(rect.width(), rect.height());
        expanderSize = qMin(expanderSize, int(Metrics::ItemView_ArrowSize));
        expanderAdjust = expanderSize / 2 + 1;
        const auto arrowRect = centerRect(rect, expanderSize, expanderSize);

        // get orientation from option
        ArrowOrientation orientation;
        if (expanderOpen) {
            orientation = ArrowDown;
        } else if (reverseLayout) {
            orientation = ArrowLeft;
        } else {
            orientation = ArrowRight;
        }

        // color
        const auto arrowColor(mouseOver ? _helper->hoverColor(palette) : _helper->arrowColor(palette, QPalette::Text));

        // render
        _helper->renderArrow(painter, arrowRect, arrowColor, orientation);
    }

    // tree branches
    if (!StyleConfigData::viewDrawTreeBranchLines()) {
        return true;
    }

    const auto center(rect.center());
    const auto lineColor(KColorUtils::mix(palette.color(QPalette::Base), palette.color(QPalette::Text), 0.25));
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->translate(0.5, 0.5);
    painter->setPen(QPen(lineColor, 1));
    if (state & (State_Item | State_Children | State_Sibling)) {
        const QLineF line(QPointF(center.x(), rect.top()), QPointF(center.x(), center.y() - expanderAdjust - 1));
        painter->drawLine(line);
    }

    // The right/left (depending on direction) line gets drawn if we have an item
    if (state & State_Item) {
        const QLineF line = reverseLayout ? QLineF(QPointF(rect.left(), center.y()), QPointF(center.x() - expanderAdjust, center.y()))
                                          : QLineF(QPointF(center.x() + expanderAdjust, center.y()), QPointF(rect.right(), center.y()));
        painter->drawLine(line);
    }

    // The bottom if we have a sibling
    if (state & State_Sibling) {
        const QLineF line(QPointF(center.x(), center.y() + expanderAdjust), QPointF(center.x(), rect.bottom()));
        painter->drawLine(line);
    }

    return true;
}

bool Style::drawDockWidgetResizeHandlePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    Q_UNUSED(widget);

    painter->setBrush(_helper->separatorColor(option->palette));
    painter->setPen(Qt::NoPen);
    painter->drawRect(option->rect);

    return true;
}

bool Style::drawPanelStatusBarPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (widget && !widget->property("_breeze_statusbar_separator").toBool() && widget->parent() && !widget->parent()->inherits("QMainWindow")) {
        return true;
    }
    auto rect(option->rect);
    const auto color(_helper->separatorColor(option->palette));
    const auto size = pixelMetric(QStyle::PM_SplitterWidth, option, widget);
    rect.setHeight(size);
    _helper->renderSeparator(painter, rect, color, false);

    return true;
}

//___________________________________________________________________________________
bool Style::drawPushButtonLabelControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto buttonOption(qstyleoption_cast<const QStyleOptionButton *>(option));
    if (!buttonOption) {
        return true;
    }

    // copy rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // state
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool sunken(state & (State_On | State_Sunken));
    const bool mouseOver(enabled && (option->state & State_MouseOver));
    const bool hasFocus(enabled && !mouseOver && (option->state & State_HasFocus));
    const bool flat(buttonOption->features & QStyleOptionButton::Flat);

    // content
    const bool hasText(!buttonOption->text.isEmpty());
    const bool hasIcon((showIconsOnPushButtons() || flat || !hasText) && !buttonOption->icon.isNull());

    // contents
    auto contentsRect(rect);

    // color role
    QPalette::ColorRole textRole;
    if (flat) {
        if (hasFocus && sunken) {
            textRole = QPalette::WindowText;
        } else {
            textRole = QPalette::WindowText;
        }

    } else if (hasFocus) {
        textRole = QPalette::ButtonText;
    } else {
        textRole = QPalette::ButtonText;
    }

    // menu arrow
    if (buttonOption->features & QStyleOptionButton::HasMenu) {
        // define rect
        auto arrowRect(contentsRect);
        arrowRect.setLeft(contentsRect.right() - Metrics::MenuButton_IndicatorWidth + 1);
        arrowRect = centerRect(arrowRect, Metrics::MenuButton_IndicatorWidth, Metrics::MenuButton_IndicatorWidth);

        contentsRect.setRight(arrowRect.left() - Metrics::Button_ItemSpacing - 1);
        contentsRect.adjust(Metrics::Button_MarginWidth, 0, 0, 0);

        arrowRect = visualRect(option, arrowRect);

        // define color
        const auto arrowColor(_helper->arrowColor(palette, textRole));
        _helper->renderArrow(painter, arrowRect, arrowColor, ArrowDown);
    }

    // icon size
    QSize iconSize;
    if (hasIcon) {
        iconSize = buttonOption->iconSize;
        if (!iconSize.isValid()) {
            const int metric(pixelMetric(PM_SmallIconSize, option, widget));
            iconSize = QSize(metric, metric);
        }
    }

    // text size
    const int textFlags(_mnemonics->textFlags() | Qt::AlignCenter);
    const QSize textSize(option->fontMetrics.size(textFlags, buttonOption->text));

    // adjust text and icon rect based on options
    QRect iconRect;
    QRect textRect;

    if (hasText && !hasIcon) {
        textRect = contentsRect;
    } else if (hasIcon && !hasText) {
        iconRect = contentsRect;
    } else {
        const int contentsWidth(iconSize.width() + textSize.width() + Metrics::Button_ItemSpacing);
        iconRect = QRect(
            QPoint(contentsRect.left() + (contentsRect.width() - contentsWidth) / 2, contentsRect.top() + (contentsRect.height() - iconSize.height()) / 2),
            iconSize);
        textRect = QRect(QPoint(iconRect.right() + Metrics::ToolButton_ItemSpacing + 1, contentsRect.top() + (contentsRect.height() - textSize.height()) / 2),
                         textSize);
    }

    // handle right to left
    if (iconRect.isValid()) {
        iconRect = visualRect(option, iconRect);
    }
    if (textRect.isValid()) {
        textRect = visualRect(option, textRect);
    }

    // make sure there is enough room for icon
    if (iconRect.isValid()) {
        iconRect = centerRect(iconRect, iconSize);
    }

    // render icon
    if (hasIcon && iconRect.isValid()) {
        // icon state and mode
        const QIcon::State iconState(sunken ? QIcon::On : QIcon::Off);
        QIcon::Mode iconMode;
        if (!enabled) {
            iconMode = QIcon::Disabled;
        } else {
            iconMode = QIcon::Normal;
        }

        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
        const auto pixmap = _helper->coloredIcon(buttonOption->icon, buttonOption->palette, iconSize, dpr, iconMode, iconState);
        drawItemPixmap(painter, iconRect, Qt::AlignCenter, pixmap);
    }

    // render text
    if (hasText && textRect.isValid()) {
        drawItemText(painter, textRect, textFlags, palette, enabled, buttonOption->text, textRole);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawToolButtonLabelControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto toolButtonOption(qstyleoption_cast<const QStyleOptionToolButton *>(option));

    // copy rect and palette
    const auto &rect = option->rect;

    // state
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool sunken(state & (State_On | State_Sunken));
    const bool mouseOver(enabled && (option->state & State_MouseOver));
    const bool flat(state & State_AutoRaise);

    // focus flag is set to match the background color in either renderButtonFrame or renderToolButtonFrame
    bool hasFocus(false);
    if (flat) {
        hasFocus = enabled && !mouseOver && (option->state & State_HasFocus);
    } else {
        hasFocus = enabled && !mouseOver && (option->state & (State_HasFocus | State_Sunken));
    }

    // contents
    auto contentsRect(rect);

    const auto menuStyle = BreezePrivate::toolButtonMenuArrowStyle(option);
    if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::InlineLarge) {
        // Place contents to the left of the menu arrow.
        const auto arrowRect = toolButtonSubControlRect(toolButtonOption, SC_ToolButtonMenu, widget);
        contentsRect.setRight(contentsRect.right() - arrowRect.width());
    }

    const auto toolButtonStyle = toolButtonOption->toolButtonStyle;
    const bool hasArrow = toolButtonOption->features & QStyleOptionToolButton::Arrow;
    bool hasIcon = toolButtonStyle != Qt::ToolButtonTextOnly
            && ((!toolButtonOption->icon.isNull() && !toolButtonOption->iconSize.isEmpty())
                || hasArrow);
    bool hasText = toolButtonStyle != Qt::ToolButtonIconOnly && !toolButtonOption->text.isEmpty();
    const bool textUnderIcon = hasIcon && hasText && toolButtonStyle == Qt::ToolButtonTextUnderIcon;

    const QSize &availableSize = contentsRect.size();
    const QSize &iconSize = toolButtonOption->iconSize;
    int textFlags(_mnemonics->textFlags());
    const QSize textSize(option->fontMetrics.size(textFlags, toolButtonOption->text));

    // adjust text and icon rect based on options
    QRect iconRect;
    QRect textRect;

    if (availableSize.isEmpty()) {
        hasIcon = false;
        hasText = false;
    }

    if (hasIcon && !hasText) { // icon only
        iconRect = contentsRect;
    } else if (!hasIcon && hasText) { // text only
        textRect = visualRect(option, contentsRect);
        textFlags |= Qt::AlignCenter;
    } else if (textUnderIcon) {
        const int contentsHeight(iconSize.height() + textSize.height() + Metrics::ToolButton_ItemSpacing);
        iconRect = {
            QPoint(contentsRect.left() + (contentsRect.width() - iconSize.width()) / 2,
                   contentsRect.top() + (contentsRect.height() - contentsHeight) / 2),
            iconSize
        };
        textRect = {
            QPoint(contentsRect.left() + (contentsRect.width() - textSize.width()) / 2,
                   iconRect.bottom() + Metrics::ToolButton_ItemSpacing + 1),
            textSize
        };

        // handle right to left layouts
        iconRect = visualRect(option, iconRect);
        textRect = visualRect(option, textRect);

        textFlags |= Qt::AlignCenter;
    } else if (hasIcon && hasText) {
        const bool leftAlign(widget && widget->property(PropertyNames::toolButtonAlignment).toInt() == Qt::AlignLeft);
        if (leftAlign) {
            const int marginWidth(Metrics::Button_MarginWidth + Metrics::Frame_FrameWidth + 1);
            iconRect = {
                QPoint(contentsRect.left() + marginWidth,
                       contentsRect.top() + (contentsRect.height() - iconSize.height()) / 2),
                iconSize
            };
        } else {
            const int contentsWidth(iconSize.width() + textSize.width() + Metrics::ToolButton_ItemSpacing);
            iconRect = {
                QPoint(contentsRect.left() + (contentsRect.width() - contentsWidth) / 2,
                       contentsRect.top() + (contentsRect.height() - iconSize.height()) / 2),
                iconSize
            };
        }

        const int padding = (contentsRect.height() - textSize.height()) / 2;
        textRect = {QPoint(iconRect.right() + Metrics::ToolButton_ItemSpacing + 1, contentsRect.top() + padding),
                    QSize(textSize.width(), contentsRect.height() - 2 * padding)};

        // Don't use QRect::isEmpty(). It does not return true if size is 0x0.
        hasText = !textRect.size().isEmpty();

        // handle right to left layouts
        iconRect = visualRect(option, iconRect);
        textRect = visualRect(option, textRect);

        textFlags |= Qt::AlignLeft | Qt::AlignVCenter;
    }

    // render arrow or icon
    if (hasIcon) {
        iconRect = centerRect(iconRect, iconSize);
        if (hasArrow) {
            QStyleOptionToolButton copy(*toolButtonOption);
            copy.rect = iconRect;
            switch (toolButtonOption->arrowType) {
            case Qt::LeftArrow:
                drawPrimitive(PE_IndicatorArrowLeft, &copy, painter, widget);
                break;
            case Qt::RightArrow:
                drawPrimitive(PE_IndicatorArrowRight, &copy, painter, widget);
                break;
            case Qt::UpArrow:
                drawPrimitive(PE_IndicatorArrowUp, &copy, painter, widget);
                break;
            case Qt::DownArrow:
                drawPrimitive(PE_IndicatorArrowDown, &copy, painter, widget);
                break;
            default:
                break;
            }

        } else {
            // icon state and mode
            const QIcon::State iconState(sunken ? QIcon::On : QIcon::Off);
            QIcon::Mode iconMode;
            if (!enabled) {
                iconMode = QIcon::Disabled;
            } else if (!flat && hasFocus) {
                iconMode = QIcon::Selected;
            } else if (mouseOver && flat) {
                iconMode = QIcon::Active;
            } else {
                iconMode = QIcon::Normal;
            }

            const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
            const QPixmap pixmap = _helper->coloredIcon(toolButtonOption->icon, toolButtonOption->palette, iconSize, dpr, iconMode, iconState);
            drawItemPixmap(painter, iconRect, Qt::AlignCenter, pixmap);
        }
    }

    // render text
    if (hasText) {
        QPalette::ColorRole textRole(QPalette::ButtonText);
        if (flat) {
            textRole = QPalette::WindowText;
        }

        auto palette = option->palette;

        painter->setFont(toolButtonOption->font);
        drawItemText(painter, textRect, textFlags, palette, enabled, toolButtonOption->text, textRole);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawCheckBoxLabelControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto buttonOption(qstyleoption_cast<const QStyleOptionButton *>(option));
    if (!buttonOption) {
        return true;
    }

    // copy palette and rect
    const auto &palette(option->palette);
    const auto &rect(option->rect);

    // store state
    const State &state(option->state);
    const bool enabled(state & State_Enabled);

    // text alignment
    const bool reverseLayout(option->direction == Qt::RightToLeft);
    const Qt::Alignment iconFlags(Qt::AlignVCenter | Qt::AlignLeft);
    const Qt::Alignment textFlags(_mnemonics->textFlags() | Qt::AlignVCenter | (reverseLayout ? Qt::AlignRight : Qt::AlignLeft));

    // text rect
    auto textRect(rect);
    // focus rect
    auto focusRect(rect);

    // render icon
    if (!buttonOption->icon.isNull()) {
        const QIcon::Mode mode(enabled ? QIcon::Normal : QIcon::Disabled);
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
        const QPixmap pixmap(_helper->coloredIcon(buttonOption->icon, buttonOption->palette, buttonOption->iconSize, dpr, mode));
        drawItemPixmap(painter, rect, iconFlags, pixmap);

        // adjust rect (copied from QCommonStyle)
        textRect.setLeft(textRect.left() + buttonOption->iconSize.width() + 4);
        textRect = visualRect(option, textRect);
        // shrink, flip and vertically center
        focusRect.setWidth(buttonOption->iconSize.width());
        focusRect = visualRect(option, focusRect);
        focusRect = centerRect(focusRect, buttonOption->iconSize);
    }

    // render text
    if (!buttonOption->text.isEmpty()) {
        textRect = option->fontMetrics.boundingRect(textRect, textFlags, buttonOption->text);
        focusRect.setTop(textRect.top());
        focusRect.setBottom(textRect.bottom());
        if (reverseLayout) {
            focusRect.setLeft(textRect.left());
        } else {
            focusRect.setRight(textRect.right());
        }
        drawItemText(painter, textRect, textFlags, palette, enabled, buttonOption->text, QPalette::WindowText);
    }

    // check focus state
    const bool hasFocus(enabled && (state & State_HasFocus));

    // update animation state
    _animations->widgetStateEngine().updateState(widget, AnimationFocus, hasFocus);

    const bool isFocusAnimated(_animations->widgetStateEngine().isAnimated(widget, AnimationFocus));
    const qreal opacity(_animations->widgetStateEngine().opacity(widget, AnimationFocus));
    // focus color
    QColor focusColor;
    if (isFocusAnimated) {
        focusColor = _helper->alphaColor(_helper->focusColor(palette), opacity);
    } else if (hasFocus) {
        focusColor = _helper->focusColor(palette);
    }

    // render focus
    _helper->renderFocusLine(painter, focusRect, focusColor);

    return true;
}

//___________________________________________________________________________________
bool Style::drawComboBoxLabelControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    const auto comboBoxOption(qstyleoption_cast<const QStyleOptionComboBox *>(option));
    if (!comboBoxOption) {
        return false;
    }
    if (comboBoxOption->editable) {
        return false;
    }

    // need to alter palette for focused buttons
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool sunken(state & (State_On | State_Sunken));
    const bool mouseOver(enabled && (option->state & State_MouseOver));
    const bool hasFocus(enabled && !mouseOver && (option->state & State_HasFocus));
    const bool flat(!comboBoxOption->frame);

    QPalette::ColorRole textRole;
    if (flat) {
        if (hasFocus && sunken) {
            textRole = QPalette::WindowText;
        } else {
            textRole = QPalette::WindowText;
        }

    } else if (hasFocus) {
        textRole = QPalette::ButtonText;
    } else {
        textRole = QPalette::ButtonText;
    }

    // change pen color directly
    painter->setPen(QPen(option->palette.color(textRole), 1));

    if (const auto cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
        auto editRect = proxy()->subControlRect(CC_ComboBox, cb, SC_ComboBoxEditField, widget);
        painter->save();
        painter->setClipRect(editRect);
        if (!cb->currentIcon.isNull()) {
            QIcon::Mode mode;

            if (!enabled) {
                mode = QIcon::Disabled;
            } else {
                mode = QIcon::Normal;
            }

            const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
            const QPixmap pixmap = _helper->coloredIcon(cb->currentIcon, cb->palette, cb->iconSize, dpr, mode);
            auto iconRect(editRect);
            iconRect.setWidth(cb->iconSize.width() + 4);
            iconRect = alignedRect(cb->direction, Qt::AlignLeft | Qt::AlignVCenter, iconRect.size(), editRect);
            if (cb->editable) {
                painter->fillRect(iconRect, option->palette.brush(QPalette::Base));
            }
            proxy()->drawItemPixmap(painter, iconRect, Qt::AlignCenter, pixmap);

            if (cb->direction == Qt::RightToLeft) {
                editRect.translate(-4 - cb->iconSize.width(), 0);
            } else {
                editRect.translate(cb->iconSize.width() + 4, 0);
            }
        }
        if (!cb->currentText.isEmpty() && !cb->editable) {
            proxy()->drawItemText(painter,
                                  editRect.adjusted(1, 0, -1, 0),
                                  visualAlignment(cb->direction, Qt::AlignLeft | Qt::AlignVCenter),
                                  cb->palette,
                                  cb->state & State_Enabled,
                                  cb->currentText);
        }
        painter->restore();
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawMenuBarItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto menuItemOption = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (!menuItemOption) {
        return true;
    }

    // copy rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // store state
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool selected(enabled && (state & State_Selected));
    const bool sunken(enabled && (state & State_Sunken));
    const bool useStrongFocus(StyleConfigData::menuItemDrawStrongFocus());

    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);

    // render hover and focus
    if (useStrongFocus && (selected || sunken)) {
        QColor outlineColor;
        if (sunken) {
            outlineColor = _helper->focusColor(palette);
        } else if (selected) {
            outlineColor = _helper->hoverColor(palette);
        }
        _helper->renderFocusRect(painter, rect, outlineColor);
    }

    /*
    check if item as an icon, in which case only the icon should be rendered
    consistently with comment in QMenuBarPrivate::calcActionRects
    */
    if (!menuItemOption->icon.isNull()) {
        // icon size is forced to SmallIconSize
        const auto iconSize = pixelMetric(QStyle::PM_SmallIconSize, nullptr, widget);
        const auto iconRect = centerRect(rect, iconSize, iconSize);

        // decide icon mode and state
        QIcon::Mode iconMode;
        QIcon::State iconState;
        if (!enabled) {
            iconMode = QIcon::Disabled;
            iconState = QIcon::Off;

        } else {
            if (useStrongFocus && sunken) {
                iconMode = QIcon::Selected;
            } else if (useStrongFocus && selected) {
                iconMode = QIcon::Active;
            } else {
                iconMode = QIcon::Normal;
            }

            iconState = sunken ? QIcon::On : QIcon::Off;
        }

        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
        const auto pixmap = _helper->coloredIcon(menuItemOption->icon, menuItemOption->palette, iconRect.size(), dpr, iconMode, iconState);
        drawItemPixmap(painter, iconRect, Qt::AlignCenter, pixmap);

        // render outline
        if (!useStrongFocus && (selected || sunken)) {
            QColor outlineColor;
            if (sunken) {
                outlineColor = _helper->focusColor(palette);
            } else if (selected) {
                outlineColor = _helper->hoverColor(palette);
            }

            _helper->renderFocusLine(painter, iconRect, outlineColor);
        }

    } else {
        // get text rect
        const int textFlags(Qt::AlignCenter | _mnemonics->textFlags());
        const auto textRect = option->fontMetrics.boundingRect(rect, textFlags, menuItemOption->text);

        // render text
        const QPalette::ColorRole role = (useStrongFocus && sunken) ? QPalette::HighlightedText : QPalette::WindowText;
        drawItemText(painter, textRect, textFlags, palette, enabled, menuItemOption->text, role);

        // render outline
        if (!useStrongFocus && (selected || sunken)) {
            QColor outlineColor;
            if (sunken) {
                outlineColor = _helper->focusColor(palette);
            } else if (selected) {
                outlineColor = _helper->hoverColor(palette);
            }

            _helper->renderFocusLine(painter, textRect, outlineColor);
        }
    }

    painter->restore();

    return true;
}

//___________________________________________________________________________________
bool Style::drawMenuItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto menuItemOption = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if (!menuItemOption) {
        return true;
    }
    if (menuItemOption->menuItemType == QStyleOptionMenuItem::EmptyArea) {
        return true;
    }

    // Size should be from sizeFromContents(CT_MenuItem, ...),
    // but with width set to the widest menu item width.
    // see QMenuPrivate::updateActionRects()
    const auto &rect = option->rect;
    const auto &palette = option->palette;

    // store state
    const State &state = option->state;
    const bool enabled = state & State_Enabled;
    const bool selected = enabled && (state & State_Selected);
    const bool sunken = enabled && (state & (State_Sunken));
    const bool reverseLayout = option->direction == Qt::RightToLeft;
    const bool useStrongFocus = StyleConfigData::menuItemDrawStrongFocus();

    // deal with separators
    if (menuItemOption->menuItemType == QStyleOptionMenuItem::Separator) {
        auto contentsRect = rect.adjusted(Metrics::MenuItem_MarginWidth, 0, -Metrics::MenuItem_MarginWidth, 0);
        QColor separatorColor;
        if (StyleConfigData::menuOpacity() < 100) {
            separatorColor = _helper->alphaColor(palette.color(QPalette::WindowText), Metrics::Bias_Default);
        } else {
            separatorColor = _helper->separatorColor(palette);
        }

        if (!menuItemOption->text.isEmpty()) { // icon is ignored on purpose
            contentsRect.adjust(0, Metrics::MenuItem_MarginHeight, 0, 0);
            int flags = visualAlignment(option->direction, Qt::AlignLeft) | Qt::AlignVCenter;
            flags |= Qt::TextSingleLine | Qt::TextHideMnemonic | Qt::TextDontClip;
            auto font = menuItemOption->font;
            font.setBold(true);
            QFontMetrics fm(font);
            auto textRect = fm.boundingRect(contentsRect, flags, menuItemOption->text);
            const auto &labelColor = palette.color(QPalette::Current, QPalette::WindowText);
            painter->setFont(font);
            painter->setBrush(Qt::NoBrush);
            // 0.7 is from Kirigami ListSectionHeader.
            // Not using painter->setOpacity() because it disables text antialiasing.
            painter->setPen(_helper->alphaColor(labelColor, 0.7));
            painter->drawText(textRect, flags, menuItemOption->text);
            // Make spacing between label and line the same as
            // the margin from the label to the left menu outline.
            // This is intentionally not factored into CT_MenuItem
            // size calculations since the line is meant to fill the
            // remaining available area after the label has been rendered.
            const qreal spacing = pixelMetric(PM_MenuHMargin, option, widget) + Metrics::MenuItem_MarginWidth - 1;
            if (reverseLayout) {
                contentsRect.setRight(textRect.left() - spacing);
            } else {
                contentsRect.setLeft(textRect.right() + spacing);
            }
        }

        _helper->renderSeparator(painter, contentsRect, separatorColor);
        return true;
    }

    // render hover and focus
    if (useStrongFocus && (selected || sunken)) {
        auto color = _helper->focusColor(palette);
        color = _helper->alphaColor(color, 0.3);
        const auto outlineColor = _helper->focusOutlineColor(palette);

        Sides sides;
        if (!menuItemOption->menuRect.isNull()) {
            const auto seamlessEdges = _helper->menuSeamlessEdges(widget);

            if (rect.top() <= menuItemOption->menuRect.top() && !seamlessEdges.testFlag(Qt::TopEdge)) {
                sides |= SideTop;
            }
            if (rect.bottom() >= menuItemOption->menuRect.bottom() && !seamlessEdges.testFlag(Qt::BottomEdge)) {
                sides |= SideBottom;
            }
            if (rect.left() <= menuItemOption->menuRect.left() && !seamlessEdges.testFlag(Qt::LeftEdge)) {
                sides |= SideLeft;
            }
            if (rect.right() >= menuItemOption->menuRect.right() && !seamlessEdges.testFlag(Qt::RightEdge)) {
                sides |= SideRight;
            }
        }

        _helper->renderFocusRect(painter, rect, color, outlineColor, SideTop | SideBottom | SideLeft | SideRight);
    }

    // get rect available for contents
    auto contentsRect(insideMargin(rect, Metrics::MenuItem_MarginWidth, (isTabletMode() ? 2 : 1) * Metrics::MenuItem_MarginHeight));

    // define relevant rectangles
    // checkbox
    QRect checkBoxRect;
    if (menuItemOption->menuHasCheckableItems) {
        checkBoxRect = QRect(contentsRect.left(),
                             contentsRect.top() + (contentsRect.height() - Metrics::CheckBox_Size) / 2,
                             Metrics::CheckBox_Size,
                             Metrics::CheckBox_Size);
        contentsRect.setLeft(checkBoxRect.right() + Metrics::MenuItem_ItemSpacing + 1);
    }

    // render checkbox indicator
    if (menuItemOption->checkType == QStyleOptionMenuItem::NonExclusive) {
        checkBoxRect = visualRect(option, checkBoxRect);

        // checkbox state

        CheckBoxState state(menuItemOption->checked ? CheckOn : CheckOff);
        _helper->renderCheckBoxBackground(painter, checkBoxRect, palette, state, false, sunken);
        _helper->renderCheckBox(painter, checkBoxRect, palette, false, state, state, false, sunken);

    } else if (menuItemOption->checkType == QStyleOptionMenuItem::Exclusive) {
        checkBoxRect = visualRect(option, checkBoxRect);

        const bool active(menuItemOption->checked);
        _helper->renderRadioButtonBackground(painter, checkBoxRect, palette, active ? RadioOn : RadioOff, false, sunken);
        _helper->renderRadioButton(painter, checkBoxRect, palette, false, active ? RadioOn : RadioOff, false, sunken);
    }

    // icon
    int iconMetric = 0;
    int iconWidth = 0;
    const bool showIcon(showIconsInMenuItems());
    if (showIcon) {
        iconMetric = pixelMetric(PM_SmallIconSize, option, widget);
        iconWidth = isQtQuickControl(option, widget) ? qMax(iconMetric, menuItemOption->maxIconWidth) : menuItemOption->maxIconWidth;
    }

    QRect iconRect;
    if (showIcon && iconWidth > 0) {
        iconRect = QRect(contentsRect.left(), contentsRect.top() + (contentsRect.height() - iconWidth) / 2, iconWidth, iconWidth);
        contentsRect.setLeft(iconRect.right() + Metrics::MenuItem_ItemSpacing + 1);
        const QSize iconSize(iconMetric, iconMetric);
        iconRect = centerRect(iconRect, iconSize);
    } else {
        contentsRect.setLeft(contentsRect.left() + Metrics::MenuItem_ExtraLeftMargin);
    }

    if (showIcon && !menuItemOption->icon.isNull()) {
        iconRect = visualRect(option, iconRect);

        // icon mode
        QIcon::Mode mode;
        if (enabled) {
            mode = QIcon::Normal;
        } else {
            mode = QIcon::Disabled;
        }

        // icon state
        const QIcon::State iconState(sunken ? QIcon::On : QIcon::Off);
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
        const QPixmap pixmap = _helper->coloredIcon(menuItemOption->icon, menuItemOption->palette, iconRect.size(), dpr, mode, iconState);
        drawItemPixmap(painter, iconRect, Qt::AlignCenter, pixmap);
    }

    // arrow
    QRect arrowRect(contentsRect.right() - Metrics::MenuButton_IndicatorWidth + 1,
                    contentsRect.top() + (contentsRect.height() - Metrics::MenuButton_IndicatorWidth) / 2,
                    Metrics::MenuButton_IndicatorWidth,
                    Metrics::MenuButton_IndicatorWidth);
    contentsRect.setRight(arrowRect.left() - Metrics::MenuItem_ItemSpacing - 1);

    if (menuItemOption->menuItemType == QStyleOptionMenuItem::SubMenu) {
        // apply right-to-left layout
        arrowRect = visualRect(option, arrowRect);

        // arrow orientation
        const ArrowOrientation orientation(reverseLayout ? ArrowLeft : ArrowRight);

        // color
        const QColor arrowColor = _helper->arrowColor(palette, QPalette::WindowText);

        // render
        _helper->renderArrow(painter, arrowRect, arrowColor, orientation);
    }

    // text
    auto textRect = contentsRect;
    if (!menuItemOption->text.isEmpty()) {
        // adjust textRect
        QString text = menuItemOption->text;
        textRect = centerRect(textRect, textRect.width(), option->fontMetrics.size(_mnemonics->textFlags(), text).height());
        textRect = visualRect(option, textRect);

        // set font
        painter->setFont(menuItemOption->font);

        // color role
        const QPalette::ColorRole role = QPalette::WindowText;

        // locate accelerator and render
        const int tabPosition(text.indexOf(QLatin1Char('\t')));
        if (tabPosition >= 0) {
            const int textFlags(Qt::AlignVCenter | Qt::AlignRight);
            QString accelerator(text.mid(tabPosition + 1));
            text = text.left(tabPosition);
            painter->save();
            painter->setOpacity(0.7);
            drawItemText(painter, textRect, textFlags, palette, enabled, accelerator, role);
            painter->restore();
        }

        // render text
        const int textFlags(Qt::AlignVCenter | (reverseLayout ? Qt::AlignRight : Qt::AlignLeft) | _mnemonics->textFlags());
        textRect = option->fontMetrics.boundingRect(textRect, textFlags, text);
        drawItemText(painter, textRect, textFlags, palette, enabled, text, role);

        // render hover and focus
        if (!useStrongFocus && (selected || sunken)) {
            QColor outlineColor;
            if (sunken) {
                outlineColor = _helper->focusColor(palette);
            } else if (selected) {
                outlineColor = _helper->hoverColor(palette);
            }

            _helper->renderFocusLine(painter, textRect, outlineColor);
        }
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawProgressBarControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    const auto progressBarOption(qstyleoption_cast<const QStyleOptionProgressBar *>(option));
    if (!progressBarOption) {
        return true;
    }

    // render groove
    QStyleOptionProgressBar progressBarOption2 = *progressBarOption;
    progressBarOption2.rect = subElementRect(SE_ProgressBarGroove, progressBarOption, widget);
    drawControl(CE_ProgressBarGroove, &progressBarOption2, painter, widget);

    const QObject *styleObject(widget ? widget : progressBarOption->styleObject);

    const bool busy(progressBarOption->minimum == 0 && progressBarOption->maximum == 0);

    // enable busy animations
    // need to check both widget and passed styleObject, used for QML
    if (styleObject && _animations->busyIndicatorEngine().enabled()) {
        // register QML object if defined
        if (!widget && progressBarOption->styleObject) {
            _animations->busyIndicatorEngine().registerWidget(progressBarOption->styleObject);
        }

        _animations->busyIndicatorEngine().setAnimated(styleObject, busy);
    }

    // check if animated and pass to option
    if (_animations->busyIndicatorEngine().isAnimated(styleObject)) {
        progressBarOption2.progress = _animations->busyIndicatorEngine().value();
    }

    // render contents
    progressBarOption2.rect = subElementRect(SE_ProgressBarContents, progressBarOption, widget);
    drawControl(CE_ProgressBarContents, &progressBarOption2, painter, widget);

    // render text
    const bool textVisible(progressBarOption->textVisible);
    if (textVisible && !busy) {
        progressBarOption2.rect = subElementRect(SE_ProgressBarLabel, progressBarOption, widget);
        drawControl(CE_ProgressBarLabel, &progressBarOption2, painter, widget);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawProgressBarContentsControl(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    const auto progressBarOption(qstyleoption_cast<const QStyleOptionProgressBar *>(option));
    if (!progressBarOption) {
        return true;
    }

    // copy rect and palette
    auto rect(option->rect);
    const auto &palette(option->palette);

    // get direction
    const bool horizontal(BreezePrivate::isProgressBarHorizontal(progressBarOption));
    const bool inverted(progressBarOption->invertedAppearance);
    bool reverse = horizontal && option->direction == Qt::RightToLeft;
    if (inverted) {
        reverse = !reverse;
    }

    // check if anything is to be drawn
    const bool busy((progressBarOption->minimum == 0 && progressBarOption->maximum == 0));
    if (busy) {
        const int progress(_animations->busyIndicatorEngine().value());

        const auto &first = palette.color(HighlightColor);
        const auto second(KColorUtils::mix(palette.color(HighlightColor), palette.color(QPalette::Window), 0.7));
        _helper->renderProgressBarBusyContents(painter, rect, first, second, horizontal, reverse, progress);

    } else {
        const QRegion oldClipRegion(painter->clipRegion());
        if (horizontal) {
            if (rect.width() < Metrics::ProgressBar_Thickness) {
                painter->setClipRect(rect, Qt::IntersectClip);
                if (reverse) {
                    rect.setLeft(rect.left() - Metrics::ProgressBar_Thickness + rect.width());
                } else {
                    rect.setWidth(Metrics::ProgressBar_Thickness);
                }
            }

        } else {
            if (rect.height() < Metrics::ProgressBar_Thickness) {
                painter->setClipRect(rect, Qt::IntersectClip);
                if (reverse) {
                    rect.setHeight(Metrics::ProgressBar_Thickness);
                } else {
                    rect.setTop(rect.top() - Metrics::ProgressBar_Thickness + rect.height());
                }
            }
        }

        auto contentsColor(option->state.testFlag(QStyle::State_Selected) ? palette.color(QPalette::HighlightedText) : palette.color(HighlightColor));

        _helper->renderProgressBarGroove(painter, rect, contentsColor, palette.color(QPalette::Window));
        painter->setClipRegion(oldClipRegion);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawProgressBarGrooveControl(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    const auto &palette(option->palette);
    const auto color(_helper->alphaColor(palette.color(QPalette::WindowText), 0.2));
    _helper->renderProgressBarGroove(painter, option->rect, color, palette.color(QPalette::Window));
    return true;
}

//___________________________________________________________________________________
bool Style::drawProgressBarLabelControl(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // cast option and check
    const auto progressBarOption(qstyleoption_cast<const QStyleOptionProgressBar *>(option));
    if (!progressBarOption) {
        return true;
    }

    // get direction and check
    const bool horizontal(BreezePrivate::isProgressBarHorizontal(progressBarOption));
    if (!horizontal) {
        return true;
    }

    // store rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // store state and direction
    const State &state(option->state);
    const bool enabled(state & State_Enabled);

    // define text rect
    const Qt::Alignment hAlign((progressBarOption->textAlignment == Qt::AlignLeft) ? Qt::AlignHCenter : progressBarOption->textAlignment);
    const QPalette::ColorRole role(progressBarOption->state.testFlag(QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text);
    drawItemText(painter, rect, Qt::AlignVCenter | hAlign, palette, enabled, progressBarOption->text, role);

    return true;
}

//___________________________________________________________________________________
bool Style::drawScrollBarSliderControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return true;
    }

    // copy rect and palette
    // const auto& rect( option->rect );
    const auto &palette(option->palette);

    // need to make it center due to the thin line separator
    QRect rect = option->rect;

    if (option->state & State_Horizontal) {
        rect.setTop(PenWidth::Frame);
    } else if (option->direction == Qt::RightToLeft) {
        rect.setRight(rect.right() - qRound(PenWidth::Frame));
    } else {
        rect.setLeft(PenWidth::Frame);
    }

    // try to understand if anywhere the widget is under mouse, not just the handle, use _animations in case of QWidget, option->styleObject in case of QML
    bool widgetMouseOver((option->state & State_MouseOver));
    if (widget) {
        widgetMouseOver = _animations->scrollBarEngine().isHovered(widget, QStyle::SC_ScrollBarGroove);
    } else if (option->styleObject) {
        widgetMouseOver = option->styleObject->property("hover").toBool();
    }

    qreal grooveAnimationOpacity(_animations->scrollBarEngine().opacity(widget, QStyle::SC_ScrollBarGroove));
    if (grooveAnimationOpacity == AnimationData::OpacityInvalid) {
        grooveAnimationOpacity = (widgetMouseOver ? 1 : 0);
    }

    // define handle rect
    QRect handleRect;
    const State &state(option->state);
    const bool horizontal(state & State_Horizontal);
    if (horizontal) {
        if (widgetMouseOver) {
            handleRect = centerRect(rect, rect.width(), Metrics::ScrollBar_SliderWidthMouseOver);
        } else
            handleRect = centerRect(rect, rect.width(), Metrics::ScrollBar_SliderWidthMouseNotOver);
    } else {
        if (widgetMouseOver) {
            handleRect = centerRect(rect, Metrics::ScrollBar_SliderWidthMouseOver, rect.height());
        } else
            handleRect = centerRect(rect, Metrics::ScrollBar_SliderWidthMouseNotOver, rect.height());
    }

    const bool enabled(state & State_Enabled);
    const bool mouseOver(enabled && (state & State_MouseOver));

    // check focus from relevant parent
    const QWidget *parent(scrollBarParent(widget));
    const bool hasFocus(enabled && ((widget && widget->hasFocus()) || (parent && parent->hasFocus())));

    // enable animation state
    const bool handleActive(sliderOption->activeSubControls & SC_ScrollBarSlider);
    _animations->scrollBarEngine().updateState(widget, AnimationFocus, hasFocus);

    _animations->scrollBarEngine().updateState(widget, AnimationHover, mouseOver && handleActive);

    const auto mode(_animations->scrollBarEngine().animationMode(widget, SC_ScrollBarSlider));
    const qreal opacity(_animations->scrollBarEngine().opacity(widget, SC_ScrollBarSlider));
    auto color = _helper->scrollBarHandleColor(palette, mouseOver, hasFocus, opacity, mode);
    if (StyleConfigData::animationsEnabled()) {
        color.setAlphaF(color.alphaF() * (0.7 + 0.3 * grooveAnimationOpacity));
    }

    _helper->renderScrollBarHandle(painter, handleRect, color, palette.color(QPalette::Window));
    return true;
}

//___________________________________________________________________________________
bool Style::drawScrollBarAddLineControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // do nothing if no buttons are defined
    if (_addLineButtons == NoButton) {
        return true;
    }

    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return true;
    }

    if ((sliderOption->minimum == sliderOption->maximum) || (shouldAutoHideArrows(widget) && (sliderOption->sliderPosition >= (sliderOption->maximum))))
        return true;

    const State &state(option->state);
    const bool horizontal(state & State_Horizontal);
    const bool reverseLayout(option->direction == Qt::RightToLeft);

    // adjust rect, based on number of buttons to be drawn
    auto rect(scrollBarInternalSubControlRect(sliderOption, SC_ScrollBarAddLine));

    // need to make it center due to the thin line separator
    if (option->state & State_Horizontal) {
        rect.setTop(PenWidth::Frame);
    } else if (option->direction == Qt::RightToLeft) {
        rect.setRight(rect.right() - qRound(PenWidth::Frame));
    } else {
        rect.setLeft(PenWidth::Frame);
    }

    QColor color;
    QStyleOptionSlider copy(*sliderOption);
    if (_addLineButtons == DoubleButton) {
        if (horizontal) {
            // Draw the arrows
            const QSize halfSize(rect.width() / 2, rect.height());
            const QRect leftSubButton(rect.topLeft(), halfSize);
            const QRect rightSubButton(leftSubButton.topRight() + QPoint(1, 0), halfSize);

            copy.rect = leftSubButton;
            color = scrollBarArrowColor(&copy, reverseLayout ? SC_ScrollBarAddLine : SC_ScrollBarSubLine, widget);
            _helper->renderArrow(painter, leftSubButton, color, ArrowLeft);

            copy.rect = rightSubButton;
            color = scrollBarArrowColor(&copy, reverseLayout ? SC_ScrollBarSubLine : SC_ScrollBarAddLine, widget);
            _helper->renderArrow(painter, rightSubButton, color, ArrowRight);

        } else {
            const QSize halfSize(rect.width(), rect.height() / 2);
            const QRect topSubButton(rect.topLeft(), halfSize);
            const QRect botSubButton(topSubButton.bottomLeft() + QPoint(0, 1), halfSize);

            copy.rect = topSubButton;
            color = scrollBarArrowColor(&copy, SC_ScrollBarSubLine, widget);
            _helper->renderArrow(painter, topSubButton, color, ArrowUp);

            copy.rect = botSubButton;
            color = scrollBarArrowColor(&copy, SC_ScrollBarAddLine, widget);
            _helper->renderArrow(painter, botSubButton, color, ArrowDown);
        }

    } else if (_addLineButtons == SingleButton) {
        copy.rect = rect;
        color = scrollBarArrowColor(&copy, SC_ScrollBarAddLine, widget);
        if (horizontal) {
            if (reverseLayout)
                _helper->renderArrow(painter, rect, color, ArrowLeft);
            else
                _helper->renderArrow(painter, rect, color, ArrowRight);
        } else
            _helper->renderArrow(painter, rect, color, ArrowDown);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawScrollBarSubLineControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // do nothing if no buttons are set
    if (_subLineButtons == NoButton) {
        return true;
    }

    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return true;
    }

    if ((sliderOption->minimum == sliderOption->maximum) || (shouldAutoHideArrows(widget) && (sliderOption->sliderPosition <= (sliderOption->minimum))))
        return true;

    const State &state(option->state);
    const bool horizontal(state & State_Horizontal);
    const bool reverseLayout(option->direction == Qt::RightToLeft);

    // adjust rect, based on number of buttons to be drawn
    auto rect(scrollBarInternalSubControlRect(sliderOption, SC_ScrollBarSubLine));

    // need to make it center due to the thin line separator
    if (option->state & State_Horizontal) {
        rect.setTop(PenWidth::Frame);
    } else if (option->direction == Qt::RightToLeft) {
        rect.setRight(rect.right() - qRound(PenWidth::Frame));
    } else {
        rect.setLeft(PenWidth::Frame);
    }

    QColor color;
    QStyleOptionSlider copy(*sliderOption);
    if (_subLineButtons == DoubleButton) {
        if (horizontal) {
            // Draw the arrows
            const QSize halfSize(rect.width() / 2, rect.height());
            const QRect leftSubButton(rect.topLeft(), halfSize);
            const QRect rightSubButton(leftSubButton.topRight() + QPoint(1, 0), halfSize);

            copy.rect = leftSubButton;
            color = scrollBarArrowColor(&copy, reverseLayout ? SC_ScrollBarAddLine : SC_ScrollBarSubLine, widget);
            _helper->renderArrow(painter, leftSubButton, color, ArrowLeft);

            copy.rect = rightSubButton;
            color = scrollBarArrowColor(&copy, reverseLayout ? SC_ScrollBarSubLine : SC_ScrollBarAddLine, widget);
            _helper->renderArrow(painter, rightSubButton, color, ArrowRight);

        } else {
            const QSize halfSize(rect.width(), rect.height() / 2);
            const QRect topSubButton(rect.topLeft(), halfSize);
            const QRect botSubButton(topSubButton.bottomLeft() + QPoint(0, 1), halfSize);

            copy.rect = topSubButton;
            color = scrollBarArrowColor(&copy, SC_ScrollBarSubLine, widget);
            _helper->renderArrow(painter, topSubButton, color, ArrowUp);

            copy.rect = botSubButton;
            color = scrollBarArrowColor(&copy, SC_ScrollBarAddLine, widget);
            _helper->renderArrow(painter, botSubButton, color, ArrowDown);
        }

    } else if (_subLineButtons == SingleButton) {
        copy.rect = rect;
        color = scrollBarArrowColor(&copy, SC_ScrollBarSubLine, widget);
        if (horizontal) {
            if (reverseLayout)
                _helper->renderArrow(painter, rect, color, ArrowRight);
            else
                _helper->renderArrow(painter, rect, color, ArrowLeft);

        } else {
            _helper->renderArrow(painter, rect, color, ArrowUp);
        }
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawShapedFrameControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto frameOpt = qstyleoption_cast<const QStyleOptionFrame *>(option);
    if (!frameOpt) {
        return false;
    }

    switch (frameOpt->frameShape) {
    case QFrame::Box: {
        if (option->state & State_Sunken) {
            return true;
        } else {
            break;
        }
    }

    case QFrame::HLine:
    case QFrame::VLine: {
        const auto &rect(option->rect);
        const auto color(_helper->separatorColor(option->palette));
        const bool isVertical(frameOpt->frameShape == QFrame::VLine);
        _helper->renderSeparator(painter, rect, color, isVertical);
        return true;
    }

    case QFrame::StyledPanel: {
        if (isQtQuickControl(option, widget) && option->styleObject->property("elementType").toString() == QLatin1String("combobox")) {
            // ComboBox popup frame
            drawFrameMenuPrimitive(option, painter, widget);
            return true;
        }

        // pixelMetric(PM_DefaultFrameWidth) contains heuristics to decide
        // when to draw a frame and return 0 when we shouldn't draw a frame.
        return pixelMetric(PM_DefaultFrameWidth, option, widget) == 0;
    }

    default:
        break;
    }

    return false;
}

// Adapted from QMacStylePrivate::drawFocusRing()
bool Style::drawFocusFrame(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    const QFocusFrame *focusFrame = qobject_cast<const QFocusFrame *>(widget);
    const QWidget *targetWidget = focusFrame ? focusFrame->widget() : nullptr;
    // If we have a QFocusFrame and no target widget, don't draw anything.
    if (focusFrame && !targetWidget) {
        return true;
    }

    const int hmargin = proxy()->pixelMetric(QStyle::PM_FocusFrameHMargin, option, widget);
    const int vmargin = proxy()->pixelMetric(QStyle::PM_FocusFrameVMargin, option, widget);
    // Not using targetWidget->rect() to get this because this should still work when widget is null.
    const QRect targetWidgetRect = option->rect.adjusted(hmargin, vmargin, -hmargin, -vmargin);

    QRect innerRect = targetWidgetRect;
    QRect outerRect = option->rect;

    qreal innerRadius = Metrics::Frame_FrameRadius;
    qreal outerRadius = innerRadius + vmargin;

    QPainterPath focusFramePath;
    focusFramePath.setFillRule(Qt::OddEvenFill);

    // If focusFrame is null, make it still possible to draw something since
    // most draw functions allow drawing something when the widget is null
    if (!focusFrame) {
        focusFramePath.addRoundedRect(innerRect, innerRadius, innerRadius);
        focusFramePath.addRoundedRect(outerRect, outerRadius, outerRadius);
    } else if (targetWidget->inherits("QLineEdit") || targetWidget->inherits("QTextEdit") || targetWidget->inherits("QAbstractSpinBox")
               || targetWidget->inherits("QComboBox") || targetWidget->inherits("QPushButton") || targetWidget->inherits("QToolButton")) {
        /* It's OK to check for QAbstractSpinBox instead of more spacific classes
         * because QAbstractSpinBox handles the painting for spinboxes,
         * unlike most abstract widget classes.
         */
        innerRect.adjust(1, 1, -1, -1);
        focusFramePath.addRoundedRect(innerRect, innerRadius, innerRadius);
        outerRect = innerRect.adjusted(-hmargin, -vmargin, hmargin, vmargin);
        focusFramePath.addRoundedRect(outerRect, outerRadius, outerRadius);
    } else if (auto checkBox = qobject_cast<const QCheckBox *>(targetWidget)) {
        QStyleOptionButton opt;
        opt.initFrom(checkBox);
        if (checkBox->isDown()) {
            opt.state |= QStyle::State_Sunken;
        }
        if (checkBox->isTristate()) {
            opt.state |= QStyle::State_NoChange;
        } else if (checkBox->isChecked()) {
            opt.state |= QStyle::State_On;
        } else {
            opt.state |= QStyle::State_Off;
        }
        opt.text = checkBox->text();
        opt.icon = checkBox->icon();
        opt.iconSize = checkBox->iconSize();
        innerRect = subElementRect(SE_CheckBoxIndicator, &opt, checkBox);
        innerRect.adjust(2, 2, -2, -2);
        innerRect.translate(hmargin, vmargin);
        focusFramePath.addRoundedRect(innerRect, innerRadius, innerRadius);
        outerRect = innerRect.adjusted(-hmargin, -vmargin, hmargin, vmargin);
        focusFramePath.addRoundedRect(outerRect, outerRadius, outerRadius);
    } else if (auto radioButton = qobject_cast<const QRadioButton *>(targetWidget)) {
        QStyleOptionButton opt;
        opt.initFrom(radioButton);
        if (radioButton->isDown()) {
            opt.state |= QStyle::State_Sunken;
        }
        if (radioButton->isChecked()) {
            opt.state |= QStyle::State_On;
        } else {
            opt.state |= QStyle::State_Off;
        }
        opt.text = radioButton->text();
        opt.icon = radioButton->icon();
        opt.iconSize = radioButton->iconSize();
        innerRect = subElementRect(SE_RadioButtonIndicator, &opt, radioButton);
        innerRect.adjust(2, 2, -2, -2);
        innerRect.translate(hmargin, vmargin);
        innerRadius = innerRect.height() / 2.0;
        focusFramePath.addRoundedRect(innerRect, innerRadius, innerRadius);
        outerRect = innerRect.adjusted(-hmargin, -vmargin, hmargin, vmargin);
        outerRadius = outerRect.height() / 2.0;
        focusFramePath.addRoundedRect(outerRect, outerRadius, outerRadius);
    } else if (auto slider = qobject_cast<const QSlider *>(targetWidget)) {
        QStyleOptionSlider opt;
        _helper->initSliderStyleOption(slider, &opt);
        innerRect = subControlRect(CC_Slider, &opt, SC_SliderHandle, slider);
        auto outerRect = _helper->pathForSliderHandleFocusFrame(focusFramePath, innerRect, hmargin, vmargin);
        if (_focusFrame) {
            // Workaround QFocusFrame not fully repainting outside the bounds of the targetWidget
            auto previousRect = _focusFrame->property("_lastOuterRect").value<QRectF>();
            if (previousRect != outerRect) {
                _focusFrame->update();
                _focusFrame->setProperty("_lastOuterRect", outerRect);
            }
        }
    } else if (auto dial = qobject_cast<const QDial *>(targetWidget)) {
        QStyleOptionSlider opt;
        opt.initFrom(dial);
        opt.maximum = dial->maximum();
        opt.minimum = dial->minimum();
        opt.sliderPosition = dial->sliderPosition();
        opt.sliderValue = dial->value();
        opt.singleStep = dial->singleStep();
        opt.pageStep = dial->pageStep();
        opt.upsideDown = !dial->invertedAppearance();
        opt.notchTarget = dial->notchTarget();
        opt.dialWrapping = dial->wrapping();
        if (!dial->notchesVisible()) {
            opt.subControls &= ~QStyle::SC_DialTickmarks;
            opt.tickPosition = QSlider::TicksAbove;
        } else {
            opt.tickPosition = QSlider::NoTicks;
        }
        opt.tickInterval = dial->notchSize();
        innerRect = subControlRect(CC_Dial, &opt, SC_DialHandle, dial);
        innerRect.adjust(1, 1, -1, -1);
        innerRect.translate(hmargin, vmargin);
        innerRadius = innerRect.height() / 2.0;
        focusFramePath.addRoundedRect(innerRect, innerRadius, innerRadius);
        outerRect = innerRect.adjusted(-hmargin, -vmargin, hmargin, vmargin);
        outerRadius = outerRect.height() / 2.0;
        focusFramePath.addRoundedRect(outerRect, outerRadius, outerRadius);
        if (_focusFrame) {
            // Workaround QFocusFrame not fully repainting outside the bounds of the targetWidget
            auto previousRect = _focusFrame->property("_lastOuterRect").value<QRect>();
            if (previousRect != outerRect) {
                _focusFrame->update();
                _focusFrame->setProperty("_lastOuterRect", outerRect);
            }
        }
    } else if (auto groupBox = qobject_cast<const QGroupBox *>(targetWidget)) {
        QStyleOptionGroupBox opt;
        opt.initFrom(groupBox);
        opt.lineWidth = 1;
        opt.midLineWidth = 0;
        opt.textAlignment = groupBox->alignment();
        opt.subControls = QStyle::SC_GroupBoxFrame;
        if (groupBox->isCheckable()) {
            opt.subControls |= QStyle::SC_GroupBoxCheckBox;
            if (groupBox->isChecked()) {
                opt.state |= QStyle::State_On;
            } else {
                opt.state |= QStyle::State_Off;
            }
            // NOTE: we can't get the sunken state of the checkbox
        }
        opt.text = groupBox->title();
        if (!opt.text.isEmpty()) {
            opt.subControls |= QStyle::SC_GroupBoxLabel;
        }
        innerRect = subControlRect(CC_GroupBox, &opt, SC_GroupBoxCheckBox, groupBox);
        innerRect.adjust(2, 2, -2, -2);
        innerRect.translate(hmargin, vmargin);
        innerRect = visualRect(option, innerRect);
        focusFramePath.addRoundedRect(innerRect, innerRadius, innerRadius);
        outerRect = innerRect.adjusted(-hmargin, -vmargin, hmargin, vmargin);
        focusFramePath.addRoundedRect(outerRect, outerRadius, outerRadius);
    } else {
        focusFramePath.addRoundedRect(innerRect, innerRadius, innerRadius);
        focusFramePath.addRoundedRect(outerRect, outerRadius, outerRadius);
    }

    auto outerColor = _helper->alphaColor(option->palette.highlight().color(), 0.33);

    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(focusFramePath, outerColor);
    return true;
}

//___________________________________________________________________________________
bool Style::drawRubberBandControl(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);

    const auto &palette(option->palette);
    const auto outline = KColorUtils::lighten(palette.color(HighlightColor));
    auto background = palette.color(HighlightColor);
    background.setAlphaF(0.20);

    painter->setPen(outline);
    painter->setBrush(background);
    painter->drawRoundedRect(_helper->strokedRect(option->rect), Metrics::Frame_FrameRadius, Metrics::Frame_FrameRadius);

    painter->restore();
    return true;
}

//___________________________________________________________________________________
bool Style::drawHeaderSectionControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    const auto &rect(option->rect);
    const auto &palette(option->palette);
    const auto &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool mouseOver(enabled && (state & State_MouseOver));
    const bool sunken(enabled && (state & (State_On | State_Sunken)));

    const auto headerOption(qstyleoption_cast<const QStyleOptionHeader *>(option));
    if (!headerOption) {
        return true;
    }

    const bool horizontal(headerOption->orientation == Qt::Horizontal);
    const bool isCorner(widget && widget->inherits("QTableCornerButton"));
    const bool reverseLayout(option->direction == Qt::RightToLeft);

    // update animation state
    _animations->headerViewEngine().updateState(widget, rect.topLeft(), mouseOver);
    const bool animated(enabled && _animations->headerViewEngine().isAnimated(widget, rect.topLeft()));
    const qreal opacity(_animations->headerViewEngine().opacity(widget, rect.topLeft()));

    // fill
    const auto &normal = palette.color(QPalette::Button);
    const auto focus(KColorUtils::mix(normal, _helper->focusColor(palette), 0.2));
    const auto hover(KColorUtils::mix(normal, _helper->hoverColor(palette), 0.2));

    QColor color;
    if (sunken) {
        color = focus;
    } else if (animated) {
        color = KColorUtils::mix(normal, hover, opacity);
    } else if (mouseOver) {
        color = hover;
    } else {
        color = normal;
    }

    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(color);
    painter->setPen(Qt::NoPen);
    painter->drawRect(rect);

    // outline
    painter->setBrush(Qt::NoBrush);
    painter->setPen(_helper->alphaColor(palette.color(QPalette::WindowText), Metrics::Bias_Default));

    if (isCorner) {
        if (reverseLayout) {
            painter->drawPoint(rect.bottomLeft());
        } else {
            painter->drawPoint(rect.bottomRight());
        }

    } else if (horizontal) {
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    } else {
        if (reverseLayout) {
            painter->drawLine(rect.topLeft(), rect.bottomLeft());
        } else {
            painter->drawLine(rect.topRight(), rect.bottomRight());
        }
    }

    // separators
    painter->setPen(_helper->alphaColor(palette.color(QPalette::WindowText), Metrics::Bias_Default));

    // If the separator is next to a "HeaderEmptyArea", skip it and let that draw
    // the separator instead. This means that those separators are only visible when necessary.

    if (horizontal) {
        if (headerOption->position != QStyleOptionHeader::OnlyOneSection) {
            if (reverseLayout && headerOption->position != QStyleOptionHeader::Beginning) {
                painter->drawLine(rect.topLeft(), rect.bottomLeft() - QPoint(0, 1));
            } else if (!reverseLayout && headerOption->position != QStyleOptionHeader::End) {
                painter->drawLine(rect.topRight(), rect.bottomRight() - QPoint(0, 1));
            }
        }

    } else if (headerOption->position != QStyleOptionHeader::End) {
        if (reverseLayout) {
            painter->drawLine(rect.bottomLeft() + QPoint(1, 0), rect.bottomRight());
        } else {
            painter->drawLine(rect.bottomLeft(), rect.bottomRight() - QPoint(1, 0));
        }
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawHeaderEmptyAreaControl(const QStyleOption *option, QPainter *painter, const QWidget *) const
{
    // use the same background as in drawHeaderPrimitive
    const auto &rect(option->rect);
    auto palette(option->palette);

    const bool horizontal(option->state & QStyle::State_Horizontal);
    const bool reverseLayout(option->direction == Qt::RightToLeft);

    // fill
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(palette.color(QPalette::Button));
    painter->setPen(Qt::NoPen);
    painter->drawRect(rect);

    // outline
    painter->setBrush(Qt::NoBrush);
    painter->setPen(_helper->alphaColor(palette.color(QPalette::ButtonText), 0.1));

    if (horizontal) {
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());

    } else {
        if (reverseLayout) {
            painter->drawLine(rect.topLeft(), rect.bottomLeft());
        } else {
            painter->drawLine(rect.topRight(), rect.bottomRight());
        }
    }

    // separators
    painter->setPen(_helper->alphaColor(palette.color(QPalette::WindowText), 0.2));

    if (horizontal) {
        // 26aa20407d in qtbase introduced a 1px empty area in reversed horizontal headers. Ignore it.
        if (reverseLayout && rect.width() != 1) {
            painter->drawLine(rect.topRight(), rect.bottomRight() - QPoint(0, 1));
        } else if (!reverseLayout) {
            painter->drawLine(rect.topLeft(), rect.bottomLeft() - QPoint(0, 1));
        }

    } else {
        if (reverseLayout) {
            painter->drawLine(rect.topLeft() + QPoint(1, 0), rect.topRight());
        } else {
            painter->drawLine(rect.topLeft(), rect.topRight() - QPoint(1, 0));
        }
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawTabBarTabLabelControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // call parent style method

    // we draw the focus ourselves so we don't need the parent to do it for us.
    auto old = option->state;
    // clear the State_HasFocus bit
    const_cast<QStyleOption *>(option)->state &= ~State_HasFocus;
    ParentStyleClass::drawControl(CE_TabBarTabLabel, option, painter, widget);
    const_cast<QStyleOption *>(option)->state = old;

    // store rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // check focus
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool selected(state & State_Selected);
    const bool hasFocus(enabled && selected && (state & State_HasFocus));

    // update mouse over animation state
    _animations->tabBarEngine().updateState(widget, rect.topLeft(), AnimationFocus, hasFocus);
    const bool animated(enabled && selected && _animations->tabBarEngine().isAnimated(widget, rect.topLeft(), AnimationFocus));
    const qreal opacity(_animations->tabBarEngine().opacity(widget, rect.topLeft(), AnimationFocus));

    if (!(hasFocus || animated)) {
        return true;
    }

    // code is copied from QCommonStyle, but adds focus
    // cast option and check
    const auto tabOption(qstyleoption_cast<const QStyleOptionTab *>(option));
    if (!tabOption || tabOption->text.isEmpty()) {
        return true;
    }

    // tab option rect
    const bool verticalTabs(isVerticalTab(tabOption));
    const int textFlags(Qt::AlignCenter | _mnemonics->textFlags());

    // text rect
    auto textRect(subElementRect(SE_TabBarTabText, option, widget));

    if (verticalTabs) {
        // properly rotate painter
        painter->save();
        int newX, newY, newRot;
        if (tabOption->shape == QTabBar::RoundedEast || tabOption->shape == QTabBar::TriangularEast) {
            newX = rect.width() + rect.x();
            newY = rect.y();
            newRot = 90;

        } else {
            newX = rect.x();
            newY = rect.y() + rect.height();
            newRot = -90;
        }

        QTransform transform;
        transform.translate(newX, newY);
        transform.rotate(newRot);
        painter->setTransform(transform, true);
    }

    // adjust text rect based on font metrics
    textRect = option->fontMetrics.boundingRect(textRect, textFlags, tabOption->text);

    // focus color
    QColor focusColor;
    if (animated) {
        focusColor = _helper->alphaColor(_helper->focusColor(palette), opacity);
    } else if (hasFocus) {
        focusColor = _helper->focusColor(palette);
    }

    // render focus line
    _helper->renderFocusLine(painter, textRect, focusColor);

    if (verticalTabs) {
        painter->restore();
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawTabBarTabShapeControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    const auto tabOption(qstyleoption_cast<const QStyleOptionTab *>(option));
    if (!tabOption) {
        return true;
    }

    // palette and state
    const bool enabled = option->state & State_Enabled;
    const bool activeFocus = option->state & State_HasFocus;
    const bool visualFocus = activeFocus && option->state & QStyle::State_KeyboardFocusChange;
    const bool hovered = option->state & State_MouseOver;
    const bool down = option->state & State_Sunken;
    const bool selected = option->state & State_Selected;
    const bool north = tabOption->shape == QTabBar::RoundedNorth || tabOption->shape == QTabBar::TriangularNorth;
    const bool south = tabOption->shape == QTabBar::RoundedSouth || tabOption->shape == QTabBar::TriangularSouth;
    const bool west = tabOption->shape == QTabBar::RoundedWest || tabOption->shape == QTabBar::TriangularWest;
    const bool east = tabOption->shape == QTabBar::RoundedEast || tabOption->shape == QTabBar::TriangularEast;

    // check if tab is being dragged
    const bool isDragged(widget && selected && painter->device() != widget);
    const bool isLocked(widget && _tabBarData->isLocked(widget));

    // store rect
    auto rect(option->rect);

    // update mouse over animation state
    _animations->tabBarEngine().updateState(widget, rect.topLeft(), AnimationHover, hovered && !selected && enabled);
    const qreal animation = _animations->tabBarEngine().opacity(widget, rect.topLeft(), AnimationHover);

    // lock state
    if (selected && widget && isDragged) {
        _tabBarData->lock(widget);
    } else if (widget && selected && _tabBarData->isLocked(widget)) {
        _tabBarData->release();
    }

    // tab position
    const QStyleOptionTab::TabPosition &position = tabOption->position;
    const bool isSingle(position == QStyleOptionTab::OnlyOneTab);
    const bool isQtQuickControl(this->isQtQuickControl(option, widget));
    bool isFirst(isSingle || position == QStyleOptionTab::Beginning);
    bool isLast(isSingle || position == QStyleOptionTab::End);
    bool isLeftOfSelected(!isLocked && tabOption->selectedPosition == QStyleOptionTab::NextIsSelected);
    bool isRightOfSelected(!isLocked && tabOption->selectedPosition == QStyleOptionTab::PreviousIsSelected);

    // true if widget is aligned to the frame
    // need to check for 'isRightOfSelected' because for some reason the isFirst flag is set when active tab is being moved
    isFirst &= !isRightOfSelected;
    isLast &= !isLeftOfSelected;

    // swap state based on reverse layout, so that they become layout independent
    const bool reverseLayout(option->direction == Qt::RightToLeft);
    const bool verticalTabs(isVerticalTab(tabOption));
    if (reverseLayout && !verticalTabs) {
        qSwap(isFirst, isLast);
        qSwap(isLeftOfSelected, isRightOfSelected);
    }

    // overlap
    // for QtQuickControls, ovelap is already accounted of in the option. Unlike in the qwidget case
    const int overlap = isQtQuickControl ? 0 : Metrics::TabBar_TabOverlap;

    // adjust rect and define corners based on tabbar orientation
    Corners corners;
    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        if (selected) {
            corners = CornersTop;
        } else {
            if (isFirst) {
                corners |= CornerTopLeft;
            }
            if (isLast) {
                corners |= CornerTopRight;
            }
            if (isRightOfSelected) {
                rect.adjust(-Metrics::Frame_FrameRadius, 0, 0, 0);
            }
            if (isLeftOfSelected) {
                rect.adjust(0, 0, Metrics::Frame_FrameRadius, 0);
            } else if (!isLast) {
                rect.adjust(0, 0, overlap, 0);
            }
        }
        break;

    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        if (selected) {
            corners = CornersBottom;
        } else {
            if (isFirst) {
                corners |= CornerBottomLeft;
            }
            if (isLast) {
                corners |= CornerBottomRight;
            }
            if (isRightOfSelected) {
                rect.adjust(-Metrics::Frame_FrameRadius, 0, 0, 0);
            }
            if (isLeftOfSelected) {
                rect.adjust(0, 0, Metrics::Frame_FrameRadius, 0);
            } else if (!isLast) {
                rect.adjust(0, 0, overlap, 0);
            }
        }
        break;

    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        if (selected) {
            corners = CornersLeft;
        } else {
            if (isFirst) {
                corners |= CornerTopLeft;
            }
            if (isLast) {
                corners |= CornerBottomLeft;
            }
            if (isRightOfSelected) {
                rect.adjust(0, -Metrics::Frame_FrameRadius, 0, 0);
            }
            if (isLeftOfSelected) {
                rect.adjust(0, 0, 0, Metrics::Frame_FrameRadius);
            } else if (!isLast) {
                rect.adjust(0, 0, 0, overlap);
            }
        }
        break;

    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        if (selected) {
            corners = CornersRight;
        } else {
            if (isFirst) {
                corners |= CornerTopRight;
            }
            if (isLast) {
                corners |= CornerBottomRight;
            }
            if (isRightOfSelected) {
                rect.adjust(0, -Metrics::Frame_FrameRadius, 0, 0);
            }
            if (isLeftOfSelected) {
                rect.adjust(0, 0, 0, Metrics::Frame_FrameRadius);
            } else if (!isLast) {
                rect.adjust(0, 0, 0, overlap);
            }
        }
        break;

    default:
        break;
    }
    switch (tabOption->shape) {
    case QTabBar::RoundedNorth:
    case QTabBar::TriangularNorth:
        rect.adjust(0, 0, 0, 1);
        break;
    case QTabBar::RoundedSouth:
    case QTabBar::TriangularSouth:
        rect.adjust(0, 0, 0, -1);
        break;
    case QTabBar::RoundedWest:
    case QTabBar::TriangularWest:
        rect.adjust(0, 0, 1, 0);
        break;
    case QTabBar::RoundedEast:
    case QTabBar::TriangularEast:
        rect.adjust(0, 0, -1, 0);
        break;
    }

    QHash<QByteArray, bool> stateProperties;
    stateProperties["enabled"] = enabled;
    stateProperties["visualFocus"] = visualFocus;
    stateProperties["hovered"] = hovered;
    stateProperties["down"] = down;
    stateProperties["selected"] = selected;
    stateProperties["documentMode"] = true;
    stateProperties["north"] = north;
    stateProperties["south"] = south;
    stateProperties["west"] = west;
    stateProperties["east"] = east;
    stateProperties["isQtQuickControl"] = isQtQuickControl;
    stateProperties["hasAlteredBackground"] = hasAlteredBackground(widget);
    _helper->renderTabBarTab(painter, rect, option->palette, stateProperties, corners, animation);

    return true;
}

//___________________________________________________________________________________
bool Style::drawToolBoxTabLabelControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // rendering is similar to drawPushButtonLabelControl
    // cast option and check
    const auto toolBoxOption(qstyleoption_cast<const QStyleOptionToolBox *>(option));
    if (!toolBoxOption) {
        return true;
    }

    // copy palette
    const auto &palette(option->palette);

    const State &state(option->state);
    const bool enabled(state & State_Enabled);

    // text alignment
    const int textFlags(_mnemonics->textFlags() | Qt::AlignCenter);

    // contents rect
    const auto rect(subElementRect(SE_ToolBoxTabContents, option, widget));

    // store icon size
    const int iconSize(pixelMetric(QStyle::PM_SmallIconSize, option, widget));

    // find contents size and rect
    auto contentsRect(rect);
    QSize contentsSize;
    if (!toolBoxOption->text.isEmpty()) {
        contentsSize = option->fontMetrics.size(_mnemonics->textFlags(), toolBoxOption->text);
        if (!toolBoxOption->icon.isNull()) {
            contentsSize.rwidth() += Metrics::ToolBox_TabItemSpacing;
        }
    }

    // icon size
    if (!toolBoxOption->icon.isNull()) {
        contentsSize.setHeight(qMax(contentsSize.height(), iconSize));
        contentsSize.rwidth() += iconSize;
    }

    // adjust contents rect
    contentsRect = centerRect(contentsRect, contentsSize);

    // render icon
    if (!toolBoxOption->icon.isNull()) {
        // icon rect
        QRect iconRect;
        if (toolBoxOption->text.isEmpty()) {
            iconRect = centerRect(contentsRect, iconSize, iconSize);
        } else {
            iconRect = contentsRect;
            iconRect.setWidth(iconSize);
            iconRect = centerRect(iconRect, iconSize, iconSize);
            contentsRect.setLeft(iconRect.right() + Metrics::ToolBox_TabItemSpacing + 1);
        }

        iconRect = visualRect(option, iconRect);
        const QIcon::Mode mode(enabled ? QIcon::Normal : QIcon::Disabled);
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
        const QPixmap pixmap(_helper->coloredIcon(toolBoxOption->icon, toolBoxOption->palette, iconRect.size(), dpr, mode));
        drawItemPixmap(painter, iconRect, textFlags, pixmap);
    }

    // render text
    if (!toolBoxOption->text.isEmpty()) {
        contentsRect = visualRect(option, contentsRect);
        drawItemText(painter, contentsRect, textFlags, palette, enabled, toolBoxOption->text, QPalette::WindowText);
    }

    return true;
}

//___________________________________________________________________________________
bool Style::drawToolBoxTabShapeControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto toolBoxOption(qstyleoption_cast<const QStyleOptionToolBox *>(option));
    if (!toolBoxOption) {
        return true;
    }

    // copy rect and palette
    const auto &rect(option->rect);
    const auto tabRect(toolBoxTabContentsRect(option, widget));

    /*
     * important: option returns the wrong palette.
     * we use the widget palette instead, when set
     */
    const auto &palette(widget ? widget->palette() : option->palette);

    // store flags
    const State &flags(option->state);
    const bool enabled(flags & State_Enabled);
    const bool selected(flags & State_Selected);
    const bool mouseOver(enabled && !selected && (flags & State_MouseOver));

    // update animation state
    /*
     * the proper widget ( the toolbox tab ) is not passed as argument by Qt.
     * What is passed is the toolbox directly. To implement animations properly,
     *the painter->device() is used instead
     */
    bool isAnimated(false);
    qreal opacity(AnimationData::OpacityInvalid);
    QPaintDevice *device = painter->device();
    if (enabled && device) {
        _animations->toolBoxEngine().updateState(device, mouseOver);
        isAnimated = _animations->toolBoxEngine().isAnimated(device);
        opacity = _animations->toolBoxEngine().opacity(device);
    }

    // color
    QColor outline;
    if (selected) {
        outline = _helper->focusColor(palette);
    } else {
        outline = _helper->frameOutlineColor(palette, mouseOver, false, opacity, isAnimated ? AnimationHover : AnimationNone);
    }

    // render
    _helper->renderToolBoxFrame(painter, rect, tabRect.width(), outline);

    return true;
}

//___________________________________________________________________________________
bool Style::drawDockWidgetTitleControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto dockWidgetOption = qstyleoption_cast<const QStyleOptionDockWidget *>(option);
    if (!dockWidgetOption) {
        return true;
    }

    const auto &palette(option->palette);
    const auto &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool reverseLayout(option->direction == Qt::RightToLeft);

    // cast to v2 to check vertical bar
    const bool verticalTitleBar(dockWidgetOption->verticalTitleBar);

    const auto buttonRect(subElementRect(dockWidgetOption->floatable ? SE_DockWidgetFloatButton : SE_DockWidgetCloseButton, option, widget));

    // get rectangle and adjust to properly accounts for buttons
    auto rect(insideMargin(dockWidgetOption->rect, Metrics::Frame_FrameWidth));
    if (verticalTitleBar) {
        if (buttonRect.isValid()) {
            rect.setTop(buttonRect.bottom() + 1);
        }

    } else if (reverseLayout) {
        if (buttonRect.isValid()) {
            rect.setLeft(buttonRect.right() + 1);
        }
        rect.adjust(0, 0, -4, 0);

    } else {
        if (buttonRect.isValid()) {
            rect.setRight(buttonRect.left() - 1);
        }
        rect.adjust(4, 0, 0, 0);
    }

    if (!verticalTitleBar) {
        auto rect(option->rect);
        rect.setY(rect.height() - 1);
        rect.setHeight(1);
        auto palette = option->palette;
        palette.setCurrentColorGroup(QPalette::Disabled);
        const auto color(_helper->separatorColor(palette));
        _helper->renderSeparator(painter, rect, color, false);
    }

    QString title(dockWidgetOption->title);
    int titleWidth = dockWidgetOption->fontMetrics.size(_mnemonics->textFlags(), title).width();
    int width = verticalTitleBar ? rect.height() : rect.width();
    if (width < titleWidth) {
        title = dockWidgetOption->fontMetrics.elidedText(title, Qt::ElideRight, width, Qt::TextShowMnemonic);
    }

    if (verticalTitleBar) {
        QSize size = rect.size();
        size.transpose();
        rect.setSize(size);

        painter->save();
        painter->translate(rect.left(), rect.top() + rect.width());
        painter->rotate(-90);
        painter->translate(-rect.left(), -rect.top());
        drawItemText(painter, rect, Qt::AlignLeft | Qt::AlignVCenter | _mnemonics->textFlags(), palette, enabled, title, QPalette::WindowText);
        painter->restore();

    } else {
        drawItemText(painter, rect, Qt::AlignLeft | Qt::AlignVCenter | _mnemonics->textFlags(), palette, enabled, title, QPalette::WindowText);
    }

    return true;
}

//______________________________________________________________
bool Style::drawSplitterControl(const QStyleOption *option, QPainter *painter, [[maybe_unused]] const QWidget *widget) const
{
    painter->setBrush(_helper->separatorColor(option->palette));
    painter->setPen(Qt::NoPen);
    painter->drawRect(option->rect);
    return true;
}

//______________________________________________________________
bool Style::drawGroupBoxComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto groupBoxOption = qstyleoption_cast<const QStyleOptionGroupBox *>(option);
    if (!groupBoxOption) {
        return true;
    }

    // Text for label
    QFont font = qApp->font();
    if (groupBoxOption->features == QStyleOptionFrame::Flat && !(groupBoxOption->subControls & SC_GroupBoxCheckBox)) {
        font.setPointSize(font.pointSize() + 1);
        font.setBold(true);
    }
    QFontMetrics fontMetrics(font);

    // Copied from QCommonStyle but change the font of the title
    // Draw frame
    QRect textRect = subControlRect(CC_GroupBox, option, SC_GroupBoxLabel, widget);
    QRect checkBoxRect = subControlRect(CC_GroupBox, option, SC_GroupBoxCheckBox, widget);
    if (groupBoxOption->subControls & QStyle::SC_GroupBoxFrame) {
        QStyleOptionFrame frame;
        frame.QStyleOption::operator=(*groupBoxOption);
        frame.features = groupBoxOption->features;
        frame.lineWidth = groupBoxOption->lineWidth;
        frame.midLineWidth = groupBoxOption->midLineWidth;
        frame.rect = subControlRect(CC_GroupBox, option, SC_GroupBoxFrame, widget);
        BreezePrivate::PainterStateSaver pss(painter);
        QRegion region(groupBoxOption->rect);
        if (!groupBoxOption->text.isEmpty()) {
            bool ltr = groupBoxOption->direction == Qt::LeftToRight;
            QRect finalRect;
            if (groupBoxOption->subControls & QStyle::SC_GroupBoxCheckBox) {
                finalRect = checkBoxRect.united(textRect);
                finalRect.adjust(ltr ? -Metrics::GroupBox_TitleMarginWidth : 0, 0, ltr ? 0 : Metrics::GroupBox_TitleMarginWidth, 0);
            } else {
                finalRect = textRect;
            }
            region -= finalRect;
        }
        painter->setClipRegion(region);
        drawPrimitive(PE_FrameGroupBox, &frame, painter, widget);
    }

    // Draw title
    if ((groupBoxOption->subControls & QStyle::SC_GroupBoxLabel) && !groupBoxOption->text.isEmpty()) {
        BreezePrivate::PainterStateSaver painterStateSaver(painter);

        painter->setFont(font);

        QColor textColor = groupBoxOption->textColor;
        if (textColor.isValid())
            painter->setPen(textColor);
        int alignment = int(groupBoxOption->textAlignment);
        if (!styleHint(QStyle::SH_UnderlineShortcut, option, widget))
            alignment |= Qt::TextHideMnemonic;

        drawItemText(painter,
                     textRect,
                     Qt::TextShowMnemonic | Qt::AlignHCenter | alignment,
                     groupBoxOption->palette,
                     groupBoxOption->state & State_Enabled,
                     groupBoxOption->text,
                     textColor.isValid() ? QPalette::NoRole : QPalette::WindowText);

        if (groupBoxOption->state & State_HasFocus) {
            QStyleOptionFocusRect fropt;
            fropt.QStyleOption::operator=(*groupBoxOption);
            fropt.rect = textRect;
            drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
        }
    }

    // Draw checkbox
    if (groupBoxOption->subControls & SC_GroupBoxCheckBox) {
        QStyleOptionButton box;
        box.QStyleOption::operator=(*groupBoxOption);
        box.rect = checkBoxRect;
        drawPrimitive(PE_IndicatorCheckBox, &box, painter, widget);
    }

    // do nothing if either label is not selected or groupbox is empty
    if (!(option->subControls & QStyle::SC_GroupBoxLabel) || groupBoxOption->text.isEmpty()) {
        return true;
    }

    // store palette and rect
    const auto &palette(option->palette);

    // check focus state
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool hasFocus(enabled && (option->state & State_HasFocus));
    if (!hasFocus) {
        return true;
    }

    // alignment
    const int textFlags(groupBoxOption->textAlignment | _mnemonics->textFlags());

    // update animation state
    _animations->widgetStateEngine().updateState(widget, AnimationFocus, hasFocus);
    const bool isFocusAnimated(_animations->widgetStateEngine().isAnimated(widget, AnimationFocus));
    const qreal opacity(_animations->widgetStateEngine().opacity(widget, AnimationFocus));

    // get relevant rect
    textRect = fontMetrics.boundingRect(textRect, textFlags, groupBoxOption->text);

    // focus color
    QColor focusColor;
    if (isFocusAnimated) {
        focusColor = _helper->alphaColor(_helper->focusColor(palette), opacity);
    } else if (hasFocus) {
        focusColor = _helper->focusColor(palette);
    }

    // render focus
    _helper->renderFocusLine(painter, textRect, focusColor);

    return true;
}

//______________________________________________________________
bool Style::drawToolButtonComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto toolButtonOption(qstyleoption_cast<const QStyleOptionToolButton *>(option));
    if (!toolButtonOption) {
        return true;
    }

    // need to alter palette for focused buttons
    const bool activeFocus = option->state & QStyle::State_HasFocus;
    const bool hovered = option->state & QStyle::State_MouseOver;
    bool flat = option->state & QStyle::State_AutoRaise;

    // update animation state
    // mouse over takes precedence over focus
    _animations->widgetStateEngine().updateState(widget, AnimationHover, hovered);
    _animations->widgetStateEngine().updateState(widget, AnimationFocus, activeFocus && !hovered);

    // detect buttons in tabbar, for which special rendering is needed
    const bool inTabBar(widget && qobject_cast<const QTabBar *>(widget->parentWidget()));

    // copy option and alter palette
    QStyleOptionToolButton copy(*toolButtonOption);

    const auto menuStyle = BreezePrivate::toolButtonMenuArrowStyle(option);

    const auto buttonRect(subControlRect(CC_ToolButton, option, SC_ToolButton, widget));
    const auto menuRect(subControlRect(CC_ToolButton, option, SC_ToolButtonMenu, widget));

    // frame
    if (toolButtonOption->subControls & SC_ToolButton) {
        if (!flat) {
            copy.rect = buttonRect;
        }
        if (inTabBar) {
            drawTabBarPanelButtonToolPrimitive(&copy, painter, widget);
        } else {
            drawPrimitive(PE_PanelButtonTool, &copy, painter, widget);
        }
    }

    // arrow
    if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::SubControl) {
        copy.rect = menuRect;
        drawPrimitive(PE_IndicatorButtonDropDown, &copy, painter, widget);

        copy.state &= ~State_MouseOver;
        copy.state &= ~State_Sunken;
        copy.state &= ~State_On;
        drawPrimitive(PE_IndicatorArrowDown, &copy, painter, widget);

    } else if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::InlineSmall || menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::InlineLarge) {
        copy.rect = menuRect;
        copy.state &= ~State_MouseOver;
        copy.state &= ~State_Sunken;
        copy.state &= ~State_On;

        if (menuStyle == BreezePrivate::ToolButtonMenuArrowStyle::InlineSmall) {
            drawIndicatorArrowPrimitive(ArrowDown, &copy, painter, widget);
        } else {
            if (option->direction == Qt::RightToLeft) {
                copy.rect.translate(Metrics::Button_ItemSpacing, 0);
            } else {
                copy.rect.translate(-Metrics::Button_ItemSpacing, 0);
            }
            drawIndicatorArrowPrimitive(ArrowDown, &copy, painter, widget);
        }
    }

    // contents
    {
        // restore state
        copy.state = option->state;

        // define contents rect
        auto contentsRect(buttonRect);

        // detect dock widget title button
        // for dockwidget title buttons, do not take out margins, so that the icons do not get scaled down
        const bool isDockWidgetTitleButton(widget && widget->inherits("QDockWidgetTitleButton"));
        if (isDockWidgetTitleButton) {
            // cast to abstract button
            // adjust state to have correct icon rendered
            const auto button(qobject_cast<const QAbstractButton *>(widget));
            if (button->isChecked() || button->isDown()) {
                copy.state |= State_On;
            }
        }

        copy.rect = contentsRect;

        // render
        drawControl(CE_ToolButtonLabel, &copy, painter, widget);
    }

    return true;
}

//______________________________________________________________
bool Style::drawComboBoxComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    // state
    bool enabled = option->state & QStyle::State_Enabled;
    bool activeFocus = option->state & QStyle::State_HasFocus;
    bool visualFocus = activeFocus && option->state & QStyle::State_KeyboardFocusChange && (widget == nullptr || widget->focusProxy() == nullptr);
    bool hovered = option->state & QStyle::State_MouseOver;
    // Only true if the arrow (SC_ComboBoxArrow) is pressed
    bool down = option->state & QStyle::State_Sunken;
    // Only true if the background (QComboBoxPrivateContainer) is pressed
    bool checked = option->state & QStyle::State_On;
    bool flat = false;
    bool editable = false;
    bool hasNeutralHighlight = hasHighlightNeutral(widget, option);

    // cast option and check
    const auto comboBoxOption = qstyleoption_cast<const QStyleOptionComboBox *>(option);
    if (comboBoxOption) {
        flat = !comboBoxOption->frame;
        editable = comboBoxOption->editable;
    }

    // frame
    if (option->subControls & SC_ComboBoxFrame) {
        if (editable) {
            flat |= (option->rect.height() <= 2 * Metrics::Frame_FrameWidth + Metrics::MenuButton_IndicatorWidth);
            if (flat) {
                const auto &background = option->palette.color(QPalette::Base);
                painter->setBrush(background);
                painter->setPen(Qt::NoPen);
                painter->drawRect(option->rect);
            } else {
                drawPrimitive(PE_FrameLineEdit, option, painter, widget);
            }
        } else {
            // NOTE: Using focus animation for bg down because the pressed animation only works on press when enabled for buttons and not on release.
            _animations->widgetStateEngine().updateState(widget, AnimationFocus, (down || checked) && enabled);
            // NOTE: Using hover animation for all pen animations to prevent flickering when closing the menu.
            _animations->widgetStateEngine().updateState(widget, AnimationHover, (hovered || visualFocus || down || checked) && enabled);
            qreal bgAnimation = _animations->widgetStateEngine().opacity(widget, AnimationFocus);
            qreal penAnimation = _animations->widgetStateEngine().opacity(widget, AnimationHover);

            QHash<QByteArray, bool> stateProperties;
            stateProperties["enabled"] = enabled;
            stateProperties["visualFocus"] = visualFocus;
            stateProperties["hovered"] = hovered;
            // See notes for down and checked above.
            stateProperties["down"] = down || checked;
            stateProperties["flat"] = flat;
            stateProperties["hasNeutralHighlight"] = hasNeutralHighlight;
            stateProperties["isActiveWindow"] = widget ? widget->isActiveWindow() : true;

            _helper->renderButtonFrame(painter, option->rect, option->palette, stateProperties, bgAnimation, penAnimation);
        }
    }

    // arrow
    if (option->subControls & SC_ComboBoxArrow) {
        // detect empty comboboxes
        const auto comboBox = qobject_cast<const QComboBox *>(widget);
        const bool empty(comboBox && !comboBox->count());

        // arrow color
        QColor arrowColor;
        if (editable) {
            if (empty || !enabled) {
                arrowColor = option->palette.color(QPalette::Disabled, QPalette::Text);
            } else {
                // check animation state
                const bool subControlHover(enabled && hovered && option->activeSubControls & SC_ComboBoxArrow);
                _animations->comboBoxEngine().updateState(widget, AnimationHover, subControlHover);

                const bool animated(enabled && _animations->comboBoxEngine().isAnimated(widget, AnimationHover));
                const qreal opacity(_animations->comboBoxEngine().opacity(widget, AnimationHover));

                // color
                const auto normal(_helper->arrowColor(option->palette, QPalette::WindowText));
                const auto hover(_helper->hoverColor(option->palette));

                if (animated) {
                    arrowColor = KColorUtils::mix(normal, hover, opacity);
                } else if (subControlHover) {
                    arrowColor = hover;
                } else {
                    arrowColor = normal;
                }
            }
        } else if (flat) {
            if (empty || !enabled) {
                arrowColor = _helper->arrowColor(option->palette, QPalette::Disabled, QPalette::WindowText);
            } else if (activeFocus && !hovered && down) {
                arrowColor = option->palette.color(QPalette::WindowText);
            } else {
                arrowColor = _helper->arrowColor(option->palette, QPalette::WindowText);
            }
        } else if (empty || !enabled) {
            arrowColor = _helper->arrowColor(option->palette, QPalette::Disabled, QPalette::ButtonText);
        } else if (activeFocus && !hovered) {
            arrowColor = option->palette.color(QPalette::WindowText);
        } else {
            arrowColor = _helper->arrowColor(option->palette, QPalette::ButtonText);
        }

        // arrow rect
        auto arrowRect(subControlRect(CC_ComboBox, option, SC_ComboBoxArrow, widget));

        // render
        _helper->renderArrow(painter, arrowRect, arrowColor, ArrowDown);
    }
    return true;
}

//______________________________________________________________
bool Style::drawSpinBoxComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    const auto spinBoxOption(qstyleoption_cast<const QStyleOptionSpinBox *>(option));
    if (!spinBoxOption) {
        return true;
    }

    // store palette and rect
    const auto &palette(option->palette);
    const auto &rect(option->rect);

    if (option->subControls & SC_SpinBoxFrame) {
        // detect flat spinboxes
        bool flat(!spinBoxOption->frame);
        flat |= (rect.height() < 2 * Metrics::Frame_FrameWidth + Metrics::SpinBox_ArrowButtonWidth);
        if (flat) {
            const auto &background = palette.color(QPalette::Base);

            painter->setBrush(background);
            painter->setPen(Qt::NoPen);
            painter->drawRect(rect);

        } else {
            drawPrimitive(PE_FrameLineEdit, option, painter, widget);
        }
    }

    if (option->subControls & SC_SpinBoxUp) {
        renderSpinBoxArrow(SC_SpinBoxUp, spinBoxOption, painter, widget);
    }
    if (option->subControls & SC_SpinBoxDown) {
        renderSpinBoxArrow(SC_SpinBoxDown, spinBoxOption, painter, widget);
    }

    return true;
}

//______________________________________________________________
bool Style::drawSliderComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return true;
    }

    // copy rect and palette
    const auto &rect(option->rect);
    const auto &palette(option->palette);

    // copy state
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool mouseOver(enabled && (state & State_MouseOver));
    const bool hasFocus(enabled && (state & State_HasFocus));

    // direction
    const bool horizontal(sliderOption->orientation == Qt::Horizontal);

    // retrieve rects of sub controls
    const auto grooveRect(subControlRect(CC_Slider, sliderOption, SC_SliderGroove, widget));
    const auto handleRect(subControlRect(CC_Slider, sliderOption, SC_SliderHandle, widget));

    // tickmarks
    if (StyleConfigData::sliderDrawTickMarks() && (sliderOption->subControls & SC_SliderTickmarks)) {
        const bool upsideDown(sliderOption->upsideDown);
        const int tickPosition(sliderOption->tickPosition);
        const int available(pixelMetric(PM_SliderSpaceAvailable, option, widget));
        int interval = sliderOption->tickInterval;
        if (interval < 1) {
            interval = sliderOption->pageStep;
        }
        if (interval >= 1) {
            const int fudge(pixelMetric(PM_SliderLength, option, widget) / 2);
            int current(sliderOption->minimum);

            // store tick lines
            QList<QLine> tickLines;
            if (horizontal) {
                if (tickPosition & QSlider::TicksAbove) {
                    tickLines.append(QLine(rect.left(),
                                           grooveRect.top() - Metrics::Slider_TickMarginWidth,
                                           rect.left(),
                                           grooveRect.top() - Metrics::Slider_TickMarginWidth - Metrics::Slider_TickLength));
                }
                if (tickPosition & QSlider::TicksBelow) {
                    tickLines.append(QLine(rect.left(),
                                           grooveRect.bottom() + Metrics::Slider_TickMarginWidth,
                                           rect.left(),
                                           grooveRect.bottom() + Metrics::Slider_TickMarginWidth + Metrics::Slider_TickLength));
                }

            } else {
                if (tickPosition & QSlider::TicksAbove) {
                    tickLines.append(QLine(grooveRect.left() - Metrics::Slider_TickMarginWidth,
                                           rect.top(),
                                           grooveRect.left() - Metrics::Slider_TickMarginWidth - Metrics::Slider_TickLength,
                                           rect.top()));
                }
                if (tickPosition & QSlider::TicksBelow) {
                    tickLines.append(QLine(grooveRect.right() + Metrics::Slider_TickMarginWidth,
                                           rect.top(),
                                           grooveRect.right() + Metrics::Slider_TickMarginWidth + Metrics::Slider_TickLength,
                                           rect.top()));
                }
            }

            // colors
            const auto reverse(option->direction == Qt::RightToLeft);
            const auto base(_helper->separatorColor(palette));
            const auto &highlight = hasHighlightNeutral(widget, option, mouseOver, hasFocus) ? _helper->neutralText(palette) : palette.color(HighlightColor);

            while (current <= sliderOption->maximum) {
                // adjust color
                const auto color((enabled && current <= sliderOption->sliderPosition) ? highlight : base);
                painter->setPen(color);

                // calculate positions and draw lines
                const int position(sliderPositionFromValue(sliderOption->minimum, sliderOption->maximum, current, available, upsideDown) + fudge);
                for (const QLine &tickLine : std::as_const(tickLines)) {
                    if (horizontal) {
                        painter->drawLine(tickLine.translated(reverse ? (rect.width() - position) : position, 0));
                    } else {
                        painter->drawLine(tickLine.translated(0, position));
                    }
                }

                // go to next position
                current += interval;
            }
        }
    }

    // groove
    if (sliderOption->subControls & SC_SliderGroove) {
        // base color
        const auto grooveColor(_helper->alphaColor(palette.color(QPalette::WindowText), 0.2));

        if (!enabled) {
            _helper->renderSliderGroove(painter, grooveRect, grooveColor, palette.color(QPalette::Window));
        } else {
            const bool upsideDown(sliderOption->upsideDown);

            // highlight color
            const auto &highlight = hasHighlightNeutral(widget, option, mouseOver, hasFocus) ? _helper->neutralText(palette) : palette.color(HighlightColor);

            if (sliderOption->orientation == Qt::Horizontal) {
                auto leftRect(grooveRect);
                leftRect.setRight(handleRect.right() - Metrics::Slider_ControlThickness / 2);

                auto rightRect(grooveRect);
                rightRect.setLeft(handleRect.left() + Metrics::Slider_ControlThickness / 2);

                if (option->direction == Qt::RightToLeft) {
                    std::swap(leftRect, rightRect);
                }

                // Background
                _helper->renderSliderGroove(painter, leftRect.united(rightRect), grooveColor, palette.color(QPalette::Window));
                // Fill
                _helper->renderSliderGroove(painter, upsideDown ? rightRect : leftRect, highlight, palette.color(QPalette::Window));

            } else {
                auto topRect(grooveRect);
                auto bottomRect(grooveRect);
                topRect.setBottom(handleRect.bottom() - Metrics::Slider_ControlThickness / 2);
                bottomRect.setTop(handleRect.top() + Metrics::Slider_ControlThickness / 2);

                // Background
                _helper->renderSliderGroove(painter, topRect.united(bottomRect), grooveColor, palette.color(QPalette::Window));
                // Fill
                _helper->renderSliderGroove(painter, upsideDown ? bottomRect : topRect, highlight, palette.color(QPalette::Window));
            }
        }
    }

    // handle
    if (sliderOption->subControls & SC_SliderHandle) {
        // handle state
        const bool handleActive(sliderOption->activeSubControls & SC_SliderHandle);
        const bool sunken(state & (State_On | State_Sunken));

        // animation state
        _animations->widgetStateEngine().updateState(widget, AnimationHover, handleActive && mouseOver);
        _animations->widgetStateEngine().updateState(widget, AnimationFocus, hasFocus);
        const AnimationMode mode(_animations->widgetStateEngine().buttonAnimationMode(widget));
        const qreal opacity(_animations->widgetStateEngine().buttonOpacity(widget));

        // define colors
        const auto &background = palette.color(QPalette::Button);
        auto outline(_helper->sliderOutlineColor(palette, handleActive && mouseOver, hasFocus, opacity, mode));
        if (hasFocus || (handleActive && mouseOver)) {
            outline = hasHighlightNeutral(widget, option, handleActive && mouseOver, hasFocus)
                ? _helper->neutralText(palette).lighter(mouseOver || hasFocus ? 150 : 100)
                : outline;
        }
        const auto shadow(_helper->shadowColor(palette));

        // render
        _helper->renderSliderHandle(painter, handleRect, background, outline, shadow, sunken);
    }

    return true;
}

//______________________________________________________________
bool Style::drawDialComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto sliderOption(qstyleoption_cast<const QStyleOptionSlider *>(option));
    if (!sliderOption) {
        return true;
    }

    const auto &palette(option->palette);
    const State &state(option->state);
    const bool enabled(state & State_Enabled);
    const bool mouseOver(enabled && (state & State_MouseOver));
    const bool hasFocus(enabled && (state & State_HasFocus));

    // do not render tickmarks
    if (sliderOption->subControls & SC_DialTickmarks) { }

    // groove
    if (sliderOption->subControls & SC_DialGroove) {
        // groove rect
        auto grooveRect(subControlRect(CC_Dial, sliderOption, SC_SliderGroove, widget));

        // groove
        const auto grooveColor(KColorUtils::mix(palette.color(QPalette::Window), palette.color(QPalette::WindowText), 0.2));

        // angles
        const qreal first(dialAngle(sliderOption, sliderOption->minimum));
        const qreal last(dialAngle(sliderOption, sliderOption->maximum));

        // render groove
        _helper->renderDialGroove(painter, grooveRect, grooveColor, palette.color(QPalette::Window), first, last);

        if (enabled) {
            // highlight
            const auto &highlight = palette.color(HighlightColor);

            // angles
            const qreal second(dialAngle(sliderOption, sliderOption->sliderPosition));

            // render contents
            _helper->renderDialGroove(painter, grooveRect, highlight, palette.color(QPalette::Window), first, second);
        }
    }

    // handle
    if (sliderOption->subControls & SC_DialHandle) {
        // get handle rect
        auto handleRect(subControlRect(CC_Dial, sliderOption, SC_DialHandle, widget));
        handleRect = centerRect(handleRect, Metrics::Slider_ControlThickness, Metrics::Slider_ControlThickness);

        // handle state
        const bool handleActive(mouseOver && handleRect.contains(_animations->dialEngine().position(widget)));
        const bool sunken(state & (State_On | State_Sunken));

        // animation state
        _animations->dialEngine().setHandleRect(widget, handleRect);
        _animations->dialEngine().updateState(widget, AnimationHover, handleActive && mouseOver);
        _animations->dialEngine().updateState(widget, AnimationFocus, hasFocus);
        const auto mode(_animations->dialEngine().buttonAnimationMode(widget));
        const qreal opacity(_animations->dialEngine().buttonOpacity(widget));

        // define colors
        const auto &background = palette.color(QPalette::Button);
        const auto outline(_helper->sliderOutlineColor(palette, handleActive && mouseOver, hasFocus, opacity, mode));
        const auto shadow(_helper->shadowColor(palette));

        // render
        _helper->renderSliderHandle(painter, handleRect, background, outline, shadow, sunken);
    }

    return true;
}

//______________________________________________________________
bool Style::drawScrollBarComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    if (!option) {
        qCWarning(KLASSY) << "Style::drawScrollBarComplexControl: Style can't draw scrollbar without options";
        return true;
    }
    // the animation for QStyle::SC_ScrollBarGroove is special: it will animate
    // the opacity of everything else as well, included slider and arrows
    qreal opacity(_animations->scrollBarEngine().opacity(widget, QStyle::SC_ScrollBarGroove));
    const bool animated(StyleConfigData::animationsEnabled() && _animations->scrollBarEngine().isAnimated(widget, AnimationHover, QStyle::SC_ScrollBarGroove));
    const bool mouseOver(option->state & State_MouseOver);

    if (opacity == AnimationData::OpacityInvalid) {
        opacity = 1;
    }

    if (StyleConfigData::scrollBarSeparator()) {
        QRect separatorRect;
        if (option->state & State_Horizontal) {
            separatorRect = QRect(0, 0, option->rect.width(), PenWidth::Frame);
        } else {
            separatorRect = alignedRect(option->direction, Qt::AlignLeft, QSize(PenWidth::Frame, option->rect.height()), option->rect);
        }

        _helper->renderScrollBarBorder(painter, separatorRect, _helper->alphaColor(option->palette.color(QPalette::Text), Metrics::Bias_Default));
    }

    // render full groove directly, rather than using the addPage and subPage control element methods
    if ((!StyleConfigData::animationsEnabled() || mouseOver || animated) && option->subControls & SC_ScrollBarGroove) {
        // retrieve groove rectangle
        auto grooveRect(subControlRect(CC_ScrollBar, option, SC_ScrollBarGroove, widget));

        // need to make it center due to the thin line separator
        if (option->state & State_Horizontal) {
            grooveRect.setTop(PenWidth::Frame);
        } else if (option->direction == Qt::RightToLeft) {
            grooveRect.setRight(grooveRect.right() - qRound(PenWidth::Frame));
        } else {
            grooveRect.setLeft(PenWidth::Frame);
        }

        const auto &palette(option->palette);
        const auto color(_helper->alphaColor(palette.color(QPalette::WindowText), 0.2 * (animated ? opacity : 1)));
        const auto &state(option->state);
        const bool horizontal(state & State_Horizontal);

        if (horizontal) {
            if (mouseOver) {
                grooveRect = centerRect(grooveRect, grooveRect.width(), Metrics::ScrollBar_SliderWidthMouseOver);
            } else
                grooveRect = centerRect(grooveRect, grooveRect.width(), Metrics::ScrollBar_SliderWidthMouseNotOver);
        } else {
            if (mouseOver) {
                grooveRect = centerRect(grooveRect, Metrics::ScrollBar_SliderWidthMouseOver, grooveRect.height());
            } else
                grooveRect = centerRect(grooveRect, Metrics::ScrollBar_SliderWidthMouseNotOver, grooveRect.height());
        }

        // render
        _helper->renderScrollBarGroove(painter, grooveRect, color);
    }

    // call base class primitive
    ParentStyleClass::drawComplexControl(CC_ScrollBar, option, painter, widget);

    return true;
}

//______________________________________________________________
bool Style::drawTitleBarComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    // cast option and check
    const auto titleBarOption(qstyleoption_cast<const QStyleOptionTitleBar *>(option));
    if (!titleBarOption) {
        return true;
    }

    // store palette and rect
    auto palette(option->palette);
    const auto &rect(option->rect);

    const State &flags(option->state);
    const bool enabled(flags & State_Enabled);
    const bool active(enabled && (titleBarOption->titleBarState & Qt::WindowActive));
    QColor titleBarColor = _helper->titleBarColor(active);
    titleBarColor.setAlpha(255); // PAM: set titlebar opaque for now

    if (titleBarOption->subControls & SC_TitleBarLabel) {
        // render background
        painter->setClipRect(rect);
        const auto outline(active ? QColor() : _helper->frameOutlineColor(palette, false, false));
        const auto background(titleBarColor);
        _helper->renderTabWidgetFrame(painter, rect.adjusted(-1, -1, 1, 3), background, outline, CornersTop);

        const bool useSeparator(_helper->decorationConfig()->drawTitleBarSeparator() && active && titleBarColor != palette.color(QPalette::Window)
                                && !(titleBarOption->titleBarState & Qt::WindowMinimized));

        if (useSeparator) {
            painter->setRenderHint(QPainter::Antialiasing, false);
            painter->setBrush(Qt::NoBrush);
            QPen pen(_helper->buttonFocusColor(palette));
            painter->setPen(pen);
            painter->drawLine(QPoint(rect.left(), rect.top() + rect.height()), QPoint(rect.left() + rect.width(), rect.top() + rect.height()));
        }

        // render text
        palette.setColor(QPalette::WindowText, _helper->titleBarTextColor(active));
        const auto textRect(subControlRect(CC_TitleBar, option, SC_TitleBarLabel, widget));
        ParentStyleClass::drawItemText(painter, textRect, Qt::AlignCenter, palette, active, titleBarOption->text, QPalette::WindowText);
    }

    // buttons
    static const QList<SubControl> subControls = {SC_TitleBarMinButton,
                                                  SC_TitleBarMaxButton,
                                                  SC_TitleBarCloseButton,
                                                  SC_TitleBarNormalButton,
                                                  SC_TitleBarSysMenu};

    // loop over supported buttons
    for (const SubControl &subControl : subControls) {
        // skip if not requested
        if (!(titleBarOption->subControls & subControl)) {
            continue;
        }

        // find matching icon
        QIcon icon;
        switch (subControl) {
        case SC_TitleBarMinButton:
            icon = standardIcon(SP_TitleBarMinButton, option, widget);
            break;
        case SC_TitleBarMaxButton:
            icon = standardIcon(SP_TitleBarMaxButton, option, widget);
            break;
        case SC_TitleBarCloseButton:
            icon = standardIcon(SP_TitleBarCloseButton, option, widget);
            break;
        case SC_TitleBarNormalButton:
            icon = standardIcon(SP_TitleBarNormalButton, option, widget);
            break;
        case SC_TitleBarSysMenu:
            icon = titleBarOption->icon;
            break;
        default:
            break;
        }

        // check icon
        if (icon.isNull()) {
            continue;
        }

        // define icon rect
        auto iconRect(subControlRect(CC_TitleBar, option, subControl, widget));
        if (iconRect.isEmpty()) {
            continue;
        }

        // active state
        const bool subControlActive(titleBarOption->activeSubControls & subControl);

        // mouse over state
        const bool mouseOver(!subControlActive && widget && iconRect.translated(widget->mapToGlobal(QPoint(0, 0))).contains(QCursor::pos()));

        // adjust iconRect
        const int iconWidth(pixelMetric(PM_SmallIconSize, option, widget));
        const QSize iconSize(iconWidth, iconWidth);
        iconRect = centerRect(iconRect, iconSize);

        // set icon mode and state
        QIcon::Mode iconMode;
        QIcon::State iconState;

        if (!enabled) {
            iconMode = QIcon::Disabled;
            iconState = QIcon::Off;

        } else {
            if (subControlActive) { // pressed
                if (active) { // Active MDI titlebar
                    iconMode = QIcon::Selected;
                    iconState = QIcon::On;
                } else { // inactive MDI titlebar
                    iconMode = QIcon::Normal;
                    iconState = QIcon::On;
                }
            } else if (mouseOver) {
                if (active) { // Active MDI titlebar
                    iconMode = QIcon::Disabled; // modified for Klassy to use this otherwise unused mode/state
                    iconState = QIcon::On;
                } else { // inactive MDI titlebar
                    iconMode = QIcon::Active;
                    iconState = QIcon::Off;
                }
            } else {
                if (active) { // Active MDI titlebar
                    iconMode = QIcon::Selected;
                    iconState = QIcon::Off;
                } else { // inactive MDI titlebar
                    iconMode = QIcon::Normal;
                    iconState = QIcon::Off;
                }
            }
        }

        // get pixmap and render
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
        const QPixmap pixmap = _helper->coloredIcon(icon, option->palette, iconSize, dpr, iconMode, iconState);
        drawItemPixmap(painter, iconRect, Qt::AlignCenter, pixmap);
    }

    return true;
}

//____________________________________________________________________________________________________
void Style::renderSpinBoxArrow(const SubControl &subControl, const QStyleOptionSpinBox *option, QPainter *painter, const QWidget *widget) const
{
    const auto &palette(option->palette);
    const State &state(option->state);

    // enable state
    bool enabled(state & State_Enabled);

    // check steps enable step
    const bool atLimit((subControl == SC_SpinBoxUp && !(option->stepEnabled & QAbstractSpinBox::StepUpEnabled))
                       || (subControl == SC_SpinBoxDown && !(option->stepEnabled & QAbstractSpinBox::StepDownEnabled)));

    // update enabled state accordingly
    enabled &= !atLimit;

    // update mouse-over effect
    const bool mouseOver(enabled && (state & State_MouseOver));

    // check animation state
    const bool subControlHover(enabled && mouseOver && (option->activeSubControls & subControl));
    _animations->spinBoxEngine().updateState(widget, subControl, subControlHover);

    const bool animated(enabled && _animations->spinBoxEngine().isAnimated(widget, subControl));
    const qreal opacity(_animations->spinBoxEngine().opacity(widget, subControl));

    auto color = _helper->arrowColor(palette, QPalette::Text);
    if (animated) {
        auto highlight = _helper->hoverColor(palette);
        color = KColorUtils::mix(color, highlight, opacity);

    } else if (subControlHover) {
        color = _helper->hoverColor(palette);

    } else if (atLimit) {
        color = _helper->arrowColor(palette, QPalette::Disabled, QPalette::Text);
    }

    // arrow orientation
    ArrowOrientation orientation((subControl == SC_SpinBoxUp) ? ArrowUp : ArrowDown);

    // arrow rect
    const auto arrowRect(subControlRect(CC_SpinBox, option, subControl, widget));

    // render
    _helper->renderArrow(painter, arrowRect, color, orientation);
}

//______________________________________________________________________________
qreal Style::dialAngle(const QStyleOptionSlider *sliderOption, int value) const
{
    // calculate angle at which handle needs to be drawn
    qreal angle(0);
    if (sliderOption->maximum == sliderOption->minimum) {
        angle = M_PI / 2;
    } else {
        qreal fraction(qreal(value - sliderOption->minimum) / qreal(sliderOption->maximum - sliderOption->minimum));
        if (!sliderOption->upsideDown) {
            fraction = 1 - fraction;
        }

        if (sliderOption->dialWrapping) {
            angle = 1.5 * M_PI - fraction * 2 * M_PI;
        } else {
            angle = (M_PI * 8 - fraction * 10 * M_PI) / 6;
        }
    }

    return angle;
}

//______________________________________________________________________________
const QWidget *Style::scrollBarParent(const QWidget *widget) const
{
    // check widget and parent
    if (!(widget && widget->parentWidget())) {
        return nullptr;
    }

    // try cast to scroll area. Must test both parent and grandparent
    QAbstractScrollArea *scrollArea;
    if (!(scrollArea = qobject_cast<QAbstractScrollArea *>(widget->parentWidget()))) {
        scrollArea = qobject_cast<QAbstractScrollArea *>(widget->parentWidget()->parentWidget());
    }

    // check scrollarea
    if (scrollArea && (widget == scrollArea->verticalScrollBar() || widget == scrollArea->horizontalScrollBar())) {
        return scrollArea;

    } else if (widget->parentWidget()->inherits("KTextEditor::View")) {
        return widget->parentWidget();

    } else {
        return nullptr;
    }
}

//______________________________________________________________________________
QColor Style::scrollBarArrowColor(const QStyleOptionSlider *option, const SubControl &control, const QWidget *widget) const
{
    const auto &rect(option->rect);
    const auto &palette(option->palette);
    auto color(_helper->arrowColor(palette, QPalette::WindowText));

    bool widgetMouseOver((option->state & State_MouseOver));
    if (widget)
        widgetMouseOver = widget->underMouse();
    // in case this QStyle is used by QQuickControls QStyle wrapper
    else if (option->styleObject)
        widgetMouseOver = option->styleObject->property("hover").toBool();

    bool autoHideArrows = shouldAutoHideArrows(widget);

    // check enabled state
    const bool enabled(option->state & State_Enabled);
    if (!enabled) {
        if (autoHideArrows) {
            // finally, global opacity when ScrollBarShowOnMouseOver
            const qreal globalOpacity(_animations->scrollBarEngine().opacity(widget, QStyle::SC_ScrollBarGroove));
            if (globalOpacity >= 0 && widgetMouseOver)
                color.setAlphaF(globalOpacity);
            // no mouse over and no animation in progress, don't draw arrows at all
            else if (!widgetMouseOver)
                return Qt::transparent;
        }
        return color;
    }
    if ((control == SC_ScrollBarSubLine && option->sliderValue == option->minimum)
        || (control == SC_ScrollBarAddLine && option->sliderValue == option->maximum)) {
        // manually disable arrow, to indicate that scrollbar is at limit
        color = _helper->arrowColor(palette, QPalette::Disabled, QPalette::WindowText);
        if (autoHideArrows) {
            // finally, global opacity when ScrollBarShowOnMouseOver
            const qreal globalOpacity(_animations->scrollBarEngine().opacity(widget, QStyle::SC_ScrollBarGroove));
            if (globalOpacity >= 0 && widgetMouseOver)
                color.setAlphaF(globalOpacity);
            // no mouse over and no animation in progress, don't draw arrows at all
            else if (!widgetMouseOver)
                return Qt::transparent;
        }
        return color;
    }

    const bool mouseOver(_animations->scrollBarEngine().isHovered(widget, control));
    const bool animated(_animations->scrollBarEngine().isAnimated(widget, AnimationHover, control));
    const qreal opacity(_animations->scrollBarEngine().opacity(widget, control));

    // retrieve mouse position from engine
    QPoint position(mouseOver ? _animations->scrollBarEngine().position(widget) : QPoint(-1, -1));
    if (mouseOver && rect.contains(position)) {
        /*
         * need to update the arrow controlRect on fly because there is no
         * way to get it from the styles directly, outside of repaint events
         */
        _animations->scrollBarEngine().setSubControlRect(widget, control, rect);
    }

    if (rect.intersects(_animations->scrollBarEngine().subControlRect(widget, control))) {
        auto highlight = _helper->hoverColor(palette);
        if (animated) {
            color = KColorUtils::mix(color, highlight, opacity);

        } else if (mouseOver) {
            color = highlight;
        }

    } else if (option->state & State_MouseOver) {
        const bool activeIsSub = option->activeSubControls & SC_ScrollBarSubLine;
        const bool activeIsAdd = option->activeSubControls & SC_ScrollBarAddLine;
        const bool drawingSub = control == SC_ScrollBarSubLine;
        const bool drawingAdd = control == SC_ScrollBarAddLine;

        if ((activeIsSub && drawingSub) || (activeIsAdd && drawingAdd)) {
            const auto highlight = _helper->hoverColor(palette);

            color = highlight;
        }
    }

    if (autoHideArrows) {
        // finally, global opacity when ScrollBarShowOnMouseOver
        const qreal globalOpacity(_animations->scrollBarEngine().opacity(widget, QStyle::SC_ScrollBarGroove));
        if (globalOpacity >= 0 && widgetMouseOver)
            color.setAlphaF(globalOpacity);
        // no mouse over and no animation in progress, don't draw arrows at all
        else if (!widgetMouseOver)
            return Qt::transparent;
    }

    return color;
}

//____________________________________________________________________________________
void Style::setTranslucentBackground(QWidget *widget) const
{
    widget->setAttribute(Qt::WA_TranslucentBackground);

#ifdef Q_OS_WIN
    // FramelessWindowHint is needed on windows to make WA_TranslucentBackground work properly
    widget->setWindowFlags(widget->windowFlags() | Qt::FramelessWindowHint);
#endif
}

//____________________________________________________________________________________
QIcon Style::toolBarExtensionIcon(StandardPixmap standardPixmap, const QStyleOption *option, const QWidget *widget) const
{
    // store palette
    // due to Qt, it is not always safe to assume that either option, nor widget are defined
    QPalette palette;
    if (option) {
        palette = option->palette;
    } else if (widget) {
        palette = widget->palette();
    } else {
        palette = QApplication::palette();
    }

    const auto direction = option ? option->direction : QGuiApplication::layoutDirection();

    // convenience class to map color to icon mode
    struct IconData {
        QColor _color;
        QIcon::Mode _mode;
        QIcon::State _state;
    };

    // map colors to icon states
    const QList<IconData> iconTypes = {{palette.color(QPalette::Active, QPalette::WindowText), QIcon::Normal, QIcon::Off},
                                       {palette.color(QPalette::Active, QPalette::WindowText), QIcon::Selected, QIcon::Off},
                                       {palette.color(QPalette::Active, QPalette::WindowText), QIcon::Active, QIcon::Off},
                                       {palette.color(QPalette::Disabled, QPalette::WindowText), QIcon::Disabled, QIcon::Off},

                                       {palette.color(QPalette::Active, QPalette::HighlightedText), QIcon::Normal, QIcon::On},
                                       {palette.color(QPalette::Active, QPalette::HighlightedText), QIcon::Selected, QIcon::On},
                                       {palette.color(QPalette::Active, QPalette::WindowText), QIcon::Active, QIcon::On},
                                       {palette.color(QPalette::Disabled, QPalette::WindowText), QIcon::Disabled, QIcon::On}};

    // default icon sizes
    static const QList<int> iconSizes = {8, 16, 22, 32, 48};

    // decide arrow orientation
    const ArrowOrientation orientation(standardPixmap == SP_ToolBarHorizontalExtensionButton ? (direction == Qt::RightToLeft ? ArrowLeft : ArrowRight)
                                                                                             : ArrowDown);

    // create icon and fill
    QIcon icon;
    for (const IconData &iconData : iconTypes) {
        for (const int &iconSize : iconSizes) {
            // create pixmap
            QPixmap pixmap(iconSize, iconSize);
            pixmap.fill(Qt::transparent);

            // render
            QPainter painter(&pixmap);

            // icon size
            const int fixedIconSize(pixelMetric(QStyle::PM_SmallIconSize, option, widget));
            const QRect fixedRect(0, 0, fixedIconSize, fixedIconSize);

            painter.setWindow(fixedRect);
            painter.translate(standardPixmap == SP_ToolBarHorizontalExtensionButton ? QPoint(1, 0) : QPoint(0, 1));
            _helper->renderArrow(&painter, fixedRect, iconData._color, orientation);
            painter.end();

            // add to icon
            icon.addPixmap(pixmap, iconData._mode, iconData._state);
        }
    }

    return icon;
}

bool Style::isTabletMode() const
{
    if (qEnvironmentVariableIsSet("BREEZE_IS_TABLET_MODE")) {
        return qEnvironmentVariableIntValue("BREEZE_IS_TABLET_MODE");
    }
#if BREEZE_HAVE_QTQUICK
    return TabletModeWatcher::self()->isTabletMode();
#else
    return false;
#endif
}

//____________________________________________________________________________________
QIcon Style::titleBarButtonIcon(StandardPixmap standardPixmap, const QStyleOption *option, const QWidget *widget) const
{
    // map standardPixmap to button type
    DecorationButtonType buttonType;
    bool buttonChecked = false;
    switch (standardPixmap) {
    case SP_TitleBarNormalButton:
        buttonType = DecorationButtonType::Maximize;
        buttonChecked = true;
        break;
    case SP_TitleBarMinButton:
        buttonType = DecorationButtonType::Minimize;
        break;
    case SP_TitleBarMaxButton:
        buttonType = DecorationButtonType::Maximize;
        break;
    case SP_TitleBarCloseButton:
    case SP_DockWidgetCloseButton:
        buttonType = DecorationButtonType::Close;
        break;

    default:
        return QIcon();
    }

    // store palette
    // due to Qt, it is not always safe to assume that either option, nor widget are defined
    QPalette palette;
    if (option) {
        palette = option->palette;
    } else if (widget) {
        palette = widget->palette();
    } else {
        palette = QApplication::palette();
    }

    //generate a different DecorationColors for buttons on a toolbar. These set the titlebar background to the toolbar background, and use the inactive button states
    DecorationColors decorationColorsToolbar(false, true);
    palette.setCurrentColorGroup(QPalette::Active);
    const QColor toolbarBase(palette.color(QPalette::Window));
    const QColor toolbarText(KColorUtils::mix(toolbarBase, palette.color(QPalette::WindowText), 0.7));
    //generate inactive decoration colours only
    decorationColorsToolbar.generateDecorationColors(palette, _helper->decorationConfig(), QColor(), QColor(), toolbarText, toolbarBase, "", true, false);
    DecorationButtonPalette decorationButtonPaletteToolbar(buttonType);
    decorationButtonPaletteToolbar.generate(_helper->decorationConfig(),
                                            _helper->decorationColors()->active(),
                                            decorationColorsToolbar.inactive(),
                                            true,
                                            false); // generate inactive button colours only);

    // active button states which are used for MDI titlebars only
    DecorationButtonPalette decorationButtonPaletteMdi(buttonType);
    decorationButtonPaletteMdi.generate(_helper->decorationConfig(),
                                        _helper->decorationColors()->active(),
                                        decorationColorsToolbar.inactive(),
                                        true,
                                        true); // generate active button colours only

    // convenience class to map color to icon mode
    struct IconData {
        QIcon::Mode _mode;
        QIcon::State _state;
        QColor _foregroundColor;
        bool _cutOutForeground;
        QColor _backgroundColor;
        QColor _outlineColor;
    };

    // map colors to icon states
    const QList<IconData> iconTypes = {
        // state off icons
        {QIcon::Normal, // used for standard widgets and inactive MDI window titlebars (hence using inactive colours)
         QIcon::Off,
         decorationButtonPaletteToolbar.inactive()->foregroundNormal,
         decorationButtonPaletteToolbar.inactive()->cutOutForegroundNormal,
         decorationButtonPaletteToolbar.inactive()->backgroundNormal,
         decorationButtonPaletteToolbar.inactive()->outlineNormal},

        {QIcon::Selected, // used for active MDI window titlebars
         QIcon::Off,
         decorationButtonPaletteMdi.active()->foregroundNormal,
         decorationButtonPaletteMdi.active()->cutOutForegroundNormal,
         decorationButtonPaletteMdi.active()->backgroundNormal,
         decorationButtonPaletteMdi.active()->outlineNormal},

        {QIcon::Active, // hover colours, standard widgets and inactive MDI titlebars
         QIcon::Off,
         decorationButtonPaletteToolbar.inactive()->foregroundHover,
         decorationButtonPaletteToolbar.inactive()->cutOutForegroundHover,
         decorationButtonPaletteToolbar.inactive()->backgroundHover,
         decorationButtonPaletteToolbar.inactive()->outlineHover},

        {QIcon::Disabled,
         QIcon::Off,
         ColorTools::alphaMix(decorationButtonPaletteToolbar.inactive()->foregroundNormal, 0.2),
         false,
         ColorTools::alphaMix(decorationButtonPaletteToolbar.inactive()->backgroundNormal, 0.2),
         ColorTools::alphaMix(decorationButtonPaletteToolbar.inactive()->outlineNormal, 0.2)},

        // state on icons
        {QIcon::Normal, // Pressed colours on a standard widget / inactive
         QIcon::On,
         decorationButtonPaletteToolbar.inactive()->foregroundPress,
         decorationButtonPaletteToolbar.inactive()->cutOutForegroundPress,
         decorationButtonPaletteToolbar.inactive()->backgroundPress,
         decorationButtonPaletteToolbar.inactive()->outlinePress},

        {QIcon::Selected, // Pressed colours on MDI active titlebar
         QIcon::On,
         decorationButtonPaletteMdi.active()->foregroundPress,
         decorationButtonPaletteMdi.active()->cutOutForegroundPress,
         decorationButtonPaletteMdi.active()->backgroundPress,
         decorationButtonPaletteMdi.active()->outlinePress},

        {QIcon::Active, // Same as Normal::On -- needed like this for compatibility in drawToolButtonLabelControl
         QIcon::On,
         decorationButtonPaletteToolbar.inactive()->foregroundPress,
         decorationButtonPaletteToolbar.inactive()->cutOutForegroundPress,
         decorationButtonPaletteToolbar.inactive()->backgroundPress,
         decorationButtonPaletteToolbar.inactive()->outlinePress},

        // This is unused elsewhere, so use instead for Hovered on an active MDI titlebar (drawTitleBarComplexControl modified to use this in Klassy)
        {QIcon::Disabled,
         QIcon::On,
         decorationButtonPaletteMdi.active()->foregroundHover,
         decorationButtonPaletteMdi.active()->cutOutForegroundHover,
         decorationButtonPaletteMdi.active()->backgroundHover,
         decorationButtonPaletteMdi.active()->outlineHover},

    };

    // default icon sizes
    static const QList<int> iconSizes = {8, 16, 22, 32, 48};

    // output icon
    QIcon icon;

    for (const IconData &iconData : iconTypes) {
        for (const int &iconSize : iconSizes) {
            // create pixmap
            QPixmap pixmap(iconSize, iconSize);
            pixmap.fill(Qt::transparent);

            // create painter and render
            QPainter painter(&pixmap);
            _helper->renderDecorationButton(&painter,
                                            pixmap.rect(),
                                            buttonType,
                                            buttonChecked,
                                            iconData._foregroundColor,
                                            iconData._cutOutForeground,
                                            iconData._backgroundColor,
                                            iconData._outlineColor,
                                            palette);

            painter.end();

            // store
            icon.addPixmap(pixmap, iconData._mode, iconData._state);
        }
    }

    return icon;
}

void Style::generateDecorationColorsOnDecorationColorSettingsUpdate(QByteArray uuid)
{
    _helper->setGenerateDecorationColorsOnDecorationColorSettingsUpdateFlag(&uuid);
    loadConfiguration();
}

//____________________________________________________________________
bool Style::isQtQuickControl(const QStyleOption *option, const QWidget *widget) const
{
#if BREEZE_HAVE_QTQUICK
    if (!widget && option) {
        if (const auto item = qobject_cast<QQuickItem *>(option->styleObject)) {
            _windowManager->registerQuickItem(item);
            return true;
        }
    }
#else
    Q_UNUSED(widget);
    Q_UNUSED(option);
#endif
    return false;
}

//____________________________________________________________________
bool Style::showIconsInMenuItems() const
{
    return !QApplication::testAttribute(Qt::AA_DontShowIconsInMenus);
}

//____________________________________________________________________
bool Style::showIconsOnPushButtons() const
{
    const KConfigGroup g(KSharedConfig::openConfig(), QStringLiteral("KDE"));
    return g.readEntry("ShowIconsOnPushButtons", true);
}

//____________________________________________________________________
bool Style::hasAlteredBackground(const QWidget *widget) const
{
    // check widget
    if (!widget) {
        return false;
    }

    // check property
    const QVariant property(widget->property(PropertyNames::alteredBackground));
    if (property.isValid()) {
        return property.toBool();
    }

    // check if widget is of relevant type
    bool hasAlteredBackground(false);
    if (const auto groupBox = qobject_cast<const QGroupBox *>(widget)) {
        hasAlteredBackground = !groupBox->isFlat();
    } else if (const auto tabWidget = qobject_cast<const QTabWidget *>(widget)) {
        hasAlteredBackground = !tabWidget->documentMode();
    } else if (qobject_cast<const QMenu *>(widget)) {
        hasAlteredBackground = true;
    } else if (StyleConfigData::dockWidgetDrawFrame() && qobject_cast<const QDockWidget *>(widget)) {
        hasAlteredBackground = true;
    }

    if (widget->parentWidget() && !hasAlteredBackground) {
        hasAlteredBackground = this->hasAlteredBackground(widget->parentWidget());
    }
    const_cast<QWidget *>(widget)->setProperty(PropertyNames::alteredBackground, hasAlteredBackground);
    return hasAlteredBackground;
}

bool Style::hasHighlightNeutral(const QObject *widget, const QStyleOption *option, [[maybe_unused]] bool mouseOver, [[maybe_unused]] bool focus) const
{
    if (!widget && (!option || !option->styleObject)) {
        return false;
    }

    const QObject *styleObject = widget;
    if (!styleObject) {
        styleObject = option->styleObject;
    }

    const QVariant property(styleObject->property(PropertyNames::highlightNeutral));
    if (property.isValid()) {
        return property.toBool();
    }
    return false;
}

}
