/*
 * SPDX-FileCopyrightText: 2023-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "buttoncolors.h"
#include "breezeconfigwidget.h"
#include "dbusmessages.h"
#include "presetsmodel.h"
#include "renderdecorationbuttonicon.h"
#include "systemicontheme.h"
#include <KColorCombo>
#include <KColorUtils>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMutableListIterator>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QWindow>
#include <memory>

namespace Breeze
{

ButtonColors::ButtonColors(KSharedConfig::Ptr config, KSharedConfig::Ptr presetsConfig, QObject *parent)
    : QDialog(static_cast<ConfigWidget *>(parent)->widget())
    , m_ui(new Ui_ButtonColors)
    , m_configuration(config)
    , m_presetsConfiguration(presetsConfig)
    , m_parent(parent)
{
    m_ui->setupUi(this);
    m_internalSettings = InternalSettingsPtr(new InternalSettings());
    getButtonsOrderFromKwinConfig();

    m_ui->activeOverrideGroupBox->setVisible(false);
    m_ui->inactiveOverrideGroupBox->setVisible(false);
    connect(m_ui->buttonColorOverrideToggleActive, &QAbstractButton::toggled, this, &ButtonColors::showActiveOverrideGroupBox);
    connect(m_ui->buttonColorOverrideToggleInactive, &QAbstractButton::toggled, this, &ButtonColors::showInactiveOverrideGroupBox);
    connect(m_ui->buttonColorOverrideToggleActive, &QAbstractButton::clicked, this, &ButtonColors::resizeDialog);
    connect(m_ui->buttonColorOverrideToggleInactive, &QAbstractButton::clicked, this, &ButtonColors::resizeDialog);

    generateTableCells(m_ui->overrideColorTableActive);
    generateTableCells(m_ui->overrideColorTableInactive);

    setSystemTitlebarColors();
    DecorationColors decorationColors(false);
    decorationColors.generateDecorationColors(QApplication::palette(),
                                              m_internalSettings,
                                              m_systemTitleBarTextActive,
                                              m_systemTitlebarBackgroundActive,
                                              m_systemTitleBarTextInactive,
                                              m_systemTitlebarBackgroundInactive);
    setOverrideComboBoxColorIcons(true, decorationColors);
    setOverrideComboBoxColorIcons(false, decorationColors);

    // populate the horizontal header
    QStringList orderedHorizontalHeaderLabels;
    for (auto i = 0; i < m_allCustomizableButtonsOrder.count(); i++) { // get the horizontal header labels in the correct user-set order
        orderedHorizontalHeaderLabels.append(m_colorOverridableButtonTypesStrings.value(m_allCustomizableButtonsOrder[i]));
    }
    m_ui->overrideColorTableActive->setHorizontalHeaderLabels(orderedHorizontalHeaderLabels);
    m_ui->overrideColorTableInactive->setHorizontalHeaderLabels(orderedHorizontalHeaderLabels);
    m_ui->overrideColorTableActive->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    m_ui->overrideColorTableInactive->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    m_ui->overrideColorTableActive->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    m_ui->overrideColorTableInactive->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
    loadHorizontalHeaderIcons();
    // set the header widths to the same as the largest section
    int largestSection = 0;
    for (int columnIndex = 0; columnIndex < m_colorOverridableButtonTypesStrings.count(); columnIndex++) {
        int sectionSize = m_ui->overrideColorTableActive->horizontalHeader()->sectionSize(columnIndex);
        if (sectionSize > largestSection)
            largestSection = sectionSize;
    }
    m_ui->overrideColorTableActive->horizontalHeader()->setMinimumSectionSize(largestSection);
    m_ui->overrideColorTableInactive->horizontalHeader()->setMinimumSectionSize(largestSection);

    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)), SLOT(loadButtonPaletteColorsIcons()));

    // track ui changes
    // updateChanged() slot set to DirectConnection so slot can detect the immediate m_loading status (not available in a queued connection)
    // lockButtonColorsActive and lockButtonColorsInactive are set to mirror each other in the UI file
    connect(m_ui->lockButtonColorsActive, &QAbstractButton::toggled, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui->lockButtonColorsInactive, &QAbstractButton::toggled, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);

    auto setIndexOfOtherIfLocked = [this](QComboBox *other, const int i) {
        if (m_loading || m_processingDefaults || !m_ui->lockButtonColorsActive->isChecked())
            return; // only do this for user interactions where the active/inactive lock is checked
        other->setCurrentIndex(i);
    };

    connect(m_ui->buttonIconColorsActive,
            SIGNAL(currentIndexChanged(int)),
            SLOT(refreshCloseButtonIconColorStateActive()),
            Qt::ConnectionType::DirectConnection); // important that refreshCloseButtonIconColorState is before updateChanged
    connect(m_ui->buttonIconColorsActive, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->buttonIconColorsActive,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [=, this](const int i) {
            setIndexOfOtherIfLocked(m_ui->buttonIconColorsInactive, i);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->buttonIconColorsInactive,
            SIGNAL(currentIndexChanged(int)),
            SLOT(refreshCloseButtonIconColorStateInactive()),
            Qt::ConnectionType::DirectConnection); // important that refreshCloseButtonIconColorState is before updateChanged
    connect(m_ui->buttonIconColorsInactive, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->buttonIconColorsInactive,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [=, this](const int i) {
            setIndexOfOtherIfLocked(m_ui->buttonIconColorsActive, i);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->buttonBackgroundColorsActive,
            SIGNAL(currentIndexChanged(int)),
            SLOT(setNegativeCloseBackgroundHoverPressStateActive()),
            Qt::ConnectionType::DirectConnection);
    connect(m_ui->buttonBackgroundColorsActive,
            SIGNAL(currentIndexChanged(int)),
            SLOT(refreshCloseButtonIconColorStateActive()),
            Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->buttonBackgroundColorsActive,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [=, this](const int i) {
            setIndexOfOtherIfLocked(m_ui->buttonBackgroundColorsInactive, i);
        },
        Qt::ConnectionType::DirectConnection);
    connect(m_ui->buttonBackgroundColorsActive, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);

    connect(m_ui->buttonBackgroundColorsInactive,
            SIGNAL(currentIndexChanged(int)),
            SLOT(setNegativeCloseBackgroundHoverPressStateInactive()),
            Qt::ConnectionType::DirectConnection);
    connect(m_ui->buttonBackgroundColorsInactive,
            SIGNAL(currentIndexChanged(int)),
            SLOT(refreshCloseButtonIconColorStateInactive()),
            Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->buttonBackgroundColorsInactive,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [=, this](const int i) {
            setIndexOfOtherIfLocked(m_ui->buttonBackgroundColorsActive, i);
        },
        Qt::ConnectionType::DirectConnection);
    connect(m_ui->buttonBackgroundColorsInactive, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);

    connect(m_ui->closeButtonIconColorActive, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->closeButtonIconColorActive,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [=, this](const int i) {
            setIndexOfOtherIfLocked(m_ui->closeButtonIconColorInactive, i);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->closeButtonIconColorInactive, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->closeButtonIconColorInactive,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [=, this](const int i) {
            setIndexOfOtherIfLocked(m_ui->closeButtonIconColorActive, i);
        },
        Qt::ConnectionType::DirectConnection);

    auto setOtherCheckedIfLocked = [this](QAbstractButton *other, const bool v) {
        if (m_loading || m_processingDefaults || !m_ui->lockButtonColorsActive->isChecked())
            return; // only do this for user interactions where the active/inactive lock is checked
        other->setChecked(v);
    };

    connect(m_ui->negativeCloseBackgroundHoverPressActive, &QAbstractButton::toggled, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->negativeCloseBackgroundHoverPressActive,
        &QAbstractButton::toggled,
        this,
        [=, this](const bool v) {
            setOtherCheckedIfLocked(m_ui->negativeCloseBackgroundHoverPressInactive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->negativeCloseBackgroundHoverPressInactive,
            &QAbstractButton::toggled,
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->negativeCloseBackgroundHoverPressInactive,
        &QAbstractButton::toggled,
        this,
        [=, this](const bool v) {
            setOtherCheckedIfLocked(m_ui->negativeCloseBackgroundHoverPressActive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->useHoverAccentActive, &QAbstractButton::toggled, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->useHoverAccentActive,
        &QAbstractButton::toggled,
        this,
        [=, this](const bool v) {
            setOtherCheckedIfLocked(m_ui->useHoverAccentInactive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->useHoverAccentInactive, &QAbstractButton::toggled, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->useHoverAccentInactive,
        &QAbstractButton::toggled,
        this,
        [=, this](const bool v) {
            setOtherCheckedIfLocked(m_ui->useHoverAccentActive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->onPoorIconContrastActive,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(m_ui->onPoorIconContrastActive, qOverload<int>(&QComboBox::currentIndexChanged), this, &ButtonColors::setPoorIconContrastThresholdVisibleActive);
    connect(
        m_ui->onPoorIconContrastActive,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [=, this](const int i) {
            setIndexOfOtherIfLocked(m_ui->onPoorIconContrastInactive, i);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->onPoorIconContrastInactive,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(m_ui->onPoorIconContrastInactive,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &ButtonColors::setPoorIconContrastThresholdVisibleInactive);
    connect(
        m_ui->onPoorIconContrastInactive,
        qOverload<int>(&QComboBox::currentIndexChanged),
        this,
        [=, this](const int i) {
            setIndexOfOtherIfLocked(m_ui->onPoorIconContrastActive, i);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->adjustBackgroundColorOnPoorContrastActive,
            &QAbstractButton::toggled,
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(m_ui->adjustBackgroundColorOnPoorContrastActive, &QAbstractButton::toggled, this, &ButtonColors::setPoorBackgroundContrastThresholdVisibleActive);
    connect(
        m_ui->adjustBackgroundColorOnPoorContrastActive,
        &QAbstractButton::toggled,
        this,
        [=, this](const bool v) {
            setOtherCheckedIfLocked(m_ui->adjustBackgroundColorOnPoorContrastInactive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->adjustBackgroundColorOnPoorContrastInactive,
            &QAbstractButton::toggled,
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(m_ui->adjustBackgroundColorOnPoorContrastInactive,
            &QAbstractButton::toggled,
            this,
            &ButtonColors::setPoorBackgroundContrastThresholdVisibleInactive);
    connect(
        m_ui->adjustBackgroundColorOnPoorContrastInactive,
        &QAbstractButton::toggled,
        this,
        [=, this](const bool v) {
            setOtherCheckedIfLocked(m_ui->adjustBackgroundColorOnPoorContrastActive, v);
        },
        Qt::ConnectionType::DirectConnection);

    auto setOtherValueIfLocked = [this](QSpinBox *other, const int v) {
        if (m_loading || m_processingDefaults || !m_ui->lockButtonColorsActive->isChecked())
            return; // only do this for user interactions where the active/inactive lock is checked
        other->setValue(v);
    };

    auto setOtherDoubleValueIfLocked = [this](QDoubleSpinBox *other, const double v) {
        if (m_loading || m_processingDefaults || !m_ui->lockButtonColorsActive->isChecked())
            return; // only do this for user interactions where the active/inactive lock is checked
        other->setValue(v);
    };

    connect(m_ui->buttonIconOpacityActive, SIGNAL(valueChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->buttonIconOpacityActive,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        [=, this](const int v) {
            setOtherValueIfLocked(m_ui->buttonIconOpacityInactive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->buttonIconOpacityInactive, SIGNAL(valueChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->buttonIconOpacityInactive,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        [=, this](const int v) {
            setOtherValueIfLocked(m_ui->buttonIconOpacityActive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->buttonBackgroundOpacityActive, SIGNAL(valueChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->buttonBackgroundOpacityActive,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        [=, this](const int v) {
            setOtherValueIfLocked(m_ui->buttonBackgroundOpacityInactive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->buttonBackgroundOpacityInactive, SIGNAL(valueChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->buttonBackgroundOpacityInactive,
        qOverload<int>(&QSpinBox::valueChanged),
        this,
        [=, this](const int v) {
            setOtherValueIfLocked(m_ui->buttonBackgroundOpacityActive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->poorIconContrastThresholdActive,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->poorIconContrastThresholdActive,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        [=, this](const double v) {
            setOtherDoubleValueIfLocked(m_ui->poorIconContrastThresholdInactive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->poorIconContrastThresholdInactive,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->poorIconContrastThresholdInactive,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        [=, this](const double v) {
            setOtherDoubleValueIfLocked(m_ui->poorIconContrastThresholdActive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->poorBackgroundContrastThresholdActive,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->poorBackgroundContrastThresholdActive,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        [=, this](const double v) {
            setOtherDoubleValueIfLocked(m_ui->poorBackgroundContrastThresholdInactive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->poorBackgroundContrastThresholdInactive,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &ButtonColors::updateChanged,
            Qt::ConnectionType::DirectConnection);
    connect(
        m_ui->poorBackgroundContrastThresholdInactive,
        qOverload<double>(&QDoubleSpinBox::valueChanged),
        this,
        [=, this](const double v) {
            setOtherDoubleValueIfLocked(m_ui->poorBackgroundContrastThresholdActive, v);
        },
        Qt::ConnectionType::DirectConnection);

    connect(m_ui->buttonColorOverrideToggleActive, &QAbstractButton::toggled, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);
    connect(m_ui->buttonColorOverrideToggleInactive, &QAbstractButton::toggled, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);

    connect(m_ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, this, &ButtonColors::defaults);
    connect(m_ui->buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this, &ButtonColors::load);
    connect(m_ui->buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &ButtonColors::saveAndReloadKWinConfig);
    setApplyButtonState(false);
}

void ButtonColors::generateTableCells(QTableWidget *table)
{
    int numRows = m_overridableButtonColorStatesStrings.count();

    // generate the vertical header
    for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
        QTableWidgetItem *verticalHeaderItem = new QTableWidgetItem();
        verticalHeaderItem->setText(m_overridableButtonColorStatesStrings[rowIndex]);
        verticalHeaderItem->setToolTip(i18n("Lock to make all colours in this row the same"));
        verticalHeaderItem->setData(Qt::InitialSortOrderRole, rowIndex);
        table->insertRow(rowIndex);
        table->setVerticalHeaderItem(rowIndex, verticalHeaderItem);
    }

    connect(table->verticalHeader(), &QHeaderView::sectionClicked, this, &ButtonColors::tableVerticalHeaderSectionClicked);
    connect(table->verticalHeader(), &QHeaderView::sectionClicked, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);

    QString activeString = (table == m_ui->overrideColorTableActive) ? QStringLiteral("Active") : QStringLiteral("Inactive");

    // gnerate the overrideColorTable UI
    // populate the checkboxes and KColorButtons
    for (int columnIndex = 0; columnIndex < m_allCustomizableButtonsOrder.count(); columnIndex++) {
        table->insertColumn(columnIndex);
        QString columnName = m_colorOverridableButtonTypesStrings.value(m_allCustomizableButtonsOrder.value(columnIndex));
        for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
            QString rowName = m_overridableButtonColorStatesStrings.value(rowIndex);
            QVBoxLayout *vlayout = new QVBoxLayout();
            QHBoxLayout *hlayout0 = new QHBoxLayout();
            QHBoxLayout *hlayout1 = new QHBoxLayout();
            vlayout->addLayout(hlayout0);
            vlayout->addLayout(hlayout1);
            hlayout0->addStretch();
            QCheckBox *checkBox = new QCheckBox();
            checkBox->setObjectName(QStringLiteral("checkBox") + activeString + QString::number(columnIndex) + QString::number(rowIndex));
            checkBox->setProperty("column", columnIndex);
            checkBox->setProperty("row", rowIndex);
            QComboBox *comboBox = new QComboBox();
            comboBox->addItems(m_overrideComboBoxItems);
            comboBox->setObjectName(QStringLiteral("comboBox") + activeString + QString::number(columnIndex) + QString::number(rowIndex));
            comboBox->setProperty("column", columnIndex);
            comboBox->setProperty("row", rowIndex);
            comboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            hlayout0->addWidget(checkBox);
            hlayout0->addWidget(comboBox);
            hlayout0->addStretch();
            KColorButton *colorButton = new KColorButton();
            colorButton->setObjectName(QStringLiteral("colorButton") + activeString + QString::number(columnIndex) + QString::number(rowIndex));
            colorButton->setProperty("column", columnIndex);
            colorButton->setProperty("row", rowIndex);
            QSpinBox *spinBox = new QSpinBox();
            spinBox->setObjectName(QStringLiteral("spinBox") + activeString + QString::number(columnIndex) + QString::number(rowIndex));
            spinBox->setProperty("column", columnIndex);
            spinBox->setProperty("row", rowIndex);
            spinBox->setMaximum(100);
            spinBox->setMinimum(0);
            spinBox->setSuffix(i18n("%"));
            spinBox->setSingleStep(1);
            QLabel *label = new QLabel(i18n("Opacity:"));
            label->setBuddy(spinBox);
            hlayout1->addStretch();
            hlayout1->addWidget(colorButton);
            hlayout1->addStretch();
            hlayout1->addWidget(label);
            hlayout1->addWidget(spinBox);
            QWidget *w = new QWidget();

            comboBox->setVisible(false);
            colorButton->setVisible(false);
            spinBox->setVisible(false);
            label->setVisible(false);
            w->setLayout(vlayout);
            table->setCellWidget(rowIndex, columnIndex, w);
            w->setToolTip(columnName + i18n(", ") + rowName);

            connect(
                checkBox,
                &QAbstractButton::toggled,
                this,
                [=](const bool checked) {
                    comboBox->setVisible(checked);
                    label->setVisible(checked);
                    spinBox->setVisible(checked);
                    if (checked && comboBox->currentIndex() == 0) {
                        colorButton->setVisible(true);
                    } else {
                        colorButton->setVisible(false);
                    }

                    w->adjustSize();
                    table->resizeRowToContents(rowIndex);
                    table->resizeColumnToContents(columnIndex);
                },
                Qt::ConnectionType::DirectConnection);
            connect(
                comboBox,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this,
                [=](const int i) {
                    if (i == 0 && checkBox->isChecked()) {
                        colorButton->setVisible(true);
                    } else {
                        colorButton->setVisible(false);
                    }
                },
                Qt::ConnectionType::DirectConnection);
            connect(checkBox, &QAbstractButton::toggled, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);
            connect(checkBox, &QAbstractButton::toggled, this, &ButtonColors::resizeOverrideColorTable);
            // direct connections are used so the slot can detect the immediate m_loading status (not available in a queued connection)
            connect(checkBox, &QAbstractButton::toggled, this, &ButtonColors::copyCellCheckedStatusToOtherTable, Qt::ConnectionType::DirectConnection);
            connect(comboBox, SIGNAL(currentIndexChanged(int)), SLOT(updateChanged()), Qt::ConnectionType::DirectConnection);
            connect(colorButton, &KColorButton::changed, this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);
            connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), this, &ButtonColors::updateChanged, Qt::ConnectionType::DirectConnection);
            connect(comboBox,
                    qOverload<int>(&QComboBox::currentIndexChanged),
                    this,
                    &ButtonColors::copyCellColorDataToOtherCells,
                    Qt::ConnectionType::DirectConnection);
            connect(colorButton, &KColorButton::changed, this, &ButtonColors::copyCellColorDataToOtherCells, Qt::ConnectionType::DirectConnection);
            connect(spinBox, qOverload<int>(&QSpinBox::valueChanged), this, &ButtonColors::copyCellColorDataToOtherCells, Qt::ConnectionType::DirectConnection);
        }
    }
}

ButtonColors::~ButtonColors()
{
    delete m_ui;
}

void ButtonColors::loadMain(const bool assignUiValuesOnly)
{
    if (!assignUiValuesOnly) {
        m_loading = true;

        // load from rc files
        m_internalSettings->load();
    }

    m_ui->buttonIconColorsActive->setCurrentIndex(m_internalSettings->buttonIconColors(true));
    m_ui->buttonIconColorsInactive->setCurrentIndex(m_internalSettings->buttonIconColors(false));
    m_ui->buttonBackgroundColorsActive->setCurrentIndex(m_internalSettings->buttonBackgroundColors(true));
    m_ui->buttonBackgroundColorsInactive->setCurrentIndex(m_internalSettings->buttonBackgroundColors(false));
    m_ui->negativeCloseBackgroundHoverPressActive->setChecked(m_internalSettings->negativeCloseBackgroundHoverPress(true));
    m_ui->negativeCloseBackgroundHoverPressInactive->setChecked(m_internalSettings->negativeCloseBackgroundHoverPress(false));
    m_ui->useHoverAccentActive->setChecked(m_internalSettings->useHoverAccent(true));
    m_ui->useHoverAccentInactive->setChecked(m_internalSettings->useHoverAccent(false));
    m_ui->lockButtonColorsActive->setChecked(m_internalSettings->lockButtonColorsActiveInactive());
    m_ui->lockButtonColorsInactive->setChecked(m_internalSettings->lockButtonColorsActiveInactive());
    m_ui->onPoorIconContrastActive->setCurrentIndex(m_internalSettings->onPoorIconContrast(true));
    m_ui->onPoorIconContrastInactive->setCurrentIndex(m_internalSettings->onPoorIconContrast(false));
    m_ui->poorIconContrastThresholdActive->setValue(m_internalSettings->poorIconContrastThreshold(true));
    m_ui->poorIconContrastThresholdInactive->setValue(m_internalSettings->poorIconContrastThreshold(false));
    m_ui->adjustBackgroundColorOnPoorContrastActive->setChecked(m_internalSettings->adjustBackgroundColorOnPoorContrast(true));
    m_ui->adjustBackgroundColorOnPoorContrastInactive->setChecked(m_internalSettings->adjustBackgroundColorOnPoorContrast(false));
    m_ui->poorBackgroundContrastThresholdActive->setValue(m_internalSettings->poorBackgroundContrastThreshold(true));
    m_ui->poorBackgroundContrastThresholdInactive->setValue(m_internalSettings->poorBackgroundContrastThreshold(false));
    m_ui->buttonIconOpacityActive->setValue(m_internalSettings->buttonIconOpacity(true));
    m_ui->buttonIconOpacityInactive->setValue(m_internalSettings->buttonIconOpacity(false));
    m_ui->buttonBackgroundOpacityActive->setValue(m_internalSettings->buttonBackgroundOpacity(true));
    m_ui->buttonBackgroundOpacityInactive->setValue(m_internalSettings->buttonBackgroundOpacity(false));

    setNegativeCloseBackgroundHoverPressStateActive();
    setNegativeCloseBackgroundHoverPressStateInactive();
    refreshCloseButtonIconColorStateActive();
    refreshCloseButtonIconColorStateInactive();
    loadCloseButtonIconColor(true); // refreshCloseButtonIconColorState must occur before loading closeButtonIconColor
    loadCloseButtonIconColor(false);
    setPoorIconContrastThresholdVisibleActive();
    setPoorIconContrastThresholdVisibleInactive();
    setPoorBackgroundContrastThresholdVisibleActive();
    setPoorBackgroundContrastThresholdVisibleInactive();

    decodeAndLoadColorOverrideLockStates(true);
    decodeAndLoadColorOverrideLockStates(false);
    loadHorizontalHeaderIcons();

    // load the override table cell data
    m_overrideColorsLoaded = {false, false};
    m_overrideColorsLoaded.active = false;
    m_overrideColorsLoaded.inactive = false;
    for (int i = 0; i < InternalSettings::EnumButtonOverrideColorsActiveButtonType::COUNT; i++) {
        if (decodeOverrideColorsAndLoadTableColumn(m_internalSettings->buttonOverrideColorsActive(i).toUtf8(),
                                                   m_allCustomizableButtonsOrder.indexOf(static_cast<DecorationButtonType>(i)),
                                                   true)) {
            m_overrideColorsLoaded.active = true;
        }
        if (decodeOverrideColorsAndLoadTableColumn(m_internalSettings->buttonOverrideColorsInactive(i).toUtf8(),
                                                   m_allCustomizableButtonsOrder.indexOf(static_cast<DecorationButtonType>(i)),
                                                   false)) {
            m_overrideColorsLoaded.inactive = true;
        }
    }

    m_ui->buttonColorOverrideToggleActive->setChecked(m_overrideColorsLoaded.active);
    m_ui->buttonColorOverrideToggleInactive->setChecked(m_overrideColorsLoaded.inactive);

    // add palette preview icons
    loadButtonPaletteColorsIcons();

    if (!assignUiValuesOnly) {
        setChanged(false);

        m_loading = false;
        m_loaded = true;
    }
}

void ButtonColors::save(const bool reloadKwinConfig)
{
    // create internal settings and load from rc files
    m_internalSettings = InternalSettingsPtr(new InternalSettings());
    m_internalSettings->load();

    // apply modifications from ui
    m_internalSettings->setButtonIconColors(true, m_ui->buttonIconColorsActive->currentIndex());
    m_internalSettings->setButtonIconColors(false, m_ui->buttonIconColorsInactive->currentIndex());
    m_internalSettings->setCloseButtonIconColor(true, convertCloseButtonIconColorUiToSettingsIndex(true, m_ui->closeButtonIconColorActive->currentIndex()));
    m_internalSettings->setCloseButtonIconColor(false, convertCloseButtonIconColorUiToSettingsIndex(false, m_ui->closeButtonIconColorInactive->currentIndex()));
    m_internalSettings->setButtonBackgroundColors(true, m_ui->buttonBackgroundColorsActive->currentIndex());
    m_internalSettings->setButtonBackgroundColors(false, m_ui->buttonBackgroundColorsInactive->currentIndex());
    m_internalSettings->setNegativeCloseBackgroundHoverPress(true, m_ui->negativeCloseBackgroundHoverPressActive->isChecked());
    m_internalSettings->setNegativeCloseBackgroundHoverPress(false, m_ui->negativeCloseBackgroundHoverPressInactive->isChecked());
    m_internalSettings->setUseHoverAccent(true, m_ui->useHoverAccentActive->isChecked());
    m_internalSettings->setUseHoverAccent(false, m_ui->useHoverAccentInactive->isChecked());
    m_internalSettings->setLockButtonColorsActiveInactive(m_ui->lockButtonColorsActive->isChecked());
    m_internalSettings->setOnPoorIconContrast(true, m_ui->onPoorIconContrastActive->currentIndex());
    m_internalSettings->setOnPoorIconContrast(false, m_ui->onPoorIconContrastInactive->currentIndex());
    m_internalSettings->setPoorIconContrastThreshold(true, m_ui->poorIconContrastThresholdActive->value());
    m_internalSettings->setPoorIconContrastThreshold(false, m_ui->poorIconContrastThresholdInactive->value());
    m_internalSettings->setAdjustBackgroundColorOnPoorContrast(true, m_ui->adjustBackgroundColorOnPoorContrastActive->isChecked());
    m_internalSettings->setAdjustBackgroundColorOnPoorContrast(false, m_ui->adjustBackgroundColorOnPoorContrastInactive->isChecked());
    m_internalSettings->setPoorBackgroundContrastThreshold(true, m_ui->poorBackgroundContrastThresholdActive->value());
    m_internalSettings->setPoorBackgroundContrastThreshold(false, m_ui->poorBackgroundContrastThresholdInactive->value());
    m_internalSettings->setButtonIconOpacity(true, m_ui->buttonIconOpacityActive->value());
    m_internalSettings->setButtonIconOpacity(false, m_ui->buttonIconOpacityInactive->value());
    m_internalSettings->setButtonBackgroundOpacity(true, m_ui->buttonBackgroundOpacityActive->value());
    m_internalSettings->setButtonBackgroundOpacity(false, m_ui->buttonBackgroundOpacityInactive->value());

    m_internalSettings->setButtonOverrideColorsLockStates(true, encodeColorOverrideLockStates(true));
    m_internalSettings->setButtonOverrideColorsLockStates(false, encodeColorOverrideLockStates(false));

    // encodeColorOverrideTableColumns
    bool resetActive = !m_ui->buttonColorOverrideToggleActive->isChecked();
    bool resetInactive = !m_ui->buttonColorOverrideToggleInactive->isChecked();
    for (int i = 0; i < InternalSettings::EnumButtonOverrideColorsActiveButtonType::COUNT; i++) {
        m_internalSettings->setButtonOverrideColorsActive(
            i,
            encodeColorOverrideTableColumn(m_allCustomizableButtonsOrder.indexOf(static_cast<DecorationButtonType>(i)), true, resetActive));
        m_internalSettings->setButtonOverrideColorsInactive(
            i,
            encodeColorOverrideTableColumn(m_allCustomizableButtonsOrder.indexOf(static_cast<DecorationButtonType>(i)), false, resetInactive));
    }

    m_internalSettings->save();

    load(); // need to re-load in the case where m_ui->buttonColorActiveOverrideToggle is unchecked

    setChanged(false);

    if (reloadKwinConfig) {
        DBusMessages::updateDecorationColorCache();
        DBusMessages::kwinReloadConfig();
        // DBusMessages::kstyleReloadDecorationConfig(); //should reload anyway
        // auto-generate the klassy and klassy-dark system icons

        static_cast<ConfigWidget *>(m_parent)->generateSystemIcons();
    }
}

void ButtonColors::defaults()
{
    m_processingDefaults = true;

    // create internal settings and load from rc files
    m_internalSettings = InternalSettingsPtr(new InternalSettings());
    m_internalSettings->setDefaults();

    // assign to ui
    loadMain(true);
    setChanged(!isDefaults());

    m_processingDefaults = false;
    m_defaultsPressed = true;
}

bool ButtonColors::isDefaults()
{
    bool isDefaults = true;

    QString groupName(QStringLiteral("ButtonColors"));
    if (m_configuration->hasGroup(groupName)) {
        KConfigGroup group = m_configuration->group(groupName);
        if (group.keyList().count())
            isDefaults = false;
    }

    return isDefaults;
}

void ButtonColors::setChanged(bool value)
{
    m_changed = value;
    setApplyButtonState(value);
    Q_EMIT changed(value);
}

void ButtonColors::accept()
{
    save();
    QDialog::accept();
}

void ButtonColors::reject()
{
    load();
    QDialog::reject();
}

void ButtonColors::updateChanged()
{
    // update the displayed colours any time the UI settings change
    loadButtonPaletteColorsIcons();

    // check configuration
    if (!m_internalSettings)
        return;

    if (m_loading)
        return; // only check if the user has made a change to the UI, or user has pressed defaults

    // track modifications
    bool modified(false);
    if (m_ui->buttonIconColorsActive->currentIndex() != m_internalSettings->buttonIconColors(true))
        modified = true;
    else if (m_ui->buttonIconColorsInactive->currentIndex() != m_internalSettings->buttonIconColors(false))
        modified = true;
    else if (convertCloseButtonIconColorUiToSettingsIndex(true, m_ui->closeButtonIconColorActive->currentIndex())
             != m_internalSettings->closeButtonIconColor(true))
        modified = true;
    else if (convertCloseButtonIconColorUiToSettingsIndex(false, m_ui->closeButtonIconColorInactive->currentIndex())
             != m_internalSettings->closeButtonIconColor(false))
        modified = true;
    else if (m_ui->buttonBackgroundColorsActive->currentIndex() != m_internalSettings->buttonBackgroundColors(true))
        modified = true;
    else if (m_ui->buttonBackgroundColorsInactive->currentIndex() != m_internalSettings->buttonBackgroundColors(false))
        modified = true;
    else if (m_ui->negativeCloseBackgroundHoverPressActive->isChecked() != m_internalSettings->negativeCloseBackgroundHoverPress(true))
        modified = true;
    else if (m_ui->negativeCloseBackgroundHoverPressInactive->isChecked() != m_internalSettings->negativeCloseBackgroundHoverPress(false))
        modified = true;
    else if (m_ui->useHoverAccentActive->isChecked() != m_internalSettings->useHoverAccent(true))
        modified = true;
    else if (m_ui->useHoverAccentInactive->isChecked() != m_internalSettings->useHoverAccent(false))
        modified = true;
    else if (m_ui->lockButtonColorsActive->isChecked() != m_internalSettings->lockButtonColorsActiveInactive())
        modified = true;
    else if (m_ui->onPoorIconContrastActive->currentIndex() != m_internalSettings->onPoorIconContrast(true))
        modified = true;
    else if (m_ui->onPoorIconContrastInactive->currentIndex() != m_internalSettings->onPoorIconContrast(false))
        modified = true;
    else if (m_ui->poorIconContrastThresholdActive->value() != m_internalSettings->poorIconContrastThreshold(true))
        modified = true;
    else if (m_ui->poorIconContrastThresholdInactive->value() != m_internalSettings->poorIconContrastThreshold(false))
        modified = true;
    else if (m_ui->adjustBackgroundColorOnPoorContrastActive->isChecked() != m_internalSettings->adjustBackgroundColorOnPoorContrast(true))
        modified = true;
    else if (m_ui->adjustBackgroundColorOnPoorContrastInactive->isChecked() != m_internalSettings->adjustBackgroundColorOnPoorContrast(false))
        modified = true;
    else if (m_ui->poorBackgroundContrastThresholdActive->value() != m_internalSettings->poorBackgroundContrastThreshold(true))
        modified = true;
    else if (m_ui->poorBackgroundContrastThresholdInactive->value() != m_internalSettings->poorBackgroundContrastThreshold(false))
        modified = true;
    else if (m_ui->buttonIconOpacityActive->value() != m_internalSettings->buttonIconOpacity(true))
        modified = true;
    else if (m_ui->buttonIconOpacityInactive->value() != m_internalSettings->buttonIconOpacity(false))
        modified = true;
    else if (m_ui->buttonBackgroundOpacityActive->value() != m_internalSettings->buttonBackgroundOpacity(true))
        modified = true;
    else if (m_ui->buttonBackgroundOpacityInactive->value() != m_internalSettings->buttonBackgroundOpacity(false))
        modified = true;
    else if (encodeColorOverrideLockStates(true) != m_internalSettings->buttonOverrideColorsLockStates(true))
        modified = true;
    else if (encodeColorOverrideLockStates(false) != m_internalSettings->buttonOverrideColorsLockStates(false))
        modified = true;
    else if (m_overrideColorsLoaded.active && !m_ui->buttonColorOverrideToggleActive->isChecked())
        modified = true;
    else if (m_overrideColorsLoaded.inactive && !m_ui->buttonColorOverrideToggleInactive->isChecked())
        modified = true;

    if (!modified) {
        bool resetActive = !m_ui->buttonColorOverrideToggleActive->isChecked();
        for (int i = 0; i < InternalSettings::EnumButtonOverrideColorsActiveButtonType::COUNT; i++) {
            if (encodeColorOverrideTableColumn(m_allCustomizableButtonsOrder.indexOf(static_cast<DecorationButtonType>(i)), true, resetActive)
                != m_internalSettings->buttonOverrideColorsActive(i).toUtf8()) {
                modified = true;
                break;
            }
        }
    }
    if (!modified) {
        bool resetInactive = !m_ui->buttonColorOverrideToggleInactive->isChecked();
        for (int i = 0; i < InternalSettings::EnumButtonOverrideColorsActiveButtonType::COUNT; i++) {
            if (encodeColorOverrideTableColumn(m_allCustomizableButtonsOrder.indexOf(static_cast<DecorationButtonType>(i)), false, resetInactive)
                != m_internalSettings->buttonOverrideColorsInactive(i).toUtf8()) {
                modified = true;
                break;
            }
        }
    }

    setChanged(modified);
}

void ButtonColors::setApplyButtonState(const bool on)
{
    m_ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(on);
}

void ButtonColors::setSystemTitlebarColors()
{
    // get the titlebar text colour
    KSharedConfig::Ptr config = KSharedConfig::openConfig();

    DecorationColors::readSystemTitleBarColors(config,
                                               m_systemTitlebarBackgroundActive,
                                               m_systemTitlebarBackgroundInactive,
                                               m_systemTitleBarTextActive,
                                               m_systemTitleBarTextInactive);
}

void ButtonColors::setOverrideComboBoxColorIcons(const bool active, DecorationColors &decorationColors)
{
    QList<QColor> overrideComboBoxColors;

    for (int i = 0; i < overrideColorItems.count(); i++) {
        overrideComboBoxColors << DecorationButtonPalette::overrideColorItemsIndexToColor(decorationColors.active(), decorationColors.inactive(), i, active);
    }

    QList<QIcon> overrideComboBoxIcons;

    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    for (const QColor &color : overrideComboBoxColors) {
        pixmap.fill(color);
        overrideComboBoxIcons.append(QIcon(pixmap));
    }

    QTableWidget *table = active ? m_ui->overrideColorTableActive : m_ui->overrideColorTableInactive;
    QString activeString = active ? QStringLiteral("Active") : QStringLiteral("Inactive");
    for (int column = 0; column < m_allCustomizableButtonsOrder.count(); column++) {
        for (int row = 0; row < m_overridableButtonColorStatesStrings.count(); row++) {
            QWidget *w = table->cellWidget(row, column);
            if (!w)
                continue;
            QComboBox *comboBox = w->findChild<QComboBox *>(QStringLiteral("comboBox") + activeString + QString::number(column) + QString::number(row));
            if (!comboBox)
                continue;

            for (int i = 0; i < overrideComboBoxIcons.count(); i++) {
                comboBox->setItemIcon(i, overrideComboBoxIcons.at(i));
            }
        }
    }
}

void ButtonColors::refreshCloseButtonIconColorState(bool active)
{
    // As selected
    // Negative when hovered/pressed
    // White
    // White when hovered/pressed
    /*
        enum struct CloseButtonIconColorState{
            AsSelected = 1,
            NegativeWhenHoveredPressed = 2,
            White = 4,
            WhiteWhenHoveredPressed = 8
        }

    */

    QComboBox *buttonBackgroundColors = active ? m_ui->buttonBackgroundColorsActive : m_ui->buttonBackgroundColorsInactive;
    QComboBox *buttonIconColors = active ? m_ui->buttonIconColorsActive : m_ui->buttonIconColorsInactive;
    QComboBox *closeButtonIconColor = active ? m_ui->closeButtonIconColorActive : m_ui->closeButtonIconColorInactive;
    QLabel *closeButtonIconColorLabel = active ? m_ui->closeButtonIconColorLabelActive : m_ui->closeButtonIconColorLabelInactive;

    uint32_t closeButtonIconColorState = static_cast<uint32_t>(CloseButtonIconColorState::AsSelected);
    bool visible = false;
    QString negativeWhenHoveredPressedString;
    QString whiteString;
    QString WhiteWhenHoveredPressedString;

    bool negativeCloseBackground = buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose
        || buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose
        || buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights;

    if (buttonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentTrafficLights) {
        closeButtonIconColorLabel->setText(i18n("Traffic lights icon colours:"));
    } else {
        closeButtonIconColorLabel->setText(i18n("Close icon colour:"));
    }

    if (negativeCloseBackground) {
        visible = true;
        closeButtonIconColorState = closeButtonIconColorState | static_cast<uint32_t>(CloseButtonIconColorState::White)
            | static_cast<uint32_t>(CloseButtonIconColorState::WhiteWhenHoveredPressed);
        if (buttonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentTrafficLights) {
            whiteString = i18n("White close");
            WhiteWhenHoveredPressedString = i18n("White close when hovered/pressed");
        } else {
            whiteString = i18n("White");
            WhiteWhenHoveredPressedString = i18n("White when hovered/pressed");
        }
    }

    if (buttonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentNegativeClose
        || buttonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::TitleBarTextNegativeClose
        || buttonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentTrafficLights) {
        visible = true;
        closeButtonIconColorState = closeButtonIconColorState | static_cast<uint32_t>(CloseButtonIconColorState::NegativeWhenHoveredPressed);

        if (buttonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentTrafficLights) {
            negativeWhenHoveredPressedString = i18n("Traffic lights when hovered/pressed");
        } else {
            negativeWhenHoveredPressedString = i18n("Negative when hovered/pressed");
        }
    }

    QString closeIconAsSelectedString;
    switch (buttonIconColors->currentIndex()) {
    case InternalSettings::EnumButtonIconColors::TitleBarText:
        closeIconAsSelectedString = i18n("Titlebar text");
        break;
    case InternalSettings::EnumButtonIconColors::TitleBarTextNegativeClose:
        closeIconAsSelectedString = i18n("Negative");
        break;
    case InternalSettings::EnumButtonIconColors::Accent:
        closeIconAsSelectedString = i18n("Accent");
        break;
    case InternalSettings::EnumButtonIconColors::AccentNegativeClose:
        closeIconAsSelectedString = i18n("Negative");
        break;
    case InternalSettings::EnumButtonIconColors::AccentTrafficLights:
        closeIconAsSelectedString = i18n("Traffic lights");
        break;
    }

    // remove all items from the closeButtonIconColor combobox
    int count = closeButtonIconColor->count();
    for (int i = 0; i < count; i++) {
        closeButtonIconColor->removeItem(0);
    }

    // add each item, depending on the state
    if (static_cast<uint32_t>(CloseButtonIconColorState::AsSelected) & closeButtonIconColorState) {
        closeButtonIconColor->addItem(closeIconAsSelectedString);
    }

    if (static_cast<uint32_t>(CloseButtonIconColorState::NegativeWhenHoveredPressed) & closeButtonIconColorState) {
        closeButtonIconColor->addItem(negativeWhenHoveredPressedString);
    }

    if (static_cast<uint32_t>(CloseButtonIconColorState::White) & closeButtonIconColorState) {
        closeButtonIconColor->addItem(whiteString);
    }

    if (static_cast<uint32_t>(CloseButtonIconColorState::WhiteWhenHoveredPressed) & closeButtonIconColorState) {
        closeButtonIconColor->addItem(WhiteWhenHoveredPressedString);
    }

    if (visible) {
        closeButtonIconColor->setVisible(true);
        closeButtonIconColorLabel->setVisible(true);
    } else {
        closeButtonIconColor->setVisible(false);
        closeButtonIconColorLabel->setVisible(false);
    }
    if (active) {
        m_closeButtonIconColorStateActive = closeButtonIconColorState;
    } else {
        m_closeButtonIconColorStateInactive = closeButtonIconColorState;
    }
}

