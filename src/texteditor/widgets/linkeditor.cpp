#include "texteditor/widgets/linkeditor.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace notetree::texteditor {

LinkEditorDialog::LinkEditorDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(5);

    auto *captionLayout = new QVBoxLayout;
    captionLayout->setContentsMargins(0, 0, 0, 0);
    captionLayout->setSpacing(5);
    captionLayout->addWidget(new QLabel(tr("Displayed text"), this));
    m_captionEdit = new QLineEdit(this);
    captionLayout->addWidget(m_captionEdit);
    layout->addLayout(captionLayout);

    auto *urlLayout = new QVBoxLayout;
    urlLayout->setContentsMargins(0, 0, 0, 0);
    urlLayout->setSpacing(5);
    urlLayout->addWidget(new QLabel(QStringLiteral("URL"), this));
    m_urlEdit = new QLineEdit(this);
    urlLayout->addWidget(m_urlEdit);
    layout->addSpacing(10);
    layout->addLayout(urlLayout);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    auto *ok = new QPushButton(tr("Ok"), this);
    ok->setMinimumWidth(120);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(ok);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    cancel->setMinimumWidth(120);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancel);

    layout->addSpacing(20);
    layout->addStretch(1);
    layout->addLayout(buttonLayout);
    setWindowTitle(title);
}

QString LinkEditorDialog::displayedText() const { return m_captionEdit->text(); }
QString LinkEditorDialog::url() const { return m_urlEdit->text(); }
void LinkEditorDialog::setDisplayedText(const QString &text) { m_captionEdit->setText(text); }
void LinkEditorDialog::setUrl(const QString &url) { m_urlEdit->setText(url); }

} // namespace notetree::texteditor
