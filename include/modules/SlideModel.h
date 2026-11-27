// Door Office v1.0 — Slide model for presentations
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QColor>
#include <QRectF>
#include <QSizeF>
#include <QString>

namespace door::modules {

struct SlideItem {
    enum class Type { Text, Image, Rect, Ellipse, Bullet };
    Type type = Type::Text;
    QString text;
    QRectF rect;          // geometry for shapes / image placement
    QColor color;         // fill / text color
    QString imagePath;    // for Image type
    QString notes;        // optional speaker note attached to this item
};

struct SlidePage {
    QString title;
    QString subtitle;
    QString body;         // free‑form text block
    QVector<QString> bullets;   // list of bullet strings
    QVector<SlideItem> shapes; // rect/ellipse / image items
    QColor backgroundColor = Qt::white;
    QColor accentColor = Qt::black;
    QString speakerNotes;   // whole‑page notes
};

class SlideDeck final : public QObject
{
    Q_OBJECT
public:
    explicit SlideDeck(QObject *parent = nullptr) : QObject(parent) {}
    // QObject is non-copyable, but SlideDeck is used as a value type in
    // loadFromFile()/openFile(). Provide an explicit copy that detaches from
    // any parent object and deep-copies the deck contents.
    SlideDeck(const SlideDeck &other)
        : QObject(nullptr), pages(other.pages), m_currentIndex(other.m_currentIndex) {}
    SlideDeck &operator=(const SlideDeck &other) {
        if (this != &other) {
            pages = other.pages;
            m_currentIndex = other.m_currentIndex;
        }
        return *this;
    }
    ~SlideDeck() override = default;

    // ---- data -------------------------------------------------
    QList<SlidePage> pages;          // all slides in order
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int idx);

    // ---- JSON persistence -------------------------------------
    static bool saveToFile(const SlideDeck &deck, const QString &filePath);
    static SlideDeck loadFromFile(const QString &filePath);

    // ---- handy -------------------------------------------------
    [[nodiscard]] int pageCount() const { return pages.size(); }
    [[nodiscard]] const SlidePage &pageAt(int idx) const;

signals:
    void pageChanged(int index);
    void deckModified();

private:
    int m_currentIndex = 0;
};

} // namespace door::modules