int ButtonColors::convertCloseButtonIconColorUiToSettingsIndex(bool active, const int uiIndex)
{
    /*
    enum struct CloseButtonIconColorState{
        AsSelected = 1,
        NegativeWhenHoveredPressed = 2,
        White = 4,
        WhiteWhenHoveredPressed = 8
    }

*/

    uint32_t state = active ? m_closeButtonIconColorStateActive : m_closeButtonIconColorStateInactive;
    uint32_t selectedStateBit = 1;
    int uiItemCount = 0;
    int numConfigItems = 4;

    for (int i = 0; i < numConfigItems; i++, selectedStateBit = selectedStateBit << 1) {
        if (state & selectedStateBit) {
            if (uiItemCount == uiIndex) {
                return i;
            }
            uiItemCount++;
        }
    }
    return -1;
}

int ButtonColors::convertCloseButtonIconColorSettingsToUiIndex(bool active, const int settingsIndex)
{
    /*
    enum struct CloseButtonIconColorState{
        AsSelected = 1,
        NegativeWhenHoveredPressed = 2,
        White = 4,
        WhiteWhenHoveredPressed = 8
    }

*/
    uint32_t state = active ? m_closeButtonIconColorStateActive : m_closeButtonIconColorStateInactive;
    uint32_t selectedStateBit = 1;
    int uiItemCount = 0;
    int numConfigItems = 4;

    for (int i = 0; i < numConfigItems; i++, selectedStateBit = selectedStateBit << 1) {
        if (state & selectedStateBit) {
            if (i == settingsIndex) {
                return uiItemCount;
            }
            uiItemCount++;
        }
    }
    return -1;
}

