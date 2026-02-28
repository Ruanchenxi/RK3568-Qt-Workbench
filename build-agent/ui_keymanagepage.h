/********************************************************************************
** Form generated from reading UI file 'keymanagepage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_KEYMANAGEPAGE_H
#define UI_KEYMANAGEPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_KeyManagePage
{
public:
    QVBoxLayout *mainLayout;
    QHBoxLayout *pageHeader;
    QWidget *keysTabs;
    QHBoxLayout *keysTabsLayout;
    QPushButton *tabAll;
    QPushButton *tabBorrowed;
    QPushButton *tabReturned;
    QPushButton *tabAbnormal;
    QSpacerItem *headerSpacer;
    QHBoxLayout *mainContent;
    QFrame *keyCabinet;
    QVBoxLayout *keyCabinetLayout;
    QWidget *cabinetHeader;
    QHBoxLayout *cabinetHeaderLayout;
    QLabel *cabinetTitle;
    QSpacerItem *cabinetHeaderSpacer;
    QWidget *cabinetStatus;
    QHBoxLayout *cabinetStatusLayout;
    QLabel *cabinetStatusDot;
    QLabel *cabinetStatusText;
    QWidget *cabinetGrid;
    QVBoxLayout *cabinetGridLayout;
    QHBoxLayout *keyRow1;
    QFrame *key1;
    QVBoxLayout *key1Layout;
    QLabel *key1Num;
    QLabel *key1Status;
    QFrame *key2;
    QVBoxLayout *key2Layout;
    QLabel *key2Num;
    QLabel *key2Status;
    QFrame *key3;
    QVBoxLayout *key3Layout;
    QLabel *key3Num;
    QLabel *key3Status;
    QFrame *key4;
    QVBoxLayout *key4Layout;
    QLabel *key4Num;
    QLabel *key4Status;
    QHBoxLayout *keyRow2;
    QFrame *key5;
    QVBoxLayout *key5Layout;
    QLabel *key5Num;
    QLabel *key5Status;
    QFrame *key6;
    QVBoxLayout *key6Layout;
    QLabel *key6Num;
    QLabel *key6Status;
    QFrame *key7;
    QVBoxLayout *key7Layout;
    QLabel *key7Num;
    QLabel *key7Status;
    QFrame *key8;
    QVBoxLayout *key8Layout;
    QLabel *key8Num;
    QLabel *key8Status;
    QHBoxLayout *keyLegend;
    QSpacerItem *legendLeftSpacer;
    QWidget *legendIn;
    QHBoxLayout *legendInLayout;
    QLabel *legendInDot;
    QLabel *legendInText;
    QWidget *legendOut;
    QHBoxLayout *legendOutLayout;
    QLabel *legendOutDot;
    QLabel *legendOutText;
    QWidget *legendErr;
    QHBoxLayout *legendErrLayout;
    QLabel *legendErrDot;
    QLabel *legendErrText;
    QSpacerItem *legendRightSpacer;
    QSpacerItem *cabinetBottomSpacer;
    QFrame *keysLogPanel;
    QVBoxLayout *keysLogPanelLayout;
    QWidget *logPanelHeader;
    QHBoxLayout *logPanelHeaderLayout;
    QLabel *logPanelTitle;
    QSpacerItem *logHeaderSpacer;
    QPushButton *btnRefreshLog;
    QTableWidget *logTable;

    void setupUi(QWidget *KeyManagePage)
    {
        if (KeyManagePage->objectName().isEmpty())
            KeyManagePage->setObjectName(QString::fromUtf8("KeyManagePage"));
        KeyManagePage->resize(1280, 608);
        KeyManagePage->setStyleSheet(QString::fromUtf8("\n"
"/* ========== \351\222\245\345\214\231\347\256\241\347\220\206\351\241\265\351\235\242\346\240\267\345\274\217 ========== */\n"
"#KeyManagePage {\n"
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
"/* \346\240\207\347\255\276\351\241\265\345\210\207\346\215\242\346\214\211\351\222\256 */\n"
"#tabAll, #tabBorrowed, #tabReturned, #tabAbnormal {\n"
"    background-color: transparent;\n"
"    color: #5D6B7A;\n"
"    font-size: 14px;\n"
"    border: none;\n"
"    border-radius: 6px;\n"
"    padding: 8px 16px;\n"
"}\n"
"\n"
"#tabAll:checked, #tabBorrowed:checked, #tabReturned:checked, #tabAbnormal:checked {\n"
"    background-color: #2E7D32;\n"
"    color: #FFFFFF;\n"
"}\n"
"\n"
"/* \351\222\245\345\214\231\346\237\234\351\235\242\346\235\277 */\n"
"#keyCabinet {\n"
"    background-color: #FFFFFF;\n"
"    border: 1px solid #E8E4DE;\n"
"    border-"
                        "radius: 12px;\n"
