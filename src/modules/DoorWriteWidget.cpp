// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#include "modules/DoorWriteWidget.h"

#include "core/ConfigManager.h"

#include <QAction>
#include <QActionGroup>
#include <QBuffer>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSaveFile>
#include <QStringDecoder>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextList>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace door::modules {

namespace {

constexpr auto kDdocFormat = "DoorDocument";
constexpr int kDdocVersion = 1;

QString lastDirectory()
{
    return core::ConfigManager::instance().value(QStringLiteral("lastDirectory")).toString();
}

void rememberDirectory(const QString& filePath)
{
    auto& cfg = core::ConfigManager::instance();
    cfg.setValue(QStringLiteral("lastDirectory"), QFileInfo(filePath).absolutePath());
    cfg.save();
}

QAction* makeToggle(QToolBar* bar, const QString& text, const QString& tip, const QKeySequence& key)
{
    auto* act = bar->addAction(text);
    act->setToolTip(QStringLiteral("%1 (%2)").arg(tip, key.toString(QKeySequence::NativeText)));
    act->setShortcut(key);
    act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    act->setCheckable(true);
    return act;
}

} // namespace

DoorWriteWidget::DoorWriteWidget(QWidget* parent)
    : QWidget(parent)
{
    m_toolBar = new QToolBar(tr("Formatting"), this);
    m_toolBar->setObjectName(QStringLiteral("DoorWriteToolBar"));
    m_toolBar->setIconSize(QSize(16, 16));

    m_edit = new QTextEdit(this);
    m_edit->setObjectName(QStringLiteral("DoorWriteEditor"));
    m_edit->setAcceptRichText(true);
    m_edit->setTabStopDistance(40.0);
    m_edit->installEventFilter(this);

    const auto& cfg = core::ConfigManager::instance();
    QFont base(cfg.value(QStringLiteral("defaultFontFamily"), QStringLiteral("Segoe UI")).toString());
    base.setPointSize(cfg.value(QStringLiteral("defaultFontSize"), 12).toInt());
    m_edit->document()->setDefaultFont(base);

    buildToolBar();
    buildFindBar();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_toolBar);
    layout->addWidget(m_edit, 1);
    layout->addWidget(m_findBar);

    connect(m_edit, &QTextEdit::currentCharFormatChanged, this, &DoorWriteWidget::onCurrentCharFormatChanged);
    connect(m_edit, &QTextEdit::cursorPositionChanged, this, &DoorWriteWidget::onCursorPositionChanged);
    connect(m_edit->document(), &QTextDocument::modificationChanged, this,
            [this](bool) { emit titleChanged(displayName()); });

    onCurrentCharFormatChanged(m_edit->currentCharFormat());
    onCursorPositionChanged();
    m_edit->setFocus();
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------
void DoorWriteWidget::buildToolBar()
{
    // Font family / size
    m_fontCombo = new QFontComboBox(m_toolBar);
    m_fontCombo->setToolTip(tr("Font Family"));
    m_fontCombo->setCurrentFont(m_edit->document()->defaultFont());
    m_toolBar->addWidget(m_fontCombo);
    connect(m_fontCombo, &QFontComboBox::textActivated, this, [this](const QString& family) {
        QTextCharFormat fmt;
        fmt.setFontFamilies({family});
        mergeFormat(fmt);
    });

    m_sizeCombo = new QComboBox(m_toolBar);
    m_sizeCombo->setToolTip(tr("Font Size"));
    m_sizeCombo->setEditable(true);
    m_sizeCombo->setMinimumContentsLength(3);
    for (int size : QFontDatabase::standardSizes())
        m_sizeCombo->addItem(QString::number(size));
    m_toolBar->addWidget(m_sizeCombo);
    connect(m_sizeCombo, &QComboBox::textActivated, this, [this](const QString& text) {
        bool ok = false;
        const qreal size = text.toDouble(&ok);
        if (ok && size > 0 && size <= 400) {
            QTextCharFormat fmt;
            fmt.setFontPointSize(size);
            mergeFormat(fmt);
        }
    });

    m_toolBar->addSeparator();

    // Bold / Italic / Underline
    m_actBold = makeToggle(m_toolBar, QStringLiteral("B"), tr("Bold"), QKeySequence::Bold);
    m_actItalic = makeToggle(m_toolBar, QStringLiteral("I"), tr("Italic"), QKeySequence::Italic);
    m_actUnderline = makeToggle(m_toolBar, QStringLiteral("U"), tr("Underline"), QKeySequence::Underline);
    {
        QFont f = font();
        f.setBold(true);
        m_actBold->setFont(f);
        f.setBold(false);
        f.setItalic(true);
        m_actItalic->setFont(f);
        f.setItalic(false);
        f.setUnderline(true);
        m_actUnderline->setFont(f);
    }
    connect(m_actBold, &QAction::triggered, this, [this](bool on) {
        QTextCharFormat fmt;
        fmt.setFontWeight(on ? QFont::Bold : QFont::Normal);
        mergeFormat(fmt);
    });
    connect(m_actItalic, &QAction::triggered, this, [this](bool on) {
        QTextCharFormat fmt;
        fmt.setFontItalic(on);
        mergeFormat(fmt);
    });
    connect(m_actUnderline, &QAction::triggered, this, [this](bool on) {
        QTextCharFormat fmt;
        fmt.setFontUnderline(on);
        mergeFormat(fmt);
    });

    // Text color
    m_actColor = m_toolBar->addAction(tr("Color"));
    m_actColor->setToolTip(tr("Text Color"));
    connect(m_actColor, &QAction::triggered, this, &DoorWriteWidget::chooseTextColor);
    if (auto* btn = qobject_cast<QToolButton*>(m_toolBar->widgetForAction(m_actColor)))
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    m_toolBar->addSeparator();

    // Alignment
    auto* alignGroup = new QActionGroup(this);
    const auto addAlign = [&](const QString& text, const QString& tip, Qt::Alignment align, const QKeySequence& key) {
        auto* act = m_toolBar->addAction(text);
        act->setToolTip(QStringLiteral("%1 (%2)").arg(tip, key.toString(QKeySequence::NativeText)));
        act->setShortcut(key);
        act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        act->setCheckable(true);
        alignGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, align] { m_edit->setAlignment(align | Qt::AlignAbsolute); });
        return act;
    };
    m_actAlignLeft = addAlign(tr("Left"), tr("Align Left"), Qt::AlignLeft, QKeySequence(Qt::CTRL | Qt::Key_L));
    m_actAlignCenter = addAlign(tr("Center"), tr("Center"), Qt::AlignHCenter, QKeySequence(Qt::CTRL | Qt::Key_E));
    m_actAlignRight = addAlign(tr("Right"), tr("Align Right"), Qt::AlignRight, QKeySequence(Qt::CTRL | Qt::Key_R));
    m_actAlignJustify = addAlign(tr("Justify"), tr("Justify"), Qt::AlignJustify, QKeySequence(Qt::CTRL | Qt::Key_J));

    m_toolBar->addSeparator();

    // AutoCorrect action
    m_actAutoCorrect = m_toolBar->addAction(tr("AutoCorrect"));
    m_actAutoCorrect->setToolTip(tr("AutoCorrect & Smart Capitalization"));
    m_actAutoCorrect->setCheckable(true);
    const auto& cfg = core::ConfigManager::instance();
    m_actAutoCorrect->setChecked(cfg.value(QStringLiteral("autoCorrect"), true).toBool());
    connect(m_actAutoCorrect, &QAction::toggled, this, &DoorWriteWidget::onAutoCorrectToggled);

    // Lists
    m_listCombo = new QComboBox(m_toolBar);
    m_listCombo->setToolTip(tr("List Style"));
    m_listCombo->addItem(tr("No List"), 0);
    m_listCombo->addItem(tr("• Bullet"), int(QTextListFormat::ListDisc));
    m_listCombo->addItem(tr("○ Circle"), int(QTextListFormat::ListCircle));
    m_listCombo->addItem(tr("■ Square"), int(QTextListFormat::ListSquare));
    m_listCombo->addItem(tr("1. Numbered"), int(QTextListFormat::ListDecimal));
    m_listCombo->addItem(tr("a. Lower Alpha"), int(QTextListFormat::ListLowerAlpha));
    m_listCombo->addItem(tr("A. Upper Alpha"), int(QTextListFormat::ListUpperAlpha));
    m_listCombo->addItem(tr("i. Lower Roman"), int(QTextListFormat::ListLowerRoman));
    m_listCombo->addItem(tr("I. Upper Roman"), int(QTextListFormat::ListUpperRoman));
    m_toolBar->addWidget(m_listCombo);
    connect(m_listCombo, &QComboBox::activated, this,
            [this](int index) { applyList(m_listCombo->itemData(index).toInt()); });

    // Insert Image button (styled like the Color action)
    m_btnInsertImage = new QPushButton(tr("Insert Image"), m_toolBar);
    m_btnInsertImage->setToolTip(tr("Insert an image from file"));
    m_btnInsertImage->setCursor(Qt::PointingHandCursor);
    m_btnInsertImage->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: 1px solid transparent; border-radius: 4px;"
        " padding: 4px 8px; color: #e4e7f2; }"
        "QPushButton:hover { background: #232a4a; border-color: #2f3760; }"
        "QPushButton:pressed { background: #5b3fe0; }"));
    m_toolBar->addWidget(m_btnInsertImage);
    connect(m_btnInsertImage, &QPushButton::clicked, this, [this] {
        const QString startDir = core::ConfigManager::instance().value(QStringLiteral("lastDirectory")).toString();
        const QString path = QFileDialog::getOpenFileName(this, tr("Insert Image"), startDir,
                                                          tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
        if (path.isEmpty())
            return;
        QImage image(path);
        if (image.isNull()) {
            QMessageBox::warning(this, tr("Insert Image"),
                                 tr("Cannot load \"%1\" as an image.").arg(QFileInfo(path).fileName()));
            return;
        }
        // Scale proportionally when wider than 600px.
        if (image.width() > 600)
            image = image.scaledToWidth(600, Qt::SmoothTransformation);
        // Encode as PNG Base64 data URI.
        QByteArray png;
        {
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");
        }
        const QString dataUri = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
        QTextCursor cursor = m_edit->textCursor();
        cursor.beginEditBlock();
        cursor.insertImage(dataUri);
        cursor.endEditBlock();
        m_edit->setFocus();
    });

    m_toolBar->addSeparator();

    auto* actFind = m_toolBar->addAction(tr("Find"));
    actFind->setToolTip(tr("Find (Ctrl+F)"));
    actFind->setShortcut(QKeySequence::Find);
    actFind->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(actFind, &QAction::triggered, this, &DoorWriteWidget::showFindBar);

    // Register shortcut-bearing actions on this widget too, so their widget-scoped
    // shortcuts fire while the editor (a sibling of the toolbar) has focus.
    addActions({m_actBold, m_actItalic, m_actUnderline, m_actAlignLeft, m_actAlignCenter, m_actAlignRight,
                m_actAlignJustify, actFind});
}

