// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
// Door Sheets persistence (.dsheet JSON / .csv), printing and PDF export.
#include "modules/DoorSheetsWidget.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QTableWidget>

#include <algorithm>

namespace door::modules {

namespace {

QString formatId() { return QStringLiteral("DoorSheet"); }

struct BusyFlag // suppresses itemChanged bookkeeping while loading
{
    bool& f;
    explicit BusyFlag(bool& flag) : f(flag) { f = true; }
    ~BusyFlag() { f = false; }
};

bool hasContent(const QTableWidgetItem* it)
{
    return it && (!it->data(Qt::EditRole).toString().isEmpty() || it->font().bold()
                  || it->data(Qt::TextAlignmentRole).isValid());
}

QString alignName(const QTableWidgetItem* it)
{
    const int a = it->data(Qt::TextAlignmentRole).toInt();
    return a & Qt::AlignRight ? QStringLiteral("right") : a & Qt::AlignHCenter ? QStringLiteral("center")
         : a & Qt::AlignLeft ? QStringLiteral("left") : QString();
}

bool parseCell(const QString& name, int& row, int& col)
{
    static const QRegularExpression re(QStringLiteral("^([A-Za-z]{1,3})(\\d{1,7})$"));
    const auto m = re.match(name);
    if (!m.hasMatch()) return false;
    col = 0;
    for (const QChar ch : m.captured(1).toUpper()) col = col * 26 + (ch.unicode() - u'A' + 1);
    --col;
    row = m.captured(2).toInt() - 1;
    return row >= 0 && col >= 0;
}

QList<QStringList> parseCsv(const QString& t)
{
    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool quoted = false, pending = false;
    for (qsizetype i = 0; i < t.size(); ++i) {
        const QChar ch = t[i];
        if (quoted) {
            if (ch != u'"') field += ch;
            else if (i + 1 < t.size() && t[i + 1] == u'"') { field += ch; ++i; }
            else quoted = false;
        } else if (ch == u'"') { quoted = pending = true; }
        else if (ch == u',') { row << field; field.clear(); pending = true; }
        else if (ch == u'\n' || ch == u'\r') {
            if (ch == u'\r' && i + 1 < t.size() && t[i + 1] == u'\n') ++i;
            row << field; rows << row; row.clear(); field.clear(); pending = false;
        } else { field += ch; pending = true; }
    }
    if (pending || !field.isEmpty() || !row.isEmpty()) { row << field; rows << row; }
    return rows;
}

} // namespace

QRect DoorSheetsWidget::usedRange() const
{
    int maxR = -1, maxC = -1;
    for (int r = 0; r < m_table->rowCount(); ++r)
        for (int c = 0; c < m_table->columnCount(); ++c)
            if (hasContent(m_table->item(r, c))) { maxR = std::max(maxR, r); maxC = std::max(maxC, c); }
    return maxR < 0 ? QRect() : QRect(0, 0, maxC + 1, maxR + 1);
}

QByteArray DoorSheetsWidget::toCsv() const
{
    static const QRegularExpression special(QStringLiteral("[,\"\\r\\n]"));
    const QRect used = usedRange();
    QString out;
    for (int r = 0; r < used.height(); ++r) {
        QStringList fields;
        for (int c = 0; c < used.width(); ++c) {
            const QTableWidgetItem* it = m_table->item(r, c);
            QString text = it ? it->data(Qt::EditRole).toString() : QString(); // raw: formulas survive
            if (text.contains(special)) text = u'"' + text.replace(u'"', QStringLiteral("\"\"")) + u'"';
            fields << text;
        }
        out += fields.join(u',') + QStringLiteral("\r\n");
    }
    return QByteArray("\xEF\xBB\xBF") + out.toUtf8(); // UTF-8 BOM so Excel detects the encoding
}

QJsonObject DoorSheetsWidget::toJson() const
{
    QJsonObject cells;
    for (int r = 0; r < m_table->rowCount(); ++r)
        for (int c = 0; c < m_table->columnCount(); ++c) {
            const QTableWidgetItem* it = m_table->item(r, c);
            if (!hasContent(it)) continue;
            QJsonObject cell{{QStringLiteral("v"), it->data(Qt::EditRole).toString()}};
            if (it->font().bold()) cell.insert(QStringLiteral("b"), true);
            if (const QString a = alignName(it); !a.isEmpty()) cell.insert(QStringLiteral("a"), a);
            cells.insert(FormulaEngine::cellName(r, c), cell);
        }
    return {{QStringLiteral("format"), formatId()}, {QStringLiteral("version"), 1},
            {QStringLiteral("rows"), m_table->rowCount()}, {QStringLiteral("cols"), m_table->columnCount()},
            {QStringLiteral("cells"), cells}};
}

bool DoorSheetsWidget::readCsv(const QString& text)
{
    const QList<QStringList> rows = parseCsv(text.startsWith(QChar(0xFEFF)) ? text.mid(1) : text);
    qsizetype cols = 0;
    for (const QStringList& row : rows) cols = std::max(cols, row.size());
    BusyFlag busy(m_busy);
    prepare(int(rows.size()), int(cols));
    for (int r = 0; r < rows.size() && r < m_table->rowCount(); ++r)
        for (int c = 0; c < rows[r].size() && c < m_table->columnCount(); ++c)
            if (!rows[r][c].isEmpty()) ensureItem(r, c)->setData(Qt::EditRole, rows[r][c]);
    return true;
}

bool DoorSheetsWidget::readJson(const QByteArray& data)
{
    QJsonParseError err;
    const QJsonObject o = QJsonDocument::fromJson(data, &err).object();
    if (err.error != QJsonParseError::NoError || o.value(QStringLiteral("format")).toString() != formatId())
        return false;
    BusyFlag busy(m_busy);
    prepare(o.value(QStringLiteral("rows")).toInt(100), o.value(QStringLiteral("cols")).toInt(26));
    const QJsonObject cells = o.value(QStringLiteral("cells")).toObject();
    for (auto i = cells.begin(); i != cells.end(); ++i) {
        int r = 0, c = 0;
        if (!parseCell(i.key(), r, c) || r >= m_table->rowCount() || c >= m_table->columnCount()) continue;
        const QJsonObject cell = i.value().toObject();
        QTableWidgetItem* it = ensureItem(r, c);
        it->setData(Qt::EditRole, cell.value(QStringLiteral("v")).toString());
        if (cell.value(QStringLiteral("b")).toBool()) { QFont f = it->font(); f.setBold(true); it->setFont(f); }
        const QString a = cell.value(QStringLiteral("a")).toString();
        if (!a.isEmpty()) {
            const Qt::Alignment h = a == QLatin1String("right") ? Qt::AlignRight
                                  : a == QLatin1String("center") ? Qt::AlignHCenter : Qt::AlignLeft;
            it->setData(Qt::TextAlignmentRole, int(h | Qt::AlignVCenter));
        }
    }
    return true;
}


} // namespace door::modules
