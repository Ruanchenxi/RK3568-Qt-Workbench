/********************************************************************************
** Form generated from reading UI file 'loginpage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGINPAGE_H
#define UI_LOGINPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LoginPage
{
public:
    QHBoxLayout *mainLayout;
    QWidget *leftPanel;
    QVBoxLayout *leftLayout;
    QSpacerItem *leftTopSpacer;
    QHBoxLayout *brandIconLayout;
    QSpacerItem *iconLeftSpacer;
    QFrame *brandIcon;
    QVBoxLayout *brandIconInnerLayout;
    QLabel *brandIconLabel;
    QSpacerItem *iconRightSpacer;
    QLabel *brandTitle;
    QLabel *brandSubtitle;
    QSpacerItem *leftBottomSpacer;
    QWidget *rightPanel;
    QVBoxLayout *rightLayout;
    QSpacerItem *rightTopSpacer;
    QWidget *loginCard;
    QVBoxLayout *loginCardLayout;
    QStackedWidget *loginStackedWidget;
    QWidget *deviceLoginPage;
    QVBoxLayout *deviceLoginLayout;
    QLabel *loginTitle;
    QHBoxLayout *deviceRowLayout;
    QFrame *cardReaderBox;
    QVBoxLayout *cardReaderLayout;
    QHBoxLayout *cardIconLayout;
    QSpacerItem *cardIconLeft;
    QFrame *cardIconBox;
    QVBoxLayout *cardIconBoxLayout;
    QLabel *cardIcon;
    QSpacerItem *cardIconRight;
    QLabel *cardLabel;
    QHBoxLayout *cardStatusLayout;
    QSpacerItem *cardStatusLeft;
    QFrame *cardStatusDot;
    QLabel *cardStatusText;
    QSpacerItem *cardStatusRight;
    QFrame *fingerReaderBox;
    QVBoxLayout *fingerReaderLayout;
    QHBoxLayout *fingerIconLayout;
    QSpacerItem *fingerIconLeft;
    QFrame *fingerIconBox;
    QVBoxLayout *fingerIconBoxLayout;
    QLabel *fingerIcon;
    QSpacerItem *fingerIconRight;
    QLabel *fingerLabel;
    QHBoxLayout *fingerStatusLayout;
    QSpacerItem *fingerStatusLeft;
    QFrame *fingerStatusDot;
    QLabel *fingerStatusText;
    QSpacerItem *fingerStatusRight;
    QLabel *hintLabel;
    QHBoxLayout *dividerLayout;
    QFrame *dividerLine1;
    QLabel *dividerText;
    QFrame *dividerLine2;
    QPushButton *accountLoginBtn;
    QWidget *accountLoginPage;
    QVBoxLayout *accountLoginLayout;
    QLabel *accountLoginTitle;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QPushButton *loginBtn;
    QPushButton *backBtn;
    QSpacerItem *rightBottomSpacer;

    void setupUi(QWidget *LoginPage)
    {
        if (LoginPage->objectName().isEmpty())
            LoginPage->setObjectName(QString::fromUtf8("LoginPage"));
        LoginPage->resize(1280, 608);
        LoginPage->setStyleSheet(QString::fromUtf8("\n"
"/* ========== \347\231\273\345\275\225\351\241\265\351\235\242\346\240\267\345\274\217 - \346\226\271\346\241\210C ========== */\n"
"#LoginPage {\n"
"    background-color: #F5F3EF;\n"
"}\n"
"\n"
"/* \345\267\246\344\276\247\345\223\201\347\211\214\351\235\242\346\235\277 */\n"
"#leftPanel {\n"
"    background-color: #2E7D32;\n"
"}\n"
"\n"
"#brandIcon {\n"
"    background-color: rgba(255, 255, 255, 0.125);\n"
"    border-radius: 16px;\n"
"}\n"
"\n"
"#brandTitle {\n"
"    color: #FFFFFF;\n"
"    font-size: 28px;\n"
"    font-weight: normal;\n"
"}\n"
"\n"
"#brandSubtitle {\n"
"    color: rgba(255, 255, 255, 0.6);\n"
"    font-size: 14px;\n"
"}\n"
"\n"
"/* \345\217\263\344\276\247\347\231\273\345\275\225\351\235\242\346\235\277 */\n"
"#rightPanel {\n"
"    background-color: #FFFFFF;\n"
"}\n"
"\n"
"/* \347\231\273\345\275\225\346\240\207\351\242\230 */\n"
"#loginTitle {\n"
"    color: #333333;\n"
"    font-size: 24px;\n"
"    font-weight: bold;\n"
"}\n"
"\n"
"/* \350\256\276\345\244\207\345\215\241\347\211\207"
                        "\351\200\232\347\224\250\346\240\267\345\274\217 */\n"
