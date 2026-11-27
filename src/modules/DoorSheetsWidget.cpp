// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
// Door Sheets UI. File I/O, printing and PDF export live in DoorSheetsFile.cpp.
#include "modules/DoorSheetsWidget.h"

#include "modules/DoorSheetsIO.h"

#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPageLayout>
#include <QPageSize>
#include <QPrinter>
#include <QPrintDialog>
#include <QTableWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>

#include <QMessageBox>
#include <QJsonDocument>
#include <QSaveFile>
#include <QXmlStreamWriter>
#include <QProcess>
#include <QRandomGenerator>
#include <QDateTime>
#include <QDateTime>

namespace door::modules {

namespace {

constexpr int kRows = 100, kCols = 26, kMaxRows = 10000, kMaxCols = 702;

bool isCsv(const QString& path) {
    return QFileInfo(path).suffix().compare(QLatin1String("csv"), Qt::CaseInsensitive) == 0;
}

// Stores the raw input ("text" or "=formula") but displays the evaluated result.
class SheetItem final : public QTableWidgetItem
{
public:
    explicit SheetItem(const FormulaEngine* engine) : m_engine(engine) {}
    QTableWidgetItem* clone() const override { return new SheetItem(*this); }
    QVariant data(int role) const override
    {
        if (role != Qt::DisplayRole) return QTableWidgetItem::data(role);
        return m_engine->display(QTableWidgetItem::data(Qt::EditRole).toString());
    }

private:
    const FormulaEngine* m_engine;
};

} // namespace

DoorSheetsWidget::DoorSheetsWidget(QWidget* parent)
    : QWidget(parent), m_table(new QTableWidget(kRows, kCols, this)), m_engine(m_table)
{
    m_table->setObjectName(QStringLiteral("DoorSheetsTable"));
    m_table->setItemPrototype(new SheetItem(&m_engine)); // the table takes ownership
    m_table->horizontalHeader()->setDefaultSectionSize(96);
    labelHeaders();
    buildToolBar();

    m_nameLabel = new QLabel(QStringLiteral("A1"), this);
    m_nameLabel->setMinimumWidth(56);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_formula = new QLineEdit(this);
    m_formula->setPlaceholderText(tr("Enter a value or a formula, e.g. =SUM(A1:A10)"));
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(8, 4, 8, 4);
    bar->addWidget(m_nameLabel);
    bar->addWidget(new QLabel(QStringLiteral("fx"), this));
    bar->addWidget(m_formula, 1);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_toolBar);
    layout->addLayout(bar);
    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::currentCellChanged, this, [this] { refreshFormulaBar(); });
    connect(m_table, &QTableWidget::itemChanged, this, [this] {
        if (!m_busy && !m_modified) setModified(true);
        m_table->viewport()->update(); // dependent formulas re-evaluate on repaint
        if (!m_busy) refreshFormulaBar();
    });
    connect(m_formula, &QLineEdit::returnPressed, this, [this] {
        if (m_table->currentRow() < 0) return;
        ensureItem(m_table->currentRow(), m_table->currentColumn())->setData(Qt::EditRole, m_formula->text());
        m_table->setFocus();
    });

    auto* del = new QAction(this); // Delete clears the selection
    del->setShortcut(QKeySequence::Delete);
    del->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(del, &QAction::triggered, this, [this] {
        forEachSelected(false, [](QTableWidgetItem* it) { it->setData(Qt::EditRole, QString()); });
    });
    addAction(del);
    m_table->setCurrentCell(0, 0);
    refreshFormulaBar();
}

void DoorSheetsWidget::buildToolBar()
{
    m_toolBar = new QToolBar(tr("Formatting"), this);
    m_actBold = m_toolBar->addAction(tr("Bold"));
    m_actBold->setToolTip(tr("Bold (Ctrl+B)"));
    m_actBold->setShortcut(QKeySequence::Bold);
    m_actBold->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_actBold->setCheckable(true);
    connect(m_actBold, &QAction::triggered, this, [this](bool on) {
        forEachSelected(true, [on](QTableWidgetItem* it) { QFont f = it->font(); f.setBold(on); it->setFont(f); });
    });
    m_toolBar->addSeparator();
    const auto align = [this](const QString& text, Qt::Alignment a) {
        QAction* act = m_toolBar->addAction(text);
        act->setToolTip(tr("Align %1").arg(text.toLower()));
        connect(act, &QAction::triggered, this, [this, a] { setAlignment(a); });
    };
    align(tr("Left"), Qt::AlignLeft);
    align(tr("Center"), Qt::AlignHCenter);
    align(tr("Right"), Qt::AlignRight);
}

void DoorSheetsWidget::labelHeaders()
{
    QStringList labels;
    for (int c = 0; c < m_table->columnCount(); ++c) labels << FormulaEngine::columnName(c);
    m_table->setHorizontalHeaderLabels(labels);
}

void DoorSheetsWidget::prepare(int rows, int cols)
{
    m_table->clear();
    m_table->setRowCount(std::clamp(rows, kRows, kMaxRows));
    m_table->setColumnCount(std::clamp(cols, kCols, kMaxCols));
    labelHeaders();
}

