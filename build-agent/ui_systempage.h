/********************************************************************************
** Form generated from reading UI file 'systempage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SYSTEMPAGE_H
#define UI_SYSTEMPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_SystemPage
{
public:
    QVBoxLayout *mainLayout;
    QHBoxLayout *pageHeader;
    QLabel *pageTitle;
    QSpacerItem *headerSpacer1;
    QWidget *sysTabs;
    QHBoxLayout *sysTabsLayout;
    QPushButton *tabBasic;
    QPushButton *tabAdvanced;
    QFrame *sysFormPanel;
    QVBoxLayout *sysFormPanelLayout;
    QWidget *formHeader;
    QHBoxLayout *formHeaderLayout;
    QLabel *formTitle;
    QSpacerItem *formHeaderSpacer;
    QPushButton *refreshBtn;
    QPushButton *saveBtn;
    QScrollArea *formScrollArea;
    QWidget *formBody;
    QGridLayout *gridLayout;
    QVBoxLayout *verticalLayout;
    QLabel *field1Label;
    QLineEdit *homeUrlEdit;
    QVBoxLayout *field2Layout;
    QLabel *field2Label;
    QLineEdit *apiUrlEdit;
    QVBoxLayout *field3Layout;
    QLabel *field3Label;
    QLineEdit *stationIdEdit;
    QVBoxLayout *field4Layout;
    QLabel *field4Label;
    QLineEdit *tenantCodeEdit;
    QFrame *divider1;
    QLabel *sectionTitle1;
    QVBoxLayout *field5Layout;
    QLabel *field5Label;
    QComboBox *keySerialCombo;
    QVBoxLayout *field6Layout;
    QLabel *field6Label;
    QComboBox *cardSerialCombo;
    QVBoxLayout *field7Layout;
    QLabel *field7Label;
    QComboBox *baudRateCombo;
    QVBoxLayout *field8Layout;
    QLabel *field8Label;
    QComboBox *dataBitsCombo;
    QSpacerItem *formBottomSpacer;

    void setupUi(QWidget *SystemPage)
    {
        if (SystemPage->objectName().isEmpty())
            SystemPage->setObjectName(QString::fromUtf8("SystemPage"));
        SystemPage->resize(1280, 608);
        SystemPage->setStyleSheet(QString::fromUtf8("\n"
"/* ========== \347\263\273\347\273\237\350\256\276\347\275\256\351\241\265\351\235\242\346\240\267\345\274\217 ========== */\n"
"#SystemPage {\n"
"    background-color: transparent;\n"
"}\n"
"\n"
"/* \351\241\265\351\235\242\346\240\207\351\242\230 */\n"
"#pageTitle {\n"
"    color: #2C3E50;\n"
"    font-size: 24px;\n"
"    font-weight: normal;\n"
"}\n"
"\n"
"/* \346\240\207\347\255\276\351\241\265\345\210\207\346\215\242\346\214\211\351\222\256 - \345\237\272\347\241\200\346\240\267\345\274\217 */\n"
"#tabBasic, #tabAdvanced, #tabUserIdentity {\n"
"    background-color: transparent;\n"
"    color: #5D6B7A;\n"
"    font-size: 13px;\n"
"    border: none;\n"
"    border-radius: 6px;\n"
"    padding: 8px 16px;\n"
"    min-width: 80px;\n"
"}\n"
"\n"
"#tabBasic:hover, #tabAdvanced:hover, #tabUserIdentity:hover {\n"
"    background-color: #E8F5E9;\n"
"}\n"
"\n"
"#tabBasic:pressed, #tabAdvanced:pressed, #tabUserIdentity:pressed {\n"
"    background-color: #C8E6C9;\n"
"}\n"
"\n"
"#tabBasic:checked, #tabAdvanced:c"
                        "hecked, #tabUserIdentity:checked {\n"
