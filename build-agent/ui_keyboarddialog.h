/********************************************************************************
** Form generated from reading UI file 'keyboarddialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_KEYBOARDDIALOG_H
#define UI_KEYBOARDDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_KeyboardDialog
{
public:
    QVBoxLayout *mainLayout;
    QLineEdit *inputDisplay;
    QHBoxLayout *numRow;
    QPushButton *btnNum1;
    QPushButton *btnNum2;
    QPushButton *btnNum3;
    QPushButton *btnNum4;
    QPushButton *btnNum5;
    QPushButton *btnNum6;
    QPushButton *btnNum7;
    QPushButton *btnNum8;
    QPushButton *btnNum9;
    QPushButton *btnNum0;
    QPushButton *btnBackspace;
    QHBoxLayout *row1;
    QPushButton *btnQ;
    QPushButton *btnW;
    QPushButton *btnE;
    QPushButton *btnR;
    QPushButton *btnT;
    QPushButton *btnY;
    QPushButton *btnU;
    QPushButton *btnI;
    QPushButton *btnO;
    QPushButton *btnP;
    QHBoxLayout *row2;
    QSpacerItem *row2LeftSpacer;
    QPushButton *btnA;
    QPushButton *btnS;
    QPushButton *btnD;
    QPushButton *btnF;
    QPushButton *btnG;
    QPushButton *btnH;
    QPushButton *btnJ;
    QPushButton *btnK;
    QPushButton *btnL;
    QSpacerItem *row2RightSpacer;
    QHBoxLayout *row3;
    QPushButton *btnShift;
    QPushButton *btnZ;
    QPushButton *btnX;
    QPushButton *btnC;
    QPushButton *btnV;
    QPushButton *btnB;
    QPushButton *btnN;
    QPushButton *btnM;
    QPushButton *btnCaps;
    QHBoxLayout *bottomRow;
    QPushButton *btnClear;
    QPushButton *btnAt;
    QPushButton *btnSpace;
    QPushButton *btnDot;
    QPushButton *btnCancel;
    QPushButton *btnOK;

    void setupUi(QDialog *KeyboardDialog)
    {
        if (KeyboardDialog->objectName().isEmpty())
            KeyboardDialog->setObjectName(QString::fromUtf8("KeyboardDialog"));
        KeyboardDialog->resize(700, 380);
        KeyboardDialog->setModal(true);
        KeyboardDialog->setStyleSheet(QString::fromUtf8("\n"
"    /* \345\257\271\350\257\235\346\241\206\346\225\264\344\275\223\346\240\267\345\274\217 */\n"
"    QDialog#KeyboardDialog {\n"
"      background-color: #2C3E50;\n"
"      border-radius: 12px;\n"
"    }\n"
"    \n"
"    /* \350\276\223\345\205\245\346\241\206\346\240\267\345\274\217 */\n"
"    QLineEdit#inputDisplay {\n"
"      background-color: #34495E;\n"
"      border: 2px solid #2E7D32;\n"
"      border-radius: 8px;\n"
"      padding: 12px 16px;\n"
"      font-size: 24px;\n"
"      color: #FFFFFF;\n"
"      font-family: \"Consolas\", \"Monaco\", monospace;\n"
"    }\n"
"    QLineEdit#inputDisplay:focus {\n"
"      border-color: #4CAF50;\n"
"    }\n"
"    \n"
"    /* \346\214\211\351\224\256\351\200\232\347\224\250\346\240\267\345\274\217 */\n"
"    QPushButton {\n"
"      background-color: #465A6E;\n"
"      color: #FFFFFF;\n"
"      border: none;\n"
"      border-radius: 8px;\n"
"      font-size: 18px;\n"
"      font-weight: bold;\n"
"      min-width: 50px;\n"
"      min-height: 50px;\n"
"    }\n"
""
                        "    QPushButton:hover {\n"
