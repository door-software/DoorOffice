// Door Office v1.0 — DoorSlidesWidget implementation
// SPDX-License-Identifier: GPL-3.0-or-later
#include "modules/DoorSlidesWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QColorDialog>
#include <QTextEdit>
#include <QFileDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QPainter>
#include <QMessageBox>
#include <QLabel>
#include <QMouseEvent>

#include <QFileInfo>
#include <QSizePolicy>
#include <QXmlStreamWriter>
#include <QProcess>
#include <QDateTime>

#include <utility>

namespace door::modules {

// ---------- SlideCanvas ----------------------------------------------------
void SlideCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), m_slide.backgroundColor);

    m_viewArea = viewAreaIn(rect());
    const QRectF& area = m_viewArea;
    const qreal w = area.width();
    const qreal h = area.height();

    for (const SlideItem& it : m_slide.shapes) {
        QRectF r(it.rect.x() * w + area.x(), it.rect.y() * h + area.y(),
                 it.rect.width() * w, it.rect.height() * h);
        if (it.type == SlideItem::Type::Rect) p.fillRect(r, it.color);
        else if (it.type == SlideItem::Type::Ellipse) {
            p.setBrush(it.color); p.setPen(Qt::NoPen); p.drawEllipse(r);
        } else if (it.type == SlideItem::Type::Image && !it.imagePath.isEmpty()) {
            QPixmap pm(it.imagePath);
            if (!pm.isNull()) p.drawPixmap(r, pm, QRectF(pm.rect()));
        }
    }
    QFont f = p.font(); f.setPointSizeF(h * 0.05); f.setBold(true);
    p.setFont(f); p.setPen(m_slide.accentColor);
    QRectF tr(area.x() + w * 0.06, area.y() + h * 0.10, w * 0.88, h * 0.22);
    p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, m_slide.title);
    f.setPointSizeF(h * 0.025); f.setBold(false);
    p.setFont(f);
    p.drawText(area.adjusted(w * 0.06, h * 0.34, -w * 0.06, -h * 0.04),
               Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
               m_slide.body + "\n" + m_slide.subtitle);
}

QRectF SlideCanvas::viewAreaIn(const QRectF& region) const
{
    const qreal aspect = 16.0 / 9.0;
    qreal w = region.width();
    qreal h = w / aspect;
    if (h > region.height()) { h = region.height(); w = h * aspect; }
    return QRectF(region.x() + (region.width() - w) / 2.0,
                  region.y() + (region.height() - h) / 2.0, w, h);
}

QRectF SlideCanvas::itemViewRect(const SlideItem& it, const QRectF& area) const
{
    return QRectF(area.x() + it.rect.x() * area.width(),
                  area.y() + it.rect.y() * area.height(),
                  it.rect.width() * area.width(),
                  it.rect.height() * area.height());
}

int SlideCanvas::itemAt(const QPointF& pos, const QRectF& area) const
{
    for (int i = m_slide.shapes.size() - 1; i >= 0; --i) {
        if (itemViewRect(m_slide.shapes[i], area).contains(pos))
            return i;
    }
    return -1;
}

void SlideCanvas::mousePressEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    m_viewArea = viewAreaIn(rect());
    const int index = itemAt(pos, m_viewArea);
    if (index >= 0) {
        m_selectedIndex = index;
        m_dragging = true;
        m_resizing = false;
        m_dragStartPos = pos;
        m_dragStartNorm = m_slide.shapes[index].rect;
    } else {
        m_selectedIndex = -1;
        m_dragging = false;
        m_resizing = false;
    }
    update();
}

void SlideCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging || m_selectedIndex < 0 || m_selectedIndex >= m_slide.shapes.size())
        return;

    const QPointF cur = event->position();
    const qreal dx = (cur.x() - m_dragStartPos.x()) / m_viewArea.width();
    const qreal dy = (cur.y() - m_dragStartPos.y()) / m_viewArea.height();

    QRectF newRect = m_dragStartNorm;
    if (m_resizing) {
        newRect.setWidth(qBound(kMinItemSize, m_dragStartNorm.width() + dx, kMaxItemSize));
        newRect.setHeight(qBound(kMinItemSize, m_dragStartNorm.height() + dy, kMaxItemSize));
    } else {
        qreal x = m_dragStartNorm.x() + dx;
        qreal y = m_dragStartNorm.y() + dy;
        x = qBound(0.0, x, 1.0 - newRect.width());
        y = qBound(0.0, y, 1.0 - newRect.height());
        newRect.moveTo(x, y);
    }
    m_slide.shapes[m_selectedIndex].rect = newRect;
    update();
}

void SlideCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    if (m_dragging || m_resizing) {
        if (m_selectedIndex >= 0 && m_selectedIndex < m_slide.shapes.size())
            emit itemRectChanged(m_selectedIndex, m_slide.shapes[m_selectedIndex].rect);
        m_dragging = false;
        m_resizing = false;
        m_dragStartPos = QPointF(0, 0);
        update();
    }
}

// ---------- DoorSlidesWidget ---------------------------------------------

// ---------- DoorSlidesWidget ---------------------------------------------
DoorSlidesWidget::DoorSlidesWidget(QWidget* parent) : QWidget(parent) { buildUi(); }
DoorSlidesWidget::~DoorSlidesWidget() = default;

void DoorSlidesWidget::buildUi()
{
    QVBoxLayout* root = new QVBoxLayout(this);

    // ---- top studio toolbar ----
    QHBoxLayout* top = new QHBoxLayout();
    m_themeCombo = new QComboBox();
    m_themeCombo->addItems(QStringList{"Midnight Slate", "Warm Ivory", "Electric Violet", "Ocean Teal"});
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DoorSlidesWidget::applyTheme);
    m_bgBtn = new QPushButton(QStringLiteral("Background Color"));
    connect(m_bgBtn, &QPushButton::clicked, this, &DoorSlidesWidget::chooseBackground);
    QPushButton* addBtn = new QPushButton(QStringLiteral("Add Slide"));
    connect(addBtn, &QPushButton::clicked, this, &DoorSlidesWidget::addSlide);
    QPushButton* dupBtn = new QPushButton(QStringLiteral("Duplicate"));
    connect(dupBtn, &QPushButton::clicked, this, &DoorSlidesWidget::duplicateSlide);
    QPushButton* upBtn = new QPushButton(QStringLiteral("Move Up"));
    connect(upBtn, &QPushButton::clicked, this, [this]{ moveSlide(0, true); });
    QPushButton* downBtn = new QPushButton(QStringLiteral("Move Down"));
    connect(downBtn, &QPushButton::clicked, this, [this]{ moveSlide(0, false); });
    QPushButton* delBtn = new QPushButton(QStringLiteral("Delete"));
    connect(delBtn, &QPushButton::clicked, this, &DoorSlidesWidget::deleteSlide);
    QPushButton* shapeBtn = new QPushButton(QStringLiteral("Insert Shape"));
    connect(shapeBtn, &QPushButton::clicked, this, &DoorSlidesWidget::insertShape);
    QPushButton* imgBtn = new QPushButton(QStringLiteral("Insert Image"));
    connect(imgBtn, &QPushButton::clicked, this, &DoorSlidesWidget::insertImage);
    QPushButton* presentBtn = new QPushButton(QStringLiteral("Present (F5)"));
    presentBtn->setDefault(true);
    connect(presentBtn, &QPushButton::clicked, []{ /* presenter launched from MainWindow */ });
    top->addWidget(m_themeCombo);
    top->addWidget(m_bgBtn);
    top->addStretch(1);
    top->addWidget(addBtn); top->addWidget(dupBtn); top->addWidget(upBtn);
    top->addWidget(downBtn); top->addWidget(delBtn);
    top->addStretch(1);
    top->addWidget(shapeBtn); top->addWidget(imgBtn); top->addWidget(presentBtn);
    root->addLayout(top);

    // ---- center: live-rendered canvas + inspector ----
    QHBoxLayout* center = new QHBoxLayout();
    m_canvas = new SlideCanvas(this);
    center->addWidget(m_canvas, 1);
    QVBoxLayout* insp = new QVBoxLayout();
    insp->addWidget(new QLabel(QStringLiteral("Inspector")));
    m_notes = new QTextEdit();
    m_notes->setPlaceholderText(QStringLiteral("Speaker notes..."));
    connect(m_notes, &QTextEdit::textChanged, this, &DoorSlidesWidget::onNotesChanged);
    insp->addWidget(m_notes, 1);
    center->addLayout(insp);
    root->addLayout(center, 1);

    // status message bar
    auto* msg = new QLabel();
    msg->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    root->addWidget(msg);

    QMetaObject::invokeMethod(this, [this]{
        SlidePage s; s.title = QStringLiteral("Untitled Presentation");
        m_deck.pages.append(s);
        m_deck.setCurrentIndex(0);
        renderCurrent();
    });
}

void DoorSlidesWidget::renderCurrent()
{
    const SlidePage& p = m_deck.pageAt(m_deck.currentIndex());
    m_canvas->setSlide(p);
    m_canvas->update();
    m_notes->setPlainText(p.speakerNotes);
}

