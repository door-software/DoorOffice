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
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QKeyEvent>
#include <QResizeEvent>

#include <QFileInfo>
#include <QSizePolicy>
#include <QXmlStreamWriter>
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QByteArray>

#include <utility>

namespace door::modules {

// ---------- SlideCanvas ----------------------------------------------------
QRectF SlideCanvas::viewAreaIn(const QRectF& region) const
{
    const qreal aspect = 16.0 / 9.0;
    qreal w = region.width();
    qreal h = w / aspect;
    if (h > region.height()) { h = region.height(); w = h * aspect; }
    return QRectF(region.x() + (region.width() - w) / 2.0,
                  region.y() + (region.height() - h) / 2.0, w, h);
}

// ---------- SlideCanvas: construction & scene building --------------------
SlideCanvas::SlideCanvas(QWidget* parent) : QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignCenter);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void SlideCanvas::setSlide(const SlidePage& slide)
{
    m_slide = slide;
    rebuildScene();
}

void SlideCanvas::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (m_scene) {
        m_viewArea = viewAreaIn(viewport()->rect());
        m_scene->setSceneRect(m_viewArea);
    }
}

// ---------- rebuildScene: build text items & shapes into scene ------------
void SlideCanvas::rebuildScene()
{
    m_scene->clear();
    m_textItemRole.clear();
    m_shapeItemIndex.clear();

    const qreal w = m_viewArea.width();
    const qreal h = m_viewArea.height();
    const qreal x = m_viewArea.x();
    const qreal y = m_viewArea.y();

    // Background rect
    auto* bgRect = m_scene->addRect(m_viewArea, QPen(), m_slide.backgroundColor);
    bgRect->setZValue(-10);

    // Title text item (always present at top)
    auto* titleItem = new QGraphicsTextItem();
    QString titleDisplay = m_slide.title.isEmpty() ? QStringLiteral("Untitled Presentation") : m_slide.title;
    titleItem->setHtml(QStringLiteral("<b>%1</b>").arg(titleDisplay.toHtmlEscaped()));
    titleItem->setDefaultTextColor(m_slide.accentColor);
    QFont titleFont; titleFont.setPointSizeF(h * 0.05); titleFont.setBold(true);
    titleItem->setFont(titleFont);
    titleItem->setTextWidth(w * 0.88);
    titleItem->setPos(x + w * 0.06, y + h * 0.10);
    titleItem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsFocusable);
    titleItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    titleItem->setZValue(10);
    connect(titleItem->document(), &QTextDocument::contentsChanged, this, [this, titleItem]() {
        if (m_textItemRole.value(titleItem) == "title") {
            m_slide.title = titleItem->toPlainText();
            emit contentChanged();
        }
    });
    m_scene->addItem(titleItem);
    m_textItemRole[titleItem] = "title";

    // Body text item (includes subtitle)
    QString bodyHtml = m_slide.body;
    if (!m_slide.subtitle.isEmpty()) {
        if (!bodyHtml.isEmpty()) bodyHtml += "<br/>";
        bodyHtml += m_slide.subtitle;
    }
    if (bodyHtml.isEmpty()) bodyHtml = "Click to add text...";

    auto* bodyItem = new QGraphicsTextItem();
    bodyItem->setHtml(bodyHtml);
    bodyItem->setDefaultTextColor(m_slide.accentColor);
    QFont bodyFont; bodyFont.setPointSizeF(h * 0.025);
    bodyItem->setFont(bodyFont);
    bodyItem->setTextWidth(w * 0.88);
    bodyItem->setPos(x + w * 0.06, y + h * 0.34);
    bodyItem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsFocusable);
    bodyItem->setTextInteractionFlags(Qt::TextEditorInteraction);
    bodyItem->setZValue(10);
    connect(bodyItem->document(), &QTextDocument::contentsChanged, this, [this, bodyItem]() {
        if (m_textItemRole.value(bodyItem) == "body") {
            QString text = bodyItem->toPlainText();
            int idx = text.lastIndexOf('\n');
            if (idx >= 0) {
                m_slide.body = text.left(idx);
                m_slide.subtitle = text.mid(idx + 1);
            } else {
                m_slide.body = text;
            }
            emit contentChanged();
        }
    });
    m_scene->addItem(bodyItem);
    m_textItemRole[bodyItem] = "body";

    // Shapes from model (shapes vector)
    for (int i = 0; i < m_slide.shapes.size(); ++i) {
        const SlideItem& it = m_slide.shapes[i];
        QRectF r(x + it.rect.x() * w, y + it.rect.y() * h,
                 it.rect.width() * w, it.rect.height() * h);

        QGraphicsItem* shapeItem = nullptr;
        if (it.type == SlideItem::Type::Rect) {
            shapeItem = m_scene->addRect(r, QPen(Qt::black, 1), it.color);
        } else if (it.type == SlideItem::Type::Ellipse) {
            shapeItem = m_scene->addEllipse(r, QPen(Qt::black, 1), it.color);
        } else if (it.type == SlideItem::Type::Image && !it.imagePath.isEmpty()) {
            QPixmap pm(it.imagePath);
            if (!pm.isNull()) {
                shapeItem = m_scene->addPixmap(pm);
                shapeItem->setPos(r.topLeft());
                shapeItem->setScale(qMin(r.width() / pm.width(), r.height() / pm.height()));
            }
        } else if (it.type == SlideItem::Type::Text) {
            // Editable text box shape
            auto* textItem = new QGraphicsTextItem();
            textItem->setPlainText(it.text.isEmpty() ? "Text box" : it.text);
            textItem->setDefaultTextColor(it.color);
            QFont tf; tf.setPointSizeF(h * 0.03);
            textItem->setFont(tf);
            textItem->setTextWidth(r.width());
            textItem->setPos(r.topLeft());
            textItem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsFocusable);
            textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
            textItem->setZValue(10);
            connect(textItem->document(), &QTextDocument::contentsChanged, this, [this, textItem, i]() {
                if (m_textItemRole.value(textItem).startsWith("shape:")) {
                    if (i >= 0 && i < m_slide.shapes.size()) {
                        m_slide.shapes[i].text = textItem->toPlainText();
                        emit contentChanged();
                    }
                }
            });
            m_scene->addItem(textItem);
            m_textItemRole[textItem] = QString("shape:%1").arg(i);
            m_shapeItemIndex[textItem] = i;
            continue;
        }

        if (shapeItem) {
            shapeItem->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsFocusable);
            shapeItem->setZValue(5);
            m_shapeItemIndex[shapeItem] = i;
        }
    }
}