"    background-color: #2E7D32;\n"
"    color: #FFFFFF;\n"
"}\n"
"\n"
"/* \350\241\250\345\215\225\351\235\242\346\235\277 */\n"
"#sysFormPanel {\n"
"    background-color: #FFFFFF;\n"
"    border: 1px solid #E8E4DE;\n"
"    border-radius: 12px;\n"
"}\n"
"\n"
"/* \350\241\250\345\215\225\346\240\207\351\242\230 */\n"
"#formTitle {\n"
"    color: #2C3E50;\n"
"    font-size: 16px;\n"
"    border: none;\n"
"    background: transparent;\n"
"}\n"
"\n"
"/* \345\210\206\345\214\272\346\240\207\351\242\230 - \345\212\240\347\262\227\351\206\222\347\233\256 */\n"
"#sectionTitle1, .sectionTitle {\n"
"    color: #2C3E50;\n"
"    font-size: 14px;\n"
"    font-weight: bold;\n"
"    background: transparent;\n"
"    border: none;\n"
"    padding: 8px 0;\n"
"}\n"
"\n"
"/* \345\255\227\346\256\265\346\240\207\347\255\276 - \346\227\240\350\276\271\346\241\206\347\272\257\346\226\207\345\255\227 */\n"
"#field1Label, #field2Label, #field3Label, #field4Label, \n"
"#field5Label, #field6Label, #"
                        "field7Label, #field8Label {\n"
"    color: #5D6B7A;\n"
"    font-size: 13px;\n"
"    background: transparent;\n"
"    border: none;\n"
"    padding: 0;\n"
"}\n"
"\n"
"/* \350\276\223\345\205\245\346\241\206 */\n"
"QLineEdit {\n"
"    background-color: #F5F3EF;\n"
"    border: 1px solid #E8E4DE;\n"
"    border-radius: 8px;\n"
"    padding: 12px 16px;\n"
"    font-size: 14px;\n"
"    color: #2C3E50;\n"
"    min-height: 20px;\n"
"}\n"
"\n"
"/* \344\270\213\346\213\211\346\241\206 */\n"
"QComboBox {\n"
"    background-color: #F5F3EF;\n"
"    border: 1px solid #E8E4DE;\n"
"    border-radius: 8px;\n"
"    padding: 12px 16px;\n"
"    font-size: 14px;\n"
"    color: #2C3E50;\n"
"    min-width: 140px;\n"
"    min-height: 20px;\n"
"}\n"
"\n"
"QLineEdit:focus, QComboBox:focus {\n"
"    border-color: #2E7D32;\n"
"}\n"
"\n"
"QComboBox::drop-down {\n"
"    border: none;\n"
"    width: 30px;\n"
"}\n"
"\n"
"QComboBox::down-arrow {\n"
"    image: none;\n"
"}\n"
"\n"
"/* \345\210\267\346\226\260\346\214\211\351\222\256 - \347\202"
                        "\271\345\207\273\346\225\210\346\236\234 */\n"