void DoorWriteWidget::buildFindBar()
{
    m_findBar = new QWidget(this);
    m_findBar->setObjectName(QStringLiteral("DoorFindBar"));

    m_findEdit = new QLineEdit(m_findBar);
    m_findEdit->setPlaceholderText(tr("Find in document…"));
    m_findEdit->setClearButtonEnabled(true);

    auto* prev = new QToolButton(m_findBar);
    prev->setText(tr("Previous"));
    auto* next = new QToolButton(m_findBar);
    next->setText(tr("Next"));
    m_findCase = new QCheckBox(tr("Match case"), m_findBar);
    m_findStatus = new QLabel(m_findBar);
    auto* close = new QToolButton(m_findBar);
    close->setText(QStringLiteral("✕"));
    close->setToolTip(tr("Close (Esc)"));

    auto* row = new QHBoxLayout(m_findBar);
    row->setContentsMargins(8, 4, 8, 4);
    row->addWidget(new QLabel(tr("Find:"), m_findBar));
    row->addWidget(m_findEdit, 1);
    row->addWidget(prev);
    row->addWidget(next);
    row->addWidget(m_findCase);
    row->addWidget(m_findStatus);
    row->addWidget(close);

    connect(m_findEdit, &QLineEdit::returnPressed, this, [this] { findNext(false); });
    connect(m_findEdit, &QLineEdit::textChanged, this, [this] { m_findStatus->clear(); });
    connect(next, &QToolButton::clicked, this, [this] { findNext(false); });
    connect(prev, &QToolButton::clicked, this, [this] { findNext(true); });
    connect(close, &QToolButton::clicked, this, [this] {
        m_findBar->hide();
        m_edit->setFocus();
    });

    auto* escape = new QAction(m_findBar);
    escape->setShortcut(Qt::Key_Escape);
    escape->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_findBar->addAction(escape);
    connect(escape, &QAction::triggered, close, &QToolButton::click);

    m_findBar->hide();
}

