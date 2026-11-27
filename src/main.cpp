// Door Office v1.0 — Opens the door to a new world of technology
// Copyright (C) 2026 Door Office contributors
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details. You should have received a copy of the GNU General
// Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/ConfigManager.h"
#include "core/MainWindow.h"

#include <QApplication>
#include <QStringList>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DoorOffice"));
    QApplication::setApplicationVersion(QStringLiteral(DOOR_OFFICE_VERSION));
    QApplication::setOrganizationName(QStringLiteral("DoorOffice"));

    // Resolve portable vs. %AppData% storage before any UI touches settings.
    (void)door::core::ConfigManager::instance();

    door::core::MainWindow window;
    window.show();

    // Files passed on the command line (e.g. "Open with…") open in Door Write tabs.
    const QStringList args = QApplication::arguments().mid(1);
    for (const QString& path : args)
        window.openPath(path);

    return QApplication::exec();
}