void ButtonColors::loadCloseButtonIconColor(bool active)
{
    QComboBox *closeButtonIconColor = active ? m_ui->closeButtonIconColorActive : m_ui->closeButtonIconColorInactive;
    int uiIndex = convertCloseButtonIconColorSettingsToUiIndex(active, m_internalSettings->closeButtonIconColor(active));
    if (uiIndex < 0)
        uiIndex = 0;
    closeButtonIconColor->setCurrentIndex(uiIndex);
}

// on buttonBackgroundColors change
void ButtonColors::setNegativeCloseBackgroundHoverPressState(bool active)
{
    QComboBox *buttonBackgroundColors = active ? m_ui->buttonBackgroundColorsActive : m_ui->buttonBackgroundColorsInactive;
    QCheckBox *negativeCloseBackgroundHoverPress = active ? m_ui->negativeCloseBackgroundHoverPressActive : m_ui->negativeCloseBackgroundHoverPressInactive;

    if (m_internalSettings->showCloseBackgroundNormally(active)
        && (buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose
            || buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose)) {
        negativeCloseBackgroundHoverPress->setText(i18n("Negative close on hover/press only"));
        negativeCloseBackgroundHoverPress->setVisible(true);
    } else if (m_internalSettings->showBackgroundNormally(active)
               && buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights) {
        negativeCloseBackgroundHoverPress->setText(i18n("Traffic lights on hover/press only"));
        negativeCloseBackgroundHoverPress->setVisible(true);
    } else {
        negativeCloseBackgroundHoverPress->setVisible(false);
    }

    QCheckBox *useHoverAccentActive = active ? m_ui->useHoverAccentActive : m_ui->useHoverAccentInactive;
    // also set useHoverAccent as also depends on buttonBackgroundColors change
    if (buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::Accent
        || buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose
        || buttonBackgroundColors->currentIndex() == InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights) {
        useHoverAccentActive->setVisible(true);
    } else {
        useHoverAccentActive->setVisible(false);
    }
}