QTableWidgetItem* DoorSheetsWidget::ensureItem(int row, int col)
{
    QTableWidgetItem* it = m_table->item(row, col);
    if (!it) m_table->setItem(row, col, it = new SheetItem(&m_engine));
    return it;
}

void DoorSheetsWidget::forEachSelected(bool create, const std::function<void(QTableWidgetItem*)>& fn)
{
    auto ranges = m_table->selectedRanges();
    const int cr = m_table->currentRow(), cc = m_table->currentColumn();
    if (ranges.isEmpty() && cr >= 0) ranges.append(QTableWidgetSelectionRange(cr, cc, cr, cc));
    for (const auto& rg : std::as_const(ranges))
        for (int r = rg.topRow(); r <= rg.bottomRow(); ++r)
            for (int c = rg.leftColumn(); c <= rg.rightColumn(); ++c)
                if (QTableWidgetItem* it = create ? ensureItem(r, c) : m_table->item(r, c)) fn(it);
}

void DoorSheetsWidget::setAlignment(Qt::Alignment alignment)
{
    const int value = int(alignment | Qt::AlignVCenter);
    forEachSelected(true, [value](QTableWidgetItem* it) { it->setData(Qt::TextAlignmentRole, value); });
}

void DoorSheetsWidget::refreshFormulaBar()
{
    const int r = m_table->currentRow(), c = m_table->currentColumn();
    if (r < 0 || c < 0) return;
    const QTableWidgetItem* it = m_table->item(r, c);
    m_nameLabel->setText(FormulaEngine::cellName(r, c));
    m_formula->setText(it ? it->data(Qt::EditRole).toString() : QString());
    m_actBold->setChecked(it && it->font().bold());
}

void DoorSheetsWidget::setModified(bool modified)
{
    m_modified = modified;
    emit titleChanged(displayName());
}

QString DoorSheetsWidget::displayName() const
{
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    return m_modified ? name + QStringLiteral(" •") : name;
}

bool DoorSheetsWidget::writeFile(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("xlsx")) {
        return writeXlsx(path);
    }
    const QByteArray bytes = isCsv(path) ? toCsv() : QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        QMessageBox::warning(this, tr("Door Sheets"),
                             tr("Cannot save \"%1\":\n%2").arg(QDir::toNativeSeparators(path), file.errorString()));
        return false;
    }
    m_filePath = path;
    io::rememberDirectory(path);
    setModified(false);
    emit statusMessage(tr("Saved %1").arg(QDir::toNativeSeparators(path)), 3000);
    return true;
}

bool DoorSheetsWidget::save() { return m_filePath.isEmpty() ? saveAs() : writeFile(m_filePath); }

bool DoorSheetsWidget::saveAs()
{
    QString selected;
    const QString start = m_filePath.isEmpty() ? QDir(io::lastDirectory()).filePath(tr("Untitled.dsheet")) : m_filePath;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Spreadsheet As"), start, fileFilter(), &selected);
    if (path.isEmpty()) return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += selected.contains(QLatin1String("*.csv")) ? QStringLiteral(".csv") : QStringLiteral(".dsheet");
    return writeFile(path);
}

bool DoorSheetsWidget::maybeSave()
{
    if (!m_modified) return true;
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    const auto answer = QMessageBox::question(this, tr("Door Sheets"),
        tr("\"%1\" has unsaved changes.\nDo you want to save them?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save) return save();
    return answer == QMessageBox::Discard;
}

QString DoorSheetsWidget::fileFilter()
{
    return tr("Microsoft Excel (*.xlsx);;Door Sheets (*.dsheet);;CSV Files (*.csv);;All Files (*)");
}


// Additional methods for printing and PDF export
void DoorSheetsWidget::renderTo(QPagedPaintDevice* printer)
{
    // Implementation for rendering to printer
    QTextDocument doc;
    QString html = "<html><head><style>";
    html += "table { border-collapse: collapse; }";
    html += "td { border: 1px solid #000; padding: 2px; }";
    html += "</style></head><body>";
    
    // Convert table content to HTML for printing
    html += "<table>";
    const QRect used = usedRange();
    for (int r = 0; r < used.height(); ++r) {
        html += "<tr>";
        for (int c = 0; c < used.width(); ++c) {
            const QTableWidgetItem* it = m_table->item(r, c);
            QString text = it ? it->data(Qt::EditRole).toString() : QString();
            html += "<td>" + text.toHtmlEscaped() + "</td>";
        }
        html += "</tr>";
    }
    html += "</table></body></html>";
    
    doc.setHtml(html);
    doc.print(printer);
}

bool DoorSheetsWidget::exportPdf()
{
    QString base = QDir(io::lastDirectory()).filePath(tr("Untitled"));
    if (!m_filePath.isEmpty()) {
        const QFileInfo info(m_filePath);
        base = info.dir().filePath(info.completeBaseName());
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Export to PDF"), base + QStringLiteral(".pdf"),
                                                tr("PDF Files (*.pdf)"));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".pdf");

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    printer.setCreator(QStringLiteral("Door Office " DOOR_OFFICE_VERSION));
    printer.setDocName(QFileInfo(path).completeBaseName());
    renderTo(&printer);

    emit statusMessage(tr("Exported PDF to %1").arg(QDir::toNativeSeparators(path)), 5000);
    return true;
}

void DoorSheetsWidget::print()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName());

    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Spreadsheet"));
    if (dialog.exec() == QDialog::Accepted) {
        renderTo(&printer);
        emit statusMessage(tr("Spreadsheet sent to printer"));
    }
}

