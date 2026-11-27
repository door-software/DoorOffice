// Door Office v1.0 — Left filmstrip + studio toolbar + canvas widget
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QSizePolicy>
#include "modules/SlideModel.h"

namespace door::modules {

// ---------- internal canvas ------------------------------------------------
class SlideCanvas final : public QWidget
{
    Q_OBJECT
public:
    explicit SlideCanvas(QWidget* parent = nullptr) : QWidget(parent) {}
    void setSlide(const SlidePage& slide) { m_slide = slide; }
    [[nodiscard]] const SlidePage& slide() const { return m_slide; }

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

signals:
    void itemRectChanged(int index, const QRectF& normalizedRect);

private:
    QRectF viewAreaIn(const QRectF& region) const;
    QRectF itemViewRect(const SlideItem& it, const QRectF& area) const;
    int itemAt(const QPointF& pos, const QRectF& area) const;

    SlidePage m_slide;
    int m_selectedIndex = -1;
    bool m_dragging = false;
    bool m_resizing = false;
    QPointF m_dragStartPos;
    QRectF m_dragStartNorm;   // item rect (normalized) captured at drag start
    QRectF m_viewArea;        // cached 16:9 drawing area inside the widget
    static constexpr int kHandle = 10;
    static constexpr qreal kMinItemSize = 0.02;
    static constexpr qreal kMaxItemSize = 0.98;
};

// ---------- top studio toolbar -------------------------------------------
class DoorSlidesWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit DoorSlidesWidget(QWidget* parent = nullptr);
    ~DoorSlidesWidget() override;

    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] QString displayName() const;

    bool openFile(const QString& path);
    bool save();
    bool saveAs();
    bool exportPdf();
    void print();

    [[nodiscard]] static QString fileFilter();

signals:
    void statusMessage(const QString& message, int timeoutMs = 3000);
    void titleChanged(const QString& title);

private slots:
    void addSlide();
    void duplicateSlide();
    void moveSlide(int row, bool up);
    void deleteSlide();
    void applyTheme(int index);
    void chooseBackground();
    void insertShape();
    void insertImage();
    void onNotesChanged();

private:
    bool writePptx(const QString& path);
    void buildUi();
    void renderCurrent();

    SlideDeck m_deck;
    SlideCanvas* m_canvas = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QPushButton* m_bgBtn = nullptr;
    QTextEdit* m_notes = nullptr;
    QString m_filePath;
};

} // namespace door::modules