"}\n"
"\n"
"/* \351\222\245\345\214\231\346\237\234\346\240\207\351\242\230 */\n"
"#cabinetTitle {\n"
"    color: #2C3E50;\n"
"    font-size: 16px;\n"
"    border: none;\n"
"}\n"
"\n"
"/* \351\222\245\345\214\231\346\237\234\347\212\266\346\200\201 */\n"
"#cabinetStatusText {\n"
"    color: #2E7D32;\n"
"    font-size: 12px;\n"
"}\n"
"\n"
"/* \351\222\245\345\214\231\346\240\274\345\255\220 */\n"
".keySlot {\n"
"    border-radius: 8px;\n"
"}\n"
"\n"
"/* \345\234\250\344\275\215\347\212\266\346\200\201 */\n"
".keySlotIn {\n"
"    background-color: #E8F5E9;\n"
"}\n"
"\n"
".keySlotIn .keyNum, .keySlotIn .keyStatus {\n"
"    color: #2E7D32;\n"
"}\n"
"\n"
"/* \345\200\237\345\207\272\347\212\266\346\200\201 */\n"
".keySlotOut {\n"
"    background-color: #FFF8E1;\n"
"}\n"
"\n"
".keySlotOut .keyNum, .keySlotOut .keyStatus {\n"
"    color: #F9A825;\n"
"}\n"
"\n"
"/* \345\274\202\345\270\270\347\212\266\346\200\201 */\n"
".keySlotErr {\n"
"    background-color: #FFEBEE;\n"
"}\n"
"\n"
".keySlotErr .keyNu"
                        "m, .keySlotErr .keyStatus {\n"
