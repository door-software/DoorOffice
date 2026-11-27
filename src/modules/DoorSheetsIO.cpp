// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
// Door Sheets file operations, printing and PDF export.
#include "modules/DoorSheetsIO.h"
#include "modules/DoorSheetsWidget.h"

#include "core/ConfigManager.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPrintDialog>
#include <QPrinter>
#include <QSaveFile>
#include <QTableWidget>
#include <QTextDocument>

namespace door {
namespace modules {
namespace io {

QString lastDirectory() { return core::ConfigManager::instance().value(QStringLiteral("lastDirectory")).toString(); }

void rememberDirectory(const QString& filePath)
{
    auto& cfg = core::ConfigManager::instance();
    cfg.setValue(QStringLiteral("lastDirectory"), QFileInfo(filePath).absolutePath());
    cfg.save();
}

bool openPath(DoorSheetsWidget* sheet, const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(sheet, sheet->tr("Door Sheets"),
                             sheet->tr("Cannot open \"%1\":\n%2").arg(QDir::toNativeSeparators(path), file.errorString()));
        return false;
    }
    const QByteArray data = file.readAll();
    const bool ok = path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive) ? 
                    sheet->readCsv(QString::fromUtf8(data)) : 
                    sheet->readJson(data);
    if (!ok) {
        QMessageBox::warning(sheet, sheet->tr("Door Sheets"),
                             sheet->tr("\"%1\" is not a valid Door Sheets file.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    sheet->m_filePath = path;
    rememberDirectory(path);
    sheet->m_table->setCurrentCell(0, 0);
    sheet->refreshFormulaBar();
    sheet->setModified(false);
    emit sheet->statusMessage(sheet->tr("Opened %1").arg(QDir::toNativeSeparators(path)), 3000);
    return true;
}

} // namespace io

namespace {

bool isCsv(const QString& path)
{
    return QFileInfo(path).suffix().compare(QLatin1String("csv"), Qt::CaseInsensitive) == 0;
}

} // namespace

bool DoorSheetsWidget::openFile(const QString& path)
{
    return io::openPath(this, path);
}

} // namespace modules
} // namespace door

