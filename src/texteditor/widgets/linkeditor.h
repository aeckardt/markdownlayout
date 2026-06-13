#pragma once

#include <QDialog>

class QLineEdit;

namespace notetree::texteditor {

class LinkEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit LinkEditorDialog(const QString &title, QWidget *parent = nullptr);

    QString displayedText() const;
    QString url() const;
    void setDisplayedText(const QString &text);
    void setUrl(const QString &url);

private:
    QLineEdit *m_captionEdit = nullptr;
    QLineEdit *m_urlEdit = nullptr;
};

} // namespace notetree::texteditor