"#cardReaderBox, #fingerReaderBox {\n"
"    background-color: #F8F9FA;\n"
"    border-radius: 12px;\n"
"    border: 2px solid #E8F5E9;\n"
"}\n"
"\n"
"#fingerReaderBox {\n"
"    border-color: #E3F2FD;\n"
"}\n"
"\n"
"/* \350\256\276\345\244\207\345\233\276\346\240\207\345\256\271\345\231\250 */\n"
"#cardIconBox {\n"
"    background-color: #E8F5E9;\n"
"    border-radius: 16px;\n"
"}\n"
"\n"
"#fingerIconBox {\n"
"    background-color: #E3F2FD;\n"
"    border-radius: 16px;\n"
"}\n"
"\n"
"/* \350\256\276\345\244\207\346\240\207\347\255\276 */\n"
"#cardLabel, #fingerLabel {\n"
"    color: #333333;\n"
"    font-size: 16px;\n"
"    font-weight: bold;\n"
"}\n"
"\n"
"/* \347\212\266\346\200\201\346\214\207\347\244\272\345\231\250 */\n"
"#cardStatusDot, #fingerStatusDot {\n"
"    background-color: #22C55E;\n"
"    border-radius: 4px;\n"
"}\n"
"\n"
"#cardStatusText, #fingerStatusText {\n"
"    color: #22C55E;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"/* \346\217\220\347\244"
                        "\272\346\226\207\346\234\254 */\n"
"#hintLabel {\n"
"    color: #999999;\n"
"    font-size: 14px;\n"
"}\n"
"\n"
"/* \345\210\206\345\211\262\347\272\277 */\n"
"#dividerLine1, #dividerLine2 {\n"
"    background-color: #E0E0E0;\n"
"}\n"
"\n"
"#dividerText {\n"
"    color: #999999;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"/* \350\264\246\345\217\267\345\257\206\347\240\201\347\231\273\345\275\225\346\214\211\351\222\256 */\n"
"#accountLoginBtn {\n"
"    background-color: #2E7D32;\n"
"    color: #FFFFFF;\n"
"    font-size: 14px;\n"
"    font-weight: bold;\n"
"    border: none;\n"
"    border-radius: 8px;\n"
"    padding: 14px 32px;\n"
"}\n"
"\n"
"#accountLoginBtn:hover {\n"
"    background-color: #1B5E20;\n"
"}\n"
"\n"
"#accountLoginBtn:pressed {\n"
"    background-color: #145214;\n"
"}\n"
"\n"
"/* ========== \350\264\246\345\217\267\345\257\206\347\240\201\347\231\273\345\275\225\345\214\272\345\237\237 ========== */\n"
"#usernameEdit, #passwordEdit {\n"
"    background-color: #FFFFFF;\n"
"    border: 1px solid #E"
                        "0E0E0;\n"
