// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#include "modules/FormulaEngine.h"

#include <QSet>
#include <QTableWidget>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace door::modules {
namespace {

struct Err { const char* code; };
[[noreturn]] void fail(const char* code) { throw Err{code}; }
bool isAlpha(QChar c) { return c.unicode() < 128 && c.isLetter(); }
bool isDigit(QChar c) { return c.unicode() < 128 && c.isDigit(); }
bool isNumCh(QChar c) { return isDigit(c) || c == u'.'; }

// Recursive descent: expr := term (('+'|'-') term)* ; term := unary (('*'|'/') unary)*
class Parser {
public:
    Parser(const QTableWidget* t, QString s, QSet<qint64>& active) : m_t(t), m_s(std::move(s)), m_active(active) {}
    double run() { m_pos = 1; const double v = expr(); if (peek() != QChar()) fail("#ERR"); return v; }

private:
    QChar peek() { while (m_pos < m_s.size() && m_s[m_pos].isSpace()) ++m_pos; return m_pos < m_s.size() ? m_s[m_pos] : QChar(); }
    bool eat(QChar c) { if (peek() != c) return false; ++m_pos; return true; }
    bool at(bool (*pred)(QChar)) const { return m_pos < m_s.size() && pred(m_s[m_pos]); }

    double expr() {
        double v = term();
        for (;;) { if (eat(u'+')) v += term(); else if (eat(u'-')) v -= term(); else return v; }
    }
    double term() {
        double v = unary();
        for (;;) {
            if (eat(u'*')) v *= unary();
            else if (eat(u'/')) { const double d = unary(); if (d == 0.0) fail("#DIV/0!"); v /= d; }
            else return v;
        }
    }
    double unary() { return eat(u'-') ? -unary() : eat(u'+') ? unary() : primary(); }

    double primary() {
        const QChar c = peek();
        if (eat(u'(')) { const double v = expr(); if (!eat(u')')) fail("#ERR"); return v; }
        const int start = m_pos;
        bool ok = false;
        if (isNumCh(c)) {
            while (at(isNumCh)) ++m_pos;
            const double v = m_s.mid(start, m_pos - start).toDouble(&ok);
            if (!ok) fail("#ERR");
            return v;
        }
        if (!isAlpha(c)) fail("#ERR");
        while (at(isAlpha)) ++m_pos;
        const QString name = m_s.mid(start, m_pos - start).toUpper();
        if (eat(u'(')) return call(name);
        m_pos = start;
        int r = 0, col = 0;
        bool text = false;
        if (!ref(r, col)) fail("#NAME?");
        const auto v = lookup(r, col, text);
        if (text) fail("#VALUE!");
        return v.value_or(0.0);
    }

    // Parses a cell reference such as A1 / ab12 at the cursor; restores the cursor on failure.
    bool ref(int& row, int& col)
    {
        peek();
        const int save = m_pos;
        int c = 0, r = 0, letters = 0, digits = 0;
        for (; at(isAlpha); ++m_pos, ++letters) {
            if (letters >= 3) fail("#REF!");
            c = c * 26 + (m_s[m_pos].toUpper().unicode() - u'A' + 1);
        }
        for (; at(isDigit); ++m_pos, ++digits) {
            if (digits >= 7) fail("#REF!");
            r = r * 10 + (m_s[m_pos].unicode() - u'0');
        }
        if (!letters || !digits || at(isAlpha)) { m_pos = save; return false; }
        row = r - 1; col = c - 1;
        return true;
    }

    // Numeric value of a cell; nullopt when empty or text (text also sets the flag).
    std::optional<double> lookup(int r, int c, bool& text)
    {
        text = false;
        if (r < 0 || c < 0 || r >= m_t->rowCount() || c >= m_t->columnCount()) fail("#REF!");
        const auto* it = m_t->item(r, c);
        const QString raw = it ? it->data(Qt::EditRole).toString().trimmed() : QString();
        if (raw.isEmpty()) return std::nullopt;
        if (raw.startsWith(u'=')) {
            const qint64 key = (qint64(r) << 32) | quint32(c);
            if (m_active.contains(key)) fail("#CIRC!");
            struct Guard { QSet<qint64>& s; qint64 k; Guard(QSet<qint64>& a, qint64 x) : s(a), k(x) { s.insert(k); } ~Guard() { s.remove(k); } } guard(m_active, key);
            return Parser(m_t, raw, m_active).run();
        }
        bool ok = false;
        const double v = raw.toDouble(&ok);
        text = !ok;
        return ok ? std::optional<double>(v) : std::nullopt;
    }

    double call(const QString& name)
    {
        QList<double> v;
        if (peek() != u')') do argument(v); while (eat(u','));
        if (!eat(u')')) fail("#ERR");
        const double sum = std::accumulate(v.begin(), v.end(), 0.0), n = double(v.size());
        const bool none = v.isEmpty();
        if (name == u"SUM") return sum;
        if (name == u"COUNT") return n;
        if (name == u"AVERAGE") return none ? (fail("#DIV/0!"), 0.0) : sum / n;
        if (name == u"MIN") return none ? 0.0 : *std::min_element(v.begin(), v.end());
        if (name == u"MAX") return none ? 0.0 : *std::max_element(v.begin(), v.end());
        fail("#NAME?");
    }

    // One function argument: a range (numeric cells only) or a scalar expression.
    void argument(QList<double>& out)
    {
        const int save = m_pos;
        int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
        bool text = false;
        if (!(ref(r1, c1) && eat(u':'))) { m_pos = save; out.append(expr()); return; }
        if (!ref(r2, c2)) fail("#REF!");
        for (int r = std::min(r1, r2); r <= std::max(r1, r2); ++r)
            for (int c = std::min(c1, c2); c <= std::max(c1, c2); ++c)
                if (const auto v = lookup(r, c, text)) out.append(*v);
    }

    const QTableWidget* m_t; QString m_s; QSet<qint64>& m_active; int m_pos = 0;
};

} // namespace

std::optional<double> FormulaEngine::evaluate(const QString& formula, QString* error) const
{
    try {
        const QString text = formula.trimmed();
        QSet<qint64> active;
        if (!text.startsWith(u'=')) fail("#ERR");
        const double v = Parser(m_table, text, active).run();
        if (!std::isfinite(v)) fail("#NUM!");
        return v;
    } catch (const Err& e) {
        if (error) *error = QString::fromLatin1(e.code);
        return std::nullopt;
    }
}

} // namespace door::modules

