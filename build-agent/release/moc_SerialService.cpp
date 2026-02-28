/****************************************************************************
** Meta object code from reading C++ file 'SerialService.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../src/services/SerialService.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SerialService.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_SerialService_t {
    QByteArrayData data[22];
    char stringdata0[272];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SerialService_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SerialService_t qt_meta_stringdata_SerialService = {
    {
QT_MOC_LITERAL(0, 0, 13), // "SerialService"
QT_MOC_LITERAL(1, 14, 17), // "packetTransmitted"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 10), // "CommPacket"
QT_MOC_LITERAL(4, 44, 6), // "packet"
QT_MOC_LITERAL(5, 51, 12), // "dataReceived"
QT_MOC_LITERAL(6, 64, 16), // "SerialDeviceType"
QT_MOC_LITERAL(7, 81, 10), // "deviceType"
QT_MOC_LITERAL(8, 92, 4), // "data"
QT_MOC_LITERAL(9, 97, 12), // "cardDetected"
QT_MOC_LITERAL(10, 110, 6), // "cardId"
QT_MOC_LITERAL(11, 117, 19), // "fingerprintDetected"
QT_MOC_LITERAL(12, 137, 13), // "fingerprintId"
QT_MOC_LITERAL(13, 151, 16), // "keyStatusChanged"
QT_MOC_LITERAL(14, 168, 6), // "slotId"
QT_MOC_LITERAL(15, 175, 9), // "isPresent"
QT_MOC_LITERAL(16, 185, 18), // "communicationError"
QT_MOC_LITERAL(17, 204, 5), // "error"
QT_MOC_LITERAL(18, 210, 23), // "deviceConnectionChanged"
QT_MOC_LITERAL(19, 234, 9), // "connected"
QT_MOC_LITERAL(20, 244, 11), // "onReadyRead"
QT_MOC_LITERAL(21, 256, 15) // "onErrorOccurred"

    },
    "SerialService\0packetTransmitted\0\0"
    "CommPacket\0packet\0dataReceived\0"
    "SerialDeviceType\0deviceType\0data\0"
    "cardDetected\0cardId\0fingerprintDetected\0"
    "fingerprintId\0keyStatusChanged\0slotId\0"
    "isPresent\0communicationError\0error\0"
    "deviceConnectionChanged\0connected\0"
    "onReadyRead\0onErrorOccurred"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SerialService[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   59,    2, 0x06 /* Public */,
       5,    2,   62,    2, 0x06 /* Public */,
       9,    1,   67,    2, 0x06 /* Public */,
      11,    1,   70,    2, 0x06 /* Public */,
      13,    2,   73,    2, 0x06 /* Public */,
      16,    2,   78,    2, 0x06 /* Public */,
      18,    2,   83,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      20,    0,   88,    2, 0x08 /* Private */,
      21,    0,   89,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 6, QMetaType::QByteArray,    7,    8,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void, QMetaType::Int, QMetaType::Bool,   14,   15,
    QMetaType::Void, 0x80000000 | 6, QMetaType::QString,    7,   17,
    QMetaType::Void, 0x80000000 | 6, QMetaType::Bool,    7,   19,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void SerialService::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SerialService *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->packetTransmitted((*reinterpret_cast< const CommPacket(*)>(_a[1]))); break;
        case 1: _t->dataReceived((*reinterpret_cast< SerialDeviceType(*)>(_a[1])),(*reinterpret_cast< const QByteArray(*)>(_a[2]))); break;
        case 2: _t->cardDetected((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->fingerprintDetected((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 4: _t->keyStatusChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 5: _t->communicationError((*reinterpret_cast< SerialDeviceType(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 6: _t->deviceConnectionChanged((*reinterpret_cast< SerialDeviceType(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 7: _t->onReadyRead(); break;
        case 8: _t->onErrorOccurred(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (SerialService::*)(const CommPacket & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialService::packetTransmitted)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (SerialService::*)(SerialDeviceType , const QByteArray & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialService::dataReceived)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (SerialService::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialService::cardDetected)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (SerialService::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialService::fingerprintDetected)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (SerialService::*)(int , bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialService::keyStatusChanged)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (SerialService::*)(SerialDeviceType , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialService::communicationError)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (SerialService::*)(SerialDeviceType , bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SerialService::deviceConnectionChanged)) {
                *result = 6;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject SerialService::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_SerialService.data,
    qt_meta_data_SerialService,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *SerialService::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SerialService::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SerialService.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SerialService::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void SerialService::packetTransmitted(const CommPacket & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void SerialService::dataReceived(SerialDeviceType _t1, const QByteArray & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void SerialService::cardDetected(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void SerialService::fingerprintDetected(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void SerialService::keyStatusChanged(int _t1, bool _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void SerialService::communicationError(SerialDeviceType _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void SerialService::deviceConnectionChanged(SerialDeviceType _t1, bool _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