// ---------------------------------------------------------------------------
// Formatting
// ---------------------------------------------------------------------------
void DoorWriteWidget::mergeFormat(const QTextCharFormat& format)
{
    QTextCursor cursor = m_edit->textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
    m_edit->mergeCurrentCharFormat(format);
    m_edit->setFocus();
}

void DoorWriteWidget::applyList(int styleValue)
{
    QTextCursor cursor = m_edit->textCursor();
    cursor.beginEditBlock();

    if (styleValue == 0) {
        if (QTextList* list = cursor.currentList()) {
            list->remove(cursor.block());
            QTextBlockFormat blockFmt = cursor.blockFormat();
            blockFmt.setIndent(0);
            cursor.setBlockFormat(blockFmt);
        }
    } else if (QTextList* list = cursor.currentList()) {
        QTextListFormat listFmt = list->format();
        listFmt.setStyle(static_cast<QTextListFormat::Style>(styleValue));
        list->setFormat(listFmt);
    } else {
        QTextListFormat listFmt;
        listFmt.setIndent(cursor.blockFormat().indent() + 1);
        listFmt.setStyle(static_cast<QTextListFormat::Style>(styleValue));
        cursor.createList(listFmt);
    }

    cursor.endEditBlock();
    m_edit->setFocus();
}

