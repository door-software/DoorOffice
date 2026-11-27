// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMainWindow>

class QTabWidget;

namespace door::modules { 
class DoorWriteWidget; 
class DoorSheetsWidget;
class DoorSlidesWidget;
}

namespace door::core {

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    static QString windowTitleText();

public slots:
    void newDocument();
    void newSpreadsheet();
    void newPresentation();
    void openFile();
    bool openPath(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void closeTab(int index);
    void onCurrentTabChanged(int index);

private:
    void applyTheme();
    void buildMenus();
    QWidget* buildHomeTab();
    void addWriter(modules::DoorWriteWidget* writer);
    void addSheet(modules::DoorSheetsWidget* sheet);
    void addSlides(modules::DoorSlidesWidget* slides);
    [[nodiscard]] modules::DoorWriteWidget* currentWriter() const;
    [[nodiscard]] modules::DoorSheetsWidget* currentSheet() const;

    // Runs fn on the active Door Write tab, or shows a hint if none is active.
    template <typename Fn>
    void withWriter(Fn&& fn);
    // Runs fn on the active Door Sheets tab, or shows a hint if none is active.
    template <typename Fn>
    void withSheet(Fn&& fn);

    QTabWidget* m_tabs = nullptr;
    QList<QAction*> m_docActions; // enabled only while a document tab is active
};

} // namespace door::core