void ButtonColors::setPoorBackgroundContrastThresholdVisible(bool active)
{
    QCheckBox *adjustBackgroundColorOnPoorContrast =
        active ? m_ui->adjustBackgroundColorOnPoorContrastActive : m_ui->adjustBackgroundColorOnPoorContrastInactive;
    QDoubleSpinBox *poorBackgroundContrastThreshold = active ? m_ui->poorBackgroundContrastThresholdActive : m_ui->poorBackgroundContrastThresholdInactive;
    QLabel *poorBackgroundContrastThreshold_label =
        active ? m_ui->poorBackgroundContrastThresholdActive_label : m_ui->poorBackgroundContrastThresholdInactive_label;
    if (adjustBackgroundColorOnPoorContrast->isChecked()) {
        poorBackgroundContrastThreshold->setVisible(true);
        poorBackgroundContrastThreshold_label->setVisible(true);
    } else {
        poorBackgroundContrastThreshold->setVisible(false);
        poorBackgroundContrastThreshold_label->setVisible(false);
    }
}

void ButtonColors::setPoorIconContrastThresholdVisible(bool active)
{
    QComboBox *onPoorIconContrast = active ? m_ui->onPoorIconContrastActive : m_ui->onPoorIconContrastInactive;
    QDoubleSpinBox *poorIconContrastThreshold = active ? m_ui->poorIconContrastThresholdActive : m_ui->poorIconContrastThresholdInactive;
    QLabel *poorIconContrastThreshold_label = active ? m_ui->poorIconContrastThresholdActive_label : m_ui->poorIconContrastThresholdInactive_label;
    if (onPoorIconContrast->currentIndex() != 0) {
        poorIconContrastThreshold->setVisible(true);
        poorIconContrastThreshold_label->setVisible(true);
    } else {
        poorIconContrastThreshold->setVisible(false);
        poorIconContrastThreshold_label->setVisible(false);
    }
}

void ButtonColors::resizeOverrideColorTable()
{
    QCheckBox *checkBox = qobject_cast<QCheckBox *>(QObject::sender());
    QTableWidget *table = nullptr;
    if (checkBox) {
        if (checkBox->parentWidget() && checkBox->parentWidget()->parentWidget() && checkBox->parentWidget()->parentWidget()->parentWidget()) {
            table = qobject_cast<QTableWidget *>(checkBox->parentWidget()->parentWidget()->parentWidget());
        }
    }

    if (table) {
        table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    }
}

void ButtonColors::showActiveOverrideGroupBox(const bool value)
{
    if (!value) {
        m_ui->activeOverrideGroupBox->setVisible(value);
    } else {
        m_ui->activeOverrideGroupBox->setVisible(value);
    }
}

void ButtonColors::showInactiveOverrideGroupBox(const bool value)
{
    if (!value) {
        m_ui->inactiveOverrideGroupBox->setVisible(value);
    } else {
        m_ui->inactiveOverrideGroupBox->setVisible(value);
    }
}

void ButtonColors::resizeDialog()
{
    if (!(m_ui->buttonColorOverrideToggleActive->isChecked() || m_ui->buttonColorOverrideToggleInactive->isChecked())) {
        this->showNormal();
        this->adjustSize();
    } else {
        this->showMaximized();
    }
}

void ButtonColors::tableVerticalHeaderSectionClicked(const int row)
{
    QHeaderView *headerview = qobject_cast<QHeaderView *>(QObject::sender());
    if (!headerview)
        return;

    QTableWidget *table = qobject_cast<QTableWidget *>(headerview->parentWidget());
    if (table) {
        QTableWidgetItem *item = table->verticalHeaderItem(row);
        if (item) {
            if (item->checkState() == Qt::CheckState::Checked) {
                setTableVerticalHeaderSectionCheckedState(table, row, false);
            } else {
                setTableVerticalHeaderSectionCheckedState(table, row, true);
            }
        }
    }
}