void DoorWriteWidget::chooseTextColor()
{
    const QColor color = QColorDialog::getColor(m_edit->textColor(), this, tr("Text Color"));
    if (!color.isValid())
        return;
    QTextCharFormat fmt;
    fmt.setForeground(color);
    mergeFormat(fmt);
    updateColorIcon(color);
}

void DoorWriteWidget::updateColorIcon(const QColor& color)
{
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor(0x9a, 0xa4, 0xc0));
    p.setBrush(color);
    p.drawRoundedRect(QRectF(1.5, 1.5, 13, 13), 3, 3);
    p.end();
    m_actColor->setIcon(QIcon(pix));
}

void DoorWriteWidget::onCurrentCharFormatChanged(const QTextCharFormat& format)
{
    const QFont f = format.font();
    m_fontCombo->setCurrentFont(f);
    const qreal size = format.fontPointSize() > 0 ? format.fontPointSize() : f.pointSizeF();
    m_sizeCombo->setCurrentText(QString::number(size > 0 ? size : 12.0, 'g', 4));
    m_actBold->setChecked(f.bold());
    m_actItalic->setChecked(f.italic());
    m_actUnderline->setChecked(f.underline());
    updateColorIcon(format.foreground().style() == Qt::NoBrush ? m_edit->palette().color(QPalette::Text)
                                                               : format.foreground().color());
}

