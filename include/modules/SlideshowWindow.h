// Door Office v1.0 — Fullscreen slideshow window
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QWindow>

#include "modules/SlideModel.h"

namespace door::modules {

class SlideshowWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit SlideshowWindow(SlideDeck* deck, QWidget* parent = nullptr);
    ~SlideshowWindow() override;

    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

    void next();
    void prev();
    void exit();

signals:
    void closed();

private:
    SlideDeck* m_deck = nullptr;
    int m_index = 0;
};

} // namespace door::modules