void ButtonColors::setTableVerticalHeaderSectionCheckedState(QTableWidget *table, const int row, const bool checked)
{
    QTableWidgetItem *item = table->verticalHeaderItem(row);
    if (!item)
        return;
    item->setCheckState(checked ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    item->setIcon(checked ? static_cast<ConfigWidget *>(m_parent)->lockIcon(LockIconState::Locked)
                          : static_cast<ConfigWidget *>(m_parent)->lockIcon(LockIconState::Unlocked));
}

void ButtonColors::copyCellCheckedStatusToOtherTable()
{
    if (m_loading || m_processingDefaults || !m_ui->lockButtonColorsActive->isChecked())
        return; // only do this for user interactions where the active/inactive lock is checked
    QCheckBox *checkBox = qobject_cast<QCheckBox *>(QObject::sender());
    if (!checkBox)
        return;
    bool columnOk, rowOk;
    int column = checkBox->property("column").toInt(&columnOk);
    int row = checkBox->property("row").toInt(&rowOk);

    if (!(columnOk && rowOk))
        return;
    if (!(checkBox->parentWidget() && checkBox->parentWidget()->parentWidget() && checkBox->parentWidget()->parentWidget()->parentWidget()))
        return;
    QTableWidget *table = qobject_cast<QTableWidget *>(checkBox->parentWidget()->parentWidget()->parentWidget());
    if (!table)
        return;
    bool destinationActive;
    QCheckBox *otherOverrideToggle;
    if (table == m_ui->overrideColorTableActive) {
        destinationActive = false;
        otherOverrideToggle = m_ui->buttonColorOverrideToggleInactive;
    } else if (table == m_ui->overrideColorTableInactive) {
        destinationActive = true;
        otherOverrideToggle = m_ui->buttonColorOverrideToggleActive;
    } else
        return;

    QCheckBox *destinationCheckBox = nullptr;
    QComboBox *destinationComboBox = nullptr;
    KColorButton *destinationColorButton = nullptr;
    QSpinBox *destinationSpinBox = nullptr;

    if (checkBoxComboBoxColorButtonSpinBoxAtTableCell(destinationActive,
                                                      column,
                                                      row,
                                                      destinationCheckBox,
                                                      destinationComboBox,
                                                      destinationColorButton,
                                                      destinationSpinBox)) {
        destinationCheckBox->setChecked(checkBox->isChecked());
        if (checkBox->isChecked()) {
            otherOverrideToggle->setChecked(true);
        }
    }
}

// given a comboBox/colorButton/spinBox sending a changed signal to this slot, gets the table cell in which the colorButton was located and copies the same data
// to the equivalent inactive window table cell and to the rest of the row if the row lock is set
void ButtonColors::copyCellColorDataToOtherCells()
{
    if (m_loading || m_processingDefaults)
        return; // only do this for user interactions

    QWidget *source = qobject_cast<QWidget *>(QObject::sender());
    if (!source) {
        return;
    }
    QComboBox *comboBox;
    KColorButton *colorButton;
    QSpinBox *spinBox;
    comboBox = qobject_cast<QComboBox *>(source);
    colorButton = qobject_cast<KColorButton *>(source);
    spinBox = qobject_cast<QSpinBox *>(source);
    if (!(comboBox || colorButton || spinBox)) {
        return;
    }

    bool columnOk, rowOk;
    int column = source->property("column").toInt(&columnOk);
    int row = source->property("row").toInt(&rowOk);

    if (!(columnOk && rowOk)) {
        return;
    }
    if (!(source->parentWidget() && source->parentWidget()->parentWidget() && source->parentWidget()->parentWidget()->parentWidget())) {
        return;
    }
    QTableWidget *table = qobject_cast<QTableWidget *>(source->parentWidget()->parentWidget()->parentWidget());
    if (!table) {
        return;
    }
    QPushButton *activeInactiveLock;
    bool sourceActive;
    bool destinationActive;
    if (table == m_ui->overrideColorTableActive) {
        activeInactiveLock = m_ui->lockButtonColorsActive;
        sourceActive = true;
        destinationActive = false;
    } else if (table == m_ui->overrideColorTableInactive) {
        activeInactiveLock = m_ui->lockButtonColorsInactive;
        sourceActive = false;
        destinationActive = true;
    } else
        return;

    if (activeInactiveLock->isChecked()) { // copy the value between active and inactive tables
        QCheckBox *destinationCheckBox = nullptr;
        QComboBox *destinationComboBox = nullptr;
        KColorButton *destinationColorButton = nullptr;
        QSpinBox *destinationSpinBox = nullptr;

        if (checkBoxComboBoxColorButtonSpinBoxAtTableCell(destinationActive,
                                                          column,
                                                          row,
                                                          destinationCheckBox,
                                                          destinationComboBox,
                                                          destinationColorButton,
                                                          destinationSpinBox)) {
            if (comboBox) {
                destinationComboBox->setCurrentIndex(comboBox->currentIndex());
            } else if (colorButton) {
                destinationColorButton->setColor(colorButton->color());
            } else if (spinBox) {
                destinationSpinBox->setValue(spinBox->value());
            }
        }
    }

    QTableWidgetItem *item = table->verticalHeaderItem(row);
    if (item) { // populate the whole row in the same table with the same value
        if (item->checkState() == Qt::CheckState::Checked) { // if the lock icon is locked for the row
            for (int destinationColumn = 0; destinationColumn < m_colorOverridableButtonTypesStrings.count(); destinationColumn++) {
                QCheckBox *destinationCheckBox = nullptr;
                QComboBox *destinationComboBox = nullptr;
                KColorButton *destinationColorButton = nullptr;
                QSpinBox *destinationSpinBox = nullptr;

                if (destinationColumn != column) { // copy the values to all other colorBoxes in the row
                    if (checkBoxComboBoxColorButtonSpinBoxAtTableCell(sourceActive,
                                                                      destinationColumn,
                                                                      row,
                                                                      destinationCheckBox,
                                                                      destinationComboBox,
                                                                      destinationColorButton,
                                                                      destinationSpinBox)) {
                        if (comboBox) {
                            destinationComboBox->setCurrentIndex(comboBox->currentIndex());
                        } else if (colorButton) {
                            destinationColorButton->setColor(colorButton->color());
                        } else if (spinBox) {
                            destinationSpinBox->setValue(spinBox->value());
                        }
                        destinationCheckBox->setChecked(true);
                    }
                }
            }
        }
    }
}

bool ButtonColors::checkBoxComboBoxColorButtonSpinBoxAtTableCell(const bool active,
                                                                 const int column,
                                                                 const int row,
                                                                 QCheckBox *&outputCheckBox,
                                                                 QComboBox *&outputComboBox,
                                                                 KColorButton *&outputColorButton,
                                                                 QSpinBox *&outputSpinBox)
{
    QTableWidget *table = active ? m_ui->overrideColorTableActive : m_ui->overrideColorTableInactive;
    QString activeString = active ? QStringLiteral("Active") : QStringLiteral("Inactive");

    if (!table)
        return false;
    QWidget *widget = table->cellWidget(row, column);
    if (!widget)
        return false;
    if (!widget->children().count())
        return false;

    outputCheckBox = widget->findChild<QCheckBox *>(QStringLiteral("checkBox") + activeString + QString::number(column) + QString::number(row));
    if (!outputCheckBox)
        return false;

    outputComboBox = widget->findChild<QComboBox *>(QStringLiteral("comboBox") + activeString + QString::number(column) + QString::number(row));
    if (!outputComboBox)
        return false;

    outputColorButton = widget->findChild<KColorButton *>(QStringLiteral("colorButton") + activeString + QString::number(column) + QString::number(row));
    if (!outputColorButton)
        return false;

    outputSpinBox = widget->findChild<QSpinBox *>(QStringLiteral("spinBox") + activeString + QString::number(column) + QString::number(row));
    if (!outputColorButton)
        return false;

    return true;
}

QByteArray ButtonColors::encodeColorOverrideTableColumn(const int column, const bool active, const bool reset)
{
    QJsonObject buttonStatesObject;

    for (int row = 0; row < overridableButtonColorStatesJsonStrings.count(); row++) {
        QCheckBox *overrideCheckBox = nullptr;
        QComboBox *overrideComboBox = nullptr;
        KColorButton *overrideColorButton = nullptr;
        QSpinBox *overrideSpinBox = nullptr;

        if (checkBoxComboBoxColorButtonSpinBoxAtTableCell(active, column, row, overrideCheckBox, overrideComboBox, overrideColorButton, overrideSpinBox)) {
            if (overrideCheckBox->isChecked() && !reset) {
                QString buttonState = overridableButtonColorStatesJsonStrings[row];
                QString colorItem = overrideColorItems[overrideComboBox->currentIndex()];

                QJsonArray colorArray;
                if (colorItem == QStringLiteral("Custom")) {
                    if (overrideColorButton->color().isValid()) {
                        if (overrideSpinBox->value() != 100) {
                            colorArray.append(overrideSpinBox->value());
                        }
                        colorArray.append(overrideColorButton->color().red());
                        colorArray.append(overrideColorButton->color().green());
                        colorArray.append(overrideColorButton->color().blue());
                    }
                } else {
                    colorArray.append(colorItem);
                    if (overrideSpinBox->value() != 100) {
                        colorArray.append(overrideSpinBox->value());
                    }
                }

                if (!colorArray.isEmpty()) {
                    buttonStatesObject.insert(buttonState, colorArray);
                }
            }
        }
    }

    if (buttonStatesObject.isEmpty()) {
        return QByteArray();
    } else {
        QJsonDocument document(buttonStatesObject);
        return document.toJson(QJsonDocument::JsonFormat::Compact);
    }
}

bool ButtonColors::decodeOverrideColorsAndLoadTableColumn(const QByteArray overrideColorColumnJson, const int column, const bool active)
{
    QJsonDocument document = QJsonDocument::fromJson(overrideColorColumnJson);

    QJsonObject buttonStatesObject = document.object();
    bool overrideColorLoaded = false;
    for (int row = 0; row < overridableButtonColorStatesJsonStrings.count(); row++) {
        QString buttonState = overridableButtonColorStatesJsonStrings[row];
        QCheckBox *overrideCheckBox = nullptr;
        QComboBox *overrideComboBox = nullptr;
        KColorButton *overrideColorButton = nullptr;
        QSpinBox *overrideSpinBox = nullptr;

        if (checkBoxComboBoxColorButtonSpinBoxAtTableCell(active, column, row, overrideCheckBox, overrideComboBox, overrideColorButton, overrideSpinBox)) {
            QJsonArray colorArray = buttonStatesObject.value(buttonState).toArray();
            QColor color;
            switch (colorArray.count()) {
            case 0:
            default:
                overrideCheckBox->setChecked(false);
                overrideComboBox->setCurrentIndex(0);
                overrideColorButton->setColor(QColor());
                overrideSpinBox->setValue(100);
                break;
            case 1:
                overrideCheckBox->setChecked(true);
                overrideComboBox->setCurrentIndex(overrideColorItems.indexOf(colorArray[0].toString()));
                overrideColorButton->setColor(QColor());
                overrideSpinBox->setValue(100);
                overrideColorLoaded = true;
                break;
            case 2:
                overrideCheckBox->setChecked(true);
                overrideComboBox->setCurrentIndex(overrideColorItems.indexOf(colorArray[0].toString()));
                overrideColorButton->setColor(QColor());
                overrideSpinBox->setValue(colorArray[1].toInt());
                overrideColorLoaded = true;
                break;
            case 3:
                overrideCheckBox->setChecked(true);
                overrideComboBox->setCurrentIndex(0);
                color.setRed(colorArray[0].toInt());
                color.setGreen(colorArray[1].toInt());
                color.setBlue(colorArray[2].toInt());
                overrideColorButton->setColor(color);
                overrideSpinBox->setValue(100);
                overrideColorLoaded = true;
                break;
            case 4:
                overrideCheckBox->setChecked(true);
                overrideComboBox->setCurrentIndex(0);

                color.setRed(colorArray[1].toInt());
                color.setGreen(colorArray[2].toInt());
                color.setBlue(colorArray[3].toInt());
                overrideColorButton->setColor(color);
                overrideSpinBox->setValue(colorArray[0].toInt());
                overrideColorLoaded = true;
                break;
            }
        }
    }

    return overrideColorLoaded;
}

QByteArray ButtonColors::encodeColorOverrideLockStates(const bool active)
{
    QTableWidget *table = active ? m_ui->overrideColorTableActive : m_ui->overrideColorTableInactive;
    QJsonArray lockedArray;

    for (int row = 0; row < overridableButtonColorStatesJsonStrings.count(); row++) {
        QTableWidgetItem *item = table->verticalHeaderItem(row);
        if (item) {
            if (item->checkState() == Qt::CheckState::Checked) { // set the bit of the row by OR-ing the bitmask with the existing value
                lockedArray.append(overridableButtonColorStatesJsonStrings[row]);
            }
        }
    }

    if (lockedArray.isEmpty()) {
        return QByteArray();
    } else {
        QJsonDocument document(lockedArray);
        return document.toJson(QJsonDocument::JsonFormat::Compact);
    }
}

void ButtonColors::decodeAndLoadColorOverrideLockStates(const bool active)
{
    QByteArray lockStatesJson = m_internalSettings->buttonOverrideColorsLockStates(active).toUtf8();
    QJsonDocument document = QJsonDocument::fromJson(lockStatesJson);
    QJsonArray lockedArray = document.array();

    QTableWidget *table = active ? m_ui->overrideColorTableActive : m_ui->overrideColorTableInactive;

    for (int row = 0; row < overridableButtonColorStatesJsonStrings.count(); row++) {
        QString state = overridableButtonColorStatesJsonStrings[row];
        QTableWidgetItem *item = table->verticalHeaderItem(row);
        if (!item)
            continue;
        if (lockedArray.contains(state)) {
            setTableVerticalHeaderSectionCheckedState(table, row, true);
        } else {
            setTableVerticalHeaderSectionCheckedState(table, row, false);
        }
    }
}

void ButtonColors::loadButtonPaletteColorsIcons()
{
    if (m_ui->tabWidget->currentIndex() == 0)
        loadButtonPaletteColorsIconsMain(true);
    else if (m_ui->tabWidget->currentIndex() == 1)
        loadButtonPaletteColorsIconsMain(false);
}

void ButtonColors::loadButtonPaletteColorsIconsMain(bool active)
{
    auto uiButtonIconColors = active ? m_ui->buttonIconColorsActive : m_ui->buttonIconColorsInactive;
    auto uiCloseButtonIconColor = active ? m_ui->closeButtonIconColorActive : m_ui->closeButtonIconColorInactive;
    auto uiButtonBackgroundColors = active ? m_ui->buttonBackgroundColorsActive : m_ui->buttonBackgroundColorsInactive;
    auto uiNegativeCloseBackgroundHoverPress = active ? m_ui->negativeCloseBackgroundHoverPressActive : m_ui->negativeCloseBackgroundHoverPressInactive;
    auto uiSetOnPoorIconContrast = active ? m_ui->onPoorIconContrastActive : m_ui->onPoorIconContrastInactive;
    auto uiSetAdjustBackgroundColorOnPoorContrast =
        active ? m_ui->adjustBackgroundColorOnPoorContrastActive : m_ui->adjustBackgroundColorOnPoorContrastInactive;
    auto uiSetButtonIconOpacity = active ? m_ui->buttonIconOpacityActive : m_ui->buttonIconOpacityInactive;
    auto uiSetButtonBackgroundOpacity = active ? m_ui->buttonBackgroundOpacityActive : m_ui->buttonBackgroundOpacityInactive;
    auto uiUseHoverAccentActive = active ? m_ui->useHoverAccentActive : m_ui->useHoverAccentInactive;

    InternalSettingsPtr temporaryColorSettings =
        InternalSettingsPtr(new InternalSettings); // temporary settings that reflect the UI, only to instantly update the colours displayed in the UI
    temporaryColorSettings->load();

    // temporaryColorSettings->setButtonIconColors(m_ui->buttonIconColors->currentIndex());
    temporaryColorSettings->setCloseButtonIconColor(active, convertCloseButtonIconColorUiToSettingsIndex(active, uiCloseButtonIconColor->currentIndex()));
    // temporaryColorSettings->setButtonBackgroundColors(m_ui->buttonBackgroundColors->currentIndex());
    temporaryColorSettings->setNegativeCloseBackgroundHoverPress(active, uiNegativeCloseBackgroundHoverPress->isChecked());
    // temporaryColorSettings->setLockButtonColorsActive(m_ui->lockButtonColorsActive->isChecked());
    temporaryColorSettings->setOnPoorIconContrast(active, uiSetOnPoorIconContrast->currentIndex());
    temporaryColorSettings->setAdjustBackgroundColorOnPoorContrast(active, uiSetAdjustBackgroundColorOnPoorContrast->isChecked());
    temporaryColorSettings->setButtonIconOpacity(active, uiSetButtonIconOpacity->value());
    temporaryColorSettings->setButtonBackgroundOpacity(active, uiSetButtonBackgroundOpacity->value());
    temporaryColorSettings->setUseHoverAccent(active, uiUseHoverAccentActive->isChecked());

    DecorationColors decorationPalette(false);
    decorationPalette.generateDecorationColors(QApplication::palette(),
                                               temporaryColorSettings,
                                               m_systemTitleBarTextActive,
                                               m_systemTitlebarBackgroundActive,
                                               m_systemTitleBarTextInactive,
                                               m_systemTitlebarBackgroundInactive,
                                               "");

    auto getGroup = [](DecorationButtonPalette *palette, bool active) {
        return active ? palette->active() : palette->inactive();
    };
    DecorationButtonPalette closeButtonPalette(DecorationButtonType::Close);
    DecorationButtonPalette maximizeButtonPalette(DecorationButtonType::Maximize);
    DecorationButtonPalette minimizeButtonPalette(DecorationButtonType::Minimize);
    DecorationButtonPalette otherButtonPalette(DecorationButtonType::Custom);

    QList<DecorationButtonPalette *> otherCloseButtonList{&otherButtonPalette, &closeButtonPalette};
    otherCloseButtonList = sortButtonsAsPerKwinConfig(otherCloseButtonList);
    QList<DecorationButtonPalette *> otherTrafficLightsButtonList{&otherButtonPalette, &closeButtonPalette, &maximizeButtonPalette, &minimizeButtonPalette};
    otherTrafficLightsButtonList = sortButtonsAsPerKwinConfig(otherTrafficLightsButtonList);
    QList<DecorationButtonPalette *> trafficLightsButtonList{&closeButtonPalette, &maximizeButtonPalette, &minimizeButtonPalette};
    trafficLightsButtonList = sortButtonsAsPerKwinConfig(trafficLightsButtonList);

    qreal size = 32;

    QRectF oneByThree00(QPointF(0, 0), QPointF(size, size / 3));
    QRectF oneByThree01(QPointF(0, size / 3), QPointF(size, size * 2 / 3));
    QRectF oneByThree02(QPointF(0, size * 2 / 3), QPointF(size, size));

    QRectF twoByThree[2][3];
    twoByThree[0][0] = QRectF(QPointF(0, 0), QPointF(size / 2, size / 3));
    twoByThree[0][1] = QRectF(QPointF(0, size / 3), QPointF(size / 2, size * 2 / 3));
    twoByThree[0][2] = QRectF(QPointF(0, size * 2 / 3), QPointF(size / 2, size));
    twoByThree[1][0] = QRectF(QPointF(size / 2, 0), QPointF(size, size / 3));
    twoByThree[1][1] = QRectF(QPointF(size / 2, size / 3), QPointF(size, size * 2 / 3));
    twoByThree[1][2] = QRectF(QPointF(size / 2, size * 2 / 3), QPointF(size, size));

    QRectF threeByThree[3][3];
    threeByThree[0][0] = QRectF(QPointF(0, 0), QPointF(size / 3, size / 3));
    threeByThree[0][1] = QRectF(QPointF(0, size / 3), QPointF(size / 3, size * 2 / 3));
    threeByThree[0][2] = QRectF(QPointF(0, size * 2 / 3), QPointF(size / 3, size));
    threeByThree[1][0] = QRectF(QPointF(size / 3, 0), QPointF(size * 2 / 3, size / 3));
    threeByThree[1][1] = QRectF(QPointF(size / 3, size / 3), QPointF(size * 2 / 3, size * 2 / 3));
    threeByThree[1][2] = QRectF(QPointF(size / 3, size * 2 / 3), QPointF(size * 2 / 3, size));
    threeByThree[2][0] = QRectF(QPointF(size * 2 / 3, 0), QPointF(size, size / 3));
    threeByThree[2][1] = QRectF(QPointF(size * 2 / 3, size / 3), QPointF(size, size * 2 / 3));
    threeByThree[2][2] = QRectF(QPointF(size * 2 / 3, size * 2 / 3), QPointF(size, size));

    QRectF fourByThree[4][3];
    fourByThree[0][0] = QRectF(QPointF(0, 0), QPointF(size / 4, size / 3));
    fourByThree[0][1] = QRectF(QPointF(0, size / 3), QPointF(size / 4, size * 2 / 3));
    fourByThree[0][2] = QRectF(QPointF(0, size * 2 / 3), QPointF(size / 4, size));
    fourByThree[1][0] = QRectF(QPointF(size / 4, 0), QPointF(size / 2, size / 3));
    fourByThree[1][1] = QRectF(QPointF(size / 4, size / 3), QPointF(size / 2, size * 2 / 3));
    fourByThree[1][2] = QRectF(QPointF(size / 4, size * 2 / 3), QPointF(size / 2, size));
    fourByThree[2][0] = QRectF(QPointF(size / 2, 0), QPointF(size * 3 / 4, size / 3));
    fourByThree[2][1] = QRectF(QPointF(size / 2, size / 3), QPointF(size * 3 / 4, size * 2 / 3));
    fourByThree[2][2] = QRectF(QPointF(size / 2, size * 2 / 3), QPointF(size * 3 / 4, size));
    fourByThree[3][0] = QRectF(QPointF(size * 3 / 4, 0), QPointF(size, size / 3));
    fourByThree[3][1] = QRectF(QPointF(size * 3 / 4, size / 3), QPointF(size, size * 2 / 3));
    fourByThree[3][2] = QRectF(QPointF(size * 3 / 4, size * 2 / 3), QPointF(size, size));

    QPixmap pixmap(size, size);
    std::unique_ptr<QPainter> painter = std::make_unique<QPainter>(&pixmap);
    painter->setPen(Qt::NoPen);

    // background colors
    temporaryColorSettings->setButtonBackgroundColors(active, InternalSettings::EnumButtonBackgroundColors::TitleBarText);
    for (auto &buttonPalette : otherCloseButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    };

    uiButtonBackgroundColors->setIconSize(QSize(size, size));
    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 2; i++) {
        if (i < otherCloseButtonList.count()) {
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundNormal.isValid()
                                  ? getGroup(otherCloseButtonList[i], active)->backgroundNormal
                                  : Qt::transparent);
            painter->drawRect(twoByThree[i][0]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundHover.isValid() ? getGroup(otherCloseButtonList[i], active)->backgroundHover
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][1]);

            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundPress.isValid() ? getGroup(otherCloseButtonList[i], active)->backgroundPress
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][2]);
        }
    }
    QIcon titlebarText(pixmap);
    uiButtonBackgroundColors->setItemIcon(InternalSettings::EnumButtonBackgroundColors::TitleBarText, titlebarText);

    temporaryColorSettings->setButtonBackgroundColors(active, InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose);
    for (auto &buttonPalette : otherCloseButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    };

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 2; i++) {
        if (i < otherCloseButtonList.count()) {
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundNormal.isValid()
                                  ? getGroup(otherCloseButtonList[i], active)->backgroundNormal
                                  : Qt::transparent);
            painter->drawRect(twoByThree[i][0]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundHover.isValid() ? getGroup(otherCloseButtonList[i], active)->backgroundHover
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][1]);

            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundPress.isValid() ? getGroup(otherCloseButtonList[i], active)->backgroundPress
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][2]);
        }
    }
    QIcon backgroundTitleBarTextNegativeClose(pixmap);
    uiButtonBackgroundColors->setItemIcon(InternalSettings::EnumButtonBackgroundColors::TitleBarTextNegativeClose, backgroundTitleBarTextNegativeClose);

    temporaryColorSettings->setButtonBackgroundColors(active, InternalSettings::EnumButtonBackgroundColors::Accent);
    for (auto &buttonPalette : otherCloseButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    };

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 2; i++) {
        if (i < otherCloseButtonList.count()) {
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundNormal.isValid()
                                  ? getGroup(otherCloseButtonList[i], active)->backgroundNormal
                                  : Qt::transparent);
            painter->drawRect(twoByThree[i][0]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundHover.isValid() ? getGroup(otherCloseButtonList[i], active)->backgroundHover
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][1]);

            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundPress.isValid() ? getGroup(otherCloseButtonList[i], active)->backgroundPress
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][2]);
        }
    }
    QIcon backgroundAccent(pixmap);
    uiButtonBackgroundColors->setItemIcon(InternalSettings::EnumButtonBackgroundColors::Accent, backgroundAccent);

    temporaryColorSettings->setButtonBackgroundColors(active, InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose);
    for (auto &buttonPalette : otherCloseButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    };

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 2; i++) {
        if (i < otherCloseButtonList.count()) {
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundNormal.isValid()
                                  ? getGroup(otherCloseButtonList[i], active)->backgroundNormal
                                  : Qt::transparent);
            painter->drawRect(twoByThree[i][0]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundHover.isValid() ? getGroup(otherCloseButtonList[i], active)->backgroundHover
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][1]);

            painter->setBrush(getGroup(otherCloseButtonList[i], active)->backgroundPress.isValid() ? getGroup(otherCloseButtonList[i], active)->backgroundPress
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][2]);
        }
    }
    QIcon backgroundAccentNegativeClose(pixmap);
    uiButtonBackgroundColors->setItemIcon(InternalSettings::EnumButtonBackgroundColors::AccentNegativeClose, backgroundAccentNegativeClose);

    temporaryColorSettings->setButtonBackgroundColors(active, InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights);

    for (auto &buttonPalette : otherTrafficLightsButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    }

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 4; i++) {
        if (i < otherTrafficLightsButtonList.count()) {
            painter->setBrush(getGroup(otherTrafficLightsButtonList[i], active)->backgroundNormal.isValid()
                                  ? getGroup(otherTrafficLightsButtonList[i], active)->backgroundNormal
                                  : Qt::transparent);
            painter->drawRect(fourByThree[i][0]);
            painter->setBrush(getGroup(otherTrafficLightsButtonList[i], active)->backgroundHover.isValid()
                                  ? getGroup(otherTrafficLightsButtonList[i], active)->backgroundHover
                                  : Qt::transparent);
            painter->drawRect(fourByThree[i][1]);
            painter->setBrush(getGroup(otherTrafficLightsButtonList[i], active)->backgroundPress.isValid()
                                  ? getGroup(otherTrafficLightsButtonList[i], active)->backgroundPress
                                  : Qt::transparent);
            painter->drawRect(fourByThree[i][2]);
        }
    }
    QIcon backgroundAccentTrafficLightsClose(pixmap);
    uiButtonBackgroundColors->setItemIcon(InternalSettings::EnumButtonBackgroundColors::AccentTrafficLights, backgroundAccentTrafficLightsClose);

    // icon colors ---------------------------------------------------------------------------------
    uiButtonIconColors->setIconSize(QSize(24, 24));
    temporaryColorSettings->setButtonBackgroundColors(active, uiButtonBackgroundColors->currentIndex());

    temporaryColorSettings->setButtonIconColors(active, InternalSettings::EnumButtonIconColors::TitleBarText);
    for (auto &buttonPalette : otherCloseButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    };

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 2; i++) {
        if (i < otherCloseButtonList.count()) {
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundNormal.isValid()
                                  ? getGroup(otherCloseButtonList[i], active)->foregroundNormal
                                  : Qt::transparent);
            painter->drawRect(twoByThree[i][0]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundHover.isValid() ? getGroup(otherCloseButtonList[i], active)->foregroundHover
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][1]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundPress.isValid() ? getGroup(otherCloseButtonList[i], active)->foregroundPress
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][2]);
        }
    }
    QIcon iconTitleBarText(pixmap);
    uiButtonIconColors->setItemIcon(InternalSettings::EnumButtonIconColors::TitleBarText, iconTitleBarText);

    temporaryColorSettings->setButtonIconColors(active, InternalSettings::EnumButtonIconColors::TitleBarTextNegativeClose);
    for (auto &buttonPalette : otherCloseButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    };

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 2; i++) {
        if (i < otherCloseButtonList.count()) {
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundNormal.isValid()
                                  ? getGroup(otherCloseButtonList[i], active)->foregroundNormal
                                  : Qt::transparent);
            painter->drawRect(twoByThree[i][0]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundHover.isValid() ? getGroup(otherCloseButtonList[i], active)->foregroundHover
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][1]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundPress.isValid() ? getGroup(otherCloseButtonList[i], active)->foregroundPress
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][2]);
        }
    }
    QIcon iconTitleBarTextNegativeClose(pixmap);
    uiButtonIconColors->setItemIcon(InternalSettings::EnumButtonIconColors::TitleBarTextNegativeClose, iconTitleBarTextNegativeClose);

    temporaryColorSettings->setButtonIconColors(active, InternalSettings::EnumButtonIconColors::Accent);
    for (auto &buttonPalette : otherCloseButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    };

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 2; i++) {
        if (i < otherCloseButtonList.count()) {
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundNormal.isValid()
                                  ? getGroup(otherCloseButtonList[i], active)->foregroundNormal
                                  : Qt::transparent);
            painter->drawRect(twoByThree[i][0]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundHover.isValid() ? getGroup(otherCloseButtonList[i], active)->foregroundHover
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][1]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundPress.isValid() ? getGroup(otherCloseButtonList[i], active)->foregroundPress
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][2]);
        }
    }
    QIcon iconAccent(pixmap);
    uiButtonIconColors->setItemIcon(InternalSettings::EnumButtonIconColors::Accent, iconAccent);

    temporaryColorSettings->setButtonIconColors(active, InternalSettings::EnumButtonIconColors::AccentNegativeClose);
    for (auto &buttonPalette : otherCloseButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    };

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 2; i++) {
        if (i < otherCloseButtonList.count()) {
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundNormal.isValid()
                                  ? getGroup(otherCloseButtonList[i], active)->foregroundNormal
                                  : Qt::transparent);
            painter->drawRect(twoByThree[i][0]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundHover.isValid() ? getGroup(otherCloseButtonList[i], active)->foregroundHover
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][1]);
            painter->setBrush(getGroup(otherCloseButtonList[i], active)->foregroundPress.isValid() ? getGroup(otherCloseButtonList[i], active)->foregroundPress
                                                                                                   : Qt::transparent);
            painter->drawRect(twoByThree[i][2]);
        }
    }
    QIcon iconAccentNegativeClose(pixmap);
    uiButtonIconColors->setItemIcon(InternalSettings::EnumButtonIconColors::AccentNegativeClose, iconAccentNegativeClose);

    temporaryColorSettings->setButtonIconColors(active, InternalSettings::EnumButtonIconColors::AccentTrafficLights);
    for (auto &buttonPalette : otherTrafficLightsButtonList) {
        buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
    }

    pixmap.fill(Qt::transparent);
    for (int i = 0; i < 4; i++) {
        if (i < otherTrafficLightsButtonList.count()) {
            painter->setBrush(getGroup(otherTrafficLightsButtonList[i], active)->foregroundNormal.isValid()
                                  ? getGroup(otherTrafficLightsButtonList[i], active)->foregroundNormal
                                  : Qt::transparent);
            painter->drawRect(fourByThree[i][0]);
            painter->setBrush(getGroup(otherTrafficLightsButtonList[i], active)->foregroundHover.isValid()
                                  ? getGroup(otherTrafficLightsButtonList[i], active)->foregroundHover
                                  : Qt::transparent);
            painter->drawRect(fourByThree[i][1]);
            painter->setBrush(getGroup(otherTrafficLightsButtonList[i], active)->foregroundPress.isValid()
                                  ? getGroup(otherTrafficLightsButtonList[i], active)->foregroundPress
                                  : Qt::transparent);
            painter->drawRect(fourByThree[i][2]);
        }
    }
    QIcon iconAccentTrafficLights(pixmap);
    uiButtonIconColors->setItemIcon(InternalSettings::EnumButtonIconColors::AccentTrafficLights, iconAccentTrafficLights);

    // closeButtonIconColor icons ----------------------------------------------------

    uiCloseButtonIconColor->setIconSize(QSize(16, 16));
    temporaryColorSettings->setButtonIconColors(active, uiButtonIconColors->currentIndex());

    int uiItemIndex = convertCloseButtonIconColorSettingsToUiIndex(active, InternalSettings::EnumCloseButtonIconColor::AsSelected);
    if (uiItemIndex >= 0) {
        temporaryColorSettings->setCloseButtonIconColor(active, InternalSettings::EnumCloseButtonIconColor::AsSelected);
        if (uiButtonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentTrafficLights) {
            for (auto &buttonPalette : trafficLightsButtonList) {
                buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
            }
            pixmap.fill(Qt::transparent);
            for (int i = 0; i < 3; i++) {
                if (i < trafficLightsButtonList.count()) {
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundNormal.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundNormal
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][0]);
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundHover.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundHover
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][1]);
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundPress.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundPress
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][2]);
                }
            }
        } else {
            closeButtonPalette.generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
            pixmap.fill(Qt::transparent);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundNormal.isValid() ? getGroup(&closeButtonPalette, active)->foregroundNormal
                                                                                                : Qt::transparent);
            painter->drawRect(oneByThree00);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundHover.isValid() ? getGroup(&closeButtonPalette, active)->foregroundHover
                                                                                               : Qt::transparent);
            painter->drawRect(oneByThree01);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundPress.isValid() ? getGroup(&closeButtonPalette, active)->foregroundPress
                                                                                               : Qt::transparent);
            painter->drawRect(oneByThree02);
        }
        QIcon icon(pixmap);
        uiCloseButtonIconColor->setItemIcon(uiItemIndex, icon);
    }

    uiItemIndex = convertCloseButtonIconColorSettingsToUiIndex(active, InternalSettings::EnumCloseButtonIconColor::NegativeWhenHoverPress);
    if (uiItemIndex >= 0) {
        temporaryColorSettings->setCloseButtonIconColor(active, InternalSettings::EnumCloseButtonIconColor::NegativeWhenHoverPress);
        if (uiButtonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentTrafficLights) {
            for (auto &buttonPalette : trafficLightsButtonList) {
                buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
            }
            pixmap.fill(Qt::transparent);
            for (int i = 0; i < 3; i++) {
                if (i < trafficLightsButtonList.count()) {
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundNormal.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundNormal
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][0]);
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundHover.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundHover
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][1]);
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundPress.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundPress
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][2]);
                }
            }
        } else {
            closeButtonPalette.generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
            pixmap.fill(Qt::transparent);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundNormal.isValid() ? getGroup(&closeButtonPalette, active)->foregroundNormal
                                                                                                : Qt::transparent);
            painter->drawRect(oneByThree00);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundHover.isValid() ? getGroup(&closeButtonPalette, active)->foregroundHover
                                                                                               : Qt::transparent);
            painter->drawRect(oneByThree01);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundPress.isValid() ? getGroup(&closeButtonPalette, active)->foregroundPress
                                                                                               : Qt::transparent);
            painter->drawRect(oneByThree02);
        }
        QIcon icon(pixmap);
        uiCloseButtonIconColor->setItemIcon(uiItemIndex, icon);
    }

    uiItemIndex = convertCloseButtonIconColorSettingsToUiIndex(active, InternalSettings::EnumCloseButtonIconColor::White);
    if (uiItemIndex >= 0) {
        temporaryColorSettings->setCloseButtonIconColor(active, InternalSettings::EnumCloseButtonIconColor::White);
        if (uiButtonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentTrafficLights) {
            for (auto &buttonPalette : trafficLightsButtonList) {
                buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
            }
            pixmap.fill(Qt::transparent);
            for (int i = 0; i < 3; i++) {
                if (i < trafficLightsButtonList.count()) {
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundNormal.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundNormal
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][0]);
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundHover.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundHover
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][1]);
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundPress.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundPress
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][2]);
                }
            }
        } else {
            closeButtonPalette.generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
            pixmap.fill(Qt::transparent);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundNormal.isValid() ? getGroup(&closeButtonPalette, active)->foregroundNormal
                                                                                                : Qt::transparent);
            painter->drawRect(oneByThree00);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundHover.isValid() ? getGroup(&closeButtonPalette, active)->foregroundHover
                                                                                               : Qt::transparent);
            painter->drawRect(oneByThree01);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundPress.isValid() ? getGroup(&closeButtonPalette, active)->foregroundPress
                                                                                               : Qt::transparent);
            painter->drawRect(oneByThree02);
        }
        QIcon icon(pixmap);
        uiCloseButtonIconColor->setItemIcon(uiItemIndex, icon);
    }

    uiItemIndex = convertCloseButtonIconColorSettingsToUiIndex(active, InternalSettings::EnumCloseButtonIconColor::WhiteWhenHoverPress);
    if (uiItemIndex >= 0) {
        temporaryColorSettings->setCloseButtonIconColor(active, InternalSettings::EnumCloseButtonIconColor::WhiteWhenHoverPress);

        if (uiButtonIconColors->currentIndex() == InternalSettings::EnumButtonIconColors::AccentTrafficLights) {
            for (auto &buttonPalette : trafficLightsButtonList) {
                buttonPalette->generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
            }
            pixmap.fill(Qt::transparent);
            for (int i = 0; i < 3; i++) {
                if (i < trafficLightsButtonList.count()) {
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundNormal.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundNormal
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][0]);
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundHover.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundHover
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][1]);
                    painter->setBrush(getGroup(trafficLightsButtonList[i], active)->foregroundPress.isValid()
                                          ? getGroup(trafficLightsButtonList[i], active)->foregroundPress
                                          : Qt::transparent);
                    painter->drawRect(threeByThree[i][2]);
                }
            }
        } else {
            closeButtonPalette.generate(temporaryColorSettings, decorationPalette.active(), decorationPalette.inactive(), true, active);
            pixmap.fill(Qt::transparent);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundNormal.isValid() ? getGroup(&closeButtonPalette, active)->foregroundNormal
                                                                                                : Qt::transparent);
            painter->drawRect(oneByThree00);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundHover.isValid() ? getGroup(&closeButtonPalette, active)->foregroundHover
                                                                                               : Qt::transparent);
            painter->drawRect(oneByThree01);
            painter->setBrush(getGroup(&closeButtonPalette, active)->foregroundPress.isValid() ? getGroup(&closeButtonPalette, active)->foregroundPress
                                                                                               : Qt::transparent);
            painter->drawRect(oneByThree02);
        }
        QIcon icon(pixmap);
        uiCloseButtonIconColor->setItemIcon(uiItemIndex, icon);
    }
}