void DoorWriteWidget::onCursorPositionChanged()
{
    const Qt::Alignment align = m_edit->alignment();
    if (align & Qt::AlignHCenter)
        m_actAlignCenter->setChecked(true);
    else if (align & Qt::AlignRight)
        m_actAlignRight->setChecked(true);
    else if (align & Qt::AlignJustify)
        m_actAlignJustify->setChecked(true);
    else
        m_actAlignLeft->setChecked(true);

    int listIndex = 0;
    if (const QTextList* list = m_edit->textCursor().currentList())
        listIndex = qMax(0, m_listCombo->findData(int(list->format().style())));
    m_listCombo->setCurrentIndex(listIndex);
}

// ---------------------------------------------------------------------------
// Find
// ---------------------------------------------------------------------------
void DoorWriteWidget::showFindBar()
{
    m_findBar->show();
    const QTextCursor cursor = m_edit->textCursor();
    if (cursor.hasSelection() && !cursor.selectedText().contains(QChar::ParagraphSeparator))
        m_findEdit->setText(cursor.selectedText());
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void DoorWriteWidget::findNext(bool backward)
{
    const QString needle = m_findEdit->text();
    if (needle.isEmpty())
        return;

    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;
    if (m_findCase->isChecked())
        flags |= QTextDocument::FindCaseSensitively;

    if (m_edit->find(needle, flags)) {
        m_findStatus->clear();
        return;
    }

    // Wrap around to the other end of the document.
    QTextCursor cursor = m_edit->textCursor();
    cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
    m_edit->setTextCursor(cursor);
    m_findStatus->setText(m_edit->find(needle, flags) ? tr("Wrapped") : tr("Not found"));
}

void DoorWriteWidget::onAutoCorrectToggled(bool enabled)
{
    auto& cfg = core::ConfigManager::instance();
    cfg.setValue(QStringLiteral("autoCorrect"), enabled);
    cfg.save();
}
void DoorWriteWidget::onImageClicked(bool /*on*/)
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Insert Image"), core::ConfigManager::instance().value(QStringLiteral("lastDirectory")).toString(),
                                                   tr("Images (*.png *.jpg *.jpeg *.bmp *.svg *.webp)"));
    if (path.isEmpty()) return;
    QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Door Write"),
                             tr("Cannot load \"%1\" as an image.").arg(QFileInfo(path).fileName()));
        return;
    }
    if (image.width() > 600)
        image = image.scaledToWidth(600, Qt::SmoothTransformation);
    QByteArray png;
    {
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
    }
    const QString dataUri = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
    QTextCursor cursor = m_edit->textCursor();
    cursor.beginEditBlock();
    cursor.insertImage(dataUri);
    cursor.endEditBlock();
    m_edit->setFocus();
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------
QString DoorWriteWidget::fileFilter()
{
    return tr("Microsoft Word (*.docx);;OpenDocument (*.odt);;Door Documents (*.ddoc);;HTML Files (*.html *.htm);;Text Files (*.txt);;All Files (*)");
}

QString DoorWriteWidget::displayName() const
{
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    return isModified() ? name + QStringLiteral(" •") : name;
}

bool DoorWriteWidget::isModified() const
{
    return m_edit->document()->isModified();
}

void DoorWriteWidget::setFilePath(const QString& path)
{
    m_filePath = path;
    m_edit->document()->setModified(false);
    emit titleChanged(displayName());
}

bool DoorWriteWidget::openFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Door Write"),
                             tr("Cannot open \"%1\":\n%2").arg(QDir::toNativeSeparators(path), file.errorString()));
        return false;
    }
    const QByteArray bytes = file.readAll();
    const QString suffix = QFileInfo(path).suffix().toLower();

    if (suffix == QLatin1String("ddoc")) {
        QJsonParseError err{};
        const QJsonObject obj = QJsonDocument::fromJson(bytes, &err).object();
        if (err.error != QJsonParseError::NoError
            || obj.value(QStringLiteral("format")).toString() != QLatin1String(kDdocFormat)) {
            QMessageBox::warning(this, tr("Door Write"),
                                 tr("\"%1\" is not a valid Door Document.").arg(QFileInfo(path).fileName()));
            return false;
        }
        m_edit->setHtml(obj.value(QStringLiteral("html")).toString());
    } else if (suffix == QLatin1String("html") || suffix == QLatin1String("htm")) {
        auto decoder = QStringDecoder::decoderForHtml(bytes);
        m_edit->setHtml(decoder.isValid() ? QString(decoder(bytes)) : QString::fromUtf8(bytes));
    } else {
        m_edit->setPlainText(QString::fromUtf8(bytes));
    }

    rememberDirectory(path);
    setFilePath(path);
    emit statusMessage(tr("Opened %1").arg(QDir::toNativeSeparators(path)));
    return true;
}

