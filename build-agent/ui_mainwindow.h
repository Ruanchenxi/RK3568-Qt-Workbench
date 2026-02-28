/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QWidget *headerWidget;
    QHBoxLayout *headerLayout;
    QWidget *logoArea;
    QHBoxLayout *logoLayout;
    QLabel *logoIcon;
    QLabel *systemTitle;
    QSpacerItem *headerSpacer1;
    QWidget *navTabs;
    QHBoxLayout *navTabsLayout;
    QPushButton *btnWorkbench;
    QPushButton *btnKeys;
    QPushButton *btnSystem;
    QPushButton *btnService;
    QPushButton *btnKeyboard;
    QSpacerItem *headerSpacer2;
    QWidget *userArea;
    QHBoxLayout *userAreaLayout;
    QLabel *userAvatar;
    QLabel *userNameLabel;
    QPushButton *logoutBtn;
    QStackedWidget *contentStackedWidget;
    QWidget *loginPage;
    QWidget *workbenchPage;
    QWidget *keyManagePage;
    QWidget *systemPage;
    QWidget *logPage;
    QWidget *keyboardPage;
    QWidget *footerWidget;
    QHBoxLayout *footerLayout;
    QLabel *companyLabel;
    QSpacerItem *footerSpacer;
    QLabel *timeLabel;
    QFrame *divider1;
    QLabel *userInfoLabel;
    QLabel *roleLabel;
    QFrame *divider2;
    QWidget *cardStatusWidget;
    QHBoxLayout *cardStatusLayout;
    QLabel *cardStatusDot;
    QLabel *cardStatusLabel;
    QWidget *fingerStatusWidget;
    QHBoxLayout *fingerStatusLayout;
    QLabel *fingerStatusDot;
    QLabel *fingerStatusLabel;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1280, 720);
        MainWindow->setMinimumSize(QSize(1280, 720));
        MainWindow->setStyleSheet(QString::fromUtf8("\n"
"/* ========== \345\205\250\345\261\200\346\240\267\345\274\217 ========== */\n"
"QMainWindow {\n"
"    background-color: #F5F3EF;\n"
"}\n"
"\n"
"/* ========== \351\241\266\351\203\250\345\257\274\350\210\252\346\240\217\346\240\267\345\274\217 ========== */\n"
"#headerWidget {\n"
"    background-color: #FFFFFF;\n"
"    border-bottom: 1px solid #E8E4DE;\n"
"}\n"
"\n"
"/* Logo\345\214\272\345\237\237\346\240\207\351\242\230 */\n"
"#systemTitle {\n"
"    color: #2C3E50;\n"
"    font-size: 18px;\n"
"    font-weight: normal;\n"
"}\n"
"\n"
"/* \345\257\274\350\210\252\346\214\211\351\222\256\351\200\232\347\224\250\346\240\267\345\274\217 */\n"
"#btnWorkbench, #btnKeys, #btnSystem, #btnService, #btnKeyboard {\n"
"    background-color: transparent;\n"
"    color: #5D6B7A;\n"
"    font-size: 14px;\n"
"    border: none;\n"
"    border-radius: 6px;\n"
"    padding: 10px 20px;\n"
"}\n"
"\n"
"#btnWorkbench:hover, #btnKeys:hover, #btnSystem:hover, #btnService:hover, #btnKeyboard:hover {\n"
"    background-color: #E8E4"
                        "DE;\n"
"}\n"
"\n"
"#btnWorkbench:checked, #btnKeys:checked, #btnSystem:checked, #btnService:checked, #btnKeyboard:checked {\n"
"    background-color: #2E7D32;\n"
"    color: #FFFFFF;\n"
"}\n"
"\n"
"/* \347\224\250\346\210\267\344\277\241\346\201\257 */\n"
"#userNameLabel {\n"
"    color: #2C3E50;\n"
"    font-size: 14px;\n"
"}\n"
"\n"
"/* \351\200\200\345\207\272\346\214\211\351\222\256 */\n"
"#logoutBtn {\n"
"    background-color: transparent;\n"
"    color: #5D6B7A;\n"
"    font-size: 13px;\n"
"    border: 1px solid #E0E0E0;\n"
"    border-radius: 6px;\n"
"    padding: 8px 16px;\n"
"}\n"
"\n"
"#logoutBtn:hover {\n"
"    background-color: #F5F5F5;\n"
"}\n"
"\n"
"/* ========== \345\272\225\351\203\250\347\212\266\346\200\201\346\240\217\346\240\267\345\274\217 ========== */\n"
"#footerWidget {\n"
"    background-color: #FFFFFF;\n"
"    border-top: 1px solid #E8E4DE;\n"
"}\n"
"\n"
"#companyLabel {\n"
"    color: #9E9E9E;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"#timeLabel {\n"
"    color: #5D6B7A;\n"
"    font-s"
                        "ize: 12px;\n"
"}\n"
"\n"
"#userInfoLabel, #roleLabel {\n"
"    color: #5D6B7A;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"#cardStatusLabel, #fingerStatusLabel {\n"
"    color: #5D6B7A;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"/* ========== \345\206\205\345\256\271\345\214\272\345\237\237\346\240\267\345\274\217 ========== */\n"
"#contentStackedWidget {\n"
"    background-color: transparent;\n"
"}\n"
"   "));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setSpacing(0);
        mainLayout->setContentsMargins(11, 11, 11, 11);
        mainLayout->setObjectName(QString::fromUtf8("mainLayout"));
        mainLayout->setContentsMargins(0, 0, 0, 0);
        headerWidget = new QWidget(centralWidget);
        headerWidget->setObjectName(QString::fromUtf8("headerWidget"));
        headerWidget->setMinimumSize(QSize(0, 64));
        headerWidget->setMaximumSize(QSize(16777215, 64));
        headerLayout = new QHBoxLayout(headerWidget);
        headerLayout->setSpacing(0);
        headerLayout->setContentsMargins(11, 11, 11, 11);
        headerLayout->setObjectName(QString::fromUtf8("headerLayout"));
        headerLayout->setContentsMargins(24, 0, 24, 0);
        logoArea = new QWidget(headerWidget);
        logoArea->setObjectName(QString::fromUtf8("logoArea"));
        logoLayout = new QHBoxLayout(logoArea);
        logoLayout->setSpacing(16);
        logoLayout->setContentsMargins(11, 11, 11, 11);
        logoLayout->setObjectName(QString::fromUtf8("logoLayout"));
        logoLayout->setContentsMargins(0, 0, 0, 0);
        logoIcon = new QLabel(logoArea);
        logoIcon->setObjectName(QString::fromUtf8("logoIcon"));
        logoIcon->setMinimumSize(QSize(40, 40));
        logoIcon->setMaximumSize(QSize(40, 40));
        logoIcon->setStyleSheet(QString::fromUtf8("background-color: #2E7D32; border-radius: 8px; color: white; font-size: 20px;"));
        logoIcon->setAlignment(Qt::AlignCenter);

        logoLayout->addWidget(logoIcon);

        systemTitle = new QLabel(logoArea);
        systemTitle->setObjectName(QString::fromUtf8("systemTitle"));

        logoLayout->addWidget(systemTitle);


        headerLayout->addWidget(logoArea);

        headerSpacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        headerLayout->addItem(headerSpacer1);

        navTabs = new QWidget(headerWidget);
        navTabs->setObjectName(QString::fromUtf8("navTabs"));
        navTabsLayout = new QHBoxLayout(navTabs);
        navTabsLayout->setSpacing(4);
        navTabsLayout->setContentsMargins(11, 11, 11, 11);
        navTabsLayout->setObjectName(QString::fromUtf8("navTabsLayout"));
        navTabsLayout->setContentsMargins(0, 0, 0, 0);
        btnWorkbench = new QPushButton(navTabs);
        btnWorkbench->setObjectName(QString::fromUtf8("btnWorkbench"));
        btnWorkbench->setCheckable(true);
        btnWorkbench->setChecked(true);
        btnWorkbench->setAutoExclusive(true);

        navTabsLayout->addWidget(btnWorkbench);

        btnKeys = new QPushButton(navTabs);
        btnKeys->setObjectName(QString::fromUtf8("btnKeys"));
        btnKeys->setCheckable(true);
        btnKeys->setAutoExclusive(true);

        navTabsLayout->addWidget(btnKeys);

        btnSystem = new QPushButton(navTabs);
        btnSystem->setObjectName(QString::fromUtf8("btnSystem"));
        btnSystem->setCheckable(true);
        btnSystem->setAutoExclusive(true);

        navTabsLayout->addWidget(btnSystem);

        btnService = new QPushButton(navTabs);
        btnService->setObjectName(QString::fromUtf8("btnService"));
        btnService->setCheckable(true);
        btnService->setAutoExclusive(true);

        navTabsLayout->addWidget(btnService);

        btnKeyboard = new QPushButton(navTabs);
        btnKeyboard->setObjectName(QString::fromUtf8("btnKeyboard"));
        btnKeyboard->setCheckable(true);
        btnKeyboard->setAutoExclusive(true);

        navTabsLayout->addWidget(btnKeyboard);


        headerLayout->addWidget(navTabs);

        headerSpacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        headerLayout->addItem(headerSpacer2);

        userArea = new QWidget(headerWidget);
        userArea->setObjectName(QString::fromUtf8("userArea"));
        userAreaLayout = new QHBoxLayout(userArea);
        userAreaLayout->setSpacing(16);
        userAreaLayout->setContentsMargins(11, 11, 11, 11);
        userAreaLayout->setObjectName(QString::fromUtf8("userAreaLayout"));
        userAreaLayout->setContentsMargins(0, 0, 0, 0);
        userAvatar = new QLabel(userArea);
        userAvatar->setObjectName(QString::fromUtf8("userAvatar"));
        userAvatar->setMinimumSize(QSize(32, 32));
        userAvatar->setMaximumSize(QSize(32, 32));
        userAvatar->setStyleSheet(QString::fromUtf8("background-color: #E8E4DE; border-radius: 16px;"));
        userAvatar->setAlignment(Qt::AlignCenter);

        userAreaLayout->addWidget(userAvatar);

        userNameLabel = new QLabel(userArea);
        userNameLabel->setObjectName(QString::fromUtf8("userNameLabel"));

        userAreaLayout->addWidget(userNameLabel);

        logoutBtn = new QPushButton(userArea);
        logoutBtn->setObjectName(QString::fromUtf8("logoutBtn"));

        userAreaLayout->addWidget(logoutBtn);


        headerLayout->addWidget(userArea);


        mainLayout->addWidget(headerWidget);

        contentStackedWidget = new QStackedWidget(centralWidget);
        contentStackedWidget->setObjectName(QString::fromUtf8("contentStackedWidget"));
        loginPage = new QWidget();
        loginPage->setObjectName(QString::fromUtf8("loginPage"));
        contentStackedWidget->addWidget(loginPage);
        workbenchPage = new QWidget();
        workbenchPage->setObjectName(QString::fromUtf8("workbenchPage"));
        contentStackedWidget->addWidget(workbenchPage);
        keyManagePage = new QWidget();
        keyManagePage->setObjectName(QString::fromUtf8("keyManagePage"));
        contentStackedWidget->addWidget(keyManagePage);
        systemPage = new QWidget();
        systemPage->setObjectName(QString::fromUtf8("systemPage"));
        contentStackedWidget->addWidget(systemPage);
        logPage = new QWidget();
        logPage->setObjectName(QString::fromUtf8("logPage"));
        contentStackedWidget->addWidget(logPage);
        keyboardPage = new QWidget();
        keyboardPage->setObjectName(QString::fromUtf8("keyboardPage"));
        contentStackedWidget->addWidget(keyboardPage);

        mainLayout->addWidget(contentStackedWidget);

        footerWidget = new QWidget(centralWidget);
        footerWidget->setObjectName(QString::fromUtf8("footerWidget"));
        footerWidget->setMinimumSize(QSize(0, 48));
        footerWidget->setMaximumSize(QSize(16777215, 48));
        footerLayout = new QHBoxLayout(footerWidget);
        footerLayout->setSpacing(24);
        footerLayout->setContentsMargins(11, 11, 11, 11);
        footerLayout->setObjectName(QString::fromUtf8("footerLayout"));
        footerLayout->setContentsMargins(24, 0, 24, 0);
        companyLabel = new QLabel(footerWidget);
        companyLabel->setObjectName(QString::fromUtf8("companyLabel"));

        footerLayout->addWidget(companyLabel);

        footerSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        footerLayout->addItem(footerSpacer);

        timeLabel = new QLabel(footerWidget);
        timeLabel->setObjectName(QString::fromUtf8("timeLabel"));

        footerLayout->addWidget(timeLabel);

        divider1 = new QFrame(footerWidget);
        divider1->setObjectName(QString::fromUtf8("divider1"));
        divider1->setMinimumSize(QSize(1, 16));
        divider1->setMaximumSize(QSize(1, 16));
        divider1->setStyleSheet(QString::fromUtf8("background-color: #E0E0E0;"));
        divider1->setFrameShape(QFrame::VLine);

        footerLayout->addWidget(divider1);

        userInfoLabel = new QLabel(footerWidget);
        userInfoLabel->setObjectName(QString::fromUtf8("userInfoLabel"));

        footerLayout->addWidget(userInfoLabel);

        roleLabel = new QLabel(footerWidget);
        roleLabel->setObjectName(QString::fromUtf8("roleLabel"));

        footerLayout->addWidget(roleLabel);

        divider2 = new QFrame(footerWidget);
        divider2->setObjectName(QString::fromUtf8("divider2"));
        divider2->setMinimumSize(QSize(1, 16));
        divider2->setMaximumSize(QSize(1, 16));
        divider2->setStyleSheet(QString::fromUtf8("background-color: #E0E0E0;"));
        divider2->setFrameShape(QFrame::VLine);

        footerLayout->addWidget(divider2);

        cardStatusWidget = new QWidget(footerWidget);
        cardStatusWidget->setObjectName(QString::fromUtf8("cardStatusWidget"));
        cardStatusLayout = new QHBoxLayout(cardStatusWidget);
        cardStatusLayout->setSpacing(6);
        cardStatusLayout->setContentsMargins(11, 11, 11, 11);
        cardStatusLayout->setObjectName(QString::fromUtf8("cardStatusLayout"));
        cardStatusLayout->setContentsMargins(0, 0, 0, 0);
        cardStatusDot = new QLabel(cardStatusWidget);
        cardStatusDot->setObjectName(QString::fromUtf8("cardStatusDot"));
        cardStatusDot->setMinimumSize(QSize(8, 8));
        cardStatusDot->setMaximumSize(QSize(8, 8));
        cardStatusDot->setStyleSheet(QString::fromUtf8("background-color: #2E7D32; border-radius: 4px;"));

        cardStatusLayout->addWidget(cardStatusDot);

        cardStatusLabel = new QLabel(cardStatusWidget);
        cardStatusLabel->setObjectName(QString::fromUtf8("cardStatusLabel"));

        cardStatusLayout->addWidget(cardStatusLabel);


        footerLayout->addWidget(cardStatusWidget);

        fingerStatusWidget = new QWidget(footerWidget);
        fingerStatusWidget->setObjectName(QString::fromUtf8("fingerStatusWidget"));
        fingerStatusLayout = new QHBoxLayout(fingerStatusWidget);
        fingerStatusLayout->setSpacing(6);
        fingerStatusLayout->setContentsMargins(11, 11, 11, 11);
        fingerStatusLayout->setObjectName(QString::fromUtf8("fingerStatusLayout"));
        fingerStatusLayout->setContentsMargins(0, 0, 0, 0);
        fingerStatusDot = new QLabel(fingerStatusWidget);
        fingerStatusDot->setObjectName(QString::fromUtf8("fingerStatusDot"));
        fingerStatusDot->setMinimumSize(QSize(8, 8));
        fingerStatusDot->setMaximumSize(QSize(8, 8));
        fingerStatusDot->setStyleSheet(QString::fromUtf8("background-color: #2E7D32; border-radius: 4px;"));

        fingerStatusLayout->addWidget(fingerStatusDot);

        fingerStatusLabel = new QLabel(fingerStatusWidget);
        fingerStatusLabel->setObjectName(QString::fromUtf8("fingerStatusLabel"));

        fingerStatusLayout->addWidget(fingerStatusLabel);


        footerLayout->addWidget(fingerStatusWidget);


        mainLayout->addWidget(footerWidget);

        MainWindow->setCentralWidget(centralWidget);

        retranslateUi(MainWindow);

        contentStackedWidget->setCurrentIndex(5);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\346\225\260\345\255\227\345\214\226\345\201\234\351\200\201\347\224\265\345\256\211\345\205\250\347\256\241\347\220\206\347\263\273\347\273\237", nullptr));
        logoIcon->setText(QCoreApplication::translate("MainWindow", "\342\232\241", nullptr));
        systemTitle->setText(QCoreApplication::translate("MainWindow", "\346\225\260\345\255\227\345\214\226\345\201\234\351\200\201\347\224\265\345\256\211\345\205\250\347\256\241\347\220\206\347\263\273\347\273\237", nullptr));
        btnWorkbench->setText(QCoreApplication::translate("MainWindow", "\345\267\245\344\275\234\345\217\260", nullptr));
        btnKeys->setText(QCoreApplication::translate("MainWindow", "\351\222\245\345\214\231\347\256\241\347\220\206", nullptr));
        btnSystem->setText(QCoreApplication::translate("MainWindow", "\347\263\273\347\273\237\350\256\276\347\275\256", nullptr));
        btnService->setText(QCoreApplication::translate("MainWindow", "\346\234\215\345\212\241\346\227\245\345\277\227", nullptr));
        btnKeyboard->setText(QCoreApplication::translate("MainWindow", "\350\231\232\346\213\237\351\224\256\347\233\230", nullptr));
        userAvatar->setText(QCoreApplication::translate("MainWindow", "\360\237\221\244", nullptr));
        userNameLabel->setText(QCoreApplication::translate("MainWindow", "\347\256\241\347\220\206\345\221\230", nullptr));
        logoutBtn->setText(QCoreApplication::translate("MainWindow", "\351\200\200\345\207\272", nullptr));
        companyLabel->setText(QCoreApplication::translate("MainWindow", "XXX\347\247\221\346\212\200\345\205\254\345\217\270", nullptr));
        timeLabel->setText(QCoreApplication::translate("MainWindow", "\345\275\223\345\211\215\346\227\266\351\227\264: 2026-01-29 10:30:14", nullptr));
        userInfoLabel->setText(QCoreApplication::translate("MainWindow", "\345\275\223\345\211\215\347\224\250\346\210\267:", nullptr));
        roleLabel->setText(QCoreApplication::translate("MainWindow", "\347\256\241\347\220\206\345\221\230", nullptr));
        cardStatusDot->setText(QString());
        cardStatusLabel->setText(QCoreApplication::translate("MainWindow", "\350\257\273\345\215\241\345\231\250\346\255\243\345\270\270", nullptr));
        fingerStatusDot->setText(QString());
        fingerStatusLabel->setText(QCoreApplication::translate("MainWindow", "\346\214\207\347\272\271\344\273\252\346\255\243\345\270\270", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