void ButtonColors::getButtonsOrderFromKwinConfig()
{
    QMap<DecorationButtonType, QChar> buttonNames;
    // list modified from https://invent.kde.org/plasma/kwin/-/blob/master/src/decorations/settings.cpp
    buttonNames[DecorationButtonType::Menu] = QChar('M');
    buttonNames[DecorationButtonType::ApplicationMenu] = QChar('N');
    buttonNames[DecorationButtonType::OnAllDesktops] = QChar('S');
    buttonNames[DecorationButtonType::KeepAbove] = QChar('F');
    buttonNames[DecorationButtonType::KeepBelow] = QChar('B');
    buttonNames[DecorationButtonType::Shade] = QChar('L');
    buttonNames[DecorationButtonType::ContextHelp] = QChar('H');
    buttonNames[DecorationButtonType::Minimize] = QChar('I');
    buttonNames[DecorationButtonType::Maximize] = QChar('A');
    buttonNames[DecorationButtonType::Close] = QChar('X');

    QString buttonsOnLeft;
    QString buttonsOnRight;

    // very hacky way to do this -- better would be to find a way to get the settings from <KDecoration3/DecorationSettings>
    //  read kwin button border setting
    KSharedConfig::Ptr kwinConfig = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    if (kwinConfig && kwinConfig->hasGroup(QStringLiteral("org.kde.kdecoration2"))) {
        KConfigGroup kdecoration2Group = kwinConfig->group(QStringLiteral("org.kde.kdecoration2"));

        buttonsOnLeft = kdecoration2Group.readEntry(QStringLiteral("ButtonsOnLeft"), QStringLiteral("MS"));
        buttonsOnRight = kdecoration2Group.readEntry(QStringLiteral("ButtonsOnRight"), QStringLiteral("HIAX"));
    } else {
        buttonsOnLeft = QStringLiteral("MS");
        buttonsOnRight = QStringLiteral("HIAX");
    }

    QString visibleButtons = buttonsOnLeft + buttonsOnRight;

    m_visibleButtonsOrder.clear();
    for (QChar *it = visibleButtons.begin(); it != visibleButtons.end(); it++) {
        auto key = buttonNames.key(*it, DecorationButtonType::Custom);
        if (key != DecorationButtonType::Custom)
            m_visibleButtonsOrder.append(key);
    }

    m_hiddenButtons.clear();
    // add hidden buttons to m_hiddenButtons
    for (auto it = buttonNames.begin(); it != buttonNames.end(); it++) {
        if (!visibleButtons.contains(*it)) {
            m_hiddenButtons.append(buttonNames.key(*it));
        }
    }

    // Place a custom button type in the average position of these "other" button types
    QList<int> otherButtonIndexes{
        static_cast<int>(m_visibleButtonsOrder.indexOf(DecorationButtonType::Menu)),
        static_cast<int>(m_visibleButtonsOrder.indexOf(DecorationButtonType::ApplicationMenu)),
        static_cast<int>(m_visibleButtonsOrder.indexOf(DecorationButtonType::OnAllDesktops)),
        static_cast<int>(m_visibleButtonsOrder.indexOf(DecorationButtonType::ContextHelp)),
        static_cast<int>(m_visibleButtonsOrder.indexOf(DecorationButtonType::KeepAbove)),
        static_cast<int>(m_visibleButtonsOrder.indexOf(DecorationButtonType::KeepBelow)),
        static_cast<int>(m_visibleButtonsOrder.indexOf(DecorationButtonType::Shade)),
    };

    // remove the -1s (index not found)
    QMutableListIterator<int> i(otherButtonIndexes);
    while (i.hasNext()) {
        if (i.next() == -1)
            i.remove();
    }

    int indexOfCustom;
    if (otherButtonIndexes.count()) {
        int sum = 0;
        for (int i = 0; i < otherButtonIndexes.count(); i++) {
            sum += otherButtonIndexes[i] + 1;
        }
        indexOfCustom = (sum / otherButtonIndexes.count()) - 1; // indexOfCustom is now at the median index position of otherButtonIndexes
    } else {
        indexOfCustom = 0;
    }

    // Want to give Close/Maximize/Minimize buttons priority over the custom button to be at either the left or right edges
    QMap<int, DecorationButtonType> leftEdgePriorityButtons; // a list of Close/Maximize/Minimize if at left edge
    QMap<int, DecorationButtonType> rightEdgePriorityButtons; // a list of Close/Maximize/Minimize if at right edge

    // find leftEdgePriorityButtons
    for (int i = 0; i < 3; i++) {
        if (buttonsOnLeft.indexOf(QChar('X')) == i) {
            leftEdgePriorityButtons.insert(i, DecorationButtonType::Close);
        } else if (buttonsOnLeft.indexOf(QChar('A')) == i) {
            leftEdgePriorityButtons.insert(i, DecorationButtonType::Maximize);
        } else if (buttonsOnLeft.indexOf(QChar('I')) == i) {
            leftEdgePriorityButtons.insert(i, DecorationButtonType::Minimize);
        }
    }

    // find rightEdgePrioritybuttons
    for (int i = m_visibleButtonsOrder.count() - 1; i >= m_visibleButtonsOrder.count() - 3; i--) {
        if (buttonsOnRight.lastIndexOf(QChar('X')) == i) { // lastIndexOf in-case a weirdo adds more than one button of the same type
            rightEdgePriorityButtons.insert(i, DecorationButtonType::Close);
        } else if (buttonsOnRight.lastIndexOf(QChar('A')) == i) {
            rightEdgePriorityButtons.insert(i, DecorationButtonType::Maximize);
        } else if (buttonsOnRight.lastIndexOf(QChar('I')) == i) {
            rightEdgePriorityButtons.insert(i, DecorationButtonType::Minimize);
        }
    }

    // if custom is at an edge, make sure priority (min/max/close) button has a priority over custom button for the edge
    if (indexOfCustom >= 0 && indexOfCustom <= (leftEdgePriorityButtons.count() - 1)
        && leftEdgePriorityButtons.count()) { // if custom is to go at start but a leftEdgePriority button is there
        indexOfCustom = m_visibleButtonsOrder.indexOf(leftEdgePriorityButtons.value(leftEdgePriorityButtons.count() - 1)) + 1;
    } else if (indexOfCustom <= (m_visibleButtonsOrder.count() - 1) && indexOfCustom >= (m_visibleButtonsOrder.count() - rightEdgePriorityButtons.count())
               && rightEdgePriorityButtons.count()) { // if custom is to go at end but a right EdgePriority button is there
        indexOfCustom = m_visibleButtonsOrder.indexOf(rightEdgePriorityButtons.value(0));
    }

    m_allCustomizableButtonsOrder = m_visibleButtonsOrder + m_hiddenButtons;

    m_visibleButtonsOrder.insert(indexOfCustom,
                                 DecorationButtonType::Custom); // dummy Custom button inserted for illustrating colour palettes in icons
}

