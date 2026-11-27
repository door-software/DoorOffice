// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <optional>

class QTableWidget;

namespace door::modules {

// Spreadsheet formula evaluator for Door Sheets.
// Supports: numbers, cell refs (A1), ranges (A1:A10) inside functions, + - * / ( ), unary +/-,
// and =SUM() =AVERAGE() =MIN() =MAX() =COUNT().
// Error results: #ERR, #NAME?, #REF!, #VALUE!, #DIV/0!, #CIRC!, #NUM!
class FormulaEngine
{
public:
    explicit FormulaEngine(const QTableWidget* table) noexcept : m_table(table) {}

    // Evaluates a formula (text starting with '='). On failure returns nullopt and sets *error.
    [[nodiscard]] std::optional<double> evaluate(const QString& formula, QString* error = nullptr) const;

    // Text to show in a cell: formulas are evaluated, anything else is returned unchanged.
    [[nodiscard]] QString display(const QString& raw) const
    {
        if (!raw.trimmed().startsWith(u'=')) return raw;
        QString error;
        const auto v = evaluate(raw, &error);
        return v ? QString::number(*v, 'g', 12) : error;
    }

    [[nodiscard]] static QString columnName(int column) // 0 -> "A", 26 -> "AA"
    {
        QString s;
        for (int n = column + 1; n > 0; n = (n - 1) / 26) s.prepend(QChar(u'A' + (n - 1) % 26));
        return s;
    }
    [[nodiscard]] static QString cellName(int row, int column) // (0,0) -> "A1"
    {
        return columnName(column) + QString::number(row + 1);
    }

private:
    const QTableWidget* m_table;
};

} // namespace door::modules