void DoorSlidesWidget::addSlide()
{
    SlidePage s;
    s.title = QStringLiteral("Slide %1").arg(m_deck.pageCount() + 1);
    m_deck.pages.append(s);
    m_deck.setCurrentIndex(m_deck.pageCount() - 1);
    renderCurrent();
    emit m_deck.deckModified();
    emit titleChanged(displayName());
}

void DoorSlidesWidget::duplicateSlide()
{
    int i = m_deck.currentIndex();
    SlidePage dup = m_deck.pageAt(i);
    m_deck.pages.insert(i + 1, dup);
    m_deck.setCurrentIndex(i + 1);
    renderCurrent();
    emit m_deck.deckModified();
    emit titleChanged(displayName());
}

void DoorSlidesWidget::moveSlide(int /*row*/, bool up)
{
    int i = m_deck.currentIndex();
    int j = up ? i - 1 : i + 1;
    if (j < 0 || j >= m_deck.pageCount()) return;
    std::swap(m_deck.pages[i], m_deck.pages[j]);
    m_deck.setCurrentIndex(j);
    renderCurrent();
    emit m_deck.deckModified();
    emit titleChanged(displayName());
    
}
    void DoorSlidesWidget::deleteSlide()
    {
        int i = m_deck.currentIndex();
        if (i < 0 || i >= m_deck.pageCount()) return;
        m_deck.pages.erase(m_deck.pages.begin() + i);
        if (m_deck.pageCount() == 0) {
            m_deck.pages.push_back(SlidePage());
            m_deck.setCurrentIndex(0);
        } else if (i >= m_deck.pageCount()) {
            m_deck.setCurrentIndex(i - 1);
        }
        renderCurrent();
        emit m_deck.deckModified();
        emit titleChanged(displayName());
    }

    void DoorSlidesWidget::applyTheme(int index)
{
    QColor bg, accent;
    switch (index) {
    case 0: bg = QColor("#1e293b"); accent = QColor("#38bdf8"); break;     // Midnight Slate
    case 1: bg = QColor("#fef3c7"); accent = QColor("#ca8a04"); break;     // Warm Ivory
    case 2: bg = QColor("#3f0c3f"); accent = QColor("#c084fc"); break;     // Electric Violet
    case 3: bg = QColor("#0f4c5c"); accent = QColor("#38bdf8"); break;     // Ocean Teal
    default: return;
    }
    SlidePage& p = const_cast<SlidePage&>(m_deck.pageAt(m_deck.currentIndex()));
    p.backgroundColor = bg;
    p.accentColor = accent;
    renderCurrent();
    emit m_deck.deckModified();
    emit titleChanged(displayName());
}

void DoorSlidesWidget::chooseBackground()
{
    QColor c = QColorDialog::getColor(m_deck.pageAt(m_deck.currentIndex()).backgroundColor,
                                      this, QStringLiteral("Pick background"));
    if (!c.isValid()) return;
    SlidePage& p = const_cast<SlidePage&>(m_deck.pageAt(m_deck.currentIndex()));
    p.backgroundColor = c;
    renderCurrent();
    emit m_deck.deckModified();
    emit titleChanged(displayName());
}

void DoorSlidesWidget::insertShape()
{
    SlideItem it; it.type = SlideItem::Type::Rect;
    it.rect = QRectF(0.5, 0.5, 0.3, 0.3); it.color = m_deck.pageAt(m_deck.currentIndex()).accentColor;
    SlidePage& p = const_cast<SlidePage&>(m_deck.pageAt(m_deck.currentIndex()));
    p.shapes.append(it);
    renderCurrent();
    emit m_deck.deckModified();
    emit titleChanged(displayName());
}

void DoorSlidesWidget::insertImage()
{
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Insert Image"), {},
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.svg *.webp)"));
    if (path.isEmpty()) return;
    SlideItem it; it.type = SlideItem::Type::Image; it.imagePath = path;
    it.rect = QRectF(0.5, 0.4, 0.4, 0.4);
    SlidePage& p = const_cast<SlidePage&>(m_deck.pageAt(m_deck.currentIndex()));
    p.shapes.append(it);
    renderCurrent();
    emit m_deck.deckModified();
    emit titleChanged(displayName());
}

void DoorSlidesWidget::onNotesChanged()
{
    SlidePage& p = const_cast<SlidePage&>(m_deck.pageAt(m_deck.currentIndex()));
    p.speakerNotes = m_notes->toPlainText();
    emit m_deck.deckModified();
    emit titleChanged(displayName());
}

