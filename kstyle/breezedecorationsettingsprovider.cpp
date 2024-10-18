/*
 * SPDX-FileCopyrightText: 2014 Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * SPDX-FileCopyrightText: 2022-2024 Paul A McAuley <kde@paulmcauley.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "breezedecorationsettingsprovider.h"
#include "decorationexceptionlist.h"
#include "presetsmodel.h"

#include <QRegularExpression>
#include <QTextStream>

namespace Breeze
{

DecorationSettingsProvider *DecorationSettingsProvider::s_self = nullptr;

//__________________________________________________________________
DecorationSettingsProvider::DecorationSettingsProvider()
    : m_config(KSharedConfig::openConfig(QStringLiteral("klassy/klassyrc")))
    , m_presetsConfig(KSharedConfigPtr())
{
    m_defaultSettings = InternalSettingsPtr(new InternalSettings());
}

//__________________________________________________________________
DecorationSettingsProvider::~DecorationSettingsProvider()
{
    s_self = nullptr;
}

//__________________________________________________________________
DecorationSettingsProvider *DecorationSettingsProvider::self()
{
    // TODO: this is not thread safe!
    if (!s_self) {
        s_self = new DecorationSettingsProvider();
    }

    return s_self;
}

//__________________________________________________________________
void DecorationSettingsProvider::reconfigure()
{
    m_defaultSettings->load();

    DecorationExceptionList exceptions;
    exceptions.readConfig(m_config);
    m_exceptions = exceptions.getDefault();
    m_exceptions.append(exceptions.get());
}

//__________________________________________________________________
InternalSettingsPtr DecorationSettingsProvider::internalSettings()
{
    for (auto internalSettings : std::as_const(m_exceptions)) {
        // discard disabled exceptions
        if (!internalSettings->enabled())
            continue;

        // discard exceptions with empty exception pattern
        if (internalSettings->exceptionProgramNamePattern().isEmpty())
            continue;

        // check matching
        QRegularExpression rx(internalSettings->exceptionProgramNamePattern());
        if (rx.match(qAppName()).hasMatch()) {
            // load window decoration preset if set
            if (!internalSettings->exceptionPreset().isEmpty()) {
                if (!m_presetsConfig) {
                    KSharedConfigPtr presetsConfig = KSharedConfig::openConfig(QStringLiteral("klassy/windecopresetsrc"));
                    m_presetsConfig.swap(presetsConfig);
                }
                if (!m_presetsConfig) {
                    return internalSettings;
                }

                PresetsModel::loadPreset(internalSettings.data(), m_presetsConfig.data(), internalSettings->exceptionPreset());
                internalSettings->setProperty("noCacheException",
                                              true); // this property is to indicate not to cache shadows or colours for an exception with a Preset
                                                     // -- this is because the Preset exception can alter shadows and colours
            }
            if (internalSettings->opaqueTitleBar()) {
                internalSettings->setProperty("noCacheException", true);
            }
            return internalSettings;
        }
    }

    return m_defaultSettings;
}
}
