// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace door::core {

namespace {

QString appDataRoot()
{
    // %AppData% (Roaming) on Windows; sensible fallback elsewhere.
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(base).filePath(QStringLiteral("DoorOffice"));
}

QJsonObject defaultSettings()
{
    return QJsonObject{
        {QStringLiteral("version"), QStringLiteral(DOOR_OFFICE_VERSION)},
        {QStringLiteral("theme"), QStringLiteral("Door Midnight Dark")},
        {QStringLiteral("defaultFontFamily"), QStringLiteral("Segoe UI")},
        {QStringLiteral("defaultFontSize"), 12},
        {QStringLiteral("lastDirectory"), QString()},
    };
}

} // namespace

ConfigManager& ConfigManager::instance()
{
    static ConfigManager manager;
    return manager;
}

ConfigManager::ConfigManager()
{
    const QDir exeDir(QCoreApplication::applicationDirPath());
    m_portable = QFileInfo::exists(exeDir.filePath(QStringLiteral("portable.txt")));
    m_root = m_portable ? exeDir.filePath(QStringLiteral("data")) : appDataRoot();

    ensureLayout();
    load();
}

QString ConfigManager::themesPath() const    { return QDir(m_root).filePath(QStringLiteral("themes")); }
QString ConfigManager::pluginsPath() const   { return QDir(m_root).filePath(QStringLiteral("plugins")); }
QString ConfigManager::templatesPath() const { return QDir(m_root).filePath(QStringLiteral("templates")); }
QString ConfigManager::settingsFile() const  { return QDir(m_root).filePath(QStringLiteral("settings.json")); }

void ConfigManager::ensureLayout()
{
    QDir().mkpath(m_root);
    for (const QString& dir : {themesPath(), pluginsPath(), templatesPath()})
        QDir().mkpath(dir);
}

void ConfigManager::load()
{
    m_settings = defaultSettings();

    QFile file(settingsFile());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            const QJsonObject stored = doc.object();
            for (auto it = stored.begin(); it != stored.end(); ++it)
                m_settings.insert(it.key(), it.value());
        }
    } else {
        save(); // first run: write defaults
    }
}

QVariant ConfigManager::value(const QString& key, const QVariant& fallback) const
{
    const auto it = m_settings.constFind(key);
    return it != m_settings.constEnd() ? it->toVariant() : fallback;
}

void ConfigManager::setValue(const QString& key, const QVariant& value)
{
    m_settings.insert(key, QJsonValue::fromVariant(value));
}

bool ConfigManager::save() const
{
    QSaveFile file(settingsFile());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(m_settings).toJson(QJsonDocument::Indented));
    return file.commit();
}

} // namespace door::core
