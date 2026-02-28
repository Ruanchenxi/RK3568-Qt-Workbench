/****************************************************************************
** Meta object code from reading C++ file 'KeySerialClient.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/protocol/KeySerialClient.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QList>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'KeySerialClient.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_KeySerialClient_t {
    QByteArrayData data[31];
    char stringdata0[370];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_KeySerialClient_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_KeySerialClient_t qt_meta_stringdata_KeySerialClient = {
    {
QT_MOC_LITERAL(0, 0, 15), // "KeySerialClient"
QT_MOC_LITERAL(1, 16, 7), // "logLine"
QT_MOC_LITERAL(2, 24, 0), // ""
QT_MOC_LITERAL(3, 25, 3), // "msg"
QT_MOC_LITERAL(4, 29, 14), // "logItemEmitted"
QT_MOC_LITERAL(5, 44, 7), // "LogItem"
QT_MOC_LITERAL(6, 52, 4), // "item"
QT_MOC_LITERAL(7, 57, 16), // "connectedChanged"
QT_MOC_LITERAL(8, 74, 9), // "connected"
QT_MOC_LITERAL(9, 84, 16), // "keyPlacedChanged"
QT_MOC_LITERAL(10, 101, 6), // "placed"
QT_MOC_LITERAL(11, 108, 16), // "keyStableChanged"
QT_MOC_LITERAL(12, 125, 6), // "stable"
QT_MOC_LITERAL(13, 132, 12), // "tasksUpdated"
QT_MOC_LITERAL(14, 145, 18), // "QList<KeyTaskInfo>"
QT_MOC_LITERAL(15, 164, 5), // "tasks"
QT_MOC_LITERAL(16, 170, 11), // "ackReceived"
QT_MOC_LITERAL(17, 182, 8), // "ackedCmd"
QT_MOC_LITERAL(18, 191, 11), // "nakReceived"
QT_MOC_LITERAL(19, 203, 7), // "origCmd"
QT_MOC_LITERAL(20, 211, 7), // "errCode"
QT_MOC_LITERAL(21, 219, 15), // "timeoutOccurred"
QT_MOC_LITERAL(22, 235, 4), // "what"
QT_MOC_LITERAL(23, 240, 6), // "notice"
QT_MOC_LITERAL(24, 247, 11), // "onReadyRead"
QT_MOC_LITERAL(25, 259, 14), // "onRetryTimeout"
QT_MOC_LITERAL(26, 274, 13), // "onSerialError"
QT_MOC_LITERAL(27, 288, 28), // "QSerialPort::SerialPortError"
QT_MOC_LITERAL(28, 317, 5), // "error"
QT_MOC_LITERAL(29, 323, 23), // "onKeyStableTimerTimeout"
QT_MOC_LITERAL(30, 347, 22) // "onQTaskRecoveryTimeout"

    },
    "KeySerialClient\0logLine\0\0msg\0"
    "logItemEmitted\0LogItem\0item\0"
    "connectedChanged\0connected\0keyPlacedChanged\0"
    "placed\0keyStableChanged\0stable\0"
    "tasksUpdated\0QList<KeyTaskInfo>\0tasks\0"
    "ackReceived\0ackedCmd\0nakReceived\0"
    "origCmd\0errCode\0timeoutOccurred\0what\0"
    "notice\0onReadyRead\0onRetryTimeout\0"
    "onSerialError\0QSerialPort::SerialPortError\0"
    "error\0onKeyStableTimerTimeout\0"
    "onQTaskRecoveryTimeout"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_KeySerialClient[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      15,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      10,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   89,    2, 0x06 /* Public */,
       4,    1,   92,    2, 0x06 /* Public */,
       7,    1,   95,    2, 0x06 /* Public */,
       9,    1,   98,    2, 0x06 /* Public */,
      11,    1,  101,    2, 0x06 /* Public */,
      13,    1,  104,    2, 0x06 /* Public */,
      16,    1,  107,    2, 0x06 /* Public */,
      18,    2,  110,    2, 0x06 /* Public */,
      21,    1,  115,    2, 0x06 /* Public */,
      23,    1,  118,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      24,    0,  121,    2, 0x08 /* Private */,
      25,    0,  122,    2, 0x08 /* Private */,
      26,    1,  123,    2, 0x08 /* Private */,
      29,    0,  126,    2, 0x08 /* Private */,
      30,    0,  127,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, 0x80000000 | 5,    6,
    QMetaType::Void, QMetaType::Bool,    8,
    QMetaType::Void, QMetaType::Bool,   10,
    QMetaType::Void, QMetaType::Bool,   12,
    QMetaType::Void, 0x80000000 | 14,   15,
    QMetaType::Void, QMetaType::UChar,   17,
    QMetaType::Void, QMetaType::UChar, QMetaType::UChar,   19,   20,
    QMetaType::Void, QMetaType::QString,   22,
    QMetaType::Void, QMetaType::QString,   22,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 27,   28,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void KeySerialClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<KeySerialClient *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->logLine((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->logItemEmitted((*reinterpret_cast< const LogItem(*)>(_a[1]))); break;
        case 2: _t->connectedChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 3: _t->keyPlacedChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->keyStableChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 5: _t->tasksUpdated((*reinterpret_cast< const QList<KeyTaskInfo>(*)>(_a[1]))); break;
        case 6: _t->ackReceived((*reinterpret_cast< quint8(*)>(_a[1]))); break;
        case 7: _t->nakReceived((*reinterpret_cast< quint8(*)>(_a[1])),(*reinterpret_cast< quint8(*)>(_a[2]))); break;
        case 8: _t->timeoutOccurred((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 9: _t->notice((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 10: _t->onReadyRead(); break;
        case 11: _t->onRetryTimeout(); break;
        case 12: _t->onSerialError((*reinterpret_cast< QSerialPort::SerialPortError(*)>(_a[1]))); break;
        case 13: _t->onKeyStableTimerTimeout(); break;
        case 14: _t->onQTaskRecoveryTimeout(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (KeySerialClient::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::logLine)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(const LogItem & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::logItemEmitted)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::connectedChanged)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::keyPlacedChanged)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::keyStableChanged)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(const QList<KeyTaskInfo> & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::tasksUpdated)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(quint8 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::ackReceived)) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(quint8 , quint8 );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::nakReceived)) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::timeoutOccurred)) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (KeySerialClient::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&KeySerialClient::notice)) {
                *result = 9;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject KeySerialClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_KeySerialClient.data,
    qt_meta_data_KeySerialClient,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *KeySerialClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KeySerialClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_KeySerialClient.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int KeySerialClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void KeySerialClient::logLine(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void KeySerialClient::logItemEmitted(const LogItem & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void KeySerialClient::connectedChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void KeySerialClient::keyPlacedChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void KeySerialClient::keyStableChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void KeySerialClient::tasksUpdated(const QList<KeyTaskInfo> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void KeySerialClient::ackReceived(quint8 _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void KeySerialClient::nakReceived(quint8 _t1, quint8 _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void KeySerialClient::timeoutOccurred(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void KeySerialClient::notice(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
