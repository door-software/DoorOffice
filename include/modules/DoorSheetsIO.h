// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

class QString;

namespace door::modules {

class DoorSheetsWidget;

namespace io {

// File operations for Door Sheets: open/save/export/print.
bool openPath(DoorSheetsWidget* sheet, const QString& path);
QString lastDirectory();
void rememberDirectory(const QString& filePath);

} // namespace io
} // namespace door::modules