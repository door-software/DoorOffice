// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/MainWindow.h"

#include "core/ConfigManager.h"
#include "modules/DoorWriteWidget.h"
#include "modules/DoorSheetsWidget.h"
#include "modules/DoorSlidesWidget.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QTableWidget>

namespace door::core {

namespace {

// "Door Midnight Dark" — deep navy surfaces with an electric-violet accent.
constexpr auto kMidnightDarkQss = R"QSS(
* { font-family: "Segoe UI"; font-size: 10pt; }
QMainWindow, QDialog { background: #0f1221; color: #e4e7f2; }
QWidget { color: #e4e7f2; }
QToolTip { background: #1b2038; color: #e4e7f2; border: 1px solid #7c5cff; padding: 4px; }

QMenuBar { background: #0b0e1a; border-bottom: 1px solid #1f2540; padding: 2px; }
QMenuBar::item { background: transparent; padding: 5px 12px; border-radius: 4px; }
QMenuBar::item:selected { background: #232a4a; }
QMenu { background: #151a2e; border: 1px solid #2a3155; padding: 4px; }
QMenu::item { padding: 6px 28px 6px 20px; border-radius: 4px; }
QMenu::item:selected { background: #7c5cff; color: #ffffff; }
QMenu::item:disabled { color: #5a6283; }
QMenu::separator { height: 1px; background: #2a3155; margin: 4px 8px; }

QTabWidget::pane { border: none; background: #0f1221; }
QTabBar { background: #0b0e1a; }
QTabBar::tab { background: #121629; color: #9aa4c0; padding: 8px 18px; margin-right: 2px;
               border-top-left-radius: 6px; border-top-right-radius: 6px; min-width: 90px; }
QTabBar::tab:selected { background: #1b2038; color: #ffffff; border-bottom: 2px solid #7c5cff; }
QTabBar::tab:hover:!selected { background: #182039; color: #e4e7f2; }

QToolBar { background: #151a2e; border: none; border-bottom: 1px solid #1f2540; padding: 4px; spacing: 4px; }
QToolBar::separator { background: #2a3155; width: 1px; margin: 4px 6px; }
QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px;
              padding: 4px 8px; color: #e4e7f2; }
QToolButton:hover { background: #232a4a; border-color: #2f3760; }
QToolButton:checked { background: #7c5cff; color: #ffffff; }
QToolButton:pressed { background: #5b3fe0; }

QComboBox, QFontComboBox, QLineEdit { background: #0f1221; border: 1px solid #2a3155; border-radius: 4px;
                                      padding: 4px 8px; selection-background-color: #7c5cff; }
QComboBox:hover, QLineEdit:hover { border-color: #3c4675; }
QComboBox:focus, QLineEdit:focus { border-color: #7c5cff; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox QAbstractItemView { background: #151a2e; border: 1px solid #2a3155;
                              selection-background-color: #7c5cff; outline: none; }

QTextEdit#DoorWriteEditor { background: #f7f8fc; color: #1a1d2b; border: none;
                            selection-background-color: #7c5cff; selection-color: #ffffff;
                            padding: 32px 48px; }

QCheckBox { spacing: 6px; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #3c4675; border-radius: 3px; background: #0f1221; }
QCheckBox::indicator:checked { background: #7c5cff; border-color: #7c5cff; }

QWidget#DoorFindBar { background: #151a2e; border-top: 1px solid #1f2540; }

QStatusBar { background: #0b0e1a; color: #9aa4c0; border-top: 1px solid #1f2540; }
QStatusBar QLabel { padding: 0 8px; }

QScrollBar:vertical { background: #0f1221; width: 12px; margin: 0; }
QScrollBar::handle:vertical { background: #2a3155; border-radius: 5px; min-height: 30px; margin: 2px; }
QScrollBar::handle:vertical:hover { background: #7c5cff; }
QScrollBar:horizontal { background: #0f1221; height: 12px; margin: 0; }
QScrollBar::handle:horizontal { background: #2a3155; border-radius: 5px; min-width: 30px; margin: 2px; }
QScrollBar::handle:horizontal:hover { background: #7c5cff; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

/* Home launcher */
QWidget#DoorHome { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0f1221, stop:1 #1a1240); }
QLabel#DoorHomeTitle { font-size: 30pt; font-weight: 600; color: #ffffff; }
QLabel#DoorHomeTagline { font-size: 12pt; color: #b7a8ff; }
QLabel#DoorBadge { font-size: 9pt; font-weight: 600; color: #ffffff; border-radius: 10px; padding: 4px 12px; }
QLabel#DoorBadge[portable="true"] { background: #0fa37f; }
QLabel#DoorBadge[portable="false"] { background: #3d5afe; }
QLabel#DoorHomePath { color: #6c7699; font-size: 9pt; }
QPushButton#DoorLaunch { background: #1b2038; border: 1px solid #2f3760; border-radius: 12px;
                         padding: 22px 28px; font-size: 12pt; font-weight: 600; text-align: left; min-width: 220px; }
QPushButton#DoorLaunch:hover { background: #262d52; border-color: #7c5cff; }
QPushButton#DoorLaunch:pressed { background: #7c5cff; }

QPushButton { background: #232a4a; border: 1px solid #2f3760; border-radius: 4px; padding: 6px 16px; }
QPushButton:hover { background: #2c3460; border-color: #7c5cff; }
QPushButton:default { border-color: #7c5cff; }
)QSS";

} // namespace

QString MainWindow::windowTitleText()
{
    return QStringLiteral("Door Office — Opens the door to a new world of technology");
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(windowTitleText());
    setObjectName(QStringLiteral("DoorMainWindow"));
    // Executable icon is embedded via resources/DoorOffice.rc; this makes the
    // same icon appear on the window/taskbar at runtime.
    setWindowIcon(QIcon(QStringLiteral(":/resources/DoorOffice.ico")));
    resize(1280, 820);
    setMinimumSize(800, 520);

    applyTheme();

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->addTab(buildHomeTab(), tr("Home"));
    // The Home launcher is permanent: no close button.
    m_tabs->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    m_tabs->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);

    buildMenus();

    const auto& cfg = ConfigManager::instance();
    auto* modeLabel = new QLabel(tr("%1 mode").arg(cfg.modeName()), this);
    modeLabel->setToolTip(QDir::toNativeSeparators(cfg.rootPath()));
    statusBar()->addPermanentWidget(modeLabel);
    statusBar()->showMessage(tr("Welcome to Door Office %1").arg(QStringLiteral(DOOR_OFFICE_VERSION)), 4000);

    onCurrentTabChanged(0);
}

void MainWindow::applyTheme()
{
    qApp->setStyle(QStringLiteral("Fusion"));
    qApp->setStyleSheet(QString::fromUtf8(kMidnightDarkQss));
}

QWidget* MainWindow::buildHomeTab()
{
    const auto& cfg = ConfigManager::instance();

    auto* home = new QWidget(this);
    home->setObjectName(QStringLiteral("DoorHome"));
    home->setAttribute(Qt::WA_StyledBackground, true);

    auto* title = new QLabel(QStringLiteral("Door Office"), home);
    title->setObjectName(QStringLiteral("DoorHomeTitle"));
    auto* tagline = new QLabel(tr("Opens the door to a new world of technology"), home);
    tagline->setObjectName(QStringLiteral("DoorHomeTagline"));

    // Home Launcher badge: the door logo shown above the product name.
    auto* logoBadge = new QLabel(home);
    logoBadge->setObjectName(QStringLiteral("DoorHomeLogo"));
    logoBadge->setAlignment(Qt::AlignCenter);
    logoBadge->setPixmap(QPixmap(QStringLiteral(":/resources/door_logo_clean.png"))
                             .scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoBadge->setToolTip(windowTitleText());

    auto* badge = new QLabel(cfg.isPortable() ? tr("PORTABLE MODE") : tr("APPDATA MODE"), home);
    badge->setObjectName(QStringLiteral("DoorBadge"));
    badge->setProperty("portable", cfg.isPortable());
    badge->setToolTip(cfg.isPortable() ? tr("portable.txt found — data stored next to DoorOffice.exe")
                                       : tr("Data stored in your %AppData% profile"));
    auto* path = new QLabel(QDir::toNativeSeparators(cfg.rootPath()), home);
    path->setObjectName(QStringLiteral("DoorHomePath"));
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* newBtn = new QPushButton(tr("📄  New Document\nStart a blank Door Write page"), home);
    newBtn->setObjectName(QStringLiteral("DoorLaunch"));
    newBtn->setCursor(Qt::PointingHandCursor);
    auto* newSheetBtn = new QPushButton(tr("📊  New Spreadsheet\nCreate a new Door Sheets document"), home);
    newSheetBtn->setObjectName(QStringLiteral("DoorLaunch"));
    newSheetBtn->setCursor(Qt::PointingHandCursor);
    auto* newSlidesBtn = new QPushButton(tr("🖼️  New Presentation\nCreate a new Door Slides deck"), home);
    newSlidesBtn->setObjectName(QStringLiteral("DoorLaunch"));
    newSlidesBtn->setCursor(Qt::PointingHandCursor);
    auto* openBtn = new QPushButton(tr("📂  Open File\n.ddoc, .html, .txt, .dsheet, .csv or .dslide"), home);
    openBtn->setObjectName(QStringLiteral("DoorLaunch"));
    openBtn->setCursor(Qt::PointingHandCursor);
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::newDocument);
    connect(newSheetBtn, &QPushButton::clicked, this, &MainWindow::newSpreadsheet);
    connect(newSlidesBtn, &QPushButton::clicked, this, &MainWindow::newPresentation);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openFile);

    auto* badgeRow = new QHBoxLayout;
    badgeRow->addWidget(badge);
    badgeRow->addWidget(path);
    badgeRow->addStretch();

    auto* cards = new QGridLayout;
    cards->setSpacing(16);
    cards->addWidget(newBtn, 0, 0);
    cards->addWidget(newSheetBtn, 0, 1);
    cards->addWidget(newSlidesBtn, 0, 2);

    auto* openRow = new QHBoxLayout;
    openRow->addWidget(openBtn);
    openRow->addStretch();

    auto* column = new QVBoxLayout;
    column->setSpacing(10);
    column->addWidget(logoBadge);
    column->addWidget(title);
    column->addWidget(tagline);
    column->addSpacing(8);
    column->addLayout(badgeRow);
    column->addSpacing(28);
    column->addLayout(cards);
    column->addSpacing(18);
    column->addLayout(openRow);
    column->addStretch();

    auto* outer = new QVBoxLayout(home);
    outer->setContentsMargins(64, 56, 64, 56);
    outer->addLayout(column);
    outer->addStretch();
    return home;
}

template <typename Fn>
void MainWindow::withWriter(Fn&& fn)
{
    if (auto* writer = currentWriter())
        fn(writer);
    else
        statusBar()->showMessage(tr("Open or create a document first."), 3000);
}

// Runs fn on the active Door Sheets tab, or shows a hint if none is active.
template <typename Fn>
void MainWindow::withSheet(Fn&& fn)
{
    if (auto* sheet = currentSheet())
        fn(sheet);
    else
        statusBar()->showMessage(tr("Open or create a spreadsheet first."), 3000);
}

void MainWindow::buildMenus()
{
    using modules::DoorWriteWidget;
    const auto docAction = [this](QMenu* menu, const QString& text, auto&& fn, const QKeySequence& key = {}) {
        QAction* act = menu->addAction(text);
        if (!key.isEmpty())
            act->setShortcut(key);
        connect(act, &QAction::triggered, this, [this, fn] { withWriter(fn); });
        m_docActions.append(act);
        return act;
    };
    
    const auto sheetAction = [this](QMenu* menu, const QString& text, auto&& fn, const QKeySequence& key = {}) {
        QAction* act = menu->addAction(text);
        if (!key.isEmpty())
            act->setShortcut(key);
        connect(act, &QAction::triggered, this, [this, fn] { withSheet(fn); });
        m_docActions.append(act);
        return act;
    };

        // File ------------------------------------------------------------------
    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New Document"), QKeySequence::New, this, &MainWindow::newDocument);
    file->addAction(tr("New &Spreadsheet"), this, &MainWindow::newSpreadsheet);
    file->addAction(tr("New &Presentation"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P), this, &MainWindow::newPresentation);
    file->addAction(tr("&Open…"), QKeySequence::Open, this, &MainWindow::openFile);
    file->addSeparator();
    docAction(file, tr("&Save"), [](DoorWriteWidget* w) { w->save(); }, QKeySequence::Save);
    docAction(file, tr("Save &As…"), [](DoorWriteWidget* w) { w->saveAs(); }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    sheetAction(file, tr("Save Sheet"), [](modules::DoorSheetsWidget* s) { s->save(); }, QKeySequence::Save);
    sheetAction(file, tr("Save Sheet As…"), [](modules::DoorSheetsWidget* s) { s->saveAs(); }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    file->addSeparator();
    docAction(file, tr("&Export to PDF…"), [](DoorWriteWidget* w) { w->exportPdf(); }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    sheetAction(file, tr("Export Sheet to PDF…"), [](modules::DoorSheetsWidget* s) { s->exportPdf(); }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    docAction(file, tr("&Print…"), [](DoorWriteWidget* w) { w->print(); }, QKeySequence::Print);
    sheetAction(file, tr("Print Sheet…"), [](modules::DoorSheetsWidget* s) { s->print(); }, QKeySequence::Print);
    file->addSeparator();
    docAction(file, tr("&Close Tab"), [this](DoorWriteWidget* w) { closeTab(m_tabs->indexOf(w)); }, QKeySequence(Qt::CTRL | Qt::Key_W));
    sheetAction(file, tr("Close Sheet Tab"), [this](modules::DoorSheetsWidget* s) { closeTab(m_tabs->indexOf(s)); }, QKeySequence(Qt::CTRL | Qt::Key_W));
    file->addAction(tr("E&xit"), QKeySequence(Qt::ALT | Qt::Key_F4), this, &QWidget::close);

    // Edit ------------------------------------------------------------------
    // QTextEdit handles the standard editing keys itself; the "\t..." suffix only displays the hint.
    QMenu* edit = menuBar()->addMenu(tr("&Edit"));
    docAction(edit, tr("&Undo\tCtrl+Z"), [](DoorWriteWidget* w) { w->editor()->undo(); });
    docAction(edit, tr("&Redo\tCtrl+Y"), [](DoorWriteWidget* w) { w->editor()->redo(); });
    edit->addSeparator();
    docAction(edit, tr("Cu&t\tCtrl+X"), [](DoorWriteWidget* w) { w->editor()->cut(); });
    docAction(edit, tr("&Copy\tCtrl+C"), [](DoorWriteWidget* w) { w->editor()->copy(); });
    docAction(edit, tr("&Paste\tCtrl+V"), [](DoorWriteWidget* w) { w->editor()->paste(); });
    docAction(edit, tr("Select &All\tCtrl+A"), [](DoorWriteWidget* w) { w->editor()->selectAll(); });
    edit->addSeparator();
    // Ctrl+F is owned by the Door Write toolbar (widget-scoped) to avoid ambiguous shortcuts.
    docAction(edit, tr("&Find…\tCtrl+F"), [](DoorWriteWidget* w) { w->showFindBar(); });

    // Format ----------------------------------------------------------------
    QMenu* format = menuBar()->addMenu(tr("F&ormat"));
    docAction(format, tr("&Bold\tCtrl+B"), [](DoorWriteWidget* w) { w->boldAction()->trigger(); });
    docAction(format, tr("&Italic\tCtrl+I"), [](DoorWriteWidget* w) { w->italicAction()->trigger(); });
    docAction(format, tr("&Underline\tCtrl+U"), [](DoorWriteWidget* w) { w->underlineAction()->trigger(); });
    format->addSeparator();
    docAction(format, tr("Text &Color…"), [](DoorWriteWidget* w) { w->chooseTextColor(); });

    // Plugins ---------------------------------------------------------------
    QMenu* plugins = menuBar()->addMenu(tr("&Plugins"));
    QAction* noPlugins = plugins->addAction(tr("No plugins loaded (coming in a later stage)"));
    noPlugins->setEnabled(false);
    plugins->addSeparator();
    plugins->addAction(tr("Open &Plugins Folder"), this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ConfigManager::instance().pluginsPath()));
    });

    // Settings --------------------------------------------------------------
    QMenu* settings = menuBar()->addMenu(tr("&Settings"));
    
    // AutoCorrect action
    QAction* autoCorrectAct = settings->addAction(tr("AutoCorrect & Auto-Capitalize"));
    autoCorrectAct->setCheckable(true);
    autoCorrectAct->setChecked(ConfigManager::instance().value(QStringLiteral("autoCorrect"), true).toBool());
    connect(autoCorrectAct, &QAction::toggled, this, [](bool enabled) {
        auto& cfg = ConfigManager::instance();
        cfg.setValue(QStringLiteral("autoCorrect"), enabled);
        cfg.save();
    });
    
    settings->addSeparator();
    QAction* themeAct = settings->addAction(tr("Theme: Door Midnight Dark"));
    themeAct->setCheckable(true);
    themeAct->setChecked(true);
    themeAct->setEnabled(false);
    settings->addSeparator();
    const auto openFolder = [this, settings](const QString& text, QString (ConfigManager::*getter)() const) {
        settings->addAction(text, this, [getter] {
            QDesktopServices::openUrl(QUrl::fromLocalFile((ConfigManager::instance().*getter)()));
        });
    };
    openFolder(tr("Open &Themes Folder"), &ConfigManager::themesPath);
    openFolder(tr("Open T&emplates Folder"), &ConfigManager::templatesPath);
    openFolder(tr("Edit settings.&json"), &ConfigManager::settingsFile);
    settings->addAction(tr("Open &Data Folder"), this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(ConfigManager::instance().rootPath()));
    });

    // Help ------------------------------------------------------------------
    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About Door Office"), this, [this] {
        const auto& cfg = ConfigManager::instance();
        QMessageBox::about(this, tr("About Door Office"),
            tr("<h2>Door Office %1</h2>"
               "<p><i>Opens the door to a new world of technology.</i></p>"
               "<p>Storage: <b>%2</b> mode<br><code>%3</code></p>"
               "<p>This program is free software: you can redistribute it and/or modify it under the terms "
               "of the GNU General Public License as published by the Free Software Foundation, either "
               "version 3 of the License, or (at your option) any later version.</p>"
               "<p>This program is distributed WITHOUT ANY WARRANTY. See the GNU GPLv3 for details.</p>")
                .arg(QStringLiteral(DOOR_OFFICE_VERSION), cfg.modeName(),
                     QDir::toNativeSeparators(cfg.rootPath()).toHtmlEscaped()));
    });
    help->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

// ---------------------------------------------------------------------------
// Tabs & documents
// ---------------------------------------------------------------------------
modules::DoorWriteWidget* MainWindow::currentWriter() const
{
    return qobject_cast<modules::DoorWriteWidget*>(m_tabs->currentWidget());
}

void MainWindow::addWriter(modules::DoorWriteWidget* writer)
{
    const int index = m_tabs->addTab(writer, writer->displayName());
    m_tabs->setTabToolTip(index, tr("Door Write"));
    connect(writer, &modules::DoorWriteWidget::titleChanged, this, [this, writer](const QString& title) {
        const int i = m_tabs->indexOf(writer);
        if (i >= 0) {
            m_tabs->setTabText(i, title);
            m_tabs->setTabToolTip(i, writer->filePath().isEmpty() ? tr("Door Write")
                                                                   : QDir::toNativeSeparators(writer->filePath()));
        }
    });
    connect(writer, &modules::DoorWriteWidget::statusMessage, this,
            [this](const QString& msg, int timeout) { statusBar()->showMessage(msg, timeout); });
    m_tabs->setCurrentIndex(index);
    writer->editor()->setFocus();
}

modules::DoorSheetsWidget* MainWindow::currentSheet() const
{
    return qobject_cast<modules::DoorSheetsWidget*>(m_tabs->currentWidget());
}

void MainWindow::addSheet(modules::DoorSheetsWidget* sheet)
{
    const int index = m_tabs->addTab(sheet, sheet->displayName());
    m_tabs->setTabToolTip(index, tr("Door Sheets"));
    connect(sheet, &modules::DoorSheetsWidget::titleChanged, this, [this, sheet](const QString& title) {
        const int i = m_tabs->indexOf(sheet);
        if (i >= 0) {
            m_tabs->setTabText(i, title);
            m_tabs->setTabToolTip(i, sheet->filePath().isEmpty() ? tr("Door Sheets")
                                                               : QDir::toNativeSeparators(sheet->filePath()));
        }
    });
    connect(sheet, &modules::DoorSheetsWidget::statusMessage, this,
            [this](const QString& msg, int timeout) { statusBar()->showMessage(msg, timeout); });
    m_tabs->setCurrentIndex(index);
    sheet->table()->setFocus();
}

void MainWindow::addSlides(modules::DoorSlidesWidget* slides)
{
    const int index = m_tabs->addTab(slides, slides->displayName());
    m_tabs->setTabToolTip(index, tr("Door Slides"));
    connect(slides, &modules::DoorSlidesWidget::titleChanged, this, [this, slides](const QString& title) {
        const int i = m_tabs->indexOf(slides);
        if (i >= 0) {
            m_tabs->setTabText(i, title);
            m_tabs->setTabToolTip(i, slides->filePath().isEmpty() ? tr("Door Slides")
                                                                  : QDir::toNativeSeparators(slides->filePath()));
        }
    });
    connect(slides, &modules::DoorSlidesWidget::statusMessage, this,
            [this](const QString& msg, int timeout) { statusBar()->showMessage(msg, timeout); });
    m_tabs->setCurrentIndex(index);
}

void MainWindow::newSpreadsheet()
{
    auto* sheet = new modules::DoorSheetsWidget(m_tabs);
    addSheet(sheet);
    statusBar()->showMessage(tr("New spreadsheet created"), 2000);
}

void MainWindow::newPresentation()
{
    auto* slides = new modules::DoorSlidesWidget(m_tabs);
    addSlides(slides);
    statusBar()->showMessage(tr("New presentation created"), 2000);
}

void MainWindow::newDocument()
{
    addWriter(new modules::DoorWriteWidget(m_tabs));
    statusBar()->showMessage(tr("New document created"), 2000);
}

void MainWindow::openFile()
{
    const QString dir = ConfigManager::instance().value(QStringLiteral("lastDirectory")).toString();
    // Combine filters for Door Write, Door Sheets, and Door Slides (Microsoft Office formats first).
    QString allFilters = QStringLiteral("Microsoft Word (*.docx);;OpenDocument (*.odt);;Door Documents (*.ddoc);;")
                       + modules::DoorWriteWidget::fileFilter() + QStringLiteral(";;")
                       + modules::DoorSheetsWidget::fileFilter() + QStringLiteral(";;")
                       + modules::DoorSlidesWidget::fileFilter();
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Open File"), dir, allFilters);
    for (const QString& path : paths)
        openPath(path);
}

bool MainWindow::openPath(const QString& path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();

    // Already open? Just switch to it.
    for (int i = 0; i < m_tabs->count(); ++i) {
        // Check for DoorWriteWidget
        auto* w = qobject_cast<modules::DoorWriteWidget*>(m_tabs->widget(i));
        if (w && !canonical.isEmpty() && QFileInfo(w->filePath()).canonicalFilePath() == canonical) {
            m_tabs->setCurrentIndex(i);
            return true;
        }
        // Check for DoorSheetsWidget
        auto* s = qobject_cast<modules::DoorSheetsWidget*>(m_tabs->widget(i));
        if (s && !canonical.isEmpty() && QFileInfo(s->filePath()).canonicalFilePath() == canonical) {
            m_tabs->setCurrentIndex(i);
            return true;
        }
    }

    // Determine file type by extension
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("dslide") || suffix == QLatin1String("pptx")) {
        // Door Slides / Microsoft PowerPoint
        auto* slides = new modules::DoorSlidesWidget(m_tabs);
        Q_UNUSED(QFileInfo(path).canonicalFilePath());
        if (!slides->openFile(path)) {
            slides->deleteLater();
            return false;
        }
        addSlides(slides);
    } else if (suffix == QLatin1String("dsheet") || suffix == QLatin1String("csv")
               || suffix == QLatin1String("xlsx")) {
        // Door Sheets / spreadsheet file
        auto* sheet = new modules::DoorSheetsWidget(m_tabs);
        if (!sheet->openFile(path)) {
            sheet->deleteLater();
            return false;
        }
        addSheet(sheet);
    } else {
        // Door Write / Word / OpenDocument / text
        auto* writer = new modules::DoorWriteWidget(m_tabs);
        if (!writer->openFile(path)) {
            writer->deleteLater();
            return false;
        }
        addWriter(writer);
    }
    return true;
}

void MainWindow::closeTab(int index)
{
    // Home tab (or invalid index) cannot be closed
    if (index == 0)
        return;

    // Try DoorWriteWidget first
    auto* writer = qobject_cast<modules::DoorWriteWidget*>(m_tabs->widget(index));
    if (writer) {
        m_tabs->setCurrentIndex(index);
        if (!writer->maybeSave())
            return;
        m_tabs->removeTab(index);
        writer->deleteLater();
        return;
    }
    
    // Try DoorSheetsWidget
    auto* sheet = qobject_cast<modules::DoorSheetsWidget*>(m_tabs->widget(index));
    if (sheet) {
        m_tabs->setCurrentIndex(index);
        if (!sheet->maybeSave())
            return;
        m_tabs->removeTab(index);
        sheet->deleteLater();
        return;
    }
    
    // Try DoorSlidesWidget
    auto* slides = qobject_cast<modules::DoorSlidesWidget*>(m_tabs->widget(index));
    if (slides) {
        m_tabs->setCurrentIndex(index);
        m_tabs->removeTab(index);
        slides->deleteLater();
        return;
    }

    // Other widget or invalid index cannot be closed
}

void MainWindow::onCurrentTabChanged(int /*index*/)
{
    const bool hasDoc = currentWriter() != nullptr || currentSheet() != nullptr;
    for (QAction* act : std::as_const(m_docActions))
        act->setEnabled(hasDoc);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (int i = m_tabs->count() - 1; i >= 0; --i) {
        // Check for DoorWriteWidget
        auto* writer = qobject_cast<modules::DoorWriteWidget*>(m_tabs->widget(i));
        if (writer && writer->isModified()) {
            m_tabs->setCurrentIndex(i);
            if (!writer->maybeSave()) {
                event->ignore();
                return;
            }
        }
        // Check for DoorSheetsWidget
        auto* sheet = qobject_cast<modules::DoorSheetsWidget*>(m_tabs->widget(i));
        if (sheet && sheet->isModified()) {
            m_tabs->setCurrentIndex(i);
            if (!sheet->maybeSave()) {
                event->ignore();
                return;
            }
        }
    }
    ConfigManager::instance().save();
    event->accept();
}

} // namespace door::core
