// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "modules/FormulaEngine.h"

#include <QJsonObject>
#include <QPagedPaintDevice>
#include <QRect>
#include <QWidget>

#include <functional>

class QAction;
class QLabel;
class QLineEdit;
class QPrinter;
class QTableWidget;
class QTableWidgetItem;
class QToolBar;

namespace door::modules {

// Forward declaration so the 'io' namespace can reference DoorSheetsWidget
class DoorSheetsWidget;

namespace io {

bool openPath(DoorSheetsWidget* sheet, const QString& path);

} // namespace io

// Door Sheets — the spreadsheet module (.dsheet JSON, .csv).
class DoorSheetsWidget final : public QWidget
{
    friend bool door::modules::io::openPath(DoorSheetsWidget* sheet, const QString& path);

    Q_OBJECT

public:
    explicit DoorSheetsWidget(QWidget* parent = nullptr);

    [[nodiscard]] QTableWidget* table() const noexcept { return m_table; }
    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool isModified() const noexcept { return m_modified; }

    static QString fileFilter();

public slots:
    bool openFile(const QString& path);
    bool save();
    bool saveAs();
    bool exportPdf();
    void print();
    bool maybeSave(); // returns false if the user cancelled

signals:
    void titleChanged(const QString& title);
    void statusMessage(const QString& message, int timeoutMs = 3000);

private:
    void buildToolBar();
    void labelHeaders();
    void prepare(int rows, int cols);
    QTableWidgetItem* ensureItem(int row, int col);
    void forEachSelected(bool create, const std::function<void(QTableWidgetItem*)>& fn);
    void setAlignment(Qt::Alignment alignment);
    void refreshFormulaBar();
    void setModified(bool modified);

    [[nodiscard]] QRect usedRange() const;
    [[nodiscard]] QByteArray toCsv() const;
    [[nodiscard]] QJsonObject toJson() const;
    bool readCsv(const QString& text);
    bool readJson(const QByteArray& data);
    bool writeFile(const QString& path);
    bool writeXlsx(const QString& path);
    void renderTo(QPagedPaintDevice* printer);

    QToolBar* m_toolBar = nullptr;
    QTableWidget* m_table; // created in the constructor; must precede m_engine
    FormulaEngine m_engine;
    QLabel* m_nameLabel = nullptr;
    QLineEdit* m_formula = nullptr;
    QAction* m_actBold = nullptr;

    QString m_filePath;
    bool m_modified = false;
    bool m_busy = false; // suppresses itemChanged handling during programmatic updates
};

} // namespace door::modules
