#include "features/auth/ui/AccountSelectDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

AccountSelectDialog::AccountSelectDialog(const QStringList &accounts,
                                         const QString &currentAccount,
                                         QWidget *parent)
    : QDialog(parent)
    , m_listWidget(new QListWidget(this))
{
    setWindowTitle(QStringLiteral("选择账号"));
    setModal(true);
    resize(420, 360);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QLabel *titleLabel = new QLabel(QStringLiteral("请选择要登录的账号"), this);
    layout->addWidget(titleLabel);

    m_listWidget->addItems(accounts);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_listWidget, 1);

    if (!currentAccount.trimmed().isEmpty())
    {
        const QList<QListWidgetItem *> matched = m_listWidget->findItems(currentAccount, Qt::MatchExactly);
        if (!matched.isEmpty())
        {
            m_listWidget->setCurrentItem(matched.first());
        }
    }
    if (!m_listWidget->currentItem() && m_listWidget->count() > 0)
    {
        m_listWidget->setCurrentRow(0);
    }

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("关闭"));
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (m_listWidget->currentItem())
        {
            accept();
        }
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this]() {
        if (m_listWidget->currentItem())
        {
            accept();
        }
    });
}

QString AccountSelectDialog::selectedAccount() const
{
    return m_listWidget->currentItem() ? m_listWidget->currentItem()->text().trimmed() : QString();
}
