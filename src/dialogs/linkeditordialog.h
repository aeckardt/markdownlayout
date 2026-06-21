#ifndef LINKEDITORDIALOG_H
#define LINKEDITORDIALOG_H

#include <QDialog>

class QLineEdit;

class LinkEditorDialog : public QDialog
{
    Q_OBJECT
public:
    LinkEditorDialog(const QString &title, QWidget *parent = nullptr);

    QString caption() const;
    void setCaption(const QString &caption);

    QString linkUrl() const;
    void setLinkUrl(const QString &linkUrl);

    void selectCaption();

public slots:
    void accept() override;
    void reject() override;

private:
    QLineEdit *m_captionEdit;
    QLineEdit *m_linkUrlEdit;
};

#endif // LINKEDITORDIALOG_H
