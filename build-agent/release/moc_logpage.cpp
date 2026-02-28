/****************************************************************************
** Meta object code from reading C++ file 'logpage.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../src/logpage.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'logpage.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_LogPage_t {
    QByteArrayData data[14];
    char stringdata0[216];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_LogPage_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_LogPage_t qt_meta_stringdata_LogPage = {
    {
QT_MOC_LITERAL(0, 0, 7), // "LogPage"
QT_MOC_LITERAL(1, 8, 17), // "autoStartServices"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 16), // "onProcessStarted"
QT_MOC_LITERAL(4, 44, 24), // "onProcessReadyReadOutput"
QT_MOC_LITERAL(5, 69, 23), // "onProcessReadyReadError"
QT_MOC_LITERAL(6, 93, 17), // "onProcessFinished"
QT_MOC_LITERAL(7, 111, 8), // "exitCode"
QT_MOC_LITERAL(8, 120, 20), // "QProcess::ExitStatus"
QT_MOC_LITERAL(9, 141, 10), // "exitStatus"
QT_MOC_LITERAL(10, 152, 14), // "onProcessError"
QT_MOC_LITERAL(11, 167, 22), // "QProcess::ProcessError"
QT_MOC_LITERAL(12, 190, 5), // "error"
QT_MOC_LITERAL(13, 196, 19) // "onStartRetryTimeout"

    },
    "LogPage\0autoStartServices\0\0onProcessStarted\0"
    "onProcessReadyReadOutput\0"
    "onProcessReadyReadError\0onProcessFinished\0"
    "exitCode\0QProcess::ExitStatus\0exitStatus\0"
    "onProcessError\0QProcess::ProcessError\0"
    "error\0onStartRetryTimeout"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_LogPage[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   49,    2, 0x08 /* Private */,
       3,    0,   50,    2, 0x08 /* Private */,
       4,    0,   51,    2, 0x08 /* Private */,
       5,    0,   52,    2, 0x08 /* Private */,
       6,    2,   53,    2, 0x08 /* Private */,
      10,    1,   58,    2, 0x08 /* Private */,
      13,    0,   61,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 8,    7,    9,
    QMetaType::Void, 0x80000000 | 11,   12,
    QMetaType::Void,

       0        // eod
};

void LogPage::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<LogPage *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->autoStartServices(); break;
        case 1: _t->onProcessStarted(); break;
        case 2: _t->onProcessReadyReadOutput(); break;
        case 3: _t->onProcessReadyReadError(); break;
        case 4: _t->onProcessFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< QProcess::ExitStatus(*)>(_a[2]))); break;
        case 5: _t->onProcessError((*reinterpret_cast< QProcess::ProcessError(*)>(_a[1]))); break;
        case 6: _t->onStartRetryTimeout(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject LogPage::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_LogPage.data,
    qt_meta_data_LogPage,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *LogPage::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *LogPage::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_LogPage.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int LogPage::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