bool DoorWriteWidget::writeFile(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    QByteArray payload;

    if (suffix == QLatin1String("txt")) {
        payload = m_edit->toPlainText().toUtf8();
    } else if (suffix == QLatin1String("html") || suffix == QLatin1String("htm")) {
        payload = m_edit->toHtml().toUtf8();
    } else {
        // .ddoc: versioned JSON container wrapping the rich-text HTML.
        const QJsonObject obj{
            {QStringLiteral("format"), QLatin1String(kDdocFormat)},
            {QStringLiteral("version"), kDdocVersion},
            {QStringLiteral("generator"), QStringLiteral("Door Office " DOOR_OFFICE_VERSION)},
            {QStringLiteral("html"), m_edit->toHtml()},
        };
        payload = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit()) {
        QMessageBox::warning(this, tr("Door Write"),
                             tr("Cannot save \"%1\":\n%2").arg(QDir::toNativeSeparators(path), file.errorString()));
        return false;
    }

    rememberDirectory(path);
    setFilePath(path);
    emit statusMessage(tr("Saved %1").arg(QDir::toNativeSeparators(path)));
    return true;
}

bool DoorWriteWidget::save()
{
    return m_filePath.isEmpty() ? saveAs() : writeFile(m_filePath);
}

bool DoorWriteWidget::saveAs()
{
    QString selectedFilter;
    const QString start = m_filePath.isEmpty() ? QDir(lastDirectory()).filePath(tr("Untitled.ddoc")) : m_filePath;
    QString path = QFileDialog::getSaveFileName(this, tr("Save Document As"), start, fileFilter(), &selectedFilter);
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty()) {
        if (selectedFilter.contains(QLatin1String("*.html")))
            path += QStringLiteral(".html");
        else if (selectedFilter.contains(QLatin1String("*.txt")))
            path += QStringLiteral(".txt");
        else
            path += QStringLiteral(".ddoc");
    }
    return writeFile(path);
}

bool DoorWriteWidget::maybeSave()
{
    if (!isModified())
        return true;
    const QString name = m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName();
    const auto answer = QMessageBox::question(this, tr("Door Write"),
                                              tr("\"%1\" has unsaved changes.\nDo you want to save them?").arg(name),
                                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                              QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

// ---------------------------------------------------------------------------
// PDF / Print
// ---------------------------------------------------------------------------
bool DoorWriteWidget::exportPdf()
{
    QString base = QDir(lastDirectory()).filePath(tr("Untitled"));
    if (!m_filePath.isEmpty()) {
        const QFileInfo info(m_filePath);
        base = info.dir().filePath(info.completeBaseName());
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Export to PDF"), base + QStringLiteral(".pdf"),
                                                tr("PDF Files (*.pdf)"));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".pdf");

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    printer.setCreator(QStringLiteral("Door Office " DOOR_OFFICE_VERSION));
    printer.setDocName(QFileInfo(path).completeBaseName());
    m_edit->document()->print(&printer);

    emit statusMessage(tr("Exported PDF to %1").arg(QDir::toNativeSeparators(path)), 5000);
    return true;
}

void DoorWriteWidget::print()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(m_filePath.isEmpty() ? tr("Untitled") : QFileInfo(m_filePath).fileName());

    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(tr("Print Document"));
    if (m_edit->textCursor().hasSelection())
        dialog.setOption(QAbstractPrintDialog::PrintSelection);

    if (dialog.exec() == QDialog::Accepted) {
        m_edit->print(&printer);
        emit statusMessage(tr("Document sent to printer"));
    }
}

