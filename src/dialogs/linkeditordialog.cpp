#include "linkeditordialog.h"

#include <QBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

LinkEditorDialog::LinkEditorDialog(const QString &title, QWidget *parent)
    : QDialog(parent)
{
    QBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(5);

    QBoxLayout *captionLayout = new QVBoxLayout;
    captionLayout->setContentsMargins(0, 0, 0, 0);
    captionLayout->setSpacing(5);

    captionLayout->addWidget(new QLabel(tr("Displayed text")));

    m_captionEdit = new QLineEdit;
    captionLayout->addWidget(m_captionEdit);

    layout->addLayout(captionLayout);

    QBoxLayout *linkUrlLayout = new QVBoxLayout;
    linkUrlLayout->setContentsMargins(0, 0, 0, 0);
    linkUrlLayout->setSpacing(5);

    linkUrlLayout->addWidget(new QLabel(tr("URL")));

    m_linkUrlEdit = new QLineEdit;
    linkUrlLayout->addWidget(m_linkUrlEdit);

    layout->addSpacing(10);
    layout->addLayout(linkUrlLayout);

    QBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(5);

    buttonLayout->addStretch(1);

    QPushButton *okButton = new QPushButton(tr("Ok"));
    okButton->setMinimumWidth(120);
    connect(okButton, &QPushButton::clicked, this, &LinkEditorDialog::accept);
    buttonLayout->addWidget(okButton);

    QPushButton *cancelButton = new QPushButton(tr("Cancel"));
    cancelButton->setMinimumWidth(120);
    connect(cancelButton, &QPushButton::clicked, this, &LinkEditorDialog::reject);
    buttonLayout->addWidget(cancelButton);

    layout->addSpacing(20);
    layout->addStretch(1);
    layout->addLayout(buttonLayout);

    setLayout(layout);
    setWindowTitle(title);

    // TODO: Restore geometry
}

QString LinkEditorDialog::caption() const
{
    return m_captionEdit->text();
}

void LinkEditorDialog::setCaption(const QString &caption)
{
    m_captionEdit->setText(caption);
}

QString LinkEditorDialog::linkUrl() const
{
    return m_linkUrlEdit->text();
}

void LinkEditorDialog::setLinkUrl(const QString &linkUrl)
{
    m_linkUrlEdit->setText(linkUrl);
}

void LinkEditorDialog::selectCaption()
{
    m_captionEdit->selectAll();
}

void LinkEditorDialog::accept()
{
    // TODO: Save geometry
    QDialog::accept();
}

void LinkEditorDialog::reject()
{
    // TODO: Save geometry
    QDialog::reject();
}