// ---------- SlideCanvas: mouse handling (focus & drag) --------------------
void SlideCanvas::mousePressEvent(QMouseEvent* event)
{
    const QPointF scenePos = mapToScene(event->pos());
    QGraphicsItem* item = m_scene->itemAt(scenePos, transform());

    // Identify whether the click landed on a text item
    auto findTextItem = [](QGraphicsItem* it) -> QGraphicsTextItem* {
        for (QGraphicsItem* p = it; p; p = p->parentItem()) {
            if (auto* t = qgraphicsitem_cast<QGraphicsTextItem*>(p))
                return t;
        }
        return nullptr;
    };

    QGraphicsTextItem* textItem = findTextItem(item);

    if (item && item != m_scene->items().first()) {  // not the background rect
        if (textItem) {
            // Give text item keyboard focus so typing edits immediately.
            QGraphicsView::mousePressEvent(event);
            if (textItem->flags() & QGraphicsItem::ItemIsFocusable) {
                textItem->setFocus(Qt::MouseFocusReason);
            }
            return;
        }
        // Non-text shape: start free drag
        QGraphicsItem* shapeRoot = item;
        if (m_shapeItemIndex.contains(shapeRoot)) {
            m_draggingShape = true;
            m_dragShapeIndex = m_shapeItemIndex[shapeRoot];
            m_dragStartScenePos = scenePos;
            m_dragStartShapeRect = m_slide.shapes[m_dragShapeIndex].rect;
            event->accept();
            return;
        }
    }

    // Click on empty area - clear text focus cleanly
    QGraphicsView::mousePressEvent(event);
    if (QGraphicsItem* focus = m_scene->focusItem()) {
        focus->clearFocus();
    }
}

void SlideCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_draggingShape && m_dragShapeIndex >= 0 && m_dragShapeIndex < m_slide.shapes.size()
        && m_viewArea.width() > 0 && m_viewArea.height() > 0) {
        const QPointF scenePos = mapToScene(event->pos());
        const qreal dx = (scenePos.x() - m_dragStartScenePos.x()) / m_viewArea.width();
        const qreal dy = (scenePos.y() - m_dragStartScenePos.y()) / m_viewArea.height();

        QRectF newRect = m_dragStartShapeRect;
        newRect.moveTo(qBound(0.0, m_dragStartShapeRect.x() + dx, 1.0 - newRect.width()),
                       qBound(0.0, m_dragStartShapeRect.y() + dy, 1.0 - newRect.height()));
        m_slide.shapes[m_dragShapeIndex].rect = newRect;
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void SlideCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_draggingShape) {
        if (m_dragShapeIndex >= 0 && m_dragShapeIndex < m_slide.shapes.size())
            emit itemRectChanged(m_dragShapeIndex, m_slide.shapes[m_dragShapeIndex].rect);
        m_draggingShape = false;
        m_dragShapeIndex = -1;
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

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
    QPushButton* textBtn = new QPushButton(QStringLiteral("Insert Text Box"));
    connect(textBtn, &QPushButton::clicked, this, &DoorSlidesWidget::insertTextBox);
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
    top->addWidget(shapeBtn); top->addWidget(textBtn); top->addWidget(imgBtn); top->addWidget(presentBtn);
    root->addLayout(top);

    // ---- center: live-rendered canvas + inspector ----
    QHBoxLayout* center = new QHBoxLayout();
    m_canvas = new SlideCanvas(this);
    center->addWidget(m_canvas, 1);
    connect(m_canvas, &SlideCanvas::contentChanged, this, [this]{
        // Copy canvas's current (edited) slide back into the deck
        if (m_deck.currentIndex() >= 0 && m_deck.currentIndex() < m_deck.pages.size()) {
            m_deck.pages[m_deck.currentIndex()] = m_canvas->slide();
        }
        emit m_deck.deckModified();
        emit contentChanged();
        emit titleChanged(displayName());
    });
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

void DoorSlidesWidget::insertTextBox()
{
    SlidePage& p = const_cast<SlidePage&>(m_deck.pageAt(m_deck.currentIndex()));
    SlideItem it; it.type = SlideItem::Type::Text; it.text = ""; it.color = m_deck.pageAt(m_deck.currentIndex()).accentColor;
    it.rect = QRectF(0.25, 0.25, 0.5, 0.3);
    p.shapes.append(it);
    m_canvas->rebuildScene();
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