"      background-color: #5D7A93;\n"
"    }\n"
"    QPushButton:pressed {\n"
"      background-color: #2E7D32;\n"
"    }\n"
"    \n"
"    /* \347\211\271\346\256\212\345\212\237\350\203\275\351\224\256\346\240\267\345\274\217 */\n"
"    QPushButton#btnBackspace, QPushButton#btnClear {\n"
"      background-color: #C0392B;\n"
"    }\n"
"    QPushButton#btnBackspace:hover, QPushButton#btnClear:hover {\n"
"      background-color: #E74C3C;\n"
"    }\n"
"    \n"
"    QPushButton#btnShift, QPushButton#btnCaps {\n"
"      background-color: #2980B9;\n"
"    }\n"
"    QPushButton#btnShift:hover, QPushButton#btnCaps:hover {\n"
"      background-color: #3498DB;\n"
"    }\n"
"    QPushButton#btnShift:checked, QPushButton#btnCaps:checked {\n"
"      background-color: #2E7D32;\n"
"    }\n"
"    \n"
"    QPushButton#btnSpace {\n"
"      min-width: 300px;\n"
"    }\n"
"    \n"
"    QPushButton#btnOK {\n"
"      background-color: #2E7D32;\n"
"      min-width: 100px;\n"
"    }\n"
"    QPushButton#btnOK"
                        ":hover {\n"