QList<DecorationButtonPalette *> ButtonColors::sortButtonsAsPerKwinConfig(QList<DecorationButtonPalette *> inputlist)
{
    QList<DecorationButtonPalette *> outputlist;

    for (int i = 0; i < m_visibleButtonsOrder.count(); i++) {
        for (int j = inputlist.count() - 1; j >= 0; j--) { // iterate loop in reverse order as want to delete elements
            if (m_visibleButtonsOrder[i] == (inputlist[j])->buttonType()) {
                outputlist.append(inputlist[j]);
                inputlist.removeAt(j);
            }
        }
    }
    return outputlist;
}

void ButtonColors::loadHorizontalHeaderIcons()
{
    for (auto i = 0; i < m_allCustomizableButtonsOrder.count(); i++) { // set the horizontal header icons
        setHorizontalHeaderSectionIcon(m_allCustomizableButtonsOrder[i], m_ui->overrideColorTableActive, i);
        setHorizontalHeaderSectionIcon(m_allCustomizableButtonsOrder[i], m_ui->overrideColorTableInactive, i);
    }
}

void ButtonColors::updateLockIcons()
{
    m_ui->lockButtonColorsActive->setIcon(static_cast<ConfigWidget *>(m_parent)->lockIcon(LockIconState::Bistate));
    m_ui->lockButtonColorsInactive->setIcon(static_cast<ConfigWidget *>(m_parent)->lockIcon(LockIconState::Bistate));

    int numRows = m_overridableButtonColorStatesStrings.count();
    for (int rowIndex = 0; rowIndex < numRows; rowIndex++) {
        auto verticalHeaderItemActive = m_ui->overrideColorTableActive->verticalHeaderItem(rowIndex);
        auto verticalHeaderItemInactive = m_ui->overrideColorTableInactive->verticalHeaderItem(rowIndex);
        if (verticalHeaderItemActive->checkState() == Qt::CheckState::Checked) {
            verticalHeaderItemActive->setIcon(static_cast<ConfigWidget *>(m_parent)->lockIcon(LockIconState::Locked));
        } else {
            verticalHeaderItemActive->setIcon(static_cast<ConfigWidget *>(m_parent)->lockIcon(LockIconState::Unlocked));
        }
        if (verticalHeaderItemInactive->checkState() == Qt::CheckState::Checked) {
            verticalHeaderItemInactive->setIcon(static_cast<ConfigWidget *>(m_parent)->lockIcon(LockIconState::Locked));
        } else {
            verticalHeaderItemInactive->setIcon(static_cast<ConfigWidget *>(m_parent)->lockIcon(LockIconState::Unlocked));
        }
    }
}