"    border-radius: 8px;\n"
"    padding: 12px 16px;\n"
"    font-size: 14px;\n"
"    color: #333333;\n"
"}\n"
"\n"
"#usernameEdit:focus, #passwordEdit:focus {\n"
"    border-color: #2E7D32;\n"
"}\n"
"\n"
"#loginBtn {\n"
"    background-color: #2E7D32;\n"
"    color: #FFFFFF;\n"
"    font-size: 16px;\n"
"    font-weight: bold;\n"
"    border: none;\n"
"    border-radius: 8px;\n"
"    padding: 14px;\n"
"}\n"
"\n"
"#loginBtn:hover {\n"
"    background-color: #1B5E20;\n"
"}\n"
"\n"
"#backBtn {\n"
"    background-color: transparent;\n"
"    color: #2E7D32;\n"
"    font-size: 14px;\n"
"    border: 1px solid #2E7D32;\n"
"    border-radius: 8px;\n"
"    padding: 10px 20px;\n"
"}\n"
"\n"
"#backBtn:hover {\n"
"    background-color: #E8F5E9;\n"
"}\n"
"   "));
        mainLayout = new QHBoxLayout(LoginPage);
        mainLayout->setSpacing(0);
        mainLayout->setObjectName(QString::fromUtf8("mainLayout"));
        mainLayout->setContentsMargins(0, 0, 0, 0);
        leftPanel = new QWidget(LoginPage);
        leftPanel->setObjectName(QString::fromUtf8("leftPanel"));
        leftPanel->setMinimumSize(QSize(500, 0));
        leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setSpacing(32);
        leftLayout->setObjectName(QString::fromUtf8("leftLayout"));
        leftLayout->setContentsMargins(60, 60, 60, 60);
        leftTopSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        leftLayout->addItem(leftTopSpacer);

        brandIconLayout = new QHBoxLayout();
        brandIconLayout->setObjectName(QString::fromUtf8("brandIconLayout"));
        iconLeftSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        brandIconLayout->addItem(iconLeftSpacer);

        brandIcon = new QFrame(leftPanel);
        brandIcon->setObjectName(QString::fromUtf8("brandIcon"));
        brandIcon->setMinimumSize(QSize(80, 80));
        brandIcon->setMaximumSize(QSize(80, 80));
        brandIconInnerLayout = new QVBoxLayout(brandIcon);
        brandIconInnerLayout->setObjectName(QString::fromUtf8("brandIconInnerLayout"));
        brandIconLabel = new QLabel(brandIcon);
        brandIconLabel->setObjectName(QString::fromUtf8("brandIconLabel"));
        brandIconLabel->setAlignment(Qt::AlignCenter);

        brandIconInnerLayout->addWidget(brandIconLabel);


        brandIconLayout->addWidget(brandIcon);

        iconRightSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        brandIconLayout->addItem(iconRightSpacer);


        leftLayout->addLayout(brandIconLayout);

        brandTitle = new QLabel(leftPanel);
        brandTitle->setObjectName(QString::fromUtf8("brandTitle"));
        brandTitle->setAlignment(Qt::AlignCenter);

        leftLayout->addWidget(brandTitle);

        brandSubtitle = new QLabel(leftPanel);
        brandSubtitle->setObjectName(QString::fromUtf8("brandSubtitle"));
        brandSubtitle->setAlignment(Qt::AlignCenter);

        leftLayout->addWidget(brandSubtitle);

        leftBottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        leftLayout->addItem(leftBottomSpacer);


        mainLayout->addWidget(leftPanel);

        rightPanel = new QWidget(LoginPage);
        rightPanel->setObjectName(QString::fromUtf8("rightPanel"));
        rightPanel->setMinimumSize(QSize(600, 0));
        rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setSpacing(0);
        rightLayout->setObjectName(QString::fromUtf8("rightLayout"));
        rightLayout->setContentsMargins(60, 60, 60, 60);
        rightTopSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);

        rightLayout->addItem(rightTopSpacer);

        loginCard = new QWidget(rightPanel);
        loginCard->setObjectName(QString::fromUtf8("loginCard"));
        loginCard->setMaximumSize(QSize(480, 16777215));
        loginCardLayout = new QVBoxLayout(loginCard);
        loginCardLayout->setSpacing(24);
        loginCardLayout->setObjectName(QString::fromUtf8("loginCardLayout"));
        loginStackedWidget = new QStackedWidget(loginCard);
        loginStackedWidget->setObjectName(QString::fromUtf8("loginStackedWidget"));
        deviceLoginPage = new QWidget();
        deviceLoginPage->setObjectName(QString::fromUtf8("deviceLoginPage"));
        deviceLoginLayout = new QVBoxLayout(deviceLoginPage);
        deviceLoginLayout->setSpacing(24);
        deviceLoginLayout->setObjectName(QString::fromUtf8("deviceLoginLayout"));
        loginTitle = new QLabel(deviceLoginPage);
        loginTitle->setObjectName(QString::fromUtf8("loginTitle"));
        loginTitle->setAlignment(Qt::AlignCenter);

        deviceLoginLayout->addWidget(loginTitle);

        deviceRowLayout = new QHBoxLayout();
        deviceRowLayout->setSpacing(24);
        deviceRowLayout->setObjectName(QString::fromUtf8("deviceRowLayout"));
        cardReaderBox = new QFrame(deviceLoginPage);
        cardReaderBox->setObjectName(QString::fromUtf8("cardReaderBox"));
        cardReaderBox->setMinimumSize(QSize(200, 180));
        cardReaderLayout = new QVBoxLayout(cardReaderBox);
        cardReaderLayout->setSpacing(12);
        cardReaderLayout->setContentsMargins(24, 24, 24, 24);
        cardReaderLayout->setObjectName(QString::fromUtf8("cardReaderLayout"));
        cardIconLayout = new QHBoxLayout();
        cardIconLayout->setObjectName(QString::fromUtf8("cardIconLayout"));
        cardIconLeft = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        cardIconLayout->addItem(cardIconLeft);

        cardIconBox = new QFrame(cardReaderBox);
        cardIconBox->setObjectName(QString::fromUtf8("cardIconBox"));
        cardIconBox->setMinimumSize(QSize(80, 80));
        cardIconBox->setMaximumSize(QSize(80, 80));
        cardIconBoxLayout = new QVBoxLayout(cardIconBox);
        cardIconBoxLayout->setObjectName(QString::fromUtf8("cardIconBoxLayout"));
        cardIcon = new QLabel(cardIconBox);
        cardIcon->setObjectName(QString::fromUtf8("cardIcon"));
        cardIcon->setAlignment(Qt::AlignCenter);

        cardIconBoxLayout->addWidget(cardIcon);


        cardIconLayout->addWidget(cardIconBox);

        cardIconRight = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        cardIconLayout->addItem(cardIconRight);


        cardReaderLayout->addLayout(cardIconLayout);

        cardLabel = new QLabel(cardReaderBox);
        cardLabel->setObjectName(QString::fromUtf8("cardLabel"));
        cardLabel->setAlignment(Qt::AlignCenter);

        cardReaderLayout->addWidget(cardLabel);

        cardStatusLayout = new QHBoxLayout();
        cardStatusLayout->setObjectName(QString::fromUtf8("cardStatusLayout"));
        cardStatusLeft = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        cardStatusLayout->addItem(cardStatusLeft);

        cardStatusDot = new QFrame(cardReaderBox);
        cardStatusDot->setObjectName(QString::fromUtf8("cardStatusDot"));
        cardStatusDot->setMinimumSize(QSize(8, 8));
        cardStatusDot->setMaximumSize(QSize(8, 8));

        cardStatusLayout->addWidget(cardStatusDot);

        cardStatusText = new QLabel(cardReaderBox);
        cardStatusText->setObjectName(QString::fromUtf8("cardStatusText"));

        cardStatusLayout->addWidget(cardStatusText);

        cardStatusRight = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        cardStatusLayout->addItem(cardStatusRight);


        cardReaderLayout->addLayout(cardStatusLayout);


        deviceRowLayout->addWidget(cardReaderBox);

        fingerReaderBox = new QFrame(deviceLoginPage);
        fingerReaderBox->setObjectName(QString::fromUtf8("fingerReaderBox"));
        fingerReaderBox->setMinimumSize(QSize(200, 180));
        fingerReaderLayout = new QVBoxLayout(fingerReaderBox);
        fingerReaderLayout->setSpacing(12);
        fingerReaderLayout->setContentsMargins(24, 24, 24, 24);
        fingerReaderLayout->setObjectName(QString::fromUtf8("fingerReaderLayout"));
        fingerIconLayout = new QHBoxLayout();
        fingerIconLayout->setObjectName(QString::fromUtf8("fingerIconLayout"));
        fingerIconLeft = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        fingerIconLayout->addItem(fingerIconLeft);

        fingerIconBox = new QFrame(fingerReaderBox);
        fingerIconBox->setObjectName(QString::fromUtf8("fingerIconBox"));
        fingerIconBox->setMinimumSize(QSize(80, 80));
        fingerIconBox->setMaximumSize(QSize(80, 80));
        fingerIconBoxLayout = new QVBoxLayout(fingerIconBox);
        fingerIconBoxLayout->setObjectName(QString::fromUtf8("fingerIconBoxLayout"));
        fingerIcon = new QLabel(fingerIconBox);
        fingerIcon->setObjectName(QString::fromUtf8("fingerIcon"));
        fingerIcon->setAlignment(Qt::AlignCenter);

        fingerIconBoxLayout->addWidget(fingerIcon);


        fingerIconLayout->addWidget(fingerIconBox);

        fingerIconRight = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        fingerIconLayout->addItem(fingerIconRight);


        fingerReaderLayout->addLayout(fingerIconLayout);

        fingerLabel = new QLabel(fingerReaderBox);
        fingerLabel->setObjectName(QString::fromUtf8("fingerLabel"));
        fingerLabel->setAlignment(Qt::AlignCenter);

        fingerReaderLayout->addWidget(fingerLabel);

        fingerStatusLayout = new QHBoxLayout();
        fingerStatusLayout->setObjectName(QString::fromUtf8("fingerStatusLayout"));
        fingerStatusLeft = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        fingerStatusLayout->addItem(fingerStatusLeft);

        fingerStatusDot = new QFrame(fingerReaderBox);
        fingerStatusDot->setObjectName(QString::fromUtf8("fingerStatusDot"));
        fingerStatusDot->setMinimumSize(QSize(8, 8));
        fingerStatusDot->setMaximumSize(QSize(8, 8));

        fingerStatusLayout->addWidget(fingerStatusDot);

        fingerStatusText = new QLabel(fingerReaderBox);
        fingerStatusText->setObjectName(QString::fromUtf8("fingerStatusText"));

        fingerStatusLayout->addWidget(fingerStatusText);

        fingerStatusRight = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

        fingerStatusLayout->addItem(fingerStatusRight);


        fingerReaderLayout->addLayout(fingerStatusLayout);


        deviceRowLayout->addWidget(fingerReaderBox);


        deviceLoginLayout->addLayout(deviceRowLayout);

        hintLabel = new QLabel(deviceLoginPage);
        hintLabel->setObjectName(QString::fromUtf8("hintLabel"));
        hintLabel->setAlignment(Qt::AlignCenter);

        deviceLoginLayout->addWidget(hintLabel);

        dividerLayout = new QHBoxLayout();
        dividerLayout->setSpacing(16);
        dividerLayout->setObjectName(QString::fromUtf8("dividerLayout"));
        dividerLine1 = new QFrame(deviceLoginPage);
        dividerLine1->setObjectName(QString::fromUtf8("dividerLine1"));
        dividerLine1->setMinimumSize(QSize(0, 1));
        dividerLine1->setMaximumSize(QSize(16777215, 1));

        dividerLayout->addWidget(dividerLine1);

        dividerText = new QLabel(deviceLoginPage);
        dividerText->setObjectName(QString::fromUtf8("dividerText"));

        dividerLayout->addWidget(dividerText);

        dividerLine2 = new QFrame(deviceLoginPage);
        dividerLine2->setObjectName(QString::fromUtf8("dividerLine2"));
        dividerLine2->setMinimumSize(QSize(0, 1));
        dividerLine2->setMaximumSize(QSize(16777215, 1));

        dividerLayout->addWidget(dividerLine2);


        deviceLoginLayout->addLayout(dividerLayout);

        accountLoginBtn = new QPushButton(deviceLoginPage);
        accountLoginBtn->setObjectName(QString::fromUtf8("accountLoginBtn"));
        accountLoginBtn->setMinimumSize(QSize(0, 48));

        deviceLoginLayout->addWidget(accountLoginBtn);

        loginStackedWidget->addWidget(deviceLoginPage);
        accountLoginPage = new QWidget();
        accountLoginPage->setObjectName(QString::fromUtf8("accountLoginPage"));
        accountLoginLayout = new QVBoxLayout(accountLoginPage);
        accountLoginLayout->setSpacing(20);
        accountLoginLayout->setObjectName(QString::fromUtf8("accountLoginLayout"));
        accountLoginTitle = new QLabel(accountLoginPage);
        accountLoginTitle->setObjectName(QString::fromUtf8("accountLoginTitle"));
        accountLoginTitle->setAlignment(Qt::AlignCenter);

        accountLoginLayout->addWidget(accountLoginTitle);

        usernameEdit = new QLineEdit(accountLoginPage);
        usernameEdit->setObjectName(QString::fromUtf8("usernameEdit"));
        usernameEdit->setMinimumSize(QSize(0, 48));

        accountLoginLayout->addWidget(usernameEdit);

        passwordEdit = new QLineEdit(accountLoginPage);
        passwordEdit->setObjectName(QString::fromUtf8("passwordEdit"));
        passwordEdit->setEchoMode(QLineEdit::Password);
        passwordEdit->setMinimumSize(QSize(0, 48));

        accountLoginLayout->addWidget(passwordEdit);

        loginBtn = new QPushButton(accountLoginPage);
        loginBtn->setObjectName(QString::fromUtf8("loginBtn"));
        loginBtn->setMinimumSize(QSize(0, 48));

        accountLoginLayout->addWidget(loginBtn);

        backBtn = new QPushButton(accountLoginPage);
        backBtn->setObjectName(QString::fromUtf8("backBtn"));
        backBtn->setMinimumSize(QSize(0, 40));

        accountLoginLayout->addWidget(backBtn);

        loginStackedWidget->addWidget(accountLoginPage);

        loginCardLayout->addWidget(loginStackedWidget);


        rightLayout->addWidget(loginCard);

        rightBottomSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);

        rightLayout->addItem(rightBottomSpacer);


        mainLayout->addWidget(rightPanel);


        retranslateUi(LoginPage);

        loginStackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(LoginPage);
    } // setupUi

    void retranslateUi(QWidget *LoginPage)
    {
        brandIconLabel->setText(QCoreApplication::translate("LoginPage", "\342\232\241", nullptr));
        brandIconLabel->setStyleSheet(QCoreApplication::translate("LoginPage", "font-size: 40px;", nullptr));
        brandTitle->setText(QCoreApplication::translate("LoginPage", "\346\225\260\345\255\227\345\214\226\345\201\234\351\200\201\347\224\265\345\256\211\345\205\250\347\256\241\347\220\206\347\263\273\347\273\237", nullptr));
        brandSubtitle->setText(QCoreApplication::translate("LoginPage", "Digital Power Safety Management System", nullptr));
        loginTitle->setText(QCoreApplication::translate("LoginPage", "\350\257\267\351\200\211\346\213\251\347\231\273\345\275\225\346\226\271\345\274\217", nullptr));
        cardIcon->setText(QCoreApplication::translate("LoginPage", "\360\237\222\263", nullptr));
        cardIcon->setStyleSheet(QCoreApplication::translate("LoginPage", "font-size: 40px;", nullptr));
        cardLabel->setText(QCoreApplication::translate("LoginPage", "\345\210\267\345\215\241\347\231\273\345\275\225", nullptr));
        cardStatusText->setText(QCoreApplication::translate("LoginPage", "\346\243\200\346\265\213\344\270\255...", nullptr));
        fingerIcon->setText(QCoreApplication::translate("LoginPage", "\360\237\221\206", nullptr));
        fingerIcon->setStyleSheet(QCoreApplication::translate("LoginPage", "font-size: 40px;", nullptr));
        fingerLabel->setText(QCoreApplication::translate("LoginPage", "\346\214\207\347\272\271\347\231\273\345\275\225", nullptr));
        fingerStatusText->setText(QCoreApplication::translate("LoginPage", "\346\243\200\346\265\213\344\270\255...", nullptr));
        hintLabel->setText(QCoreApplication::translate("LoginPage", "\350\257\267\345\210\267\345\215\241\346\210\226\346\214\211\346\214\207\347\272\271\357\274\214\347\263\273\347\273\237\345\260\206\350\207\252\345\212\250\350\257\206\345\210\253\347\231\273\345\275\225", nullptr));
        dividerText->setText(QCoreApplication::translate("LoginPage", "\346\210\226", nullptr));
        accountLoginBtn->setText(QCoreApplication::translate("LoginPage", "\360\237\221\244 \350\264\246\345\217\267\345\257\206\347\240\201\347\231\273\345\275\225", nullptr));
        accountLoginTitle->setText(QCoreApplication::translate("LoginPage", "\350\264\246\345\217\267\345\257\206\347\240\201\347\231\273\345\275\225", nullptr));
        accountLoginTitle->setStyleSheet(QCoreApplication::translate("LoginPage", "color: #333333; font-size: 24px; font-weight: bold;", nullptr));
        usernameEdit->setPlaceholderText(QCoreApplication::translate("LoginPage", "\350\257\267\350\276\223\345\205\245\347\224\250\346\210\267\345\220\215", nullptr));
        passwordEdit->setPlaceholderText(QCoreApplication::translate("LoginPage", "\350\257\267\350\276\223\345\205\245\345\257\206\347\240\201", nullptr));
        loginBtn->setText(QCoreApplication::translate("LoginPage", "\347\231\273 \345\275\225", nullptr));
        backBtn->setText(QCoreApplication::translate("LoginPage", "\342\206\220 \350\277\224\345\233\236\345\210\267\345\215\241/\346\214\207\347\272\271\347\231\273\345\275\225", nullptr));
        (void)LoginPage;
    } // retranslateUi

};

namespace Ui {
    class LoginPage: public Ui_LoginPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGINPAGE_H
