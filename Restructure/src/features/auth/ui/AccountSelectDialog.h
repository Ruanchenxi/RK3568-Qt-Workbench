#ifndef ACCOUNTSELECTDIALOG_H
#define ACCOUNTSELECTDIALOG_H

#include <QDialog>
#include <QStringList>

class QListWidget;

class AccountSelectDialog : public QDialog
{
public:
    explicit AccountSelectDialog(const QStringList &accounts,
                                 const QString &currentAccount,
                                 QWidget *parent = nullptr);

    QString selectedAccount() const;

private:
    QListWidget *m_listWidget;
};

#endif // ACCOUNTSELECTDIALOG_H
