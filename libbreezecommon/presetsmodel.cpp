/*
 * SPDX-FileCopyrightText: 2023-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "presetsmodel.h"
#include <KConfigGroup>
#include <QDir>
#include <QRegularExpression>

namespace Breeze
{

QString PresetsModel::presetGroupName(const QString str)
{
    return QString("Windeco Preset %1").arg(str);
}

//______________________________________________________________
void PresetsModel::writePreset(KCoreConfigSkeleton *skeleton, KConfig *presetsConfig, const QString &presetName)
{
    QString groupName = presetGroupName(presetName);

    // write window decoration configuration as a preset
    for (auto item : skeleton->items()) {
        if (item->group() == QStringLiteral("Exceptions") || item->group() == QStringLiteral("Global"))
            continue;

        KConfigGroup configGroup(presetsConfig, groupName);
        writeSkeletonItemToConfigGroup(item, configGroup);
    }

    // read kwin window border setting and write to the preset
    KSharedConfig::Ptr kwinConfig = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    if (!kwinConfig)
        return;
    if (kwinConfig->hasGroup(QStringLiteral("org.kde.kdecoration2"))) {
        KConfigGroup kdecoration2Group = kwinConfig->group(QStringLiteral("org.kde.kdecoration2"));
        QString borderSize;
        if (!kdecoration2Group.hasKey(QStringLiteral("BorderSize"))) {
            borderSize = QStringLiteral("Normal"); // Normal is the KWin default, so will nor write a BorderSize key in this case
        } else {
            borderSize = kdecoration2Group.readEntry(QStringLiteral("BorderSize"));
        }
        if (!groupName.isEmpty()) {
            KConfigGroup configGroup(presetsConfig, groupName);
            configGroup.writeEntry(QStringLiteral("KwinBorderSize"), borderSize);
        }
    }
}

void PresetsModel::writeSkeletonItemToConfigGroup(KConfigSkeletonItem *item, KConfigGroup &configGroup)
{
    // enum properties are ints, but it is more robust to write the full string name to the file, rather than an int
    // therefore if an enum get the name instead
    if (auto enumItem = dynamic_cast<KCoreConfigSkeleton::ItemEnum *>(item)) { // if the item is an enum
        if (item->property().toInt() >= 0) { // invalid enum values are set to -1
            configGroup.writeEntry(item->key(), enumItem->choices()[item->property().toInt()].name);
        }
    } else if (auto intListItem =
                   dynamic_cast<KCoreConfigSkeleton::ItemIntList *>(item)) { // if the item is an IntList, need to loop through each element in list and write
        QVariant property = intListItem->property();
        QList<int> *list = static_cast<QList<int> *>(property.data());

        QString intListString;
        int i = 0;
        for (int colorInt : *list) {
            QTextStream(&intListString) << colorInt;
            if (i < (list->count() - 1))
                intListString += ",";
            i++;
        }
        if (!intListString.isNull())
            configGroup.writeEntry(item->key(), intListString);
    } else {
        configGroup.writeEntry(item->key(), item->property());
    }
}

bool PresetsModel::loadPreset(KCoreConfigSkeleton *skeleton, KConfig *presetsConfig, const QString &presetName, bool writeKwinBorderConfig)
{
    QString groupName = presetGroupName(presetName);

    if (groupName.isEmpty() || !presetsConfig->hasGroup(groupName))
        return false;

    for (KConfigSkeletonItem *item : skeleton->items()) {
        QString originalGroup = item->group();
        if (originalGroup == QStringLiteral("Exceptions") || originalGroup == QStringLiteral("Global")) {
            continue;
        }
        item->setGroup(groupName);
        item->readConfig(presetsConfig);
        item->setGroup(originalGroup);
    }

    // writes the value of KwinBorderSize from the preset into the kwinrc file
    if (writeKwinBorderConfig) {
        KConfigGroup configGroup = presetsConfig->group(groupName);
        if (configGroup.hasKey(QStringLiteral("KwinBorderSize"))) {
            writeBorderSizeToKwinConfig(configGroup.readEntry(QStringLiteral("KwinBorderSize")));
        }
    }

    return true;
}

bool PresetsModel::loadPresetAndSave(KCoreConfigSkeleton *skeleton,
                                     KConfig *mainConfig,
                                     KConfig *presetsConfig,
                                     const QString &presetName,
                                     bool writeKwinBorderConfig)
{
    if (!loadPreset(skeleton, presetsConfig, presetName, writeKwinBorderConfig)) {
        return false;
    }

    for (KConfigSkeletonItem *item : skeleton->items()) {
        KConfigGroup mainConfigGroup = mainConfig->group(item->group());
        if (item->isDefault()) { // written defaults should be blank
            if (mainConfigGroup.hasKey(item->key())) {
                mainConfigGroup.deleteEntry(item->key());
            }
        } else {
            writeSkeletonItemToConfigGroup(item, mainConfigGroup);
        }
    }
    mainConfig->sync();
    return true;
}

void PresetsModel::copyKwinBorderSizeFromPresetToExceptionBorderSize(KCoreConfigSkeleton *skeleton, KConfig *presetsConfig, const QString &presetName)
{
    QString groupName = presetGroupName(presetName);

    if (groupName.isEmpty() || !presetsConfig->hasGroup(groupName))
        return;

    KCoreConfigSkeleton::ItemEnum *borderSize = static_cast<KCoreConfigSkeleton::ItemEnum *>(skeleton->findItem("BorderSize"));
    KConfigGroup configGroup = presetsConfig->group(groupName);
    if (configGroup.hasKey(QStringLiteral("KwinBorderSize"))) {
        auto choiceList = borderSize->choices();
        int borderValue = -1;
        for (int i = 0; i < choiceList.count(); i++) { // need to convert the string value of the enum value to an int for compatibility
            if (choiceList[i].name == configGroup.readEntry(QStringLiteral("KwinBorderSize")))
                borderValue = i;
        }
        if (borderValue == -1)
            return;
        borderSize->setProperty(borderValue);
    }
}

bool PresetsModel::presetHasKwinBorderSizeKey(KConfig *presetsConfig, const QString &presetName)
{
    QString groupName = presetGroupName(presetName);

    if (groupName.isEmpty() || !presetsConfig->hasGroup(groupName))
        return false;
    KConfigGroup configGroup = presetsConfig->group(groupName);
    if (configGroup.hasKey(QStringLiteral("KwinBorderSize")))
        return true;
    else
        return false;
}

void PresetsModel::writeBorderSizeToKwinConfig(const QString &borderSize)
{
    KSharedConfig::Ptr kwinConfig = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    if (kwinConfig) {
        KConfigGroup kdecoration2Group = kwinConfig->group(QStringLiteral("org.kde.kdecoration2"));

        // this is when "Theme's Default" is selected for the border size - if this is true then kwin will ignore the "BorderSize" key
        kdecoration2Group.writeEntry(QStringLiteral("BorderSizeAuto"), QStringLiteral("false"));

        kdecoration2Group.writeEntry(QStringLiteral("BorderSize"), borderSize);
        kwinConfig->sync();
    }
}

void PresetsModel::deletePreset(KConfig *presetsConfig, const QString &presetName)
{
    QString groupName = presetGroupName(presetName);

    if (presetsConfig->hasGroup(groupName))
        presetsConfig->deleteGroup(groupName);
}

void PresetsModel::deleteBundledPresets(KConfig *presetsConfig)
{
    QStringList presetList = readPresetsList(presetsConfig);
    for (const QString &presetName : presetList) {
        QString groupName = presetGroupName(presetName);
        if (presetsConfig->hasGroup(groupName)) {
            KConfigGroup presetGroup = presetsConfig->group(groupName);
            if (presetGroup.hasKey("BundledPreset")) {
                if (presetGroup.readEntry("BundledPreset") == "true") {
                    presetsConfig->deleteGroup(groupName);
                }
            }
        }
    }
}

QStringList PresetsModel::readPresetsList(KConfig *presetsConfig)
{
    QStringList presetsList;
    QRegularExpression re("^Windeco Preset (.+)");
    for (const QString &group : presetsConfig->groupList()) {
        QRegularExpressionMatch match = re.match(group);
        if (match.hasMatch()) {
            QString presetName = match.captured(1);
            presetsList.append(presetName);
        }
    }
    return presetsList;
}

bool PresetsModel::isPresetPresent(KConfig *presetsConfig, const QString &presetName)
{
    QStringList list = readPresetsList(presetsConfig);
    return list.contains(presetName);
}

bool PresetsModel::isPresetFromFilePresent(KConfig *presetsConfig, const QString &presetFileName, QString &presetName)
{
    KSharedConfig::Ptr importPresetConfig = KSharedConfig::openConfig(presetFileName);
    if (!importPresetConfig) {
        return false;
    }

    QStringList list = readPresetsList(importPresetConfig.data());
    if (!list.count())
        return false;
    presetName = list[0];
    return isPresetPresent(presetsConfig, presetName);
}

void PresetsModel::exportPreset(KConfig *presetsConfig, const QString &presetName, const QString &filePath)
{
    if (presetName.isEmpty() || filePath.isEmpty())
        return;

    KSharedConfig::Ptr outputPresetConfig = KSharedConfig::openConfig(filePath);
    QString groupName = presetGroupName(presetName);

    if (!outputPresetConfig)
        return;

    if (groupName.isEmpty() || !presetsConfig->hasGroup(groupName))
        return;

    KConfigGroup inputPresetGroup = presetsConfig->group(groupName);
    KConfigGroup outputGlobalGroup = outputPresetConfig->group("Klassy Window Decoration Preset File");
    KConfigGroup outputPresetGroup = outputPresetConfig->group(groupName);

    outputGlobalGroup.writeEntry("version", klassyLongVersion());

    auto internalSettings = InternalSettingsPtr(new InternalSettings());

    for (const QString &inputKey : inputPresetGroup.keyList()) {
        auto item = internalSettings->findItem(inputKey);
        if (!item)
            continue;

        QString exportProperty = inputPresetGroup.readEntry(inputKey);
        outputPresetGroup.writeEntry(inputKey, exportProperty);
    }
    if (inputPresetGroup.hasKey("KwinBorderSize"))
        outputPresetGroup.writeEntry("KwinBorderSize", inputPresetGroup.readEntry("KwinBorderSize"));

    outputPresetConfig->sync();
}

PresetsErrorFlag
PresetsModel::importPreset(KConfig *presetsConfig, const QString &filePath, QString &presetName, QString &error, bool forceInvalidVersion, bool markAsBundled)
{
    KSharedConfig::Ptr importPresetConfig = KSharedConfig::openConfig(filePath);

    if (!importPresetConfig)
        return PresetsErrorFlag::InvalidGlobalGroup;

    // perform validation first
    if (!(importPresetConfig->hasGroup("Klassy Window Decoration Preset File")))
        return PresetsErrorFlag::InvalidGlobalGroup;
    KConfigGroup importGlobalGroup = importPresetConfig->group("Klassy Window Decoration Preset File");
    QString importVersion = importGlobalGroup.readEntry("version");
    bool versionValid = (importVersion == klassyLongVersion());
    if (!versionValid && !forceInvalidVersion)
        return PresetsErrorFlag::InvalidVersion;

    QStringList presetsList = readPresetsList(importPresetConfig.data());
    if (presetsList.count())
        presetName = presetsList[0];
    else {
        return PresetsErrorFlag::InvalidGroup;
    }

    QString importGroupName = presetGroupName(presetName);

    KConfigGroup importGroup = importPresetConfig->group(importGroupName);

    for (const QString &importKey : importGroup.keyList()) {
        if (!isKeyValid(importKey)) {
            error = importKey;
            return PresetsErrorFlag::InvalidKey;
        }
    }

    // end of validation

    // delete an existing preset if has the same name
    if (isPresetPresent(presetsConfig, presetName)) {
        deletePreset(presetsConfig, presetName);
    }

    // start writing the values
    auto internalSettings = InternalSettingsPtr(new InternalSettings());
    KConfigGroup configGroup(presetsConfig, importGroupName);

    for (const QString &importKey : importGroup.keyList()) {
        QString importProperty = importGroup.readEntry(importKey);
        configGroup.writeEntry(importKey, importProperty);
    }

    if (markAsBundled)
        configGroup.writeEntry("BundledPreset", "true");

    return PresetsErrorFlag::None;
}

bool PresetsModel::isKeyValid(const QString &key)
{
    auto internalSettings = InternalSettingsPtr(new InternalSettings());

    for (const auto &item : internalSettings->items()) {
        if (item->key() == key) {
            return true;
        }
    }

    if (key == "KwinBorderSize")
        return true; // additional valid key containing KWin border size setting from kwinrc

    return false;
}

// copies bundled presets in /usr/lib64/qt6/plugins/org.kde.kdecoration2.kcm/klassydecoration/presets into ~/.config/klassy/klassyrc once per release
void PresetsModel::importBundledPresets(KConfig *presetsConfig)
{
    // don't copy if BundledWindecoPresetsImportedVersion has been set for the current release version
    if (presetsConfig->hasGroup("Global")) {
        KConfigGroup globalGroup = presetsConfig->group("Global");
        if (globalGroup.hasKey("BundledWindecoPresetsImportedVersion")) {
            if (globalGroup.readEntry("BundledWindecoPresetsImportedVersion") == klassyLongVersion()) {
                return;
            }
        }
    }

    // qDebug() << "librarypaths: " << QCoreApplication::libraryPaths(); //librarypaths:  ("/usr/lib64/qt6/plugins", "/usr/bin")

    // delete bundled presets from a previous release first
    // if the user modified the preset it will not contain the BundledPreset flag and hence won't be deleted
    PresetsModel::deleteBundledPresets(presetsConfig);

    for (QString libraryPath : QCoreApplication::libraryPaths()) {
        libraryPath += "/org.kde.kdecoration2.kcm/klassydecoration/presets";
        QDir presetsDir(libraryPath);
        if (presetsDir.exists()) {
            QStringList filters;
            filters << "*.klpw";
            presetsDir.setNameFilters(filters);
            QStringList presetFiles = presetsDir.entryList();

            for (QString presetFile : presetFiles) {
                presetFile = libraryPath + "/" + presetFile; // set absolute full path
                QString presetName;
                QString error;

                PresetsErrorFlag importErrors = PresetsModel::importPreset(presetsConfig, presetFile, presetName, error, false, true);
                if (importErrors != PresetsErrorFlag::None) {
                    continue;
                }
            }
        }
    }

    KConfigGroup globalGroup = presetsConfig->group("Global");
    globalGroup.writeEntry("BundledWindecoPresetsImportedVersion", klassyLongVersion());
    presetsConfig->sync();
}
}