bool DoorWriteWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_edit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        
        // Check if AutoCorrect is enabled
        const auto& cfg = core::ConfigManager::instance();
        bool autoCorrectEnabled = cfg.value(QStringLiteral("autoCorrect"), true).toBool();
        
        if (!autoCorrectEnabled) {
            return QWidget::eventFilter(obj, event);
        }
        
        // Handle auto-capitalization
        if (keyEvent->key() >= Qt::Key_A && keyEvent->key() <= Qt::Key_Z) {
            autoCapitalize();
        }
        
        // Handle word corrections on space or return
        if (keyEvent->key() == Qt::Key_Space || keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            correctLastWord();
        }
    }
    
    return QWidget::eventFilter(obj, event);
}

void DoorWriteWidget::autoCapitalize()
{
    QTextCursor cursor = m_edit->textCursor();
    if (!cursor.hasSelection()) {
        // Check if we're at the beginning of the document
        if (cursor.position() == 0) {
            // First character of document - always capitalize
            QString currentChar = cursor.selectedText();
            if (!currentChar.isEmpty() && currentChar[0].isLower()) {
                cursor.beginEditBlock();
                cursor.deleteChar();
                cursor.insertText(currentChar.toUpper());
                cursor.endEditBlock();
            }
            return;
        }
        
        // Move cursor to the previous character to check context
        QTextCursor prevCursor = cursor;
        prevCursor.setPosition(cursor.position() - 1);
        
        QString prevChar = prevCursor.selectedText();
        if (prevChar.isEmpty()) {
            return;
        }
            
        // Check if previous character is a sentence-ending punctuation or newline
        if (prevChar == "." || prevChar == "!" || prevChar == "?" || prevChar == "\n") {
            // We're at the start of a sentence, capitalize the current character
            QTextCursor currentCursor = m_edit->textCursor();
            QString currentChar = currentCursor.selectedText();
            if (!currentChar.isEmpty() && currentChar[0].isLower()) {
                currentCursor.beginEditBlock();
                currentCursor.deleteChar();
                currentCursor.insertText(currentChar.toUpper());
                currentCursor.endEditBlock();
            }
        }
    }
}