bool DoorSheetsWidget::writeXlsx(const QString& path)
{
    const QString tempDir = QDir::tempPath() + QLatin1String("/door_xlsx_") + QDateTime::currentDateTime().toString(QLatin1String("yyyyMMdd_HHmmss"));
    const QString xlDir = tempDir + QLatin1String("/xl/worksheets");
    const QString xlRelsDir = tempDir + QLatin1String("/xl/_rels");
    QDir().mkpath(xlDir);
    QDir().mkpath(xlRelsDir);

    QFile sharedStrings(xlDir + QLatin1String("/../sharedStrings.xml"));
    sharedStrings.open(QIODevice::WriteOnly);
    QXmlStreamWriter sharedWriter(&sharedStrings);
    sharedWriter.writeStartDocument();
    sharedWriter.writeStartElement(QLatin1String("s:sharedStrings"));
    sharedWriter.writeEndElement();
    sharedWriter.writeEndDocument();
    sharedStrings.close();

    QFile sheetXml(xlDir + QLatin1String("/sheet1.xml"));
    sheetXml.open(QIODevice::WriteOnly);
    QXmlStreamWriter sheetWriter(&sheetXml);
    sheetWriter.writeStartDocument();
    sheetWriter.writeStartElement(QLatin1String("x:worksheet"));
    sheetWriter.writeEndElement();
    sheetWriter.writeEndDocument();
    sheetXml.close();

    QFile wbXml(tempDir + QLatin1String("/xl/workbook.xml"));
    wbXml.open(QIODevice::WriteOnly);
    QXmlStreamWriter wbWriter(&wbXml);
    wbWriter.writeStartDocument();
    wbWriter.writeStartElement(QLatin1String("x:workbook"));
    wbWriter.writeStartElement(QLatin1String("x:sheets"));
    wbWriter.writeStartElement(QLatin1String("x:sheet"));
    wbWriter.writeAttribute(QLatin1String("name"), QLatin1String("Sheet1"));
    wbWriter.writeAttribute(QLatin1String("sheetId"), QLatin1String("1"));
    wbWriter.writeAttribute(QLatin1String("r:id"), QLatin1String("rId1"));
    wbWriter.writeEndElement();
    wbWriter.writeEndElement();
    wbWriter.writeEndDocument();
    wbXml.close();

    QFile wbRels(xlRelsDir + QLatin1String("/workbook.xml.rels"));
    wbRels.open(QIODevice::WriteOnly);
    QXmlStreamWriter wbRelsWriter(&wbRels);
    wbRelsWriter.writeStartDocument();
    wbRelsWriter.writeStartElement(QLatin1String("Relationships"));
    wbRelsWriter.writeStartElement(QLatin1String("Relationship"));
    wbRelsWriter.writeAttribute(QLatin1String("Id"), QLatin1String("rId1"));
    wbRelsWriter.writeAttribute(QLatin1String("Type"), QLatin1String("http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet"));
    wbRelsWriter.writeAttribute(QLatin1String("Target"), QLatin1String("worksheets/sheet1.xml"));
    wbRelsWriter.writeEndElement();
    wbRelsWriter.writeEndElement();
    wbRelsWriter.writeEndDocument();
    wbRels.close();

    QFile ct(tempDir + QLatin1String("/[Content_Types].xml"));
    ct.open(QIODevice::WriteOnly);
    ct.write("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
             "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
             "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
             "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
             "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
             "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
             "<Override PartName=\"/xl/sharedStrings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml\"/>"
             "</Types>");
    ct.close();

    QFile rootRels(tempDir + QLatin1String("/_rels/.rels"));
    rootRels.open(QIODevice::WriteOnly);
    rootRels.write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                   "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                   "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
                   "</Relationships>");
    rootRels.close();

    QProcess::execute(QLatin1String("powershell.exe"), QStringList{
        QLatin1String("-NoProfile"), QLatin1String("-NonInteractive"), QLatin1String("-Command"),
        QStringLiteral("Compress-Archive -Path '%1/*' -DestinationPath '%2' -Force")
            .arg(QDir::toNativeSeparators(tempDir), QDir::toNativeSeparators(path)) });

    QProcess::execute(QLatin1String("powershell.exe"), QStringList{
        QLatin1String("-NoProfile"), QLatin1String("-NonInteractive"), QLatin1String("-Command"),
        QStringLiteral("Remove-Item -LiteralPath '%1' -Recurse -Force").arg(QDir::toNativeSeparators(tempDir)) });

    return true;
}

} // namespace door::modules