"    color: #E53935;\n"
"}\n"
"\n"
"/* \346\227\245\345\277\227\351\235\242\346\235\277 */\n"
"#keysLogPanel {\n"
"    background-color: #FFFFFF;\n"
"    border: 1px solid #E8E4DE;\n"
"    border-radius: 12px;\n"
"}\n"
"\n"
"/* \346\227\245\345\277\227\350\241\250\345\244\264 */\n"
"#logPanelTitle {\n"
"    color: #2C3E50;\n"
"    font-size: 16px;\n"
"    border: none;\n"
"}\n"
"\n"
"/* \350\241\250\346\240\274\346\240\267\345\274\217 */\n"
"#logTable {\n"
"    border: none;\n"
"    background-color: transparent;\n"
"}\n"
"\n"
"#logTable::item {\n"
"    padding: 8px;\n"
"}\n"
"\n"
"#logTable QHeaderView::section {\n"
"    background-color: #F5F3EF;\n"
"    color: #5D6B7A;\n"
"    font-size: 12px;\n"
"    padding: 10px;\n"
"    border: none;\n"
"    border-bottom: 1px solid #E8E4DE;\n"
"}\n"
"\n"
"/* \345\233\276\344\276\213 */\n"
".legendText {\n"
"    color: #5D6B7A;\n"
"    font-size: 11px;\n"
"}\n"
"   "));
        mainLayout = new QVBoxLayout(KeyManagePage);
        mainLayout->setSpacing(20);
        mainLayout->setObjectName(QString::fromUtf8("mainLayout"));
        mainLayout->setContentsMargins(24, 24, 24, 24);
        pageHeader = new QHBoxLayout();
        pageHeader->setSpacing(0);
        pageHeader->setObjectName(QString::fromUtf8("pageHeader"));
        keysTabs = new QWidget(KeyManagePage);
        keysTabs->setObjectName(QString::fromUtf8("keysTabs"));
        keysTabs->setStyleSheet(QString::fromUtf8("background-color: #FFFFFF; border: 1px solid #E8E4DE; border-radius: 8px; padding: 4px;"));
        keysTabsLayout = new QHBoxLayout(keysTabs);
        keysTabsLayout->setSpacing(4);
        keysTabsLayout->setObjectName(QString::fromUtf8("keysTabsLayout"));
        keysTabsLayout->setContentsMargins(4, 4, 4, 4);
        tabAll = new QPushButton(keysTabs);
        tabAll->setObjectName(QString::fromUtf8("tabAll"));
        tabAll->setCheckable(true);
        tabAll->setChecked(true);
        tabAll->setAutoExclusive(true);

        keysTabsLayout->addWidget(tabAll);

        tabBorrowed = new QPushButton(keysTabs);
        tabBorrowed->setObjectName(QString::fromUtf8("tabBorrowed"));
        tabBorrowed->setCheckable(true);
        tabBorrowed->setAutoExclusive(true);

        keysTabsLayout->addWidget(tabBorrowed);

        tabReturned = new QPushButton(keysTabs);
        tabReturned->setObjectName(QString::fromUtf8("tabReturned"));
        tabReturned->setCheckable(true);
        tabReturned->setAutoExclusive(true);

        keysTabsLayout->addWidget(tabReturned);

        tabAbnormal = new QPushButton(keysTabs);
        tabAbnormal->setObjectName(QString::fromUtf8("tabAbnormal"));
        tabAbnormal->setCheckable(true);
        tabAbnormal->setAutoExclusive(true);

        keysTabsLayout->addWidget(tabAbnormal);


        pageHeader->addWidget(keysTabs);

        headerSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        pageHeader->addItem(headerSpacer);


        mainLayout->addLayout(pageHeader);

        mainContent = new QHBoxLayout();
        mainContent->setSpacing(20);
        mainContent->setObjectName(QString::fromUtf8("mainContent"));
        keyCabinet = new QFrame(KeyManagePage);
        keyCabinet->setObjectName(QString::fromUtf8("keyCabinet"));
        keyCabinet->setMinimumSize(QSize(320, 0));
        keyCabinet->setMaximumSize(QSize(320, 16777215));
        keyCabinet->setStyleSheet(QString::fromUtf8("background-color: #FFFFFF; border: 1px solid #E8E4DE; border-radius: 12px;"));
        keyCabinet->setFrameShape(QFrame::StyledPanel);
        keyCabinetLayout = new QVBoxLayout(keyCabinet);
        keyCabinetLayout->setSpacing(0);
        keyCabinetLayout->setObjectName(QString::fromUtf8("keyCabinetLayout"));
        keyCabinetLayout->setContentsMargins(0, 0, 0, 0);
        cabinetHeader = new QWidget(keyCabinet);
        cabinetHeader->setObjectName(QString::fromUtf8("cabinetHeader"));
        cabinetHeader->setStyleSheet(QString::fromUtf8("border-bottom: 1px solid #E8E4DE; background: transparent;"));
        cabinetHeaderLayout = new QHBoxLayout(cabinetHeader);
        cabinetHeaderLayout->setSpacing(0);
        cabinetHeaderLayout->setObjectName(QString::fromUtf8("cabinetHeaderLayout"));
        cabinetHeaderLayout->setContentsMargins(20, 16, 20, 16);
        cabinetTitle = new QLabel(cabinetHeader);
        cabinetTitle->setObjectName(QString::fromUtf8("cabinetTitle"));

        cabinetHeaderLayout->addWidget(cabinetTitle);

        cabinetHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        cabinetHeaderLayout->addItem(cabinetHeaderSpacer);

        cabinetStatus = new QWidget(cabinetHeader);
        cabinetStatus->setObjectName(QString::fromUtf8("cabinetStatus"));
        cabinetStatusLayout = new QHBoxLayout(cabinetStatus);
        cabinetStatusLayout->setSpacing(6);
        cabinetStatusLayout->setObjectName(QString::fromUtf8("cabinetStatusLayout"));
        cabinetStatusLayout->setContentsMargins(0, 0, 0, 0);
        cabinetStatusDot = new QLabel(cabinetStatus);
        cabinetStatusDot->setObjectName(QString::fromUtf8("cabinetStatusDot"));
        cabinetStatusDot->setMinimumSize(QSize(8, 8));
        cabinetStatusDot->setMaximumSize(QSize(8, 8));
        cabinetStatusDot->setStyleSheet(QString::fromUtf8("background-color: #2E7D32; border-radius: 4px;"));

        cabinetStatusLayout->addWidget(cabinetStatusDot);

        cabinetStatusText = new QLabel(cabinetStatus);
        cabinetStatusText->setObjectName(QString::fromUtf8("cabinetStatusText"));

        cabinetStatusLayout->addWidget(cabinetStatusText);


        cabinetHeaderLayout->addWidget(cabinetStatus);


        keyCabinetLayout->addWidget(cabinetHeader);

        cabinetGrid = new QWidget(keyCabinet);
        cabinetGrid->setObjectName(QString::fromUtf8("cabinetGrid"));
        cabinetGridLayout = new QVBoxLayout(cabinetGrid);
        cabinetGridLayout->setSpacing(12);
        cabinetGridLayout->setObjectName(QString::fromUtf8("cabinetGridLayout"));
        cabinetGridLayout->setContentsMargins(16, 16, 16, 16);
        keyRow1 = new QHBoxLayout();
        keyRow1->setSpacing(12);
        keyRow1->setObjectName(QString::fromUtf8("keyRow1"));
        key1 = new QFrame(cabinetGrid);
        key1->setObjectName(QString::fromUtf8("key1"));
        key1->setMinimumSize(QSize(0, 70));
        key1->setStyleSheet(QString::fromUtf8("background-color: #E8F5E9; border-radius: 8px; border: none;"));
        key1Layout = new QVBoxLayout(key1);
        key1Layout->setSpacing(4);
        key1Layout->setObjectName(QString::fromUtf8("key1Layout"));
        key1Num = new QLabel(key1);
        key1Num->setObjectName(QString::fromUtf8("key1Num"));
        key1Num->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 18px; background: transparent;"));

        key1Layout->addWidget(key1Num, 0, Qt::AlignHCenter);

        key1Status = new QLabel(key1);
        key1Status->setObjectName(QString::fromUtf8("key1Status"));
        key1Status->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 11px; background: transparent;"));

        key1Layout->addWidget(key1Status, 0, Qt::AlignHCenter);


        keyRow1->addWidget(key1);

        key2 = new QFrame(cabinetGrid);
        key2->setObjectName(QString::fromUtf8("key2"));
        key2->setMinimumSize(QSize(0, 70));
        key2->setStyleSheet(QString::fromUtf8("background-color: #FFF8E1; border-radius: 8px; border: none;"));
        key2Layout = new QVBoxLayout(key2);
        key2Layout->setSpacing(4);
        key2Layout->setObjectName(QString::fromUtf8("key2Layout"));
        key2Num = new QLabel(key2);
        key2Num->setObjectName(QString::fromUtf8("key2Num"));
        key2Num->setStyleSheet(QString::fromUtf8("color: #F9A825; font-size: 18px; background: transparent;"));

        key2Layout->addWidget(key2Num, 0, Qt::AlignHCenter);

        key2Status = new QLabel(key2);
        key2Status->setObjectName(QString::fromUtf8("key2Status"));
        key2Status->setStyleSheet(QString::fromUtf8("color: #F9A825; font-size: 11px; background: transparent;"));

        key2Layout->addWidget(key2Status, 0, Qt::AlignHCenter);


        keyRow1->addWidget(key2);

        key3 = new QFrame(cabinetGrid);
        key3->setObjectName(QString::fromUtf8("key3"));
        key3->setMinimumSize(QSize(0, 70));
        key3->setStyleSheet(QString::fromUtf8("background-color: #E8F5E9; border-radius: 8px; border: none;"));
        key3Layout = new QVBoxLayout(key3);
        key3Layout->setSpacing(4);
        key3Layout->setObjectName(QString::fromUtf8("key3Layout"));
        key3Num = new QLabel(key3);
        key3Num->setObjectName(QString::fromUtf8("key3Num"));
        key3Num->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 18px; background: transparent;"));

        key3Layout->addWidget(key3Num, 0, Qt::AlignHCenter);

        key3Status = new QLabel(key3);
        key3Status->setObjectName(QString::fromUtf8("key3Status"));
        key3Status->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 11px; background: transparent;"));

        key3Layout->addWidget(key3Status, 0, Qt::AlignHCenter);


        keyRow1->addWidget(key3);

        key4 = new QFrame(cabinetGrid);
        key4->setObjectName(QString::fromUtf8("key4"));
        key4->setMinimumSize(QSize(0, 70));
        key4->setStyleSheet(QString::fromUtf8("background-color: #E8F5E9; border-radius: 8px; border: none;"));
        key4Layout = new QVBoxLayout(key4);
        key4Layout->setSpacing(4);
        key4Layout->setObjectName(QString::fromUtf8("key4Layout"));
        key4Num = new QLabel(key4);
        key4Num->setObjectName(QString::fromUtf8("key4Num"));
        key4Num->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 18px; background: transparent;"));

        key4Layout->addWidget(key4Num, 0, Qt::AlignHCenter);

        key4Status = new QLabel(key4);
        key4Status->setObjectName(QString::fromUtf8("key4Status"));
        key4Status->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 11px; background: transparent;"));

        key4Layout->addWidget(key4Status, 0, Qt::AlignHCenter);


        keyRow1->addWidget(key4);


        cabinetGridLayout->addLayout(keyRow1);

        keyRow2 = new QHBoxLayout();
        keyRow2->setSpacing(12);
        keyRow2->setObjectName(QString::fromUtf8("keyRow2"));
        key5 = new QFrame(cabinetGrid);
        key5->setObjectName(QString::fromUtf8("key5"));
        key5->setMinimumSize(QSize(0, 70));
        key5->setStyleSheet(QString::fromUtf8("background-color: #FFEBEE; border-radius: 8px; border: none;"));
        key5Layout = new QVBoxLayout(key5);
        key5Layout->setSpacing(4);
        key5Layout->setObjectName(QString::fromUtf8("key5Layout"));
        key5Num = new QLabel(key5);
        key5Num->setObjectName(QString::fromUtf8("key5Num"));
        key5Num->setStyleSheet(QString::fromUtf8("color: #E53935; font-size: 18px; background: transparent;"));

        key5Layout->addWidget(key5Num, 0, Qt::AlignHCenter);

        key5Status = new QLabel(key5);
        key5Status->setObjectName(QString::fromUtf8("key5Status"));
        key5Status->setStyleSheet(QString::fromUtf8("color: #E53935; font-size: 11px; background: transparent;"));

        key5Layout->addWidget(key5Status, 0, Qt::AlignHCenter);


        keyRow2->addWidget(key5);

        key6 = new QFrame(cabinetGrid);
        key6->setObjectName(QString::fromUtf8("key6"));
        key6->setMinimumSize(QSize(0, 70));
        key6->setStyleSheet(QString::fromUtf8("background-color: #E8F5E9; border-radius: 8px; border: none;"));
        key6Layout = new QVBoxLayout(key6);
        key6Layout->setSpacing(4);
        key6Layout->setObjectName(QString::fromUtf8("key6Layout"));
        key6Num = new QLabel(key6);
        key6Num->setObjectName(QString::fromUtf8("key6Num"));
        key6Num->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 18px; background: transparent;"));

        key6Layout->addWidget(key6Num, 0, Qt::AlignHCenter);

        key6Status = new QLabel(key6);
        key6Status->setObjectName(QString::fromUtf8("key6Status"));
        key6Status->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 11px; background: transparent;"));

        key6Layout->addWidget(key6Status, 0, Qt::AlignHCenter);


        keyRow2->addWidget(key6);

        key7 = new QFrame(cabinetGrid);
        key7->setObjectName(QString::fromUtf8("key7"));
        key7->setMinimumSize(QSize(0, 70));
        key7->setStyleSheet(QString::fromUtf8("background-color: #FFF8E1; border-radius: 8px; border: none;"));
        key7Layout = new QVBoxLayout(key7);
        key7Layout->setSpacing(4);
        key7Layout->setObjectName(QString::fromUtf8("key7Layout"));
        key7Num = new QLabel(key7);
        key7Num->setObjectName(QString::fromUtf8("key7Num"));
        key7Num->setStyleSheet(QString::fromUtf8("color: #F9A825; font-size: 18px; background: transparent;"));

        key7Layout->addWidget(key7Num, 0, Qt::AlignHCenter);

        key7Status = new QLabel(key7);
        key7Status->setObjectName(QString::fromUtf8("key7Status"));
        key7Status->setStyleSheet(QString::fromUtf8("color: #F9A825; font-size: 11px; background: transparent;"));

        key7Layout->addWidget(key7Status, 0, Qt::AlignHCenter);


        keyRow2->addWidget(key7);

        key8 = new QFrame(cabinetGrid);
        key8->setObjectName(QString::fromUtf8("key8"));
        key8->setMinimumSize(QSize(0, 70));
        key8->setStyleSheet(QString::fromUtf8("background-color: #E8F5E9; border-radius: 8px; border: none;"));
        key8Layout = new QVBoxLayout(key8);
        key8Layout->setSpacing(4);
        key8Layout->setObjectName(QString::fromUtf8("key8Layout"));
        key8Num = new QLabel(key8);
        key8Num->setObjectName(QString::fromUtf8("key8Num"));
        key8Num->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 18px; background: transparent;"));

        key8Layout->addWidget(key8Num, 0, Qt::AlignHCenter);

        key8Status = new QLabel(key8);
        key8Status->setObjectName(QString::fromUtf8("key8Status"));
        key8Status->setStyleSheet(QString::fromUtf8("color: #2E7D32; font-size: 11px; background: transparent;"));

        key8Layout->addWidget(key8Status, 0, Qt::AlignHCenter);


        keyRow2->addWidget(key8);


        cabinetGridLayout->addLayout(keyRow2);

        keyLegend = new QHBoxLayout();
        keyLegend->setSpacing(16);
        keyLegend->setObjectName(QString::fromUtf8("keyLegend"));
        legendLeftSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        keyLegend->addItem(legendLeftSpacer);

        legendIn = new QWidget(cabinetGrid);
        legendIn->setObjectName(QString::fromUtf8("legendIn"));
        legendInLayout = new QHBoxLayout(legendIn);
        legendInLayout->setSpacing(6);
        legendInLayout->setObjectName(QString::fromUtf8("legendInLayout"));
        legendInLayout->setContentsMargins(0, 0, 0, 0);
        legendInDot = new QLabel(legendIn);
        legendInDot->setObjectName(QString::fromUtf8("legendInDot"));
        legendInDot->setMinimumSize(QSize(12, 12));
        legendInDot->setMaximumSize(QSize(12, 12));
        legendInDot->setStyleSheet(QString::fromUtf8("background-color: #E8F5E9; border: 1px solid #2E7D32; border-radius: 2px;"));

        legendInLayout->addWidget(legendInDot);

        legendInText = new QLabel(legendIn);
        legendInText->setObjectName(QString::fromUtf8("legendInText"));
        legendInText->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 11px;"));

        legendInLayout->addWidget(legendInText);


        keyLegend->addWidget(legendIn);

        legendOut = new QWidget(cabinetGrid);
        legendOut->setObjectName(QString::fromUtf8("legendOut"));
        legendOutLayout = new QHBoxLayout(legendOut);
        legendOutLayout->setSpacing(6);
        legendOutLayout->setObjectName(QString::fromUtf8("legendOutLayout"));
        legendOutLayout->setContentsMargins(0, 0, 0, 0);
        legendOutDot = new QLabel(legendOut);
        legendOutDot->setObjectName(QString::fromUtf8("legendOutDot"));
        legendOutDot->setMinimumSize(QSize(12, 12));
        legendOutDot->setMaximumSize(QSize(12, 12));
        legendOutDot->setStyleSheet(QString::fromUtf8("background-color: #FFF8E1; border: 1px solid #F9A825; border-radius: 2px;"));

        legendOutLayout->addWidget(legendOutDot);

        legendOutText = new QLabel(legendOut);
        legendOutText->setObjectName(QString::fromUtf8("legendOutText"));
        legendOutText->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 11px;"));

        legendOutLayout->addWidget(legendOutText);


        keyLegend->addWidget(legendOut);

        legendErr = new QWidget(cabinetGrid);
        legendErr->setObjectName(QString::fromUtf8("legendErr"));
        legendErrLayout = new QHBoxLayout(legendErr);
        legendErrLayout->setSpacing(6);
        legendErrLayout->setObjectName(QString::fromUtf8("legendErrLayout"));
        legendErrLayout->setContentsMargins(0, 0, 0, 0);
        legendErrDot = new QLabel(legendErr);
        legendErrDot->setObjectName(QString::fromUtf8("legendErrDot"));
        legendErrDot->setMinimumSize(QSize(12, 12));
        legendErrDot->setMaximumSize(QSize(12, 12));
        legendErrDot->setStyleSheet(QString::fromUtf8("background-color: #FFEBEE; border: 1px solid #E53935; border-radius: 2px;"));

        legendErrLayout->addWidget(legendErrDot);

        legendErrText = new QLabel(legendErr);
        legendErrText->setObjectName(QString::fromUtf8("legendErrText"));
        legendErrText->setStyleSheet(QString::fromUtf8("color: #5D6B7A; font-size: 11px;"));

        legendErrLayout->addWidget(legendErrText);


        keyLegend->addWidget(legendErr);

        legendRightSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        keyLegend->addItem(legendRightSpacer);


        cabinetGridLayout->addLayout(keyLegend);

        cabinetBottomSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        cabinetGridLayout->addItem(cabinetBottomSpacer);


        keyCabinetLayout->addWidget(cabinetGrid);


        mainContent->addWidget(keyCabinet);

        keysLogPanel = new QFrame(KeyManagePage);
        keysLogPanel->setObjectName(QString::fromUtf8("keysLogPanel"));
        keysLogPanel->setStyleSheet(QString::fromUtf8("background-color: #FFFFFF; border: 1px solid #E8E4DE; border-radius: 12px;"));
        keysLogPanel->setFrameShape(QFrame::StyledPanel);
        keysLogPanelLayout = new QVBoxLayout(keysLogPanel);
        keysLogPanelLayout->setSpacing(0);
        keysLogPanelLayout->setObjectName(QString::fromUtf8("keysLogPanelLayout"));
        keysLogPanelLayout->setContentsMargins(0, 0, 0, 0);
        logPanelHeader = new QWidget(keysLogPanel);
        logPanelHeader->setObjectName(QString::fromUtf8("logPanelHeader"));
        logPanelHeader->setStyleSheet(QString::fromUtf8("border-bottom: 1px solid #E8E4DE; background: transparent;"));
        logPanelHeaderLayout = new QHBoxLayout(logPanelHeader);
        logPanelHeaderLayout->setSpacing(0);
        logPanelHeaderLayout->setObjectName(QString::fromUtf8("logPanelHeaderLayout"));
        logPanelHeaderLayout->setContentsMargins(20, 16, 20, 16);
        logPanelTitle = new QLabel(logPanelHeader);
        logPanelTitle->setObjectName(QString::fromUtf8("logPanelTitle"));
        logPanelTitle->setStyleSheet(QString::fromUtf8("color: #2C3E50; font-size: 16px; border: none;"));

        logPanelHeaderLayout->addWidget(logPanelTitle);

        logHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        logPanelHeaderLayout->addItem(logHeaderSpacer);

        btnRefreshLog = new QPushButton(logPanelHeader);
        btnRefreshLog->setObjectName(QString::fromUtf8("btnRefreshLog"));
        btnRefreshLog->setStyleSheet(QString::fromUtf8("background-color: transparent; color: #5D6B7A; font-size: 13px; border: 1px solid #E0E0E0; border-radius: 6px; padding: 6px 12px;"));

        logPanelHeaderLayout->addWidget(btnRefreshLog);


        keysLogPanelLayout->addWidget(logPanelHeader);

        logTable = new QTableWidget(keysLogPanel);
        if (logTable->columnCount() < 5)
            logTable->setColumnCount(5);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        logTable->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        logTable->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        logTable->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        logTable->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        logTable->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        logTable->setObjectName(QString::fromUtf8("logTable"));
        logTable->setStyleSheet(QString::fromUtf8("\n"
"QTableWidget {\n"
"    border: none;\n"
"    background-color: transparent;\n"
"    gridline-color: #E8E4DE;\n"
"}\n"
"QTableWidget::item {\n"
"    padding: 10px;\n"
"    border-bottom: 1px solid #E8E4DE;\n"
"}\n"
"QHeaderView::section {\n"
"    background-color: #F5F3EF;\n"
"    color: #5D6B7A;\n"
"    font-size: 12px;\n"
"    padding: 10px;\n"
"    border: none;\n"
"    border-bottom: 1px solid #E8E4DE;\n"
"}\n"
"           "));
        logTable->setAlternatingRowColors(true);
        logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        logTable->setShowGrid(false);

        keysLogPanelLayout->addWidget(logTable);


        mainContent->addWidget(keysLogPanel);


        mainLayout->addLayout(mainContent);


        retranslateUi(KeyManagePage);

        QMetaObject::connectSlotsByName(KeyManagePage);
    } // setupUi

    void retranslateUi(QWidget *KeyManagePage)
    {
        tabAll->setText(QCoreApplication::translate("KeyManagePage", "\345\205\250\351\203\250", nullptr));
        tabBorrowed->setText(QCoreApplication::translate("KeyManagePage", "\345\267\262\345\200\237\345\207\272", nullptr));
        tabReturned->setText(QCoreApplication::translate("KeyManagePage", "\345\267\262\345\275\222\350\277\230", nullptr));
        tabAbnormal->setText(QCoreApplication::translate("KeyManagePage", "\345\274\202\345\270\270", nullptr));
        cabinetTitle->setText(QCoreApplication::translate("KeyManagePage", "\346\231\272\350\203\275\351\222\245\345\214\231\346\237\234", nullptr));
        cabinetStatusDot->setText(QString());
        cabinetStatusText->setText(QCoreApplication::translate("KeyManagePage", "\345\234\250\347\272\277", nullptr));
        key1Num->setText(QCoreApplication::translate("KeyManagePage", "01", nullptr));
        key1Status->setText(QCoreApplication::translate("KeyManagePage", "\345\234\250\344\275\215", nullptr));
        key2Num->setText(QCoreApplication::translate("KeyManagePage", "02", nullptr));
        key2Status->setText(QCoreApplication::translate("KeyManagePage", "\345\200\237\345\207\272", nullptr));
        key3Num->setText(QCoreApplication::translate("KeyManagePage", "03", nullptr));
        key3Status->setText(QCoreApplication::translate("KeyManagePage", "\345\234\250\344\275\215", nullptr));
        key4Num->setText(QCoreApplication::translate("KeyManagePage", "04", nullptr));
        key4Status->setText(QCoreApplication::translate("KeyManagePage", "\345\234\250\344\275\215", nullptr));
        key5Num->setText(QCoreApplication::translate("KeyManagePage", "05", nullptr));
        key5Status->setText(QCoreApplication::translate("KeyManagePage", "\345\274\202\345\270\270", nullptr));
        key6Num->setText(QCoreApplication::translate("KeyManagePage", "06", nullptr));
        key6Status->setText(QCoreApplication::translate("KeyManagePage", "\345\234\250\344\275\215", nullptr));
        key7Num->setText(QCoreApplication::translate("KeyManagePage", "07", nullptr));
        key7Status->setText(QCoreApplication::translate("KeyManagePage", "\345\200\237\345\207\272", nullptr));
        key8Num->setText(QCoreApplication::translate("KeyManagePage", "08", nullptr));
        key8Status->setText(QCoreApplication::translate("KeyManagePage", "\345\234\250\344\275\215", nullptr));
        legendInDot->setText(QString());
        legendInText->setText(QCoreApplication::translate("KeyManagePage", "\345\234\250\344\275\215", nullptr));
        legendOutDot->setText(QString());
        legendOutText->setText(QCoreApplication::translate("KeyManagePage", "\345\200\237\345\207\272", nullptr));
        legendErrDot->setText(QString());
        legendErrText->setText(QCoreApplication::translate("KeyManagePage", "\345\274\202\345\270\270", nullptr));
        logPanelTitle->setText(QCoreApplication::translate("KeyManagePage", "\346\223\215\344\275\234\346\227\245\345\277\227", nullptr));
        btnRefreshLog->setText(QCoreApplication::translate("KeyManagePage", "\345\210\267\346\226\260", nullptr));
        QTableWidgetItem *___qtablewidgetitem = logTable->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("KeyManagePage", "\346\227\266\351\227\264", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = logTable->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("KeyManagePage", "\351\222\245\345\214\231\347\274\226\345\217\267", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = logTable->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("KeyManagePage", "\346\223\215\344\275\234\347\261\273\345\236\213", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = logTable->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("KeyManagePage", "\346\223\215\344\275\234\344\272\272", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = logTable->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("KeyManagePage", "\345\244\207\346\263\250", nullptr));
        (void)KeyManagePage;
    } // retranslateUi

};

namespace Ui {
    class KeyManagePage: public Ui_KeyManagePage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_KEYMANAGEPAGE_H
