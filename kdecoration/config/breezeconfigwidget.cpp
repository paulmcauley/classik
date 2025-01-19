//////////////////////////////////////////////////////////////////////////////
// breezeconfigwidget.cpp
// -------------------
//
// SPDX-FileCopyrightText: 2009 Hugo Pereira Da Costa <hugo.pereira@free.fr>
// SPDX-FileCopyrightText: 2021-2024 Paul A McAuley <kde@paulmcauley.com>
//
// SPDX-License-Identifier: MIT
//////////////////////////////////////////////////////////////////////////////

#include "breezeconfigwidget.h"
#include "dbusmessages.h"
#include "decorationexceptionlist.h"
#include "presetsmodel.h"
#include "renderdecorationbuttonicon.h"

#include <KLocalizedString>

#include <QIcon>
#include <QRegularExpression>
#include <QScreen>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QTimer>
#include <QWindow>

void initKlassydecorationConfigQrc()
{
    // needed to display images when qrc is statically linked
    // must be in global namespace to work
    Q_INIT_RESOURCE(klassydecoration_config);
}

void cleanupKlassydecorationConfigQrc()
{
    // needed to free qrc resources
    // must be in global namespace to work
    Q_CLEANUP_RESOURCE(klassydecoration_config);
}

namespace Breeze
{

//_________________________________________________________
ConfigWidget::ConfigWidget(QObject *parent, const KPluginMetaData &data, const QVariantList & /*args*/)
    : KCModule(parent, data)
    , m_configuration(KSharedConfig::openConfig(QStringLiteral("klassy/klassyrc")))
    , m_presetsConfiguration(KSharedConfig::openConfig(QStringLiteral("klassy/windecopresetsrc")))
    , m_changed(false)
{
    // this is a hack to get an Apply button
    if (widget() && QCoreApplication::applicationName() == QStringLiteral("systemsettings")) {
        system("kcmshell6 org.kde.kdecoration3.kcm/kcm_klassydecoration.so &");
        if (widget()->window()) {
            widget()->window()->close();
        }
    }
    setButtons(KCModule::Default | KCModule::Apply);

    initKlassydecorationConfigQrc();

    // configuration
    m_ui.setupUi(widget());

    m_ui.defaultExceptions->setKConfig(m_configuration, m_presetsConfiguration);
    m_ui.exceptions->setKConfig(m_configuration, m_presetsConfiguration);

    // add the "Presets..." button
    QVBoxLayout *presetsButtonVLayout = new QVBoxLayout();
    m_presetsButton = new QPushButton(i18n("&Presets..."));
    presetsButtonVLayout->addWidget(m_presetsButton);
    m_presetsButton->setMinimumWidth(125);
    presetsButtonVLayout->setContentsMargins(0, 0, 0, 0);
    m_ui.gridLayout_9->addLayout(presetsButtonVLayout, 0, 0, Qt::AlignRight | Qt::AlignTop);
    connect(m_presetsButton, &QAbstractButton::clicked, this, &ConfigWidget::presetsButtonClicked);

    widget()->setTabOrder(m_presetsButton, m_ui.tabWidget);

    // hide the title if not klassy-settings
    if (widget()->window() && qAppName() != "klassy-settings") {
        m_kPageWidget = widget()->window()->findChild<KPageWidget *>();
        if (m_kPageWidget) {
            KPageWidgetItem *currentPage = m_kPageWidget->currentPage();
            if (currentPage)
                kPageWidgetChanged(currentPage, currentPage); // this line usually is false but currentPage is valid on change
            connect(m_kPageWidget, &KPageWidget::currentPageChanged, this, &ConfigWidget::kPageWidgetChanged);
        }
    }

    // hide the push buttons for default exceptions
    QList<QPushButton *> defaultPushButtons = m_ui.defaultExceptions->findChildren<QPushButton *>();
    for (QPushButton *defaultPushButton : defaultPushButtons) {
        QSizePolicy spRetain = defaultPushButton->sizePolicy();
        spRetain.setRetainSizeWhenHidden(true);
        defaultPushButton->setSizePolicy(spRetain);

        defaultPushButton->hide();
    }

    m_unlockedIcon.addFile(QStringLiteral(":/klassy_config_icons/object-unlocked-symbolic.svg"), QSize(16, 16));
    m_lockedIcon.addFile(QStringLiteral(":/klassy_config_icons/object-locked-symbolic.svg"), QSize(16, 16));
    // add corner icon
    m_ui.cornerRadiusIcon->setPixmap(QIcon::fromTheme(QStringLiteral("tool_curve")).pixmap(16, 16));

    m_systemIconGenerationDialog = new SystemIconGeneration(m_configuration, m_presetsConfiguration, this);
    m_loadPresetDialog = new LoadPreset(m_configuration, m_presetsConfiguration, this);
    m_buttonSizingDialog = new ButtonSizing(m_configuration, m_presetsConfiguration, this);
    m_buttonColorsDialog = new ButtonColors(m_configuration, m_presetsConfiguration, this);
    m_buttonBehaviourDialog = new ButtonBehaviour(m_configuration, m_presetsConfiguration, this);
    m_titleBarSpacingDialog = new TitleBarSpacing(m_configuration, m_presetsConfiguration, this);
    m_titleBarOpacityDialog = new TitleBarOpacity(m_configuration, m_presetsConfiguration, this);
    m_windowOutlineStyleDialog = new WindowOutlineStyle(m_configuration, m_presetsConfiguration, this);
    m_shadowStyleDialog = new ShadowStyle(m_configuration, m_presetsConfiguration, this);

    connect(m_systemIconGenerationDialog, &SystemIconGeneration::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_buttonSizingDialog, &ButtonSizing::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_buttonColorsDialog, &ButtonColors::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_buttonBehaviourDialog, &ButtonBehaviour::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_titleBarSpacingDialog, &TitleBarSpacing::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_titleBarOpacityDialog, &TitleBarOpacity::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_windowOutlineStyleDialog, &WindowOutlineStyle::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_shadowStyleDialog, &ShadowStyle::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);

    // the displayed colours in the ButtonColors UI depend upon ButtonBehaviour
    connect(m_buttonBehaviourDialog, &ButtonBehaviour::saved, m_buttonColorsDialog, &ButtonColors::loadButtonPaletteColorsIcons);

    // update the horizontal header icons in-case the icon style has changed
    connect(this, &ConfigWidget::saved, m_buttonColorsDialog, &ButtonColors::load);

#if KLASSY_GIT_MASTER
    // set the long version string if from the git master
    m_ui.version->setText("v" + klassyLongVersion());

#else
    // set shortened version string in UI if an official release
    QRegularExpression re("\\d+\\.\\d+");
    QRegularExpressionMatch match = re.match(KLASSY_VERSION);
    if (match.hasMatch()) {
        QString matched = match.captured(0);
        m_ui.version->setText("v" + matched);
    }
#endif

    connect(m_ui.systemIconGenerationButton, &QAbstractButton::clicked, this, &ConfigWidget::systemIconGenerationButtonClicked);
    connect(m_ui.buttonSizingButton, &QAbstractButton::clicked, this, &ConfigWidget::buttonSizingButtonClicked);
    connect(m_ui.buttonColorsButton, &QAbstractButton::clicked, this, &ConfigWidget::buttonColorsButtonClicked);
    connect(m_ui.buttonBehaviourButton, &QAbstractButton::clicked, this, &ConfigWidget::buttonBehaviourButtonClicked);
    connect(m_ui.titleBarSpacingButton, &QAbstractButton::clicked, this, &ConfigWidget::titleBarSpacingButtonClicked);
    connect(m_ui.titleBarOpacityButton, &QAbstractButton::clicked, this, &ConfigWidget::titleBarOpacityButtonClicked);
    connect(m_ui.thinWindowOutlineStyleButton, &QAbstractButton::clicked, this, &ConfigWidget::windowOutlineStyleButtonClicked);
    connect(m_ui.shadowStyleButton, &QAbstractButton::clicked, this, &ConfigWidget::shadowStyleButtonClicked);

    updateIconsStackedWidgetVisible();

    // track ui changes
    // direct connections are used in several places so the slot can detect the immediate m_loading status (not available in a queued connection)
    connect(m_ui.buttonIconStyle, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(m_ui.buttonIconStyle, SIGNAL(currentIndexChanged(int)), SLOT(updateIconsStackedWidgetVisible()), Qt::ConnectionType::DirectConnection);
    connect(m_ui.buttonShape, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(m_ui.iconSize, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(m_ui.systemIconSize, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(m_ui.cornerRadius, SIGNAL(valueChanged(double)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(m_ui.boldButtonIcons, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(m_ui.boldButtonIcons, qOverload<int>(&QComboBox::currentIndexChanged), this, &ConfigWidget::updateWindowControlPreviewIcons);
    connect(m_ui.drawBorderOnMaximizedWindows, &QAbstractButton::toggled, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui.drawBackgroundGradient, &QAbstractButton::toggled, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui.drawTitleBarSeparator, &QAbstractButton::toggled, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui.useTitleBarColorForAllBorders, &QAbstractButton::toggled, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui.roundBottomCornersWhenNoBorders, &QAbstractButton::toggled, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui.forceColorizeSystemIcons, &QAbstractButton::toggled, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);

    // only enable animationsSpeed when animationsEnabled is checked
    connect(m_ui.animationsEnabled, &QAbstractButton::toggled, this, &ConfigWidget::setEnabledAnimationsSpeed);

    // track animations changes
    connect(m_ui.animationsEnabled, &QAbstractButton::toggled, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui.animationsSpeedRelativeSystem, SIGNAL(valueChanged(int)), SLOT(updateChanged()));

    connect(m_ui.colorizeThinWindowOutlineWithButton, &QAbstractButton::toggled, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);

    // track exception changes
    connect(m_ui.defaultExceptions, &ExceptionListWidget::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui.exceptions, &ExceptionListWidget::changed, this, &ConfigWidget::updateChanged, Qt::ConnectionType::DirectConnection);

    QApplication::instance()->installEventFilter(this); // to monitor palette changes in the app (can't get them from this)
}

ConfigWidget::~ConfigWidget()
{
    cleanupKlassydecorationConfigQrc();
}

//_________________________________________________________
void ConfigWidget::load()
{
    m_loading = true;

    // create internal settings and load from rc files
    m_internalSettings = InternalSettingsPtr(new InternalSettings());
    m_internalSettings->load();
    m_systemIconGenerationDialog->load();
    m_buttonSizingDialog->load();
    m_buttonColorsDialog->load();
    m_buttonBehaviourDialog->load();
    m_titleBarSpacingDialog->load();
    m_titleBarOpacityDialog->load();
    m_windowOutlineStyleDialog->load();
    m_shadowStyleDialog->load();
    PresetsModel::importBundledPresets(m_presetsConfiguration.data());
    updateIcons();
    updateWindowControlPreviewIcons();

    // assign to ui
    m_ui.buttonIconStyle->setCurrentIndex(m_internalSettings->buttonIconStyle());
    m_ui.buttonShape->setCurrentIndex(m_internalSettings->buttonShape());
    m_ui.iconSize->setCurrentIndex(m_internalSettings->iconSize());
    m_ui.systemIconSize->setCurrentIndex(m_internalSettings->systemIconSize());
    m_ui.cornerRadius->setValue(m_internalSettings->windowCornerRadius());

    m_ui.drawBorderOnMaximizedWindows->setChecked(m_internalSettings->drawBorderOnMaximizedWindows());
    m_ui.boldButtonIcons->setCurrentIndex(m_internalSettings->boldButtonIcons());
    m_ui.drawBackgroundGradient->setChecked(m_internalSettings->drawBackgroundGradient());
    m_ui.drawTitleBarSeparator->setChecked(m_internalSettings->drawTitleBarSeparator());
    m_ui.animationsEnabled->setChecked(m_internalSettings->animationsEnabled());
    m_ui.animationsSpeedRelativeSystem->setValue(m_internalSettings->animationsSpeedRelativeSystem());
    m_ui.useTitleBarColorForAllBorders->setChecked(m_internalSettings->useTitleBarColorForAllBorders());
    m_ui.roundBottomCornersWhenNoBorders->setChecked(m_internalSettings->roundBottomCornersWhenNoBorders());
    m_ui.forceColorizeSystemIcons->setChecked(m_internalSettings->forceColorizeSystemIcons());

    m_ui.colorizeThinWindowOutlineWithButton->setChecked(m_internalSettings->colorizeThinWindowOutlineWithButton());

    updateIconsStackedWidgetVisible();

    // load exceptions
    DecorationExceptionList exceptions;
    exceptions.readConfig(m_configuration);
    if (exceptions.numberDefaults()) {
        m_ui.defaultExceptions->setExceptions(exceptions.getDefault());
    } else {
        m_ui.defaultExceptions->hide();
        m_ui.defaultExceptionsLabel->hide();
        m_ui.defaultExceptionsSpacer->setGeometry(QRect());
    }
    m_ui.exceptions->setExceptions(exceptions.get());
    setNeedsSave(false);
    m_loading = false;
}

//_________________________________________________________
void ConfigWidget::save()
{
    saveMain();
}

void ConfigWidget::saveMain(QString saveAsPresetName)
{
    // create internal settings and load from rc files
    m_internalSettings = InternalSettingsPtr(new InternalSettings());
    m_internalSettings->load();

    // apply modifications from ui
    m_internalSettings->setButtonIconStyle(m_ui.buttonIconStyle->currentIndex());
    m_internalSettings->setButtonShape(m_ui.buttonShape->currentIndex());
    m_internalSettings->setIconSize(m_ui.iconSize->currentIndex());
    m_internalSettings->setSystemIconSize(m_ui.systemIconSize->currentIndex());
    m_internalSettings->setWindowCornerRadius(m_ui.cornerRadius->value());
    m_internalSettings->setBoldButtonIcons(m_ui.boldButtonIcons->currentIndex());
    m_internalSettings->setDrawBorderOnMaximizedWindows(m_ui.drawBorderOnMaximizedWindows->isChecked());
    m_internalSettings->setDrawBackgroundGradient(m_ui.drawBackgroundGradient->isChecked());
    m_internalSettings->setDrawTitleBarSeparator(m_ui.drawTitleBarSeparator->isChecked());
    m_internalSettings->setAnimationsEnabled(m_ui.animationsEnabled->isChecked());
    m_internalSettings->setAnimationsSpeedRelativeSystem(m_ui.animationsSpeedRelativeSystem->value());
    m_internalSettings->setUseTitleBarColorForAllBorders(m_ui.useTitleBarColorForAllBorders->isChecked());
    m_internalSettings->setRoundBottomCornersWhenNoBorders(m_ui.roundBottomCornersWhenNoBorders->isChecked());
    m_internalSettings->setForceColorizeSystemIcons(m_ui.forceColorizeSystemIcons->isChecked());
    m_internalSettings->setColorizeThinWindowOutlineWithButton(m_ui.colorizeThinWindowOutlineWithButton->isChecked());

    m_systemIconGenerationDialog->save(false);
    m_buttonSizingDialog->save(false);
    m_buttonColorsDialog->save(false);
    m_buttonBehaviourDialog->save(false);
    m_titleBarSpacingDialog->save(false);
    m_titleBarOpacityDialog->save(false);
    m_shadowStyleDialog->save(false);
    m_windowOutlineStyleDialog->save(false);

    // save configuration
    m_internalSettings->save();

    // get list of exceptions and write
    InternalSettingsList exceptions(m_ui.exceptions->exceptions());
    InternalSettingsList defaultExceptions(m_ui.defaultExceptions->exceptions());
    DecorationExceptionList(exceptions, defaultExceptions).writeConfig(m_configuration);

    // sync configuration for exceptions
    m_configuration->sync();

    setNeedsSave(false);
    Q_EMIT saved();

    if (!saveAsPresetName.isEmpty()) { // set the preset
        m_internalSettings->load();

        // delete the preset if one of that name already exists
        PresetsModel::deletePreset(m_presetsConfiguration.data(), saveAsPresetName);

        // write the new internalSettings value as a new preset
        PresetsModel::writePreset(m_internalSettings.data(), m_presetsConfiguration.data(), saveAsPresetName);
        // sync configuration for presets
        m_presetsConfiguration->sync();
    }

    DBusMessages::updateDecorationColorCache();
    // needed to tell kwin to reload when running from external kcmshell
    DBusMessages::kwinReloadConfig();

    // not needed as both of the other DBUS messages also update KStyle
    // DBusMessages::kstyleReloadDecorationConfig();

    // auto-generate the klassy and klassy-dark system icons
    generateSystemIcons();
}

//_________________________________________________________
void ConfigWidget::defaults()
{
    m_processingDefaults = true;
    // create internal settings and load from rc files
    m_internalSettings = InternalSettingsPtr(new InternalSettings());
    m_internalSettings->setDefaults();

    // assign to ui
    m_ui.buttonIconStyle->setCurrentIndex(m_internalSettings->buttonIconStyle());
    m_ui.buttonShape->setCurrentIndex(m_internalSettings->buttonShape());
    m_ui.iconSize->setCurrentIndex(m_internalSettings->iconSize());
    m_ui.systemIconSize->setCurrentIndex(m_internalSettings->systemIconSize());
    m_ui.cornerRadius->setValue(m_internalSettings->windowCornerRadius());
    m_ui.boldButtonIcons->setCurrentIndex(m_internalSettings->boldButtonIcons());
    m_ui.drawBorderOnMaximizedWindows->setChecked(m_internalSettings->drawBorderOnMaximizedWindows());
    m_ui.drawBackgroundGradient->setChecked(m_internalSettings->drawBackgroundGradient());
    m_ui.animationsEnabled->setChecked(m_internalSettings->animationsEnabled());
    m_ui.animationsSpeedRelativeSystem->setValue(m_internalSettings->animationsSpeedRelativeSystem());
    m_ui.drawTitleBarSeparator->setChecked(m_internalSettings->drawTitleBarSeparator());
    m_ui.useTitleBarColorForAllBorders->setChecked(m_internalSettings->useTitleBarColorForAllBorders());
    m_ui.roundBottomCornersWhenNoBorders->setChecked(m_internalSettings->roundBottomCornersWhenNoBorders());
    m_ui.forceColorizeSystemIcons->setChecked(m_internalSettings->forceColorizeSystemIcons());
    m_ui.colorizeThinWindowOutlineWithButton->setChecked(m_internalSettings->colorizeThinWindowOutlineWithButton());

    // set defaults in dialogs
    m_systemIconGenerationDialog->defaults();
    m_buttonSizingDialog->defaults();
    m_buttonColorsDialog->defaults();
    m_buttonBehaviourDialog->defaults();
    m_titleBarSpacingDialog->defaults();
    m_titleBarOpacityDialog->defaults();
    m_windowOutlineStyleDialog->defaults();
    m_shadowStyleDialog->defaults();

    updateWindowControlPreviewIcons();

    // load default exceptions and refresh (leave user-set exceptions alone)
    DecorationExceptionList exceptions;
    exceptions.readConfig(m_configuration, true);
    if (exceptions.numberDefaults()) {
        m_ui.defaultExceptions->setExceptions(exceptions.getDefault());
    } else {
        m_ui.defaultExceptions->hide();
    }

    updateIconsStackedWidgetVisible();

    setNeedsSave(!isDefaults());

    m_processingDefaults = false;
    m_defaultsPressed = true;
}

bool ConfigWidget::isDefaults()
{
    bool isDefaults = true;

    QString groupName(QStringLiteral("Windeco"));
    if (m_configuration->hasGroup(groupName)) {
        KConfigGroup group = m_configuration->group(groupName);
        if (group.keyList().count())
            return false;
    }

    if (!m_systemIconGenerationDialog->isDefaults()) {
        return false;
    }

    if (!m_buttonSizingDialog->isDefaults()) {
        return false;
    }

    if (!m_buttonColorsDialog->isDefaults()) {
        return false;
    }

    if (!m_buttonBehaviourDialog->isDefaults()) {
        return false;
    }

    if (!m_titleBarSpacingDialog->isDefaults()) {
        return false;
    }

    if (!m_titleBarOpacityDialog->isDefaults()) {
        return false;
    }

    if (!m_windowOutlineStyleDialog->isDefaults()) {
        return false;
    }

    if (!m_shadowStyleDialog->isDefaults()) {
        return false;
    }

    return isDefaults;
}

//_______________________________________________
void ConfigWidget::updateChanged()
{
    // check configuration
    if (!m_internalSettings) {
        return;
    }

    if (m_loading)
        return; // only check if the user has made a change to the UI, or user has pressed defaults

    // track modifications
    bool modified(false);

    if (m_ui.drawTitleBarSeparator->isChecked() != m_internalSettings->drawTitleBarSeparator())
        modified = true;
    else if (m_ui.useTitleBarColorForAllBorders->isChecked() != m_internalSettings->useTitleBarColorForAllBorders())
        modified = true;
    else if (m_ui.roundBottomCornersWhenNoBorders->isChecked() != m_internalSettings->roundBottomCornersWhenNoBorders())
        modified = true;
    else if (m_ui.forceColorizeSystemIcons->isChecked() != m_internalSettings->forceColorizeSystemIcons())
        modified = true;
    else if (m_ui.buttonIconStyle->currentIndex() != m_internalSettings->buttonIconStyle())
        modified = true;
    else if (m_ui.buttonShape->currentIndex() != m_internalSettings->buttonShape())
        modified = true;
    else if (m_ui.iconSize->currentIndex() != m_internalSettings->iconSize())
        modified = true;
    else if (m_ui.systemIconSize->currentIndex() != m_internalSettings->systemIconSize())
        modified = true;
    else if (m_ui.boldButtonIcons->currentIndex() != m_internalSettings->boldButtonIcons())
        modified = true;
    else if (m_ui.drawBorderOnMaximizedWindows->isChecked() != m_internalSettings->drawBorderOnMaximizedWindows())
        modified = true;
    else if (m_ui.drawBackgroundGradient->isChecked() != m_internalSettings->drawBackgroundGradient())
        modified = true;
    else if (qAbs(m_ui.cornerRadius->value() - m_internalSettings->windowCornerRadius()) > 0.001)
        modified = true;
    else if (m_ui.colorizeThinWindowOutlineWithButton->isChecked() != m_internalSettings->colorizeThinWindowOutlineWithButton())
        modified = true;

    // animations
    else if (m_ui.animationsEnabled->isChecked() != m_internalSettings->animationsEnabled())
        modified = true;
    else if (m_ui.animationsSpeedRelativeSystem->value() != m_internalSettings->animationsSpeedRelativeSystem())
        modified = true;

    // dialogs
    else if (m_systemIconGenerationDialog->m_changed)
        modified = true;
    else if (m_buttonSizingDialog->m_changed)
        modified = true;
    else if (m_buttonColorsDialog->m_changed)
        modified = true;
    else if (m_buttonBehaviourDialog->m_changed)
        modified = true;
    else if (m_titleBarSpacingDialog->m_changed)
        modified = true;
    else if (m_titleBarOpacityDialog->m_changed)
        modified = true;
    else if (m_windowOutlineStyleDialog->m_changed)
        modified = true;
    else if (m_shadowStyleDialog->m_changed)
        modified = true;

    // exceptions
    else if (m_ui.defaultExceptions->isChanged())
        modified = true;
    else if (m_ui.exceptions->isChanged())
        modified = true;

    setNeedsSave(modified);
}

void ConfigWidget::kPageWidgetChanged(KPageWidgetItem *current, KPageWidgetItem *before)
{
    Q_UNUSED(before)
    if (current) {
        current->setHeaderVisible(false);
    }
}

// only enable animationsSpeedRelativeSystem and animationsSpeedLabelx when animationsEnabled is checked
void ConfigWidget::setEnabledAnimationsSpeed()
{
    m_ui.animationsSpeedRelativeSystem->setEnabled(m_ui.animationsEnabled->isChecked());
    m_ui.animationsSpeedLabel1->setEnabled(m_ui.animationsEnabled->isChecked());
    m_ui.animationsSpeedLabel2->setEnabled(m_ui.animationsEnabled->isChecked());
    m_ui.animationsSpeedLabel4->setEnabled(m_ui.animationsEnabled->isChecked());
}

void ConfigWidget::updateIconsStackedWidgetVisible()
{
    if (m_ui.buttonIconStyle->currentIndex() == InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme) {
        m_ui.iconSizeStackedWidget->setCurrentIndex(1);
        m_ui.iconOptionsStackedWidget->setCurrentIndex(1);
    } else {
        m_ui.iconSizeStackedWidget->setCurrentIndex(0);
        m_ui.iconOptionsStackedWidget->setCurrentIndex(0);
    }
}

void ConfigWidget::dialogChanged(bool changed)
{
    setNeedsSave(changed);
}

void ConfigWidget::systemIconGenerationButtonClicked()
{
    m_systemIconGenerationDialog->show();
}

void ConfigWidget::buttonSizingButtonClicked()
{
    m_buttonSizingDialog->setVisibleUiElements();
    m_buttonSizingDialog->show();
}

void ConfigWidget::buttonColorsButtonClicked()
{
    m_buttonColorsDialog->setWindowTitle(i18n("Button Colours - Klassy Settings"));
    m_buttonColorsDialog->setWindowIcon(QIcon::fromTheme(QStringLiteral("color-management")));
    m_buttonColorsDialog->show();
}

void ConfigWidget::buttonBehaviourButtonClicked()
{
    m_buttonBehaviourDialog->setWindowTitle(i18n("Button Behaviour - Klassy Settings"));
    m_buttonBehaviourDialog->show();
}

void ConfigWidget::titleBarSpacingButtonClicked()
{
    m_titleBarSpacingDialog->setWindowTitle(i18n("Titlebar Spacing - Klassy Settings"));
    m_titleBarSpacingDialog->show();
}

void ConfigWidget::titleBarOpacityButtonClicked()
{
    m_titleBarOpacityDialog->setWindowTitle(i18n("Titlebar Opacity - Klassy Settings"));
    m_titleBarOpacityDialog->show();
}

void ConfigWidget::shadowStyleButtonClicked()
{
    m_shadowStyleDialog->setWindowTitle(i18n("Shadow Style - Klassy Settings"));
    m_shadowStyleDialog->show();
}

void ConfigWidget::windowOutlineStyleButtonClicked()
{
    m_windowOutlineStyleDialog->setWindowTitle(i18n("Window Outline Style - Klassy Settings"));
    m_windowOutlineStyleDialog->show();
}

void ConfigWidget::presetsButtonClicked()
{
    m_loadPresetDialog->setWindowTitle(i18n("Presets - Klassy Settings"));
    m_loadPresetDialog->initPresetsList();
    m_loadPresetDialog->show();
}

void ConfigWidget::generateSystemIcons()
{
    // auto-generate the klassy and klassy-dark system icons in a separate process
    system("klassy-settings -g &");
}

void ConfigWidget::updateIcons()
{
    QSize sizeSixteen(16, 16);

    QIcon icon;

    icon = QIcon(QStringLiteral(":/klassy_config_icons/full_height_rectangle.svg"));
    ColorTools::convertAlphaToColor(icon, sizeSixteen, QApplication::palette().windowText().color());
    m_ui.buttonShape->setItemIcon(0, icon);

    icon = QIcon(QStringLiteral(":/klassy_config_icons/full_height_rounded_rectangle.svg"));
    ColorTools::convertAlphaToColor(icon, sizeSixteen, QApplication::palette().windowText().color());
    m_ui.buttonShape->setItemIcon(1, icon);

    icon = QIcon(QStringLiteral(":/klassy_config_icons/integrated_rounded_rectangle.svg"));
    ColorTools::convertAlphaToColor(icon, sizeSixteen, QApplication::palette().windowText().color());
    m_ui.buttonShape->setItemIcon(2, icon);

    icon = QIcon(QStringLiteral(":/klassy_config_icons/integrated_rounded_rectangle_grouped.svg"));
    ColorTools::convertAlphaToColor(icon, sizeSixteen, QApplication::palette().windowText().color());
    m_ui.buttonShape->setItemIcon(3, icon);

    icon = QIcon(QStringLiteral(":/klassy_config_icons/circle.svg"));
    ColorTools::convertAlphaToColor(icon, sizeSixteen, QApplication::palette().windowText().color());
    m_ui.buttonShape->setItemIcon(4, icon);

    icon = QIcon(QStringLiteral(":/klassy_config_icons/square.svg"));
    ColorTools::convertAlphaToColor(icon, sizeSixteen, QApplication::palette().windowText().color());
    m_ui.buttonShape->setItemIcon(5, icon);

    icon = QIcon(QStringLiteral(":/klassy_config_icons/rounded_square.svg"));
    ColorTools::convertAlphaToColor(icon, sizeSixteen, QApplication::palette().windowText().color());
    m_ui.buttonShape->setItemIcon(6, icon);

    ColorTools::convertAlphaToColor(m_unlockedIcon, sizeSixteen, QApplication::palette().windowText().color());
    ColorTools::convertAlphaToColor(m_lockedIcon, sizeSixteen, QApplication::palette().windowText().color());
    m_lockIcon.addPixmap(m_unlockedIcon.pixmap(sizeSixteen), QIcon::Mode::Normal, QIcon::State::Off);
    m_lockIcon.addPixmap(m_lockedIcon.pixmap(sizeSixteen), QIcon::Mode::Normal, QIcon::State::On);

    m_buttonSizingDialog->updateLockIcons();
    m_buttonColorsDialog->updateLockIcons();
    m_buttonBehaviourDialog->updateLockIcons();
    m_titleBarSpacingDialog->updateLockIcons();
    m_windowOutlineStyleDialog->updateLockIcons();
}

void ConfigWidget::updateWindowControlPreviewIcons()
{
    QSize size(115, 72);
    m_ui.buttonIconStyle->setIconSize(size);

    for (int i = 0; i < InternalSettings::EnumButtonIconStyle::COUNT; i++) {
        if (i != static_cast<int>(InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme)) {
            generateWindowControlPreviewIcon(size, static_cast<InternalSettings::EnumButtonIconStyle::type>(i));
        } else {
            m_ui.buttonIconStyle->setItemIcon(static_cast<int>(InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme),
                                              QIcon::fromTheme(QStringLiteral("preferences-desktop-icons")));
        }
    }
}

void ConfigWidget::generateWindowControlPreviewIcon(QSize size, InternalSettings::EnumButtonIconStyle::type iconStyle)
{
    qreal dpr = widget()->devicePixelRatioF();
    QSize sizeScaled(qRound(size.width() * dpr), qRound(size.height() * dpr));
    QPixmap pixmap(sizeScaled);
    pixmap.setDevicePixelRatio(dpr);

    pixmap.fill(QColor("#eeeff0"));

    QRect windowRect(0, size.height() / 2, size.width(), size.height() / 2);

    std::unique_ptr<QPainter> painter = std::make_unique<QPainter>(&pixmap);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor("#a3a6a9"));
    painter->drawRect(windowRect);

    painter->setRenderHints(QPainter::Antialiasing);

    QSize iconSize(20, 20);
    int maximizedButtonTop = size.height() / 4 - iconSize.height() / 2;
    int floatingButtonTop = (size.height() * 3 / 4) - (iconSize.height() * 3 / 4);
    int iconSpacing = 14;

    bool boldIcons = (m_ui.boldButtonIcons->currentIndex() == InternalSettings::EnumBoldButtonIcons::BoldIconsBold
                      || (m_ui.boldButtonIcons->currentIndex() == InternalSettings::EnumBoldButtonIcons::BoldIconsHiDpiOnly && dpr >= 1.2));
    auto internalSettings = InternalSettingsPtr(new InternalSettings());
    internalSettings->setButtonIconStyle(iconStyle);

    auto [iconRenderer, localRenderingWidth](RenderDecorationButtonIcon::factory(internalSettings, painter.get(), false, boldIcons, dpr));

    QPen pen("#bcc1c5");
    pen.setWidthF(PenWidth::Symbol * dpr);
    pen.setCosmetic(true);
    painter->setPen(pen);

    int iconLeft = maximizedButtonTop;
    QPoint iconTopLeft(iconLeft, maximizedButtonTop);
    painter->save();
    painter->translate(iconTopLeft);
    iconRenderer->setDeviceOffsetFromZeroReference(painter->deviceTransform().map(iconTopLeft));
    painter->setViewport(0, 0, iconSize.width(), iconSize.height());
    painter->setWindow(0, 0, localRenderingWidth, localRenderingWidth);
    iconRenderer->renderIcon(DecorationButtonType::Minimize, false);
    painter->restore();

    iconTopLeft = QPoint(iconTopLeft.x() + iconSize.width() + iconSpacing, iconTopLeft.y());
    painter->save();
    painter->translate(iconTopLeft);
    iconRenderer->setDeviceOffsetFromZeroReference(painter->deviceTransform().map(iconTopLeft));
    painter->setViewport(0, 0, iconSize.width(), iconSize.height());
    painter->setWindow(0, 0, localRenderingWidth, localRenderingWidth);
    iconRenderer->renderIcon(DecorationButtonType::Maximize, true);
    painter->restore();

    iconTopLeft = QPoint(iconTopLeft.x() + iconSize.width() + iconSpacing, iconTopLeft.y());
    painter->save();
    painter->translate(iconTopLeft);
    iconRenderer->setDeviceOffsetFromZeroReference(painter->deviceTransform().map(iconTopLeft));
    painter->setViewport(0, 0, iconSize.width(), iconSize.height());
    painter->setWindow(0, 0, localRenderingWidth, localRenderingWidth);
    iconRenderer->renderIcon(DecorationButtonType::Close, false);
    painter->restore();

    pen = painter->pen();
    pen.setColor(QColor("#fcfcfc"));
    painter->setPen(pen);
    iconTopLeft = QPoint(maximizedButtonTop, floatingButtonTop);
    painter->save();
    painter->translate(iconTopLeft);
    iconRenderer->setDeviceOffsetFromZeroReference(painter->deviceTransform().map(iconTopLeft));
    painter->setViewport(0, 0, iconSize.width(), iconSize.height());
    painter->setWindow(0, 0, localRenderingWidth, localRenderingWidth);
    iconRenderer->renderIcon(DecorationButtonType::Minimize, false);
    painter->restore();

    iconTopLeft = QPoint(iconTopLeft.x() + iconSize.width() + iconSpacing, iconTopLeft.y());
    painter->save();
    painter->translate(iconTopLeft);
    iconRenderer->setDeviceOffsetFromZeroReference(painter->deviceTransform().map(iconTopLeft));
    painter->setViewport(0, 0, iconSize.width(), iconSize.height());
    painter->setWindow(0, 0, localRenderingWidth, localRenderingWidth);
    iconRenderer->renderIcon(DecorationButtonType::Maximize, false);
    painter->restore();

    iconTopLeft = QPoint(iconTopLeft.x() + iconSize.width() + iconSpacing, iconTopLeft.y());
    painter->save();
    painter->translate(iconTopLeft);
    iconRenderer->setDeviceOffsetFromZeroReference(painter->deviceTransform().map(iconTopLeft));
    painter->setViewport(0, 0, iconSize.width(), iconSize.height());
    painter->setWindow(0, 0, localRenderingWidth, localRenderingWidth);
    iconRenderer->renderIcon(DecorationButtonType::Close, false);
    painter->restore();

    QIcon icon(pixmap);

    m_ui.buttonIconStyle->setItemIcon(iconStyle, icon);
}

bool ConfigWidget::eventFilter(QObject *obj, QEvent *ev)
{
    if (ev->type() == QEvent::ApplicationPaletteChange) {
        // overwrite handling of palette change
        updateIcons();
        return QObject::eventFilter(obj, ev);
    } else if (ev->type() == QEvent::Show) {
        if (widget()->window() && widget()->window()->windowHandle()) {
            connect(widget()->window()->windowHandle(), &QWindow::screenChanged, this, &ConfigWidget::updateIcons, Qt::ConnectionType::UniqueConnection);
            connect(widget()->window()->windowHandle(),
                    &QWindow::screenChanged,
                    this,
                    &ConfigWidget::updateWindowControlPreviewIcons,
                    Qt::ConnectionType::UniqueConnection);
        }
        return QObject::eventFilter(obj, ev);
    }

    // Make sure the rest of events are handled
    return QObject::eventFilter(obj, ev);
}
}
