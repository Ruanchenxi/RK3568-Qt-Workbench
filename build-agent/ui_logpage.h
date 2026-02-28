/********************************************************************************
** Form generated from reading UI file 'logpage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGPAGE_H
#define UI_LOGPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LogPage
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *labelTitle;
    QVBoxLayout *layoutLog;

    void setupUi(QWidget *LogPage)
    {
        if (LogPage->objectName().isEmpty())
            LogPage->setObjectName(QString::fromUtf8("LogPage"));
        LogPage->resize(1280, 640);
        LogPage->setStyleSheet(QString::fromUtf8("\n"
"    QWidget#LogPage {\n"
"      background-color: #1A1A2E;\n"
"    }\n"
"   "));
        verticalLayout = new QVBoxLayout(LogPage);
        verticalLayout->setSpacing(12);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(20, 20, 20, 20);
        labelTitle = new QLabel(LogPage);
        labelTitle->setObjectName(QString::fromUtf8("labelTitle"));
        labelTitle->setStyleSheet(QString::fromUtf8("\n"
"       QLabel {\n"
"         color: #E2E8F0;\n"
"         font-size: 20px;\n"
"         font-weight: bold;\n"
"         padding: 10px 0px;\n"
"       }\n"
"      "));

        verticalLayout->addWidget(labelTitle);

        layoutLog = new QVBoxLayout();
        layoutLog->setSpacing(0);
        layoutLog->setObjectName(QString::fromUtf8("layoutLog"));

        verticalLayout->addLayout(layoutLog);


        retranslateUi(LogPage);

        QMetaObject::connectSlotsByName(LogPage);
    } // setupUi

    void retranslateUi(QWidget *LogPage)
    {
        LogPage->setWindowTitle(QCoreApplication::translate("LogPage", "\346\234\215\345\212\241\346\227\245\345\277\227", nullptr));
        labelTitle->setText(QCoreApplication::translate("LogPage", "\346\234\215\345\212\241\345\220\257\345\212\250\346\227\245\345\277\227", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LogPage: public Ui_LogPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGPAGE_H
