// Door Office v1.0 — Slideshow window implementation
// SPDX-License-Identifier: GPL-3.0-or-later
#include "modules/SlideshowWindow.h"

#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QStyleHints>

namespace door::modules {

SlideshowWindow::SlideshowWindow(SlideDeck* deck, QWidget* parent)
    : QWidget(parent), m_deck(deck)
{
    setWindowTitle(QGuiApplication::translate("DoorSlides", "Door Office — Presentation"));
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_index = m_deck ? m_deck->currentIndex() : 0;
    if (QWindow* w = windowHandle()) w->setScreen(QGuiApplication::primaryScreen());
    showFullScreen();
}

SlideshowWindow::~SlideshowWindow() = default;

void SlideshowWindow::next()
{
    if (!m_deck || m_index + 1 >= m_deck->pageCount()) return;
    ++m_index;
    m_deck->setCurrentIndex(m_index);
    update();
}

void SlideshowWindow::prev()
{
    if (!m_deck || m_index <= 0) return;
    --m_index;
    m_deck->setCurrentIndex(m_index);
    update();
}

void SlideshowWindow::exit()
{
    emit closed();
    close();
}

void SlideshowWindow::paintEvent(QPaintEvent*)
{
    if (!m_deck) return;
    const SlidePage &p = m_deck->pageAt(m_index);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), p.backgroundColor);

    // 16:9 content rect centered inside widget
    const QRectF view = rect();
    const qreal aspect = 16.0 / 9.0;
    qreal w = view.width(), h = w / aspect;
    if (h > view.height()) { h = view.height(); w = h * aspect; }
    const QRectF area(view.x() + (view.width() - w) / 2, view.y() + (view.height() - h) / 2, w, h);

    // draw shapes / images
    for (const SlideItem &it : p.shapes) {
        const QRectF r(it.rect.x() * w + area.x(), it.rect.y() * h + area.y(),
                      it.rect.width() * w, it.rect.height() * h);
        if (it.type == SlideItem::Type::Rect) {
            painter.fillRect(r, it.color);
        } else if (it.type == SlideItem::Type::Ellipse) {
            painter.setBrush(it.color);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(r);
        } else if (it.type == SlideItem::Type::Image && !it.imagePath.isEmpty()) {
            const QPixmap pm(it.imagePath);
            if (!pm.isNull())
                painter.drawPixmap(r, pm, QRectF(pm.rect()));
        }
    }

    // title (large, bold, accent)
    QFont f = painter.font();
    f.setPointSizeF(qMax(24.0, h * 0.055));
    f.setWeight(QFont::Bold);
    painter.setFont(f);
    painter.setPen(p.accentColor);
    QRectF tr(area.x() + w * 0.06, area.y() + h * 0.10, w * 0.88, h * 0.22);
    painter.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, p.title);

    // subtitle
    f.setPointSizeF(qMax(14.0, h * 0.028));
    f.setWeight(QFont::Normal);
    painter.setFont(f);
    painter.setPen(p.accentColor);
    QRectF sr(area.x() + w * 0.06, area.y() + h * 0.34, w * 0.88, h * 0.12);
    painter.drawText(sr, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, p.subtitle);

    // body text
    f.setPointSizeF(qMax(12.0, h * 0.022));
    painter.setFont(f);
    QRectF br(area.x() + w * 0.06, area.y() + h * 0.48, w * 0.88, h * 0.36);
    painter.drawText(br, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, p.body);

    // bullets
    f.setPointSizeF(qMax(10.0, h * 0.018));
    painter.setFont(f);
    QRectF blr(area.x() + w * 0.06, area.y() + h * 0.86, w * 0.88, h * 0.10);
    QString b = p.bullets.join(QStringLiteral("  •  "));
    painter.drawText(blr, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, b);

    // subtle HUD
    QFont hud = painter.font();
    hud.setPointSizeF(10.0);
    painter.setFont(hud);
    painter.setPen(QColor(255, 255, 255, 120));
    painter.drawText(rect().adjusted(0, 0, -12, -6), Qt::AlignRight | Qt::AlignBottom,
                     QStringLiteral("Slide %1 of %2").arg(m_index + 1).arg(m_deck->pageCount()));
}

void SlideshowWindow::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Escape:   exit(); break;
    case Qt::Key_Space:    next(); break;
    case Qt::Key_Right:    next(); break;
    case Qt::Key_Left:     prev(); break;
    default:               QWidget::keyPressEvent(e); break;
    }
}

void SlideshowWindow::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) next();
    else if (e->button() == Qt::RightButton) prev();
    e->accept();
}

} // namespace door::modules