"#refreshBtn {\n"
"    background-color: #FFFFFF;\n"
"    color: #5D6B7A;\n"
"    font-size: 13px;\n"
"    border: 1px solid #E0E0E0;\n"
"    border-radius: 6px;\n"
"    padding: 8px 16px;\n"
"    min-width: 60px;\n"
"}\n"
"\n"
"#refreshBtn:pressed {\n"
"    background-color: #BDBDBD;\n"
"    border-color: #888888;\n"
"    color: #333333;\n"
"}\n"
"\n"
"/* \344\277\235\345\255\230\346\214\211\351\222\256 - \347\202\271\345\207\273\346\225\210\346\236\234 */\n"
"#saveBtn {\n"
"    background-color: #2E7D32;\n"
"    color: #FFFFFF;\n"
"    font-size: 13px;\n"
"    border: none;\n"
"    border-radius: 6px;\n"
"    padding: 8px 16px;\n"
"    min-width: 80px;\n"
"}\n"
"\n"
"#saveBtn:pressed {\n"
"    background-color: #145A14;\n"
"}\n"
"\n"
"/* \345\210\206\351\232\224\347\272\277 */\n"
".divider {\n"
"    background-color: #E8E4DE;\n"
"}\n"
"   "));
        mainLayout = new QVBoxLayout(SystemPage);
        mainLayout->setSpacing(20);
        mainLayout->setObjectName(QString::fromUtf8("mainLayout"));
        mainLayout->setContentsMargins(24, 24, 24, 24);
        pageHeader = new QHBoxLayout();
        pageHeader->setSpacing(0);
        pageHeader->setObjectName(QString::fromUtf8("pageHeader"));
        pageTitle = new QLabel(SystemPage);
        pageTitle->setObjectName(QString::fromUtf8("pageTitle"));

        pageHeader->addWidget(pageTitle);

        headerSpacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        pageHeader->addItem(headerSpacer1);

        sysTabs = new QWidget(SystemPage);
        sysTabs->setObjectName(QString::fromUtf8("sysTabs"));
        sysTabs->setStyleSheet(QString::fromUtf8("background-color: #FFFFFF; border: 1px solid #E8E4DE; border-radius: 8px; padding: 4px;"));
        sysTabsLayout = new QHBoxLayout(sysTabs);
        sysTabsLayout->setSpacing(4);
        sysTabsLayout->setObjectName(QString::fromUtf8("sysTabsLayout"));
        sysTabsLayout->setContentsMargins(4, 4, 4, 4);
        tabBasic = new QPushButton(sysTabs);
        tabBasic->setObjectName(QString::fromUtf8("tabBasic"));
        tabBasic->setCheckable(true);
        tabBasic->setChecked(true);
        tabBasic->setAutoExclusive(true);

        sysTabsLayout->addWidget(tabBasic);

        tabAdvanced = new QPushButton(sysTabs);
        tabAdvanced->setObjectName(QString::fromUtf8("tabAdvanced"));
        tabAdvanced->setCheckable(true);
        tabAdvanced->setAutoExclusive(true);

        sysTabsLayout->addWidget(tabAdvanced);


        pageHeader->addWidget(sysTabs);


        mainLayout->addLayout(pageHeader);

        sysFormPanel = new QFrame(SystemPage);
        sysFormPanel->setObjectName(QString::fromUtf8("sysFormPanel"));
        sysFormPanel->setStyleSheet(QString::fromUtf8("background-color: #FFFFFF; border: 1px solid #E8E4DE; border-radius: 12px;"));
        sysFormPanel->setFrameShape(QFrame::StyledPanel);
        sysFormPanelLayout = new QVBoxLayout(sysFormPanel);
        sysFormPanelLayout->setSpacing(0);
        sysFormPanelLayout->setObjectName(QString::fromUtf8("sysFormPanelLayout"));
        sysFormPanelLayout->setContentsMargins(0, 0, 0, 0);
        formHeader = new QWidget(sysFormPanel);
        formHeader->setObjectName(QString::fromUtf8("formHeader"));
        formHeader->setStyleSheet(QString::fromUtf8("border-bottom: 1px solid #E8E4DE; background: transparent;"));
        formHeaderLayout = new QHBoxLayout(formHeader);
        formHeaderLayout->setSpacing(12);
        formHeaderLayout->setObjectName(QString::fromUtf8("formHeaderLayout"));
        formHeaderLayout->setContentsMargins(20, 16, 20, 16);
        formTitle = new QLabel(formHeader);
        formTitle->setObjectName(QString::fromUtf8("formTitle"));

        formHeaderLayout->addWidget(formTitle);

        formHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        formHeaderLayout->addItem(formHeaderSpacer);

        refreshBtn = new QPushButton(formHeader);
        refreshBtn->setObjectName(QString::fromUtf8("refreshBtn"));
        refreshBtn->setStyleSheet(QString::fromUtf8("QPushButton { background-color: #FFFFFF; color: #5D6B7A; font-size: 13px; border: 1px solid #E0E0E0; border-radius: 6px; padding: 8px 16px; min-width: 60px; } QPushButton:pressed { background-color: #BDBDBD; border-color: #888888; color: #333333; }"));

        formHeaderLayout->addWidget(refreshBtn);

        saveBtn = new QPushButton(formHeader);
        saveBtn->setObjectName(QString::fromUtf8("saveBtn"));
        saveBtn->setStyleSheet(QString::fromUtf8("QPushButton { background-color: #2E7D32; color: #FFFFFF; font-size: 13px; border: none; border-radius: 6px; padding: 8px 16px; min-width: 80px; } QPushButton:pressed { background-color: #145A14; }"));

        formHeaderLayout->addWidget(saveBtn);


        sysFormPanelLayout->addWidget(formHeader);

        formScrollArea = new QScrollArea(sysFormPanel);
        formScrollArea->setObjectName(QString::fromUtf8("formScrollArea"));
        formScrollArea->setStyleSheet(QString::fromUtf8("QScrollArea { border: none; background: transparent; }"));
        formScrollArea->setWidgetResizable(true);
        formBody = new QWidget();
        formBody->setObjectName(QString::fromUtf8("formBody"));
        formBody->setGeometry(QRect(0, 0, 1230, 442));
        gridLayout = new QGridLayout(formBody);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        field1Label = new QLabel(formBody);
        field1Label->setObjectName(QString::fromUtf8("field1Label"));
        field1Label->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 13px; background: transparent; border: none;"));

        verticalLayout->addWidget(field1Label);

        homeUrlEdit = new QLineEdit(formBody);
        homeUrlEdit->setObjectName(QString::fromUtf8("homeUrlEdit"));

        verticalLayout->addWidget(homeUrlEdit);


        gridLayout->addLayout(verticalLayout, 0, 0, 1, 1);

        field2Layout = new QVBoxLayout();
        field2Layout->setSpacing(8);
        field2Layout->setObjectName(QString::fromUtf8("field2Layout"));
        field2Label = new QLabel(formBody);
        field2Label->setObjectName(QString::fromUtf8("field2Label"));
        field2Label->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 13px; background: transparent; border: none;"));

        field2Layout->addWidget(field2Label);

        apiUrlEdit = new QLineEdit(formBody);
        apiUrlEdit->setObjectName(QString::fromUtf8("apiUrlEdit"));

        field2Layout->addWidget(apiUrlEdit);


        gridLayout->addLayout(field2Layout, 0, 1, 1, 1);

        field3Layout = new QVBoxLayout();
        field3Layout->setSpacing(8);
        field3Layout->setObjectName(QString::fromUtf8("field3Layout"));
        field3Label = new QLabel(formBody);
        field3Label->setObjectName(QString::fromUtf8("field3Label"));
        field3Label->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 13px; background: transparent; border: none;"));

        field3Layout->addWidget(field3Label);

        stationIdEdit = new QLineEdit(formBody);
        stationIdEdit->setObjectName(QString::fromUtf8("stationIdEdit"));

        field3Layout->addWidget(stationIdEdit);


        gridLayout->addLayout(field3Layout, 1, 0, 1, 1);

        field4Layout = new QVBoxLayout();
        field4Layout->setSpacing(8);
        field4Layout->setObjectName(QString::fromUtf8("field4Layout"));
        field4Label = new QLabel(formBody);
        field4Label->setObjectName(QString::fromUtf8("field4Label"));
        field4Label->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 13px; background: transparent; border: none;"));

        field4Layout->addWidget(field4Label);

        tenantCodeEdit = new QLineEdit(formBody);
        tenantCodeEdit->setObjectName(QString::fromUtf8("tenantCodeEdit"));

        field4Layout->addWidget(tenantCodeEdit);


        gridLayout->addLayout(field4Layout, 1, 1, 1, 1);

        divider1 = new QFrame(formBody);
        divider1->setObjectName(QString::fromUtf8("divider1"));
        divider1->setMinimumSize(QSize(0, 1));
        divider1->setMaximumSize(QSize(16777215, 1));
        divider1->setStyleSheet(QString::fromUtf8("background-color: #E8E4DE;"));
        divider1->setFrameShape(QFrame::HLine);

        gridLayout->addWidget(divider1, 2, 0, 1, 2);

        sectionTitle1 = new QLabel(formBody);
        sectionTitle1->setObjectName(QString::fromUtf8("sectionTitle1"));
        sectionTitle1->setStyleSheet(QString::fromUtf8("color: #2C3E50; font-size: 14px; font-weight: bold; background: transparent; border: none;"));

        gridLayout->addWidget(sectionTitle1, 3, 0, 1, 2);

        field5Layout = new QVBoxLayout();
        field5Layout->setSpacing(8);
        field5Layout->setObjectName(QString::fromUtf8("field5Layout"));
        field5Label = new QLabel(formBody);
        field5Label->setObjectName(QString::fromUtf8("field5Label"));
        field5Label->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 13px; background: transparent; border: none;"));

        field5Layout->addWidget(field5Label);

        keySerialCombo = new QComboBox(formBody);
        keySerialCombo->addItem(QString());
        keySerialCombo->addItem(QString());
        keySerialCombo->addItem(QString());
        keySerialCombo->addItem(QString());
        keySerialCombo->setObjectName(QString::fromUtf8("keySerialCombo"));

        field5Layout->addWidget(keySerialCombo);


        gridLayout->addLayout(field5Layout, 4, 0, 1, 1);

        field6Layout = new QVBoxLayout();
        field6Layout->setSpacing(8);
        field6Layout->setObjectName(QString::fromUtf8("field6Layout"));
        field6Label = new QLabel(formBody);
        field6Label->setObjectName(QString::fromUtf8("field6Label"));
        field6Label->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 13px; background: transparent; border: none;"));

        field6Layout->addWidget(field6Label);

        cardSerialCombo = new QComboBox(formBody);
        cardSerialCombo->addItem(QString());
        cardSerialCombo->addItem(QString());
        cardSerialCombo->addItem(QString());
        cardSerialCombo->addItem(QString());
        cardSerialCombo->setObjectName(QString::fromUtf8("cardSerialCombo"));

        field6Layout->addWidget(cardSerialCombo);


        gridLayout->addLayout(field6Layout, 4, 1, 1, 1);

        field7Layout = new QVBoxLayout();
        field7Layout->setSpacing(8);
        field7Layout->setObjectName(QString::fromUtf8("field7Layout"));
        field7Label = new QLabel(formBody);
        field7Label->setObjectName(QString::fromUtf8("field7Label"));
        field7Label->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 13px; background: transparent; border: none;"));

        field7Layout->addWidget(field7Label);

        baudRateCombo = new QComboBox(formBody);
        baudRateCombo->addItem(QString());
        baudRateCombo->addItem(QString());
        baudRateCombo->addItem(QString());
        baudRateCombo->addItem(QString());
        baudRateCombo->setObjectName(QString::fromUtf8("baudRateCombo"));

        field7Layout->addWidget(baudRateCombo);


        gridLayout->addLayout(field7Layout, 5, 0, 1, 1);

        field8Layout = new QVBoxLayout();
        field8Layout->setSpacing(8);
        field8Layout->setObjectName(QString::fromUtf8("field8Layout"));
        field8Label = new QLabel(formBody);
        field8Label->setObjectName(QString::fromUtf8("field8Label"));
        field8Label->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 13px; background: transparent; border: none;"));

        field8Layout->addWidget(field8Label);

        dataBitsCombo = new QComboBox(formBody);
        dataBitsCombo->addItem(QString());
        dataBitsCombo->addItem(QString());
        dataBitsCombo->setObjectName(QString::fromUtf8("dataBitsCombo"));

        field8Layout->addWidget(dataBitsCombo);


        gridLayout->addLayout(field8Layout, 5, 1, 1, 1);

        formBottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(formBottomSpacer, 6, 0, 1, 2);

        formScrollArea->setWidget(formBody);

        sysFormPanelLayout->addWidget(formScrollArea);


        mainLayout->addWidget(sysFormPanel);


        retranslateUi(SystemPage);

        QMetaObject::connectSlotsByName(SystemPage);
    } // setupUi

    void retranslateUi(QWidget *SystemPage)
    {
        pageTitle->setText(QCoreApplication::translate("SystemPage", "\347\263\273\347\273\237\350\256\276\347\275\256", nullptr));
        tabBasic->setText(QCoreApplication::translate("SystemPage", "\345\237\272\346\234\254\351\205\215\347\275\256", nullptr));
        tabAdvanced->setText(QCoreApplication::translate("SystemPage", "\347\224\250\346\210\267\350\272\253\344\273\275\351\207\207\351\233\206", nullptr));
        formTitle->setText(QCoreApplication::translate("SystemPage", "\347\263\273\347\273\237\345\217\202\346\225\260\350\256\276\347\275\256", nullptr));
        refreshBtn->setText(QCoreApplication::translate("SystemPage", "\345\210\267\346\226\260", nullptr));
        saveBtn->setText(QCoreApplication::translate("SystemPage", "\344\277\235\345\255\230\351\205\215\347\275\256", nullptr));
        field1Label->setText(QCoreApplication::translate("SystemPage", "\351\246\226\351\241\265\345\234\260\345\235\200", nullptr));
        homeUrlEdit->setText(QCoreApplication::translate("SystemPage", "http://192.168.1.100:8080", nullptr));
        field2Label->setText(QCoreApplication::translate("SystemPage", "\346\216\245\345\217\243\345\234\260\345\235\200", nullptr));
        apiUrlEdit->setText(QCoreApplication::translate("SystemPage", "http://192.168.1.100:8080/api", nullptr));
        field3Label->setText(QCoreApplication::translate("SystemPage", "\345\275\223\345\211\215\347\253\231\345\217\267", nullptr));
        stationIdEdit->setText(QCoreApplication::translate("SystemPage", "001", nullptr));
        field4Label->setText(QCoreApplication::translate("SystemPage", "\347\247\237\346\210\267\347\274\226\347\240\201", nullptr));
        tenantCodeEdit->setText(QCoreApplication::translate("SystemPage", "TENANT001", nullptr));
        sectionTitle1->setText(QCoreApplication::translate("SystemPage", "\344\270\262\345\217\243\351\205\215\347\275\256", nullptr));
        field5Label->setText(QCoreApplication::translate("SystemPage", "\351\222\245\345\214\231\344\270\262\345\217\243", nullptr));
        keySerialCombo->setItemText(0, QCoreApplication::translate("SystemPage", "COM1", nullptr));
        keySerialCombo->setItemText(1, QCoreApplication::translate("SystemPage", "COM2", nullptr));
        keySerialCombo->setItemText(2, QCoreApplication::translate("SystemPage", "COM3", nullptr));
        keySerialCombo->setItemText(3, QCoreApplication::translate("SystemPage", "COM4", nullptr));

        field6Label->setText(QCoreApplication::translate("SystemPage", "\350\257\273\345\215\241\344\270\262\345\217\243", nullptr));
        cardSerialCombo->setItemText(0, QCoreApplication::translate("SystemPage", "COM1", nullptr));
        cardSerialCombo->setItemText(1, QCoreApplication::translate("SystemPage", "COM2", nullptr));
        cardSerialCombo->setItemText(2, QCoreApplication::translate("SystemPage", "COM3", nullptr));
        cardSerialCombo->setItemText(3, QCoreApplication::translate("SystemPage", "COM4", nullptr));

        field7Label->setText(QCoreApplication::translate("SystemPage", "\346\263\242\347\211\271\347\216\207", nullptr));
        baudRateCombo->setItemText(0, QCoreApplication::translate("SystemPage", "9600", nullptr));
        baudRateCombo->setItemText(1, QCoreApplication::translate("SystemPage", "19200", nullptr));
        baudRateCombo->setItemText(2, QCoreApplication::translate("SystemPage", "38400", nullptr));
        baudRateCombo->setItemText(3, QCoreApplication::translate("SystemPage", "115200", nullptr));

        field8Label->setText(QCoreApplication::translate("SystemPage", "\346\225\260\346\215\256\344\275\215", nullptr));
        dataBitsCombo->setItemText(0, QCoreApplication::translate("SystemPage", "7", nullptr));
        dataBitsCombo->setItemText(1, QCoreApplication::translate("SystemPage", "8", nullptr));

        (void)SystemPage;
    } // retranslateUi

};

namespace Ui {
    class SystemPage: public Ui_SystemPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SYSTEMPAGE_H
