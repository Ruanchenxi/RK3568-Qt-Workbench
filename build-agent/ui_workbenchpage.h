/********************************************************************************
** Form generated from reading UI file 'workbenchpage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WORKBENCHPAGE_H
#define UI_WORKBENCHPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_WorkbenchPage
{
public:
    QVBoxLayout *mainLayout;
    QWidget *webViewContainer;

    void setupUi(QWidget *WorkbenchPage)
    {
        if (WorkbenchPage->objectName().isEmpty())
            WorkbenchPage->setObjectName(QString::fromUtf8("WorkbenchPage"));
        WorkbenchPage->resize(1280, 608);
        WorkbenchPage->setStyleSheet(QString::fromUtf8("\n"
"#WorkbenchPage {\n"
"    background-color: #FFFFFF;\n"
"}\n"
"   "));
        mainLayout = new QVBoxLayout(WorkbenchPage);
        mainLayout->setSpacing(0);
        mainLayout->setObjectName(QString::fromUtf8("mainLayout"));
        mainLayout->setContentsMargins(0, 0, 0, 0);
        webViewContainer = new QWidget(WorkbenchPage);
        webViewContainer->setObjectName(QString::fromUtf8("webViewContainer"));
        webViewContainer->setStyleSheet(QString::fromUtf8("background-color: #FFFFFF;"));

        mainLayout->addWidget(webViewContainer);


        retranslateUi(WorkbenchPage);

        QMetaObject::connectSlotsByName(WorkbenchPage);
    } // setupUi

    void retranslateUi(QWidget *WorkbenchPage)
    {
        (void)WorkbenchPage;
    } // retranslateUi

};

namespace Ui {
    class WorkbenchPage: public Ui_WorkbenchPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WORKBENCHPAGE_H
