// Door Office v1.0 — Slide model implementation
// SPDX-License-Identifier: GPL-3.0-or-later
#include "modules/SlideModel.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonValue>
#include <QVariant>
#include <QDebug>

namespace door::modules {

// ---------- helpers ---------------------------------------------------------
namespace {
SlideItem::Type strToType(const QString &s) {
    if (s == "image") return SlideItem::Type::Image;
    if (s == "rect")  return SlideItem::Type::Rect;
    if (s == "ellipse") return SlideItem::Type::Ellipse;
    if (s == "bullet") return SlideItem::Type::Bullet;
    return SlideItem::Type::Text;
}

QString typeToStr(SlideItem::Type t) {
    switch (t) {
    case SlideItem::Type::Text:    return "text";
    case SlideItem::Type::Image:   return "image";
    case SlideItem::Type::Rect:    return "rect";
    case SlideItem::Type::Ellipse: return "ellipse";
    case SlideItem::Type::Bullet:  return "bullet";
    }
    return "text";
}

QJsonObject itemToJson(const SlideItem &item) {
    QJsonObject o;
    o["type"] = typeToStr(item.type);
    o["text"] = item.text;
    o["rect"] = QJsonObject{{"x", item.rect.x()}, {"y", item.rect.y()},
                             {"w", item.rect.width()}, {"h", item.rect.height()}};
    o["color"] = item.color.name(QColor::HexArgb);
    o["imagePath"] = item.imagePath;
    o["notes"] = item.notes;
    return o;
}

SlideItem itemFromJson(const QJsonObject &o) {
    SlideItem i;
    i.type = strToType(o.value("type").toString());
    i.text = o.value("text").toString();
    QJsonObject r = o.value("rect").toObject();
    i.rect = QRectF(r.value("x").toDouble(), r.value("y").toDouble(),
                    r.value("w").toDouble(), r.value("h").toDouble());
    i.color = QColor(o.value("color").toString());
    i.imagePath = o.value("imagePath").toString();
    i.notes = o.value("notes").toString();
    return i;
}

QJsonObject pageToJson(const SlidePage &p) {
    QJsonObject o;
    o["title"]    = p.title;
    o["subtitle"] = p.subtitle;
    o["body"]     = p.body;
    QJsonArray bl;
    for (const QString &b : p.bullets) bl.append(b);
    o["bullets"] = bl;
    QJsonArray sh;
    for (const SlideItem &it : p.shapes) sh.append(itemToJson(it));
    o["shapes"] = sh;
    o["bg"]     = p.backgroundColor.name(QColor::HexArgb);
    o["accent"] = p.accentColor.name(QColor::HexArgb);
    o["notes"]  = p.speakerNotes;
    return o;
}

SlidePage pageFromJson(const QJsonObject &o) {
    SlidePage p;
    p.title    = o.value("title").toString();
    p.subtitle = o.value("subtitle").toString();
    p.body     = o.value("body").toString();
    for (const QJsonValue &v : o.value("bullets").toArray()) p.bullets.append(v.toString());
    for (const QJsonValue &v : o.value("shapes").toArray()) p.shapes.append(itemFromJson(v.toObject()));
    p.backgroundColor = QColor(o.value("bg").toString());
    p.accentColor     = QColor(o.value("accent").toString());
    p.speakerNotes    = o.value("notes").toString();
    return p;
}
} // anonymous namespace

// ---------- SlideDeck methods ---------------------------------------------
void SlideDeck::setCurrentIndex(int idx) {
    if (pages.isEmpty()) { m_currentIndex = 0; return; }
    m_currentIndex = qBound(0, idx, pages.size() - 1);
    emit pageChanged(m_currentIndex);
}

const SlidePage &SlideDeck::pageAt(int idx) const {
    static SlidePage empty;
    if (idx < 0 || idx >= pages.size()) return empty;
    return pages.at(idx);
}

bool SlideDeck::saveToFile(const SlideDeck &deck, const QString &filePath) {
    QJsonArray arr;
    for (const SlidePage &p : deck.pages) arr.append(pageToJson(p));
    QJsonObject root{{"version", 1}, {"pages", arr}};
    QSaveFile f(filePath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return f.commit();
}

SlideDeck SlideDeck::loadFromFile(const QString &filePath) {
    SlideDeck d;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return d;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return d;
    QJsonObject root = doc.object();
    for (const QJsonValue &v : root.value("pages").toArray())
        d.pages.append(pageFromJson(v.toObject()));
    if (!d.pages.isEmpty()) d.setCurrentIndex(0);
    return d;
}

} // namespace door::modules