"      background-color: #4CAF50;\n"
"    }\n"
"    \n"
"    QPushButton#btnCancel {\n"
"      background-color: #7F8C8D;\n"
"      min-width: 80px;\n"
"    }\n"
"    QPushButton#btnCancel:hover {\n"
"      background-color: #95A5A6;\n"
"    }\n"
"    \n"
"    /* \346\225\260\345\255\227\351\224\256\346\240\267\345\274\217 */\n"
"    QPushButton[objectName^=\"btnNum\"] {\n"
"      background-color: #5D4E6D;\n"
"    }\n"
"    QPushButton[objectName^=\"btnNum\"]:hover {\n"
"      background-color: #7B6B8D;\n"
"    }\n"
"   "));
        mainLayout = new QVBoxLayout(KeyboardDialog);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setObjectName(QString::fromUtf8("mainLayout"));
        inputDisplay = new QLineEdit(KeyboardDialog);
        inputDisplay->setObjectName(QString::fromUtf8("inputDisplay"));
        inputDisplay->setReadOnly(true);

        mainLayout->addWidget(inputDisplay);

        numRow = new QHBoxLayout();
        numRow->setSpacing(6);
        numRow->setObjectName(QString::fromUtf8("numRow"));
        btnNum1 = new QPushButton(KeyboardDialog);
        btnNum1->setObjectName(QString::fromUtf8("btnNum1"));

        numRow->addWidget(btnNum1);

        btnNum2 = new QPushButton(KeyboardDialog);
        btnNum2->setObjectName(QString::fromUtf8("btnNum2"));

        numRow->addWidget(btnNum2);

        btnNum3 = new QPushButton(KeyboardDialog);
        btnNum3->setObjectName(QString::fromUtf8("btnNum3"));

        numRow->addWidget(btnNum3);

        btnNum4 = new QPushButton(KeyboardDialog);
        btnNum4->setObjectName(QString::fromUtf8("btnNum4"));

        numRow->addWidget(btnNum4);

        btnNum5 = new QPushButton(KeyboardDialog);
        btnNum5->setObjectName(QString::fromUtf8("btnNum5"));

        numRow->addWidget(btnNum5);

        btnNum6 = new QPushButton(KeyboardDialog);
        btnNum6->setObjectName(QString::fromUtf8("btnNum6"));

        numRow->addWidget(btnNum6);

        btnNum7 = new QPushButton(KeyboardDialog);
        btnNum7->setObjectName(QString::fromUtf8("btnNum7"));

        numRow->addWidget(btnNum7);

        btnNum8 = new QPushButton(KeyboardDialog);
        btnNum8->setObjectName(QString::fromUtf8("btnNum8"));

        numRow->addWidget(btnNum8);

        btnNum9 = new QPushButton(KeyboardDialog);
        btnNum9->setObjectName(QString::fromUtf8("btnNum9"));

        numRow->addWidget(btnNum9);

        btnNum0 = new QPushButton(KeyboardDialog);
        btnNum0->setObjectName(QString::fromUtf8("btnNum0"));

        numRow->addWidget(btnNum0);

        btnBackspace = new QPushButton(KeyboardDialog);
        btnBackspace->setObjectName(QString::fromUtf8("btnBackspace"));

        numRow->addWidget(btnBackspace);


        mainLayout->addLayout(numRow);

        row1 = new QHBoxLayout();
        row1->setSpacing(6);
        row1->setObjectName(QString::fromUtf8("row1"));
        btnQ = new QPushButton(KeyboardDialog);
        btnQ->setObjectName(QString::fromUtf8("btnQ"));

        row1->addWidget(btnQ);

        btnW = new QPushButton(KeyboardDialog);
        btnW->setObjectName(QString::fromUtf8("btnW"));

        row1->addWidget(btnW);

        btnE = new QPushButton(KeyboardDialog);
        btnE->setObjectName(QString::fromUtf8("btnE"));

        row1->addWidget(btnE);

        btnR = new QPushButton(KeyboardDialog);
        btnR->setObjectName(QString::fromUtf8("btnR"));

        row1->addWidget(btnR);

        btnT = new QPushButton(KeyboardDialog);
        btnT->setObjectName(QString::fromUtf8("btnT"));

        row1->addWidget(btnT);

        btnY = new QPushButton(KeyboardDialog);
        btnY->setObjectName(QString::fromUtf8("btnY"));

        row1->addWidget(btnY);

        btnU = new QPushButton(KeyboardDialog);
        btnU->setObjectName(QString::fromUtf8("btnU"));

        row1->addWidget(btnU);

        btnI = new QPushButton(KeyboardDialog);
        btnI->setObjectName(QString::fromUtf8("btnI"));

        row1->addWidget(btnI);

        btnO = new QPushButton(KeyboardDialog);
        btnO->setObjectName(QString::fromUtf8("btnO"));

        row1->addWidget(btnO);

        btnP = new QPushButton(KeyboardDialog);
        btnP->setObjectName(QString::fromUtf8("btnP"));

        row1->addWidget(btnP);


        mainLayout->addLayout(row1);

        row2 = new QHBoxLayout();
        row2->setSpacing(6);
        row2->setObjectName(QString::fromUtf8("row2"));
        row2LeftSpacer = new QSpacerItem(25, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        row2->addItem(row2LeftSpacer);

        btnA = new QPushButton(KeyboardDialog);
        btnA->setObjectName(QString::fromUtf8("btnA"));

        row2->addWidget(btnA);

        btnS = new QPushButton(KeyboardDialog);
        btnS->setObjectName(QString::fromUtf8("btnS"));

        row2->addWidget(btnS);

        btnD = new QPushButton(KeyboardDialog);
        btnD->setObjectName(QString::fromUtf8("btnD"));

        row2->addWidget(btnD);

        btnF = new QPushButton(KeyboardDialog);
        btnF->setObjectName(QString::fromUtf8("btnF"));

        row2->addWidget(btnF);

        btnG = new QPushButton(KeyboardDialog);
        btnG->setObjectName(QString::fromUtf8("btnG"));

        row2->addWidget(btnG);

        btnH = new QPushButton(KeyboardDialog);
        btnH->setObjectName(QString::fromUtf8("btnH"));

        row2->addWidget(btnH);

        btnJ = new QPushButton(KeyboardDialog);
        btnJ->setObjectName(QString::fromUtf8("btnJ"));

        row2->addWidget(btnJ);

        btnK = new QPushButton(KeyboardDialog);
        btnK->setObjectName(QString::fromUtf8("btnK"));

        row2->addWidget(btnK);

        btnL = new QPushButton(KeyboardDialog);
        btnL->setObjectName(QString::fromUtf8("btnL"));

        row2->addWidget(btnL);

        row2RightSpacer = new QSpacerItem(25, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        row2->addItem(row2RightSpacer);


        mainLayout->addLayout(row2);

        row3 = new QHBoxLayout();
        row3->setSpacing(6);
        row3->setObjectName(QString::fromUtf8("row3"));
        btnShift = new QPushButton(KeyboardDialog);
        btnShift->setObjectName(QString::fromUtf8("btnShift"));
        btnShift->setCheckable(true);

        row3->addWidget(btnShift);

        btnZ = new QPushButton(KeyboardDialog);
        btnZ->setObjectName(QString::fromUtf8("btnZ"));

        row3->addWidget(btnZ);

        btnX = new QPushButton(KeyboardDialog);
        btnX->setObjectName(QString::fromUtf8("btnX"));

        row3->addWidget(btnX);

        btnC = new QPushButton(KeyboardDialog);
        btnC->setObjectName(QString::fromUtf8("btnC"));

        row3->addWidget(btnC);

        btnV = new QPushButton(KeyboardDialog);
        btnV->setObjectName(QString::fromUtf8("btnV"));

        row3->addWidget(btnV);

        btnB = new QPushButton(KeyboardDialog);
        btnB->setObjectName(QString::fromUtf8("btnB"));

        row3->addWidget(btnB);

        btnN = new QPushButton(KeyboardDialog);
        btnN->setObjectName(QString::fromUtf8("btnN"));

        row3->addWidget(btnN);

        btnM = new QPushButton(KeyboardDialog);
        btnM->setObjectName(QString::fromUtf8("btnM"));

        row3->addWidget(btnM);

        btnCaps = new QPushButton(KeyboardDialog);
        btnCaps->setObjectName(QString::fromUtf8("btnCaps"));
        btnCaps->setCheckable(true);

        row3->addWidget(btnCaps);


        mainLayout->addLayout(row3);

        bottomRow = new QHBoxLayout();
        bottomRow->setSpacing(6);
        bottomRow->setObjectName(QString::fromUtf8("bottomRow"));
        btnClear = new QPushButton(KeyboardDialog);
        btnClear->setObjectName(QString::fromUtf8("btnClear"));
        btnClear->setMinimumWidth(70);

        bottomRow->addWidget(btnClear);

        btnAt = new QPushButton(KeyboardDialog);
        btnAt->setObjectName(QString::fromUtf8("btnAt"));

        bottomRow->addWidget(btnAt);

        btnSpace = new QPushButton(KeyboardDialog);
        btnSpace->setObjectName(QString::fromUtf8("btnSpace"));

        bottomRow->addWidget(btnSpace);

        btnDot = new QPushButton(KeyboardDialog);
        btnDot->setObjectName(QString::fromUtf8("btnDot"));

        bottomRow->addWidget(btnDot);

        btnCancel = new QPushButton(KeyboardDialog);
        btnCancel->setObjectName(QString::fromUtf8("btnCancel"));

        bottomRow->addWidget(btnCancel);

        btnOK = new QPushButton(KeyboardDialog);
        btnOK->setObjectName(QString::fromUtf8("btnOK"));

        bottomRow->addWidget(btnOK);


        mainLayout->addLayout(bottomRow);


        retranslateUi(KeyboardDialog);
        QObject::connect(btnCancel, SIGNAL(clicked()), KeyboardDialog, SLOT(reject()));
        QObject::connect(btnOK, SIGNAL(clicked()), KeyboardDialog, SLOT(accept()));

        QMetaObject::connectSlotsByName(KeyboardDialog);
    } // setupUi

    void retranslateUi(QDialog *KeyboardDialog)
    {
        KeyboardDialog->setWindowTitle(QCoreApplication::translate("KeyboardDialog", "\350\231\232\346\213\237\351\224\256\347\233\230", nullptr));
        inputDisplay->setPlaceholderText(QCoreApplication::translate("KeyboardDialog", "\350\257\267\350\276\223\345\205\245\345\206\205\345\256\271...", nullptr));
        btnNum1->setText(QCoreApplication::translate("KeyboardDialog", "1", nullptr));
        btnNum2->setText(QCoreApplication::translate("KeyboardDialog", "2", nullptr));
        btnNum3->setText(QCoreApplication::translate("KeyboardDialog", "3", nullptr));
        btnNum4->setText(QCoreApplication::translate("KeyboardDialog", "4", nullptr));
        btnNum5->setText(QCoreApplication::translate("KeyboardDialog", "5", nullptr));
        btnNum6->setText(QCoreApplication::translate("KeyboardDialog", "6", nullptr));
        btnNum7->setText(QCoreApplication::translate("KeyboardDialog", "7", nullptr));
        btnNum8->setText(QCoreApplication::translate("KeyboardDialog", "8", nullptr));
        btnNum9->setText(QCoreApplication::translate("KeyboardDialog", "9", nullptr));
        btnNum0->setText(QCoreApplication::translate("KeyboardDialog", "0", nullptr));
        btnBackspace->setText(QCoreApplication::translate("KeyboardDialog", "\342\214\253", nullptr));
#if QT_CONFIG(tooltip)
        btnBackspace->setToolTip(QCoreApplication::translate("KeyboardDialog", "\351\200\200\346\240\274", nullptr));
#endif // QT_CONFIG(tooltip)
        btnQ->setText(QCoreApplication::translate("KeyboardDialog", "Q", nullptr));
        btnW->setText(QCoreApplication::translate("KeyboardDialog", "W", nullptr));
        btnE->setText(QCoreApplication::translate("KeyboardDialog", "E", nullptr));
        btnR->setText(QCoreApplication::translate("KeyboardDialog", "R", nullptr));
        btnT->setText(QCoreApplication::translate("KeyboardDialog", "T", nullptr));
        btnY->setText(QCoreApplication::translate("KeyboardDialog", "Y", nullptr));
        btnU->setText(QCoreApplication::translate("KeyboardDialog", "U", nullptr));
        btnI->setText(QCoreApplication::translate("KeyboardDialog", "I", nullptr));
        btnO->setText(QCoreApplication::translate("KeyboardDialog", "O", nullptr));
        btnP->setText(QCoreApplication::translate("KeyboardDialog", "P", nullptr));
        btnA->setText(QCoreApplication::translate("KeyboardDialog", "A", nullptr));
        btnS->setText(QCoreApplication::translate("KeyboardDialog", "S", nullptr));
        btnD->setText(QCoreApplication::translate("KeyboardDialog", "D", nullptr));
        btnF->setText(QCoreApplication::translate("KeyboardDialog", "F", nullptr));
        btnG->setText(QCoreApplication::translate("KeyboardDialog", "G", nullptr));
        btnH->setText(QCoreApplication::translate("KeyboardDialog", "H", nullptr));
        btnJ->setText(QCoreApplication::translate("KeyboardDialog", "J", nullptr));
        btnK->setText(QCoreApplication::translate("KeyboardDialog", "K", nullptr));
        btnL->setText(QCoreApplication::translate("KeyboardDialog", "L", nullptr));
        btnShift->setText(QCoreApplication::translate("KeyboardDialog", "\342\207\247", nullptr));
#if QT_CONFIG(tooltip)
        btnShift->setToolTip(QCoreApplication::translate("KeyboardDialog", "Shift", nullptr));
#endif // QT_CONFIG(tooltip)
        btnZ->setText(QCoreApplication::translate("KeyboardDialog", "Z", nullptr));
        btnX->setText(QCoreApplication::translate("KeyboardDialog", "X", nullptr));
        btnC->setText(QCoreApplication::translate("KeyboardDialog", "C", nullptr));
        btnV->setText(QCoreApplication::translate("KeyboardDialog", "V", nullptr));
        btnB->setText(QCoreApplication::translate("KeyboardDialog", "B", nullptr));
        btnN->setText(QCoreApplication::translate("KeyboardDialog", "N", nullptr));
        btnM->setText(QCoreApplication::translate("KeyboardDialog", "M", nullptr));
        btnCaps->setText(QCoreApplication::translate("KeyboardDialog", "Caps", nullptr));
#if QT_CONFIG(tooltip)
        btnCaps->setToolTip(QCoreApplication::translate("KeyboardDialog", "\345\244\247\345\206\231\351\224\201\345\256\232", nullptr));
#endif // QT_CONFIG(tooltip)
        btnClear->setText(QCoreApplication::translate("KeyboardDialog", "\346\270\205\347\251\272", nullptr));
        btnAt->setText(QCoreApplication::translate("KeyboardDialog", "@", nullptr));
        btnSpace->setText(QCoreApplication::translate("KeyboardDialog", "\347\251\272\346\240\274", nullptr));
        btnDot->setText(QCoreApplication::translate("KeyboardDialog", ".", nullptr));
        btnCancel->setText(QCoreApplication::translate("KeyboardDialog", "\345\217\226\346\266\210", nullptr));
        btnOK->setText(QCoreApplication::translate("KeyboardDialog", "\347\241\256\345\256\232", nullptr));
    } // retranslateUi

};

namespace Ui {
    class KeyboardDialog: public Ui_KeyboardDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_KEYBOARDDIALOG_H
