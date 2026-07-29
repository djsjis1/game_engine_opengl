/****************************************************************************
** Meta object code from reading C++ file 'EditorPanel.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.14.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../../my_demo/Editor/EditorPanel.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'EditorPanel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.14.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_EditorPanel_t {
    QByteArrayData data[15];
    char stringdata0[213];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_EditorPanel_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_EditorPanel_t qt_meta_stringdata_EditorPanel = {
    {
QT_MOC_LITERAL(0, 0, 11), // "EditorPanel"
QT_MOC_LITERAL(1, 12, 12), // "valueChanged"
QT_MOC_LITERAL(2, 25, 0), // ""
QT_MOC_LITERAL(3, 26, 11), // "onNewEntity"
QT_MOC_LITERAL(4, 38, 14), // "onDeleteEntity"
QT_MOC_LITERAL(5, 53, 11), // "onSetParent"
QT_MOC_LITERAL(6, 65, 10), // "onUnparent"
QT_MOC_LITERAL(7, 76, 15), // "onToggleVisible"
QT_MOC_LITERAL(8, 92, 12), // "onSavePrefab"
QT_MOC_LITERAL(9, 105, 19), // "onInstantiatePrefab"
QT_MOC_LITERAL(10, 125, 14), // "onAddComponent"
QT_MOC_LITERAL(11, 140, 17), // "onRemoveComponent"
QT_MOC_LITERAL(12, 158, 16), // "onEntitySelected"
QT_MOC_LITERAL(13, 175, 19), // "onComponentSelected"
QT_MOC_LITERAL(14, 195, 17) // "onPropertyChanged"

    },
    "EditorPanel\0valueChanged\0\0onNewEntity\0"
    "onDeleteEntity\0onSetParent\0onUnparent\0"
    "onToggleVisible\0onSavePrefab\0"
    "onInstantiatePrefab\0onAddComponent\0"
    "onRemoveComponent\0onEntitySelected\0"
    "onComponentSelected\0onPropertyChanged"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_EditorPanel[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   79,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,   80,    2, 0x08 /* Private */,
       4,    0,   81,    2, 0x08 /* Private */,
       5,    0,   82,    2, 0x08 /* Private */,
       6,    0,   83,    2, 0x08 /* Private */,
       7,    0,   84,    2, 0x08 /* Private */,
       8,    0,   85,    2, 0x08 /* Private */,
       9,    0,   86,    2, 0x08 /* Private */,
      10,    0,   87,    2, 0x08 /* Private */,
      11,    0,   88,    2, 0x08 /* Private */,
      12,    0,   89,    2, 0x08 /* Private */,
      13,    0,   90,    2, 0x08 /* Private */,
      14,    0,   91,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void EditorPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<EditorPanel *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->valueChanged(); break;
        case 1: _t->onNewEntity(); break;
        case 2: _t->onDeleteEntity(); break;
        case 3: _t->onSetParent(); break;
        case 4: _t->onUnparent(); break;
        case 5: _t->onToggleVisible(); break;
        case 6: _t->onSavePrefab(); break;
        case 7: _t->onInstantiatePrefab(); break;
        case 8: _t->onAddComponent(); break;
        case 9: _t->onRemoveComponent(); break;
        case 10: _t->onEntitySelected(); break;
        case 11: _t->onComponentSelected(); break;
        case 12: _t->onPropertyChanged(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (EditorPanel::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&EditorPanel::valueChanged)) {
                *result = 0;
                return;
            }
        }
    }
    Q_UNUSED(_a);
}

QT_INIT_METAOBJECT const QMetaObject EditorPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_EditorPanel.data,
    qt_meta_data_EditorPanel,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *EditorPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *EditorPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_EditorPanel.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int EditorPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void EditorPanel::valueChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