void DoorWriteWidget::correctLastWord()
{
    QTextCursor cursor = m_edit->textCursor();
    cursor.beginEditBlock();
    
    // Move to the end of the word before the space/return
    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::StartOfWord, QTextCursor::KeepAnchor);
    
    QString word = cursor.selectedText();
    if (word.isEmpty()) {
        cursor.endEditBlock();
        return;
    }
    
    // Common typo corrections
    static const QMap<QString, QString> corrections = {
        // Contractions
        {"i", "I"},
        {"im", "I'm"},
        {"ive", "I've"},
        {"id", "I'd"},
        {"ill", "I'll"},
        {"dont", "don't"},
        {"cant", "can't"},
        {"wont", "won't"},
        {"isnt", "isn't"},
        {"didnt", "didn't"},
        {"doesnt", "doesn't"},
        {"couldnt", "couldn't"},
        {"wouldnt", "wouldn't"},
        {"shouldnt", "shouldn't"},
        {"hasnt", "hasn't"},
        {"havent", "haven't"},
        {"wasnt", "wasn't"},
        {"werent", "weren't"},
        {"arent", "aren't"},
        {"hes", "he's"},
        {"shes", "she's"},
        {"its", "it's"},
        {"lets", "let's"},
        {"youre", "you're"},
        {"gonna", "going to"},
        {"wanna", "want to"},
        {"kinda", "kind of"},
        {"sorta", "sort of"},
        {"outta", "out of"},
        {"gotta", "got to"},
        {"coulda", "could have"},
        {"shoulda", "should have"},
        {"woulda", "would have"},
        {"musta", "must have"},
        {"mighta", "might have"},
        // Common misspellings
        {"teh", "the"},
        {"adn", "and"},
        {"recieve", "receive"},
        {"seperate", "separate"},
        {"definately", "definitely"},
        {"occured", "occurred"},
        {"untill", "until"},
        {"wich", "which"},
        {"becuase", "because"},
        {"tehre", "there"},
        {"writting", "writing"},
        {"grammer", "grammar"},
        {"thier", "their"},
        {"beleive", "believe"},
        {"belive", "believe"},
        {"freind", "friend"},
        {"freindly", "friendly"},
        {"enviroment", "environment"},
        {"goverment", "government"},
        {"knowlege", "knowledge"},
        {"calender", "calendar"},
        {"comming", "coming"},
        {"dissapear", "disappear"},
        {"experiance", "experience"},
        {"foriegn", "foreign"},
        {"immense", "immense"},
        {"lisence", "license"},
        {"neccessary", "necessary"},
        {"occassion", "occasion"},
        {"occassionally", "occasionally"},
        {"persue", "pursue"},
        {"posession", "possession"},
        {"publically", "publicly"},
        {"recomend", "recommend"},
        {"refering", "referring"},
        {"rember", "remember"},
        {"sucess", "success"},
        {"begining", "beginning"},
        {"concious", "conscious"},
        {"colaps", "collapse"},
        {"arguement", "argument"},
        {"embarass", "embarrass"},
        {"accomodate", "accommodate"},
        {"wierd", "weird"},
        {"acheive", "achieve"},
        {"aquire", "acquire"},
        {"apparant", "apparent"},
        {"cemetary", "cemetery"},
        {"existance", "existence"},
        {"firey", "fiery"},
        {"garantee", "guarantee"},
        {"hieght", "height"},
        {"independant", "independent"},
        {"indispensible", "indispensable"},
        {"liason", "liaison"},
        {"maintainance", "maintenance"},
        {"millenium", "millennium"},
        {"neice", "niece"},
        {"noticable", "noticeable"},
        {"payed", "paid"},
        {"priviledge", "privilege"},
        {"questionaire", "questionnaire"},
        {"rythm", "rhythm"},
        {"sieze", "seize"},
        {"supercede", "supersede"},
        {"tatoo", "tattoo"},
        {"tommorow", "tomorrow"},
        {"vaccuum", "vacuum"},
        // Shorthand
        {"tht", "that"},
        {"thta", "that"},
        {"wrd", "word"},
        {"wy", "way"},
        {"u", "you"},
        {"r", "are"},
        {"ur", "your"},
        {"bc", "because"},
        {"ppl", "people"},
        {"msg", "message"},
        {"abt", "about"},
        {"tho", "though"},
        {"thru", "through"},
        {"plz", "please"},
        {"thx", "thanks"},
        {"asap", "ASAP"},
        {"brb", "be right back"},
        {"gtg", "got to go"},
        {"lol", "laugh out loud"},
        {"lmao", "laugh my ass off"},
        {"idk", "I don't know"},
        {"tbh", "to be honest"},
        {"omg", "oh my god"},
        {"wtf", "what the fuck"},
        {"afk", "away from keyboard"},
        {"ttyl", "talk to you later"},
        {"idc", "I don't care"},
        {"imo", "in my opinion"},
        {"imho", "in my humble opinion"},
        {"fyi", "for your information"},
        {"rofl", "rolling on the floor laughing"},
        {"smh", "shaking my head"},
        {"btw", "by the way"},
        {"np", "no problem"},
        {"hbu", "how about you"},
        {"cya", "see you"},
        {"yolo", "you only live once"},
        {"jk", "just kidding"},
        {"omw", "on my way"},
        {"rn", "right now"},
        {"tmr", "tomorrow"},
        {"atm", "at the moment"},
        {"cuz", "because"},
        {"coz", "because"},
        {"ok", "OK"},
        {"ya", "you"},
        // Symbols
        {"(c)", "©"},
        {"(r)", "®"},
        {"(tm)", "™"},
        {"--", "—"}
    };

    if (corrections.contains(word)) {
        QString replacement = corrections[word];
        cursor.insertText(replacement);
    }

    cursor.endEditBlock();
}

} // namespace door::modules
