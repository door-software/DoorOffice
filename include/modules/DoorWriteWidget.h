// Door Office v1.0 — Opens the door to a new world of technology
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTextCharFormat>
#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QMovie>

class QAction;
class QActionGroup;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolBar;

namespace door::modules {

// Door Write — the rich-text word processor module.
class DoorWriteWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit DoorWriteWidget(QWidget* parent = nullptr);

    [[nodiscard]] QTextEdit* editor() const noexcept { return m_edit; }
    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool isModified() const;

    // Formatting actions (shared with the main window's Format menu).
    [[nodiscard]] QAction* boldAction() const noexcept { return m_actBold; }
    [[nodiscard]] QAction* italicAction() const noexcept { return m_actItalic; }
    [[nodiscard]] QAction* underlineAction() const noexcept { return m_actUnderline; }

    static QString fileFilter();

public slots:
    bool openFile(const QString& path);
    bool save();
    bool saveAs();
    bool exportPdf();
    void print();
    void showFindBar();
    void chooseTextColor();
    bool maybeSave(); // returns false if the user cancelled

signals:
    void titleChanged(const QString& title);
    void statusMessage(const QString& message, int timeoutMs = 3000);

private slots:
    void onCurrentCharFormatChanged(const QTextCharFormat& format);
    void onCursorPositionChanged();
    void findNext(bool backward = false);
    void onAutoCorrectToggled(bool enabled);
    void onImageClicked(bool /*on*/);

private:
    void buildToolBar();
    void buildFindBar();
    void mergeFormat(const QTextCharFormat& format);
    void applyList(int styleValue);
    void updateColorIcon(const QColor& color);
    bool writeFile(const QString& path);
    bool openImageFile(const QString& path);
    void setFilePath(const QString& path);
    bool eventFilter(QObject* obj, QEvent* event) override;
    void autoCapitalize();
    void correctLastWord();

    // ODT/DOCX helpers
    bool readOdt(const QString& path);
    bool writeOdt(const QString& path);
    bool readDocx(const QString& path);
    bool writeDocx(const QString& path);

    QToolBar* m_toolBar = nullptr;
    QTextEdit* m_edit = nullptr;

    QFontComboBox* m_fontCombo = nullptr;
    QComboBox* m_sizeCombo = nullptr;
    QComboBox* m_listCombo = nullptr;

    QAction* m_actBold = nullptr;
    QAction* m_actItalic = nullptr;
    QAction* m_actUnderline = nullptr;
    QAction* m_actColor = nullptr;
    QAction* m_actAlignLeft = nullptr;
    QAction* m_actAlignCenter = nullptr;
    QAction* m_actAlignRight = nullptr;
    QAction* m_actAlignJustify = nullptr;
    QPushButton* m_btnInsertImage = nullptr;
    QAction* m_actAutoCorrect = nullptr;

    QWidget* m_findBar = nullptr;
    QLineEdit* m_findEdit = nullptr;
    QCheckBox* m_findCase = nullptr;
    QLabel* m_findStatus = nullptr;

    QString m_filePath;
};

} // namespace door::modules
