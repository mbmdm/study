#include <QCoreApplication>
#include <QtCore>
#include <iostream>
#include "myclass.h"
#include "myslot.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    myclass m; MySlot ms;

    //QObject::connect(&m, SIGNAL(priorityChanged(myclass::Priority)), &ms, SLOT(slot(myclass::Priority)));
    QObject::connect(&m, SIGNAL(priorityChanged(Priority)), &ms, SLOT(slot1()));
    QObject::connect(&m, &myclass::priorityChanged, &ms, &MySlot::slot2);
    QObject::connect(&m, &myclass::priorityChanged, [] () {
        qDebug() << "lambda expression";
    });

    auto metaobject = m.metaObject();
    int count = metaobject->propertyCount();
    for (int i= 0; i < count; i++ ) {

        auto prop = metaobject->property(i);
        qDebug() << "Property #" << i;
        qDebug() << "Type: " << prop.typeName();
        qDebug() << "Name: "<< prop.name();
        qDebug() << "Value: " << m.property(prop.name());
        qDebug() << "";
    }

    qDebug() << "";

    m.set_priority(myclass::Priority::high);

    return a.exec();
}