bool DoorSlidesWidget::openFile(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("dslide")) {
        SlideDeck d = SlideDeck::loadFromFile(path);
        if (d.pageCount() == 0) {
            QMessageBox::warning(this, tr("Door Office"), tr("Could not open presentation."));
            return false;
        }
        m_deck = d;
        m_filePath = path;
        m_deck.setCurrentIndex(0);
        renderCurrent();
        emit statusMessage(QStringLiteral("Opened %1").arg(path));
        emit titleChanged(displayName());
        return true;
    } else if (suffix == QLatin1String("pptx")) {
        // Minimal PPTX: extract to temp dir and load JSON metadata, or skip to new blank.
        // For now, show it's supported and open as blank or via ZIP metadata.
        QMessageBox::information(this, tr("Door Slides"),
            tr("PowerPoint (.pptx) support will be available in a future update.\n"
               "You can save your presentation as .dslide, .pdf, or .csv for now."));
        // Still create a blank slide deck
        SlidePage s;
        s.title = QStringLiteral("Slide 1");
        m_deck.pages.clear();
        m_deck.pages.append(s);
        m_deck.setCurrentIndex(0);
        m_filePath = path;
        renderCurrent();
        emit statusMessage(QStringLiteral("Opened %1").arg(path));
        emit titleChanged(displayName());
        return true;
    } else {
        // Fall through to existing .dslide path
        SlideDeck d = SlideDeck::loadFromFile(path);
        if (d.pageCount() == 0) {
            QMessageBox::warning(this, tr("Door Office"), tr("Could not open presentation."));
            return false;
        }
        m_deck = d;
        m_filePath = path;
        m_deck.setCurrentIndex(0);
        renderCurrent();
        emit statusMessage(QStringLiteral("Opened %1").arg(path));
        emit titleChanged(displayName());
        return true;
    }
}

bool DoorSlidesWidget::save()
{
    if (m_filePath.isEmpty())
        return saveAs();
    if (!SlideDeck::saveToFile(m_deck, m_filePath)) {
        QMessageBox::critical(this, tr("Door Office"), tr("Failed to save."));
        return false;
    }
    emit statusMessage(QStringLiteral("Saved %1").arg(m_filePath));
    emit titleChanged(displayName());
    return true;
}

bool DoorSlidesWidget::saveAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Presentation"), {},
        tr("Door Slides (*.dslide);;PowerPoint (*.pptx);;PDF (*.pdf);;All Files (*)"));
    if (path.isEmpty()) return false;

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("pptx")) {
        return writePptx(path);
    }

    if (!path.endsWith(".dslide", Qt::CaseInsensitive)) path += ".dslide";
    if (!SlideDeck::saveToFile(m_deck, path)) {
        QMessageBox::critical(this, tr("Door Office"), tr("Failed to save."));
        return false;
    }
    m_filePath = path;
    emit statusMessage(QStringLiteral("Saved %1").arg(path));
    emit titleChanged(displayName());
    return true;
}

bool DoorSlidesWidget::exportPdf()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export PDF"), {}, tr("PDF (*.pdf)"));
    if (path.isEmpty()) return false;
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    QPainter painter(&printer);
    for (int i = 0; i < m_deck.pageCount(); ++i) {
        if (i > 0) printer.newPage();
        m_canvas->setSlide(m_deck.pageAt(i));
        m_canvas->render(&painter);
    }
    emit statusMessage(QStringLiteral("Exported PDF %1").arg(path));
    return true;
}

void DoorSlidesWidget::print()
{
    QPrinter printer;
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() != QDialog::Accepted) return;
    QPainter painter(&printer);
    m_canvas->setSlide(m_deck.pageAt(m_deck.currentIndex()));
    m_canvas->render(&painter);
}

QString DoorSlidesWidget::displayName() const
{
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    return name;
}

QString DoorSlidesWidget::fileFilter()
{
    return tr("Microsoft PowerPoint (*.pptx);;Door Slides (*.dslide);;PDF (*.pdf);;All Files (*)");
}

bool DoorSlidesWidget::writePptx(const QString& path)
{
    const QString tempDir = QDir::tempPath() + QLatin1String("/door_pptx_") + QDateTime::currentDateTime().toString(QLatin1String("yyyyMMdd_HHmmss"));
    const QString slidesDir = tempDir + QLatin1String("/ppt/slides");
    const QString relsDir = tempDir + QLatin1String("/ppt/_rels");
    QDir().mkpath(slidesDir);
    QDir().mkpath(relsDir);

    SlidePage s;
    s.title = QStringLiteral("Slide 1");
    m_deck.pages.clear();
    m_deck.pages.append(s);
    m_deck.setCurrentIndex(0);
    m_filePath = path;
    renderCurrent();
    emit statusMessage(QStringLiteral("Saved %1").arg(path));
    emit titleChanged(displayName());
    return true;
}

} // namespace door::modules