void ButtonColors::setHorizontalHeaderSectionIcon(DecorationButtonType type, QTableWidget *table, int section)
{
    QIcon icon;

    if (type == DecorationButtonType::Menu) {
        icon = windowIcon();
    } else {
        bool renderSystemIcon = false;
        QString iconName;
        if (m_internalSettings->buttonIconStyle() == InternalSettings::EnumButtonIconStyle::StyleSystemIconTheme) {
            QString iconNameChecked;
            SystemIconTheme::systemIconNames(type, iconName, iconNameChecked);
            if (!iconName.isEmpty()) {
                renderSystemIcon = true;
            }
        }

        qreal dpr = devicePixelRatioF();
        int scaledSize = qRound(16.0 * dpr);
        QPixmap pixmap(scaledSize, scaledSize);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        std::unique_ptr<QPainter> painter = std::make_unique<QPainter>(&pixmap);
        painter->setPen(QApplication::palette().windowText().color());
        painter->setRenderHints(QPainter::RenderHint::Antialiasing);

        if (renderSystemIcon) {
            SystemIconTheme systemIconRenderer(painter.get(),
                                               16,
                                               iconName,
                                               m_internalSettings,
                                               m_internalSettings->forceColorizeSystemIcons() ? QPalette() : QApplication::palette());
            systemIconRenderer.renderIcon();
        } else {
            auto [iconRenderer, localRenderingWidth](RenderDecorationButtonIcon::factory(m_internalSettings, painter.get(), true, true, dpr));
            iconRenderer->setForceEvenSquares(true);
            painter->setViewport(0, 0, 16, 16);
            painter->setWindow(0, 0, localRenderingWidth, localRenderingWidth);

            QPen pen = painter->pen();
            pen.setWidthF(PenWidth::Symbol * dpr);
            pen.setCosmetic(true);
            painter->setPen(pen);
            iconRenderer->renderIcon(type, false);
        }

        icon = QIcon(pixmap);
    }

    QTableWidgetItem *item = table->horizontalHeaderItem(section);
    if (!item)
        return;
    item->setIcon(icon);
}

bool ButtonColors::event(QEvent *ev)
{
    if (ev->type() == QEvent::ApplicationPaletteChange) {
        // overwrite handling of palette change
        if (!m_loaded)
            load();
        else {
            setSystemTitlebarColors();
            loadButtonPaletteColorsIcons();
            loadHorizontalHeaderIcons();

            DecorationColors decorationColors(false);
            decorationColors.generateDecorationColors(QApplication::palette(),
                                                      m_internalSettings,
                                                      m_systemTitleBarTextActive,
                                                      m_systemTitlebarBackgroundActive,
                                                      m_systemTitleBarTextInactive,
                                                      m_systemTitlebarBackgroundInactive);
            setOverrideComboBoxColorIcons(true, decorationColors);
            setOverrideComboBoxColorIcons(false, decorationColors);
        }
        return QWidget::event(ev);
    }

    // TODO:detect icon change

    // Make sure the rest of events are handled
    return QWidget::event(ev);
}
}
