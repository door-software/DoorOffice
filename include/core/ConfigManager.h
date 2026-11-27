// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariant>

namespace door::core {

// Resolves where Door Office stores its data.
//  * Portable mode : "portable.txt" next to DoorOffice.exe  -> <exe dir>/data/
//  * Installed mode: %AppData%/DoorOffice/
// Either root contains themes/, plugins/, templates/ and settings.json.
class ConfigManager final
{
public:
    static ConfigManager& instance();

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    [[nodiscard]] bool isPortable() const noexcept { return m_portable; }
    [[nodiscard]] QString modeName() const { return m_portable ? QStringLiteral("Portable") : QStringLiteral("AppData"); }

    [[nodiscard]] const QString& rootPath() const noexcept { return m_root; }
    [[nodiscard]] QString themesPath() const;
    [[nodiscard]] QString pluginsPath() const;
    [[nodiscard]] QString templatesPath() const;
    [[nodiscard]] QString settingsFile() const;

    [[nodiscard]] QVariant value(const QString& key, const QVariant& fallback = {}) const;
    void setValue(const QString& key, const QVariant& value);
    bool save() const;

private:
    ConfigManager();

    void ensureLayout();
    void load();

    bool m_portable = false;
    QString m_root;
    QJsonObject m_settings;
};

} // namespace door::core
